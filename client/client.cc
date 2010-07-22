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

// client_main.cc
//
// Description:
//      This is the main module for the receptionist client program.  It is
//      little more than a shell around the main worker classes,
//
// Invocation:
//      The program is invoked with the command:
//
//      client [--address <addr>] [--port <port>] [--pktsize <pktsize>]
//             [--count <count>] [--log <file>] 
//
//      where...
//
//      --address <addr>
//      The address to which the client should connect (V4 address).  The
//      default is 127.0.0.1
//
//      --port <portnum>
//      The port to which the client should connect on the target system.
//      If not specified, port 5400 is assumed.
//
//      --pktsize <pktsize>
//      The size of each packet.  By default, packets of 8192 bytes are sent.
//
//      --count <count>
//      Total number of packets to send.  By default, this is 256.
//
//      --log <file>
//      Name of the log file.  By default, information is logged to stdout.

#include <string.h>

#include <iostream>
#include <iomanip>

#include "client_command.h"
#include "client_communicator.h"
#include "client_controller_asynchronous.h"
#include "client_controller_synchronous.h"
#include "exception.h"
#include "logger.h"


int main(int argc, char**argv)
{
    try {
        // Parse the command line.
        ClientCommand command(argc, argv);
        if (command.getHelp()) {
            command.usage(std::cout);
            return (0);
        }

        // Create the I/O module and initialize.
        ClientCommunicator communicator(command.getAddress(),
            command.getPort());
        communicator.open();

        // Initialize the logging module
        Logger logger(command.getLogfile(),
            command.getCount(), command.getSize(),
            command.getBurst(), command.getMargin());

        // Run the tests
        if (command.isAsynchronous()) {
            ClientControllerAsynchronous controller(command.getCount(),
                command.getBurst(), command.getSize(), command.getMargin());
            controller.run(communicator, logger);
        } else {
            ClientControllerSynchronous controller(command.getCount(),
                command.getBurst(), command.getSize());
            controller.run(communicator, logger);
        }

        // Print the results 
        logger.log();

        // ... and finish up.
        communicator.close();

    } catch (Exception& e) {
        std::cerr << "Exception thrown" << std::endl;
        std::cerr << e.getReason() << std::endl;
    }

    return (0);
}
