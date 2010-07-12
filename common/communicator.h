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

#ifndef __COMMUNICATOR_H
#define __COMMUNICATOR_H

#include <boost/utility.hpp>

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

class Communicator : private boost::noncopyable {
public:
    
    /// \brief Store parameters
    ///
    /// Stores the parameters that were given on the command line.
    /// \param port Port number on the server to which communications should be
    /// addressed
    Communicator() {}
    virtual ~Communicator() {}

    /// \brief Open link to server
    ///
    /// Sets up for communication.  As UDP is used, no logical connection is
    /// made: rather the sockets and the like are created, and "connect" called
    /// to bind the socket to the target.
    ///
    /// \exception Exception Address or port is invalid.
    virtual void open() = 0;

    /// \brief Send data to server
    ///
    /// Sends data to the server without waiting.
    ///
    /// \param buffer Data to be sent to the server.
    virtual void send(UdpBuffer& buffer) = 0;

    /// \brief Receive data from the server
    ///
    /// Receives data from the server.  This call blocks until data is received
    /// or until the timeout has expired.
    virtual UdpBuffer receive() = 0;

    /// \brief Close down link
    ///
    /// Closes down the link to the server, deallocates all resources.
    virtual void close() = 0;
};

#endif // __TARGET_COMMUNICATOR_H
