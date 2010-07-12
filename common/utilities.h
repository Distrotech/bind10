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

#ifndef __UTILITIES_H
#define __UTILITIES_H

#include <stdint.h>

#include "udp_buffer.h"

/// \brief General Utility Functions
///
/// Holds general utility functions that don't naturally fit into any class.

class Utilities {
public:

    /// \brief Calculates CRC and appends to end of buffer
    ///
    /// Calculates the CRC32 checksum of an array of data and appends the result
    /// (in network byte order) to the end of the data.
    /// \param buffer Data buffer.  It is assumed to be sized to (size+4)
    /// \param size Amount of data in the buffer.  The buffer is assumed to be
    /// four bytes longer to contain the CRC checksum.
    static void Crc(uint8_t* buffer, size_t size);

    /// \brief Append Endpoint
    ///
    /// Many of the programs require that both the data and the location to
    /// which the data should be sent to be transferred as a string of bytes.
    ///
    /// This method takes a UdpBuffer elements, extracts the UDP address and
    /// port number, and appends it (in network byte order) to the data.
    /// The address is assumed to be IPV4, so six bytes (address and port) are
    /// appended to the data and the data count adjusted accordingly.
    ///
    /// \exception Exception Thrown if there is not enough space to append the
    /// endpoint.
    static void AppendEndpoint(UdpBuffer& buffer);

    /// \brief Extract Endpoint
    ///
    /// Does the reverse of AppendEndpoint.  The last six bytes of the buffer
    /// are assumed to contain the IP address and port number of the endpoint
    /// (in network byte order).  The data is extracted and put into the
    /// endpoint structure in the buffer.
    ///
    /// \exception Exception Thrown if there is no enough data in the buffer.
    static void ExtractEndpoint(UdpBuffer& buffer);

    /// \brief Prints endpoint information
    ///
    /// A debugging function, prints the IP address and port of the given
    /// endpoint.
    /// \param output Output stream on which to print
    /// \param endpoint Endpoint for which information is required.
    static void PrintEndpoint(std::ostream& output,
        boost::asio::ip::udp::endpoint endpoint);
};

#endif // __UTILITIES_H
