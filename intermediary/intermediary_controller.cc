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

#include <boost/thread.hpp>

#include "communicator.h"
#include "debug.h"
#include "intermediary_controller.h"
#include "packet_counter.h"
#include "udp_buffer.h"
#include "utilities.h"

// Forward task wrapper.  It is only specified to simplify the starting of
// the wrd ask in a separate thread.
static void forwardTaskWrapper(IntermediaryController& controller,
    Communicator& client_communicator, Communicator& contractor_communicator)
{
    controller.forwardTask(client_communicator, contractor_communicator);

    return;
}

// Forwarded task - forward packets from client to contractor in bursts

void
IntermediaryController::forwardTask(Communicator& client_communicator,
   Communicator& contractor_communicator)
{
    PacketCounter counter;              // Access global counters
    std::list<UdpBuffer> queue;         // Packet queue...
    std::list<UdpBuffer>::iterator li;  // and its iterator
    uint32_t pktnum;                    // Packet number

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
            contractor_communicator.send(((pktnum++ % ncontractor_) + 1), *li);
        }
        queue.clear();
    }
}

void
IntermediaryController::returnTask(Communicator& client_communicator,
   Communicator& contractor_communicator)
{
    PacketCounter counter;              // Access global counters
    std::list<UdpBuffer> queue;         // Packet queue...
    std::list<UdpBuffer>::iterator li;  // and its iterator

    while (true) {

        // Read the packets from the contractor and store in the queue.
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


// Starts the intermediary task.  Never returns.
void
IntermediaryController::run(Communicator& client_communicator,
   Communicator& contractor_communicator)
{
 
    // Start the forwarding task in a separate thread.
    boost::thread forward_thread(forwardTaskWrapper,
        boost::ref(*this), boost::ref(client_communicator),
        boost::ref(contractor_communicator));

    // ... and start the return task in this thread.
    returnTask(client_communicator, contractor_communicator);

    // Should never terminate!
    return;
}
