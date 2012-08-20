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

#ifndef __LIBTESTUTILS_H
#define __LIBTESTUTILS_H 1

#if !defined(_WIN32) || defined(USE_STATIC_LINK)
#define B10_LIBTESTUTILS_API
#else
#ifdef B10_LIBTESTUTILS_EXPORT
#define B10_LIBTESTUTILS_API __declspec(dllexport)
#else
#define B10_LIBTESTUTILS_API __declspec(dllimport)
#endif
#endif

#endif // __LIBTESTUTILS_H

// Local Variables: 
// mode: c++
// End: 
