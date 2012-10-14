// Copyright (C) 2012  Internet Systems Consortium, Inc. ("ISC")
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

#include <cc/session.h>
#include <cc/data.h>
#include <config/ccsession.h>

#include <asiolink/asiolink.h>

#include "spec_config.h"
#include "memmgr_messages.h"
#include "logger.h"

#include <boost/scoped_ptr.hpp>

#include <iostream>
#include <string>
#include <cstdlib>

using namespace isc::cc;
using namespace isc::config;
using namespace isc::asiolink;
using namespace isc::memmgr;

namespace {
} // end of unnamed namespace

int
main(int, char**) {
    int ret = 0;

    boost::scoped_ptr<Session> cc_session;
    boost::scoped_ptr<ModuleCCSession> config_session;
    boost::scoped_ptr<IOService> io_service;
    try {
        // Initialize logging.
        isc::log::initLogger("b10-memmgr", isc::log::INFO,
                             isc::log::MAX_DEBUG_LEVEL, NULL);

        const char* const from_build = std::getenv("B10_FROM_BUILD");
        const std::string specfile = (from_build != NULL) ?
            (std::string(from_build) + "/src/bin/memmgr/memmgr.spec") :
            std::string(MEMMGR_SPECFILE_LOCATION);

        io_service.reset(new IOService());
        cc_session.reset(new Session(io_service->get_io_service()));

#if 0
        // We delay starting listening to new commands/config just before we
        // go into the main loop to avoid confusion due to mixture of
        // synchronous and asynchronous operations, just in case (this
        // shouldn't be a concern for this program)
        config_session.reset(new ModuleCCSession(specfile, *cc_session,
                                                 configHandler,
                                                 commandHandler, false));

        // Start the data source configuration.  We pass first_time and
        // config_session for the hack described in datasrcConfigHandler.
        bool first_time = true;
        config_session->addRemoteConfig("data_sources",
                                        boost::bind(datasrcConfigHandler,
                                                    auth_server, &first_time,
                                                    config_session,
                                                    _1, _2, _3),
                                        false);
#endif

        // Now start asynchronous read.
        config_session->start();

        // Successfully initialized.
        LOG_INFO(memmgr_logger, MEMMGR_STARTED);

        // Main event loop
        io_service->run();
    } catch (const std::exception& ex) {
        LOG_FATAL(memmgr_logger, MEMMGR_FAILED).arg(ex.what());
        ret = 1;
    }

    // If we haven't registered callback for data sources, this will be just
    // no-op.
    if (config_session) {
        config_session->removeRemoteConfig("data_sources");
    }

    return (ret);
}
