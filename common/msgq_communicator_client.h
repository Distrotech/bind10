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

#ifndef __MSGQ_COMMUNICATOR_CLIENT_H
#define __MSGQ_COMMUNICATOR_CLIENT_H

#include <string>

#include <boost/interprocess/ipc/message_queue.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/utility.hpp>

#include "communicator.h"
#include "udp_buffer.h"

/// \brief Client Message Communicator
///
/// This will instantiate a number of different message queues for communicating
/// with multiple clients, and one message queue for receiving messages.
/// The interface is basically:
/// open - create/open message queues
/// send - send data on appropriate channel
/// receive - receive data
/// close - close all queues

class MsgqCommunicatorClient : public Communicator {
public:

    /// \brief Pointer to Message Queue
    typedef boost::shared_ptr<boost::interprocess::message_queue> mq_ptr;

    /// \brief Store parameters
    ///
    /// \param count Number of send message queues to create.
    MsgqCommunicatorClient(uint32_t count = 1) : count_(count) {}

    /// \brief Destructor
    virtual ~MsgqCommunicatorClient() {}

    /// \brief Open Link
    ///
    /// Sets up for communication.  As message queues are being used, this
    /// justs links to the queues.  The queues are created if they don't
    /// already exist.
    virtual void open();

    /// \brief Send data
    ///
    /// Places a pqacket of data on the outgoing message queue, waiting if
    /// there is no space available.
    ///
    /// \param queue Queue number on which to send data.  If the queue does
    /// not exist, an exception is thrown.  The queue number must be in the
    /// range 1..count (where count is the value given in the constructor).
    /// \param buffer Data to be sent to the server.
    virtual void send(uint32_t queue, UdpBuffer& buffer);

    /// \brief Send Data
    ///
    /// Sends data on the first outgoing queue.
    /// \param buffer Data to be sent to the server.
    virtual void send(UdpBuffer& buffer) {
        send(1, buffer);
    }

    /// \brief Receive data
    ///
    /// Reads a packet from the incoming message queue, waiting if none is
    /// available.
    ///
    /// \return Buffer describing data received
    virtual UdpBuffer receive();

    /// \brief Close down link
    ///
    /// Closes down the link to the server, deallocates all resources.
    virtual void close();

private:
    uint32_t    count_;             ///< Count of outgoing message queues
    mq_ptr  rcv_queue_;             ///< Incoming queue
    std::vector<mq_ptr> snd_queue_; ///< Outgoing queues
};
    
#endif // __MSGQ_COMMUNICATOR_CLIENT_H
