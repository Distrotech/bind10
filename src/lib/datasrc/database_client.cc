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
#include <vector>

#include <boost/scoped_ptr.hpp>

#include <dns/name.h>
#include <dns/rrtype.h>
#include <dns/rrclass.h>
#include <dns/rrset.h>
#include <dns/rdata.h>
#include <dns/rrsetlist.h>
#include <dns/rrttl.h>

#include <cc/data.h>

#include <datasrc/data_source.h>
#include <datasrc/client.h>
#include <datasrc/database_client.h>
#include <datasrc/zone.h>
#include <datasrc/sqlite3_datasrc.h>
#include <datasrc/mysql_conn.h>

using namespace std;
using namespace boost;
using namespace isc::data;
using namespace isc::dns;
using namespace isc::dns::rdata;

namespace isc {
namespace datasrc {
class SQLite3Connection : public DataBaseConnection {
public:
    SQLite3Connection(ConstElementPtr param) {
        sqlite3_src_.init(param);
    }

    virtual ~SQLite3Connection() {}

    virtual DataSrc::Result getZone(const string& name, int& zone_id) const {
        return (sqlite3_src_.getZone(name, zone_id));
    }

    virtual void searchForRecords(int zone_id, const string& name,
                                  bool match_subdomain)
    {
        sqlite3_src_.searchForRecords(zone_id, name, match_subdomain);
    }

    virtual void traverseZone(int zone_id) {
        sqlite3_src_.traverseZone(zone_id);
    }

    virtual DataSrc::Result getNextRecord(vector<string>& columns) {
        return (sqlite3_src_.getNextRecord(columns));
    }

    virtual string getPreviousName(int zone_id, const string& name) const {
        return (sqlite3_src_.getPreviousName(zone_id, name));
    }

    virtual void startUpdateZoneTransaction(int zone_id, bool replace) {
        sqlite3_src_.startUpdateZoneTransaction(zone_id, replace);
    }

    virtual void addRecordToZone(int zone_id,
                                 const vector<string>& record_params)
    {
        sqlite3_src_.addRecordToZone(zone_id, record_params);
    }

    virtual void commitUpdateZoneTransaction() {
        sqlite3_src_.commitUpdateZoneTransaction();
    }

    virtual void resetQuery() {
        sqlite3_src_.resetQuery();
    }

private:
    // In this initial experiment, we use the current sqlite3 datasrc
    Sqlite3DataSrc sqlite3_src_;
};

class DataBaseZoneHandle : public ZoneHandle {
public:
    DataBaseZoneHandle(const DataBaseDataSourceClient& client,
                       DataBaseConnection& conn,
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
    DataBaseConnection& conn_;
    const Name origin_;
    const int id_;
};

namespace {
DataBaseConnection*
dbconnFactory(ConstElementPtr db_param) {
    if (db_param->contains("type")) {
        const string dbtype = db_param->get("type")->stringValue();
        if (dbtype == "sqlite3") {
            return (new SQLite3Connection(db_param));
        } else if (dbtype == "mysql") {
            return (new MySQLConnection());
        }
    }
    isc_throw(DataSourceError, "database type unknown or unspecified");
}

class DataBaseZoneIterator : public ZoneIterator {
public:
    DataBaseZoneIterator(const Name& zone_name, ConstElementPtr conn_param) :
        conn_(dbconnFactory(conn_param))
    {
        int zone_id;
        conn_->getZone(zone_name.toText(), zone_id);
        conn_->traverseZone(zone_id);
    }
    virtual ~DataBaseZoneIterator() {
    }
    ConstRRsetPtr getNextRRset() {
        vector<string> columns;
        const DataSrc::Result result = conn_->getNextRecord(columns);
        if (result != DataSrc::SUCCESS) {
            return (RRsetPtr());
        }
        assert(columns.size() == 4); // should actually be exception
        const RRType rrtype(columns[1]);
        RRsetPtr rrset(new RRset(Name(columns[0]), RRClass::IN(),
                                 rrtype, RRTTL(columns[2])));
        rrset->addRdata(createRdata(rrtype, RRClass::IN(), columns[3]));
        return (rrset);
    }
private:
    boost::scoped_ptr<DataBaseConnection> conn_;
};

class DataBaseZoneUpdater : public ZoneUpdater {
public:
    DataBaseZoneUpdater(const Name& zone_name, DataBaseConnection& conn,
                        bool replace) :
        conn_(conn)
    {
        conn_.getZone(zone_name.toText(), zone_id_);
        conn_.startUpdateZoneTransaction(zone_id_, replace);
    }

    virtual void addRRset(const isc::dns::RRset& rrset) {
        vector<string> record_params;
        record_params.push_back(rrset.getName().toText());
        record_params.push_back(rrset.getName().reverse().toText());
        record_params.push_back(rrset.getTTL().toText());
        record_params.push_back(rrset.getType().toText());
        // don't do RRSIG specific process for simplicity
        record_params.push_back("");
        record_params.push_back(""); // placeholder
        for (RdataIteratorPtr it = rrset.getRdataIterator();
             !it->isLast();
             it->next()) {
            record_params.back() = it->getCurrent().toText();
            conn_.addRecordToZone(zone_id_, record_params);
        }
    }

    virtual void commit() {
        conn_.commitUpdateZoneTransaction();
    }
private:
    int zone_id_;
    DataBaseConnection& conn_;
};

typedef map<RRType, RRsetPtr> RRsetMap;
typedef RRsetMap::value_type RRsetMapEntry;

bool
getRRsets(DataBaseConnection& conn, int zone_id, const Name& name,
          RRClass rrclass, RRsetMap& target, int& rows)
{
    bool found = false;

    vector<string> columns;
    conn.searchForRecords(zone_id, name.toText());
    rows = 0;
    while (true) {
        const DataSrc::Result result = conn.getNextRecord(columns);
        if (result != DataSrc::SUCCESS) {
            break;
        }

        assert(columns.size() == 4); // should actually be exception
        ++rows;
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

bool
isEmptyNodeName(DataBaseConnection& conn, int zone_id, const Name& name) {
    vector<string> columns;
    conn.searchForRecords(zone_id, name.toText(), true);
    const bool result = (conn.getNextRecord(columns) == DataSrc::SUCCESS);
    conn.resetQuery();
    return (result);
}
}

ZoneHandle::FindResult
DataBaseZoneHandle::find(const Name& name, const RRType& type,
                         RRsetList*, const FindOptions options) const
{
    RRsetMap rrsets;
    RRsetPtr nsrrset;
    RRsetPtr rrset;
    int match_rrs;

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
                          rrsets, match_rrs)) {
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

    rrsets.clear();
    rrsets.insert(RRsetMapEntry(type, RRsetPtr()));
    rrsets.insert(RRsetMapEntry(RRType::CNAME(), RRsetPtr()));
    if (getRRsets(conn_, id_, name, RRClass::IN(), rrsets, match_rrs)) {
        if ((rrset = rrsets[type])) {
            return (FindResult(SUCCESS, rrset));
        } else if ((rrset = rrsets[RRType::CNAME()])) {
            return (FindResult(CNAME, rrset));
        }
    }
    if (match_rrs == 0) {
        // There isn't even any type of RR of the name.  It's either NXRRSET
        // if it's an empty non terminal or NXDOMAIN.
        if (isEmptyNodeName(conn_, id_, name)) {
            return (FindResult(NXRRSET, ConstRRsetPtr()));
        }
        return (FindResult(NXDOMAIN, ConstRRsetPtr()));
    }
    return (FindResult(NXRRSET, ConstRRsetPtr()));
}

DataBaseDataSourceClient::DataBaseDataSourceClient() : conn_(NULL) {
}

DataBaseDataSourceClient::~DataBaseDataSourceClient() {
    delete conn_;
}

void
DataBaseDataSourceClient::open(ConstElementPtr db_param) {
    db_param_ = db_param;
    conn_ = dbconnFactory(db_param);
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
                                                 *this, *conn_,
                                                 matchname, zone_id))));
        }
    }
    return (FindResult(result::NOTFOUND, ZoneHandlePtr()));
}

ZoneIteratorPtr
DataBaseDataSourceClient::createZoneIterator(const isc::dns::Name& name)
    const
{
    return (ZoneIteratorPtr(new DataBaseZoneIterator(name, db_param_)));
}

ZoneUpdaterPtr
DataBaseDataSourceClient::startUpdateZone(const Name& zone_name, bool replace)
{
    return (ZoneUpdaterPtr(new DataBaseZoneUpdater(zone_name, *conn_,
                                                   replace)));
}
}
}
