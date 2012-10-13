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

#include <config.h>

#include <log/logger_support.h>

#include <iostream>

#include <unistd.h>

namespace {
void
usage() {
    std::cerr <<
        "Usage: b10-memmgr [-d] [-h] mmap_file"
        "  -d Enable debug logging to stdout\n"
        "  -h Print this message\n"
              << std::endl;
    exit (1);
}
}

int
main(int argc, char* argv[]) {
    int ch;
    bool debug_log = false;

    while ((ch = getopt(argc, argv, "dh")) != -1) {
        switch (ch) {
        case 'd':
            debug_log = true;
            break;
        case 'h':
        default:
            usage();
        }
    }
    argc -= optind;
    argv += optind;
    if (argc < 2) {
        usage();
    }

    initLogger("b10-memmgr", debug_log ? isc::log::DEBUG : isc::log::NONE,
               isc::log::MAX_DEBUG_LEVEL, NULL);

    return (0);
}
