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
#include <sstream>

#include <boost/asio.hpp>
#include <boost/interprocess/creation_tags.hpp>
#include <boost/system/error_code.hpp>
#include <boost/system/system_error.hpp>

#include "defaults.h"
#include "msgq_communicator.h"
#include "defaults.h"
#include "exception.h"
#include "udp_buffer.h"

// Opens the connection to the network, in this case associating with the
// message queues.  The queue is deleted before it is created; if it already
// exists, it may have some messages in it that we are not interested in.
//
// Note that as two programs (p1 and p2) use the message queue, both must
// be stopped and restarted for the delections to be effective; if both are
// running, the sequence stop p1, start p1, stop p2, start p2 is not effective -
// the message queue is never closed to the deletion will fail.

void
MsgqCommunicator::open() {

//    (void) boost::interprocess::message_queue::remove(rcv_name_.c_str());
    rcv_queue_ = mq_ptr(
        new boost::interprocess::message_queue(
            boost::interprocess::open_or_create,
            rcv_name_.c_str(), QUEUE_MAX_MESSAGE_COUNT, QUEUE_MAX_MESSAGE_SIZE)
    );

//    (void) boost::interprocess::message_queue::remove(snd_name_.c_str());
    snd_queue_ = mq_ptr(
        new boost::interprocess::message_queue(
            boost::interprocess::open_or_create,
            snd_name_.c_str(), QUEUE_MAX_MESSAGE_COUNT, QUEUE_MAX_MESSAGE_SIZE)
    );

    return;
}

// Sends data back to the caller.  The data is assumed to ber processed, so
// it is put on the processed queue.  The method blocks if the queue is full.
void
MsgqCommunicator::send(UdpBuffer& buffer) {

    snd_queue_->send(
        buffer.data(),          // Data element
        buffer.size(),          // Amount of data transferred
        QUEUE_PRIORITY);        // Message priority

    return;
}

// Receive data from the target.  This just pulls the next message off the
// unprocessed queue.  It blocks if there is no data.
UdpBuffer
MsgqCommunicator::receive() {

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
// so triggerting the message queue destructors.
void
MsgqCommunicator::close() {

    snd_queue_.reset();
    (void) boost::interprocess::message_queue::remove(snd_name_.c_str());

    rcv_queue_.reset();
    (void) boost::interprocess::message_queue::remove(rcv_name_.c_str());

    return;
}

