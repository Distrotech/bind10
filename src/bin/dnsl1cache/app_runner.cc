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

#include <log/logger_support.h>

#include <dns/rrclass.h>

#include <dnsl1cache/app_runner.h>
#include <dnsl1cache/logger.h>

#include <boost/bind.hpp>
#include <boost/scoped_ptr.hpp>

#include <iostream>
#include <map>
#include <string>
#include <cstdlib>

using namespace isc::cc;
using namespace isc::config;
using namespace isc::asiolink;
using namespace isc::dnsl1cache;
using namespace isc::data;

using std::vector;

namespace {
typedef vector<RemoteConfigInfo> RemoteConfigs;
}

namespace isc {
namespace dnsl1cache {

AppRunner::AppRunner(const std::string& specfile,
                     AppConfigHandler config_handler,
                     AppCommandHandler command_handler,
                     const vector<RemoteConfigInfo>& remote_configs) :
    specfile_(specfile),
    app_config_handler_(config_handler), app_command_handler_(command_handler),
    remote_configs_(remote_configs),
    io_service_(new IOService()),
    cc_session_(new Session(io_service_->get_io_service()))
{
}

void
AppRunner::initialize() {
    // We delay starting listening to new commands/config just before we
    // go into the main loop to avoid confusion due to mixture of
    // synchronous and asynchronous operations, just in case (this
    // shouldn't be a concern for this program)
    config_session_.reset(new ModuleCCSession(
                              specfile_, *cc_session_,
                              boost::bind(&AppRunner::configHandler, this,
                                          _1),
                              boost::bind(&AppRunner::commandHandler, this,
                                          _1, _2), false));

    // Subscribe to remote configurations, performing initial local
    // configuration regarding remote ones.
    for (RemoteConfigs::const_iterator it = remote_configs_.begin();
         it != remote_configs_.end();
         ++it)
    {
        config_session_->addRemoteConfig(it->module_name,
                                         boost::bind(&AppRunner::remoteHandler,
                                                     this, _1, _2, _3),
                                         it->spec_is_filename);
    }

    // Start asynchronous event sessions.
    config_session_->start();
}

void
AppRunner::run() {
    io_service_->run();
}

ConstElementPtr
AppRunner::configHandler(ConstElementPtr new_config) {
    ConstElementPtr answer = isc::config::createAnswer();
    try {
        if (new_config) {
            answer = app_config_handler_(config_session_.get(), new_config);
        }
    } catch (const isc::Exception& ex) {
        LOG_ERROR(logger, APPRUNNER_CONFIG_HANDLER_FAIL).arg(ex.what());
        return (createAnswer(1,
                             std::string("Failed to update configuration: ") +
                             ex.what()));
    }
    return (answer);
}

void
AppRunner::remoteHandler(const std::string& module_name,
                         isc::data::ConstElementPtr new_config,
                         const ConfigData& config_data)
{
    try {
        for (RemoteConfigs::const_iterator it = remote_configs_.begin();
             it != remote_configs_.end();
             ++it)
        {
            if (it->module_name == module_name) {
                it->handler(*config_session_, new_config, config_data);
            }
        }
    } catch (const isc::Exception& ex) {
        // XXX: remoteHandler doesn't allow propagating an exception, but
        // we rather let it go through except for known ones.
        LOG_ERROR(logger, APPRUNNER_REMOTE_HANDLER_FAIL).
            arg(module_name).arg(ex.what());
    }
}

ConstElementPtr
AppRunner::commandHandler(const std::string& command, ConstElementPtr args) {
    LOG_INFO(logger, APPRUNNER_COMMAND_HANDLER).arg(command);

    ConstElementPtr answer = isc::config::createAnswer(); // default

    // Invoke application specific handler
    try {
        answer = app_command_handler_(*config_session_, command, args);
    } catch (const isc::Exception& ex) {
        return (createAnswer(1, "Failed to perform " + command + " command: " +
                             ex.what()));
    }

    if (command == "shutdown") {
        for (RemoteConfigs::const_iterator it = remote_configs_.begin();
             it != remote_configs_.end();
             ++it) {
            // If we haven't registered callback for the module, this will
            // be just no-op.
            config_session_->removeRemoteConfig(it->module_name);
        }

        // Stop the loop
        io_service_->stop();
    }
    return (answer);
}

}
}
