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

#ifndef __UDP_BUFFER_H
#define __UDP_BUFFER_H

#include <boost/asio.hpp>
#include <boost/shared_array.hpp>

/// \brief UDP Buffer Class
///
/// Holds information about data received from/sent to a UDP socket.
struct UdpBuffer {
public:
    boost::asio::ip::udp::endpoint  endpoint;   //< Socket end point
    size_t                          size;       //< Amount of data in buffer
    size_t                          capacity;   //< Maximum size of buffer
    boost::shared_array<uint8_t>    data;       //< Data in the buffer

    UdpBuffer(boost::asio::ip::udp::endpoint ep, size_t amount,
        size_t max_amount, boost::shared_array<uint8_t> contents) :
        endpoint(ep), size(amount), capacity(max_amount), data(contents)
    {}
};

#endif // __UDP_BUFFER_H
