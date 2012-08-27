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

#include <bench/benchmark.h>

#include <dns/name.h>
#include <dns/labelsequence.h>
#include <dns/messagerenderer.h>
#include <dns/rrtype.h>
#include <dns/rrttl.h>
#include <dns/rdata.h>
#include <dns/rrset.h>

#include <datasrc/database.h>

#include <mysql.h>

#include <boost/lexical_cast.hpp>

#include <stdexcept>
#include <cassert>
#include <string>
#include <sstream>
#include <vector>

using std::string;
using boost::lexical_cast;
using namespace isc::dns;
using namespace isc::dns::rdata;
using namespace isc::datasrc;
using namespace isc::bench;

namespace {
#if 0
const char*
convertToPlainChar(const unsigned char* ucp) {
    const void* p = ucp;
    return (static_cast<const char*>(p));
}

const char* const get_zone_stmt =
    "SELECT id FROM zones WHERE name=?1";
const char* const get_newzone_stmt =
    "SELECT id, name FROM newzones WHERE class=?1 and rname<?2 "
    "order by rname desc limit 1";
const char* const get_record_stmt =
    "SELECT rdtype, ttl, sigtype, rdata FROM records "
    "WHERE zone_id=?1 AND name=?2";
const char* const get_types_stmt =
    "SELECT ttl FROM records WHERE zone_id=?1 AND name=?2";
    //"SELECT rdtype FROM records WHERE zone_id=?1 AND name=?2";
#endif

struct RRParam {
    RRParam(uint32_t ttl0, const RRType& rrtype0, const char* rdata0) :
        ttl(ttl0), rrtype(rrtype0), rdata(rdata0 != NULL ?
                                          string(rdata0) : string())
    {}
    uint32_t ttl;
    RRType rrtype;
    string rdata;
};

class MySQLBenchMark {
public:
    MySQLBenchMark(MYSQL* mysql, const char* name_txt) :
        mysql_(mysql), name_(name_txt),
        getzone_query1_(
            "SELECT id, name FROM zones WHERE class=1 AND rname<='"),
        getzone_query2_("' ORDER BY rname DESC LIMIT 1"),
        any_query1_("SELECT typecode, ttl, rdata FROM records WHERE zone_id="),
        any_query2_(" AND name='")
    {}
    int getZone() {
        const string query_txt = getzone_query1_ +
            name_.reverse().toText() + getzone_query2_;

        MYSQL_RES* res;
        if (mysql_query(mysql_, query_txt.c_str()) != 0 ||
            (res = mysql_store_result(mysql_)) == NULL) {
            std::cerr << "SQL statement failed " << query_txt << std::endl;
            assert(false);
        }

        const int rows = mysql_num_rows(res);
        MYSQL_ROW row;
        if (rows == 1 && (row = mysql_fetch_row(res)) != NULL &&
            row[0] != NULL) {
            const int zone_id = atoi(row[0]);
            const Name zname(row[1]);
            const NameComparisonResult::NameRelation rel =
                name_.compare(zname).getRelation();
            if (rel != NameComparisonResult::EQUAL &&
                rel != NameComparisonResult::SUBDOMAIN) {
                assert(false);
            }
            mysql_free_result(res);
            return (zone_id);
        }
        assert(false);
    }
    unsigned int run() {
        const int zone_id = getZone();

        string query_txt(any_query1_);
        query_txt += lexical_cast<string>(zone_id);
        query_txt += any_query2_;
        query_txt += name_.toText();
        query_txt.push_back('\'');

        MYSQL_RES* current_res_;
        if (mysql_query(mysql_, query_txt.c_str()) != 0 ||
            //(current_res_ = mysql_store_result(mysql_)) == NULL)
            (current_res_ = mysql_use_result(mysql_)) == NULL)
        {
            std::cerr << "SQL statement failed " << query_txt << std::endl;
            assert(false);
        }

        MYSQL_ROW row;
        while ((row = mysql_fetch_row(current_res_)) != NULL) {
            assert(mysql_num_fields(current_res_) == 3);
            //std::cout << row[2] << std::endl; // print RDATA
        }
        
        mysql_free_result(current_res_);

        return (1);
    }
private:
    MYSQL* mysql_;
    const Name name_;
    const string getzone_query1_;
    const string getzone_query2_;
    const string any_query1_;
    const string any_query2_;
    MessageRenderer renderer_;
    std::vector<RRParam> results_;
    string type_;
    string ttl_;
    string rdata_;
};

void
usage() {
    std::cerr << "Usage: mysql_bench [-n iterations] qname" << std::endl;
    exit (1);
}
}

int
main(int argc, char* argv[]) {
    int ch;
    int iteration = 10000;
    while ((ch = getopt(argc, argv, "n:")) != -1) {
        switch (ch) {
        case 'n':
            iteration = atoi(optarg);
            break;
        case '?':
        default:
            usage();
        }
    }
    argc -= optind;
    argv += optind;
    if (argc != 1) {
        usage();
    }

    MYSQL* mysql = mysql_init(NULL);
    assert(mysql != NULL);
    if (mysql_real_connect(mysql, "localhost", "bind", "bind", "bind", 0,
                           NULL, 0) == NULL) {
        mysql_close(mysql);
        std::cout << "failed to connect to MySQL DB" << std::endl;
        return (1);
    }
    try {
        std::cout << "Benchmark with MySQL DB" << std::endl;
        BenchMark<MySQLBenchMark>(iteration, MySQLBenchMark(mysql, argv[0]));
    } catch (const std::exception& ex) {
        std::cout << "unexpected failure: " << ex.what() << std::endl;
        return (1);
    }

    return (0);
}
