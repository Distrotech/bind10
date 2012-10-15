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

#include <exceptions/exceptions.h>

#include <dns/name.h>
#include <dns/rrclass.h>

#include <log/logger_support.h>

#include <cc/data.h>
#include <config/ccsession.h>

#include <datasrc/client_list.h>

#include "memmgr.h"
#include "logger.h"

#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/foreach.hpp>

#include <vector>

using namespace isc::dns;
using namespace isc::datasrc;
using namespace isc::config;
using namespace isc::data;

using std::vector;
using boost::lexical_cast;

namespace isc {
namespace memmgr {

MemoryMgr::MemoryMgr() :
    first_time_(true),
    remapcmd_start("{\"command\": [\"remapfile\", {"),
    remapcmd_filebase("\"file_base\": \""),
    remapcmd_class("\", \"class\": \""),
    remapcmd_version("\", \"version\": \""),
    remapcmd_end("\"}]}")
{}

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
MemoryMgr::commandHandler(ModuleCCSession& /*cc_session*/,
                          const std::string& command, ConstElementPtr args)
{
    if (command != "loadzone") {
        isc_throw(isc::BadValue, "unknown command: " << command);
    }

    ConstElementPtr class_elem = args->get("class");
    const RRClass zone_class(class_elem ? RRClass(class_elem->stringValue()) :
                             RRClass::IN());

    ConstElementPtr origin_elem = args->get("origin");
    if (!origin_elem) {
        isc_throw(isc::BadValue, "Zone origin is missing");
    }
    const Name origin(origin_elem->stringValue());

    DataSrcClientListsMap::iterator found =
        datasrc_clients_map_->find(zone_class);
    if (found == datasrc_clients_map_->end()) {
        isc_throw(isc::BadValue,
                  "No data source client is configured for class " <<
                  zone_class);
    }
    if (found->second->reload(origin) !=
        ConfigurableClientList::ZONE_RELOADED) {
        isc_throw(isc::Unexpected, "zone reload failed");
    }
    LOG_INFO(memmgr_logger, MEMMGR_RELOAD_ZONE).arg(origin).arg(zone_class);

    return (createAnswer());
}

namespace {
MemoryMgr::DataSrcClientListsPtr
configureDataSource(const ConstElementPtr& config) {
    typedef std::map<std::string, ConstElementPtr> ConfigMap;
    MemoryMgr::DataSrcClientListsPtr new_lists(
        new MemoryMgr::DataSrcClientListsMap);
    typedef boost::shared_ptr<ConfigurableClientList> ClientListPtr;

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
        datasrc_clients_map_ = lists;

        // Notify other modules (right now only Auth is hardcoded) of the
        // mapped file info so that they can map it into memory.
        for (DataSrcClientListsMap::const_iterator it = lists->begin();
             it != lists->end();
             ++it)
        {
            BOOST_FOREACH(ConfigurableClientList::MappedMemoryInfo info,
                          it->second->getMappedMemories()) {
                LOG_INFO(memmgr_logger, MEMMGR_NOTIFY_AUTH_REMAP)
                    .arg(it->first);

                ConstElementPtr remap_command = Element::fromJSON(
                    remapcmd_start +
                    remapcmd_filebase + info.base_file_name +
                    remapcmd_class + it->first.toText() +
                    remapcmd_version +
                    lexical_cast<std::string>(info.version) +
                    remapcmd_end);
                const unsigned int seq =
                    cc_session.groupSendMsg(remap_command, "Auth");
                ConstElementPtr env, answer, parsed_answer;
                cc_session.groupRecvMsg(env, answer, false, seq);
                int rcode;
                parsed_answer = parseAnswer(rcode, answer);
                if (rcode != 0) {
                    LOG_ERROR(memmgr_logger, MEMMGR_NOTIFY_AUTH_ERROR)
                        .arg(parsed_answer->str());
                }
            }
        }
    }
}

}
}
