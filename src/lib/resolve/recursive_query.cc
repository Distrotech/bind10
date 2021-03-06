// Copyright (C) 2011  Internet Systems Consortium, Inc. ("ISC")
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
// REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
// AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
// INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
// LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
// OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
// PERFORMANCE OF THIS SOFTWARE.

#include <config.h>

#include <stdlib.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>             // for some IPC/network system calls
#include <string>

#include <boost/lexical_cast.hpp>
#include <boost/bind.hpp>

#include <dns/question.h>
#include <dns/message.h>
#include <dns/opcode.h>
#include <dns/exceptions.h>
#include <dns/rdataclass.h>
#include <resolve/resolve.h>
#include <resolve/resolve_log.h>
#include <resolve/resolve_messages.h>
#include <cache/resolver_cache.h>
#include <nsas/address_request_callback.h>
#include <nsas/nameserver_address.h>

#include <asio.hpp>
#include <asiodns/dns_service.h>
#include <asiodns/io_fetch.h>
#include <asiolink/io_service.h>
#include <resolve/response_classifier.h>
#include <resolve/recursive_query.h>

using namespace isc::dns;
using namespace isc::nsas;
using namespace isc::util;
using namespace isc::asiolink;
using namespace isc::resolve;

namespace isc {
namespace asiodns {

namespace {
// Function to check if the given name/class has any address in the cache
bool
hasAddress(const Name& name, const RRClass& rrClass,
      const isc::cache::ResolverCache& cache)
{
    // FIXME: If we are single-stack and we get only the other type of
    // address, what should we do? In that case, it will be considered
    // unreachable, which is most probably true, because A and AAAA will
    // usually have the same RTT, so we should have both or none from the
    // glue.
    return (cache.lookup(name, RRType::A(), rrClass) != RRsetPtr() ||
            cache.lookup(name, RRType::AAAA(), rrClass) != RRsetPtr());
}

// Convenience function for debug messages.  Question::toText() includes
// a trailing newline in its output, which makes it awkward to embed in a
// message.  This just strips that newline from it.
std::string
questionText(const isc::dns::Question& question) {
    std::string text = question.toText();
    if (!text.empty()) {
        text.erase(text.size() - 1);
    }
    return (text);
}

} // anonymous namespace

/// \brief Find deepest usable delegation in the cache
///
/// This finds the deepest delegation we have in cache and is safe to use.
/// It is not public function, therefore it's not in header. But it's not
/// in anonymous namespace, so we can call it from unittests.
/// \param name The name we want to delegate to.
/// \param rrclass The class.
/// \param cache The place too look for known delegations.
std::string
deepestDelegation(Name name, RRClass rrclass,
                  isc::cache::ResolverCache& cache)
{
    RRsetPtr cachedNS;
    // Look for delegation point from bottom, until we find one with
    // IP address or get to root.
    //
    // We need delegation with IP address so we can ask it right away.
    // If we don't have the IP address, we would need to ask above it
    // anyway in the best case, and the NS could be inside the zone,
    // and we could get all loopy with the NSAS in the worst case.
    while (name.getLabelCount() > 1 &&
           (cachedNS = cache.lookupDeepestNS(name, rrclass)) != RRsetPtr()) {
        // Look if we have an IP address for the NS
        for (RdataIteratorPtr ns(cachedNS->getRdataIterator());
             !ns->isLast(); ns->next()) {
            // Do we have IP for this specific NS?
            if (hasAddress(dynamic_cast<const rdata::generic::NS&>(
                               ns->getCurrent()).getNSName(), rrclass,
                           cache)) {
                // Found one, stop checking and use this zone
                // (there may be more addresses, that's only better)
                return (cachedNS->getName().toText());
            }
        }
        // We don't have anything for this one, so try something higher
        if (name.getLabelCount() > 1) {
            name = name.split(1);
        }
    }
    // Fallback, nothing found, start at root
    return (".");
}

// Here we do not use the typedef above, as the SunStudio compiler
// mishandles this in its name mangling, and wouldn't compile.
// We can probably use a typedef, but need to move it to a central
// location and use it consistently.
RecursiveQuery::RecursiveQuery(DNSServiceBase& dns_service,
    isc::nsas::NameserverAddressStore& nsas,
    isc::cache::ResolverCache& cache,
    const std::vector<std::pair<std::string, uint16_t> >& upstream,
    const std::vector<std::pair<std::string, uint16_t> >& upstream_root,
    int query_timeout, int client_timeout, int lookup_timeout,
    unsigned retries)
    :
    dns_service_(dns_service),
    nsas_(nsas), cache_(cache),
    upstream_(new AddressVector(upstream)),
    upstream_root_(new AddressVector(upstream_root)),
    test_server_("", 0),
    query_timeout_(query_timeout), client_timeout_(client_timeout),
    lookup_timeout_(lookup_timeout), retries_(retries), rtt_recorder_()
{
}

// Set the test server - only used for unit testing.
void
RecursiveQuery::setTestServer(const std::string& address, uint16_t port) {
    LOG_WARN(isc::resolve::logger, RESLIB_TEST_SERVER).arg(address).arg(port);
    test_server_.first = address;
    test_server_.second = port;
}

// Set the RTT recorder - only used for testing
void
RecursiveQuery::setRttRecorder(boost::shared_ptr<RttRecorder>& recorder) {
    rtt_recorder_ = recorder;
}

namespace {
typedef std::pair<std::string, uint16_t> addr_t;

/*
 * This is a query in progress. When a new query is made, this one holds
 * the context information about it, like how many times we are allowed
 * to retry on failure, what to do when we succeed, etc.
 *
 * Used by RecursiveQuery::sendQuery.
 */
class RunningQuery : public IOFetch::Callback, public AbstractRunningQuery {

class ResolverNSASCallback : public isc::nsas::AddressRequestCallback {
public:
    ResolverNSASCallback(RunningQuery* rq) : rq_(rq) {}

    void success(const isc::nsas::NameserverAddress& address) {
        // Success callback, send query to found namesever
        LOG_DEBUG(isc::resolve::logger, RESLIB_DBG_CB, RESLIB_RUNQ_SUCCESS)
                  .arg(address.getAddress().toText());
        rq_->nsasCallbackCalled();
        rq_->sendTo(address);
    }

    void unreachable() {
        // Nameservers unreachable: drop query or send servfail?
        LOG_DEBUG(isc::resolve::logger, RESLIB_DBG_CB, RESLIB_RUNQ_FAIL);
        rq_->nsasCallbackCalled();
        rq_->makeSERVFAIL();
        rq_->callCallback(true);
        rq_->stop();
    }

private:
    RunningQuery* rq_;
};


private:
    // The io service to handle async calls
    IOService& io_;

    // Info for (re)sending the query (the question and destination)
    Question question_;

    // This is the query message got from client
    ConstMessagePtr query_message_;

    // This is where we build and store our final answer
    MessagePtr answer_message_;

    // Test server - only used for testing.  This takes precedence over all
    // other servers if the port is non-zero.
    std::pair<std::string, uint16_t> test_server_;

    // Buffer to store the intermediate results.
    OutputBufferPtr buffer_;

    // The callback will be called when we have either decided we
    // are done, or when we give up
    isc::resolve::ResolverInterface::CallbackPtr resolvercallback_;

    // Protocol used for the last query.  This is set to IOFetch::UDP when a
    // new upstream query is initiated, and changed to IOFetch::TCP if a
    // packet is returned with the TC bit set.  It is stored here to detect the
    // case of a TCP packet being returned with the TC bit set.
    IOFetch::Protocol protocol_;

    // EDNS flag
    bool edns_;

    // To prevent both unreasonably long cname chains and cname loops,
    // we simply keep a counter of the number of CNAMEs we have
    // followed so far (and error if it exceeds RESOLVER_MAX_CNAME_CHAIN
    // from lib/resolve/response_classifier.h)
    unsigned cname_count_;

    /*
     * TODO Do something more clever with timeouts. In the long term, some
     *     computation of average RTT, increase with each retry, etc.
     */
    // Timeout information for outgoing queries
    int query_timeout_;
    unsigned retries_;

    // normal query state

    // TODO: replace by our wrapper
    asio::deadline_timer client_timer;
    asio::deadline_timer lookup_timer;

    // If we timed out ourselves (lookup timeout), stop issuing queries
    bool done_;

    // If we have a client timeout, we call back with a failure message,
    // but we do not stop yet. We use this variable to make sure we
    // don't call back a second time later
    bool callback_called_;

    // Reference to our NSAS
    isc::nsas::NameserverAddressStore& nsas_;

    // Reference to our cache
    isc::cache::ResolverCache& cache_;

    // the 'current' zone we are in (i.e.) we start out at the root,
    // and for each delegation this gets updated with the zone the
    // delegation points to.
    // TODO: make this a Name (it is a string right now because most
    // of the call we use it in take a string, we need update those
    // too).
    std::string cur_zone_;

    // This is the handler we pass on to the NSAS; it is called when
    // the NSAS has an address for us to query
    boost::shared_ptr<ResolverNSASCallback> nsas_callback_;

    // this is set to true if we have asked the nsas to give us
    // an address and we are waiting for it to call us back.
    // We use is to cancel the outstanding callback in case we
    // have a lookup timeout and decide to give up
    bool nsas_callback_out_;

    // This is the nameserver we have an outstanding query to.
    // It is used to update the RTT once the query returns
    isc::nsas::NameserverAddress current_ns_address;

    // The moment in time we sent a query to the nameserver above.
    struct timeval current_ns_qsent_time;

    // RunningQuery deletes itself when it is done. In order for us
    // to do this safely, we must make sure that there are no events
    // that might call back to it. There are two types of events in
    // this sense; the timers we set ourselves (lookup and client),
    // and outstanding queries to nameservers. When each of these is
    // started, we increase this value. When they fire, it is decreased
    // again. We cannot delete ourselves until this value is back to 0.
    //
    // Note that the NSAS callback is *not* seen as an outstanding
    // event; we can cancel the NSAS callback safely.
    size_t outstanding_events_;

    // RTT Recorder.  Used for testing, the RTTs of queries are
    // sent to this object as well as being used to update the NSAS.
    boost::shared_ptr<RttRecorder> rtt_recorder_;

    // perform a single lookup; first we check the cache to see
    // if we have a response for our query stored already. if
    // so, call handlerecursiveresponse(), if not, we call send()
    void doLookup() {
        LOG_DEBUG(isc::resolve::logger, RESLIB_DBG_CACHE, RESLIB_RUNQ_CACHE_LOOKUP)
                  .arg(questionText(question_));

        Message cached_message(Message::RENDER);
        isc::resolve::initResponseMessage(question_, cached_message);
        if (cache_.lookup(question_.getName(), question_.getType(),
                          question_.getClass(), cached_message)) {

            LOG_DEBUG(isc::resolve::logger, RESLIB_DBG_CACHE, RESLIB_RUNQ_CACHE_FIND)
                      .arg(questionText(question_));
            // Should these be set by the cache too?
            cached_message.setOpcode(Opcode::QUERY());
            cached_message.setRcode(Rcode::NOERROR());
            cached_message.setHeaderFlag(Message::HEADERFLAG_QR);
            if (handleRecursiveAnswer(cached_message)) {
                callCallback(true);
                stop();
            }
        } else {
            cur_zone_ = deepestDelegation(question_.getName(),
                                          question_.getClass(), cache_);
            LOG_DEBUG(isc::resolve::logger, RESLIB_DBG_CACHE, RESLIB_DEEPEST)
                      .arg(questionText(question_)).arg(cur_zone_);
            send();
        }

    }

    // Send the current question to the given nameserver address
    void sendTo(const isc::nsas::NameserverAddress& address) {
        // We need to keep track of the Address, so that we can update
        // the RTT
        current_ns_address = address;
        gettimeofday(&current_ns_qsent_time, NULL);
        ++outstanding_events_;
        if (test_server_.second != 0) {
            IOFetch query(protocol_, io_, question_,
                test_server_.first,
                test_server_.second, buffer_, this,
                query_timeout_, edns_);
            io_.get_io_service().post(query);
        } else {
            IOFetch query(protocol_, io_, question_,
                current_ns_address.getAddress(),
                53, buffer_, this,
                query_timeout_, edns_);
            io_.get_io_service().post(query);
        }
    }

    // 'general' send, ask the NSAS to give us an address.
    void send(IOFetch::Protocol protocol = IOFetch::UDP, bool edns = true) {
        protocol_ = protocol;   // Store protocol being used for this
        edns_ = edns;
        if (test_server_.second != 0) {
            // Send query to test server
            LOG_DEBUG(isc::resolve::logger,
                      RESLIB_DBG_TRACE, RESLIB_TEST_UPSTREAM)
                .arg(questionText(question_)).arg(test_server_.first);
            gettimeofday(&current_ns_qsent_time, NULL);
            ++outstanding_events_;
            IOFetch query(protocol, io_, question_,
                test_server_.first,
                test_server_.second, buffer_, this,
                query_timeout_, edns_);
            io_.get_io_service().post(query);

        } else {
            // Ask the NSAS for an address for the current zone,
            // the callback will call the actual sendTo()
            LOG_DEBUG(isc::resolve::logger,
                      RESLIB_DBG_TRACE, RESLIB_NSAS_LOOKUP)
                .arg(cur_zone_);

            // Can we have multiple calls to nsas_out? Let's assume not
            // for now
            assert(!nsas_callback_out_);
            nsas_callback_out_ = true;
            nsas_.lookup(cur_zone_, question_.getClass(), nsas_callback_);
        }
    }

    // Called by our NSAS callback handler so we know we do not have
    // an outstanding NSAS call anymore.
    void nsasCallbackCalled() {
        nsas_callback_out_ = false;
    }

    // This function is called by operator() and lookup();
    // We have an answer either from a nameserver or the cache, and
    // we do not know yet if this is a final answer we can send back or
    // that more recursive processing needs to be done.
    // Depending on the content, we go on recursing or return
    //
    // This method also updates the cache, depending on the content
    // of the message
    //
    // returns true if we are done (either we have an answer or an
    //              error message)
    // returns false if we are not done
    bool handleRecursiveAnswer(const Message& incoming) {

        // In case we get a CNAME, we store the target
        // here (classify() will set it when it walks through
        // the cname chain to verify it).
        Name cname_target(question_.getName());

        isc::resolve::ResponseClassifier::Category category =
            isc::resolve::ResponseClassifier::classify(
                question_, incoming, cname_target, cname_count_);

        bool found_ns = false;

        switch (category) {
        case isc::resolve::ResponseClassifier::ANSWER:
        case isc::resolve::ResponseClassifier::ANSWERCNAME:
            // Answer received - copy and return.
            LOG_DEBUG(isc::resolve::logger, RESLIB_DBG_RESULTS, RESLIB_ANSWER)
                      .arg(questionText(question_));
            isc::resolve::copyResponseMessage(incoming, answer_message_);
            cache_.update(*answer_message_);
            return (true);
            break;

        case isc::resolve::ResponseClassifier::CNAME:
            // CNAME received.

            // (unfinished) CNAME. We set our question_ to the CNAME
            // target, then start over at the beginning (for now, that
            // is, we reset our 'current servers' to the root servers).
            if (cname_count_ >= RESOLVER_MAX_CNAME_CHAIN) {
                // CNAME chain too long - just give up
                LOG_DEBUG(isc::resolve::logger, RESLIB_DBG_RESULTS, RESLIB_LONG_CHAIN)
                          .arg(questionText(question_));
                makeSERVFAIL();
                return (true);
            }

            LOG_DEBUG(isc::resolve::logger, RESLIB_DBG_RESULTS, RESLIB_CNAME)
                      .arg(questionText(question_));

            answer_message_->appendSection(Message::SECTION_ANSWER,
                                           incoming);

            question_ = Question(cname_target, question_.getClass(),
                                 question_.getType());

            // Follow CNAME chain.
            LOG_DEBUG(isc::resolve::logger, RESLIB_DBG_RESULTS, RESLIB_FOLLOW_CNAME)
                      .arg(questionText(question_));
            doLookup();
            return (false);
            break;

        case isc::resolve::ResponseClassifier::NXDOMAIN:
        case isc::resolve::ResponseClassifier::NXRRSET:
            // Received NXDOMAIN or NXRRSET, just copy and return
            LOG_DEBUG(isc::resolve::logger, RESLIB_DBG_RESULTS, RESLIB_NXDOM_NXRR)
                      .arg(questionText(question_));
            isc::resolve::copyResponseMessage(incoming, answer_message_);
            // no negcache yet
            //cache_.update(*answer_message_);
            return (true);
            break;

        case isc::resolve::ResponseClassifier::REFERRAL:
            // Response is a referral
            LOG_DEBUG(isc::resolve::logger, RESLIB_DBG_RESULTS, RESLIB_REFERRAL)
                      .arg(questionText(question_));

            cache_.update(incoming);
            // Referral. For now we just take the first glue address
            // we find and continue with that

            // auth section should have at least one RRset
            // and one of them should be an NS (otherwise
            // classifier should have error'd) to a subdomain
            for (RRsetIterator rrsi = incoming.beginSection(Message::SECTION_AUTHORITY);
                 rrsi != incoming.endSection(Message::SECTION_AUTHORITY) && !found_ns;
                 ++rrsi) {
                ConstRRsetPtr rrs = *rrsi;
                if (rrs->getType() == RRType::NS()) {
                    NameComparisonResult compare(Name(cur_zone_).compare(rrs->getName()));
                    if (compare.getRelation() == NameComparisonResult::SUPERDOMAIN) {
                        // TODO: make cur_zone_ a Name instead of a string
                        // (this requires a few API changes in related
                        // libraries, so as not to need many conversions)
                        cur_zone_ = rrs->getName().toText();
                        LOG_DEBUG(isc::resolve::logger, RESLIB_DBG_RESULTS, RESLIB_REFER_ZONE)
                                  .arg(cur_zone_);
                        found_ns = true;
                        break;
                    }
                }
            }

            if (found_ns) {
                // next resolver round
                // we do NOT use doLookup() here, but send() (i.e. we
                // skip the cache), since if we had the final answer
                // instead of a delegation cached, we would have been
                // there by now.
                GlueHints glue_hints(cur_zone_, incoming);

                // Ask the NSAS for an address, or glue.
                // This will eventually result in either sendTo()
                // or stop() being called by nsas_callback_
                assert(!nsas_callback_out_);
                nsas_callback_out_ = true;
                nsas_.lookup(cur_zone_, question_.getClass(),
                             nsas_callback_, ANY_OK, glue_hints);
                return (false);
            } else {
                // Referral was received but did not contain an NS RRset.
                LOG_DEBUG(isc::resolve::logger, RESLIB_DBG_RESULTS, RESLIB_NO_NS_RRSET)
                          .arg(questionText(question_));

                // TODO this will result in answering with the delegation. oh well
                isc::resolve::copyResponseMessage(incoming, answer_message_);
                return (true);
            }
            break;

        case isc::resolve::ResponseClassifier::TRUNCATED:
            // Truncated packet.  If the protocol we used for the last one is
            // UDP, re-query using TCP.  Otherwise regard it as an error.
            if (protocol_ == IOFetch::UDP) {
                LOG_DEBUG(isc::resolve::logger, RESLIB_DBG_RESULTS,
                          RESLIB_TRUNCATED).arg(questionText(question_));
                send(IOFetch::TCP);
                return (false);
            }

            // Was a TCP query so we have received a packet over TCP with the
            // TC bit set: report an error by going to the common
            // error code.
            goto SERVFAIL;

        case isc::resolve::ResponseClassifier::RCODE:
            // see if it's a FORMERR and a potential EDNS problem
            if (incoming.getRcode() == Rcode::FORMERR()) {
                if (protocol_ == IOFetch::UDP && edns_) {
                    // TODO: in case we absolutely need EDNS (i.e. for DNSSEC
                    // aware queries), we might want to try TCP before we give
                    // up. For now, just try UDP, no EDNS
                    send(IOFetch::UDP, false);
                    return (false);
                }

                // TC should take care of non-EDNS over UDP, fall through to
                // SERVFAIL if we get FORMERR instead
            }
            goto SERVFAIL;

        default:
SERVFAIL:
            // Some error in received packet it.  Report it and return SERVFAIL
            // to the caller.
            if (logger.isDebugEnabled()) {
                reportResponseClassifierError(category, incoming.getRcode());
            }
            makeSERVFAIL();
            return (true);
        }

        // If we get here, there is some serious logic error (or a missing
        // "return").
        assert(false);
        return (true);  // To keep the compiler happy
    }

    /// \brief Report classification-detected error
    ///
    /// When the response classifier has detected an error in the response from
    /// an upstream query, this method is called to log a debug message giving
    /// information about the problem.
    ///
    /// \param category Classification code for the packet
    /// \param rcode RCODE value in the packet
    void reportResponseClassifierError(ResponseClassifier::Category category,
                                       const Rcode& rcode)
    {
        // We could set up a table of response classifications to message
        // IDs here and index into that table.  But given that (a) C++ does
        // not have C's named initializers, (b) the codes for the
        // response classifier are in another module and (c) not all messages
        // have the same number of arguments, the setup of the table would be
        // almost as long as the code here: it would need to include a number
        // of assertions to ensure that any change to the the response
        // classifier codes was detected, and the checking logic would need to
        // check that the numeric value of the code lay within the defined
        // limits of the table.

        if (category == ResponseClassifier::RCODE) {

            // Special case as this message takes two arguments.
            LOG_DEBUG(logger, RESLIB_DBG_RESULTS, RESLIB_RCODE_RETURNED).
                      arg(questionText(question_)).arg(rcode);

        } else {

            isc::log::MessageID message_id;
            switch (category) {
            case ResponseClassifier::TRUNCATED:
                message_id = RESLIB_TCP_TRUNCATED;
                break;

            case ResponseClassifier::EMPTY:
                message_id = RESLIB_EMPTY_RESPONSE;
                break;

            case ResponseClassifier::EXTRADATA:
                message_id = RESLIB_EXTRADATA_RESPONSE;
                break;

            case ResponseClassifier::INVNAMCLASS:
                message_id = RESLIB_INVALID_NAMECLASS_RESPONSE;
                break;

            case ResponseClassifier::INVTYPE:
                message_id = RESLIB_INVALID_TYPE_RESPONSE;
                break;

            case ResponseClassifier::MISMATQUEST:
                message_id = RESLIB_INVALID_QNAME_RESPONSE;
                break;

            case ResponseClassifier::MULTICLASS:
                message_id = RESLIB_MULTIPLE_CLASS_RESPONSE;
                break;

            case ResponseClassifier::NOTONEQUEST:
                message_id = RESLIB_NOT_ONE_QNAME_RESPONSE;
                break;

            case ResponseClassifier::NOTRESPONSE:
                message_id = RESLIB_NOT_RESPONSE;
                break;

            case ResponseClassifier::NOTSINGLE:
                message_id = RESLIB_NOTSINGLE_RESPONSE;
                break;

            case ResponseClassifier::OPCODE:
                message_id = RESLIB_OPCODE_RESPONSE;
                break;

            default:
                message_id = RESLIB_ERROR_RESPONSE;
                break;
            }
            LOG_DEBUG(logger, RESLIB_DBG_RESULTS, message_id).
                      arg(questionText(question_));
        }
    }

public:
    RunningQuery(IOService& io,
        const Question& question,
        MessagePtr answer_message,
        std::pair<std::string, uint16_t>& test_server,
        OutputBufferPtr buffer,
        isc::resolve::ResolverInterface::CallbackPtr cb,
        int query_timeout, int client_timeout, int lookup_timeout,
        unsigned retries,
        isc::nsas::NameserverAddressStore& nsas,
        isc::cache::ResolverCache& cache,
        boost::shared_ptr<RttRecorder>& recorder)
        :
        io_(io),
        question_(question),
        query_message_(),
        answer_message_(answer_message),
        test_server_(test_server),
        buffer_(buffer),
        resolvercallback_(cb),
        protocol_(IOFetch::UDP),
        cname_count_(0),
        query_timeout_(query_timeout),
        retries_(retries),
        client_timer(io.get_io_service()),
        lookup_timer(io.get_io_service()),
        done_(false),
        callback_called_(false),
        nsas_(nsas),
        cache_(cache),
        cur_zone_("."),
        nsas_callback_(),
        nsas_callback_out_(false),
        outstanding_events_(0),
        rtt_recorder_(recorder)
    {
        // Set here to avoid using "this" in initializer list.
        nsas_callback_.reset(new ResolverNSASCallback(this));

        // Setup the timer to stop trying (lookup_timeout)
        if (lookup_timeout >= 0) {
            lookup_timer.expires_from_now(
                boost::posix_time::milliseconds(lookup_timeout));
            ++outstanding_events_;
            lookup_timer.async_wait(boost::bind(&RunningQuery::lookupTimeout, this));
        }

        // Setup the timer to send an answer (client_timeout)
        if (client_timeout >= 0) {
            client_timer.expires_from_now(
                boost::posix_time::milliseconds(client_timeout));
            ++outstanding_events_;
            client_timer.async_wait(boost::bind(&RunningQuery::clientTimeout, this));
        }

        doLookup();
    }

    virtual ~RunningQuery() {};

    // called if we have a lookup timeout; if our callback has
    // not been called, call it now. Then stop.
    void lookupTimeout() {
        if (!callback_called_) {
            makeSERVFAIL();
            callCallback(true);
        }
        assert(outstanding_events_ > 0);
        --outstanding_events_;
        stop();
    }

    // called if we have a client timeout; if our callback has
    // not been called, call it now. But do not stop.
    void clientTimeout() {
        if (!callback_called_) {
            makeSERVFAIL();
            callCallback(true);
        }
        assert(outstanding_events_ > 0);
        --outstanding_events_;
        if (outstanding_events_ == 0) {
            stop();
        }
    }

    // If the callback has not been called yet, call it now
    // If success is true, we call 'success' with our answer_message
    // If it is false, we call failure()
    void callCallback(bool success) {
        if (!callback_called_) {
            callback_called_ = true;

            // There are two types of messages we could store in the
            // cache;
            // 1. answers to our fetches from authoritative servers,
            //    exactly as we receive them, and
            // 2. answers to queries we received from clients, which
            //    have received additional processing (following CNAME
            //    chains, for instance)
            //
            // Doing only the first would mean we would have to re-do
            // processing when we get data from our cache, and doing
            // only the second would miss out on the side-effect of
            // having nameserver data in our cache.
            //
            // So right now we do both. Since the cache (currently)
            // stores Messages on their question section only, this
            // does mean that we overwrite the messages we stored in
            // the previous iteration if we are following a delegation.
            if (success) {
                resolvercallback_->success(answer_message_);
            } else {
                resolvercallback_->failure();
            }
        }
    }

    // We are done. If there are no more outstanding events, we delete
    // ourselves. If there are any, we do not.
    void stop() {
        done_ = true;
        if (nsas_callback_out_) {
            nsas_.cancel(cur_zone_, question_.getClass(), nsas_callback_);
            nsas_callback_out_ = false;
        }
        client_timer.cancel();
        lookup_timer.cancel();
        if (outstanding_events_ > 0) {
            return;
        } else {
            delete this;
        }
    }

    // This function is used as callback from DNSQuery.
    virtual void operator()(IOFetch::Result result) {
        // XXX is this the place for TCP retry?
        assert(outstanding_events_ > 0);
        --outstanding_events_;

        if (!done_ && result != IOFetch::TIME_OUT) {
            // we got an answer

            // Update the NSAS with the time it took
            struct timeval cur_time;
            gettimeofday(&cur_time, NULL);
            uint32_t rtt = 0;

            // Only calculate RTT if it is positive
            if (cur_time.tv_sec > current_ns_qsent_time.tv_sec ||
                (cur_time.tv_sec == current_ns_qsent_time.tv_sec &&
                 cur_time.tv_usec > current_ns_qsent_time.tv_usec)) {
                rtt = 1000 * (cur_time.tv_sec - current_ns_qsent_time.tv_sec);
                rtt += (cur_time.tv_usec - current_ns_qsent_time.tv_usec) / 1000;
            }
            LOG_DEBUG(isc::resolve::logger, RESLIB_DBG_RESULTS, RESLIB_RTT).arg(rtt);
            current_ns_address.updateRTT(rtt);
            if (rtt_recorder_) {
                rtt_recorder_->addRtt(rtt);
            }

            try {
                Message incoming(Message::PARSE);
                InputBuffer ibuf(buffer_->getData(), buffer_->getLength());

                incoming.fromWire(ibuf);

                buffer_->clear();
                done_ = handleRecursiveAnswer(incoming);
                if (done_) {
                    callCallback(true);
                    stop();
                }
            } catch (const isc::dns::DNSProtocolError& dpe) {
                // Right now, we treat this similar to timeouts
                // (except we don't store RTT)
                // We probably want to make this an integral part
                // of the fetch data process. (TODO)
                if (retries_--) {
                    // Retry
                    LOG_DEBUG(isc::resolve::logger, RESLIB_DBG_RESULTS,
                              RESLIB_PROTOCOL_RETRY)
                              .arg(questionText(question_)).arg(dpe.what())
                              .arg(retries_);
                    send();
                } else {
                    // Give up
                    LOG_DEBUG(isc::resolve::logger, RESLIB_DBG_RESULTS,
                              RESLIB_PROTOCOL)
                              .arg(questionText(question_)).arg(dpe.what());
                    if (!callback_called_) {
                        makeSERVFAIL();
                        callCallback(true);
                    }
                    stop();
                }
            }
        } else if (!done_ && retries_--) {
            // Query timed out, but we have some retries, so send again
            LOG_DEBUG(isc::resolve::logger, RESLIB_DBG_RESULTS, RESLIB_TIMEOUT_RETRY)
                      .arg(questionText(question_))
                      .arg(current_ns_address.getAddress().toText()).arg(retries_);
            current_ns_address.updateRTT(isc::nsas::AddressEntry::UNREACHABLE);
            send();
        } else {
            // We are either already done, or out of retries
            if (result == IOFetch::TIME_OUT) {
                LOG_DEBUG(isc::resolve::logger, RESLIB_DBG_RESULTS, RESLIB_TIMEOUT)
                          .arg(questionText(question_))
                          .arg(current_ns_address.getAddress().toText());
                current_ns_address.updateRTT(isc::nsas::AddressEntry::UNREACHABLE);
            }
            if (!callback_called_) {
                makeSERVFAIL();
                callCallback(true);
            }
            stop();
        }
    }

    // Clear the answer parts of answer_message, and set the rcode
    // to servfail
    void makeSERVFAIL() {
        if (answer_message_) {
            isc::resolve::makeErrorMessage(answer_message_, Rcode::SERVFAIL());
        }
    }
};

class ForwardQuery : public IOFetch::Callback, public AbstractRunningQuery {
private:
    // The io service to handle async calls
    IOService& io_;

    // This is the query message got from client
    ConstMessagePtr query_message_;

    // This is where we build and store our final answer
    MessagePtr answer_message_;

    // List of nameservers to forward to
    boost::shared_ptr<AddressVector> upstream_;

    // Buffer to store the result.
    OutputBufferPtr buffer_;

    // This will be notified when we succeed or fail
    isc::resolve::ResolverInterface::CallbackPtr resolvercallback_;

    /*
     * TODO Do something more clever with timeouts. In the long term, some
     *     computation of average RTT, increase with each retry, etc.
     */
    // Timeout information
    int query_timeout_;

    // TODO: replace by our wrapper
    asio::deadline_timer client_timer;
    asio::deadline_timer lookup_timer;

    // Make ForwardQuery deletes itself safely. for more information see
    // the comments of outstanding_events in RunningQuery.
    size_t outstanding_events_;

    // If we have a client timeout, we call back with a failure message,
    // but we do not stop yet. We use this variable to make sure we
    // don't call back a second time later
    bool callback_called_;

    // send the query to the server.
    void send(IOFetch::Protocol protocol = IOFetch::UDP) {
        const int uc = upstream_->size();
        buffer_->clear();
        int serverIndex = rand() % uc;
        ConstQuestionPtr question = *(query_message_->beginQuestion());
        LOG_DEBUG(isc::resolve::logger, RESLIB_DBG_TRACE, RESLIB_UPSTREAM)
            .arg(questionText(*question))
            .arg(upstream_->at(serverIndex).first);

        ++outstanding_events_;
        // Forward the query, create the IOFetch with
        // query message, so that query flags can be forwarded
        // together.
        IOFetch query(protocol, io_, query_message_,
            upstream_->at(serverIndex).first,
            upstream_->at(serverIndex).second,
            buffer_, this, query_timeout_);

        io_.get_io_service().post(query);
    }

public:
    ForwardQuery(IOService& io,
        ConstMessagePtr query_message,
        MessagePtr answer_message,
        boost::shared_ptr<AddressVector> upstream,
        OutputBufferPtr buffer,
        isc::resolve::ResolverInterface::CallbackPtr cb,
        int query_timeout, int client_timeout, int lookup_timeout) :
        io_(io),
        query_message_(query_message),
        answer_message_(answer_message),
        upstream_(upstream),
        buffer_(buffer),
        resolvercallback_(cb),
        query_timeout_(query_timeout),
        client_timer(io.get_io_service()),
        lookup_timer(io.get_io_service()),
        outstanding_events_(0),
        callback_called_(false)
    {
        // Setup the timer to stop trying (lookup_timeout)
        if (lookup_timeout >= 0) {
            lookup_timer.expires_from_now(
                boost::posix_time::milliseconds(lookup_timeout));
            ++outstanding_events_;
            lookup_timer.async_wait(boost::bind(&ForwardQuery::lookupTimeout, this));
        }

        // Setup the timer to send an answer (client_timeout)
        if (client_timeout >= 0) {
            client_timer.expires_from_now(
                boost::posix_time::milliseconds(client_timeout));
            ++outstanding_events_;
            client_timer.async_wait(boost::bind(&ForwardQuery::clientTimeout, this));
        }

        send();
    }

    virtual ~ForwardQuery() {};

    virtual void lookupTimeout() {
        if (!callback_called_) {
            makeSERVFAIL();
            callCallback(false);
        }
        assert(outstanding_events_ > 0);
        --outstanding_events_;
        stop();
    }

    virtual void clientTimeout() {
        if (!callback_called_) {
            makeSERVFAIL();
            callCallback(false);
        }
        assert(outstanding_events_ > 0);
        --outstanding_events_;
        stop();
    }

    // If the callback has not been called yet, call it now
    // If success is true, we call 'success' with our answer_message
    // If it is false, we call failure()
    void callCallback(bool success) {
        if (!callback_called_) {
            callback_called_ = true;
            if (success) {
                resolvercallback_->success(answer_message_);
            } else {
                resolvercallback_->failure();
            }
        }
    }

    virtual void stop() {
        // if we cancel our timers, we will still get an event for
        // that, so we cannot delete ourselves just yet (those events
        // would be bound to a deleted object)
        // cancel them one by one, both cancels should get us back
        // here again.
        // same goes if we have an outstanding query (can't delete
        // until that one comes back to us)
        lookup_timer.cancel();
        client_timer.cancel();
        if (outstanding_events_ > 0) {
            return;
        } else {
            delete this;
        }
    }

    // This function is used as callback from DNSQuery.
    virtual void operator()(IOFetch::Result result) {
        // XXX is this the place for TCP retry?
        assert(outstanding_events_ > 0);
        --outstanding_events_;
        if (result != IOFetch::TIME_OUT) {
            // we got an answer
            Message incoming(Message::PARSE);
            InputBuffer ibuf(buffer_->getData(), buffer_->getLength());
            incoming.fromWire(ibuf);
            isc::resolve::copyResponseMessage(incoming, answer_message_);
            callCallback(true);
        }

        stop();
    }

    // Clear the answer parts of answer_message, and set the rcode
    // to servfail
    void makeSERVFAIL() {
        isc::resolve::makeErrorMessage(answer_message_, Rcode::SERVFAIL());
    }
};

}

AbstractRunningQuery*
RecursiveQuery::resolve(const QuestionPtr& question,
    const isc::resolve::ResolverInterface::CallbackPtr callback)
{
    IOService& io = dns_service_.getIOService();

    MessagePtr answer_message(new Message(Message::RENDER));
    isc::resolve::initResponseMessage(*question, *answer_message);

    OutputBufferPtr buffer(new OutputBuffer(0));

    // First try to see if we have something cached in the messagecache
    LOG_DEBUG(isc::resolve::logger, RESLIB_DBG_TRACE, RESLIB_RESOLVE)
              .arg(questionText(*question)).arg(1);
    if (cache_.lookup(question->getName(), question->getType(),
                      question->getClass(), *answer_message) &&
        answer_message->getRRCount(Message::SECTION_ANSWER) > 0) {
        // Message found, return that
        LOG_DEBUG(isc::resolve::logger, RESLIB_DBG_CACHE, RESLIB_RECQ_CACHE_FIND)
                  .arg(questionText(*question)).arg(1);

        // TODO: err, should cache set rcode as well?
        answer_message->setRcode(Rcode::NOERROR());
        callback->success(answer_message);
    } else {
        // Perhaps we only have the one RRset?
        // TODO: can we do this? should we check for specific types only?
        RRsetPtr cached_rrset = cache_.lookup(question->getName(),
                                              question->getType(),
                                              question->getClass());
        if (cached_rrset) {
            // Found single RRset in cache
            LOG_DEBUG(isc::resolve::logger, RESLIB_DBG_CACHE, RESLIB_RRSET_FOUND)
                      .arg(questionText(*question)).arg(1);
            answer_message->addRRset(Message::SECTION_ANSWER,
                                     cached_rrset);
            answer_message->setRcode(Rcode::NOERROR());
            callback->success(answer_message);
        } else {
            // Message not found in cache, start recursive query.  It will
            // delete itself when it is done
            LOG_DEBUG(isc::resolve::logger, RESLIB_DBG_TRACE, RESLIB_RECQ_CACHE_NO_FIND)
                      .arg(questionText(*question)).arg(1);
            return (new RunningQuery(io, *question, answer_message,
                                     test_server_, buffer, callback,
                                     query_timeout_, client_timeout_,
                                     lookup_timeout_, retries_, nsas_,
                                     cache_, rtt_recorder_));
        }
    }
    return (NULL);
}

AbstractRunningQuery*
RecursiveQuery::resolve(const Question& question,
                        MessagePtr answer_message,
                        OutputBufferPtr buffer,
                        DNSServer* server)
{
    // XXX: eventually we will need to be able to determine whether
    // the message should be sent via TCP or UDP, or sent initially via
    // UDP and then fall back to TCP on failure, but for the moment
    // we're only going to handle UDP.
    IOService& io = dns_service_.getIOService();

    isc::resolve::ResolverInterface::CallbackPtr crs(
        new isc::resolve::ResolverCallbackServer(server));

    // TODO: general 'prepareinitialanswer'
    answer_message->setOpcode(isc::dns::Opcode::QUERY());
    answer_message->addQuestion(question);

    // First try to see if we have something cached in the messagecache
    LOG_DEBUG(isc::resolve::logger, RESLIB_DBG_TRACE, RESLIB_RESOLVE)
              .arg(questionText(question)).arg(2);

    if (cache_.lookup(question.getName(), question.getType(),
                      question.getClass(), *answer_message) &&
        answer_message->getRRCount(Message::SECTION_ANSWER) > 0) {

        // Message found, return that
        LOG_DEBUG(isc::resolve::logger, RESLIB_DBG_CACHE, RESLIB_RECQ_CACHE_FIND)
                  .arg(questionText(question)).arg(2);
        // TODO: err, should cache set rcode as well?
        answer_message->setRcode(Rcode::NOERROR());
        crs->success(answer_message);
    } else {
        // Perhaps we only have the one RRset?
        // TODO: can we do this? should we check for specific types only?
        RRsetPtr cached_rrset = cache_.lookup(question.getName(),
                                              question.getType(),
                                              question.getClass());
        if (cached_rrset) {
            // Found single RRset in cache
            LOG_DEBUG(isc::resolve::logger, RESLIB_DBG_CACHE, RESLIB_RRSET_FOUND)
                      .arg(questionText(question)).arg(2);
            answer_message->addRRset(Message::SECTION_ANSWER,
                                     cached_rrset);
            answer_message->setRcode(Rcode::NOERROR());
            crs->success(answer_message);

        } else {
            // Message not found in cache, start recursive query.  It will
            // delete itself when it is done
            LOG_DEBUG(isc::resolve::logger, RESLIB_DBG_TRACE, RESLIB_RECQ_CACHE_NO_FIND)
                      .arg(questionText(question)).arg(2);
            return (new RunningQuery(io, question, answer_message,
                                     test_server_, buffer, crs, query_timeout_,
                                     client_timeout_, lookup_timeout_, retries_,
                                     nsas_, cache_, rtt_recorder_));
        }
    }
    return (NULL);
}

AbstractRunningQuery*
RecursiveQuery::forward(ConstMessagePtr query_message,
    MessagePtr answer_message,
    OutputBufferPtr buffer,
    DNSServer* server,
    isc::resolve::ResolverInterface::CallbackPtr callback)
{
    // XXX: eventually we will need to be able to determine whether
    // the message should be sent via TCP or UDP, or sent initially via
    // UDP and then fall back to TCP on failure, but for the moment
    // we're only going to handle UDP.
    IOService& io = dns_service_.getIOService();

    if (!callback) {
        callback.reset(new isc::resolve::ResolverCallbackServer(server));
    }

    // TODO: general 'prepareinitialanswer'
    answer_message->setOpcode(isc::dns::Opcode::QUERY());
    ConstQuestionPtr question = *query_message->beginQuestion();
    answer_message->addQuestion(*question);

    // implement the simplest forwarder, which will pass
    // everything throught without interpretation, except
    // QID, port number. The response will not be cached.
    // It will delete itself when it is done
    return (new ForwardQuery(io, query_message, answer_message,
                             upstream_, buffer, callback, query_timeout_,
                             client_timeout_, lookup_timeout_));
}

} // namespace asiodns
} // namespace isc
