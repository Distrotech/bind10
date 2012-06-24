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

#include <dns/rrtype.h>
#include <dns/rrclass.h>
#include <dns/rdata.h>
#include <datasrc/rdata_encoder.h>

#include <gtest/gtest.h>

using namespace std;

using namespace isc::dns;
using namespace isc::dns::rdata;
using namespace isc::datasrc;

namespace {
class RdataEncoderTest : public::testing::Test {
protected:
    RdataEncoderTest() :
        a_rdata_(createRdata(RRType::A(), RRClass::IN(), "192.0.2.1")),
        a_rdata2_(createRdata(RRType::A(), RRClass::IN(), "192.0.2.53")),
        ns_rdata_(createRdata(RRType::NS(), RRClass::IN(), "ns.example.com")),
        soa_rdata_(createRdata(RRType::SOA(), RRClass::IN(),
                               "ns.example.org. root.example.org. 0 0 0 0 0")),
        txt_rdata_(createRdata(RRType::TXT(), RRClass::IN(), "0123456789")),
        txt_rdata2_(createRdata(RRType::TXT(), RRClass::IN(), "01234"))
    {}
    internal::RdataEncoder encoder_;
    ConstRdataPtr a_rdata_;
    ConstRdataPtr a_rdata2_;
    ConstRdataPtr ns_rdata_;
    ConstRdataPtr soa_rdata_;
    ConstRdataPtr txt_rdata_;
    ConstRdataPtr txt_rdata2_;
};

TEST_F(RdataEncoderTest, basicTests) {
    encoder_.addRdata(*a_rdata_);
    encoder_.construct(RRType::A());
    EXPECT_EQ(4, encoder_.getStorageLength());

    encoder_.clear();
    encoder_.addRdata(*a_rdata_);
    encoder_.addRdata(*a_rdata2_);
    encoder_.construct(RRType::A());
    EXPECT_EQ(8, encoder_.getStorageLength());

    encoder_.clear();
    encoder_.addRdata(*ns_rdata_);
    encoder_.construct(RRType::NS());
    // namelen = 16, offsetlen = 4
    EXPECT_EQ((16 + 4 + 2), encoder_.getStorageLength());

    encoder_.clear();
    encoder_.addRdata(*soa_rdata_);
    encoder_.construct(RRType::SOA());
    // 1st name data: 16+4+2bytes, 2nd: 18+4+2bytes, rest 20
    EXPECT_EQ((16 + 4 + 2) + (18 + 4 + 2) + 20, encoder_.getStorageLength());

    encoder_.clear();
    encoder_.addRdata(*txt_rdata_);
    encoder_.addRdata(*txt_rdata2_);
    encoder_.construct(RRType::TXT());
    // 2 varlen fields, 1st text data: 11bytes, 2nd: 6bytes
    EXPECT_EQ((2 * 2) + 11 + 6, encoder_.getStorageLength());
}
} // end of unnamed namespace
