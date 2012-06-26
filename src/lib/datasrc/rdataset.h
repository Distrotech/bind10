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

#ifndef _RDATESET_H
#define _RDATESET_H 1

#include <dns/rrset.h>

#include <datasrc/memory_segment.h>
#include <datasrc/rdata_encoder.h>

#include <boost/interprocess/offset_ptr.hpp>

#include <stdint.h>

namespace isc {
namespace datasrc {
namespace internal {

struct RdataSet;
typedef boost::interprocess::offset_ptr<RdataSet> RdataSetPtr;
typedef boost::interprocess::offset_ptr<const RdataSet> ConstRdataSetPtr;

// Ugly, but okay
typedef boost::interprocess::offset_ptr<
    experimental::RBNode<datasrc::internal::RdataSet> > DomainNodePtr;
typedef boost::interprocess::offset_ptr<
    const experimental::RBNode<datasrc::internal::RdataSet> >
ConstDomainNodePtr;

struct RdataSet {
    RdataSet(dns::RRType type, dns::RRTTL ttl, uint16_t n_rdata,
             bool with_sig);
    RdataSetPtr next;
    const uint16_t type;
    const uint16_t is_signed : 1;
    const uint16_t many_rrs : 1;
    const uint16_t num_rrs : 14;
    const uint32_t ttl;

    static RdataSet* allocate(MemorySegment& segment,
                              RdataEncoder& encoder,
                              RdataEncoder::NamePtrCreator name_creator,
                              dns::ConstRRsetPtr rrset,
                              dns::ConstRRsetPtr sig_rrset);
    static void deallocate(MemorySegment& segment, RdataSetPtr rdataset);
    uint16_t getRdataCount() const {
        if (many_rrs == 1) {
            return (*reinterpret_cast<const uint16_t*>(this + 1));
        } else {
            return (num_rrs);
        }
    }
    DomainNodePtr* getNameBuf() {
        if (many_rrs == 1) {
            return (reinterpret_cast<DomainNodePtr*>(this + 1) + 1);
        } else {
            return (reinterpret_cast<DomainNodePtr*>(this + 1));
        }
    }
    const ConstDomainNodePtr* getNameBuf() const {
        if (many_rrs == 1) {
            return (reinterpret_cast<const ConstDomainNodePtr*>(
                        this + 1) + 1);
        } else {
            return (reinterpret_cast<const ConstDomainNodePtr*>(
                        this + 1));
        }
    }
    uint16_t* getLengthBuf() {
        if (many_rrs == 1) {
            return (reinterpret_cast<uint16_t*>(this + 1) + 1);
        } else {
            return (reinterpret_cast<uint16_t*>(this + 1));
        }
    }
    const uint16_t* getLengthBuf() const {
        if (many_rrs == 1) {
            return (reinterpret_cast<const uint16_t*>(this + 1) + 1);
        } else {
            return (reinterpret_cast<const uint16_t*>(this + 1));
        }
    }
};

} // internal
} // datasrc
} // isc

#endif  // _RDATESET_H

// Local Variables:
// mode: c++
// End:
