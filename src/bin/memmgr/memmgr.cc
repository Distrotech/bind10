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

#include <dns/rrclass.h>

#include <log/logger_support.h>

#include <cc/data.h>
#include <config/ccsession.h>

#include <datasrc/client_list.h>

#include "memmgr.h"
#include "logger.h"

#include <boost/bind.hpp>

using namespace isc::dns;
using namespace isc::datasrc;
using namespace isc::config;
using namespace isc::data;

namespace {
typedef std::map<RRClass, boost::shared_ptr<ConfigurableClientList> >
DataSrcClientListsMap;
typedef boost::shared_ptr<DataSrcClientListsMap> DataSrcClientListsPtr;
}

namespace isc {
namespace memmgr {

AppConfigHandler
MemoryMgr::getConfigHandler() {
    return (boost::bind(&MemoryMgr::configHandler, this, _1, _2));
}

AppCommandHandler
MemoryMgr::getCommandHandler() {
    return (boost::bind(&MemoryMgr::commandHandler, this, _1, _2, _3));
}

std::vector<RemoteConfigInfo>
MemoryMgr::getRemoteHandlers() {
    std::vector<RemoteConfigInfo> remote_configs;
    remote_configs.push_back(RemoteConfigInfo(
                                 "data_sources",
                                 boost::bind(&MemoryMgr::datasrcConfigHandler,
                                             this, _1, _2, _3), false));
    return (remote_configs);
}

ConstElementPtr
MemoryMgr::configHandler(ModuleCCSession&, ConstElementPtr) {
    return (createAnswer());
}

ConstElementPtr
MemoryMgr::commandHandler(ModuleCCSession&, const std::string&,
                          ConstElementPtr)
{
    return (createAnswer());
}

namespace {
DataSrcClientListsPtr
configureDataSource(const ConstElementPtr& config) {
    typedef std::map<std::string, ConstElementPtr> ConfigMap;
    typedef boost::shared_ptr<ConfigurableClientList> ClientListPtr;

    DataSrcClientListsPtr new_lists(new DataSrcClientListsMap);

    // Go through the configuration and create corresponding list.
    const ConfigMap& map(config->mapValue());
    for (ConfigMap::const_iterator it(map.begin()); it != map.end(); ++it) {
        const RRClass rrclass(it->first);
        ClientListPtr list(new ConfigurableClientList(rrclass, true));

        LOG_INFO(memmgr_logger, MEMMGR_CONFIGURE_DATASRC_START).arg(rrclass);
        list->configure(it->second, true);
        LOG_INFO(memmgr_logger, MEMMGR_CONFIGURE_DATASRC_END).arg(rrclass);
        new_lists->insert(std::pair<dns::RRClass, ClientListPtr>(rrclass,
                                                                 list));
    }

    return (new_lists);
}
}

void
MemoryMgr::datasrcConfigHandler(ModuleCCSession& cc_session,
                                ConstElementPtr config, const ConfigData&)
{
    if (config->contains("classes")) {
        DataSrcClientListsPtr lists;

        if (first_time_) {
            // HACK: The default is not passed to the handler in the first
            // callback. This one will get the default (or, current value).
            // Further updates will work the usual way.
            first_time_ = false;
            lists = configureDataSource(
                cc_session.getRemoteConfigValue("data_sources", "classes"));
        } else {
            lists = configureDataSource(config->get("classes"));
        }
    }
}

}
}
