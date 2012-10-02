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

#include <libpq-fe.h>

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

const char* const GETZONE_STATEMENT = "getzone";
const char* const ANY_QUERY_STATEMENT = "any_query";

struct RRParam {
    RRParam(uint32_t ttl0, const RRType& rrtype0, const char* rdata0) :
        ttl(ttl0), rrtype(rrtype0), rdata(rdata0 != NULL ?
                                          string(rdata0) : string())
    {}
    uint32_t ttl;
    RRType rrtype;
    string rdata;
};

class PostgresBenchMark {
public:
    PostgresBenchMark(PGconn* pgconn, const char* name_txt) :
        pgconn_(pgconn), name_(name_txt)
    {}
    int getZone() {
        // Prepare query parameters
        const char* params[1];
        const int param_formats[1] = {0}; // it's a text parameter
        const string rname = name_.reverse().toText();
        params[0] = rname.c_str();

        PGresult* result = PQexecPrepared(pgconn_, GETZONE_STATEMENT, 1, params,
                                          NULL, param_formats, 0);
        if (result == NULL || PQresultStatus(result) != PGRES_TUPLES_OK) {
            if (result != NULL) {
                PQclear(result);
            }
            std::cerr << "SQL statement failed " << GETZONE_STATEMENT
                      << std::endl;
            assert(false);
        }

        if (PQntuples(result) == 1 && PQnfields(result) == 2) {
            const int zone_id = atoi(PQgetvalue(result, 0, 0));
            const Name zname(PQgetvalue(result, 0, 1));
            PQclear(result);
            const NameComparisonResult::NameRelation rel =
                name_.compare(zname).getRelation();
            if (rel != NameComparisonResult::EQUAL &&
                rel != NameComparisonResult::SUBDOMAIN) {
                assert(false);
            }

            return (zone_id);
        }
        PQclear(result);
        assert(false);
    }
    unsigned int run() {
        const int zone_id = getZone();
        assert(zone_id != -1);

#if 1
        const char* params[2];
        const int param_formats[2] = {0, 0}; // they are text parameters
        params[0] = lexical_cast<string>(zone_id).c_str();
        const string name_txt = name_.toText();
        params[1] = name_txt.c_str();

        PGresult* result = PQexecPrepared(pgconn_, ANY_QUERY_STATEMENT, 2,
                                          params, NULL, param_formats, 0);
        if (result == NULL || PQresultStatus(result) != PGRES_TUPLES_OK) {
            if (result != NULL) {
                PQclear(result);
            }
            std::cerr << "SQL statement failed " << ANY_QUERY_STATEMENT
                      << std::endl;
        }
        const size_t n_tuples = PQntuples(result);
        for (size_t i = 0; i < n_tuples; ++i) {
            assert(PQnfields(result) == 3);
            //std::cout << PQgetvalue(result, i, 2) << std::endl; // print RDATA
        }
        PQclear(result);
#endif

        return (1);
    }
private:
    PGconn* pgconn_;
    const Name name_;
    MessageRenderer renderer_;
    std::vector<RRParam> results_;
    string type_;
    string ttl_;
    string rdata_;
};

void
usage() {
    std::cerr << "Usage: postgres_bench [-n iterations] qname" << std::endl;
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

    const char* const postgres_conn_info = "host = 127.0.0.1 dbname = bind "
        "password = bind port = 5432";
    PGconn* pgconn;
    if ((pgconn = PQconnectdb(postgres_conn_info)) == NULL ||
        PQstatus(pgconn) != CONNECTION_OK) {
        if (pgconn != NULL) {
            PQfinish(pgconn);
        }
        std::cout << "failed to connect to PostgreSQL DB" << std::endl;
        return (1);
    }
    const char* const getzone_query_txt =
        "SELECT id, name FROM zones WHERE class=1 AND rname<=$1 "
        "ORDER BY rname DESC LIMIT 1;";
    PGresult* result =
        PQprepare(pgconn, GETZONE_STATEMENT, getzone_query_txt, 1, NULL);
    assert(result != NULL && PQresultStatus(result) == PGRES_COMMAND_OK);
    PQclear(result);

    const char* const any_query_txt =
        "SELECT typecode, ttl, rdata FROM records WHERE zone_id=$1 "
        "AND name=$2;";
    result = PQprepare(pgconn, ANY_QUERY_STATEMENT, any_query_txt, 2, NULL);
    assert(result != NULL && PQresultStatus(result) == PGRES_COMMAND_OK);
    PQclear(result);

    try {
        std::cout << "Benchmark with PostgreSQL DB" << std::endl;
        BenchMark<PostgresBenchMark>(iteration, PostgresBenchMark(pgconn,
                                                                  argv[0]));
    } catch (const std::exception& ex) {
        std::cout << "unexpected failure: " << ex.what() << std::endl;
        return (1);
    }
    PQfinish(pgconn);

    return (0);
}
