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

#include <stdint.h>

#include <string>
#include <vector>

#include <dns/name.h>
#include <dns/rrclass.h>
#include <dns/rrset.h>
#include <dns/rrtype.h>
#include <dns/rrttl.h>

#include "rbt_datasrc.h"

using namespace std;
using namespace isc::dns;

struct RRsetParams {
    const char* owner;
    const char* rrclass;
    const char* rrtype;
    const uint32_t rrttl;
};

struct RdataParams {
    unsigned int rrset_index;
    const char* rrtype;
    const char* rrclass;
    const char* rdata;
};

namespace {
#include "rootdb"
}

const RbtDataSrc*
createRootRbtDataSrc() {
    RbtDataSrc* datasrc = new RbtDataSrc(Name::ROOT_NAME());
    RRsetPtr rrset;
    int i, j;

    for (i = 0, j = 0; i < n_rrsets; ++i) {
        rrset = RRsetPtr(new RRset(Name(rrset_params_list[i].owner),
                                   RRClass(rrset_params_list[i].rrclass),
                                   RRType(rrset_params_list[i].rrtype),
                                   RRTTL(rrset_params_list[i].rrttl)));
        for (; j < n_rdata && rdata_params_list[j].rrset_index == i; j++) {
            rrset->addRdata(rdata::createRdata(
                                RRType(rdata_params_list[j].rrtype),
                                RRClass(rdata_params_list[j].rrclass),
                                rdata_params_list[j].rdata));
        }
        datasrc->addRRset(*rrset);
    }

    return (datasrc);
}
