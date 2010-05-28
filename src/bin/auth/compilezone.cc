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

#include <sys/mman.h>
#include <fcntl.h>

#include <stdlib.h>
#include <unistd.h>

#include <fstream>
#include <iostream>
#include <string>

#include <exceptions/exceptions.h>

#include <dns/name.h>
#include <dns/rdata.h>
#include <dns/rrclass.h>
#include <dns/rrtype.h>
#include <dns/rrttl.h>
#include <dns/rrset.h>

#include "rbt_datasrc.h"

using namespace std;
using namespace isc;
using namespace isc::dns;

namespace {
static const size_t PAGE_SIZE = 4096; // XXX not always true

void
loadZoneFile(const char* const zone_file, RbtDataSrc* datasrc) {
    ifstream ifs;

    ifs.open(zone_file, ios_base::in);
    if ((ifs.rdstate() & istream::failbit) != 0) {
        isc_throw(Exception, "failed to open zone file: " + string(zone_file));
    }

    string line;
    RRsetPtr rrset;
    const Name* prev_owner = NULL;
    const RRType* prev_rrtype = NULL;
    while (getline(ifs, line), !ifs.eof()) {
        if (ifs.bad() || ifs.fail()) {
            isc_throw(Exception, "Unexpected line in zone file");
        }
        if (line.empty() || line[0] == ';') {
            continue;           // skip comment and blank lines
        }

        istringstream iss(line);
        string owner, ttl, rrclass, rrtype;
        stringbuf rdatabuf;
        iss >> owner >> ttl >> rrclass >> rrtype >> &rdatabuf;
        if (iss.bad() || iss.fail()) {
            isc_throw(Exception, "Invalid/unrecognized RR: " << line);
        }
        if (prev_owner == NULL || *prev_owner != Name(owner) ||
            *prev_rrtype != RRType(rrtype)) {
            if (rrset) {
                datasrc->addRRset(*rrset);
            }
            rrset = RRsetPtr(new RRset(Name(owner), RRClass(rrclass),
                                       RRType(rrtype), RRTTL(ttl)));
        }
        rrset->addRdata(rdata::createRdata(RRType(rrtype), RRClass(rrclass),
                                           rdatabuf.str()));
        prev_owner = &rrset->getName();
        prev_rrtype = &rrset->getType();
    }
    if (rrset) {
        datasrc->addRRset(*rrset);
    }
}

void
usage() {
    cout << "Usage: b10-compilezone -f output_file -o origin zone_file" << endl;
    exit(1);
}
}

int
main(int argc, char* argv[]) {
    const char* output_file = NULL;
    const char* origin_name = NULL;
    int ch;

    while ((ch = getopt(argc, argv, "f:o:")) != -1) {
        switch (ch) {
        case 'f':
            output_file = optarg;
            break;
        case 'o':
            origin_name = optarg;
            break;
        case '?':
        default:
            usage();
        }
    }

    if (argc - optind != 1 || output_file == NULL || origin_name == NULL) {
        usage();
    }
    argv += optind;

    const char* const zone_file = argv[0];

    // Phase 1: load the text based zone file to estimate necessary memory
    const Name origin(origin_name);
    RbtDataSrc* datasrc = new RbtDataSrc(origin);
    loadZoneFile(zone_file, datasrc);
    size_t db_memory_size = datasrc->getAllocatedMemorySize(); 
    cout << "Zone load completed, allocated memory: " << db_memory_size
         << " bytes" << endl;
    delete datasrc;

    // Phase 2: open the DB file, make room for the data if necessary.
    // We reserve 50% larger than the estimated size with rounding it up to
    // a multiple of "page size".
    db_memory_size += db_memory_size/2;
    db_memory_size = ((db_memory_size + PAGE_SIZE - 1) & (~(PAGE_SIZE - 1))); 
    fstream dbfile(output_file, ios::out|ios::binary|ios::ate);
    if (dbfile.is_open()) {
        fstream::pos_type current_size = dbfile.tellp();
        cout << "Opened DB file, current size: " << current_size << endl;

        if (db_memory_size > current_size) {
            db_memory_size -= current_size;
            char* newblock = new char [db_memory_size];
            dbfile.write(newblock, db_memory_size);
            cout << "Added " << db_memory_size << " bytes" << endl;
        }

        // Write the (possibly) updated file size
        dbfile.seekp(0);
        uint64_t dbsize = db_memory_size;
        dbfile.write(reinterpret_cast<const char*>(&dbsize), sizeof(dbsize));

        dbfile.close();
    } else {
        isc_throw(Exception, "Failed to open DB file");
    }

    // Phase 3: re-load the zone into the DB file.
    datasrc = new RbtDataSrc(origin, *output_file, RbtDataSrc::LOAD);
    loadZoneFile(zone_file, datasrc);
    cout << "Write load completed, data size: "
         << datasrc->getAllocatedMemorySize() << " bytes" << endl;
    delete datasrc;

    return (0);
}
