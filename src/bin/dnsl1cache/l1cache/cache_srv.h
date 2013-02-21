// Copyright (C) 2013  Internet Systems Consortium, Inc. ("ISC")
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

#ifndef CACHE_SRV_H
#define CACHE_SRV_H 1

#include <exceptions/exceptions.h>

#include <dns/rrclass.h>

#include <cc/session.h>

#include <config/ccsession.h>

#include <dnsl1cache/app_runner.h>
#include <dnsl1cache/l1cache/l1hash.h>
#include <dnsl1cache/l1cache/message_handler.h>

#include <asiolink/asiolink.h>
#include <asiolink/interval_timer.h>

#include <asiodns/dns_lookup.h>
#include <asiodns/dns_answer.h>
#include <asiodns/dns_service.h>

#include <server_common/portconfig.h>

#include <boost/scoped_ptr.hpp>

#include <ctime>
#include <vector>

namespace isc {
namespace dnsl1cache {

class DNSCacheSrvError : public Exception {
public:
    DNSCacheSrvError(const char* file, size_t line, const char* what) :
        Exception(file, line, what) {}
};

class DNSCacheSrv {
public:
    DNSCacheSrv();
    AppConfigHandler getConfigHandler();
    AppCommandHandler getCommandHandler();
    std::vector<RemoteConfigInfo> getRemoteHandlers();
    void initialize(asiolink::IOService& io_service,
                    cc::AbstractSession& session);

private:
    data::ConstElementPtr configHandler(config::ModuleCCSession* cc_session,
                                        data::ConstElementPtr new_config);
    data::ConstElementPtr commandHandler(config::ModuleCCSession& cc_session,
                                         const std::string& command,
                                         data::ConstElementPtr args);
    void installCache(const char* cahce_file);
    void updateClock();

    boost::scoped_ptr<DNSL1HashTable> cache_table_;
    boost::scoped_ptr<asiodns::DNSServiceBase> dns_service_;
    server_common::portconfig::AddressList listen_addresses_;
    boost::scoped_ptr<asiodns::DNSLookup> dns_lookup_;
    boost::scoped_ptr<asiodns::DNSAnswer> dns_answer_;
    boost::scoped_ptr<asiolink::IntervalTimer> clock_timer_;
    std::time_t now_;
    MessageHandler msg_handler_;
};

} // namespace dnsl1cache
} // namespace isc
#endif // CACHE_SRV_H

// Local Variables:
// mode: c++
// End:
