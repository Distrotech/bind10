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
#include "client_controller.h"
#include "defaults.h"
#include "logger.h"
#include "utilities.h"


// Runs the tests and records the results in the logger.
void
ClientController::run(ClientCommunicator& communicator, Logger& logger) {

    // Set up for the test.  The packet size is rounded up to a four-byte
    // boundary, and not allowed to be more than the 64-bytes less than the
    // maximum UDP packet size.  (A bit arbitrary, but allows for overhead.)
    size_t packet_size = ((pktsize_ + 3) / 4) * 4;
    packet_size = std::min(packet_size, (UDP_BUFFER_SIZE - 64));
    pktsize_ = boost::numeric_cast<uint16_t>(packet_size);
    logger.setPktsize(pktsize_);

    setUp();

    // Start the timer.
    logger.start();

    // Actually execute the test
    runTest(communicator);

    // Stop the timer and write it out.
    logger.end();

    return;
}



// Sets up the data for the test.
void
ClientController::setUp() {

    // Allocate the buffers.

    snd_buffer_ = boost::shared_array<uint8_t>(new uint8_t[UDP_BUFFER_SIZE]);
    rcv_buffer_ = boost::shared_array<uint8_t>(new uint8_t[UDP_BUFFER_SIZE]);

    // Fill the send buffer with data and calculate the checksum.

    for (unsigned int i = 0; i < pktsize_; ++i) {
        snd_buffer_[i] = boost::numeric_cast<uint8_t>(i % 256);
    }

    // Now calculate the CRC check and put onto the end of the buffer.
    // We'll send pktsize_ bytes of data, but compare pktsize_+4.  Note
    // that the check in the constructor ensures that we have this space.
    Utilities::Crc(snd_buffer_.get(), pktsize_);

    return;
}



// Sender task.  Sends up to "npacket" packets as fast as it can.
void
ClientController::sendTask(ClientCommunicator& communicator, int npacket) {

    // Send the packets as fast as we can.  The send() method will block when
    // the outstanding packet limit is reached.

    for (int i = 0; i < npacket; ++i) {
        communicator.send(snd_buffer_.get(), pktsize_);
    }

    return;
}



// Receiver task.  Receives "npacket" packets as fast as it can and compares
// the received packets with the sent ones (including CRC).  Prints an
// error if there is a mismatch.
void
ClientController::receiveTask(ClientCommunicator& communicator, int npacket) {

    for (int i = 0; i < npacket; ++i) {

        // Zero the receive buffer before issuing the receive to counter
        // the case where the receive fails but the buffer is left
        // unchanged.
        memset(rcv_buffer_.get(), 0, UDP_BUFFER_SIZE);
        size_t received  = communicator.receive(rcv_buffer_.get(),
            UDP_BUFFER_SIZE);

        // The received packet should be the same as the sent packet but
        // with the addition of a CRC.
        if (received != (pktsize_ + 4)) {
            std::cout << "Received packet wrong size: expected " << (pktsize_ + 4)
                << ", received " << received << std::endl;
        } else if (memcmp(snd_buffer_.get(), rcv_buffer_.get(), received) != 0) {
            std::cout << "Received packet differs from sent packet: ";
            if (memcmp(snd_buffer_.get() + pktsize_, rcv_buffer_.get() + pktsize_, 4) != 0) {
                std::cout << "checksum is different" << std::endl;
            } else {
                std::cout << "contents are different" << std::endl;
            }
        }
    }
}


// Runs the test.  Sends all the packets in groups of "burst" packets.  If
// the total number of packets is not a multiple of "burst", the number of
// packets is rounded up to the next multiple.
void
ClientController::runTest(ClientCommunicator& communicator) {

    int packets = count_;
    while (packets > 0) {

        // Send and receive a burst of packets

        sendTask(communicator, burst_);
        receiveTask(communicator, burst_);
        packets -= burst_;
    }

    return;
}
