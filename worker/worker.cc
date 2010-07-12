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

// processor.cc
//
// Description:
//      This is the main module for the receptionist worker program.  It is
//      little more than a shell around the main worker classes.
//
//      The worker simply listens for incoming packets.  These are then echoed
//      to the sender but with the addition of a 4-byte checksum on the end.
//
// Invocation:
//      The program is invoked with the command:
//
//      worker [--burst <burst>]
//
//      where...
//
//      --burst <burst>
//      How many packets the worker receives before passing them to the
//      sender task.
//
//      The option "--port" is also accepted, but is ignored.

#include <iostream>
#include <iomanip>

#include "defaults.h"
#include "target_command.h"
#include "msgq_communicator.h"
#include "udp_communicator.h"
#include "worker_controller.h"
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

        // Create the communicator module for communicating with the
        // receptionist.
        MsgqCommunicator receptionist_communicator(QUEUE_PROCESSED_NAME,
            QUEUE_UNPROCESSED_NAME);
        receptionist_communicator.open();

        // Create the communicator for sending data back to the client.
        UdpCommunicator client_communicator(0);
        client_communicator.open();

        // Create the task controller.
        WorkerController controller(command.getBurst());

        // ... and enter the run loop.

        controller.run(receptionist_communicator, client_communicator);

    } catch (Exception& e) {
        std::cerr << "Exception thrown" << std::endl;
        std::cerr << e.getReason() << std::endl;
    }

    return (0);
}
