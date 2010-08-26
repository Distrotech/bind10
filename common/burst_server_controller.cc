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
#include <iomanip>
#include <list>
#include <utility>

#include "communicator.h"
#include "debug.h"
#include "packet_counter.h"
#include "burst_server_controller.h"
#include "udp_buffer.h"
#include "utilities.h"


// Starts the server task.  Never returns.
void
BurstServerController::run(Communicator& receive_communicator,
    Communicator& send_communicator)
{
    // Initialize the packet count (outputs info on SIGTERM).
    PacketCounter counter;

    // If a memory size was specified, allocate it.
    char* memory = NULL;
    if (memsize_ > 0) {
        memory = new char[memsize_ * 1024];
    }

    std::list<UdpBuffer> queue;        //< Send/receive queue
    while (true) {  // Forever

        // Receive the burst of packets and put them onto the queue
        for (int i = 0; i < burst_; ++i) {
            Debug::log(counter.incrementReceive(), "Calling receive");
            UdpBuffer buffer = receive_communicator.receive();
            queue.push_back(buffer);
        }

        // Calculate the CRC and put in the packet.
        std::list<UdpBuffer>::iterator li;
        for (li = queue.begin(); li != queue.end(); ++li) {
            uint32_t crc = Utilities::Crc(li->data(), li->dataSize());
            li->setCrc(crc);

            // If a memory size was specified, run over the memory and touch
            // each page (assumed to be 1kB pages to cope with most
            // architectures).
            if (memsize_ > 0) {
                touchMemory(memory, memsize_);
            }
        }

        // Now return the packets back to the sender
        for (li = queue.begin(); li != queue.end(); ++li) {
            Debug::log(counter.incrementSend(), "Calling send");
            send_communicator.send(*li);
        }
        queue.clear();
    }

    return;
}
