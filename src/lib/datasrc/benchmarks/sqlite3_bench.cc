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

#include <sqlite3.h>

#include <bench/benchmark.h>

#include <dns/name.h>
#include <dns/labelsequence.h>
#include <dns/messagerenderer.h>
#include <dns/rrtype.h>
#include <dns/rrttl.h>
#include <dns/rdata.h>
#include <dns/rrset.h>

#include <datasrc/database.h>

#include <cassert>
#include <string>
#include <vector>

using std::string;
using namespace isc::dns;
using namespace isc::dns::rdata;
using namespace isc::datasrc;
using namespace isc::bench;

namespace {
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

struct RRParam {
    RRParam(uint32_t ttl0, const RRType& rrtype0, const char* rdata0) :
        ttl(ttl0), rrtype(rrtype0), rdata(rdata0 != NULL ?
                                          string(rdata0) : string())
    {}
    uint32_t ttl;
    RRType rrtype;
    string rdata;
};

class SQLite3BenchMark {
public:
    SQLite3BenchMark(sqlite3* db, const char* name_txt) :
        db_(db), name_(name_txt)
    {
        assert(sqlite3_prepare_v2(db, get_record_stmt, -1, &record_stmt_,
                                  NULL) == SQLITE_OK);
        assert(sqlite3_prepare_v2(db_, get_zone_stmt, -1, &zone_stmt_, NULL)
               == SQLITE_OK);
        assert(sqlite3_prepare_v2(db_, get_newzone_stmt, -1, &newzone_stmt_,
                                  NULL) == SQLITE_OK);
        assert(sqlite3_prepare_v2(db_, get_types_stmt, -1, &types_stmt_, NULL)
               == SQLITE_OK);
    }
    int getZoneNew() {
        sqlite3_reset(newzone_stmt_);

        const string rname_txt = name_.reverse().toText();
        assert(sqlite3_bind_int(newzone_stmt_, 1, RRClass::IN().getCode()) ==
               SQLITE_OK);
        assert(sqlite3_bind_text(newzone_stmt_, 2, rname_txt.c_str(),
                                     -1, SQLITE_STATIC) == SQLITE_OK);
        const int rc = sqlite3_step(newzone_stmt_);
        if (rc == SQLITE_ROW) {
            const int ret_id = sqlite3_column_int(newzone_stmt_, 0);
            const Name zname(convertToPlainChar(
                                 sqlite3_column_text(
                                     newzone_stmt_, 1)));
            const NameComparisonResult::NameRelation rel =
                name_.compare(zname).getRelation();
            if (rel != NameComparisonResult::EQUAL &&
                rel != NameComparisonResult::SUBDOMAIN) {
                assert(false);
            }
            sqlite3_reset(newzone_stmt_);
            return (ret_id);
        }
        assert(false);
    }
    int getZone() {
        const size_t label_count = name_.getLabelCount();
        LabelSequence labels(name_);
        for (size_t i = 0; i < label_count; ++i, labels.stripLeft(1)) {
            sqlite3_reset(zone_stmt_);
            const string labels_txt = labels.toText();
            assert(sqlite3_bind_text(zone_stmt_, 1, labels_txt.c_str(),
                                     -1, SQLITE_STATIC) == SQLITE_OK);
            const int rc = sqlite3_step(zone_stmt_);
            if (rc == SQLITE_ROW) {
                const int ret_id = sqlite3_column_int(zone_stmt_, 0);
                sqlite3_reset(zone_stmt_);
                return (ret_id);
            }
        }
        assert(false);
    }
    unsigned int run() {
        const int zone_id = getZoneNew();
        assert(sqlite3_bind_int(record_stmt_, 1, zone_id) == SQLITE_OK);
        const string& name_txt = name_.toText();
        assert(sqlite3_bind_text(record_stmt_, 2, name_txt.c_str(), -1,
                                 SQLITE_STATIC) == SQLITE_OK);
        results_.clear();
        renderer_.clear();

        while (true) {
            const int rc = sqlite3_step(record_stmt_);
            if (rc == SQLITE_DONE) {
                sqlite3_reset(record_stmt_);
                break;
            } else if (rc == SQLITE_ROW) {
                // heuristics
                const RRType rrtype(convertToPlainChar(
                        sqlite3_column_text(
                            record_stmt_,
                            DatabaseAccessor::TYPE_COLUMN)));
                const char* rdata_cp =
                    (rrtype == RRType::CNAME()) ?
                    convertToPlainChar(
                        sqlite3_column_text(
                            record_stmt_,
                            DatabaseAccessor::RDATA_COLUMN)) : NULL;
                results_.push_back(
                    RRParam(sqlite3_column_int(record_stmt_,
                                               DatabaseAccessor::TTL_COLUMN),
                            rrtype, rdata_cp));
            } else {
                assert(false);
            }
        }

        LabelSequence labels(name_);
        labels.stripLeft(1);
        const string labels_txt = labels.toText();
        assert(sqlite3_bind_int(types_stmt_, 1, zone_id) == SQLITE_OK);
        assert(sqlite3_bind_text(types_stmt_, 2, labels_txt.c_str(), -1,
                                 SQLITE_STATIC) == SQLITE_OK);
        while (true) {
            const int rc = sqlite3_step(types_stmt_);
            if (rc == SQLITE_DONE) {
                sqlite3_reset(types_stmt_);
                break;
            } else if (rc == SQLITE_ROW) {
                sqlite3_column_int(types_stmt_, 0);
            } else {
                assert(false);
            }
        }
        
        std::vector<RRParam>::const_iterator it_end = results_.end();
        for (std::vector<RRParam>::const_iterator it = results_.begin();
             it != it_end;
             ++it) {
            if (it->rrtype == RRType::CNAME()) {
                const RRTTL rrttl(it->ttl);
                ConstRdataPtr rdata = createRdata(it->rrtype, RRClass::IN(),
                                                  it->rdata);
                BasicRRset rrset(name_, RRClass::IN(), it->rrtype, rrttl);
                rrset.addRdata(rdata);
                rrset.toWire(renderer_);
                //std::cout << rrset << std::endl;
            }
        }
        return (1);
    }
private:
    sqlite3* db_;
    const Name name_;
    sqlite3_stmt* zone_stmt_;
    sqlite3_stmt* newzone_stmt_;
    sqlite3_stmt* record_stmt_;
    sqlite3_stmt* types_stmt_;
    MessageRenderer renderer_;
    std::vector<RRParam> results_;
    string type_;
    string ttl_;
    string rdata_;
};

void
usage() {
    std::cerr << "Usage: sqlite3_bench [-n iterations]" << std::endl;
    exit (1);
}
}

int
main(int argc, char* argv[]) {
    int ch;
    int iteration = 100000;
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
    if (argc != 2) {
        usage();
    }

    sqlite3* db = NULL;
    assert(sqlite3_open(argv[0], &db) == 0);
    assert(db != NULL);
    std::cout << "Benchmark for RdataReader" << std::endl;
    BenchMark<SQLite3BenchMark>(iteration, SQLite3BenchMark(db, argv[1]));

    return (0);
}
