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

#include <iostream>
#include <iomanip>
#include <sstream>

#include <boost/asio.hpp>
#include <boost/interprocess/creation_tags.hpp>
#include <boost/system/error_code.hpp>
#include <boost/system/system_error.hpp>
#include <boost/lexical_cast.hpp>

#include "defaults.h"
#include "msgq_communicator_client.h"
#include "defaults.h"
#include "exception.h"
#include "udp_buffer.h"

using namespace std;

// Opens the connection to the network, in this case associating with the
// message queues.

void
MsgqCommunicatorClient::open() {

    // Create the receive queue with a known name,.
    rcv_queue_ = mq_ptr(
        new boost::interprocess::message_queue(
            boost::interprocess::open_or_create,
            "ISC_QUEUE_0", QUEUE_MAX_MESSAGE_COUNT, QUEUE_MAX_MESSAGE_SIZE)
    );

    // Create the send queues.
    for (int i = 1; i <= count_; ++i) {
        string name = string("ISC_QUEUE_") + boost::lexical_cast<string>(i);
        mq_ptr queue = mq_ptr(
            new boost::interprocess::message_queue(
            boost::interprocess::open_or_create,
            name.c_str(), QUEUE_MAX_MESSAGE_COUNT, QUEUE_MAX_MESSAGE_SIZE)
            );
        snd_queue_.push_back(queue);
    }

    return;
}

// Sends data back to the caller.  The data is assumed to be processed, so
// it is put on the processed queue.  The method blocks if the queue is full.
void
MsgqCommunicatorClient::send(uint32_t queue, UdpBuffer& buffer) {

    snd_queue_[queue - 1]->send(
        buffer.data(),          // Data element
        buffer.size(),          // Amount of data transferred
        QUEUE_PRIORITY);        // Message priority

    return;
}

// Receive data from the target.  This just pulls the next message off the
// unprocessed queue.  It blocks if there is no data.
UdpBuffer
MsgqCommunicatorClient::receive() {

    UdpBuffer buffer;            // Buffer to receive data
    size_t  received_size;       // Amount of data received
    unsigned int priority;       // Priority of received data

    rcv_queue_->receive(buffer.data(), buffer.capacity(), received_size,
        priority);
    buffer.setSize(received_size);

    return (buffer);
}

// Closes down the message queues by deleting them.  This may not work if
// something else has them open. The pointers to the queues are reset,
// so triggering the message queue destructors.
void
MsgqCommunicatorClient::close() {

    (void) boost::interprocess::message_queue::remove("ISC_QUEUE_0");

    for (int i = 1; i < count_; ++i) {
        string name = string("ISC_QUEUE_") + boost::lexical_cast<string>(i);
        (void) boost::interprocess::message_queue::remove(name.c_str());
    }

    return;
}

