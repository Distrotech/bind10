// Copyright (C) 2009-2011  Internet Systems Consortium, Inc. ("ISC")
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

#include <util/buffer.h>
#include <util/io/socketsession.h>

#include <dns/message.h>
#include <dns/messagerenderer.h>

#include <cc/session.h>
#include <cc/data.h>
#include <config/ccsession.h>

#include <xfr/xfrout_client.h>

#include <auth/spec_config.h>
#include <auth/common.h>
#include <auth/auth_config.h>
#include <auth/command.h>
#include <auth/auth_srv.h>
#include <auth/auth_log.h>
#include <auth/datasrc_config.h>
#include <auth/datasrc_clients_mgr.h>

#include <asiodns/asiodns.h>
#include <asiolink/asiolink.h>
#include <log/logger_support.h>
#include <server_common/keyring.h>
#include <server_common/socket_request.h>

#include <boost/bind.hpp>
#include <boost/scoped_ptr.hpp>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <errno.h>

#include <cassert>
#include <iostream>

using namespace std;
using namespace isc::asiodns;
using namespace isc::asiolink;
using namespace isc::auth;
using namespace isc::cc;
using namespace isc::config;
using namespace isc::data;
using namespace isc::dns;
using namespace isc::log;
using namespace isc::util;
using namespace isc::util::io;
using namespace isc::xfr;

namespace {

/* need global var for config/command handlers.
 * todo: turn this around, and put handlers in the authserver
 * class itself? */
AuthSrv* auth_server;

ConstElementPtr
my_config_handler(ConstElementPtr new_config) {
    return (auth_server->updateConfig(new_config));
}

ConstElementPtr
my_command_handler(const string& command, ConstElementPtr args) {
    assert(auth_server != NULL);
    return (execAuthServerCommand(*auth_server, command, args));
}

void
datasrcConfigHandler(AuthSrv* server, bool* first_time,
                     ModuleCCSession* config_session, const std::string&,
                     isc::data::ConstElementPtr config,
                     const isc::config::ConfigData&)
{
    assert(server != NULL);

    // Note: remote config handler is requested to be exception free.
    // While the code below is not 100% exception free, such an exception
    // is really fatal and the server should actually stop.  So we don't
    // bother to catch them; the exception would be propagated to the
    // top level of the server and terminate it.

    if (*first_time) {
        // HACK: The default is not passed to the handler in the first
        // callback. This one will get the default (or, current value).
        // Further updates will work the usual way.
        assert(config_session != NULL);
        *first_time = false;
        server->getDataSrcClientsMgr().reconfigure(
            config_session->getRemoteConfigValue("data_sources", "classes"),
            boost::bind(&AuthSrv::listsReconfigured, server));
    } else if (config->contains("classes")) {
        server->getDataSrcClientsMgr().reconfigure(config->get("classes"),
            boost::bind(&AuthSrv::listsReconfigured, server));
    }
}

void
usage() {
    cerr << "Usage:  b10-auth [-v]"
         << endl;
    cerr << "\t-v: verbose logging (debug-level)" << endl;
    exit(1);
}

} // end of anonymous namespace

int
main(int argc, char* argv[]) {
    int ch;
    bool verbose = false;

    while ((ch = getopt(argc, argv, ":nu:v")) != -1) {
        switch (ch) {
        case 'v':
            verbose = true;
            break;
        case '?':
        default:
            usage();
        }
    }

    if (argc - optind > 0) {
        usage();
    }

    // Initialize logging.  If verbose, we'll use maximum verbosity.
    isc::log::initLogger(AUTH_NAME,
                         (verbose ? isc::log::DEBUG : isc::log::INFO),
                         isc::log::MAX_DEBUG_LEVEL, NULL, true);

    int ret = 0;

    // XXX: we should eventually pass io_service here.
    boost::scoped_ptr<AuthSrv> auth_server_; // placeholder
    boost::scoped_ptr<Session> cc_session;
    boost::scoped_ptr<Session> xfrin_session;
    bool xfrin_session_established = false; // XXX (see Trac #287)
    boost::scoped_ptr<ModuleCCSession> config_session;
    XfroutClient xfrout_client(getXfroutSocketPath());
    SocketSessionForwarder ddns_forwarder(getDDNSSocketPath());
    try {
        string specfile;
        if (getenv("B10_FROM_BUILD")) {
            specfile = string(getenv("B10_FROM_BUILD")) +
                "/src/bin/auth/auth.spec";
        } else {
            specfile = string(AUTH_SPECFILE_LOCATION);
        }

        auth_server_.reset(new AuthSrv(xfrout_client, ddns_forwarder));
        auth_server = auth_server_.get();
        LOG_INFO(auth_logger, AUTH_SERVER_CREATED);

        IOService& io_service = auth_server->getIOService();
        DNSLookup* lookup = auth_server->getDNSLookupProvider();
        DNSAnswer* answer = auth_server->getDNSAnswerProvider();

        DNSService dns_service(io_service, lookup, answer);
        auth_server->setDNSService(dns_service);
        LOG_DEBUG(auth_logger, DBG_AUTH_START, AUTH_DNS_SERVICES_CREATED);

        cc_session.reset(new Session(io_service.get_io_service()));
        LOG_DEBUG(auth_logger, DBG_AUTH_START, AUTH_CONFIG_CHANNEL_CREATED);
        // Initialize the Socket Requestor
        isc::server_common::initSocketRequestor(*cc_session, AUTH_NAME);

        // We delay starting listening to new commands/config just before we
        // go into the main loop to avoid confusion due to mixture of
        // synchronous and asynchronous operations (this would happen in
        // initial communication with b10-init that takes place in
        // updateConfig() for listen_on and in initializing TSIG keys below).
        // Until then all operations on the CC session will take place
        // synchronously.
        config_session.reset(new ModuleCCSession(specfile, *cc_session,
                                                 my_config_handler,
                                                 my_command_handler, false));
        LOG_DEBUG(auth_logger, DBG_AUTH_START, AUTH_CONFIG_CHANNEL_ESTABLISHED);

        xfrin_session.reset(new Session(io_service.get_io_service()));
        LOG_DEBUG(auth_logger, DBG_AUTH_START, AUTH_XFRIN_CHANNEL_CREATED);
        xfrin_session->establish(NULL);
        xfrin_session_established = true;
        LOG_DEBUG(auth_logger, DBG_AUTH_START, AUTH_XFRIN_CHANNEL_ESTABLISHED);

        auth_server->setXfrinSession(xfrin_session.get());

        // Configure the server.  configureAuthServer() is expected to install
        // all initial configurations, but as a short term workaround we
        // handle the traditional "database_file" setup by directly calling
        // updateConfig().
        // if server load configure failed, we won't exit, give user second
        // chance to correct the configure.
        auth_server->setConfigSession(config_session.get());
        try {
            configureAuthServer(*auth_server, config_session->getFullConfig());
            auth_server->updateConfig(ElementPtr());
        } catch (const AuthConfigError& ex) {
            LOG_ERROR(auth_logger, AUTH_CONFIG_LOAD_FAIL).arg(ex.what());
        }

        LOG_DEBUG(auth_logger, DBG_AUTH_START, AUTH_LOAD_TSIG);
        isc::server_common::initKeyring(*config_session);
        auth_server->setTSIGKeyRing(&isc::server_common::keyring);

        // Start the data source configuration.  We pass first_time and
        // config_session for the hack described in datasrcConfigHandler.
        bool first_time = true;
        config_session->addRemoteConfig("data_sources",
                                        boost::bind(datasrcConfigHandler,
                                                    auth_server, &first_time,
                                                    config_session.get(),
                                                    _1, _2, _3),
                                        false);

        // Now start asynchronous read.
        config_session->start();
        LOG_DEBUG(auth_logger, DBG_AUTH_START, AUTH_CONFIG_CHANNEL_STARTED);

        // Successfully initialized.
        LOG_INFO(auth_logger, AUTH_SERVER_STARTED);

        // Ping any interested module that (a new) auth is up
        // Currently, only the DDNS module is notified, but we could consider
        // make an announcement channel for these (one-way) messages
        cc_session->group_sendmsg(
            isc::config::createCommand(AUTH_STARTED_NOTIFICATION), "DDNS");
        io_service.run();
    } catch (const std::exception& ex) {
        LOG_FATAL(auth_logger, AUTH_SERVER_FAILED).arg(ex.what());
        ret = 1;
    }

    if (xfrin_session_established) {
        xfrin_session->disconnect();
    }

    // If we haven't registered callback for data sources, this will be just
    // no-op.
    if (config_session != NULL) {
        config_session->removeRemoteConfig("data_sources");
    }

    LOG_INFO(auth_logger, AUTH_SERVER_EXITING);

    return (ret);
}
