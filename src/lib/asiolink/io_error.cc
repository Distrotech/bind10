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

#define ISC_LIBASIOLINK_EXPORT

#include <config.h>

#include <asio.hpp>

#include <asiolink/io_asio_socket.h>
#include <asiolink/tcp_socket.h>

// Instantiate classes

class ISC_LIBASIOLINK_API SocketNotOpen;
class ISC_LIBASIOLINK_API SocketSetError;
class ISC_LIBASIOLINK_API BufferOverflow;
class ISC_LIBASIOLINK_API BufferTooLarge;

// Local Variables:
// mode: c++
// End:
