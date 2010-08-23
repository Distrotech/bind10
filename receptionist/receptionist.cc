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

/// \brief Receptionist process
///
/// The receptionist process receives packets from the client, batches them
/// up and forwards them to the worker.  The worker takes care of returning
/// them to the client.
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
#include <vector>
#include <boost/lexical_cast.hpp>

#include <sys/types.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#include "children.h"
#include "defaults.h"
#include "string.h"
#include "target_command.h"
#include "msgq_communicator_client.h"
#include "udp_communicator.h"
#include "receptionist_controller.h"
#include "exception.h"

int main(int argc, char**argv)
{
    try {
        // Parse the command line.
        TargetCommand command(argc, argv);
        if (command.getHelp()) {
            command.usage(std::cout);
            return (0);
        }

        // Create the child processes if asked.
        std::string worker = command.getWorker();
        if (! worker.empty()) {
            Children::spawnChildren(worker, command.getQueue(),
                command.getMemorySize(), command.getDebug());
        }

        // Create the I/O modules and initialize.
        UdpCommunicator client_communicator(command.getPort());
        client_communicator.open();

        // TODO: improve terminology
        // Create a client message queue object (as the receptionist
        // is client to the worker server).  The --queue on the command
        // line specifies the number of queues to create.
        MsgqCommunicatorClient worker_communicator(command.getQueue());
        worker_communicator.open();

        // Create the task controller.
        ReceptionistController controller(command.getBurst(), command.getQueue());

        // ... and enter the run loop.

        controller.run(client_communicator, worker_communicator);

    } catch (Exception& e) {
        std::cerr << "Exception thrown" << std::endl;
        std::cerr << e.getReason() << std::endl;
    }

    return (0);
}
