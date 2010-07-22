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

#include <algorithm>
#include <iostream>
#include <list>
#include <utility>

#include "communicator.h"
#include "debug.h"
#include "intermediary_controller.h"
#include "packet_counter.h"
#include "udp_buffer.h"
#include "utilities.h"


// Starts the intermediary task.  Never returns.
void
IntermediaryController::run(Communicator& client_communicator,
   Communicator& contractor_communicator)
{
    PacketCounter counter;

    std::list<UdpBuffer> queue;         // Packet queue...
    std::list<UdpBuffer>::iterator li;  // and its iterator

    while (true) {  // Forever

        // Receive the burst of packets from the client and put them
        // into a local queue.
        for (int i = 0; i < burst_; ++i) {
            Debug::log(counter.incrementReceive(), "Receiving data from client");
            UdpBuffer buffer = client_communicator.receive();
            queue.push_back(buffer);
        }

        // Forward them to the contractor via the message queue.
        for (li = queue.begin(); li != queue.end(); ++li) {
            Debug::log(counter.incrementSend(), "Sending data to contractor");
            contractor_communicator.send(*li);
        }
        queue.clear();

        // Now read the packets from the contractor and store in the queue.
        for (int i = 0; i < burst_; ++i) {
            Debug::log(counter.incrementReceive2(),
                "Receiving data from contractor");
            UdpBuffer buffer = contractor_communicator.receive();
            queue.push_back(buffer);
        }

        // ... and forward them back to the client.
        for (li = queue.begin(); li != queue.end(); ++li) {
            Debug::log(counter.incrementSend2(), "Sending data to client");
            client_communicator.send(*li);
        }
        queue.clear();
    }

    return;
}
