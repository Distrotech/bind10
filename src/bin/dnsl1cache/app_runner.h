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

#ifndef __DNSL1CACHE_APP_RUNNER_H
#define __DNSL1CACHE_APP_RUNNER_H 1

#include <cc/session.h>
#include <cc/data.h>
#include <config/ccsession.h>

#include <asiolink/asiolink.h>

#include <boost/function.hpp>

#include <string>
#include <utility>
#include <vector>

namespace isc {
namespace dnsl1cache {

typedef boost::function<data::ConstElementPtr(
    config::ModuleCCSession&, data::ConstElementPtr)> AppConfigHandler;
typedef boost::function<data::ConstElementPtr(
    config::ModuleCCSession&, const std::string&, data::ConstElementPtr)>
AppCommandHandler;
typedef boost::function<void(config::ModuleCCSession&,
                             isc::data::ConstElementPtr,
                             const config::ConfigData&)> AppRemoteHandler;

struct RemoteConfigInfo {
    RemoteConfigInfo(const std::string& module_name_param,
                     AppRemoteHandler handler_param,
                     bool spec_is_filename_param) :
        module_name(module_name_param), handler(handler_param),
        spec_is_filename(spec_is_filename_param)
    {}
    std::string module_name; 
    AppRemoteHandler handler;
    bool spec_is_filename;
};

class AppRunner {
public:
    AppRunner(const std::string& specfile,
              AppConfigHandler config_handler,
              AppCommandHandler command_handler,
              const std::vector<RemoteConfigInfo>& remote_configs);
    void initialize();

    /// Start main event loop
    void run();

private:
    data::ConstElementPtr configHandler(data::ConstElementPtr new_config);
    data::ConstElementPtr commandHandler(const std::string& command,
                                         data::ConstElementPtr args);
    void remoteHandler(const std::string& module_name,
                       isc::data::ConstElementPtr new_config,
                       const config::ConfigData& config_data);

private:
    const std::string specfile_;
    const AppConfigHandler app_config_handler_;
    const AppCommandHandler app_command_handler_;
    const std::vector<RemoteConfigInfo> remote_configs_;
    boost::scoped_ptr<asiolink::IOService> io_service_;
    boost::scoped_ptr<cc::Session> cc_session_;
    boost::scoped_ptr<config::ModuleCCSession> config_session_;
};

} // namespace dnsl1cache
} // namespace isc
#endif // __DNSL1CACHE_APP_RUNNER_H

// Local Variables:
// mode: c++
// End:
