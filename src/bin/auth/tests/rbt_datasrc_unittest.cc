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
#include <dns/messagerenderer.h>
#include <dns/name.h>
#include <dns/rdata.h>
#include <dns/rrclass.h>
#include <dns/rrset.h>
#include <dns/rrtype.h>
#include <dns/rrttl.h>

#include <auth/rbt_datasrc.h>
#include <auth/root_datasrc.h>

#include <dns/tests/unittest_util.h>

using namespace std;
using isc::UnitTestUtil;
using namespace isc::dns;

namespace {
class RBTDataSrcTest : public ::testing::Test {
protected:
    RBTDataSrcTest() : datasrc(Name::ROOT_NAME()), buffer(0),
                       renderer(buffer, &compress_offsets)
    {
        datasrc.addNode(Name("a"), &rbtnode);
        datasrc.addNode(Name("b"), &rbtnode);
        datasrc.addNode(Name("c"), &rbtnode);
        datasrc.addNode(Name("x.d.e.f"), &rbtnode);
        datasrc.addNode(Name("z.d.e.f"), &rbtnode);
        datasrc.addNode(Name("g.h"), &rbtnode);
        datasrc.addNode(Name("o.w.y.d.e.f"), &rbtnode);
        datasrc.addNode(Name("p.w.y.d.e.f"), &rbtnode);
        datasrc.addNode(Name("q.w.y.d.e.f"), &rbtnode);
    }
    RbtDataSrc datasrc;
    OutputBuffer buffer;
    MessageRenderer renderer;
    vector<unsigned char> data;
    RbtNode rbtnode;
    CompressOffset compress_offsets;
};

TEST_F(RBTDataSrcTest, addNames) {
    EXPECT_EQ(RbtDataSrcSuccess,
              datasrc.addNode(Name("example.com"), &rbtnode));
    cout << datasrc.toText() << endl;
    EXPECT_EQ("example.com", rbtnode.toText());
    EXPECT_EQ(RbtDataSrcExist, datasrc.addNode(Name("x.d.e.f"), &rbtnode));
    EXPECT_EQ("x", rbtnode.toText());
    //cout << datasrc.toText() << endl;
}

TEST_F(RBTDataSrcTest, findName) {
    cout << datasrc.toText() << endl;
    EXPECT_EQ(RbtDataSrcSuccess,
              datasrc.findNode(Name("o.w.y.d.e.f"), &rbtnode));
    EXPECT_EQ("o", rbtnode.toText());
    EXPECT_EQ(RbtDataSrcNotFound,
              datasrc.findNode(Name("example.com"), &rbtnode));
    EXPECT_EQ(RbtDataSrcPartialMatch,
              datasrc.findNode(Name("y.d.e.f"), &rbtnode));
    EXPECT_EQ("d.e.f", rbtnode.toText());
}

TEST_F(RBTDataSrcTest, addRRset) {
    RRsetPtr rrset = RRsetPtr(new RRset(Name("."), RRClass::IN(),
                                        RRType::SOA(), RRTTL(86400)));
    rrset->addRdata(rdata::createRdata(RRType::SOA(), RRClass::IN(),
                                       "a.root-servers.net. nstld.verisign-grs.com. 2010052302 1800 900 604800 86400"));
    datasrc.addRRset(*rrset);

    rrset = RRsetPtr(new RRset(Name("ns1.telone.co.zw"), RRClass::IN(),
                               RRType::A(), RRTTL(172800)));
    rrset->addRdata(rdata::createRdata(RRType::A(), RRClass::IN(),
                                       "194.133.122.47"));
    rrset->addRdata(rdata::createRdata(RRType::A(), RRClass::IN(),
                                       "194.133.122.48"));
    datasrc.addRRset(*rrset);

    RbtRRset rbtrrset;
    datasrc.findNode(Name("ns1.telone.co.zw"), &rbtnode);
    rbtnode.findRRset(RRType::A(), rbtrrset);
    EXPECT_EQ(Name("ns1.telone.co.zw"), rbtrrset.getName());
    EXPECT_EQ(rrset->getType(), rbtrrset.getType());
    EXPECT_EQ(rrset->getClass(), rbtrrset.getClass());
    EXPECT_EQ(rrset->getTTL(), rbtrrset.getTTL());

    rrset = RRsetPtr(new RRset(Name("x.d.e.f"), RRClass::IN(),
                               RRType::AAAA(), RRTTL(3600)));
    rrset->addRdata(rdata::createRdata(RRType::AAAA(), RRClass::IN(),
                                       "2001:db8::1234"));
    datasrc.addRRset(*rrset);

    rbtrrset.clear();
    datasrc.findNode(Name("x.d.e.f"), &rbtnode);
    rbtnode.findRRset(RRType::AAAA(), rbtrrset);
    EXPECT_EQ(Name("x.d.e.f"), rbtrrset.getName());
    EXPECT_EQ(rrset->getType(), rbtrrset.getType());
    EXPECT_EQ(rrset->getClass(), rbtrrset.getClass());
    EXPECT_EQ(rrset->getTTL(), rbtrrset.getTTL());

    rrset = RRsetPtr(new RRset(Name("example.com"), RRClass::IN(), RRType::NS(),
                               RRTTL(3600)));
    rrset->addRdata(rdata::createRdata(RRType::NS(), RRClass::IN(),
                                       "a.example.com."));
    rrset->addRdata(rdata::createRdata(RRType::NS(), RRClass::IN(),
                                       "c.example.com."));
    datasrc.addRRset(*rrset);

    // NS names should have been added.
    EXPECT_EQ(RbtDataSrcExist, datasrc.addNode(Name("a.example.com"), &rbtnode));

    // Check if it's rendered correctly.
    rbtrrset.clear();
    EXPECT_EQ(RbtDataSrcSuccess, datasrc.findNode(Name("example.com"),
                                                  &rbtnode));
    EXPECT_EQ(RbtDataSrcSuccess, rbtnode.findRRset(RRType::NS(), rbtrrset));
    renderer.clear();
    memset(&compress_offsets, 0xff, sizeof(compress_offsets));
    rbtrrset.toWire(renderer);
    data.clear();
    UnitTestUtil::readWireData("rr_ns_toWire", data);
    EXPECT_PRED_FORMAT4(UnitTestUtil::matchWireData,
                        static_cast<const uint8_t*>(buffer.getData()),
                        buffer.getLength() , &data[0], data.size());

    rrset = RRsetPtr(new RRset(Name("example.com"), RRClass::IN(),
                               RRType::SOA(), RRTTL(3600)));
    rrset->addRdata(rdata::createRdata(RRType::SOA(), RRClass::IN(),
                                       "ns.example.com. root.example.com. "
                                       "2010012601 3600 300 3600000 1200"));
    datasrc.addRRset(*rrset);
    // M/Rnames should have been added.
    EXPECT_EQ(RbtDataSrcExist, datasrc.addNode(Name("ns.example.com"),
                                               &rbtnode));
    EXPECT_EQ(RbtDataSrcExist, datasrc.addNode(Name("root.example.com"),
                                               &rbtnode));
    rbtrrset.clear();
    EXPECT_EQ(RbtDataSrcSuccess, datasrc.findNode(Name("example.com"),
                                                  &rbtnode));
    EXPECT_EQ(RbtDataSrcSuccess, rbtnode.findRRset(RRType::SOA(), rbtrrset));
    renderer.clear();
    memset(&compress_offsets, 0xff, sizeof(compress_offsets));
    rbtrrset.toWire(renderer);

    data.clear();
    UnitTestUtil::readWireData("rr_soa_toWire", data);
    EXPECT_PRED_FORMAT4(UnitTestUtil::matchWireData,
                        static_cast<const uint8_t*>(buffer.getData()),
                        buffer.getLength() , &data[0], data.size());
}

TEST_F(RBTDataSrcTest, rootDataSrcTest) {
    const RbtDataSrc* root_datasrc = createRootRbtDataSrc();

    EXPECT_EQ(RbtDataSrcPartialMatch,
              root_datasrc->findNode(Name("www.example.com"), &rbtnode));
}
}
