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

#include <algorithm>
#include <iostream>

#include <arpa/inet.h>
#include <boost/asio.hpp>
#include <boost/crc.hpp>

#include "exception.h"
#include "utilities.h"
#include "udp_buffer.h"

namespace ip = boost::asio::ip;

// Calculates CRC and appends to end of the buffer
uint32_t
Utilities::Crc(uint8_t* buffer, size_t size) {

    uint32_t crc;           // CRC check

    // Calculate the CRC
    boost::crc_32_type calculator;
    calculator.reset();
    calculator.process_bytes(buffer, size);
    crc = calculator.checksum();

    return crc;
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

/*
// Conversion between a larger data type and a byte array

template<typename T> void
Utilities::Convert(T from, uint8_t* to) {
    union {
        T           word;
        uint8_t     byte[sizeof(T)];
    } cvt;

    cvt.word = from;
    std::copy(cvt.byte, cvt.byte + sizeof(T), to);

    return;
}

// ... and instantiate for uint16_t and uint32_t.

template void Utilities::Convert(const uint32_t from, uint8_t* to);
template void Utilities::Convert(const uint16_t from, uint8_t* to);


// Conversion between a byte array and a larger type.

template<typename T> void
Utilities::Convert(const uint8_t* from, T& to) {
    union {
        T           word;
        uint8_t     byte[sizeof(T)];
    } cvt;

    std::copy(from, from + sizeof(T), cvt.byte);
    to = cvt.word;

    return;
}

// ... and instantiate for uint16_t and uint32_t.

template void Utilities::Convert(const uint8_t* from, uint32_t& to);
template void Utilities::Convert(const uint8_t* from, uint16_t& to);
*/
