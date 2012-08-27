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
#include <cc/session.h>

#include <auth/statistics.h>
#include <auth/statistics_items.h>

#include <dns/tests/unittest_util.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

using isc::UnitTestUtil;
using namespace std;
using namespace isc::cc;
using namespace isc::dns;
using namespace isc::data;
using isc::auth::statistics::Counters;
using isc::auth::statistics::QRAttributes;

namespace {

class CountersTest : public ::testing::Test {
private:
    class MockSession : public AbstractSession {
    public:
        MockSession() :
            // by default we return a simple "success" message.
            msg_(Element::fromJSON("{\"result\": [0, \"SUCCESS\"]}")),
            throw_session_error_(false), throw_session_timeout_(false)
        {}
        virtual void establish(const char* socket_file);
        virtual void disconnect();
        virtual int group_sendmsg(ConstElementPtr msg, string group,
                                  string instance, string to);
        virtual bool group_recvmsg(ConstElementPtr& envelope,
                                   ConstElementPtr& msg,
                                   bool nonblock, int seq);
        virtual void subscribe(string group, string instance);
        virtual void unsubscribe(string group, string instance);
        virtual void startRead(boost::function<void()> read_callback);
        virtual int reply(ConstElementPtr envelope, ConstElementPtr newmsg);
        virtual bool hasQueuedMsgs() const;
        virtual void setTimeout(size_t) {}
        virtual size_t getTimeout() const { return (0); };

        void setThrowSessionError(bool flag);
        void setThrowSessionTimeout(bool flag);

        void setMessage(ConstElementPtr msg) { msg_ = msg; }

        ConstElementPtr sent_msg;
        string msg_destination;
    private:
        ConstElementPtr msg_;
        bool throw_session_error_;
        bool throw_session_timeout_;
    };

protected:
    CountersTest() : counters() {
        counters.setStatisticsSession(&statistics_session_);
    }
    ~CountersTest() {
    }
    MockSession statistics_session_;
    Counters counters;
    // no need to be inherited from the original class here.
    class MockModuleSpec {
    public:
        bool validateStatistics(ConstElementPtr, const bool valid) const
            { return (valid); }
    };
    MockModuleSpec module_spec_;
};

void
CountersTest::MockSession::establish(const char*) {}

void
CountersTest::MockSession::disconnect() {}

void
CountersTest::MockSession::subscribe(string, string)
{}

void
CountersTest::MockSession::unsubscribe(string, string)
{}

void
CountersTest::MockSession::startRead(boost::function<void()>)
{}

int
CountersTest::MockSession::reply(ConstElementPtr, ConstElementPtr) {
    return (-1);
}

bool
CountersTest::MockSession::hasQueuedMsgs() const {
    return (false);
}

int
CountersTest::MockSession::group_sendmsg(ConstElementPtr msg,
                                              string group, string, string)
{
    if (throw_session_error_) {
        isc_throw(SessionError, "Session Error");
    }
    sent_msg = msg;
    msg_destination = group;
    return (0);
}

bool
CountersTest::MockSession::group_recvmsg(ConstElementPtr&,
                                              ConstElementPtr& msg, bool, int)
{
    if (throw_session_timeout_) {
        isc_throw(SessionTimeout, "Session Timeout");
    }
    msg = msg_;
    return (true);
}

void
CountersTest::MockSession::setThrowSessionError(bool flag) {
    throw_session_error_ = flag;
}

void
CountersTest::MockSession::setThrowSessionTimeout(bool flag) {
    throw_session_timeout_ = flag;
}

TEST_F(CountersTest, submitStatisticsWithoutSession) {
    // Set statistics_session to NULL and call submitStatistics().
    // Expect to return false.
    counters.setStatisticsSession(NULL);
    EXPECT_FALSE(counters.submitStatistics());
}

TEST_F(CountersTest, submitStatisticsWithException) {
    // Exception SessionError and SessionTimeout will be thrown
    // while sending statistics data.
    // Both expect to return false.
    statistics_session_.setThrowSessionError(true);
    EXPECT_FALSE(counters.submitStatistics());
    statistics_session_.setThrowSessionError(false);
    statistics_session_.setThrowSessionTimeout(true);
    EXPECT_FALSE(counters.submitStatistics());
    statistics_session_.setThrowSessionTimeout(false);
}

void
opcodeDataCheck(ConstElementPtr data, const int expected[16]) {
    const char* item_names[] = {
        "query", "iquery", "other", "reserved3", "notify", "update",
        "other", "other", "other", "other", "other",
        "other", "other", "other", "other", "other",
        NULL
    };
    int i;
    for (i = 0; i < 16; ++i) {
        ASSERT_NE(static_cast<const char*>(NULL), item_names[i]);
        const string item_name = "opcode." + string(item_names[i]);
        if (expected[i] == 0) {
            EXPECT_FALSE(data->get(item_name));
        } else {
            EXPECT_EQ(expected[i], data->get(item_name)->intValue());
        }
    }
    // We should have examined all names
    ASSERT_EQ(static_cast<const char*>(NULL), item_names[i]);
}

void
rcodeDataCheck(ConstElementPtr data, const int expected[17]) {
    const char* item_names[] = {
        "noerror", "formerr", "servfail", "nxdomain", "notimp", "refused",
        "yxdomain", "yxrrset", "nxrrset", "notauth", "notzone", "other",
        "other", "other", "other", "other", "badsigvers",
        NULL
    };
    int i;
    for (i = 0; i < 17; ++i) {
        ASSERT_NE(static_cast<const char*>(NULL), item_names[i]);
        const string item_name = "rcode." + string(item_names[i]);
        if (expected[i] == 0) {
            EXPECT_FALSE(data->get(item_name));
        } else {
            EXPECT_EQ(expected[i], data->get(item_name)->intValue());
        }
    }
    // We should have examined all names
    ASSERT_EQ(static_cast<const char*>(NULL), item_names[i]);
}

TEST_F(CountersTest, submitStatisticsWithoutValidator) {
    // Submit statistics data.
    // Validate if it submits correct data.

    counters.submitStatistics();

    // Destination is "Stats".
    EXPECT_EQ("Stats", statistics_session_.msg_destination);
    // Command is "set".
    EXPECT_EQ("set", statistics_session_.sent_msg->get("command")
                         ->get(0)->stringValue());
    EXPECT_EQ("Auth", statistics_session_.sent_msg->get("command")
                         ->get(1)->get("owner")->stringValue());
    EXPECT_EQ(statistics_session_.sent_msg->get("command")
              ->get(1)->get("pid")->intValue(), getpid());
    ConstElementPtr statistics_data = statistics_session_.sent_msg
                                          ->get("command")->get(1)
                                          ->get("data");

    // By default these counters are all 0
    EXPECT_EQ(0, statistics_data->get("queries.udp")->intValue());
    EXPECT_EQ(0, statistics_data->get("queries.tcp")->intValue());

    // By default opcode counters are all 0 and omitted
    const int opcode_results[16] = { 0, 0, 0, 0, 0, 0, 0, 0,
                                     0, 0, 0, 0, 0, 0, 0, 0 };
    opcodeDataCheck(statistics_data, opcode_results);
    // By default rcode counters are all 0 and omitted
    const int rcode_results[17] = { 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                    0, 0, 0, 0, 0, 0, 0, 0 };
    rcodeDataCheck(statistics_data, rcode_results);
}

TEST_F(CountersTest, submitStatisticsWithValidator) {

    //a validator for the unittest
    Counters::validator_type validator;
    ConstElementPtr el;

    // Submit statistics data with correct statistics validator.
    validator = boost::bind(
        &CountersTest::MockModuleSpec::validateStatistics,
        &module_spec_, _1, true);

    EXPECT_TRUE(validator(el));

    // register validator to AuthCounters
    counters.registerStatisticsValidator(validator);

    // checks the value returned by submitStatistics
    EXPECT_TRUE(counters.submitStatistics());

    // Destination is "Stats".
    EXPECT_EQ("Stats", statistics_session_.msg_destination);
    // Command is "set".
    EXPECT_EQ("set", statistics_session_.sent_msg->get("command")
                         ->get(0)->stringValue());
    EXPECT_EQ("Auth", statistics_session_.sent_msg->get("command")
                         ->get(1)->get("owner")->stringValue());
    ConstElementPtr statistics_data = statistics_session_.sent_msg
                                          ->get("command")->get(1)
                                          ->get("data");

    // By default these counters are all 0
    EXPECT_EQ(0, statistics_data->get("queries.udp")->intValue());
    EXPECT_EQ(0, statistics_data->get("queries.tcp")->intValue());

    // Submit statistics data with incorrect statistics validator.
    validator = boost::bind(
        &CountersTest::MockModuleSpec::validateStatistics,
        &module_spec_, _1, false);

    EXPECT_FALSE(validator(el));

    counters.registerStatisticsValidator(validator);

    // checks the value returned by submitStatistics
    EXPECT_FALSE(counters.submitStatistics());
}

bool checkCountersAllZeroExcept(const Counters::ItemTreeType counters,
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

    Counters::ItemNodeNameSetType names;

    // get "auth.server.qr"
    names.insert("auth.server.qr");
    counters.get(names)->getValue(stats_map);
    EXPECT_EQ(QR_COUNTER_TYPES, stats_map.size());
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
