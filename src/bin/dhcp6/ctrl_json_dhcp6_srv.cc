// Copyright (C) 2012-2013 Internet Systems Consortium, Inc. ("ISC")
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

#include <asiolink/asiolink.h>
#include <dhcp/iface_mgr.h>
#include <dhcpsrv/dhcp_config_parser.h>
#include <dhcpsrv/cfgmgr.h>
#include <dhcp6/json_config_parser.h>
#include <dhcp6/ctrl_dhcp6_srv.h>
#include <dhcp6/dhcp6_log.h>
#include <dhcp6/spec_config.h>
#include <exceptions/exceptions.h>
#include <util/buffer.h>

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

using namespace isc::asiolink;
using namespace isc::cc;
using namespace isc::config;
using namespace isc::data;
using namespace isc::dhcp;
using namespace isc::log;
using namespace isc::util;
using namespace std;

namespace isc {
namespace dhcp {

void ControlledDhcpv6Srv::sessionReader(void) {
}

bool
ControlledDhcpv6Srv::init(const std::string& file_name) {
    // This is BIND10 configuration backed it does not have any notion on 

    string config;

    istringstream cfg_stream(file_name, std::ios::in);
    cfg_stream >> config;

    isc::data::ConstElementPtr json;
    isc::data::ConstElementPtr result;

    try {
        json = Element::fromJSON(config);
        result = configureDhcp6Server(*this, json);
    }  catch (const std::exception& ex) {
        LOG_ERROR(dhcp6_logger, DHCP6_CONFIG_LOAD_FAIL).arg(ex.what());
        return (false);
    }

    if (!result) {
        // Undetermined status of the configuration
        LOG_ERROR(dhcp6_logger, DHCP6_CONFIG_LOAD_FAIL)
            .arg("Undefined result of configureDhcp6Server() function");
        return (false);
    }

    ConstElementPtr comment; ///< Comment (see @ref isc::config::parseAnswer)
    int rcode;

    comment = parseAnswer(rcode, result);
    if (rcode != 0) {
        string reason = "";
        if (comment) {
            reason = string(" (") + comment->stringValue() + string(")");
        }
        LOG_ERROR(dhcp6_logger, DHCP6_CONFIG_LOAD_FAIL).arg(reason);
        return (false);
    }

    // Configuration may disable or enable interfaces so we have to
    // reopen sockets according to new configuration.
    openActiveSockets(getPort());

    // Server will start DDNS communications if its enabled.
    this->startD2();

    return (true);
}

void ControlledDhcpv6Srv::cleanup() {
    // Nothing to do here. No need to disconnect from anything.
}

ControlledDhcpv6Srv::ControlledDhcpv6Srv(uint16_t port)
    : Dhcpv6Srv(port), cc_session_(NULL), config_session_(NULL) {
}

void ControlledDhcpv6Srv::shutdown() {
    io_service_.stop(); // Stop ASIO transmissions
    Dhcpv6Srv::shutdown(); // Initiate DHCPv6 shutdown procedure.
}

ControlledDhcpv6Srv::~ControlledDhcpv6Srv() {
    cleanup();
}

};
};
