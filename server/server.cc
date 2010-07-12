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

/// \brief Server process
///
/// The server process receives packets from the client, calculates the
/// checksum, and returns them to the client.  The packets are processed in
/// batches, specified by the "burst" option.
///
/// The program is invoked by the command:
///
/// receptionist [--burst <burst>]
///
/// where...
///
/// --burst <burst>
/// How many packets the recptionist will wait for before batching them up for
/// the worker.
///
/// The option "--port" is also recognised but is ignored.

#include <iostream>
#include <iomanip>

#include "target_command.h"
#include "server_controller.h"
#include "udp_communicator.h"
#include "exception.h"

#include <string.h>

int main(int argc, char**argv)
{
    try {
        // Parse the command line.
        TargetCommand command(argc, argv);
        if (command.getHelp()) {
            command.usage(std::cout);
            return (0);
        }

        // Create the I/O module and initialize.
        UdpCommunicator communicator(command.getPort());
        communicator.open();

        // Create the task controller.
        ServerController controller(command.getBurst());

        // ... and enter the run loop.

        controller.run(communicator, communicator);

    } catch (Exception& e) {
        std::cerr << "Exception thrown" << std::endl;
        std::cerr << e.getReason() << std::endl;
    }

    return (0);
}
