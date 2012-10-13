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

#include <exceptions/exceptions.h>

#include <dns/name.h>
#include <dns/rrclass.h>

#include <util/memory_segment.h>
#include <util/memory_segment_mmap.h>

#include <log/logger_support.h>

#include <datasrc/memory/zone_data.h>
#include <datasrc/memory/zone_data_loader.h>
#include <datasrc/memory/zone_table.h>

#include <iostream>

#include <unistd.h>

using namespace isc::dns;
using namespace isc::datasrc::memory;
using isc::util::MemorySegment;
using isc::util::MemorySegmentMmap;

namespace {
ZoneTable*
getZoneTable(MemorySegment& mem_sgmt, RRClass rrclass, bool create) {
    ZoneTable* zone_table;
    if (create) {
        zone_table = ZoneTable::create(mem_sgmt, rrclass);
        mem_sgmt.setNamedAddress("zone_table", zone_table);
    } else {
        zone_table = static_cast<ZoneTable*>(
            mem_sgmt.getNamedAddress("zone_table"));
        if (zone_table == NULL) {
            isc_throw(isc::Unexpected,
                      "Can't find zone table, likely a broken memory image");
        }
    }
    return (zone_table);
}

ZoneData*
getZoneData(MemorySegment& mem_sgmt, RRClass zone_class, const Name& zone_name,
            const char* const zone_file)
{
    return (loadZoneData(mem_sgmt, zone_class, zone_name, zone_file));
}

void
usage() {
    std::cerr <<
        "Usage: b10-memmgr [-c] [-d] [-h] [i] mmap_file zone_name zone_file\n"
        "  -i initialize (existing file content will be destroyed)\n"
        "  -c RR class of the zones (default: IN)\n"
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
    bool do_initialize = false;
    const char* rrclass_txt = "IN";

    while ((ch = getopt(argc, argv, "c:idh")) != -1) {
        switch (ch) {
        case 'c':
            rrclass_txt = optarg;
            break;
        case 'd':
            debug_log = true;
            break;
        case 'i':
            do_initialize = true;
            break;
        case 'h':
        default:
            usage();
        }
    }
    argc -= optind;
    argv += optind;
    if (argc < 3) {
        usage();
    }
    const char* const mmap_file = argv[0];
    const char* const zone_name_txt = argv[1];
    const char* const zone_file = argv[2];

    try {
        const RRClass rrclass(rrclass_txt);
        const Name zone_name(zone_name_txt);

        initLogger("b10-memmgr", debug_log ? isc::log::DEBUG : isc::log::NONE,
                   isc::log::MAX_DEBUG_LEVEL, NULL);
        if (do_initialize) {
            unlink(mmap_file);
        }
        MemorySegmentMmap mem_sgmt(mmap_file, do_initialize, 1024 * 1024);
        ZoneTable* zone_table = getZoneTable(mem_sgmt, rrclass, do_initialize);
        assert(zone_table != NULL);
        ZoneData* zone_data = getZoneData(mem_sgmt, rrclass, zone_name,
                                          zone_file);
        ZoneTable::AddResult result =
            zone_table->addZone(mem_sgmt, rrclass, zone_name, zone_data);
        if (result.code == isc::datasrc::result::EXIST) {
            std::cout << "existing zone data found, destroyed." << std::endl;
            ZoneData::destroy(mem_sgmt, result.zone_data, rrclass);
        }
    } catch (const std::exception& ex) {
        std::cerr << "unexpected error: " << ex.what() << std::endl;
        return (1);
    }

    return (0);
}
