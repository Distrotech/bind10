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

#include <util/buffer.h>

#include <dns/labelsequence.h>
#include <dns/messagerenderer.h>
#include <dns/rrtype.h>
#include <dns/rrclass.h>
#include <dns/rdata.h>
#include <datasrc/rdata_encoder.h>

#include <boost/scoped_ptr.hpp>
#include <boost/bind.hpp>

#include <gtest/gtest.h>

using namespace std;
using boost::scoped_ptr;

using namespace isc::util;
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
                               "ns.example.org. root.example.org. "
                               "1 2 3 4 65536")),
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
    vector<uint8_t> encode_buf_;
    MessageRenderer m_renderer_;
};

TEST_F(RdataEncoderTest, basicTests) {
    encoder_.addRdata(*a_rdata_);
    encoder_.construct(RRType::A());
    EXPECT_EQ(4, encoder_.getStorageLength());
    encode_buf_.resize(encoder_.getStorageLength());
    EXPECT_EQ(0, encoder_.encodeLengths(
                  reinterpret_cast<uint16_t*>(&encode_buf_[0]),
                  encode_buf_.size() / 2));

    encoder_.clear();
    encoder_.addRdata(*a_rdata_);
    encoder_.addRdata(*a_rdata2_);
    encoder_.construct(RRType::A());
    EXPECT_EQ(8, encoder_.getStorageLength());
    encode_buf_.resize(encoder_.getStorageLength());
    EXPECT_EQ(0, encoder_.encodeLengths(
                  reinterpret_cast<uint16_t*>(&encode_buf_[0]),
                  encode_buf_.size() / 2));

    encoder_.clear();
    encoder_.addRdata(*ns_rdata_);
    encoder_.construct(RRType::NS());
    // namelen = 16, offsetlen = 4
    EXPECT_EQ((16 + 4 + 2), encoder_.getStorageLength());
    encode_buf_.resize(encoder_.getStorageLength());
    EXPECT_EQ(0, encoder_.encodeLengths(
                  reinterpret_cast<uint16_t*>(&encode_buf_[0]),
                  encode_buf_.size() / 2));
    EXPECT_EQ(22, encoder_.encodeData(&encode_buf_[0], encode_buf_.size()));
    LabelSequence seq(&encode_buf_[2], &encode_buf_[encode_buf_[0] + 2],
                      encode_buf_[1]);
    EXPECT_EQ("ns.example.com.", seq.toText());

    encoder_.clear();
    encoder_.addRdata(*soa_rdata_);
    encoder_.construct(RRType::SOA());
    // 1st name data: 16+4+2bytes, 2nd: 18+4+2bytes, rest 20
    EXPECT_EQ((16 + 4 + 2) + (18 + 4 + 2) + 20, encoder_.getStorageLength());
    encode_buf_.resize(encoder_.getStorageLength());
    EXPECT_EQ(0, encoder_.encodeLengths(
                  reinterpret_cast<uint16_t*>(&encode_buf_[0]),
                  encode_buf_.size() / 2));
    EXPECT_EQ(66, encoder_.encodeData(&encode_buf_[0], encode_buf_.size()));
    seq = LabelSequence(&encode_buf_[2], &encode_buf_[encode_buf_[0] + 2],
                        encode_buf_[1]);
    EXPECT_EQ("ns.example.org.", seq.toText());
    seq = LabelSequence(&encode_buf_[24], &encode_buf_[encode_buf_[22] + 2],
                        encode_buf_[23]);
    EXPECT_EQ("root.example.org.", seq.toText());
    const uint8_t expected_soa_data[] = { 0, 0, 0, 1, 0, 0, 0, 2, 0, 0, 0, 3,
                                          0, 0, 0, 4, 0, 1, 0, 0 };
    EXPECT_TRUE(memcmp(expected_soa_data, &encode_buf_[46], 20) == 0);

    encoder_.clear();
    encoder_.addRdata(*txt_rdata_);
    encoder_.addRdata(*txt_rdata2_);
    encoder_.construct(RRType::TXT());
    // 2 varlen fields, 1st text data: 11bytes, 2nd: 6bytes
    EXPECT_EQ((2 * 2) + 11 + 6, encoder_.getStorageLength());
    encode_buf_.resize(encoder_.getStorageLength());
    EXPECT_EQ(2, encoder_.encodeLengths(
                  reinterpret_cast<uint16_t*>(&encode_buf_[0]),
                  encode_buf_.size() / 2));
    const uint16_t* lengths = reinterpret_cast<uint16_t*>(&encode_buf_[0]);
    EXPECT_EQ(11, lengths[0]);
    EXPECT_EQ(6, lengths[1]);

    const uint8_t expected_txt_data[] = { 10, '0', '1', '2', '3', '4', '5',
                                          '6', '7', '8', '9',
                                          5, '0', '1', '2', '3', '4'};
    EXPECT_TRUE(memcmp(expected_txt_data, &encode_buf_[4], 17));
}

TEST_F(RdataEncoderTest, renderer) {
    encoder_.clear();
    encoder_.addRdata(*a_rdata_);
    encoder_.construct(RRType::A());
    encode_buf_.resize(encoder_.getStorageLength()); // should be 4
    encoder_.encodeData(&encode_buf_[0], encode_buf_.size());

    internal::RdataIterator rd_it(
        RRType::A(), 1, NULL, &encode_buf_[0],
        boost::bind(internal::renderName, &m_renderer_, _1, _2),
        boost::bind(internal::renderData, &m_renderer_, _1, _2));
    EXPECT_FALSE(rd_it.isLast());
    rd_it.action();
    EXPECT_TRUE(rd_it.isLast());
    scoped_ptr<InputBuffer> b(new InputBuffer(m_renderer_.getData(),
                                              m_renderer_.getLength()));
    EXPECT_EQ(0, createRdata(RRType::A(), RRClass::IN(),
                             *b, b->getLength())->compare(*a_rdata_));

    m_renderer_.clear();
    encoder_.clear();
    encoder_.addRdata(*soa_rdata_);
    encoder_.construct(RRType::SOA());
    encode_buf_.resize(encoder_.getStorageLength());
    encoder_.encodeData(&encode_buf_[0], encode_buf_.size());

    internal::RdataIterator rd_it_soa(
        RRType::SOA(), 1, NULL, &encode_buf_[0],
        boost::bind(internal::renderName, &m_renderer_, _1, _2),
        boost::bind(internal::renderData, &m_renderer_, _1, _2));
    EXPECT_FALSE(rd_it_soa.isLast());
    rd_it_soa.action();
    EXPECT_TRUE(rd_it_soa.isLast());
    b.reset(new InputBuffer(m_renderer_.getData(), m_renderer_.getLength()));
    EXPECT_EQ(0, createRdata(RRType::SOA(), RRClass::IN(),
                             *b, b->getLength())->compare(*soa_rdata_));

    m_renderer_.clear();
    encoder_.clear();
    encoder_.addRdata(*txt_rdata_);
    encoder_.addRdata(*txt_rdata2_);
    encoder_.construct(RRType::TXT());
    encode_buf_.resize(encoder_.getStorageLength());
    encoder_.encodeLengths(reinterpret_cast<uint16_t*>(&encode_buf_[0]),
                           encode_buf_.size() / 2);
    encoder_.encodeData(&encode_buf_[4], encode_buf_.size() - 4);

    internal::RdataIterator rd_it_txt(
        RRType::TXT(), 2,
        reinterpret_cast<const uint16_t*>(&encode_buf_[0]), &encode_buf_[4],
        boost::bind(internal::renderName, &m_renderer_, _1, _2),
        boost::bind(internal::renderData, &m_renderer_, _1, _2));
    EXPECT_FALSE(rd_it_txt.isLast());
    rd_it_txt.action();
    EXPECT_FALSE(rd_it_txt.isLast());
    rd_it_txt.action();
    EXPECT_TRUE(rd_it_txt.isLast());
    b.reset(new InputBuffer(m_renderer_.getData(), m_renderer_.getLength()));
    EXPECT_EQ(0, createRdata(RRType::TXT(), RRClass::IN(),
                             *b, 11)->compare(*txt_rdata_));
    EXPECT_EQ(0, createRdata(RRType::TXT(), RRClass::IN(),
                             *b, 6)->compare(*txt_rdata2_));
}
} // end of unnamed namespace
