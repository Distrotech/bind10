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

#include <boost/asio.hpp>
#include "udp_buffer.h"

namespace ip = boost::asio::ip;

// Sets the endpoint information in the buffer
void
UdpBuffer::setAddressInfo(const boost::asio::ip::udp::endpoint& ep) {

    // Get the address and port associated with the endpoint.
    ip::address ep_address = ep.address();
    if (! ep_address.is_v4()) {
        throw Exception("Endpoint address is not IP V4 address");
    }
    uint32_t address = ep_address.to_v4().to_ulong();
    setAddress(address);

    // Do the same for the port.
    uint16_t port = ep.port();
    setPort(port);

    return;
}

// Extracts UDP endpoint information into an endpoint object

ip::udp::endpoint UdpBuffer::getAddressInfo() const {

    // Extract the address and port information.
    uint32_t address = getAddress();
    uint16_t port = getPort();

    // Return an endpoint.
    return ip::udp::endpoint(ip::address_v4(address), port);
}

// Clone - create a new UDP buffer with nothing shared

UdpBuffer UdpBuffer::clone() const {

    // Duplicate current object.  At this point the data buffer is shared.
    UdpBuffer result = *this;

    // Duplicate the data buffer and copy the source into it.
    result.data_ = boost::shared_array<uint8_t>(new uint8_t[capacity()]);
    memmove(static_cast<void*>(result.data_.get()),
        static_cast<const void*>(data_.get()), capacity());

    return (result);
}
