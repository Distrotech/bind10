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

#ifndef FD_SHARE_H_
#define FD_SHARE_H_

/**
 * \file fd_share.h
 * \short Support to transfer file descriptors between processes.
 * \todo This interface is very C-ish. Should we have some kind of exceptions?
 */

#include <util/io/lib.h>

namespace isc {
namespace util {
namespace io {

ISC_LIBUTIL_IO_API const int FD_SYSTEM_ERROR = -2;
ISC_LIBUTIL_IO_API const int FD_OTHER_ERROR = -1;

/**
 * \short Receives a file descriptor.
 * This receives a file descriptor sent over an unix domain socket. This
 * is the counterpart of send_fd().
 *
 * \return FD_SYSTEM_ERROR when there's an error at the operating system
 * level (such as a system call failure).  The global 'errno' variable
 * indicates the specific error.  FD_OTHER_ERROR when there's a different
 * error.
 *
 * \param sock The unix domain socket to read from. Tested and it does
 *     not work with a pipe.
 */
ISC_LIBUTIL_IO_API
#ifdef _WIN32
SOCKET recv_fd(const SOCKET sock);
#else
int recv_fd(const int sock);
#endif

/**
 * \short Sends a file descriptor.
 * This sends a file descriptor over an unix domain socket. This is the
 * counterpart of recv_fd().
 *
 * \return FD_SYSTEM_ERROR when there's an error at the operating system
 * level (such as a system call failure).  The global 'errno' variable
 * indicates the specific error.
 * \param sock The unix domain socket to send to. Tested and it does not
 *     work with a pipe.
 * \param fd The file descriptor to send. It should work with any valid
 *     file descriptor.
 */
ISC_LIBUTIL_IO_API
#ifdef _WIN32
int send_fd(const SOCKET sock, const SOCKET fd);
#else
int send_fd(const int sock, const int fd);
#endif

} // End for namespace io
} // End for namespace util
} // End for namespace isc

#endif

// Local Variables:
// mode: c++
// End:
