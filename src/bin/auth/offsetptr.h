/*
 * Copyright (C) 2010  Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id$ */

#ifndef __OFFSETPTR_H
#define __OFFSETPTR_H 1

#include <inttypes.h>

#include <cassert>

template <typename T>
class OffsetPtr {
private:
    static const intptr_t NULL_OFFSET = 0;
public:
    // XXX: we heavily use reinterpret_cast in this class.
    // We know it's a very bad practice, but this is one of the rare cases we
    // have to use the necessary evil.
    OffsetPtr() : offset_(NULL_OFFSET) {}
    OffsetPtr(T* ptr, void* const base) :
        offset_(reinterpret_cast<intptr_t>(ptr) -
                reinterpret_cast<intptr_t>(base))
    {
        assert(offset_ != NULL_OFFSET);   // should better be an exception.
    }
    T* getPtr(void* const base) const {
        if (offset_ == NULL_OFFSET) {
            return (NULL);
        }
        return (reinterpret_cast<T*>(reinterpret_cast<intptr_t>(base) +
                                     offset_));
    }
    const T* getConstPtr(const void* const base) const {
        if (offset_ == NULL_OFFSET) {
            return (NULL);
        }
        return (reinterpret_cast<const T*>(reinterpret_cast<intptr_t>(base) +
                                           offset_));
    }
    bool isNull() const {
        return (offset_ == NULL_OFFSET);
    }
    // The following comparison operators assume the base pointer is the same.
    // It's the caller's responsibility to ensure that.
    bool operator==(const OffsetPtr<T>& other) const {
        return (offset_ == other.offset_);
    }
    bool operator!=(const OffsetPtr<T>& other) const {
        return (offset_ != other.offset_);
    }
private:
    intptr_t offset_;
};
#endif // __OFFSETPTR_H

// Local Variables:
// mode: c++
// End:
