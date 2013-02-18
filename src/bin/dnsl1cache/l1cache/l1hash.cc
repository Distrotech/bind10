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

#include <config.h>

#include <dnsl1cache/l1cache/l1hash.h>
#include <dnsl1cache/logger.h>

#include <log/logger_support.h>

#include <dns/name.h>
#include <dns/labelsequence.h>
#include <dns/rrclass.h>
#include <dns/rrtype.h>
#include <dns/rcode.h>
#include <dns/rrset.h>
#include <dns/messagerenderer.h>
#include <dns/rrcollator.h>
#include <dns/master_loader.h>
#include <dns/master_loader_callbacks.h>

#include <boost/functional/hash.hpp>
#include <boost/bind.hpp>

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <sstream>

using namespace isc::dns;

namespace isc {
namespace dnsl1cache {

inline
size_t
getQueryHash(const LabelSequence& labels, const RRType& rrtype) {
    size_t hash_val = labels.getHash(false);
    boost::hash_combine(hash_val, rrtype.getCode());
    return (hash_val);
}

void
loadWarn(const std::string, size_t, const std::string&)
{}

void
loadError(const std::string&, size_t, const std::string& reason) {
    isc_throw(DNSL1HashError, "Error in RRset: " << reason);
}

namespace {
class CacheDataCreater {
public:
    CacheDataCreater() {}
    void start(const Name& qname, const RRType& qtype, const RRClass& qclass,
               size_t ans_count, size_t soa_count)
    {
        renderer_.clear();

        static uint8_t dummy_header[12]; // placeholder for DNS header
        renderer_.writeData(dummy_header, sizeof(dummy_header));
        renderer_.writeName(qname);
        qtype.toWire(renderer_);
        qclass.toWire(renderer_);

        offset0_ = renderer_.getLength();
        ans_count_ = ans_count;
        soa_count_ = soa_count;
        offsets_.clear();
    }
    void end() {                // consistency check
        if (ans_count_ != 0 || soa_count_ != 0) {
            isc_throw(DNSL1HashError, "broken cache data");
        }
    }
    void addRRset(const RRsetPtr& rrset) {
        if (ans_count_ > 0) {
            assert(rrset->getRdataCount() <= ans_count_);
        } else if (soa_count_ > 0) {
            assert(soa_count_ == 1 && rrset->getRdataCount() == soa_count_);
        } else {
            assert(false);
        }
        for (RdataIteratorPtr rditer = rrset->getRdataIterator();
             !rditer->isLast();
             rditer->next())
        {
            offsets_.push_back(renderer_.getLength() - offset0_);
            renderer_.writeName(rrset->getName());

            offsets_.push_back(renderer_.getLength() - offset0_);
            rrset->getType().toWire(renderer_);
            rrset->getClass().toWire(renderer_);
            rrset->getTTL().toWire(renderer_);
            const size_t pos = renderer_.getLength();
            renderer_.skip(sizeof(uint16_t));
            rditer->getCurrent().toWire(renderer_);
            renderer_.writeUint16At(renderer_.getLength() - pos, pos);

            if (ans_count_ > 0) {
                --ans_count_;
            } else if (soa_count_ > 0) {
                --soa_count_;
            }
        }
    }
    MessageRenderer renderer_;
    std::vector<uint16_t> offsets_;
    size_t offset0_; // offset to the pointer immediately after question
private:
    size_t ans_count_;
    size_t soa_count_;
};
}

DNSL1HashTable::DNSL1HashTable(const char* cache_file) {
    std::ifstream ifs(cache_file);
    if (!ifs.good()) {
        isc_throw(DNSL1HashError, "failed to open cache file");
    }

    CacheDataCreater creator;
    RRCollator collator(boost::bind(&CacheDataCreater::addRRset, &creator,
                                    _1));
    MasterLoader loader(ifs, Name::ROOT_NAME(), RRClass::IN(),
                        MasterLoaderCallbacks(loadError, loadWarn),
                        collator.getCallback());

    std::string line, qname_str, qclass_str, qtype_str;
    size_t rcode_code, ans_count, soa_count;
    size_t entry_count = 0;
    while (ifs.good()) {
        line.clear();
        std::getline(ifs, line);
        if (ifs.eof()) {
            break;
        } else if (!ifs.good()) {
            isc_throw(DNSL1HashError, "Read error for a cache entry");
        }

        std::stringstream ss(line);
        ss >> qname_str >>  qclass_str >> qtype_str >> rcode_code
           >> ans_count >> soa_count;
        if (!ifs.good()) {
            isc_throw(DNSL1HashError, "Read error for a cache entry params");
        }
        const Name qname(qname_str);
        const RRClass qclass(qclass_str);
        if (qclass != RRClass::IN()) { // for simplicity
            isc_throw(NotImplemented, "Unsupported RR class for cache: " <<
                      qclass);
        }
        const RRType qtype(qtype_str);
        const Rcode rcode(rcode_code);

        creator.start(qname, qtype, qclass, ans_count, soa_count);
        if (ans_count > 0) {
            loader.loadIncremental(ans_count);
        }
        if (soa_count > 0) {
            loader.loadIncremental(soa_count);
        }
        collator.flush();
        creator.end();

        const LabelSequence labels(qname);
        const size_t name_buflen =
            (labels.getSerializedLength() + 1) & ~1; // include padding
        const size_t data_len =
            creator.renderer_.getLength() - creator.offset0_;
        const size_t entry_len = sizeof(DNSL1HashEntry) + name_buflen +
            sizeof(uint16_t) * creator.offsets_.size() + data_len;

        void* p = std::malloc(entry_len);
        DNSL1HashEntry* entry =
            new(p) DNSL1HashEntry(qtype, ans_count, soa_count, 0, rcode,
                                  data_len,
                                  86400,   // dummy TTL for experiment
                                  std::time(NULL));
        entry_buckets_[getQueryHash(labels, qtype) % N_BUCKETS].
            push_back(entry);
        labels.serialize(entry->getNameBuf(), name_buflen);
        void* offsetp = entry->getOffsetBuf(name_buflen);
        std::memcpy(offsetp, &creator.offsets_[0],
                    creator.offsets_.size() * sizeof(uint16_t));
        const uint8_t* dp = static_cast<const uint8_t*>(
            creator.renderer_.getData());
        std::memcpy(entry->getDataBuf(offsetp), dp + creator.offset0_,
                    data_len);
        ++entry_count;
    }

    LOG_INFO(logger, DNSL1CACHE_CACHE_TABLE_CREATED).arg(entry_count);
}

}
}
