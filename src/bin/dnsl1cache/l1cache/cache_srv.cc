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

#include <config.h>

#include <exceptions/exceptions.h>

#include <dns/name.h>
#include <dns/rrclass.h>

#include <log/logger_support.h>

#include <cc/data.h>
#include <config/ccsession.h>

#include <dnsl1cache/l1cache/cache_srv.h>
#include <dnsl1cache/logger.h>

#include <boost/bind.hpp>
#include <boost/foreach.hpp>

#include <vector>

#include <unistd.h>

using namespace isc::dns;
using namespace isc::config;
using namespace isc::data;

using std::vector;

namespace isc {
namespace dnsl1cache {

DNSCacheSrv::DNSCacheSrv()
{}

AppConfigHandler
DNSCacheSrv::getConfigHandler() {
    return (boost::bind(&DNSCacheSrv::configHandler, this, _1, _2));
}

AppCommandHandler
DNSCacheSrv::getCommandHandler() {
    return (boost::bind(&DNSCacheSrv::commandHandler, this, _1, _2, _3));
}

std::vector<RemoteConfigInfo>
DNSCacheSrv::getRemoteHandlers() {
    return (std::vector<RemoteConfigInfo>());
}

ConstElementPtr
DNSCacheSrv::configHandler(ModuleCCSession&, ConstElementPtr) {
    return (createAnswer());
}

ConstElementPtr
DNSCacheSrv::commandHandler(ModuleCCSession& /*cc_session*/,
                            const std::string& command,
                            ConstElementPtr /*args*/)
{
    LOG_INFO(logger, DNSL1CACHE_COMMAND_RECEIVED).arg(command);

    return (createAnswer());
}

}
}
