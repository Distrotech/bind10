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

#include <dnsl1cache/l1cache/cache_srv.h>
#include <dnsl1cache/l1cache/l1hash.h>
#include <dnsl1cache/logger.h>

#include <exceptions/exceptions.h>

#include <util/buffer.h>

#include <dns/name.h>
#include <dns/rrclass.h>

#include <log/logger_support.h>

#include <cc/data.h>
#include <config/ccsession.h>

#include <server_common/socket_request.h>
#include <server_common/portconfig.h>

#include <asiolink/asiolink.h>

#include <asiodns/dns_lookup.h>
#include <asiodns/dns_service.h>

#include <boost/bind.hpp>
#include <boost/foreach.hpp>

#include <ctime>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include <unistd.h>

using namespace isc::dns;
using namespace isc::config;
using namespace isc::data;
using namespace isc::asiolink;
using namespace isc::asiodns;

using std::vector;
using isc::cc::AbstractSession;
using isc::server_common::portconfig::parseAddresses;
using isc::server_common::portconfig::installListenAddresses;

namespace isc {
namespace dnsl1cache {

namespace {
// This is a derived class of \c DNSLookup, to serve as a
// callback in the asiolink module.  It calls
// AuthSrv::processMessage() on a single DNS message.
class MessageLookup : public asiodns::DNSLookup {
public:
    MessageLookup(MessageHandler& msg_handler) : msg_handler_(msg_handler) {}
    virtual void operator()(const IOMessage& io_message,
                            MessagePtr message,
                            MessagePtr, // Not used here
                            util::OutputBufferPtr buffer,
                            asiodns::DNSServer* server,
                            std::vector<asiodns::DNSLookup::Buffer>* buffers)
        const
    {
        msg_handler_.process(io_message, *message, *buffer, server,
                             std::time(NULL), buffers);
    }
private:
    MessageHandler& msg_handler_;
};

class MessageAnswer : public asiodns::DNSAnswer {
public:
    MessageAnswer() {}
    virtual void operator()(const IOMessage&, MessagePtr,
                            MessagePtr, util::OutputBufferPtr) const
    {}
};
}

DNSCacheSrv::DNSCacheSrv() {}

void
DNSCacheSrv::initialize(IOService& io_service, AbstractSession& session) {
    isc::server_common::initSocketRequestor(session, "b10-dnsl1cache");

    dns_lookup_.reset(new MessageLookup(msg_handler_));
    dns_answer_.reset(new MessageAnswer);
    dns_service_.reset(new DNSService(io_service, NULL, dns_lookup_.get(),
                                      dns_answer_.get()));
}

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
DNSCacheSrv::configHandler(ModuleCCSession*, ConstElementPtr new_config) {
    typedef std::pair<std::string, ConstElementPtr> ConfigPair;
    bool scatter_send = true;
    ConstElementPtr listen_addrs;
    BOOST_FOREACH(ConfigPair config_pair, new_config->mapValue()) {
        if (config_pair.first == "cache_file") {
            installCache(config_pair.second->stringValue().c_str());
        } else if (config_pair.first == "enable_scatter_send") {
            scatter_send = config_pair.second->boolValue();
            LOG_INFO(logger, DNSL1CACHE_SRV_SEND_MODE).
                arg(scatter_send ? "true" : "false");
        } else if (config_pair.first == "listen_on") {
            listen_addrs = config_pair.second;
        }
    }

    if (listen_addrs) {
        installListenAddresses(parseAddresses(listen_addrs, "listen_on"),
                               listen_addresses_, *dns_service_,
                               scatter_send ? (DNSService::SERVER_SYNC_OK |
                                               DNSService::SCATTER_WRITE) :
                               DNSService::SERVER_SYNC_OK);
    }
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

void
DNSCacheSrv::installCache(const char* cache_file) {
    LOG_INFO(logger, DNSL1CACHE_INSTALLING_CACHE).arg(cache_file);
    if (cache_file[0] == '\0') {
        cache_table_.reset();
    } else {
        cache_table_.reset(new DNSL1HashTable(cache_file));
    }
    msg_handler_.setCache(cache_table_.get());
    LOG_INFO(logger, DNSL1CACHE_CACHE_INSTALLED);
}

}
}
