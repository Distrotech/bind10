// Copyright (C) 2013  Internet Systems Consortium, Inc. ("ISC")
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

#ifndef L1CACHE_L1HASH_H
#define L1CACHE_L1HASH_H 1

#include <exceptions/exceptions.h>

#include <dns/dns_fwd.h>
#include <dns/rcode.h>
#include <dns/rrtype.h>
#include <dns/rrttl.h>

#include <ctime>
#include <list>

#include <stdint.h>

namespace isc {
namespace dnsl1cache {

class DNSL1HashError : public Exception {
public:
    DNSL1HashError(const char* file, size_t line, const char* what) :
        Exception(file, line, what) {}
};

class DNSL1HashEntry {
public:
    DNSL1HashEntry(const dns::RRType& rrtype, uint16_t ancount,
                   uint16_t nscount, uint16_t adcount, const dns::Rcode& rcode,
                   const uint16_t data_len, std::time_t ttl,
                   std::time_t last_used_time) :
        rrtype_(rrtype), ancount_(ancount), nscount_(nscount),
        adcount_(adcount), rcode_(rcode), data_len_(data_len), ttl_(ttl),
        last_used_time_(last_used_time)
    {}
    const dns::RRType rrtype_;
    const uint16_t ancount_;
    const uint16_t nscount_;
    const uint16_t adcount_;
    const dns::Rcode rcode_;
    const uint16_t data_len_;   // total size of RR data
    std::time_t ttl_;           // TTL of this entry
    std::time_t last_used_time_;

    void* getNameBuf() {
        return (this + 1);
    }
    uint16_t* getOffsetBuf(size_t namebuf_len) {
        const size_t padded_nbuf_len = (namebuf_len + 1) & ~1;
        return (static_cast<uint16_t*>(getNameBuf()) +
                (padded_nbuf_len >> 1));
    }
    void* getDataBuf(void* offsetp) {
        uint8_t* cp = static_cast<uint8_t*>(offsetp);
        return (cp + (ancount_ + nscount_ + adcount_) * sizeof(uint16_t) * 2);
    }
    // Variable length fields follow
    // owner name: serialized LabelSequence
    // (possible padding)
    // const uint16_t offsets_[]; encoded offset info for RRs
    //   1-bit: Begin of a new RRset
    //   1-bit: whether the type is rotatable
    //  14-bit: offset to the owner name
    //  16-bit: offset to the RR type field (following owner name)
    static const uint16_t FLAG_START_RRSET = 0x8000;
    static const uint16_t FLAG_ROTATABLE = 0x4000;
    static const uint16_t MASK_OFFSET = 0x3fff;
    // RR data
};

class DNSL1HashTable {
public:
    explicit DNSL1HashTable(const char* cache_file);
    DNSL1HashEntry* find(const dns::LabelSequence& labels,
                         const dns::RRType& type);

private:
    static const size_t N_BUCKETS = 10000;
    typedef std::list<DNSL1HashEntry*> EntryList;
    EntryList entry_buckets_[N_BUCKETS];
};

} // namespace dnsl1cache
} // namespace isc
#endif // L1CACHE_L1HASH_H

// Local Variables:
// mode: c++
// End:
