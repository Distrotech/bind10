// Copyright (C) 2011  Internet Systems Consortium, Inc. ("ISC")
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

#ifndef __IOSERVICE_COUNTER_H
#define __IOSERVICE_COUNTER_H 1

#include <string>

namespace isc {
namespace asiolink {

// enum for IOService Counters
enum IOServiceCounterType {
    // IOService Counters
    IOSERVICE_FD_ADD_TCP,         ///< adding a new TCP server
    IOSERVICE_FD_ADD_UDP,         ///< adding a new UDP server
    IOSERVICE_FETCH_COMPLETED,    ///< upstream fetch has now completed
    IOSERVICE_FETCH_STOPPED,      ///< upstream fetch has been stopped
    IOSERVICE_SOCKET_OPEN_ERROR,  ///< error opening socket
    IOSERVICE_READ_DATA_ERROR,    ///< error reading data
    IOSERVICE_READ_TIMEOUT,       ///< receive timeout
    IOSERVICE_SEND_DATA_ERROR,    ///< error sending data
    IOSERVICE_UNKNOWN_ORIGIN,     ///< unknown origin for ASIO error
    IOSERVICE_UNKNOWN_RESULT,     ///< unknown result
    IOSERVICE_IPV4_TCP_ACCEPT,    ///< number of IPv4 TCP accept
    IOSERVICE_IPV6_TCP_ACCEPT,    ///< number of IPv6 TCP accept
    IOSERVICE_IPV4_TCP_ACCEPTFAIL, ///< number of IPv4 TCP acceptfail
    IOSERVICE_IPV6_TCP_ACCEPTFAIL, ///< number of IPv6 TCP acceptfail
    IOSERVICE_TCP_OPEN,           /// number of TCP OPEN
    IOSERVICE_COUNTER_TYPES       ///< The number of defined counters
};

/// This file defines a set of statistics items in Auth module for internal
/// use. This file is intended to be included in statistics.cc.

}   // namespace asiolink
}   // namespace isc

#endif // __IOSERVICE_COUNTER_H

// Local Variables:
// mode: c++
// End:
