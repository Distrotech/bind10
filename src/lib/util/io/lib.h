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

#ifndef __LIBUTIL_IO_H
#define __LIBUTIL_IO_H 1

#if !defined(_WIN32) || defined(USE_STATIC_LINK)
#define ISC_LIBUTIL_IO_API
#else
#ifdef ISC_LIBUTIL_IO_EXPORT
#define ISC_LIBUTIL_IO_API __declspec(dllexport)
#else
#defineISC_LIBUTIL_IO_API __declspec(dllimport)
#endif
#endif

#endif // __LIBUTIL_IO_H

// Local Variables: 
// mode: c++
// End: 
