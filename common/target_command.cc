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

#include <iostream>
#include <iomanip>
#include <stdint.h>
#include <string>

#include <boost/program_options.hpp>

#include "target_command.h"
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

TargetCommand::TargetCommand(int argc, char** argv) : port_(0),
    desc_("Usage: <target> [options...]"), vm_()
{
    // Parse the command line

    parseCommandLine(argc, argv);

    return;
}


// Print usage information to the specified stream.
void
TargetCommand::usage(std::ostream& output) const {
    output << desc_ << std::endl;

    return;
}


// Parse the command line and return a variable map with all the options set.
// The module automatically sets the debug level - there is no need for the
// caller to worry about that.

void
TargetCommand::parseCommandLine(int argc, char** argv)  {
    uint32_t    debug;      // Debug level

    // Set up the command-line options

    desc_.add_options()
        ("help", "produce help message")
        ("burst,b",
            po::value<uint32_t>(&burst_)->default_value(CL_DEF_BURST),
            "Burst size: number of packets processed at one time")
        ("debug,d",
            po::value<uint32_t>(&debug)->default_value(CL_DEF_DEBUG),
            "Debug level: a value of 0 disables debug messages")
        ("port,p",
            po::value<uint16_t>(&port_)->default_value(CL_DEF_PORT),
            "Port on which to listen");

    // Parse

    po::store(po::command_line_parser(argc, argv).options(desc_).run(), vm_);
    po::notify(vm_);

    // ... and handle options that we can cope with internally.

    Debug::setLevel(debug);

    return;
}
