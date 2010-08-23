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

#ifndef __MSGQ_COMMUNICATOR_SERVER_H
#define __MSGQ_COMMUNICATOR_SERVER_H

#include <string>

#include <boost/interprocess/ipc/message_queue.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/utility.hpp>

#include "communicator.h"
#include "udp_buffer.h"

/// \brief Communcates with the server
///
/// The communicator encapsulates all the I/O needed between the client and the
/// server.  After construction, the interface is really the following calls:
/// - open: open link to the server
/// - send: send the data to the server and return immediately
/// - receive: receive data and block
/// - close: close link to the server
///
/// For simplicity of implementation, this class is non-copyable.

class MsgqCommunicatorServer : public Communicator {
public:

    /// \brief Pointer to Message Queue
    typedef boost::shared_ptr<boost::interprocess::message_queue> mq_ptr;

    /// \brief Store parameters
    ///
    /// \param number Number of the message queue on which to receive data
    MsgqCommunicatorServer(uint32_t number) : number_(number)
       {}

    virtual ~MsgqCommunicatorServer() {}

    /// \brief Open Link
    ///
    /// Sets up for communication.  As message queues are being used, this
    /// justs links to the queues.  The queues are created if they don't
    /// already exist.
    virtual void open();

    /// \brief Send data
    ///
    /// Places a packet of data on the outgoing message queue, waiting if
    /// there is no space available.
    ///
    /// \param buffer Data to be sent to the server.
    virtual void send(UdpBuffer& buffer);

    /// \brief Send data
    ///
    /// Places a packet of data on the outgoing message queue, waiting if
    /// there is no space available.  As there is only one channel, this
    /// is a no-op.
    ///
    /// \param channel Channel on which to send data
    /// \param buffer Data to be sent to the server.
    virtual void send(uint32_t channel, UdpBuffer& buffer) {
        send(buffer);
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
    // Both message queues are stored in shared pointer objects to allow
    // creation to be deferred until the "open" method.

    uint32_t   number_;         ///< Number of incoming message queue
    mq_ptr snd_queue_;          ///< Outgoing
    mq_ptr rcv_queue_;          ///< Incoming

};

#endif // __MSGQ_COMMUNICATOR_SERVER_H
