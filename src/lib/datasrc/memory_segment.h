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

#ifndef _MEMORY_SEGMENT_H
#define _MEMORY_SEGMENT_H 1

namespace isc {
namespace datasrc {

class MemorySegment {
public:
    MemorySegment() : allocated_(0) {}
    void* allocate(size_t size) {
        void* p = malloc(size);
        assert(p != NULL);      // skip error handling for experiments
        allocated_ += size;
        return (p);
    }
    void deallocate(void* ptr, size_t size) {
        assert(allocated_ >= size);
        free(ptr);
        allocated_ -= size;
    }
    bool allMemoryDeallocated() const { return (allocated_ == 0); }
private:
    size_t allocated_;
};

}
}

#endif  // _MEMORY_SEGMENT_H

// Local Variables:
// mode: c++
// End:
