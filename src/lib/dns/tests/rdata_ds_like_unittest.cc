// Copyright (C) 2010  Internet Systems Consortium, Inc. ("ISC")
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

#include <algorithm>
#include <string>

#include <util/buffer.h>
#include <dns/messagerenderer.h>
#include <dns/rdata.h>
#include <dns/rdataclass.h>
#include <dns/rrclass.h>
#include <dns/rrtype.h>

#include <gtest/gtest.h>

#include <dns/tests/unittest_util.h>
#include <dns/tests/rdata_unittest.h>

using isc::UnitTestUtil;
using namespace std;
using namespace isc::dns;
using namespace isc::util;
using namespace isc::dns::rdata;

namespace {
// hacks to make templates work
template <class T>
class RRTYPE : public RRType {
public:
    RRTYPE();
};

template<> RRTYPE<generic::DS>::RRTYPE() : RRType(RRType::DS()) {}
template<> RRTYPE<generic::DLV>::RRTYPE() : RRType(RRType::DLV()) {}

template <class DS_LIKE>
class Rdata_DS_LIKE_Test : public RdataTest {
protected:
    static DS_LIKE const rdata_ds_like;
};

string ds_like_txt("12892 5 2 F1E184C0E1D615D20EB3C223ACED3B03C773DD952D"
                   "5F0EB5C777586DE18DA6B5");

template <class DS_LIKE>
DS_LIKE const Rdata_DS_LIKE_Test<DS_LIKE>::rdata_ds_like(ds_like_txt);

// The list of types we want to test.
typedef testing::Types<generic::DS, generic::DLV> Implementations;

TYPED_TEST_CASE(Rdata_DS_LIKE_Test, Implementations);

TYPED_TEST(Rdata_DS_LIKE_Test, toText_DS_LIKE) {
    EXPECT_EQ(ds_like_txt,
              Rdata_DS_LIKE_Test<TypeParam>::rdata_ds_like.toText());
}

TYPED_TEST(Rdata_DS_LIKE_Test, badText_DS_LIKE) {
    EXPECT_THROW(const TypeParam ds_like2("99999 5 2 BEEF"), InvalidRdataText);
    EXPECT_THROW(const TypeParam ds_like2("11111 555 2 BEEF"),
                 InvalidRdataText);
    EXPECT_THROW(const TypeParam ds_like2("11111 5 22222 BEEF"),
                 InvalidRdataText);
    EXPECT_THROW(const TypeParam ds_like2("11111 5 2"), InvalidRdataText);
    EXPECT_THROW(const TypeParam ds_like2("GARBAGE IN"), InvalidRdataText);
}

// this test currently fails; we must fix it, and then migrate the test to
// badText_DS_LIKE
TYPED_TEST(Rdata_DS_LIKE_Test, DISABLED_badText_DS_LIKE) {
    // no space between the digest type and the digest.
    EXPECT_THROW(const TypeParam ds_like2(
                     "12892 5 2F1E184C0E1D615D20EB3C223ACED3B03C773DD952D"
                     "5F0EB5C777586DE18DA6B5"), InvalidRdataText);
}

TYPED_TEST(Rdata_DS_LIKE_Test, createFromWire_DS_LIKE) {
    EXPECT_EQ(0, Rdata_DS_LIKE_Test<TypeParam>::rdata_ds_like.compare(
              *this->rdataFactoryFromFile(RRTYPE<TypeParam>(), RRClass::IN(),
                                          "rdata_ds_fromWire")));
}

TYPED_TEST(Rdata_DS_LIKE_Test, getTag_DS_LIKE) {
    EXPECT_EQ(12892, Rdata_DS_LIKE_Test<TypeParam>::rdata_ds_like.getTag());
}

TYPED_TEST(Rdata_DS_LIKE_Test, toWireRenderer) {
    Rdata_DS_LIKE_Test<TypeParam>::renderer.skip(2);
    TypeParam rdata_ds_like(ds_like_txt);
    rdata_ds_like.toWire(Rdata_DS_LIKE_Test<TypeParam>::renderer);

    vector<unsigned char> data;
    UnitTestUtil::readWireData("rdata_ds_fromWire", data);
    EXPECT_PRED_FORMAT4(UnitTestUtil::matchWireData,
                        static_cast<const uint8_t*>
                        (Rdata_DS_LIKE_Test<TypeParam>::obuffer.getData()) + 2,
                        Rdata_DS_LIKE_Test<TypeParam>::obuffer.getLength() - 2,
                        &data[2], data.size() - 2);
}

TYPED_TEST(Rdata_DS_LIKE_Test, toWireBuffer) {
    TypeParam rdata_ds_like(ds_like_txt);
    rdata_ds_like.toWire(Rdata_DS_LIKE_Test<TypeParam>::obuffer);
}

TYPED_TEST(Rdata_DS_LIKE_Test, compare) {
    // trivial case: self equivalence
    EXPECT_EQ(0, TypeParam(ds_like_txt).compare(TypeParam(ds_like_txt)));

    // TODO: need more tests
}

}
