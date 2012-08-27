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

#include <dns/rrclass.h>

#include "mysql_accessor.h"
#include "database.h"

#include <string>

using namespace isc::dns;
using namespace isc::data;

namespace isc {
namespace datasrc {

DataSourceClient *
createInstance(isc::data::ConstElementPtr, std::string& error) {
    ElementPtr errors(Element::createList());
    try {
        boost::shared_ptr<DatabaseAccessor> mysql_accessor(
            new MySQLAccessor("IN")); // XXX: avoid hardcode RR class
        return (new DatabaseClient(isc::dns::RRClass::IN(), mysql_accessor));
    } catch (const std::exception& exc) {
        error = std::string("Error creating MySQL datasource: ") +
            exc.what();
        return (NULL);
    } catch (...) {
        error = std::string("Error creating mysql datasource, "
                            "unknown exception");
        return (NULL);
    }
}

void destroyInstance(DataSourceClient* instance) {
    delete instance;
}

} // end of namespace datasrc
} // end of namespace isc
