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

#ifndef __NONCOPYABLE_
#define __NONCOPYABLE_

#include <util/dll.h>

namespace isc {
namespace util {
namespace noncopyable_ {

class B10_LIBUTIL_API noncopyable {
protected:
    noncopyable() {}
    ~noncopyable() {}
private:
    // emphasize the following members are private
    noncopyable(const noncopyable&);
    const noncopyable& operator=(const noncopyable&);
};

} // namespace noncopyable_

typedef noncopyable_::noncopyable noncopyable;

} // namespace util
} // namespace isc

#endif // __NONCOPYABLE_
