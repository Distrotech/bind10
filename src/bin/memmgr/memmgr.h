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

#ifndef __MEMMGR_H
#define __MEMMGR_H 1

#include <dns/rrclass.h>

#include <config/ccsession.h>

#include <memmgr/app_runner.h>

#include <vector>

namespace isc {
namespace memmgr {

class MemoryMgr {
public:
    typedef std::map<dns::RRClass,
                     boost::shared_ptr<datasrc::ConfigurableClientList> >
    DataSrcClientListsMap;
    typedef boost::shared_ptr<DataSrcClientListsMap> DataSrcClientListsPtr;
    DataSrcClientListsPtr datasrc_clients_map_;

    MemoryMgr();
    AppConfigHandler getConfigHandler();
    AppCommandHandler getCommandHandler();
    std::vector<RemoteConfigInfo> getRemoteHandlers();

private:
    data::ConstElementPtr configHandler(config::ModuleCCSession& cc_session,
                                        data::ConstElementPtr new_config);
    data::ConstElementPtr commandHandler(config::ModuleCCSession& cc_session,
                                         const std::string& command,
                                         data::ConstElementPtr args);
    void datasrcConfigHandler(config::ModuleCCSession& cc_session,
                              data::ConstElementPtr config,
                              const config::ConfigData&);
    bool first_time_;

    const std::string remapcmd_start;
    const std::string remapcmd_filebase;
    const std::string remapcmd_class;
    const std::string remapcmd_version;
    const std::string remapcmd_end;
};

} // namespace memmgr
} // namespace isc
#endif // __MEMMGR_H

// Local Variables:
// mode: c++
// End:
