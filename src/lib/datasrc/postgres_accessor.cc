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

#include <exceptions/exceptions.h>

#include <dns/rrclass.h>
#include <dns/rrtype.h>

#include "database.h"
#include "postgres_accessor.h"

#include <libpq-fe.h>

#include <boost/lexical_cast.hpp>
#include <boost/enable_shared_from_this.hpp>

#include <utility>
#include <string>

using std::string;
using std::pair;
using boost::lexical_cast;
using namespace isc::data;
using namespace isc::dns;

namespace isc {
namespace datasrc {

namespace {
const char* const GETZONE_STATEMENT = "getzone";
const char* const ANY_QUERY_STATEMENT = "any_query";

const char* const getzone_query_txt =
    "SELECT id, name FROM zones WHERE class=1 AND rname<=$1 "
    "ORDER BY rname DESC LIMIT 1;";
const char* const any_query_txt =
    "SELECT typecode, ttl, typecode, rdata FROM records WHERE zone_id=$1 "
    "AND name=$2;";
}

PostgresAccessor::PostgresAccessor(const string& rrclass) :
    pgconn_(NULL),
    database_name_("Postgres 'bind' database at localhost"),
    rrclass_(rrclass),
    rrclass_code_(lexical_cast<string>(dns::RRClass(rrclass).getCode())),
    updated_zone_id_(-1)
{
    const char* const postgres_conn_info = "host = /tmp dbname = bind "
    "password = bind port = 5432";
    if ((pgconn_ = PQconnectdb(postgres_conn_info)) == NULL ||
        PQstatus(pgconn_) != CONNECTION_OK) {
        if (pgconn_ != NULL) {
            PQfinish(pgconn_);
        }
        isc_throw(PostgresError, "failed to open a PostgreSQL Connection");
    }

    // for brevity we don't handle failure cases gracefully
    PGresult* result =
        PQprepare(pgconn_, GETZONE_STATEMENT, getzone_query_txt, 1, NULL);
    assert(result != NULL && PQresultStatus(result) == PGRES_COMMAND_OK);
    PQclear(result);

    result = PQprepare(pgconn_, ANY_QUERY_STATEMENT, any_query_txt, 2, NULL);
    assert(result != NULL && PQresultStatus(result) == PGRES_COMMAND_OK);
    PQclear(result);
}

boost::shared_ptr<DatabaseAccessor>
PostgresAccessor::clone() {
    return (boost::shared_ptr<DatabaseAccessor>(new PostgresAccessor(rrclass_)));
}

PostgresAccessor::~PostgresAccessor() {
    PQfinish(pgconn_);
}

pair<bool, int>
PostgresAccessor::getZone(const string& zone_name) const {
    const string getzone_query = string("SELECT id FROM zones WHERE name='") +
        zone_name + "';";
    PGresult* result = PQexec(pgconn_, getzone_query.c_str());
    if (result == NULL || PQresultStatus(result) != PGRES_TUPLES_OK) {
        if (result != NULL) {
            PQclear(result);
        }
        isc_throw(PostgresError, "failed to get zone: " << zone_name);
    }
    if (PQntuples(result) == 1 && PQnfields(result) == 1) {
        const int zone_id = atoi(PQgetvalue(result, 0, 0));
        PQclear(result);
        return (pair<bool, int>(true, zone_id));
    }
    PQclear(result);
    return (pair<bool, int>(false, -1));
}

pair<bool, int>
PostgresAccessor::getZone(const string& rname, string& zname_txt) const {
    // Prepare query parameters
    const char* params[1];
    const int param_formats[1] = {0}; // it's a text parameter
    params[0] = rname.c_str();

    PGresult* result = PQexecPrepared(pgconn_, GETZONE_STATEMENT, 1, params,
                                      NULL, param_formats, 0);
    if (result == NULL || PQresultStatus(result) != PGRES_TUPLES_OK) {
        if (result != NULL) {
            PQclear(result);
        }
        isc_throw(PostgresError, "failed to get zone: " << rname);
    }
    if (PQntuples(result) == 1 && PQnfields(result) == 2) {
        const int zone_id = atoi(PQgetvalue(result, 0, 0));
        zname_txt = PQgetvalue(result, 0, 1);
        PQclear(result);
        return (pair<bool, int>(true, zone_id));
    }
    PQclear(result);
    return (pair<bool, int>(false, -1));
}

class PostgresAccessor::Context : public DatabaseAccessor::IteratorContext {
public:
    // Construct an iterator for records with a specific name. When constructed
    // this way, the getNext() call will copy all fields except name
    Context(const boost::shared_ptr<const PostgresAccessor>& accessor, int id,
            const std::string& name) :
        accessor_(accessor), result_(NULL), row_count_(0)

    {
        const char* params[2];
        const int param_formats[2] = {0, 0}; // they are text parameters
        params[0] = lexical_cast<string>(id).c_str();
        params[1] = name.c_str();

        result_ = PQexecPrepared(accessor_->pgconn_, ANY_QUERY_STATEMENT,
                                 2, params, NULL, param_formats, 0);
        if (result_ == NULL || PQresultStatus(result_) != PGRES_TUPLES_OK) {
            if (result_ != NULL) {
                PQclear(result_);
            }
            isc_throw(PostgresError, "Postgres query failure for "
                      << any_query_txt);
        }
        n_rows_ = PQntuples(result_);
    }

    virtual bool getNext(std::string (&)[COLUMN_COUNT]) {
        isc_throw(NotImplemented, "not implemented");
    }
    virtual bool next() {
        return (row_count_++ < n_rows_);
    }
    virtual std::string getColumnAsText(RecordColumns column) {
        assert(row_count_ > 0);
        return (PQgetvalue(result_, row_count_ - 1, column));
    }
    virtual int getColumnAsInt(RecordColumns column) {
        assert(row_count_ > 0);
        return (lexical_cast<int>(PQgetvalue(result_, row_count_ - 1,
                                             column)));
    }

    virtual ~Context() {
        finalize();
    }

private:
    void finalize() {
        if (result_ != NULL) {
            PQclear(result_);
        }
    }
    boost::shared_ptr<const PostgresAccessor> accessor_;
    PGresult* result_;
    size_t row_count_;
    size_t n_rows_;
};

//
// Methods to retrieve the various iterators
//
DatabaseAccessor::IteratorContextPtr
PostgresAccessor::getRecords(const string& name, int id,
                              bool subdomains) const
{
    if (subdomains) {
        isc_throw(NotImplemented, "subdomain search not yet implemented");
    }
    return (IteratorContextPtr(new Context(shared_from_this(), id, name)));
}

DatabaseAccessor::IteratorContextPtr
PostgresAccessor::getNSEC3Records(const string&, int) const {
    isc_throw(NotImplemented, "not implemented");
}

DatabaseAccessor::IteratorContextPtr
PostgresAccessor::getAllRecords(int) const {
    isc_throw(NotImplemented, "not implemented");
}


/// \brief Difference Iterator
// ... not yet

DatabaseAccessor::IteratorContextPtr
PostgresAccessor::getDiffs(int, uint32_t, uint32_t) const {
    isc_throw(NotImplemented, "not implemented");
}

pair<bool, int>
PostgresAccessor::startUpdateZone(const string& zone_name,
                                   const bool replace)
{
    const pair<bool, int> zone_info(getZone(zone_name));
    if (!zone_info.first) {
        return (zone_info);
    }

    PGresult* result = PQexec(pgconn_, "START TRANSACTION");
    if (result == NULL || PQresultStatus(result) != PGRES_COMMAND_OK) {
        if (result != NULL) {
            PQclear(result);
        }
        isc_throw(PostgresError, "failed to start a transaction");
    }
    PQclear(result);

    if (replace) {
        // First, clear all current data from tables.
        const string flush_query = string("DELETE FROM records WHERE zone_id=")
            + lexical_cast<string>(zone_info.second);
        result = PQexec(pgconn_, flush_query.c_str());
        if (result == NULL || PQresultStatus(result) != PGRES_COMMAND_OK) {
            if (result != NULL) {
                PQclear(result);
            }
            // XXX should rollback
            isc_throw(PostgresError, "failed to delete zone records");
        }
    }
    PQclear(result);
    updated_zone_id_ = zone_info.second;
    return (zone_info);
}

void
PostgresAccessor::startTransaction() {
    isc_throw(NotImplemented, "not yet implemented");
}

void
PostgresAccessor::commit() {
    PGresult* result = PQexec(pgconn_, "COMMIT");
    if (result == NULL || PQresultStatus(result) != PGRES_COMMAND_OK) {
        if (result != NULL) {
            PQclear(result);
        }
        isc_throw(PostgresError, "failed to commit a transaction");
    }
    PQclear(result);
    updated_zone_id_ = -1;
}

void
PostgresAccessor::rollback() {
    PGresult* result = PQexec(pgconn_, "ROLLBACK");
    if (result == NULL || PQresultStatus(result) != PGRES_COMMAND_OK) {
        if (result != NULL) {
            PQclear(result);
        }
        isc_throw(PostgresError, "failed to rollback a transaction");
    }
    PQclear(result);
    updated_zone_id_ = -1;
}

void
PostgresAccessor::addRecordToZone(const string (&columns)[ADD_COLUMN_COUNT]) {
    string add_query = string("INSERT INTO records "
                              "(zone_id, name, rname, ttl, typecode, "
                              "rdata) VALUES(") +
        lexical_cast<string>(updated_zone_id_);

    for (int i = 0; i < ADD_COLUMN_COUNT; ++i) {
        if (i == ADD_SIGTYPE) {
            continue;           // our schema doesn't support this.
        }
        add_query += ", ";
        if (!columns[i].empty()) {
            if (i == ADD_TYPE) {
                add_query += lexical_cast<string>(
                    RRType(columns[i]).getCode());
            } else {
                add_query.push_back('\'');
                add_query += columns[i];
                add_query.push_back('\'');
            }
        } else {
            add_query += "NULL";
        }
    }
    add_query += ");";
    PGresult* result = PQexec(pgconn_, add_query.c_str());
    if (result == NULL || PQresultStatus(result) != PGRES_COMMAND_OK) {
        if (result != NULL) {
            PQclear(result);
        }
        isc_throw(PostgresError,
                  "failed to add a record to Postgres DB: " << add_query);
    }
    PQclear(result);
}

void
PostgresAccessor::addNSEC3RecordToZone(
    const string (&)[ADD_NSEC3_COLUMN_COUNT])
{
    isc_throw(NotImplemented, "not implemented");
}

void
PostgresAccessor::deleteRecordInZone(const string (&/*params*/)[DEL_PARAM_COUNT]) {
    isc_throw(NotImplemented, "not yet implemented");
}

void
PostgresAccessor::deleteNSEC3RecordInZone(
    const string (&/*params*/)[DEL_PARAM_COUNT])
{
    isc_throw(NotImplemented, "not implemented");
}

void
PostgresAccessor::addRecordDiff(int, uint32_t,
                             DiffOperation,
                             const string (&)[DIFF_PARAM_COUNT])
{
    isc_throw(NotImplemented, "not implemented");
}

string
PostgresAccessor::findPreviousName(int, const string&)
    const
{
    isc_throw(NotImplemented, "not implemented");
}

string
PostgresAccessor ::findPreviousNSEC3Hash(int, const string&)
    const
{
    isc_throw(NotImplemented, "not implemented");
}

} // end of namespace datasrc
} // end of namespace isc
