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

#ifndef __COMPRESS_TABLE_H
#define __COMPRESS_TABLE_H

#include <functional>
#include <vector>

#include <stdint.h>

namespace isc {
namespace datasrc {
namespace experimental {
namespace internal {


class CompressOffsetTable {
private:
    // our keys have clear bias, so using a prime number should make sense
    static const size_t HASH_SIZE = 1009;
    static const size_t BUCKET_MIN_SIZE = 4;

    struct CompressOffset {
        CompressOffset(const void* k, const uint16_t o) :
            key(k), offset(o) {}
        const void* key;
        uint16_t offset;
    };
    typedef std::vector<CompressOffset> CompressOffsets;

    struct MatchIndex : public std::unary_function<unsigned long, bool> {
        MatchIndex(const void* key_address) : key_address_(key_address) {}
        bool operator()(const CompressOffset& entry) const {
            return (entry.key == key_address_);
        }
        const void* const key_address_;
    };

public:
    static const uint16_t OFFSET_NOTFOUND = 0xffff;
    CompressOffsetTable() {
        for (int i = 0; i < HASH_SIZE; ++i) {
            buckets_[i].reserve(BUCKET_MIN_SIZE);
            used_buckets_[i] = 0; // value doesn't matter
        }
        nused_buckets_ = 0;
    }
    ~CompressOffsetTable() {
        for (int i = 0; i < HASH_SIZE; ++i) {
            buckets_[i].clear();
        }
    }
    uint16_t find(const void* key_address) const {
        // Bad cast, but for experiment it should be a good compromise
        const size_t hash =
            (reinterpret_cast<unsigned long>(key_address) % HASH_SIZE);
        CompressOffsets::const_iterator found =
            find_if(buckets_[hash].begin(), buckets_[hash].end(),
                    MatchIndex(key_address));
        if (found != buckets_[hash].end()) {
            return ((*found).offset);
        }
        return (OFFSET_NOTFOUND);
    }
    void insert(const void* key_address, const uint16_t offset) {
        const size_t hash =
            (reinterpret_cast<unsigned long>(key_address) % HASH_SIZE);
        const bool was_empty = buckets_[hash].empty();
        buckets_[hash].push_back(CompressOffset(key_address, offset));
        if (was_empty) {
            assert(nused_buckets_ < HASH_SIZE);
            used_buckets_[nused_buckets_++] = hash;
        }
    }
    void clear() {
        for (int i = 0; i < nused_buckets_; ++i) {
            buckets_[used_buckets_[i]].clear();
        }
        nused_buckets_ = 0;
    }
private:
    CompressOffsets buckets_[HASH_SIZE];
    int used_buckets_[HASH_SIZE];
    int nused_buckets_;
};

}   // namespace internal
}   // namespace experimental
}   // namespace datasrc
}   // namespace isc

#endif  // __COMPRESS_TABLE_H

// Local Variables:
// mode: c++
// End:
