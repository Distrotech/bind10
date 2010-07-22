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

#ifndef __CLIENT_COMMUNICATOR_H
#define __CLIENT_COMMUNICATOR_H

#include <string>

#include <boost/asio.hpp>
#include <boost/utility.hpp>

/// \brief Communcates with the server
///
/// The communicator encapsulates all the I/O needed between the client and the
/// server program,.  After construction, the interface is really the following
/// calls:
/// - open: open link to the server
/// - send: send the data to the server and return immediately
/// - receive: receive data and block
/// - close: close link to the server
///
/// For simplicity of implementation, this class is non-copyable.
class ClientCommunicator : private boost::noncopyable {
public:
    
    /// \brief Store parameters
    ///
    /// Stores the parameters that were given on the command line.
    /// \param address Address of the server.  This is a string: validation is
    /// deferred until the channel is opened.
    /// \param port Port number on the server to which communications should be
    /// addressed
    ClientCommunicator(const std::string& address, uint16_t port) :
       address_(address), port_(port), io_service_()
       {}

    /// \brief Open link to server
    ///
    /// Sets up for communications, setting buffer size and the like.
    ///
    /// \exception Exception Address or port is invalid.
    void open();

    /// \brief Send data to server
    ///
    /// Sends data to the server without waiting.
    ///
    /// \param data Data to be sent to the server.
    /// \param size Length of data to be sent.
    ///
    /// \exception Exception some error on the send.
    void send(const uint8_t* data, size_t size);

    /// \brief Receive data from the server
    ///
    /// Receives data from the server.  This call blocks until data is received
    /// or until the timeout has expired.
    ///
    /// \param buffer Pointer to buffer to hold the data
    /// \param size Size of buffer
    ///
    /// \exception Exception some error on the send.
    size_t receive(uint8_t* buffer, size_t size);

    /// \brief Close down link
    ///
    /// Closes down the link to the server, deallocates all resources.
    void close();

private:
    std::string     address_;           //< Address as passed to constructor
    uint32_t        port_;              //< Port as passed to constructor
    boost::asio::io_service io_service_; //< Connection to the network
    boost::shared_ptr<boost::asio::ip::udp::socket> socket_; //< Socket for network connection
    boost::asio::ip::udp::endpoint  endpoint_;  // Target endpoint for comms
};

#endif // __CLIENT_COMMUNICATOR_H
