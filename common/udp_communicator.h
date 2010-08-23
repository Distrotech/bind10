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

#ifndef __UDP_COMMUNICATOR_H
#define __UDP_COMMUNICATOR_H

#include <string>

#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/utility.hpp>

#include "communicator.h"
#include "udp_buffer.h"

/// \brief Server Communication over UDP
///
/// The communicator encapsulates all the I/O needed for a test server that
/// communicates using UDP.  After construction, the interface is really the
/// following calls:
/// - open: open link to the udp
/// - send: send the data to the udp and return immediately
/// - receive: receive data and block
/// - close: close link to the udp
///
/// For simplicity of implementation, this class is non-copyable.

class UdpCommunicator : public Communicator {
public:

    /// \brief Shorthand for a long declaration.
    typedef boost::shared_ptr<boost::asio::ip::udp::socket> ptr_socket;
    
    /// \brief Store parameters
    ///
    /// Stores the parameters that were given on the command line.
    /// \param port Port number on the udp to which communications should be
    /// addressed.  A value of zero means that no port is connected and that
    /// the object cannot be used for receiving data.
    UdpCommunicator(uint16_t port = 0) : Communicator(), port_(port),
        io_service_(), socket_ptr_()
       {}
    virtual ~UdpCommunicator() {}


    /// \brief Open link to udp
    ///
    /// Sets up for communication.  As UDP is used, no logical connection is
    /// made: rather the sockets and the like are created, and "connect" called
    /// to bind the socket to the target.
    ///
    /// \exception Exception Address or port is invalid.
    virtual void open();

    /// \brief Send data to udp
    ///
    /// Sends data to the udp without waiting.  As there is only one channel,
    /// this just maps to the other send function.
    ///
    /// \param channel Channel on which to send data
    /// \param buffer Data to be sent to the udp.
    virtual void send(uint32_t channel, UdpBuffer& buffer) {
        send(buffer);
    }

    /// \brief Send data to udp
    ///
    /// Sends data to the udp without waiting.
    ///
    /// \param buffer Data to be sent to the udp.
    virtual void send(UdpBuffer& buffer);

    /// \brief Receive data from the udp
    ///
    /// Receives data from the udp.  This call blocks until data is received
    /// or until the timeout has expired.
    virtual UdpBuffer receive();

    /// \brief Close down link
    ///
    /// Closes down the link to the udp, deallocates all resources.
    virtual void close();

protected:
    uint16_t        port_;              //< Port as passed to constructor
    boost::asio::io_service io_service_; //< Connection to the network
    ptr_socket      socket_ptr_;        //< Socket for network connection
};

#endif // __UDP_COMMUNICATOR_H
