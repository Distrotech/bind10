// Copyright (C) 2013  Internet Systems Consortium, Inc. ("ISC")
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

#include <bench/benchmark.h>
#include <bench/benchmark_util.h>

#include <util/buffer.h>

#include <dns/message.h>
#include <dns/name.h>
#include <dns/question.h>
#include <dns/rrclass.h>

#include <log/logger_support.h>

#include <util/unittests/mock_socketsession.h>

#include <asiodns/asiodns.h>
#include <asiolink/asiolink.h>

#include <boost/shared_ptr.hpp>

#include <cstdlib>
#include <iostream>
#include <vector>

namespace {
const int ITERATION_DEFAULT = 1;

void
usage() {
    std::cerr << "Usage: query_bench [-d] [-n iterations]"
        "  -d Enable debug logging to stdout\n"
        "  -n Number of iterations per test case (default: "
              << ITERATION_DEFAULT << ")\n"
        "  query_datafile: queryperf style input data"
              << std::endl;
    exit (1);
}
}

int
main(int argc, char* argv[]) {
    int ch;
    int iteration = ITERATION_DEFAULT;
    bool debug_log = false;
    while ((ch = getopt(argc, argv, "dn:")) != -1) {
        switch (ch) {
        case 'n':
            iteration = atoi(optarg);
            break;
        case 'd':
            debug_log = true;
            break;
        default:
            usage();
        }
    }
    argc -= optind;
    argv += optind;
    if (argc < 1) {
        usage();
    }

    // By default disable logging to avoid unwanted noise.
    initLogger("query-bench", debug_log ? isc::log::DEBUG : isc::log::NONE,
               isc::log::MAX_DEBUG_LEVEL, NULL);

    return (0);
}
