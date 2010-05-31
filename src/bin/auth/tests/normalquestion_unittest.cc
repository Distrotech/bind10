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

// $Id$

#include <gtest/gtest.h>

#include <dns/buffer.h>
#include <dns/name.h>
#include <dns/rrclass.h>
#include <dns/rrtype.h>

#include <auth/normalquestion.h>

#include <dns/tests/unittest_util.h>

using namespace std;
using isc::UnitTestUtil;
using namespace isc::dns;

namespace {
class NormalQuestionTest : public ::testing::Test {
protected:
    NormalQuestion question;
    LabelSequence sequence;
};

TEST_F(NormalQuestionTest, fromWire) {
    const char data[] =  { 0x07, 0x65, 0x78, 0x61, 0x6d, 0x70, 0x6c, 0x65, 0x03,
                           0x63, 0x6f, 0x6d, 0x00, // "example.com."
                           0x00, 0x06, 0x00, 0x01 }; // SOA/IN
    InputBuffer buffer(data, sizeof(data));
    question.fromWire(buffer);
    question.setLabelSequenceToQName(sequence);
    EXPECT_EQ("example.com.", sequence.toText());
    EXPECT_EQ(RRType::SOA(), question.getType());
    EXPECT_EQ(RRClass::IN(), question.getClass());

    question.clear();
    const char data2[] =  { 0x03, 0x77, 0x77, 0x77, 0x07, 0x65, 0x78, 0x61,
                            0x6d, 0x70, 0x6c, 0x65, 0x03, 0x6f, 0x72, 0x67,
                            0x00, // "www.example.org."
                            0x00, 0x01, 0x00, 0x03 };     // A/CH
    InputBuffer buffer2(data2, sizeof(data2));
    question.fromWire(buffer2);
    question.setLabelSequenceToQName(sequence);
    EXPECT_EQ("www.example.org.", sequence.toText());
    EXPECT_EQ(RRType::A(), question.getType());
    EXPECT_EQ(RRClass::CH(), question.getClass());
}

}
