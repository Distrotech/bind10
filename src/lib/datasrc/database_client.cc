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

#include <string>

#include <dns/name.h>
#include <dns/rrtype.h>
#include <dns/rrclass.h>
#include <dns/rrset.h>
#include <dns/rrsetlist.h>

#include <datasrc/client.h>
#include <datasrc/database_client.h>
#include <datasrc/zone.h>
#include <datasrc/sqlite3_datasrc.h>

using namespace std;
using namespace isc::dns;

namespace isc {
namespace datasrc {
class DataBaseConnection {
protected:
    DataBaseConnection() {}
public:
    virtual ~DataBaseConnection() {}
    virtual DataSrc::Result findZone(const string& name,
                                     int& zone_id) const = 0;
};

class SQLite3Connection : public DataBaseConnection {
public:
    SQLite3Connection(const string& db_file) {
        sqlite3_src_.init(db_file);
    }
    virtual ~SQLite3Connection() {}
    virtual DataSrc::Result findZone(const string& name, int& zone_id) const {
        return (sqlite3_src_.findZone(name, zone_id));
    }
private:
    // In this initial experiment, we use the current sqlite3 datasrc
    Sqlite3DataSrc sqlite3_src_;
};

class DataBaseZoneHandle : public ZoneHandle {
public:
    DataBaseZoneHandle(const DataBaseDataSourceClient& client,
                       const Name& origin, int id) :
        client_(client), origin_(origin), id_(id)
    {}
    virtual const Name& getOrigin() const { return (origin_); }
    virtual const RRClass& getClass() const { return (RRClass::IN()); }
    virtual FindResult find(const Name& name, const RRType& type,
                            RRsetList* target,
                            const FindOptions options) const;

private:
    const DataBaseDataSourceClient& client_;
    const Name origin_;
    const int id_;
};

ZoneHandle::FindResult
DataBaseZoneHandle::find(const Name&, const RRType&,
                         RRsetList*, const FindOptions) const
{
    return (FindResult(SUCCESS, ConstRRsetPtr()));
}

DataBaseDataSourceClient::DataBaseDataSourceClient() : conn_(NULL) {
}

void
DataBaseDataSourceClient::open(const string& param) {
    conn_ = new SQLite3Connection(param);
}

DataSourceClient::FindResult
DataBaseDataSourceClient::findZone(const isc::dns::Name& name) const {
    // derived the logic of Sqlite3DataSrc::findClosest
    DataSrc::Result result = DataSrc::ERROR;
    const unsigned int nlabels = name.getLabelCount();
    for (unsigned int i = 0; i < nlabels; ++i) {
        const Name matchname(name.split(i));
        int zone_id;
        result = conn_->findZone(matchname.toText(), zone_id);
        if (result == DataSrc::SUCCESS) {
            return (FindResult(result::SUCCESS,
                               ZoneHandlePtr(new DataBaseZoneHandle(
                                                 *this, matchname,
                                                 zone_id))));
        }
    }
    return (FindResult(result::NOTFOUND, ZoneHandlePtr()));
}
}
}
