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

/// \brief Contractor process for intermediary
///
/// This process (the contractor) receives data packets from the intermediary,
/// processes them (by adding a four-byte CRC) then returns them to the original
/// sender.
///
/// The program is invoked with the command:
///
/// contractor [--burst <burst>]
///
/// where...
///
/// --burst <burst> specifies how many packets the contractor receives before
/// processing them and returning them to the sender.
///
/// The option "--port" is also accepted, but is ignored.

#include <iostream>
#include <iomanip>

#include "burst_server_controller.h"
#include "defaults.h"
#include "exception.h"
#include "msgq_communicator_server.h"
#include "target_command.h"

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
        MsgqCommunicatorServer communicator(command.getQueue());
        communicator.open();

        // Create the task controller.
        BurstServerController controller(command.getBurst(),
            command.getMemorySize());

        // ... and enter the run loop.

        controller.run(communicator, communicator);

    } catch (Exception& e) {
        std::cerr << "Exception thrown" << std::endl;
        std::cerr << e.getReason() << std::endl;
    }

    return (0);
}
