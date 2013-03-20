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

/// From boost/noncopyable.hpp

#ifndef UTIL_NONASSIGNABLE
#define UTIL_NONASSIGNABLE

namespace isc {
namespace util {
namespace nonassignable_ {

class nonassignable {
protected:
    nonassignable() {}
    ~nonassignable() {}
private:
    // emphasize the following member is private
    const nonassignable& operator=(const nonassignable&);
};

} // namespace nonassignable_

typedef nonassignable_::nonassignable nonassignable;

} // namespace util
} // namespace isc

#endif // UTIL_NONASSIGNABLE
