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

#include <auth/statistics.h>
#include <auth/statistics_items.h>
#include <auth/auth_log.h>

#include <dns/opcode.h>
#include <dns/rcode.h>

#include <cc/data.h>
#include <cc/session.h>

#include <statistics/counter.h>
#include <statistics/counter_dict.h>

#include <algorithm>
#include <cctype>
#include <cassert>
#include <string>
#include <sstream>
#include <iostream>

#include <boost/noncopyable.hpp>
#include <boost/foreach.hpp>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

using namespace isc::dns;
using namespace isc::auth;
using namespace isc::statistics;

namespace {
using namespace isc::data;
using isc::statistics::Counter;
using isc::auth::statistics::Counters;
void
fillNodes(const Counter &counter, const char *nodename[], const size_t size,
          const std::string &prefix, Counters::item_tree_type &trees)
{
    for (size_t i = 0; i < size; ++i) {
        trees->set (prefix + nodename[i],
                    Element::create( static_cast<long int>( counter.get(i) ) )
                    );
    }
}
} // anonymous namespace

namespace isc {
namespace auth {
namespace statistics {

class CountersImpl : boost::noncopyable {
public:
    CountersImpl();
    ~CountersImpl();
    void inc(const QRAttributes& qrattrs, const Message& response);
    const Counters::item_tree_type
        get(const Counters::item_node_name_set_type& trees) const;
    // Currently for testing purpose only
    const Counters::item_tree_type dump() const;
    bool submitStatistics() const;
    void registerStatisticsValidator (Counters::validator_type validator);
    // Currently for testing purpose only
private:
    // counter for server
    Counter server_qr_counter_;
    Counter server_socket_counter_;
    // set of counters for zones
    CounterDictionary zone_qr_counters_;
    // validator
    Counters::validator_type validator_;
};

CountersImpl::CountersImpl() :
    // initialize counter
    // size of server_qr_counter_, zone_qr_counters_: QR_COUNTER_TYPES
    // size of server_socket_counter_: SOCKET_COUNTER_TYPES
    server_qr_counter_(QR_COUNTER_TYPES),
    server_socket_counter_(SOCKET_COUNTER_TYPES),
    zone_qr_counters_(QR_COUNTER_TYPES),
    validator_()
{}

CountersImpl::~CountersImpl()
{}

void
CountersImpl::inc(const QRAttributes& qrattrs, const Message& response) {
    // protocols carrying request
    if (qrattrs.req_ip_version_ == AF_INET) {
        server_qr_counter_.inc(QR_REQUEST_IPV4);
    } else if (qrattrs.req_ip_version_ == AF_INET6) {
        server_qr_counter_.inc(QR_REQUEST_IPV6);
    }
    if (qrattrs.req_transport_protocol_ == IPPROTO_UDP) {
        server_qr_counter_.inc(QR_REQUEST_UDP);
    } else if (qrattrs.req_transport_protocol_ == IPPROTO_TCP) {
        server_qr_counter_.inc(QR_REQUEST_TCP);
    }

    // query TSIG
    if (qrattrs.req_is_tsig_) {
        server_qr_counter_.inc(QR_REQUEST_TSIG);
    }
    if (qrattrs.req_is_sig0_) {
        server_qr_counter_.inc(QR_REQUEST_SIG0);
    }
    if (qrattrs.req_is_badsig_) {
        server_qr_counter_.inc(QR_REQUEST_BADSIG);
    }

    // query EDNS
    if (qrattrs.req_is_edns_0_) {
        server_qr_counter_.inc(QR_REQUEST_EDNS0);
    }
    if (qrattrs.req_is_edns_badver_) {
        server_qr_counter_.inc(QR_REQUEST_BADEDNSVER);
    }

    // query DNSSEC
    if (qrattrs.req_is_dnssec_ok_) {
        server_qr_counter_.inc(QR_REQUEST_DNSSEC_OK);
    }

    // QTYPE
    unsigned int qtype_type = QR_QTYPE_OTHER;
    const QuestionIterator qiter = response.beginQuestion();
    if (qiter != response.endQuestion()) {
        // get the first and only question section
        const QuestionPtr qptr = *qiter;
        if (qptr != NULL) {
            // get the qtype code
            const unsigned int qtype = qptr->getType().getCode();
            if (qtype == 0) {
                // qtype 0
                qtype_type = QR_QTYPE_OTHER;
            } else if (qtype < 52) {
                // qtype 1..51
                qtype_type = QR_QTYPE_A + (qtype - 1);
            } else if (qtype < 55) {
                // qtype 52..54
                qtype_type = QR_QTYPE_OTHER;
            } else if (qtype < 59) {
                // qtype 55..58
                qtype_type = QR_QTYPE_HIP + (qtype - 55);
            } else if (qtype < 99) {
                // qtype 59..98
                qtype_type = QR_QTYPE_OTHER;
            } else if (qtype < 104) {
                // qtype 99..103
                qtype_type = QR_QTYPE_SPF + (qtype - 99);
            } else if (qtype < 249) {
                // qtype 104..248
                qtype_type = QR_QTYPE_OTHER;
            } else if (qtype < 255) {
                // qtype 249..254
                qtype_type = QR_QTYPE_TKEY + (qtype - 249);
            } else if (qtype == 255) {
                // qtype 255: all records
                qtype_type = QR_QTYPE_OTHER;
            } else if (qtype < 258) {
                // qtype 256..257
                qtype_type = QR_QTYPE_URI + (qtype - 256);
            } else if (qtype < 32768) {
                // qtype 258..32767
                qtype_type = QR_QTYPE_OTHER;
            } else if (qtype < 32770) {
                // qtype 32768..32769
                qtype_type = QR_QTYPE_TA + (qtype - 32768);
            } else {
                // qtype 32770..65535
                qtype_type = QR_QTYPE_OTHER;
            }
        }
    }
    server_qr_counter_.inc(qtype_type);
    // OPCODE
    unsigned int opcode_type = QR_OPCODE_OTHER;
    if (qrattrs.req_opcode_ < 3) {
        // opcode 0..2
        opcode_type = QR_OPCODE_QUERY + qrattrs.req_opcode_;
    } else if (qrattrs.req_opcode_ == 3) {
        // opcode 3 is reserved
        opcode_type = QR_OPCODE_OTHER;
    } else if (qrattrs.req_opcode_ < 6) {
        // opcode 4..5
        opcode_type = QR_OPCODE_NOTIFY + (qrattrs.req_opcode_ - 4);
    } else {
        // opcode larger than 6 is reserved
        opcode_type = QR_OPCODE_OTHER;
    }
    server_qr_counter_.inc(opcode_type);

    // response
    if (qrattrs.answer_sent_) {
        // responded
        server_qr_counter_.inc(QR_RESPONSE);

        // response truncated
        if (qrattrs.res_is_truncated_) {
            server_qr_counter_.inc(QR_RESPONSE_TRUNCATED);
        }

        // response EDNS
        ConstEDNSPtr response_edns = response.getEDNS();
        if (response_edns != NULL && response_edns->getVersion() == 0) {
            server_qr_counter_.inc(QR_RESPONSE_EDNS0);
        }

        // response TSIG
        if (qrattrs.req_is_tsig_) {
            // assume response is TSIG signed if request is TSIG signed
            server_qr_counter_.inc(QR_RESPONSE_TSIG);
        }

        // response SIG(0) is currently not implemented

        // RCODE
        const unsigned int rcode = response.getRcode().getCode();
        unsigned int rcode_type = QR_RCODE_OTHER;
        if (rcode < 11) {
            // rcode 0..10
            rcode_type = QR_RCODE_NOERROR + rcode;
        } else if (rcode < 16) {
            // rcode 11..15 is reserved
            rcode_type = QR_RCODE_OTHER;
        } else if (rcode == 16) {
            // rcode 16
            rcode_type = QR_RCODE_BADSIGVERS;
        } else if (rcode < 23) {
            // rcode 17..22
            rcode_type = QR_RCODE_BADKEY + (rcode - 17);
        } else {
            // opcode larger than 22 is reserved or unassigned
            rcode_type = QR_RCODE_OTHER;
        }
        server_qr_counter_.inc(rcode_type);

        // compound attributes
        const unsigned int answer_rrs =
            response.getRRCount(Message::SECTION_ANSWER);
        const bool is_aa_set = response.getHeaderFlag(Message::HEADERFLAG_AA);

        if (is_aa_set) {
            // QryAuthAns
            server_qr_counter_.inc(QR_QRYAUTHANS);
        } else {
            // QryNoAuthAns
            server_qr_counter_.inc(QR_QRYNOAUTHANS);
        }

        if (rcode == Rcode::NOERROR_CODE) {
            if (answer_rrs > 0) {
                // QrySuccess
                server_qr_counter_.inc(QR_QRYSUCCESS);
            } else {
                if (is_aa_set) {
                    // QryNxrrset
                    server_qr_counter_.inc(QR_QRYNXRRSET);
                } else {
                    // QryReferral
                    server_qr_counter_.inc(QR_QRYREFERRAL);
                }
            }
        }
    }
}


void
CountersImpl::registerStatisticsValidator
    (Counters::validator_type validator)
{
    validator_ = validator;
}

const Counters::item_tree_type
CountersImpl::get(const Counters::item_node_name_set_type &trees) const {
    using namespace isc::data;

    Counters::item_tree_type item_tree = Element::createMap();

    BOOST_FOREACH(const Counters::item_node_name_type& node, trees) {
        if (node == "auth.server.qr") {
            fillNodes(server_qr_counter_, QRCounterItemName, QR_COUNTER_TYPES,
                      "auth.server.qr.", item_tree);
        } else if (node == "auth.server.socket") {
            // currently not implemented
            fillNodes(server_socket_counter_, SocketCounterItemName,
                      SOCKET_COUNTER_TYPES, "auth.server.socket.", item_tree);
        } else if (node == "auth.zones") {
            // currently not implemented
        } else {
            // unknown tree
        }
    }

    return (item_tree);
}

// Currently for testing purpose only
const Counters::item_tree_type
CountersImpl::dump() const {
    using namespace isc::data;

    Counters::item_tree_type item_tree = Element::createMap();

    fillNodes(server_qr_counter_, QRCounterItemName, QR_COUNTER_TYPES,
              "auth.server.qr.", item_tree);

    return (item_tree);
}

Counters::Counters() : impl_(new CountersImpl())
{}

Counters::~Counters() {}

void
Counters::inc(const QRAttributes& qrattrs, const Message& response) {
    impl_->inc(qrattrs, response);
}

const Counters::item_tree_type
Counters::get(const Counters::item_node_name_set_type &trees) const {
    return (impl_->get(trees));
}

const Counters::item_tree_type
Counters::dump() const {
    return (impl_->dump());
}

void
Counters::registerStatisticsValidator
    (Counters::validator_type validator) const
{
    return (impl_->registerStatisticsValidator(validator));
}

QRAttributes::QRAttributes() :
    req_ip_version_(0), req_transport_protocol_(0),
    req_opcode_(0),
    req_is_edns_0_(false), req_is_edns_badver_(false),
    req_is_dnssec_ok_(false),
    req_is_tsig_(false), req_is_sig0_(false), req_is_badsig_(false),
    zone_origin_(),
    answer_sent_(false),
    res_is_truncated_(false)
{}

QRAttributes::~QRAttributes()
{}

void
QRAttributes::setQueryIPVersion(const int ip_version) {
    req_ip_version_ = ip_version;
}

void
QRAttributes::setQueryTransportProtocol(const int transport_protocol) {
    req_transport_protocol_ = transport_protocol;
}

void
QRAttributes::setQueryOpCode(const int opcode) {
    req_opcode_ = opcode;
}

void
QRAttributes::setQueryEDNS(const bool is_edns_0, const bool is_edns_badver) {
    req_is_edns_0_ = is_edns_0;
    req_is_edns_badver_ = is_edns_badver;
}

void
QRAttributes::setQueryDO(const bool is_dnssec_ok) {
    req_is_dnssec_ok_ = is_dnssec_ok;
}

void
QRAttributes::setQuerySig(const bool is_tsig, const bool is_sig0,
                          const bool is_badsig)
{
    req_is_tsig_ = is_tsig;
    req_is_sig0_ = is_sig0;
    req_is_badsig_ = is_badsig;
}

void
QRAttributes::setOrigin(const std::string& origin) {
    zone_origin_ = origin;
}

void
QRAttributes::answerHasSent() {
    answer_sent_ = true;
}

void
QRAttributes::setResponseTruncated(const bool is_truncated) {
    res_is_truncated_ = is_truncated;
}

} // namespace statistics
} // namespace auth
} // namespace isc
