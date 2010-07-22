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

#include <boost/system/error_code.hpp>
#include <boost/system/system_error.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/asio.hpp>

namespace ip = boost::asio::ip;

#include "debug.h"
#include "debug_flags.h"
#include "defaults.h"
#include "exception.h"
#include "udp_buffer.h"
#include "udp_communicator.h"

// Opens the connection to the network, in this case constructing the socket
// and associating it with the given port.

void
UdpCommunicator::open() {

    if (port_ != 0) {
        // Declare a V4 end point.
        ip::udp::endpoint ep(ip::udp::v4(), port_);

        // Open a UDP socket on the specified port.
        socket_ptr_ = boost::shared_ptr<ip::udp::socket>(
            new ip::udp::socket(io_service_, ep));
    }
    else {

        // Just open a V4 socket.  Only the send function can be
        // used.
        socket_ptr_ = boost::shared_ptr<ip::udp::socket>(
            new ip::udp::socket(io_service_, ip::udp::v4()));
    }

    boost::asio::socket_base::send_buffer_size snd_option(SCKT_MAX_SND_SIZE);
    socket_ptr_->set_option(snd_option);

    boost::asio::socket_base::receive_buffer_size rcv_option(SCKT_MAX_RCV_SIZE);
    socket_ptr_->set_option(rcv_option);


    return;
}

// Send data to the target (which is assumed to be listening).
void
UdpCommunicator::send(UdpBuffer& buffer) {
    try {
        if (Debug::flagSet(DebugFlags::PRINT_IP)) {
            std::cout << "Sending to IP address "
                << std::hex << buffer.getAddress()
                << ", port " << std::hex << buffer.getPort()
                << std::endl;
        }

        // Send the data.
        size_t sent = socket_ptr_->send_to(
            boost::asio::buffer(buffer.data(), buffer.size()),
            buffer.getAddressInfo());
        if (sent != buffer.size()) {
            throw Exception("Not all data passed to send() was sent");
        }
    } catch (boost::system::system_error& e) {
        throw Exception("Error on socket send", e.what());
    }

    return;
}

// Receive data from the target.
UdpBuffer
UdpCommunicator::receive() {

    // Alert if the socket hasn't been bound to a port.
    if (port_ == 0) {
        throw Exception("Attempt to read from unbound socket");
    }

    UdpBuffer buffer;                       // Buffer to receive data

    try {
        ip::udp::endpoint sender_endpoint;  // Where the packet came from

        // Receive the data and encode where the data came from into the
        // data packet.
        size_t received = socket_ptr_->receive_from(
            boost::asio::buffer(buffer.data(), buffer.capacity()),
            sender_endpoint);
        buffer.setSize(received);
        buffer.setAddressInfo(sender_endpoint);

        if (Debug::flagSet(DebugFlags::PRINT_IP)) {
            std::cout << "Received from IP address "
                << std::hex << buffer.getAddress()
                << ", port " << std::hex << buffer.getPort()
                << std::endl;
        }
    } catch (boost::system::system_error& e) {
        throw Exception("Error on socket receive", e.what());
    }
    return (buffer);
}

// Closes down the socket
void
UdpCommunicator::close() {
    try {
        socket_ptr_->close();
    } catch (boost::system::system_error& e) {
        throw Exception("Error on socket close", e.what());
    }

    return;
}

