// Copyright (C) 2010  Internet Systems Consortium, Inc. ("ISC")
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

// $Id$

#include <iomanip>
#include <stdint.h>
#include <string>

#include <boost/program_options.hpp>

#include "client_command.h"
#include "defaults.h"
#include "debug.h"

namespace po = boost::program_options;

// Constructor
//
// Passed the command-line arguments, the constructor parses them and sets the
// member variables.
//
// All members are initialized to their defaults in when the command line is
// parsed, hence the initialization to zero values here.

ClientCommand::ClientCommand(int argc, char** argv) :
    address_(""), count_(0), logfile_(""), size_(0), port_(0),
    desc_("Usage: client [options...]"), vm_(), burst_(0), margin_(0)
{
    // Parse the command line

    parseCommandLine(argc, argv);

    return;
}


// Print usage information to the specified stream.
void
ClientCommand::usage(std::ostream& output) const {
    output << desc_ << std::endl;

    return;
}


// Parse the command line and return a variable map with all the options set.
// The method does not validate the string arguments (--address and --logfile)

void
ClientCommand::parseCommandLine(int argc, char** argv)  {

    // Set up the command-line options

    uint32_t dbglevel;  // Debug level

    desc_.add_options()
        ("help", "produce help message")
        ("address,a",
            po::value<std::string>(&address_)->default_value(CL_DEF_ADDRESS),
            "IP address (V4 or V6) of the server system")
        ("asynchronous,y",
            "Specify for asynchronous tests; the default is synchronous")
        ("burst,b",
            po::value<long>(&burst_)->default_value(CL_DEF_BURST),
            "Burst size: for asynchronous runs, this is logged but not used")
        ("count,c",
            po::value<long>(&count_)->default_value(CL_DEF_COUNT),
            "Count of packets to send")
        ("debug,d",
            po::value<uint32_t>(&dbglevel)->default_value(CL_DEF_DEBUG),
            "Debug level")
        ("logfile,l",
            po::value<std::string>(&logfile_)->default_value(CL_DEF_LOGFILE),
            "File to which statistics are logged")
        ("margin,m",
            po::value<long>(&margin_)->default_value(CL_DEF_LOST),
            "Maximum allowed lost packets in asynchronous tests")
        ("size,k",
            po::value<uint16_t>(&size_)->default_value(CL_DEF_PKTSIZE),
            "Size of each packet (max = 65,536)")
        ("port,p",
            po::value<uint16_t>(&port_)->default_value(CL_DEF_PORT),
            "Port on server to which packets are sent")
        ("synchronous,s",
            "Specify for synchronous tests; the default is asynchronous");

    // Parse

    po::store(po::command_line_parser(argc, argv).options(desc_).run(), vm_);
    po::notify(vm_);

    // Set the debug level.

    Debug::setLevel(dbglevel);

    return;
}
