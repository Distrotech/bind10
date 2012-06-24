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

#include <dns/labelsequence.h>
#include <dns/name.h>
#include <exceptions/exceptions.h>

#include <gtest/gtest.h>

#include <boost/functional/hash.hpp>

#include <string>
#include <set>

using namespace isc::dns;
using namespace std;

namespace {

class LabelSequenceTest : public ::testing::Test {
public:
    LabelSequenceTest() : n1("example.org"), n2("example.com"),
                          n3("example.org"), n4("foo.bar.test.example"),
                          n5("example.ORG"), n6("ExAmPlE.org"),
                          n7("."), n8("foo.example.org.bar"),
                          ls1(n1), ls2(n2), ls3(n3), ls4(n4), ls5(n5),
                          ls6(n6), ls7(n7), ls8(n8)
    {};
    // Need to keep names in scope for at least the lifetime of
    // the labelsequences
    Name n1, n2, n3, n4, n5, n6, n7, n8;

    LabelSequence ls1, ls2, ls3, ls4, ls5, ls6, ls7, ls8;
};

// Basic equality tests
TEST_F(LabelSequenceTest, equals_sensitive) {
    EXPECT_TRUE(ls1.equals(ls1, true));
    EXPECT_FALSE(ls1.equals(ls2, true));
    EXPECT_TRUE(ls1.equals(ls3, true));
    EXPECT_FALSE(ls1.equals(ls4, true));
    EXPECT_FALSE(ls1.equals(ls5, true));
    EXPECT_FALSE(ls1.equals(ls6, true));
    EXPECT_FALSE(ls1.equals(ls7, true));
    EXPECT_FALSE(ls1.equals(ls8, true));

    EXPECT_FALSE(ls2.equals(ls1, true));
    EXPECT_TRUE(ls2.equals(ls2, true));
    EXPECT_FALSE(ls2.equals(ls3, true));
    EXPECT_FALSE(ls2.equals(ls4, true));
    EXPECT_FALSE(ls2.equals(ls5, true));
    EXPECT_FALSE(ls2.equals(ls6, true));
    EXPECT_FALSE(ls2.equals(ls7, true));
    EXPECT_FALSE(ls2.equals(ls8, true));

    EXPECT_FALSE(ls4.equals(ls1, true));
    EXPECT_FALSE(ls4.equals(ls2, true));
    EXPECT_FALSE(ls4.equals(ls3, true));
    EXPECT_TRUE(ls4.equals(ls4, true));
    EXPECT_FALSE(ls4.equals(ls5, true));
    EXPECT_FALSE(ls4.equals(ls6, true));
    EXPECT_FALSE(ls4.equals(ls7, true));
    EXPECT_FALSE(ls4.equals(ls8, true));

    EXPECT_FALSE(ls5.equals(ls1, true));
    EXPECT_FALSE(ls5.equals(ls2, true));
    EXPECT_FALSE(ls5.equals(ls3, true));
    EXPECT_FALSE(ls5.equals(ls4, true));
    EXPECT_TRUE(ls5.equals(ls5, true));
    EXPECT_FALSE(ls5.equals(ls6, true));
    EXPECT_FALSE(ls5.equals(ls7, true));
    EXPECT_FALSE(ls5.equals(ls8, true));
}

//
// Helper class to hold comparison test parameters.
//
struct CompareParameters {
    CompareParameters(const Name& n1, const Name&  n2,
                      NameComparisonResult::NameRelation r, int o,
                      unsigned int l) :
        name1(n1), name2(n2), reln(r), order(o), labels(l) {}
    static int normalizeOrder(int o) {
        if (o > 0) {
            return (1);
        } else if (o < 0) {
            return (-1);
        }
        return (0);
    }
    Name name1;
    Name name2;
    NameComparisonResult::NameRelation reln;
    int order;
    unsigned int labels;
};

TEST_F(LabelSequenceTest, equals_insensitive) {
    EXPECT_TRUE(ls1.equals(ls1));
    EXPECT_FALSE(ls1.equals(ls2));
    EXPECT_TRUE(ls1.equals(ls3));
    EXPECT_FALSE(ls1.equals(ls4));
    EXPECT_TRUE(ls1.equals(ls5));
    EXPECT_TRUE(ls1.equals(ls6));
    EXPECT_FALSE(ls1.equals(ls7));

    EXPECT_FALSE(ls2.equals(ls1));
    EXPECT_TRUE(ls2.equals(ls2));
    EXPECT_FALSE(ls2.equals(ls3));
    EXPECT_FALSE(ls2.equals(ls4));
    EXPECT_FALSE(ls2.equals(ls5));
    EXPECT_FALSE(ls2.equals(ls6));
    EXPECT_FALSE(ls2.equals(ls7));

    EXPECT_TRUE(ls3.equals(ls1));
    EXPECT_FALSE(ls3.equals(ls2));
    EXPECT_TRUE(ls3.equals(ls3));
    EXPECT_FALSE(ls3.equals(ls4));
    EXPECT_TRUE(ls3.equals(ls5));
    EXPECT_TRUE(ls3.equals(ls6));
    EXPECT_FALSE(ls3.equals(ls7));

    EXPECT_FALSE(ls4.equals(ls1));
    EXPECT_FALSE(ls4.equals(ls2));
    EXPECT_FALSE(ls4.equals(ls3));
    EXPECT_TRUE(ls4.equals(ls4));
    EXPECT_FALSE(ls4.equals(ls5));
    EXPECT_FALSE(ls4.equals(ls6));
    EXPECT_FALSE(ls4.equals(ls7));

    EXPECT_TRUE(ls5.equals(ls1));
    EXPECT_FALSE(ls5.equals(ls2));
    EXPECT_TRUE(ls5.equals(ls3));
    EXPECT_FALSE(ls5.equals(ls4));
    EXPECT_TRUE(ls5.equals(ls5));
    EXPECT_TRUE(ls5.equals(ls6));
    EXPECT_FALSE(ls5.equals(ls7));
}

TEST_F(LabelSequenceTest, compare) {
    vector<CompareParameters> params;
    params.push_back(CompareParameters(Name("c.d"), Name("a.b.c.d"),
                                       NameComparisonResult::SUPERDOMAIN,
                                       -1, 3));
    params.push_back(CompareParameters(Name("a.b.c.d"), Name("c.d"),
                                       NameComparisonResult::SUBDOMAIN, 1, 3));
    params.push_back(CompareParameters(Name("a.b.c.d"), Name("c.d.e.f"),
                                       NameComparisonResult::COMMONANCESTOR,
                                       -1, 1));
    params.push_back(CompareParameters(Name("a.b.c.d"), Name("f.g.c.d"),
                                       NameComparisonResult::COMMONANCESTOR,
                                       -1, 3));
    params.push_back(CompareParameters(Name("a.b.c.d"), Name("A.b.C.d."),
                                       NameComparisonResult::EQUAL,
                                       0, 5));

    vector<CompareParameters>::const_iterator it;
    for (it = params.begin(); it != params.end(); ++it) {
        NameComparisonResult result = LabelSequence((*it).name1).
            compare(LabelSequence((*it).name2));
        EXPECT_EQ((*it).reln, result.getRelation());
        EXPECT_EQ((*it).order,
                  CompareParameters::normalizeOrder(result.getOrder()));
        EXPECT_EQ((*it).labels, result.getCommonLabels());
    }

    const Name a("example.com");
    const Name b("example.org");
    LabelSequence la(a);
    LabelSequence lb(b);
    la.stripRight(1);
    lb.stripRight(1);
    EXPECT_EQ(NameComparisonResult::NONE, la.compare(lb).getRelation());
    EXPECT_EQ(0, la.compare(lb).getCommonLabels());
    EXPECT_EQ(-1, CompareParameters::normalizeOrder(la.compare(lb).getOrder()));

    const Name c("example.c");
    LabelSequence lc(c);
    lc.stripRight(1);
    EXPECT_EQ(NameComparisonResult::NONE, la.compare(lc).getRelation());
}

void
getDataCheck(const char* expected_cdata, size_t expected_len,
             const LabelSequence& ls)
{
    vector<uint8_t> expected_data(expected_cdata,
                                  expected_cdata + expected_len);

    size_t len;
    const unsigned char* data = ls.getData(&len);
    ASSERT_EQ(expected_len, len) << "Expected data: " << expected_cdata <<
                                    " name: " << ls.toText();
    EXPECT_EQ(expected_len, ls.getDataLength()) <<
        "Expected data: " << expected_cdata <<
        " name: " << ls.toText();
    for (size_t i = 0; i < len; ++i) {
        EXPECT_EQ(expected_data[i], data[i]) <<
          "Difference at pos " << i << ": Expected data: " << expected_cdata <<
          " name: " << ls.toText();;
    }
}

TEST_F(LabelSequenceTest, getData) {
    getDataCheck("\007example\003org\000", 13, ls1);
    ls1.stripRight(1);
    getDataCheck("\007example\003org", 12, ls1);
    ls1.stripLeft(1);
    getDataCheck("\003org", 4, ls1);
    getDataCheck("\007example\003org\000", 13, ls3);
    getDataCheck("\003foo\003bar\004test\007example\000", 22, ls4);
    getDataCheck("\007example\003ORG\000", 13, ls5);
    getDataCheck("\007ExAmPlE\003org\000", 13, ls6);
    getDataCheck("\000", 1, ls7);
};

TEST_F(LabelSequenceTest, getOffsetData) {
    uint8_t placeholder[Name::MAX_LABELS];

    size_t olen;
    const uint8_t* odata;
    odata = ls1.getOffsetData(&olen, placeholder);
    uint8_t expect1[] = {0, 8, 12};
    EXPECT_EQ(olen, sizeof(expect1));
    EXPECT_EQ(0, memcmp(odata, expect1, olen));

    ls1.stripRight(1);
    odata = ls1.getOffsetData(&olen, placeholder);
    uint8_t expect2[] = {0, 8};
    EXPECT_EQ(olen, sizeof(expect2));
    EXPECT_EQ(0, memcmp(odata, expect2, olen));

    ls1.stripLeft(1);
    odata = ls1.getOffsetData(&olen, placeholder);
    uint8_t expect3[] = {0};
    EXPECT_EQ(olen, sizeof(expect3));
    EXPECT_EQ(0, memcmp(odata, expect3, olen));

    // boundary check
    const Name longname("a.a.a.a.a.a.a.a.a.a.a.a.a.a.a.a."
                        "a.a.a.a.a.a.a.a.a.a.a.a.a.a.a.a."
                        "a.a.a.a.a.a.a.a.a.a.a.a.a.a.a.a."
                        "a.a.a.a.a.a.a.a.a.a.a.a.a.a.a.a."
                        "a.a.a.a.a.a.a.a.a.a.a.a.a.a.a.a."
                        "a.a.a.a.a.a.a.a.a.a.a.a.a.a.a.a."
                        "a.a.a.a.a.a.a.a.a.a.a.a.a.a.a.a."
                        "a.a.a.a.a.a.a.a.a.a.a.a.a.a.a.");
    LabelSequence sequence(longname);
    odata = sequence.getOffsetData(&olen, placeholder);
    EXPECT_EQ(olen, 128);
};

TEST_F(LabelSequenceTest, stripLeft) {
    EXPECT_TRUE(ls1.equals(ls3));
    ls1.stripLeft(0);
    getDataCheck("\007example\003org\000", 13, ls1);
    EXPECT_TRUE(ls1.equals(ls3));
    ls1.stripLeft(1);
    getDataCheck("\003org\000", 5, ls1);
    EXPECT_FALSE(ls1.equals(ls3));
    ls1.stripLeft(1);
    getDataCheck("\000", 1, ls1);
    EXPECT_TRUE(ls1.equals(ls7));

    ls2.stripLeft(2);
    getDataCheck("\000", 1, ls2);
    EXPECT_TRUE(ls2.equals(ls7));
}

TEST_F(LabelSequenceTest, stripRight) {
    EXPECT_TRUE(ls1.equals(ls3));
    ls1.stripRight(1);
    getDataCheck("\007example\003org", 12, ls1);
    EXPECT_FALSE(ls1.equals(ls3));
    ls1.stripRight(1);
    getDataCheck("\007example", 8, ls1);
    EXPECT_FALSE(ls1.equals(ls3));

    ASSERT_FALSE(ls1.equals(ls2));
    ls2.stripRight(2);
    getDataCheck("\007example", 8, ls2);
    EXPECT_TRUE(ls1.equals(ls2));
}

TEST_F(LabelSequenceTest, stripOutOfRange) {
    EXPECT_THROW(ls1.stripLeft(100), isc::OutOfRange);
    EXPECT_THROW(ls1.stripLeft(5), isc::OutOfRange);
    EXPECT_THROW(ls1.stripLeft(4), isc::OutOfRange);
    EXPECT_THROW(ls1.stripLeft(3), isc::OutOfRange);
    getDataCheck("\007example\003org\000", 13, ls1);

    EXPECT_THROW(ls1.stripRight(100), isc::OutOfRange);
    EXPECT_THROW(ls1.stripRight(5), isc::OutOfRange);
    EXPECT_THROW(ls1.stripRight(4), isc::OutOfRange);
    EXPECT_THROW(ls1.stripRight(3), isc::OutOfRange);
    getDataCheck("\007example\003org\000", 13, ls1);
}

TEST_F(LabelSequenceTest, getLabelCount) {
    EXPECT_EQ(3, ls1.getLabelCount());
    ls1.stripLeft(0);
    EXPECT_EQ(3, ls1.getLabelCount());
    ls1.stripLeft(1);
    EXPECT_EQ(2, ls1.getLabelCount());
    ls1.stripLeft(1);
    EXPECT_EQ(1, ls1.getLabelCount());

    EXPECT_EQ(3, ls2.getLabelCount());
    ls2.stripRight(1);
    EXPECT_EQ(2, ls2.getLabelCount());
    ls2.stripRight(1);
    EXPECT_EQ(1, ls2.getLabelCount());

    EXPECT_EQ(3, ls3.getLabelCount());
    ls3.stripRight(2);
    EXPECT_EQ(1, ls3.getLabelCount());

    EXPECT_EQ(5, ls4.getLabelCount());
    ls4.stripRight(3);
    EXPECT_EQ(2, ls4.getLabelCount());

    EXPECT_EQ(3, ls5.getLabelCount());
    ls5.stripLeft(2);
    EXPECT_EQ(1, ls5.getLabelCount());
}

TEST_F(LabelSequenceTest, comparePart) {
    EXPECT_FALSE(ls1.equals(ls8));

    // strip root label from example.org.
    ls1.stripRight(1);
    // strip foo from foo.example.org.bar.
    ls8.stripLeft(1);
    // strip bar. (i.e. bar and root) too
    ls8.stripRight(2);

    EXPECT_TRUE(ls1.equals(ls8));

    // Data comparison
    size_t len;
    const unsigned char* data = ls1.getData(&len);
    getDataCheck((const char*) data, len, ls8);
}

TEST_F(LabelSequenceTest, isAbsolute) {
    EXPECT_TRUE(ls1.isAbsolute());

    ls1.stripLeft(1);
    EXPECT_TRUE(ls1.isAbsolute());
    ls1.stripRight(1);
    EXPECT_FALSE(ls1.isAbsolute());

    EXPECT_TRUE(ls2.isAbsolute());
    ls2.stripRight(1);
    EXPECT_FALSE(ls2.isAbsolute());

    EXPECT_TRUE(ls3.isAbsolute());
    ls3.stripLeft(2);
    EXPECT_TRUE(ls3.isAbsolute());
}

// The following are test data used in the getHash test below.  Normally
// we use example/documentation domain names for testing, but in this case
// we'd specifically like to use more realistic data, and are intentionally
// using real-world samples: They are the NS names of root and some top level
// domains as of this test.
const char* const root_servers[] = {
    "a.root-servers.net", "b.root-servers.net", "c.root-servers.net",
    "d.root-servers.net", "e.root-servers.net", "f.root-servers.net",
    "g.root-servers.net", "h.root-servers.net", "i.root-servers.net",
    "j.root-servers.net", "k.root-servers.net", "l.root-servers.net",
    "m.root-servers.net", NULL
};
const char* const gtld_servers[] = {
    "a.gtld-servers.net", "b.gtld-servers.net", "c.gtld-servers.net",
    "d.gtld-servers.net", "e.gtld-servers.net", "f.gtld-servers.net",
    "g.gtld-servers.net", "h.gtld-servers.net", "i.gtld-servers.net",
    "j.gtld-servers.net", "k.gtld-servers.net", "l.gtld-servers.net",
    "m.gtld-servers.net", NULL
};
const char* const jp_servers[] = {
    "a.dns.jp", "b.dns.jp", "c.dns.jp", "d.dns.jp", "e.dns.jp",
    "f.dns.jp", "g.dns.jp", NULL
};
const char* const cn_servers[] = {
    "a.dns.cn", "b.dns.cn", "c.dns.cn", "d.dns.cn", "e.dns.cn",
    "ns.cernet.net", NULL
};
const char* const ca_servers[] = {
    "k.ca-servers.ca", "e.ca-servers.ca", "a.ca-servers.ca", "z.ca-servers.ca",
    "tld.isc-sns.net", "c.ca-servers.ca", "j.ca-servers.ca", "l.ca-servers.ca",
    "sns-pb.isc.org", "f.ca-servers.ca", NULL
};

// A helper function used in the getHash test below.
void
hashDistributionCheck(const char* const* servers) {
    const size_t BUCKETS = 64;  // constant used in the MessageRenderer
    set<Name> names;
    vector<size_t> hash_counts(BUCKETS);

    // Store all test names and their super domain names (excluding the
    // "root" label) in the set, calculates their hash values, and increments
    // the counter for the corresponding hash "bucket".
    for (size_t i = 0; servers[i] != NULL; ++i) {
        const Name name(servers[i]);
        for (size_t l = 0; l < name.getLabelCount() - 1; ++l) {
            pair<set<Name>::const_iterator, bool> ret =
                names.insert(name.split(l));
            if (ret.second) {
                hash_counts[LabelSequence((*ret.first)).getHash(false) %
                            BUCKETS]++;
            }
        }
    }

    // See how many conflicts we have in the buckets.  For the testing purpose
    // we expect there's at most 2 conflicts in each set, which is an
    // arbitrary choice (it should happen to succeed with the hash function
    // and data we are using; if it's not the case, maybe with an update to
    // the hash implementation, we should revise the test).
    for (size_t i = 0; i < BUCKETS; ++i) {
        EXPECT_GE(3, hash_counts[i]);
    }
}

TEST_F(LabelSequenceTest, getHash) {
    // Trivial case.  The same sequence should have the same hash.
    EXPECT_EQ(ls1.getHash(true), ls1.getHash(true));

    // Check the case-insensitive mode behavior.
    EXPECT_EQ(ls1.getHash(false), ls5.getHash(false));

    // Check that the distribution of hash values is "not too bad" (such as
    // everything has the same hash value due to a stupid bug).  It's
    // difficult to check such things reliably.  We do some ad hoc tests here.
    hashDistributionCheck(root_servers);
    hashDistributionCheck(jp_servers);
    hashDistributionCheck(cn_servers);
    hashDistributionCheck(ca_servers);
}

}
