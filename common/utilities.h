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
#include <string.h>

#include "udp_buffer.h"

/// \brief General Utility Functions
///
/// Holds general utility functions that don't naturally fit into any class.

class Utilities {
public:

    /// \brief Calculates CRC
    ///
    /// Calculates the CRC32 checksum of an array of data and appends the result
    /// (in network byte order) to the end of the data.
    /// \param buffer Data buffer. 
    /// \param size Amount of data in the buffer. 
    static uint32_t Crc(uint8_t* buffer, size_t size);

    /// \brief Prints endpoint information
    ///
    /// A debugging function, prints the IP address and port of the given
    /// endpoint.
    /// \param output Output stream on which to print
    /// \param endpoint Endpoint for which information is required.
    static void PrintEndpoint(std::ostream& output,
        boost::asio::ip::udp::endpoint endpoint);

    /// \brief Converts from byte array
    ///
    /// Copies a sequence of bytes into a larger data type.
    /// \param from Byte array to copy
    /// \param to Larger data type into which to copy array
    template <typename T> static void CopyBytes(const uint8_t* from, T& to) {
        memmove(static_cast<void*>(&to), static_cast<const void*>(from), sizeof(T));
    }

    /// \brief Converts to byte array
    ///
    /// Copies the contents of a large data type into a sequence of bytes
    /// \param from Data to copy,
    /// \param to Byte array into which to copy the data
    template <typename T> static void CopyBytes(const T& from, uint8_t* to) {
        memmove(static_cast<void*>(to), static_cast<const void*>(&from), sizeof(T));
    }
};

#endif // __UTILITIES_H
