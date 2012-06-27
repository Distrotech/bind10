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

#include <datasrc/memory_segment.h>
#include <datasrc/rbtree2.h>
#include <datasrc/rdataset.h>
#include <datasrc/treenode_rrset.h>
#include <datasrc/compress_table.h>

#include <testutils/dnsmessage_test.h>

#include <boost/bind.hpp>
#include <boost/scoped_ptr.hpp>

#include <gtest/gtest.h>

using namespace std;

using namespace isc::testutils;
using namespace isc::dns;
using namespace isc::dns::rdata;
using namespace isc::datasrc;
using experimental::internal::TreeNodeRRset;
using experimental::internal::CompressOffsetTable;

namespace {
class TreeNodeRRsetTest : public::testing::Test {
protected:
    typedef experimental::RBTree<internal::RdataSet> DomainTree;
    typedef experimental::RBNode<internal::RdataSet> DomainNode;

    TreeNodeRRsetTest() :
        tree_(segment_, true),
        rrset_soa_(textToRRset(". 86400 IN SOA "
                               "ns.example.org. admin.example.org. "
                               "2012052901 7200 3600 2592000 1200"))
    {}
    void SetUp() {
        rdset_soa_ = internal::RdataSet::allocate(
            segment_, encoder_,
            boost::bind(&TreeNodeRRsetTest::nameCreator, this, _1),
            rrset_soa_, ConstRRsetPtr());
    }
    ~TreeNodeRRsetTest() {
        if (rdset_soa_) {
            internal::RdataSet::deallocate(segment_, rdset_soa_);
        }
    }
    internal::DomainNodePtr
    nameCreator(const Name* name) {
        assert(name != NULL);
        DomainNode* node = NULL;
        tree_.insert(*name, &node);
        assert(node != NULL);
        return (internal::DomainNodePtr(node));
    }

    CompressOffsetTable offset_table_;
    MemorySegment segment_;
    internal::RdataEncoder encoder_;
    DomainTree tree_;
    ConstRRsetPtr rrset_soa_;
    internal::RdataSetPtr rdset_soa_;
    MessageRenderer renderer_;
};

TEST_F(TreeNodeRRsetTest, construct) {
    DomainNode* node;
    // as a side effect, "." node should have been created.
    EXPECT_EQ(DomainTree::ALREADYEXISTS,
              tree_.insert(rrset_soa_->getName(), &node));
    EXPECT_TRUE(node->isEmpty());
    node->setData(rdset_soa_);
    TreeNodeRRset rrset(RRClass::IN(), *node, *rdset_soa_);
    rrset.setCompressTable(&offset_table_);
    rrset.toWire(renderer_);

    isc::util::InputBuffer b(renderer_.getData(), renderer_.getLength());
    EXPECT_EQ(Name("."), Name(b));
    EXPECT_EQ(RRType::SOA(), RRType(b));
    EXPECT_EQ(RRClass::IN(), RRClass(b));
    EXPECT_EQ(RRTTL(86400), RRTTL(b));
    EXPECT_EQ(44, b.readUint16());
    EXPECT_EQ("ns.example.org. admin.example.org. "
              "2012052901 7200 3600 2592000 1200",
              createRdata(RRType::SOA(), RRClass::IN(), b, 44)->toText());
}
} // end of unnamed namespace
