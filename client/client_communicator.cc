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

#include <boost/system/error_code.hpp>
#include <boost/system/system_error.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/asio.hpp>

namespace ip = boost::asio::ip;

#include "client_communicator.h"
#include "defaults.h"
#include "exception.h"

// Opens link.  As this is UDP, it is not actually a link open. Instead, it
// creates the neccessary 
void
ClientCommunicator::open() {

    // Validate that the address given is an IP V4 address.
    boost::system::error_code errcode;
    ip::address_v4 v4addr = ip::address_v4::from_string(address_, errcode);
    if (errcode.value() != 0) {
        std::stringstream errmsg;
        errmsg << "'" << address_ << "' is not an IPV4 address";
        throw Exception(errmsg.str());
    }

    // Construct the endpoint from it
    endpoint_ = ip::udp::endpoint(v4addr, port_);

    // Create the socket.
    socket_ = boost::shared_ptr<ip::udp::socket>(
        new ip::udp::socket(io_service_, ip::udp::v4()));

    // Bump up the socket send and receive sizes.
    boost::asio::socket_base::send_buffer_size snd_option(SCKT_MAX_SND_SIZE);
    socket_->set_option(snd_option);
    boost::asio::socket_base::receive_buffer_size rcv_option(SCKT_MAX_RCV_SIZE);
    socket_->set_option(rcv_option);

    return;
}

// Send data to the target (which is assumed to be listening).
void
ClientCommunicator::send(const uint8_t* data, size_t size) {

    try {
        // Send the data.
        size_t sent = socket_->send_to(boost::asio::buffer(data, size),
            endpoint_);
        if (sent != size) {
            throw Exception("Not all data passed to send() was sent");
        }
    } catch (boost::system::system_error& e) {
        throw Exception("Error on socket send", e.what());
    }

    return;
}

// Receive data from the target.
size_t
ClientCommunicator::receive(uint8_t* buffer, size_t size) {

    size_t received = 0;
    ip::udp::endpoint endpoint;

    try {
        received = socket_->receive_from(boost::asio::buffer(buffer, size),
            endpoint);
    } catch (boost::system::system_error& e) {
        throw Exception("Error on socket receive", e.what());
    }

    return (received);
}

// Closes down the socket.
void
ClientCommunicator::close() {
    try {
        socket_->close();
    } catch (boost::system::system_error& e) {
        throw Exception("Error on socket close", e.what());
    }

    return;
}
