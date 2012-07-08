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

#define ISC_LIBDATASRC_EXPORT

#include <config.h>

#include <string>
#include <sstream>
#include <utility>

#include <sqlite3.h>

#include <datasrc/sqlite3_datasrc.h>
#include <datasrc/logger.h>
#include <exceptions/exceptions.h>
#include <dns/rrttl.h>
#include <dns/rdata.h>
#include <dns/rdataclass.h>
#include <dns/rrset.h>
#include <dns/rrsetlist.h>

namespace {
// Expected schema.  The major version must match else there is an error.  If
// the minor version of the database is less than this, a warning is output.
//
// It is assumed that a program written to run on m.n of the database will run
// with a database version m.p, where p is any number.  However, if p < n,
// we assume that the database structure was upgraded for some reason, and that
// some advantage may result if the database is upgraded. Conversely, if p > n,
// The database is at a later version than the program was written for and the
// program may not be taking advantage of features (possibly performance
// improvements) added to the database.
const int SQLITE_SCHEMA_MAJOR_VERSION = 2;
const int SQLITE_SCHEMA_MINOR_VERSION = 0;
}

using namespace std;
using namespace isc::dns;
using namespace isc::dns::rdata;

namespace isc {
namespace datasrc {

struct ISC_LIBDATASRC_API Sqlite3Parameters {
    Sqlite3Parameters() :  db_(NULL), major_version_(-1), minor_version_(-1),
        q_zone_(NULL), q_record_(NULL), q_addrs_(NULL), q_referral_(NULL),
        q_any_(NULL), q_count_(NULL), q_previous_(NULL), q_nsec3_(NULL),
        q_prevnsec3_(NULL)
    {}
    sqlite3* db_;
    int major_version_;
    int minor_version_;
    sqlite3_stmt* q_zone_;
    sqlite3_stmt* q_record_;
    sqlite3_stmt* q_addrs_;
    sqlite3_stmt* q_referral_;
    sqlite3_stmt* q_any_;
    sqlite3_stmt* q_count_;
    sqlite3_stmt* q_previous_;
    sqlite3_stmt* q_nsec3_;
    sqlite3_stmt* q_prevnsec3_;
};

namespace {
const char* const SCHEMA_LIST[] = {
    "CREATE TABLE schema_version (version INTEGER NOT NULL, "
        "minor INTEGER NOT NULL DEFAULT 0)",
    "INSERT INTO schema_version VALUES (2, 0)",
    "CREATE TABLE zones (id INTEGER PRIMARY KEY, "
    "name TEXT NOT NULL COLLATE NOCASE, "
    "rdclass TEXT NOT NULL COLLATE NOCASE DEFAULT 'IN', "
    "dnssec BOOLEAN NOT NULL DEFAULT 0)",
    "CREATE INDEX zones_byname ON zones (name)",
    "CREATE TABLE records (id INTEGER PRIMARY KEY, "
    "zone_id INTEGER NOT NULL, name TEXT NOT NULL COLLATE NOCASE, "
    "rname TEXT NOT NULL COLLATE NOCASE, ttl INTEGER NOT NULL, "
    "rdtype TEXT NOT NULL COLLATE NOCASE, sigtype TEXT COLLATE NOCASE, "
    "rdata TEXT NOT NULL)",
    "CREATE INDEX records_byname ON records (name)",
    "CREATE INDEX records_byrname ON records (rname)",
    "CREATE INDEX records_bytype_and_rname ON records (rdtype, rname)",
    "CREATE TABLE nsec3 (id INTEGER PRIMARY KEY, zone_id INTEGER NOT NULL, "
    "hash TEXT NOT NULL COLLATE NOCASE, "
    "owner TEXT NOT NULL COLLATE NOCASE, "
    "ttl INTEGER NOT NULL, rdtype TEXT NOT NULL COLLATE NOCASE, "
    "rdata TEXT NOT NULL)",
    "CREATE INDEX nsec3_byhash ON nsec3 (hash)",
    "CREATE TABLE diffs (id INTEGER PRIMARY KEY, "
        "zone_id INTEGER NOT NULL, "
        "version INTEGER NOT NULL, "
        "operation INTEGER NOT NULL, "
        "name TEXT NOT NULL COLLATE NOCASE, "
        "rrtype TEXT NOT NULL COLLATE NOCASE, "
        "ttl INTEGER NOT NULL, "
        "rdata TEXT NOT NULL)",
    NULL
};

const char* const q_version_str = "SELECT version FROM schema_version";
const char* const q_minor_str = "SELECT minor FROM schema_version";

const char* const q_zone_str = "SELECT id FROM zones WHERE name=?1";

const char* const q_record_str = "SELECT rdtype, ttl, sigtype, rdata "
    "FROM records WHERE zone_id=?1 AND name=?2 AND "
    "((rdtype=?3 OR sigtype=?3) OR "
    "(rdtype='CNAME' OR sigtype='CNAME') OR "
    "(rdtype='NS' OR sigtype='NS'))";

const char* const q_addrs_str = "SELECT rdtype, ttl, sigtype, rdata "
    "FROM records WHERE zone_id=?1 AND name=?2 AND "
    "(rdtype='A' OR sigtype='A' OR rdtype='AAAA' OR sigtype='AAAA')";

const char* const q_referral_str = "SELECT rdtype, ttl, sigtype, rdata FROM "
    "records WHERE zone_id=?1 AND name=?2 AND"
    "(rdtype='NS' OR sigtype='NS' OR rdtype='DS' OR sigtype='DS' OR "
    "rdtype='DNAME' OR sigtype='DNAME')";

const char* const q_any_str = "SELECT rdtype, ttl, sigtype, rdata "
    "FROM records WHERE zone_id=?1 AND name=?2";

// Note: the wildcard symbol '%' is expected to be added to the text
// for the placeholder for LIKE given via sqlite3_bind_text().  We don't
// use the expression such as (?2 || '%') because it would disable the use
// of indices and could result in terrible performance.
const char* const q_count_str = "SELECT COUNT(*) FROM records "
    "WHERE zone_id=?1 AND rname LIKE ?2;";

const char* const q_previous_str = "SELECT name FROM records "
    "WHERE rname < ?2 AND zone_id=?1 AND rdtype = 'NSEC' "
    "ORDER BY rname DESC LIMIT 1";

const char* const q_nsec3_str = "SELECT rdtype, ttl, rdata FROM nsec3 "
    "WHERE zone_id = ?1 AND hash = $2";

const char* const q_prevnsec3_str = "SELECT hash FROM nsec3 "
    "WHERE zone_id = ?1 AND hash <= $2 ORDER BY hash DESC LIMIT 1";

}

//
//  Find the exact zone match.  Return -1 if not found, or the zone's
//  ID if found.  This will always be >= 0 if found.
//
int
Sqlite3DataSrc::hasExactZone(const char* const name) const {
    int rc;

    sqlite3_reset(dbparameters->q_zone_);
    rc = sqlite3_bind_text(dbparameters->q_zone_, 1, name, -1, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        isc_throw(Sqlite3Error, "Could not bind " << name <<
                  " to SQL statement (zone)");
    }

    rc = sqlite3_step(dbparameters->q_zone_);
    const int i = (rc == SQLITE_ROW) ?
        sqlite3_column_int(dbparameters->q_zone_, 0) : -1; 
    sqlite3_reset(dbparameters->q_zone_);
    return (i);
}

namespace {
int
importSqlite3Rows(sqlite3_stmt* query, const Name& qname, const RRClass& qclass,
                  const RRType& qtype, const bool nsec3_tree,
                  RRsetList& result_sets, uint32_t& flags)
{
    int rows = 0;
    int rc = sqlite3_step(query);
    const bool qtype_is_any = (qtype == RRType::ANY());

    while (rc == SQLITE_ROW) {
        const char* type = (const char*)sqlite3_column_text(query, 0);
        int ttl = sqlite3_column_int(query, 1);
        const char* sigtype = NULL;
        const char* rdata;

        if (nsec3_tree) {
            rdata = (const char*)sqlite3_column_text(query, 2);
            if (RRType(type) == RRType::RRSIG()) {
                sigtype = "NSEC3";
            }
        } else {
            sigtype = (const char*)sqlite3_column_text(query, 2);
            rdata = (const char*)sqlite3_column_text(query, 3);
        }

        const RRType base_rrtype(sigtype != NULL ? sigtype : type);

        // found an NS; we need to inform the caller that this might be a
        // referral, but we do not return the NS RRset to the caller
        // unless asked for it.
        if (base_rrtype == RRType::NS()) {
            flags |= DataSrc::REFERRAL;
            if (!qtype_is_any && qtype != RRType::NS()) {
                rc = sqlite3_step(query);
                continue;
            }
        }

        ++rows;

        // Looking for something else but found CNAME
        if (base_rrtype == RRType::CNAME() && qtype != RRType::CNAME()) {
            if (qtype == RRType::NSEC()) {
                // NSEC query, just skip the CNAME
                rc = sqlite3_step(query);
                continue;
            } else if (!qtype_is_any) {
                // include the CNAME, but don't flag it for chasing if
                // this is an ANY query
                flags |= DataSrc::CNAME_FOUND;
            }
        }

        RRsetPtr rrset = result_sets.findRRset(base_rrtype, qclass);
        if (rrset == NULL) {
            rrset = RRsetPtr(new RRset(qname, qclass, base_rrtype, RRTTL(ttl)));
            result_sets.addRRset(rrset);
        }

        if (sigtype == NULL && base_rrtype == rrset->getType()) {
            rrset->addRdata(createRdata(rrset->getType(), qclass, rdata));
            if (ttl > rrset->getTTL().getValue()) {
                rrset->setTTL(RRTTL(ttl));
            }
        } else if (sigtype != NULL && base_rrtype == rrset->getType()) {
            RdataPtr rrsig = createRdata(RRType::RRSIG(), qclass, rdata);
            if (rrset->getRRsig()) {
                rrset->getRRsig()->addRdata(rrsig);
            } else {
                RRsetPtr sigs = RRsetPtr(new RRset(qname, qclass,
                                                   RRType::RRSIG(),
                                                   RRTTL(ttl)));
                sigs->addRdata(rrsig);
                rrset->addRRsig(sigs);
            }

            if (ttl > rrset->getRRsig()->getTTL().getValue()) {
                rrset->getRRsig()->setTTL(RRTTL(ttl));
            }
        }

        rc = sqlite3_step(query);
    }

    return (rows);
}
}

int
Sqlite3DataSrc::findRecords(const Name& name, const RRType& rdtype,
                            RRsetList& target, const Name* zonename,
                            const Mode mode, uint32_t& flags) const
{
    LOG_DEBUG(logger, DBG_TRACE_DETAILED, DATASRC_SQLITE_FINDREC).arg(name).
        arg(rdtype);
    flags = 0;
    int zone_id = (zonename == NULL) ? findClosest(name, NULL) :
        findClosest(*zonename, NULL);
    if (zone_id < 0) {
        flags = NO_SUCH_ZONE;
        return (0);
    }

    sqlite3_stmt* query;
    switch (mode) {
    case ADDRESS:
        query = dbparameters->q_addrs_;
        break;
    case DELEGATION:
        query = dbparameters->q_referral_;
        break;
    default:
        if (rdtype == RRType::ANY()) {
            query = dbparameters->q_any_;
        } else {
            query = dbparameters->q_record_;
        }
        break;
    }

    sqlite3_reset(query);
    sqlite3_clear_bindings(query);

    int rc;
    rc = sqlite3_bind_int(query, 1, zone_id);
    if (rc != SQLITE_OK) {
        isc_throw(Sqlite3Error, "Could not bind zone ID " << zone_id <<
                  " to SQL statement (query)");
    }
    const string name_text = name.toText();
    rc = sqlite3_bind_text(query, 2, name_text.c_str(), -1, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        isc_throw(Sqlite3Error, "Could not bind name " << name_text <<
                  " to SQL statement (query)");
    }

    const string rdtype_text = rdtype.toText();
    if (query == dbparameters->q_record_) {
        rc = sqlite3_bind_text(query, 3, rdtype_text.c_str(), -1,
                               SQLITE_STATIC);
        if (rc != SQLITE_OK) {
            isc_throw(Sqlite3Error, "Could not bind RR type " <<
                      rdtype.toText() << " to SQL statement (query)");
        }
    }

    const int rows = importSqlite3Rows(query, name, getClass(), rdtype, false,
                                       target, flags);
    sqlite3_reset(query);
    if (rows > 0) {
        return (rows);
    }

    //
    // No rows were found.  We need to find out whether there are
    // any RRs with that name to determine whether this is NXDOMAIN or
    // NXRRSET
    //
    sqlite3_reset(dbparameters->q_count_);
    sqlite3_clear_bindings(dbparameters->q_count_);

    rc = sqlite3_bind_int(dbparameters->q_count_, 1, zone_id);
    if (rc != SQLITE_OK) {
        isc_throw(Sqlite3Error, "Could not bind zone ID " << zone_id <<
                  " to SQL statement (qcount)");
    }

    const string revname_text = name.reverse().toText() + "%";
    rc = sqlite3_bind_text(dbparameters->q_count_, 2,
                           revname_text.c_str(),
                           -1, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        isc_throw(Sqlite3Error, "Could not bind name " << name.reverse() <<
                  " to SQL statement (qcount)");
    }

    rc = sqlite3_step(dbparameters->q_count_);
    if (rc == SQLITE_ROW) {
        if (sqlite3_column_int(dbparameters->q_count_, 0) != 0) {
            flags |= TYPE_NOT_FOUND;
            sqlite3_reset(dbparameters->q_count_);
            return (0);
        }
    }

    flags |= NAME_NOT_FOUND;
    sqlite3_reset(dbparameters->q_count_);
    return (0);
}

//
//  Search for the closest enclosing zone.  Will return -1 if not found,
//  >= 0 if found.  If position is not NULL, it will be filled in with the
//  longest match found.
//
int
Sqlite3DataSrc::findClosest(const Name& name, unsigned int* position) const {
    const unsigned int nlabels = name.getLabelCount();
    for (unsigned int i = 0; i < nlabels; ++i) {
        const Name matchname(name.split(i));
        const int rc = hasExactZone(matchname.toText().c_str());
        if (rc >= 0) {
            if (position != NULL) {
                *position = i;
            }
            return (rc);
        }
    }

    return (-1);
}

void
Sqlite3DataSrc::findClosestEnclosure(DataSrcMatch& match) const {
    LOG_DEBUG(logger, DBG_TRACE_DATA, DATASRC_SQLITE_ENCLOSURE).
        arg(match.getName());
    if (match.getClass() != getClass() && match.getClass() != RRClass::ANY()) {
        return;
    }

    unsigned int position;
    if (findClosest(match.getName(), &position) == -1) {
        LOG_DEBUG(logger, DBG_TRACE_DATA, DATASRC_SQLITE_ENCLOSURE_NOT_FOUND)
                  .arg(match.getName());
        return;
    }

    match.update(*this, match.getName().split(position));
}

DataSrc::Result
Sqlite3DataSrc::findPreviousName(const Name& qname,
                                 Name& target,
                                 const Name* zonename) const
{
    LOG_DEBUG(logger, DBG_TRACE_DATA, DATASRC_SQLITE_PREVIOUS).arg(qname);
    const int zone_id = (zonename == NULL) ?
        findClosest(qname, NULL) : findClosest(*zonename, NULL);
    if (zone_id < 0) {
        LOG_ERROR(logger, DATASRC_SQLITE_PREVIOUS_NO_ZONE).arg(qname.toText());
        return (ERROR);
    }
    
    sqlite3_reset(dbparameters->q_previous_);
    sqlite3_clear_bindings(dbparameters->q_previous_);

    int rc = sqlite3_bind_int(dbparameters->q_previous_, 1, zone_id);
    if (rc != SQLITE_OK) {
        isc_throw(Sqlite3Error, "Could not bind zone ID " << zone_id <<
                  " to SQL statement (qprevious)");        
    }
    const string revname_text = qname.reverse().toText();
    rc = sqlite3_bind_text(dbparameters->q_previous_, 2,
                           revname_text.c_str(), -1, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        isc_throw(Sqlite3Error, "Could not bind name " << qname <<
                  " to SQL statement (qprevious)");
    }
  
    rc = sqlite3_step(dbparameters->q_previous_);
    if (rc != SQLITE_ROW) {
        sqlite3_reset(dbparameters->q_previous_);
        return (ERROR);
    }

    // XXX: bad cast.  we should revisit this.
    target = Name((const char*)sqlite3_column_text(dbparameters->q_previous_,
                                                   0));
    sqlite3_reset(dbparameters->q_previous_);
    return (SUCCESS);
}

DataSrc::Result
Sqlite3DataSrc::findCoveringNSEC3(const Name& zonename,
                                  string& hashstr,
                                  RRsetList& target) const
{
    LOG_DEBUG(logger, DBG_TRACE_DATA, DATASRC_SQLITE_FIND_NSEC3).
        arg(zonename).arg(hashstr);
    const int zone_id = findClosest(zonename, NULL);
    if (zone_id < 0) {
        LOG_ERROR(logger, DATASRC_SQLITE_FIND_NSEC3_NO_ZONE).arg(zonename);
        return (ERROR);
    }

    sqlite3_reset(dbparameters->q_prevnsec3_);
    sqlite3_clear_bindings(dbparameters->q_prevnsec3_);

    int rc = sqlite3_bind_int(dbparameters->q_prevnsec3_, 1, zone_id);
    if (rc != SQLITE_OK) {
        isc_throw(Sqlite3Error, "Could not bind zone ID " << zone_id <<
                  " to SQL statement (previous NSEC3)");        
    }

    rc = sqlite3_bind_text(dbparameters->q_prevnsec3_, 2, hashstr.c_str(),
                           -1, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        isc_throw(Sqlite3Error, "Could not bind hash " << hashstr <<
                  " to SQL statement (previous NSEC3)");
    }

    rc = sqlite3_step(dbparameters->q_prevnsec3_);
    const char* hash;
    if (rc == SQLITE_ROW) {
        hash = (const char*) sqlite3_column_text(dbparameters->q_prevnsec3_, 0);
    } else {
        // We need to find the final NSEC3 in the chain.
        // A valid NSEC3 hash is in base32, which contains no
        // letters higher than V, so a search for the previous 
        // NSEC3 from "w" will always find it.
        sqlite3_reset(dbparameters->q_prevnsec3_);
        rc = sqlite3_bind_text(dbparameters->q_prevnsec3_, 2, "w", -1,
                               SQLITE_STATIC);
        if (rc != SQLITE_OK) {
            isc_throw(Sqlite3Error, "Could not bind \"w\""
                      " to SQL statement (previous NSEC3)");
        }

        rc = sqlite3_step(dbparameters->q_prevnsec3_);
        if (rc != SQLITE_ROW) {
            return (ERROR);
        }

        hash = (const char*) sqlite3_column_text(dbparameters->q_prevnsec3_, 0);
    }

    sqlite3_reset(dbparameters->q_nsec3_);
    sqlite3_clear_bindings(dbparameters->q_nsec3_);

    rc = sqlite3_bind_int(dbparameters->q_nsec3_, 1, zone_id);
    if (rc != SQLITE_OK) {
        isc_throw(Sqlite3Error, "Could not bind zone ID " << zone_id <<
                  " to SQL statement (NSEC3)");        
    }

    rc = sqlite3_bind_text(dbparameters->q_nsec3_, 2, hash, -1, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        isc_throw(Sqlite3Error, "Could not bind hash " << hash <<
                  " to SQL statement (NSEC3)");
    }

    DataSrc::Result result = SUCCESS;
    uint32_t flags = 0;
    if (importSqlite3Rows(dbparameters->q_nsec3_,
                          Name(hash).concatenate(zonename),
                          getClass(), RRType::NSEC3(), true, target,
                          flags) == 0 || flags != 0) {
        result = ERROR;
    }
    hashstr = string(hash);
    sqlite3_reset(dbparameters->q_nsec3_);
    return (result);
}

DataSrc::Result
Sqlite3DataSrc::findRRset(const Name& qname,
                          const RRClass& qclass,
                          const RRType& qtype,
                          RRsetList& target,
                          uint32_t& flags,
                          const Name* zonename) const
{
    LOG_DEBUG(logger, DBG_TRACE_DATA, DATASRC_SQLITE_FIND).arg(qname).
        arg(qtype);
    if (qclass != getClass() && qclass != RRClass::ANY()) {
        LOG_ERROR(logger, DATASRC_SQLITE_FIND_BAD_CLASS).arg(getClass()).
            arg(qclass);
        return (ERROR);
    }
    findRecords(qname, qtype, target, zonename, NORMAL, flags);
    return (SUCCESS);
}

DataSrc::Result
Sqlite3DataSrc::findExactRRset(const Name& qname,
                               const RRClass& qclass,
                               const RRType& qtype,
                               RRsetList& target,
                               uint32_t& flags,
                               const Name* zonename) const
{
    LOG_DEBUG(logger, DBG_TRACE_DATA, DATASRC_SQLITE_FINDEXACT).arg(qname).
        arg(qtype);
    if (qclass != getClass() && qclass != RRClass::ANY()) {
        LOG_ERROR(logger, DATASRC_SQLITE_FINDEXACT_BAD_CLASS).arg(getClass()).
            arg(qclass);
        return (ERROR);
    }
    findRecords(qname, qtype, target, zonename, NORMAL, flags);

    // Ignore referrals in this case
    flags &= ~REFERRAL;

    // CNAMEs don't count in this case
    if (flags & CNAME_FOUND) {
        flags &= ~CNAME_FOUND;
        flags |= TYPE_NOT_FOUND;
    }

    return (SUCCESS);
}

DataSrc::Result
Sqlite3DataSrc::findAddrs(const Name& qname,
                          const RRClass& qclass,
                          RRsetList& target,
                          uint32_t& flags,
                          const Name* zonename) const
{
    LOG_DEBUG(logger, DBG_TRACE_DATA, DATASRC_SQLITE_FINDADDRS).arg(qname);
    if (qclass != getClass() && qclass != RRClass::ANY()) {
        LOG_ERROR(logger, DATASRC_SQLITE_FINDADDRS_BAD_CLASS).arg(getClass()).
            arg(qclass);
        return (ERROR);
    }
    findRecords(qname, RRType::ANY(), target, zonename, ADDRESS, flags);
    return (SUCCESS);
}

DataSrc::Result
Sqlite3DataSrc::findReferral(const Name& qname,
                             const RRClass& qclass,
                             RRsetList& target,
                             uint32_t& flags,
                             const Name* zonename) const
{
    LOG_DEBUG(logger, DBG_TRACE_DATA, DATASRC_SQLITE_FINDREF).arg(qname);
    if (qclass != getClass() && qclass != RRClass::ANY()) {
        LOG_ERROR(logger, DATASRC_SQLITE_FINDREF_BAD_CLASS).arg(getClass()).
            arg(qclass);
        return (ERROR);
    }
    findRecords(qname, RRType::ANY(), target, zonename, DELEGATION, flags);
    return (SUCCESS);
}

Sqlite3DataSrc::Sqlite3DataSrc() :
    dbparameters(new Sqlite3Parameters)
{
    LOG_DEBUG(logger, DBG_TRACE_BASIC, DATASRC_SQLITE_CREATE);
}

Sqlite3DataSrc::~Sqlite3DataSrc() {
    LOG_DEBUG(logger, DBG_TRACE_BASIC, DATASRC_SQLITE_DESTROY);
    if (dbparameters->db_ != NULL) {
        close();
    }
    delete dbparameters;
}

DataSrc::Result
Sqlite3DataSrc::init(isc::data::ConstElementPtr config) {
    if (config && config->contains("database_file")) {
        open(config->get("database_file")->stringValue());
    } else {
        isc_throw(DataSourceError, "No SQLite database file specified");
    }
    return (SUCCESS);
}

namespace {
// This is a helper class to initialize a Sqlite3 DB safely.  An object of
// this class encapsulates all temporary resources that are necessary for
// the initialization, and release them in the destructor.  Once everything
// is properly initialized, the move() method moves the allocated resources
// to the main object in an exception free manner.  This way, the main code
// for the initialization can be exception safe, and can provide the strong
// exception guarantee.
class Sqlite3Initializer {
public:
    ~Sqlite3Initializer() {
        if (params_.q_zone_ != NULL) {
            sqlite3_finalize(params_.q_zone_);
        }
        if (params_.q_record_ != NULL) {
            sqlite3_finalize(params_.q_record_);
        }
        if (params_.q_addrs_ != NULL) {
            sqlite3_finalize(params_.q_addrs_);
        }
        if (params_.q_referral_ != NULL) {
            sqlite3_finalize(params_.q_referral_);
        }
        if (params_.q_any_ != NULL) {
            sqlite3_finalize(params_.q_any_);
        }
        if (params_.q_count_ != NULL) {
            sqlite3_finalize(params_.q_count_);
        }
        if (params_.q_previous_ != NULL) {
            sqlite3_finalize(params_.q_previous_);
        }
        if (params_.q_nsec3_ != NULL) {
            sqlite3_finalize(params_.q_nsec3_);
        }
        if (params_.q_prevnsec3_ != NULL) {
            sqlite3_finalize(params_.q_prevnsec3_);
        }
        if (params_.db_ != NULL) {
            sqlite3_close(params_.db_);
        }
    }
    void move(Sqlite3Parameters* dst) {
        *dst = params_;
        params_ = Sqlite3Parameters(); // clear everything
    }
    Sqlite3Parameters params_;
};

sqlite3_stmt*
prepare(sqlite3* const db, const char* const statement) {
    sqlite3_stmt* prepared = NULL;
    if (sqlite3_prepare_v2(db, statement, -1, &prepared, NULL) != SQLITE_OK) { 
        isc_throw(Sqlite3Error, "Could not prepare SQLite statement: " <<
                  statement);
    }
    return (prepared);
}

// small function to sleep for 0.1 seconds, needed when waiting for
// exclusive database locks (which should only occur on startup, and only
// when the database has not been created yet)
void do_sleep() {
#ifdef _WIN32
    Sleep(100);
#else
    struct timespec req;
    req.tv_sec = 0;
    req.tv_nsec = 100000000;
    nanosleep(&req, NULL);
#endif
}

// returns the schema version element if the schema version table exists
// returns -1 if it does not
int check_schema_version_element(sqlite3* db, const char* const version_query) {
    sqlite3_stmt* prepared = NULL;
    // At this point in time, the database might be exclusively locked, in
    // which case even prepare() will return BUSY, so we may need to try a
    // few times
    for (size_t i = 0; i < 50; ++i) {
        int rc = sqlite3_prepare_v2(db, version_query, -1, &prepared, NULL);
        if (rc == SQLITE_ERROR) {
            // this is the error that is returned when the table does not
            // exist
            return (-1);
        } else if (rc == SQLITE_OK) {
            break;
        } else if (rc != SQLITE_BUSY || i == 50) {
            isc_throw(Sqlite3Error, "Unable to prepare version query: "
                        << rc << " " << sqlite3_errmsg(db));
        }
        do_sleep();
    }
    if (sqlite3_step(prepared) != SQLITE_ROW) {
        isc_throw(Sqlite3Error,
                    "Unable to query version: " << sqlite3_errmsg(db));
    }
    int version = sqlite3_column_int(prepared, 0);
    sqlite3_finalize(prepared);
    return (version);
}

// Returns the schema major and minor version numbers in a pair.
// Returns (-1, -1) if the table does not exist, (1, 0) for a V1
// database, and (n, m) for any other.
pair<int, int> check_schema_version(sqlite3* db) {
    int major = check_schema_version_element(db, q_version_str);
    if (major == -1) {
        return (make_pair(-1, -1));
    } else if (major == 1) {
        return (make_pair(1, 0));
    } else {
        int minor = check_schema_version_element(db, q_minor_str);
        return (make_pair(major, minor));
    }
}

// A helper class used in create_database() below so we manage the one shot
// transaction safely.
class ScopedTransaction {
public:
    ScopedTransaction(sqlite3* db) : db_(NULL) {
        // try for 5 secs (50*0.1)
        for (size_t i = 0; i < 50; ++i) {
            const int rc = sqlite3_exec(db, "BEGIN EXCLUSIVE TRANSACTION",
                                        NULL, NULL, NULL);
            if (rc == SQLITE_OK) {
                break;
            } else if (rc != SQLITE_BUSY || i == 50) {
                isc_throw(Sqlite3Error, "Unable to acquire exclusive lock "
                          "for database creation: " << sqlite3_errmsg(db));
            }
            do_sleep();
        }
        // Hold the DB pointer once we have successfully acquired the lock.
        db_ = db;
    }
    ~ScopedTransaction() {
        if (db_ != NULL) {
            // Note: even rollback could fail in theory, but in that case
            // we cannot do much for safe recovery anyway.  We could at least
            // log the event, but for now don't even bother to do that, with
            // the expectation that we'll soon stop creating the schema in this
            // module.
            sqlite3_exec(db_, "ROLLBACK", NULL, NULL, NULL);
        }
    }
    void commit() {
        if (sqlite3_exec(db_, "COMMIT TRANSACTION", NULL, NULL, NULL) !=
            SQLITE_OK) {
            isc_throw(Sqlite3Error, "Unable to commit newly created database "
                      "schema: " << sqlite3_errmsg(db_));
        }
        db_ = NULL;
    }

private:
    sqlite3* db_;
};

// return db version
pair<int, int> create_database(sqlite3* db) {
    logger.info(DATASRC_SQLITE_SETUP);

    // try to get an exclusive lock. Once that is obtained, do the version
    // check *again*, just in case this process was racing another
    ScopedTransaction transaction(db);
    pair<int, int> schema_version = check_schema_version(db);
    if (schema_version.first == -1) {
        for (int i = 0; SCHEMA_LIST[i] != NULL; ++i) {
            if (sqlite3_exec(db, SCHEMA_LIST[i], NULL, NULL, NULL) !=
                SQLITE_OK) {
                isc_throw(Sqlite3Error,
                        "Failed to set up schema " << SCHEMA_LIST[i]);
            }
        }
        transaction.commit();

        // Return the version. We query again to ensure that the only point
        // in which the current schema version is defined is in the
        // CREATE statements.
        schema_version = check_schema_version(db);
    }
    return (schema_version);
}

void
checkAndSetupSchema(Sqlite3Initializer* initializer) {
    sqlite3* const db = initializer->params_.db_;

    // Note: we use the same SCHEMA_xxx_VERSION log IDs here and in
    // sqlite3_accessor.cc, which is against our policy of ID uniqueness.
    // The assumption is that this file will soon be deprecated, and we don't
    // bother to define separate IDs for the short period.
    pair<int, int> schema_version = check_schema_version(db);
    if (schema_version.first == -1) {
        schema_version = create_database(db);
    } else if (schema_version.first != SQLITE_SCHEMA_MAJOR_VERSION) {
        LOG_ERROR(logger, DATASRC_SQLITE_INCOMPATIBLE_VERSION)
            .arg(schema_version.first).arg(schema_version.second)
            .arg(SQLITE_SCHEMA_MAJOR_VERSION).arg(SQLITE_SCHEMA_MINOR_VERSION);
        isc_throw(IncompatibleDbVersion, "Incompatible database version");
    } else if (schema_version.second < SQLITE_SCHEMA_MINOR_VERSION) {
        LOG_WARN(logger, DATASRC_SQLITE_COMPATIBLE_VERSION)
            .arg(schema_version.first).arg(schema_version.second)
            .arg(SQLITE_SCHEMA_MAJOR_VERSION).arg(SQLITE_SCHEMA_MINOR_VERSION);
    }

    initializer->params_.major_version_ = schema_version.first;
    initializer->params_.minor_version_ = schema_version.second;
    initializer->params_.q_zone_ = prepare(db, q_zone_str);
    initializer->params_.q_record_ = prepare(db, q_record_str);
    initializer->params_.q_addrs_ = prepare(db, q_addrs_str);
    initializer->params_.q_referral_ = prepare(db, q_referral_str);
    initializer->params_.q_any_ = prepare(db, q_any_str);
    initializer->params_.q_count_ = prepare(db, q_count_str);
    initializer->params_.q_previous_ = prepare(db, q_previous_str);
    initializer->params_.q_nsec3_ = prepare(db, q_nsec3_str);
    initializer->params_.q_prevnsec3_ = prepare(db, q_prevnsec3_str);
}
}

//
//  Open the database.
//
void
Sqlite3DataSrc::open(const string& name) {
    LOG_DEBUG(logger, DBG_TRACE_BASIC, DATASRC_SQLITE_OPEN).arg(name);
    if (dbparameters->db_ != NULL) {
        isc_throw(DataSourceError, "Duplicate SQLite open with " << name);
    }

    Sqlite3Initializer initializer;

    if (sqlite3_open(name.c_str(), &initializer.params_.db_) != 0) {
        isc_throw(Sqlite3Error, "Cannot open SQLite database file: " << name);
    }

    checkAndSetupSchema(&initializer);
    initializer.move(dbparameters);
}

//
//  Close the database.
//
DataSrc::Result
Sqlite3DataSrc::close(void) {
    LOG_DEBUG(logger, DBG_TRACE_BASIC, DATASRC_SQLITE_CLOSE);
    if (dbparameters->db_ == NULL) {
        isc_throw(DataSourceError,
                  "SQLite data source is being closed before open");
    }

    // XXX: sqlite3_finalize() could fail.  What should we do in that case?
    sqlite3_finalize(dbparameters->q_zone_);
    dbparameters->q_zone_ = NULL;

    sqlite3_finalize(dbparameters->q_record_);
    dbparameters->q_record_ = NULL;

    sqlite3_finalize(dbparameters->q_addrs_);
    dbparameters->q_addrs_ = NULL;

    sqlite3_finalize(dbparameters->q_referral_);
    dbparameters->q_referral_ = NULL;

    sqlite3_finalize(dbparameters->q_any_);
    dbparameters->q_any_ = NULL;

    sqlite3_finalize(dbparameters->q_count_);
    dbparameters->q_count_ = NULL;

    sqlite3_finalize(dbparameters->q_previous_);
    dbparameters->q_previous_ = NULL;

    sqlite3_finalize(dbparameters->q_prevnsec3_);
    dbparameters->q_prevnsec3_ = NULL;

    sqlite3_finalize(dbparameters->q_nsec3_);
    dbparameters->q_nsec3_ = NULL;

    sqlite3_close(dbparameters->db_);
    dbparameters->db_ = NULL;

    return (SUCCESS);
}

}
}
