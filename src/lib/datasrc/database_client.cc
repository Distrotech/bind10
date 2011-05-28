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
#include <map>

#include <boost/foreach.hpp>

#include <dns/name.h>
#include <dns/rrtype.h>
#include <dns/rrclass.h>
#include <dns/rrset.h>
#include <dns/rdata.h>
#include <dns/rrsetlist.h>
#include <dns/rrttl.h>

#include <datasrc/client.h>
#include <datasrc/database_client.h>
#include <datasrc/zone.h>
#include <datasrc/sqlite3_datasrc.h>

using namespace std;
using namespace isc::dns;
using namespace isc::dns::rdata;

namespace isc {
namespace datasrc {
class DataBaseConnection {
protected:
    DataBaseConnection() {}
public:
    virtual ~DataBaseConnection() {}
    virtual DataSrc::Result getZone(const string& name,
                                    int& zone_id) const = 0;
    virtual void searchForRecords(int zone_id, const string& name) const = 0;
    virtual DataSrc::Result getNextRecord(vector<string>& columns) const = 0;
};

class SQLite3Connection : public DataBaseConnection {
public:
    SQLite3Connection(const string& db_file) {
        sqlite3_src_.init(db_file);
    }

    virtual ~SQLite3Connection() {}

    virtual DataSrc::Result getZone(const string& name, int& zone_id) const {
        return (sqlite3_src_.getZone(name, zone_id));
    }

    virtual void searchForRecords(int zone_id, const string& name) const {
        sqlite3_src_.searchForRecords(zone_id, name);
    }

    virtual DataSrc::Result getNextRecord(vector<string>& columns) const {
        return (sqlite3_src_.getNextRecord(columns));
    }

#ifdef notyet
    virtual DataSrc::Result searchForRecords(const string& name,
                                             const string& type) const {
    }
#endif
private:
    // In this initial experiment, we use the current sqlite3 datasrc
    Sqlite3DataSrc sqlite3_src_;
};

class DataBaseZoneHandle : public ZoneHandle {
public:
    DataBaseZoneHandle(const DataBaseDataSourceClient& client,
                       const DataBaseConnection& conn,
                       const Name& origin, int id) :
        client_(client), conn_(conn), origin_(origin), id_(id)
    {}
    virtual const Name& getOrigin() const { return (origin_); }
    virtual const RRClass& getClass() const { return (RRClass::IN()); }
    virtual FindResult find(const Name& name, const RRType& type,
                            RRsetList* target,
                            const FindOptions options) const;

private:
    const DataBaseDataSourceClient& client_;
    const DataBaseConnection& conn_;
    const Name origin_;
    const int id_;
};

namespace {
typedef map<RRType, RRsetPtr> RRsetMap;
typedef RRsetMap::value_type RRsetMapEntry;

bool
getRRsets(const DataBaseConnection& conn, int zone_id, const Name& name,
          RRClass rrclass, RRsetMap& target)
{
    bool found = false;

    vector<string> columns;
    conn.searchForRecords(zone_id, name.toText());
    while (true) {
        const DataSrc::Result result = conn.getNextRecord(columns);
        if (result != DataSrc::SUCCESS) {
            break;
        }

        assert(columns.size() == 4); // should actually be exception
        const RRType rrtype(columns[0]);
        if (target.find(rrtype) != target.end()) {
            found = true;
            RRsetPtr rrset = target[rrtype];
            const RRTTL rrttl(columns[1]);
            if (!rrset) {
                rrset = RRsetPtr(new RRset(name, rrclass, rrtype, rrttl));
                target[rrtype] = rrset;
            }
            if (rrttl < rrset->getTTL()) {
                rrset->setTTL(rrttl);
            }
            rrset->addRdata(createRdata(rrtype, rrclass, columns[3]));
        }
    }

    return (found);
}
}

ZoneHandle::FindResult
DataBaseZoneHandle::find(const Name& name, const RRType& type,
                         RRsetList*, const FindOptions options) const
{
    RRsetMap rrsets;
    RRsetPtr nsrrset;
    RRsetPtr rrset;

    // Match downward, from the zone apex to the query name, looking for
    // referrals.  Note that we exclude the apex name and query name
    // themselves; they'll be handled in a normal lookup in the zone.
    const int diff = name.getLabelCount() - origin_.getLabelCount();
    if (diff > 1) {
        for (int i = diff - 1; i > 0; --i) {
            const Name sub(name.split(i));
            rrsets.insert(RRsetMapEntry(RRType::NS(), RRsetPtr()));
            rrsets.insert(RRsetMapEntry(RRType::DNAME(), RRsetPtr()));
            if (getRRsets(conn_, id_, sub,
                          RRClass::IN(), // experimentally faked
                          rrsets)) {
                if ((rrset = rrsets[RRType::DNAME()])) {
                    return (FindResult(DNAME, rrset));
                }

                if ((nsrrset = rrsets[RRType::NS()])) {
                    if ((options & FIND_GLUE_OK) == 0) {
                        return (FindResult(DELEGATION, nsrrset));
                    }
                    // If glues are ok, remember the NS of the highest zonecut
                    // and seek an exact match.
                    break;
                }
            }
        }
    }

    // wildcard, NXRRSET/NXDOMAIN cases

    rrsets.clear();
    rrsets.insert(RRsetMapEntry(type, RRsetPtr()));
    rrsets.insert(RRsetMapEntry(RRType::CNAME(), RRsetPtr()));
    if (getRRsets(conn_, id_, name, RRClass::IN(), rrsets)) {
        if ((rrset = rrsets[type])) {
            return (FindResult(SUCCESS, rrset));
        } else if ((rrset = rrsets[RRType::CNAME()])) {
            return (FindResult(CNAME, rrset));
        }
    }

    return (FindResult(NXRRSET, ConstRRsetPtr()));
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
        result = conn_->getZone(matchname.toText(), zone_id);
        if (result == DataSrc::SUCCESS) {
            return (FindResult(result::SUCCESS,
                               ZoneHandlePtr(new DataBaseZoneHandle(
                                                 *this, *this->conn_,
                                                 matchname, zone_id))));
        }
    }
    return (FindResult(result::NOTFOUND, ZoneHandlePtr()));
}
}
}
