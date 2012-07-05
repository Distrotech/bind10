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

#ifndef __SOCKADDR_UTIL_H_
#define __SOCKADDR_UTIL_H_ 1

#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#endif

#include <cassert>

#include <util/io/lib.h>

// These definitions in this file are for the convenience of internal
// implementation and test code, and are not intended to be used publicly.
// The namespace "internal" indicates the intent.

namespace isc {
namespace util {
namespace io {
namespace internal {

inline socklen_t
getSALength(const struct sockaddr& sa) {
    if (sa.sa_family == AF_INET) {
        return (sizeof(struct sockaddr_in));
    } else {
        assert(sa.sa_family == AF_INET6);
        return (sizeof(struct sockaddr_in6));
    }
}

// Lower level C-APIs require conversion between various variants of
// sockaddr's, which is not friendly with C++.  The following templates
// are a shortcut of common workaround conversion in such cases.

template <typename DSAType, typename SSAType>
const DSAType*
convertSockAddr(const SSAType* sa) {
    const void* p = sa;
    return (static_cast<const DSAType*>(p));
}

template <typename DSAType, typename SSAType>
DSAType*
convertSockAddr(SSAType* sa) {
    void* p = sa;
    return (static_cast<DSAType*>(p));
}

}
}
}
}

#endif  // __SOCKADDR_UTIL_H_

// Local Variables:
// mode: c++
// End:
