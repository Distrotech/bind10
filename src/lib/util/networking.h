// Copyright (C) 2012  Internet Systems Consortium, Inc. ("ISC")
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

#ifndef __NETWORKING_H
#define __NETWORKING_H

#include <cstring>
#include <cerrno>
#include <asio/detail/socket_types.hpp>

namespace isc {
namespace util {

/// \brief Networking code portability inlines

inline int closesocket(asio::detail::socket_type sd) {
#ifdef _WIN32
    return (::closesocket(sd));
#else
    return (close(sd));
#endif
}

inline int getneterror(void) {
#ifdef _WIN32
    return (WSAGetLastError());
#else
    return (errno);
#endif
}

inline void setneterror(int errnum) {
#ifdef _WIN32
    WSASetLastError(errnum);
#else
    errno = errnum;
#endif
}

inline char *strneterror(void) {
#ifdef _WIN32
#define ERRBUF 128
    static char buf[ERRBUF];
    strerror_s<ERRBUF>(buf, getneterror());
    return (buf);
#else
    return (::strerror(getneterror()));
#endif
}

inline const char *inetntop(int af, const void *src, char *dst, size_t size) {
#ifdef _WIN32
    return (inet_ntop(af, (void *)src, dst, size));
#else
    return (inet_ntop(af, src, dst, size));
#endif
}

} // namespace util
} // namespace isc

#endif // __NETWORKING_H
