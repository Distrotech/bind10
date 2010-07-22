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

// Runs the test

#include <algorithm>
#include <iostream>
#include <iomanip>
#include <arpa/inet.h>
#include <utility>

#include <boost/cast.hpp>
#include <boost/crc.hpp>
#include <boost/cast.hpp>

#include "client_communicator.h"
#include "client_controller_synchronous.h"
#include "debug.h"
#include "defaults.h"
#include "logger.h"
#include "packet_counter.h"
#include "utilities.h"



// Runs the test.  Sends all the packets in groups of "burst" packets.  If
// the total number of packets is not a multiple of "burst", the number of
// packets is rounded up to the next multiple.
void
ClientControllerSynchronous::runTest(ClientCommunicator& communicator,
    Logger& logger) {

    logger.start();
    while (count_ > 0) {

        // Send and receive a burst of packets

        sendTask(communicator, burst_);
        receiveTask(communicator, burst_);
        count_ -= burst_;
    }
    logger.end();

    return;
}



// Sender task.  Sends up to "npacket" packets as fast as it can.
void
ClientControllerSynchronous::sendTask(ClientCommunicator& communicator,
    int npacket) {

    // Send the packets as fast as we can.
    for (int i = 0; i < npacket; ++i) {
        Debug::log(PacketCounter::incrementSend(), "sending packet");
        communicator.send(snd_buffer_.data(), snd_buffer_.size());
    }

    return;
}



// Receiver task.  Receives "npacket" packets as fast as it can and compares
// the received packets with the sent ones (including CRC).  Prints an
// error if there is a mismatch.
void
ClientControllerSynchronous::receiveTask(ClientCommunicator& communicator,
    int npacket) {

    for (int i = 0; i < npacket; ++i) {

        // Zero the receive buffer before issuing the receive to counter
        // the case where the receive fails but the buffer is left
        // unchanged.
        rcv_buffer_.zero();
        Debug::log(PacketCounter::incrementReceive(), "receiving packet");
        size_t received  = communicator.receive(rcv_buffer_.data(),
            rcv_buffer_.size());

        // The received packet should be the same as the sent packet but
        // with the addition of a CRC.
        if (received != rcv_buffer_.size()) {
            std::cout << "Received packet wrong size: expected "
                << rcv_buffer_.size() << ", received " << received << "\n";
        } else if (! snd_buffer_.compareData(rcv_buffer_)) {
            std::cout << "Received data differs from sent packet\n";
        } else if (! snd_buffer_.compareCrc(rcv_buffer_)) {
            std::cout << "Received CRC differs from sent packet\n";
        }
    }
}
