// Copyright (C) 2010  Internet Systems Consortium, Inc. ("ISC")
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

#ifndef __STATISTICS_H
#define __STATISTICS_H 1

#include <cc/session.h>

#include <dns/message.h>

#include <string>

#include <stdint.h>
#include <boost/scoped_ptr.hpp>

namespace isc {
namespace auth {
namespace statistics {

class CountersImpl;

class QRAttributes {
/// \brief Query/Response attributes for statistics.
///
/// This class holds some attributes related to a query/response
/// for statistics data collection.
///
/// This class does not have getter methods since it exposes private members
/// to \c CountersImpl directly.
friend class CountersImpl;
private:
    // request attributes
    int req_ip_version_;            // IP version
    int req_transport_protocol_;    // Transport layer protocol
    int req_opcode_;                // OpCode
    bool req_is_edns_0_;            // EDNS ver.0
    bool req_is_edns_badver_;       // other EDNS version
    bool req_is_dnssec_ok_;         // DO bit
    bool req_is_tsig_;              // signed with valid TSIG
    bool req_is_sig0_;              // signed with valid SIG(0)
    bool req_is_badsig_;            // signed but bad signature
    // zone origin
    std::string zone_origin_;       // zone origin
    // response attributes
    bool answer_sent_;              // DNS message has sent
    bool res_is_truncated_;         // DNS message is truncated
public:
    /// The constructor.
    ///
    /// This constructor is mostly exception free. But it may still throw
    /// a standard exception if memory allocation fails inside the method.
    ///
    QRAttributes();
    /// The destructor.
    ///
    /// This method never throws an exception.
    ///
    ~QRAttributes();
    /// \brief Set query opcode.
    /// \throw None
    void setQueryOpCode(const int opcode);
    /// \brief Set IP version carrying a query.
    /// \throw None
    void setQueryIPVersion(const int ip_version);
    /// \brief Set transport protocol carrying a query.
    /// \throw None
    void setQueryTransportProtocol(const int transport_protocol);
    /// \brief Set query EDNS attributes.
    /// \throw None
    void setQueryEDNS(const bool is_edns_0, const bool is_edns_badver);
    /// \brief Set query DO bit.
    /// \throw None
    void setQueryDO(const bool is_dnssec_ok);
    /// \brief Set query TSIG attributes.
    /// \throw None
    void setQuerySig(const bool is_tsig, const bool is_sig0,
                     const bool is_badsig);
    /// \brief Set zone origin.
    /// \throw None
    void setOrigin(const std::string& origin);
    /// \brief Set if the answer has sent.
    /// \throw None
    void answerHasSent();
    /// \brief Set if the response is truncated.
    /// \throw None
    void setResponseTruncated(const bool is_truncated);
};

/// \brief Set of query counters.
///
/// \c Counters is set of query counters class. It holds query counters
/// and provides an interface to increment the counter of specified type
/// (e.g. UDP query, TCP query).
///
/// This class also provides a function to send statistics information to
/// statistics module.
///
/// This class is designed to be a part of \c AuthSrv.
/// Call \c setStatisticsSession() to set a session to communicate with
/// statistics module like Xfrin session.
/// Call \c inc() to increment a counter for the query.
/// Call \c submitStatistics() to submit statistics information to statistics
/// module with statistics_session, periodically or at a time the command
/// \c sendstats is received.
///
/// We may eventually want to change the structure to hold values that are
/// not counters (such as concurrent TCP connections), or seperate generic
/// part to src/lib to share with the other modules.
///
/// This class uses pimpl idiom and hides detailed implementation.
/// This class is constructed on startup of the server, so
/// construction overhead of this approach should be acceptable.
///
/// \todo Hold counters for each query types (Notify, Axfr, Ixfr, Normal)
/// \todo Consider overhead of \c Counters::inc()
class Counters {
private:
    boost::scoped_ptr<CountersImpl> impl_;
public:
    // Enum for the type of counter
    enum ServerCounterType {
        SERVER_UDP_QUERY,       ///< SERVER_UDP_QUERY: counter for UDP queries
        SERVER_TCP_QUERY,       ///< SERVER_TCP_QUERY: counter for TCP queries
        SERVER_COUNTER_TYPES    ///< The number of defined counters
    };
    /// The constructor.
    ///
    /// This constructor is mostly exception free. But it may still throw
    /// a standard exception if memory allocation fails inside the method.
    ///
    Counters();
    /// The destructor.
    ///
    /// This method never throws an exception.
    ///
    ~Counters();

    /// \brief Increment counters according to the parameters.
    ///
    /// \param qrattrs Query/Response attributes.
    /// \param response DNS response message.
    ///
    /// \throw None
    ///
    void inc(const QRAttributes& qrattrs, const isc::dns::Message& response);

    /// \brief Submit statistics counters to statistics module.
    ///
    /// This method is desinged to be called periodically
    /// with \c asio_link::StatisticsSendTimer, or arbitrary
    /// by the command 'sendstats'.
    ///
    /// Note: Set the session to communicate with statistics module
    /// by \c setStatisticsSession() before calling \c submitStatistics().
    ///
    /// This method is mostly exception free (error conditions are
    /// represented via the return value). But it may still throw
    /// a standard exception if memory allocation fails inside the method.
    ///
    /// \return true on success, false on error.
    ///
    /// \todo Do not block message handling in auth_srv while submitting
    /// statistics data.
    ///
    bool submitStatistics() const;

    /// \brief Set the session to communicate with Statistics
    /// module.
    ///
    /// This method never throws an exception.
    ///
    /// Note: this interface is tentative.  We'll revisit the ASIO and session
    /// frameworks, at which point the session will probably be passed on
    /// construction of the server.
    ///
    /// Ownership isn't transferred: the caller is responsible for keeping
    /// this object to be valid while the server object is working and for
    /// disconnecting the session and destroying the object when the server
    /// is shutdown.
    ///
    /// \param statistics_session A pointer to the session
    ///
    void setStatisticsSession(isc::cc::AbstractSession* statistics_session);

    /// \brief Get the value of a counter in the Counters.
    ///
    /// This function returns a value of the counter specified by \a type.
    /// This method never throws an exception.
    ///
    /// Note: Currently this function is for testing purpose only.
    ///
    /// \param type Type of a counter to get the value of
    ///
    /// \return the value of the counter specified by \a type.
    uint64_t getCounter(const Counters::ServerCounterType type) const;

    /// \brief Get the value of a per opcode counter.
    ///
    /// This method returns the value of the per opcode counter for the
    /// specified \c opcode.
    ///
    /// \note This is a tentative interface as an attempt of experimentally
    /// supporting more statistics counters.  This should eventually be more
    /// generalized.  In any case, this method is mainly for testing.
    ///
    /// \throw None
    /// \param opcode The opcode of the counter to get the value of
    /// \return the value of the counter.
    uint64_t getCounter(const isc::dns::Opcode opcode) const;

    /// \brief Get the value of a per rcode counter.
    ///
    /// This method returns the value of the per rcode counter for the
    /// specified \c rcode.
    ///
    /// \note As mentioned in getCounter(const isc::dns::Opcode opcode),
    /// This is a tentative interface as an attempt of experimentally
    /// supporting more statistics counters.  This should eventually be more
    /// generalized.  In any case, this method is mainly for testing.
    ///
    /// \throw None
    /// \param rcode The rcode of the counter to get the value of
    /// \return the value of the counter.
    uint64_t getCounter(const isc::dns::Rcode rcode) const;

    /// \brief A type of validation function for the specification in
    /// isc::config::ModuleSpec.
    ///
    /// This type might be useful for not only statistics
    /// specificatoin but also for config_data specification and for
    /// command.
    ///
    typedef boost::function<bool(const isc::data::ConstElementPtr&)>
    validator_type;

    /// \brief Register a function type of the statistics validation
    /// function for Counters.
    ///
    /// This method never throws an exception.
    ///
    /// \param validator A function type of the validation of
    /// statistics specification.
    ///
    void registerStatisticsValidator(Counters::validator_type validator) const;
};

} // namespace statistics
} // namespace auth
} // namespace isc


#endif // __STATISTICS_H

// Local Variables:
// mode: c++
// End:
