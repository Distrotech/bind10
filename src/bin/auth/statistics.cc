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
#include <auth/auth_log.h>

#include <dns/opcode.h>

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

using namespace isc::dns;
using namespace isc::auth;
using namespace isc::statistics;

namespace isc {
namespace auth {
namespace statistics {

class CountersImpl : boost::noncopyable {
public:
    CountersImpl();
    ~CountersImpl();
    void inc(const Counters::ServerCounterType type);
    void inc(const Opcode opcode) {
        opcode_counter_.inc(opcode.getCode());
    }
    void inc(const Rcode rcode) {
        rcode_counter_.inc(rcode.getCode());
    }
    void inc(const std::string& zone, const Counters::PerZoneCounterType type);
    bool submitStatistics() const;
    void setStatisticsSession(isc::cc::AbstractSession* statistics_session);
    void registerStatisticsValidator (Counters::validator_type validator);
    // Currently for testing purpose only
    uint64_t getCounter(const Counters::ServerCounterType type) const;
    uint64_t getCounter(const Opcode opcode) const {
        return (opcode_counter_.get(opcode.getCode()));
    }
    uint64_t getCounter(const Rcode rcode) const {
        return (rcode_counter_.get(rcode.getCode()));
    }
private:
    Counter server_counter_;
    Counter opcode_counter_;
    static const size_t NUM_OPCODES = 16;
    Counter rcode_counter_;
    static const size_t NUM_RCODES = 17;
    CounterDictionary per_zone_counter_;
    isc::cc::AbstractSession* statistics_session_;
    Counters::validator_type validator_;
};

CountersImpl::CountersImpl() :
    // initialize counter
    // size of server_counter_: Counters::SERVER_COUNTER_TYPES
    // size of per_zone_counter_: Counters::PER_ZONE_COUNTER_TYPES
    server_counter_(Counters::SERVER_COUNTER_TYPES),
    opcode_counter_(NUM_OPCODES), rcode_counter_(NUM_RCODES),
    per_zone_counter_(Counters::PER_ZONE_COUNTER_TYPES),
    statistics_session_(NULL)
{
    per_zone_counter_.addElement("_SERVER_");
}

CountersImpl::~CountersImpl()
{}

void
CountersImpl::inc(const Counters::ServerCounterType type) {
    server_counter_.inc(type);
}

void
CountersImpl::inc(const std::string& zone,
                      const Counters::PerZoneCounterType type)
{
    per_zone_counter_[zone].inc(type);
}

bool
CountersImpl::submitStatistics() const {
    if (statistics_session_ == NULL) {
        LOG_ERROR(auth_logger, AUTH_NO_STATS_SESSION);
        return (false);
    }
    std::stringstream statistics_string;
    // add pid in order for stats to identify which auth sends
    // statistics in the situation that multiple auth instances are
    // working
    statistics_string << "{\"command\": [\"set\","
                      <<   "{ \"owner\": \"Auth\","
                      <<   "  \"pid\":" << getpid()
                      <<   ", \"data\":"
                      <<     "{ \"queries.udp\": "
                      <<     server_counter_.get(Counters::SERVER_UDP_QUERY)
                      <<     ", \"queries.tcp\": "
                      <<     server_counter_.get(Counters::SERVER_TCP_QUERY);
    // Insert non 0 Opcode counters.
    for (int i = 0; i < NUM_OPCODES; ++i) {
        const Counter::Type counter = opcode_counter_.get(i);
        if (counter != 0) {
            // The counter item name should be derived lower-cased textual
            // representation of the code.
            std::string opcode_txt = Opcode(i).toText();
            std::transform(opcode_txt.begin(), opcode_txt.end(),
                           opcode_txt.begin(), ::tolower);
            statistics_string << ", \"opcode." << opcode_txt << "\": "
                              << counter;
        }
    }
    // Insert non 0 Rcode counters.
    for (int i = 0; i < NUM_RCODES; ++i) {
        const Counter::Type counter = rcode_counter_.get(i);
        if (counter != 0) {
            // The counter item name should be derived lower-cased textual
            // representation of the code.
            std::string rcode_txt = Rcode(i).toText();
            std::transform(rcode_txt.begin(), rcode_txt.end(),
                           rcode_txt.begin(), ::tolower);
            statistics_string << ", \"rcode." << rcode_txt << "\": "
                              << counter;
        }
    }
    statistics_string <<   " }"
                      <<   "}"
                      << "]}";
    isc::data::ConstElementPtr statistics_element =
        isc::data::Element::fromJSON(statistics_string);
    // validate the statistics data before send
    if (validator_) {
        if (!validator_(
                statistics_element->get("command")->get(1)->get("data"))) {
            LOG_ERROR(auth_logger, AUTH_INVALID_STATISTICS_DATA);
            return (false);
        }
    }
    try {
        // group_{send,recv}msg() can throw an exception when encountering
        // an error, and group_recvmsg() will throw an exception on timeout.
        // We don't want to kill the main server just due to this, so we
        // handle them here.
        const int seq =
            statistics_session_->group_sendmsg(statistics_element, "Stats");
        isc::data::ConstElementPtr env, answer;
        // TODO: parse and check response from statistics module
        // currently it just returns empty message
        statistics_session_->group_recvmsg(env, answer, false, seq);
    } catch (const isc::cc::SessionError& ex) {
        LOG_ERROR(auth_logger, AUTH_STATS_COMMS).arg(ex.what());
        return (false);
    } catch (const isc::cc::SessionTimeout& ex) {
        LOG_ERROR(auth_logger, AUTH_STATS_TIMEOUT).arg(ex.what());
        return (false);
    }
    return (true);
}

void
CountersImpl::setStatisticsSession
    (isc::cc::AbstractSession* statistics_session)
{
    statistics_session_ = statistics_session;
}

void
CountersImpl::registerStatisticsValidator
    (Counters::validator_type validator)
{
    validator_ = validator;
}

// Currently for testing purpose only
uint64_t
CountersImpl::getCounter(const Counters::ServerCounterType type) const {
    return (server_counter_.get(type));
}

Counters::Counters() : impl_(new CountersImpl())
{}

Counters::~Counters() {}

void
Counters::inc(const Counters::ServerCounterType type) {
    impl_->inc(type);
}

void
Counters::inc(const Opcode opcode) {
    impl_->inc(opcode);
}

void
Counters::inc(const Rcode rcode) {
    impl_->inc(rcode);
}

bool
Counters::submitStatistics() const {
    return (impl_->submitStatistics());
}

void
Counters::setStatisticsSession (isc::cc::AbstractSession* statistics_session) {
    impl_->setStatisticsSession(statistics_session);
}

uint64_t
Counters::getCounter(const Counters::ServerCounterType type) const {
    return (impl_->getCounter(type));
}

uint64_t
Counters::getCounter(const Opcode opcode) const {
    return (impl_->getCounter(opcode));
}

uint64_t
Counters::getCounter(const Rcode rcode) const {
    return (impl_->getCounter(rcode));
}

void
Counters::registerStatisticsValidator
    (Counters::validator_type validator) const
{
    return (impl_->registerStatisticsValidator(validator));
}

QRAttributes::QRAttributes() :
    req_ip_version_(-1), req_transport_protocol_(-1),
    req_opcode_(-1),
    req_is_edns_0_(false), req_is_edns_badver_(false),
    req_is_dnssec_ok_(false),
    req_is_tsig_(false), req_is_sig0_(false), req_is_badsig_(false),
    zone_origin_(),
    answer_sent_(false),
    res_is_truncated_(false)
{}

} // namespace statistics
} // namespace auth
} // namespace isc
