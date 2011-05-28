// Copyright (C) 2011  Internet Systems Consortium, Inc. ("ISC")
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

#include <gtest/gtest.h>

#include <dns/name.h>

#include <datasrc/database_client.h>

using namespace std;
using namespace isc::dns;
using namespace isc::datasrc;

namespace {
const char* const SQLITE_DBFILE_EXAMPLE = TEST_DATA_DIR "/test.sqlite3";

class DataBaseDataSourceClientTest : public ::testing::Test {
protected:
    DataBaseDataSourceClientTest() {
        db_client.open(SQLITE_DBFILE_EXAMPLE);
    }

    DataBaseDataSourceClient db_client;
};

TEST_F(DataBaseDataSourceClientTest, findZone) {
    EXPECT_EQ(result::SUCCESS, db_client.findZone(Name("example.com")).code);
    EXPECT_EQ(Name("example.com"),
              db_client.findZone(Name("example.com")).zone->getOrigin());
    EXPECT_EQ(Name("example.com"),
              db_client.findZone(Name("EXAMPLE.com")).zone->getOrigin());
    EXPECT_EQ(result::SUCCESS,
              db_client.findZone(Name("sql1.example.com")).code);
    EXPECT_EQ(Name("sql1.example.com"),
              db_client.findZone(Name("sql1.example.com")).zone->getOrigin());
    EXPECT_EQ(result::SUCCESS,
              db_client.findZone(Name("foo.example.com")).code);
    EXPECT_EQ(Name("example.com"),
              db_client.findZone(Name("foo.example.com")).zone->getOrigin());
    EXPECT_EQ(result::NOTFOUND, db_client.findZone(Name("com")).code);
    EXPECT_FALSE(db_client.findZone(Name("com")).zone);
}

TEST_F(DataBaseDataSourceClientTest, find) {
    ZoneHandlePtr zone = db_client.findZone(Name("example.com")).zone;

    // normal successful case
    EXPECT_EQ(ZoneHandle::SUCCESS, zone->find(Name("example.com"),
                                              RRType::SOA()).code);

    // delegation with NS.  if glue is okay, search continues under a zone cut
    EXPECT_EQ(ZoneHandle::DELEGATION,
              zone->find(Name("www.subzone.example.com"), RRType::A()).code);
    EXPECT_EQ(ZoneHandle::DELEGATION,
              zone->find(Name("ns1.subzone.example.com"), RRType::A()).code);
    EXPECT_EQ(ZoneHandle::SUCCESS,
              zone->find(Name("ns1.subzone.example.com"),
                         RRType::A(), NULL, ZoneHandle::FIND_GLUE_OK).code);

    // delegation with DNAME
    EXPECT_EQ(ZoneHandle::DNAME,
              zone->find(Name("www.dname.example.com"), RRType::A()).code);

    // Exact-matching names exist.  Including CNAME case.
    EXPECT_EQ(ZoneHandle::SUCCESS,
              zone->find(Name("www.example.com"), RRType::A()).code);
    EXPECT_EQ(ZoneHandle::NXRRSET,
              zone->find(Name("www.example.com"), RRType::AAAA()).code);
    EXPECT_EQ(ZoneHandle::CNAME,
              zone->find(Name("foo.example.com"), RRType::AAAA()).code);
    EXPECT_EQ(ZoneHandle::SUCCESS,
              zone->find(Name("foo.example.com"), RRType::CNAME()).code);
}

}
