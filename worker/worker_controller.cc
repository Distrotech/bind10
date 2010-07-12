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
#include "defaults.h"
#include "udp_buffer.h"
#include "utilities.h"
#include "worker_controller.h"


// Starts the target task.  Never returns.

void
WorkerController::run(Communicator& receptionist_communicator,
    Communicator& client_communicator)
{
    int num = 0;
    for (;;) {  // Forever

        // Receive the burst of packets and put them onto the queue
        for (int i = 0; i < burst_; ++i) {
            UdpBuffer buffer = receptionist_communicator.receive();
            queue_.push_back(buffer);
        }

        // Calculate the CRCs.  Since the packets include UDP endpoint
        // information, we extract it before doing the calculation.
        std::list<UdpBuffer>::iterator li;
        for (li = queue_.begin(); li != queue_.end(); ++li) {

            // Remove UDP endpoint information and store in buffer
            Utilities::ExtractEndpoint(*li);

            // Calculate CRC
            Utilities::Crc((li->data).get(), li->size);
            li->size += 4;
        }

        // Now return the packets back to the client.
        for (li = queue_.begin(); li != queue_.end(); ++li) {
            client_communicator.send(*li);
        }
        queue_.clear();
    }

    return;
}
