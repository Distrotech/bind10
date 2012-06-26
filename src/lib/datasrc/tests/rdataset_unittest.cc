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
#include <dns/rdata.h>

#include <datasrc/memory_segment.h>
#include <datasrc/rdataset.h>

#include <testutils/dnsmessage_test.h>

#include <gtest/gtest.h>

#include <boost/interprocess/offset_ptr.hpp>

using namespace std;

using namespace isc::testutils;
using namespace isc::dns;
using namespace isc::dns::rdata;
using namespace isc::datasrc;
using namespace isc::datasrc::internal;

namespace {
class RdataSetTest : public::testing::Test {
protected:
    RdataSetTest() :
        rrset_soa_(textToRRset(". 86400 IN SOA "
                               "ns.example.org. admin.example.org. "
                               "2012052901 7200 3600 2592000 1200")),
        rrset_txt_(textToRRset("example.org. 86400 IN TXT \"abcdefg\""))
    {}
    RdataEncoder encoder;
    ConstRRsetPtr rrset_soa_;
    ConstRRsetPtr rrset_txt_;
};

DomainNodePtr
dummyPtrCreator(const Name*) {
    return (NULL);
}

TEST_F(RdataSetTest, allocate) {
    MemorySegment segment;
    RdataSetPtr rdsetptr1(RdataSet::allocate(segment, encoder, dummyPtrCreator,
                                             rrset_soa_, ConstRRsetPtr()));
    RdataSetPtr rdsetptr2(RdataSet::allocate(segment, encoder, dummyPtrCreator,
                                             rrset_txt_, ConstRRsetPtr()));
    RdataSet::deallocate(segment, rdsetptr1);
    RdataSet::deallocate(segment, rdsetptr2);
    EXPECT_TRUE(segment.allMemoryDeallocated());
}

} // end of unnamed namespace
