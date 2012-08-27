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

#include <cc/data.h>

#include <log/logger_support.h>

#include <dns/name.h>
#include <dns/masterload.h>
#include <dns/rrset.h>

#include <datasrc/client.h>
#include <datasrc/factory.h>

#include <stdexcept>
#include <iostream>

using namespace isc::datasrc;
using namespace isc::data;
using namespace isc::dns;

namespace {
void
usage() {
    std::cerr << "Usage: datasrc_loader datasrc_type zone_name zone_file"
              << std::endl;
    exit (1);
}

class Loader {
public:
    Loader(ZoneUpdaterPtr updater) : updater_(updater) {}
    void operator()(ConstRRsetPtr rrset) {
        updater_->addRRset(*rrset);
    }
private:
    ZoneUpdaterPtr updater_;
};
}

int
main(int argc, char* argv[]) {
    int ch;
    while ((ch = getopt(argc, argv, "h:")) != -1) {
        switch (ch) {
        case 'h':
        default:
            usage();
        }
    }
    argc -= optind;
    argv += optind;
    if (argc != 3) {
        usage();
    }

    try {
        isc::log::initLogger();

        const Name zone_name(argv[1]);

        DataSourceClientContainerPtr client_ctnr(
            new DataSourceClientContainer(argv[0], ConstElementPtr()));
        ZoneUpdaterPtr updater =
            client_ctnr->getInstance().getUpdater(zone_name, true);
        
        masterLoad(argv[2], zone_name, RRClass::IN(), Loader(updater));
        updater->commit();
    } catch (const std::exception& ex) {
        std::cerr << "Unexpected failure: " << ex.what() << std::endl;
        return (1);
    }

    return (0);
}
