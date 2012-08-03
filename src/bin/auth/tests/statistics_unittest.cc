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

using isc::UnitTestUtil;
using namespace std;
using namespace isc::cc;
using namespace isc::dns;
using namespace isc::data;
using isc::auth::statistics::Counters;

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
        "query", "iquery", "status", "reserved3", "notify", "update",
        "reserved6", "reserved7", "reserved8", "reserved9", "reserved10",
        "reserved11", "reserved12", "reserved13", "reserved14", "reserved15",
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
        "yxdomain", "yxrrset", "nxrrset", "notauth", "notzone", "reserved11",
        "reserved12", "reserved13", "reserved14", "reserved15", "badvers",
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
