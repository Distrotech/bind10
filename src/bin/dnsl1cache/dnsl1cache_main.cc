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

#include <log/logger_support.h>

#include <datasrc/client_list.h>

#include <dnsl1cache/spec_config.h>
#include <dnsl1cache/app_runner.h>
#include <dnsl1cache/l1cache/cache_srv.h>
#include <dnsl1cache/logger.h>

#include <stdexcept>
#include <string>
#include <cstdlib>

using namespace isc::dnsl1cache;

int
main(int, char**) {
    try {
        // Initialize logging.
        isc::log::initLogger("b10-dnsl1cache", isc::log::INFO,
                             isc::log::MAX_DEBUG_LEVEL, NULL, true);

        DNSCacheSrv cache_srv;

        const char* const from_build = std::getenv("B10_FROM_BUILD");
        const std::string specfile = (from_build != NULL) ?
            (std::string(from_build) + "/src/bin/dnsl1cache/dnsl1cache.spec") :
            std::string(DNSL1CACHE_SPECFILE_LOCATION);
        AppRunner app_runner(specfile, cache_srv.getConfigHandler(),
                             cache_srv.getCommandHandler(),
                             cache_srv.getRemoteHandlers());
        cache_srv.initialize(app_runner.getIOService(),
                             app_runner.getCCSession());
        app_runner.initialize();

        // Successfully initialized.
        LOG_INFO(logger, DNSL1CACHE_STARTED);
        app_runner.run();

        LOG_INFO(logger, DNSL1CACHE_STOPPED);
    } catch (const std::exception& ex) {
        LOG_FATAL(logger, DNSL1CACHE_FAILED).arg(ex.what());
        return (1);
    }

    return (0);
}
