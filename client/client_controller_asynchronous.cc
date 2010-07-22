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
#include <unistd.h>

#include <boost/cast.hpp>
#include <boost/thread.hpp>

#include "client_communicator.h"
#include "client_controller_asynchronous.h"
#include "debug.h"
#include "defaults.h"
#include "logger.h"
#include "packet_counter.h"
#include "token_bucket.h"
#include "utilities.h"

// Sender task.  Sends packets as fast as it can.
void
ClientControllerAsynchronous::sendTask(ClientCommunicator& communicator,
    TokenBucket& synchronizer)
{

    // Just keep sending packets.  The send will block when the the outstanding
    // packet limit is reached.
    //
    // Note that packets are numbered from one on, which allows the receiver
    // thread to initialize the unsigned "last packet number" field to 0.

    bool terminate = false;
    for (uint32_t i = 1; !terminate; ++i) {

        // Send a packet (always assumed to be able to send the first packet).
        Debug::log(PacketCounter::incrementSend(), "calling send()");
        snd_buffer_.setPacketNumber(i);
        communicator.send(snd_buffer_.data(), snd_buffer_.size());

        // Wait for the pipeline to clear (i.e. the number of outstanding
        // packets to fall below the threshold).
        terminate = synchronizer.removeToken();
    }

    return;
}

// Send task wrapper.  It is only specified to simplify the starting of
// the send task in a separate thread.
static void sendTaskWrapper(ClientControllerAsynchronous& controller,
    ClientCommunicator& communicator, TokenBucket& synchronizer)
{
    controller.sendTask(communicator, synchronizer);

    return;
}


// Receiver task.  Run in the main thread, this receives packets as fast as
// it can and does a memory comparison with the send buffer.  It exits when
// a fixed number of packets has been received.
void
ClientControllerAsynchronous::receiveTask(ClientCommunicator& communicator,
    TokenBucket& synchronizer)
{
    // Received packets should start at packet number 1, so set the last
    // packet number to 0.
    uint32_t    last_packet = 0;

    // Receive the packets as fast as we can.
    for (int i = 0; i < count_; ++i) {

        // Zero the receive buffer before issuing the receive to counter
        // the case where the receive fails but the buffer is left
        // unchanged.
        rcv_buffer_.zero();
        Debug::log(PacketCounter::incrementReceive(), "calling receive()");
        size_t received  = communicator.receive(rcv_buffer_.data(),
            rcv_buffer_.capacity());
        rcv_buffer_.setSize(received);

        // Check the packet number and check how many packets have been dropped.
        // (The packets are numbered sequentially and should be received in the
        // same order in these simple tests.)  Add the number to the lost count.

        uint32_t missing = 0;
        uint32_t this_packet = rcv_buffer_.getPacketNumber();
        if (this_packet < last_packet) {
            std::cout << "ERROR: packets received out of sequence\n";
        }
        else {
            missing = this_packet - last_packet - 1;
            lost_ += missing;
            last_packet = this_packet;
        }


        // Have now got rid of one outstanding packet, so notify the
        // synchronization mechanism of that; this will allow the sender to
        // send another packet.  Note that we also account for missing packets
        // here to prevent the client from hanging.
        //
        // On the last iteration, tell the sender that it should terminate
        // instead.
        if (i < (count_ - 1)) {
            synchronizer.addToken(missing + 1);
        }
        else {
            synchronizer.notify();
        }

        // The received packet should be the same as the sent packet but
        // with the addition of a valid CRC.  Check this.
        if (rcv_buffer_.size() != snd_buffer_.size()) {
            std::cout << "Received packet wrong size: expected "
                << snd_buffer_.size() << ", received " << received << "\n";
        } else if (! rcv_buffer_.compareData(snd_buffer_)) {
            std::cout << "Received packet data differs from sent packet\n";
        } else if (! rcv_buffer_.compareCrc(snd_buffer_)) {
            std::cout << "Received packet CRC differs from sent packet\n";
        }
    }
}


// Runs the test.  Sets up a separate thread for the receiver task and
// executes the sender task in the main thread.  Returns only when both have
// finished.
void
ClientControllerAsynchronous::runTest(ClientCommunicator& communicator,
    Logger& logger) {

    // Create the synchronization object with zero tokens.  This will allow the
    // starting of the sending thread without starting execution.
    TokenBucket synchronizer(0);

    // Start the send task in a separate thread.  It will immediately block
    // because of the synchronizer's token limit.  Give it a second to
    // initialize.
    boost::thread send_thread(sendTaskWrapper,
        boost::ref(*this), boost::ref(communicator), boost::ref(synchronizer));
    sleep(1);

    // Now start the timer, kick off the send thread by adding the required
    // number of tokens to the bucket.  Start the receive task in this thread.
    // When the required number of packets has been received, end the timer.
    logger.start();
    synchronizer.addToken(burst_ + margin_);
    receiveTask(communicator, synchronizer);
    logger.end();

    // Record how many packets were lost in the test
    logger.setLost(lost_);

    // When the receive thread exits, it will have notified the sending task to
    // terminate as well.
    send_thread.join();

    return;
}
