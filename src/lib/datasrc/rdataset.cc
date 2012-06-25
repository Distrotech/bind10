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

#include <dns/rrset.h>
#include <dns/rrtype.h>
#include <dns/rrttl.h>
#include <dns/rdata.h>

#include <datasrc/rdataset.h>
#include <datasrc/memory_segment.h>
#include <datasrc/rdata_encoder.h>

#include <boost/interprocess/offset_ptr.hpp>

#include <arpa/inet.h>

using namespace isc::dns;
using namespace isc::dns::rdata;

namespace isc {
namespace datasrc {
namespace internal {

RdataSet::RdataSet(dns::RRType type, dns::RRTTL ttl, uint16_t n_rdata,
                   bool with_sig) :
    next(NULL), type(type.getCode()), is_signed(with_sig ? 1 : 0),
    many_rrs(n_rdata > 16383 ? 1 : 0), num_rrs(n_rdata),
    ttl(htonl(ttl.getValue()))
{}

RdataSet*
RdataSet::allocate(MemorySegment& segment, RdataEncoder& encoder,
                   ConstRRsetPtr rrset, ConstRRsetPtr sig_rrset)
{
    assert(!sig_rrset);         // for now not support sigs

    const RRType type = rrset->getType();
    encoder.clear();

    size_t n_rdata = 0;
    for (RdataIteratorPtr it = rrset->getRdataIterator();
         !it->isLast();
         it->next())
    {
        encoder.addRdata(it->getCurrent());
        ++n_rdata;
    }
    assert(n_rdata <= 0xffff);
    encoder.construct(type);

    const size_t data_size = encoder.getStorageLength() +
        ((n_rdata > 16383) ? 2 : 0); 
    void* region = segment.allocate(sizeof(RdataSet) + data_size);
    RdataSet* rdset = new(region) RdataSet(type, rrset->getTTL(), n_rdata,
                                           false);
    uint16_t* lengths = rdset->getLengthBuf();
    const size_t n_lens = encoder.encodeLengths(lengths,
                                                data_size / sizeof(uint16_t));
    uint8_t* data_buf = reinterpret_cast<uint8_t*>(lengths + n_lens);
    const size_t dlen = encoder.encodeData(data_buf, data_size -
                                           (n_lens * sizeof(uint16_t)));
    assert(n_lens * sizeof(uint16_t) + dlen == data_size);

    return (rdset);
}

void
RdataSet::deallocate(MemorySegment& segment, RdataSetPtr rdsetptr) {
    RdataSet* rdset = rdsetptr.get();
    segment.deallocate(rdset, sizeof(RdataSet) +
                       getEncodedDataSize(
                           RRType(rdset->type), rdset->getRdataCount(),
                           reinterpret_cast<const uint8_t*>(rdset + 1)));
}

} // internal
} // datasrc
} // isc
