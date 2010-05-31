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

#include <fstream>
#include <string>
#include <sstream>

#include <exceptions/exceptions.h>

#include <dns/name.h>
#include <dns/rrclass.h>
#include <dns/rrset.h>
#include <dns/rrtype.h>
#include <dns/rrttl.h>

#include "rbt_datasrc.h"

using namespace std;
using namespace isc;
using namespace isc::dns;

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

