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

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include <iostream>
#include <vector>

#include <bench/benchmark.h>
#include <bench/benchmark_util.h>

#include <dns/buffer.h>
#include <dns/message.h>
#include <dns/messagerenderer.h>
#include <dns/name.h>
#include <dns/question.h>
#include <dns/rrclass.h>

#include <auth/auth_srv.h>
#include <auth/rbt_datasrc.h>
#include <auth/loadzone.h>

using namespace std;
using namespace isc;
using namespace isc::dns;
using namespace isc::bench;

namespace {
class QueryBenchMark {
public:
    QueryBenchMark(AuthSrv& server, const BenchQueries& queries) :
        server_(server), queries_(queries), query_message_(Message::PARSE),
        obuffer_(4096)
        //, renderer_(obuffer_)
        , renderer_(obuffer_, &offset_table_)
        , null_fd_(-1)
    {
        null_fd_ = open("/dev/null", O_RDWR);
        if (null_fd_ < 0) {
            isc_throw(Exception, "failed to open output file");
        }
    }
    ~QueryBenchMark() {
        if (null_fd_ >= 0) {
            close(null_fd_);
        }
    }
    unsigned int run() {
        BenchQueries::const_iterator query;
        const BenchQueries::const_iterator query_end = queries_.end();
        for (query = queries_.begin(); query != query_end; ++query) {
            InputBuffer buffer(&(*query)[0], (*query).size());
            query_message_.clear(Message::PARSE);
            renderer_.clear();
            server_.processMessage(buffer, query_message_, renderer_, true);
            //write(null_fd_, obuffer_.getData(), obuffer_.getLength());
        }

        return (queries_.size());
    }
private:
    AuthSrv& server_;
    const BenchQueries& queries_;
    Message query_message_;
    OutputBuffer obuffer_;
    MessageRenderer renderer_;
    CompressOffsetTable offset_table_;
    int null_fd_;
};
}

namespace isc {
namespace bench {
template<>
void
BenchMark<QueryBenchMark>::printResult() const {
    cout.precision(6);
    cout << "Processed " << getIteration() << " queries in "
         << fixed << getDuration() << "s";
    cout.precision(2);
    cout << " (" << fixed << getIterationPerSecond() << "qps)" << endl;
}
}
}

namespace {
void
usage() {
    cerr << "Usage: query_bench [-n iterations] zone_file "
        "zone_origin query_datafile" << endl;
    exit (1);
}
}

int
main(int argc, char* argv[]) {
    int ch;
    int iteration = 1;
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
    if (argc < 3) {
        usage();
    }
    const char* const zone_file = argv[0];
    const char* const origin = argv[1];
    const char* const query_data_file = argv[2];

    // Prepare the database
    RbtDataSrc* datasrc = new RbtDataSrc(Name(origin));
    loadZoneFile(zone_file, datasrc);

    // Load queries
    BenchQueries queries;
    loadQueryData(query_data_file, queries, RRClass::IN());

    // Create server object.
    // The current experimental implementation depends on some environment
    // variables, so we configure them before constructing a new server.
    setenv("ZONEFILE", zone_file, 1);
    setenv("DBORIGIN", origin, 1);
    AuthSrv* auth_server = new AuthSrv;

    // Perform benchmark test and dump the result
    QueryBenchMark query_bench(*auth_server, queries);
    BenchMark<QueryBenchMark> bench(iteration, query_bench);
    bench.run();

    delete auth_server;

    return (0);
}
