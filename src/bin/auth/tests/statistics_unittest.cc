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

#include <config.h>

#include <string>

#include <gtest/gtest.h>

#include <boost/bind.hpp>

#include <dns/opcode.h>
#include <dns/rcode.h>

#include <cc/data.h>

#include <auth/statistics.h>
#include <auth/statistics_items.h>

#include <dns/tests/unittest_util.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

using namespace std;
using namespace isc::cc;
using namespace isc::dns;
using namespace isc::data;
using isc::auth::statistics::Counters;
using isc::auth::statistics::QRAttributes;

namespace {

class CountersTest : public ::testing::Test {
protected:
    CountersTest() : counters() {}
    ~CountersTest() {}
    Counters counters;
};

bool checkCountersAllZeroExcept(const Counters::item_tree_type counters,
                                const std::set<std::string>& except_for) {
    std::map<std::string, ConstElementPtr> stats_map;
    counters->getValue(stats_map);

    for (std::map<std::string, ConstElementPtr>::const_iterator
            i = stats_map.begin(), e = stats_map.end();
            i != e;
            ++i)
    {
        int expect = 0;
        if (except_for.count(i->first) != 0) {
            expect = 1;
        }
        EXPECT_EQ(expect, i->second->intValue()) << "Expected counter "
            << i->first << " = " << expect << ", actual: "
            << i->second->intValue();
    }

    return false;
}

TEST_F(CountersTest, incrementNormalQuery) {
    Message response(Message::RENDER);
    QRAttributes qrattrs;
    std::set<std::string> expect_nonzero;

    expect_nonzero.clear();
    checkCountersAllZeroExcept(counters.dump(), expect_nonzero);

    qrattrs.setQueryIPVersion(AF_INET6);
    qrattrs.setQueryTransportProtocol(IPPROTO_UDP);
    qrattrs.setQueryOpCode(Opcode::QUERY_CODE);
    qrattrs.setQueryEDNS(true, false);
    qrattrs.setQueryDO(true);
    qrattrs.answerHasSent();

    response.setRcode(Rcode::REFUSED());
    response.addQuestion(Question(Name("example.com"), RRClass::IN(), RRType::AAAA()));

    counters.inc(qrattrs, response);

    expect_nonzero.clear();
    expect_nonzero.insert("auth.server.qr.opcode.query");
    expect_nonzero.insert("auth.server.qr.qtype.aaaa");
    expect_nonzero.insert("auth.server.qr.request.v6");
    expect_nonzero.insert("auth.server.qr.request.udp");
    expect_nonzero.insert("auth.server.qr.request.edns0");
    expect_nonzero.insert("auth.server.qr.request.dnssec_ok");
    expect_nonzero.insert("auth.server.qr.response");
    expect_nonzero.insert("auth.server.qr.qrynoauthans");
    expect_nonzero.insert("auth.server.qr.rcode.refused");
    expect_nonzero.insert("auth.server.qr.authqryrej");
    checkCountersAllZeroExcept(counters.dump(), expect_nonzero);
}

TEST_F(CountersTest, incrementNormalQuery_delta) {
    Message response(Message::RENDER);
    QRAttributes qrattrs;
    std::set<std::string> expect_nonzero;
    Counters::item_node_name_set_type names;

    // get "auth.server.qr"
    names.insert("auth.server.qr");

    expect_nonzero.clear();
    checkCountersAllZeroExcept(counters.get(names), expect_nonzero);

    qrattrs.setQueryIPVersion(AF_INET6);
    qrattrs.setQueryTransportProtocol(IPPROTO_UDP);
    qrattrs.setQueryOpCode(Opcode::QUERY_CODE);
    qrattrs.setQueryEDNS(true, false);
    qrattrs.setQueryDO(true);
    qrattrs.answerHasSent();

    response.setRcode(Rcode::REFUSED());
    response.addQuestion(Question(Name("example.com"), RRClass::IN(), RRType::AAAA()));

    counters.inc(qrattrs, response);

    expect_nonzero.clear();
    expect_nonzero.insert("auth.server.qr.opcode.query");
    expect_nonzero.insert("auth.server.qr.qtype.aaaa");
    expect_nonzero.insert("auth.server.qr.request.v6");
    expect_nonzero.insert("auth.server.qr.request.udp");
    expect_nonzero.insert("auth.server.qr.request.edns0");
    expect_nonzero.insert("auth.server.qr.request.dnssec_ok");
    expect_nonzero.insert("auth.server.qr.response");
    expect_nonzero.insert("auth.server.qr.qrynoauthans");
    expect_nonzero.insert("auth.server.qr.rcode.refused");
    checkCountersAllZeroExcept(counters.get(names), expect_nonzero);
    expect_nonzero.clear();
    checkCountersAllZeroExcept(counters.get(names), expect_nonzero);
    qrattrs.setQueryIPVersion(AF_INET6);
    expect_nonzero.insert("auth.server.qr.qtype.aaaa");
    checkCountersAllZeroExcept(counters.get(names), expect_nonzero);
}

TEST_F(CountersTest, checkDumpItems) {
    std::map<std::string, ConstElementPtr> stats_map;
    counters.dump()->getValue(stats_map);
    for (std::map<std::string, ConstElementPtr>::const_iterator
            i = stats_map.begin(), e = stats_map.end();
            i != e;
            ++i)
    {
        // item name check
        const std::string stats_prefix("auth.server.");
        EXPECT_EQ(0, i->first.compare(0, stats_prefix.size(), stats_prefix))
            << "Item name " << i->first << " does not begin with "
            << stats_prefix;

        // item type check
        EXPECT_NO_THROW(i->second->intValue())
            << "Item " << i->first << " is not IntElement";
    }
}

TEST_F(CountersTest, checkGetItems) {
    std::map<std::string, ConstElementPtr> stats_map;

    Counters::item_node_name_set_type names;

    // get "auth.server.qr"
    names.insert("auth.server.qr");
    counters.get(names)->getValue(stats_map);
    EXPECT_EQ(QR_COUNTER_TYPES, 0);
    for (std::map<std::string, ConstElementPtr>::const_iterator
            i = stats_map.begin(), e = stats_map.end();
            i != e;
            ++i)
    {
        // item name check
        const std::string stats_prefix("auth.server.qr.");
        EXPECT_EQ(0, i->first.compare(0, stats_prefix.size(), stats_prefix))
            << "Item name " << i->first << " does not begin with "
            << stats_prefix;

        // item type check
        EXPECT_NO_THROW(i->second->intValue())
            << "Item " << i->first << " is not IntElement";
    }

    // get undefined tree
    names.clear();
    stats_map.clear();
    names.insert("auth.server.UNDEFINED_TREE");
    counters.get(names)->getValue(stats_map);
    EXPECT_EQ(0, stats_map.size());

    // get empty list
    names.clear();
    stats_map.clear();
    counters.get(names)->getValue(stats_map);
    EXPECT_EQ(0, stats_map.size());
}

TEST_F(CountersTest, checkGetClearItems) {
    std::map<std::string, ConstElementPtr> stats_map;
    QRAttributes qrattrs;
    Counters::item_node_name_set_type names;

    // get "auth.server.qr"
    names.insert("auth.server.qr");
    counters.get(names)->getValue(stats_map);
    EXPECT_EQ(0, stats_map.size());

    qrattrs.setQueryIPVersion(AF_INET6);
    qrattrs.setQueryTransportProtocol(IPPROTO_UDP);
    qrattrs.setQueryOpCode(Opcode::QUERY_CODE);
    qrattrs.setQueryEDNS(true, false);
    qrattrs.setQueryDO(true);
    qrattrs.answerHasSent();

    counters.get(names)->getValue(stats_map);
    EXPECT_EQ(0, stats_map.size());
}

TEST(StatisticsItemsTest, QRItemNamesCheck) {
    // check the number of elements in the array
    EXPECT_EQ(sizeof(QRCounterItemName) / sizeof(QRCounterItemName[0]), QR_COUNTER_TYPES);
    // check the name of the first enum element
    EXPECT_EQ(QRCounterItemName[QR_REQUEST_IPV4], "request.v4");
    // check the name of the last enum element
    EXPECT_EQ(QRCounterItemName[QR_RCODE_OTHER], "rcode.other");
}

TEST(StatisticsItemsTest, SocketItemNamesCheck) {
    // check the number of elements in the array
    EXPECT_EQ(sizeof(SocketCounterItemName) / sizeof(SocketCounterItemName[0]), SOCKET_COUNTER_TYPES);
    // check the name of the first enum element
    EXPECT_EQ(SocketCounterItemName[SOCKET_IPV4_UDP_BINDFAIL], "ipv4.udp.bindfail");
    // check the name of the last enum element
    EXPECT_EQ(SocketCounterItemName[SOCKET_UNIXDOMAIN_SENDERR], "unixdomain.senderr");
}

}
