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

#include <arpa/inet.h>
#include <boost/asio.hpp>
#include <boost/crc.hpp>
#include <boost/foreach.hpp>

#include "exception.h"
#include "utilities.h"
#include "udp_buffer.h"

namespace ip = boost::asio::ip;

// Calculates CRC and appends to end of the buffer
void
Utilities::Crc(uint8_t* buffer, size_t size) {

    union {
        uint8_t  bytes[4];  // Byte representation of the CRC
        uint32_t integer;   // 32-bit word representation of the CRC
    } result;

    // Calculate the CRC
    boost::crc_32_type calculator;
    calculator.reset();
    calculator.process_bytes(buffer, size);
    result.integer = calculator.checksum();

    // Ensure result is in network byte order and copy to the buffer.
    result.integer = htonl(result.integer);

    for (int i = 0; i < 4; ++i) {
        buffer[size + i] = result.bytes[i];
    }

    return;
}

// Extracts UDP endpoint data and appends to the buffer.
void
Utilities::AppendEndpoint(UdpBuffer& buffer) {

    union {
        uint8_t     bytes[4];
        uint32_t    ulong;
    } address;
    union {
        uint8_t     bytes[2];
        uint16_t    ushort;
    } port;

    // Check we have enough space for the information.
    if ((buffer.capacity - buffer.size) < (4 + 2)) {
        throw Exception("Not enough space in UdpBuffer for endpoint information");
    }

    // Get the address asssociated with the endpoint and convert to network
    // byte order.
    ip::address ep_address = buffer.endpoint.address();
    if (! ep_address.is_v4()) {
        throw Exception("Endpoint address is not IP V4 address");
    }
    address.ulong = htonl(ep_address.to_v4().to_ulong());

    // Do the same for the port.
    port.ushort = htons(buffer.endpoint.port());

    // Copy to the end of the buffer.
    for (int i = 0; i < 4; ++i) {
        buffer.data[buffer.size + i] = address.bytes[i];
    }

    for (int i = 0; i < 2; ++i) {
        buffer.data[buffer.size + 4 + i] = port.bytes[i];
    }

    // Update the buffer size
    buffer.size += 6;

    return;
}

// Extracts UDP endpoint data and puts in the endpoint structure
void
Utilities::ExtractEndpoint(UdpBuffer& buffer) {

    union {
        uint8_t     bytes[4];
        uint32_t    ulong;
    } address;
    union {
        uint8_t     bytes[2];
        uint16_t    ushort;
    } port;

    // Check there is enough information
    if (buffer.size < (4 + 2)) {
        throw Exception("Not enough data in UdpBuffer for endpoint information");
    }

    // Extract the address information and convert to host byte order.
    for (int i = 0; i < 4; ++i) {
        address.bytes[i] = buffer.data[buffer.size - 6 + i];
    }
    address.ulong = ntohl(address.ulong);

    // ... and the port
    for (int i = 0; i < 2; ++i) {
        port.bytes[i] = buffer.data[buffer.size - 2 + i];
    }
    port.ushort = ntohs(port.ushort);

    // Update the buffer size to reflect less information.
    buffer.size -= 6;

    // Construct the endpoint with the information given and put in the
    // UdpBuffer object.
    buffer.endpoint = ip::udp::endpoint(ip::address_v4(address.ulong),
        port.ushort);

    return;
}

// Prints endpoint data to the specified stream
void
Utilities::PrintEndpoint(std::ostream& output,
    boost::asio::ip::udp::endpoint endpoint)
{
    output << "Address = "
        << endpoint.address().to_v4().to_string()
        << ", port = "
        << endpoint.port()
        << "\n";
}

