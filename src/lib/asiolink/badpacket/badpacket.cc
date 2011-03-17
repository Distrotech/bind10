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

#include <unistd.h>

#include <cstddef>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include <boost/lexical_cast.hpp>

#include <config.h>

#include <asiolink/io_fetch.h>
#include <dns/message.h>
#include <log/strutil.h>

#include "header_flags_scan.h"

using namespace std;
using namespace asiolink;
using namespace isc::dns;
using namespace isc::test;

/// \brief Send Bad Packets
///
/// Sends a sequence of bad packets to the target nameserver and prints the
/// packet settings and the response received.
///
/// Really, it should be "potentially bad packets".  The program treats the
/// flags field in the header as an unsigned 16-bit number, and cycles through
/// all combination of bits, sending a query for "www.example.com".  The result
/// is then printed.
///
/// The principle raison d'etre for this is to check if the combination of bits
/// will cause a crash.

/// \brief Print usage information
void
Usage() {
    cout << "Usage: badpacket [-a <address>] [-f<start>[-<end>]] [-p <port>]\n"
            "                 [-h] [-v]\n"
            "\n"
            "Sends a sequence of packets to the specified nameserver and\n"
            "prints the results.\n"
            "\n"
            "-a<address>      Address of nameserver.  Defaults to 127.0.0.1.\n"
            "-f<start>[-<end>]\n"
            "                 Do a flags scan.  This treats the packet header\n"
            "                 flags as a 16-bit number and cycles through all\n"
            "                 values in the range <start> to <end>. (If only\n"
            "                 a single value is given, only that value is\n"
            "                 tested.) Default to 0-65535.\n"
            "-h               Print the help message and exits.\n"
            "-p<port>         Port to which to send query.  Defaults to 53.\n"
            "-t<timeout>      Timeout value in ms.  Default is 500\n"
            "-v               Prints version information and exits\n";
}

/// \brief Print version information
void
Version() {
    cout << "1.0-0\n";
}

/// \brief Parse -f flag
///
/// The "-f" flag should be of the form "-f<value>" or "-f<val1>-<val2>".
/// This function checks that value and returns the start/end values (both the
/// same if only one value were specified).  On an error it will output a
/// message and terminate thge program.
///
/// \param ftext String passed to the parameter
///
/// \return pair of uint16_t holding the start and end values.
pair<uint16_t, uint16_t>
getRange(const char* ftext) {

    // Split the string up into one or two tokens
    vector<string> values = isc::strutil::tokens(string(ftext), "-");
    if ((values.size() < 1) || (values.size() > 2)) {
        cerr << "Error: format of flags scan parameter is 'v1' or 'v1-v2'\n";
        exit(1);
    }

    // Convert to uint16.
    uint16_t start = boost::lexical_cast<uint16_t>(values[0]);
    uint16_t end = start;
    if (values.size() == 2) {
        end = boost::lexical_cast<uint16_t>(values[1]);
    }

    return make_pair(start, end);
}


/// \brief Main Program
int main(int argc, char* argv[]) {
    pair<uint16_t, uint16_t> range(0, 65535);   // Range of values to check
    string  address = "127.0.0.1";              // Address of nameserver
    uint16_t port = 53;                         // Port of nameserver
    int timeout = 500;                          // Query timeout in ms

    char    c;                          // Option being processed

    while ((c = getopt(argc, argv, "a:f:hp:t:v")) != -1) {
        switch (c) {
            case 'a':
                address = optarg;
                break;

            case 'f':
                range = getRange(optarg);
                break;

            case 'h':
                Usage();
                exit(0);

            case 'p':
                port = boost::lexical_cast<uint16_t>(string(optarg));
                break;

            case 't':
                timeout = boost::lexical_cast<int>(string(optarg));
                break;

            case 'v':
                Version();
                exit(0);

            default:
                cerr << "Error: unknown option " << c << "\n";
                exit(1);
        }
    }

    // Construct the scan object
    HeaderFlagsScan scanner(address, port, timeout, range.first, range.second);

    // Now perform the scan
    scanner.scan("www.example.com");

    return 0;
}
