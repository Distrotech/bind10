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
#include "mysql_accessor.h"

#include <mysql.h>

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

MySQLAccessor::MySQLAccessor(const string& rrclass) :
    mysql_(mysql_init(NULL)),
    current_res_(NULL),
    database_name_("MySQL 'bind' database at localhost"),
    rrclass_(rrclass),
    rrclass_code_(lexical_cast<string>(dns::RRClass(rrclass).getCode())),
    getzone_query1_("SELECT id, name FROM zones WHERE class=1 AND rname<='"),
    getzone_query2_("' ORDER BY rname DESC LIMIT 1"),
    any_query1_("SELECT typecode, ttl, typecode, rdata FROM "
                "records WHERE zone_id="),
    any_query2_(" AND name='"),
    updated_zone_id_(-1)
{
    const char* host = "localhost";
    const char* user = "bind";
    const char* passwd = "bind";
    const char* dbname = "bind";
    const unsigned int port = 0;
    const char* socket = NULL;
    if (mysql_real_connect(mysql_, host, user, passwd, dbname, port, socket, 0)
        == NULL) {
        mysql_close(mysql_);
        isc_throw(MySQLError, "failed to open a MySQLConnection");
    }
}

boost::shared_ptr<DatabaseAccessor>
MySQLAccessor::clone() {
    return (boost::shared_ptr<DatabaseAccessor>(new MySQLAccessor(rrclass_)));
}

MySQLAccessor::~MySQLAccessor() {
    mysql_close(mysql_);
}

pair<bool, int>
MySQLAccessor::getZone(const string& zone_name) const {
    const string getzone_query = string("SELECT id FROM zones WHERE name='") +
        zone_name + "'";

    MYSQL_RES* res;
    if (mysql_query(mysql_, getzone_query.c_str()) != 0 ||
        (res = mysql_store_result(mysql_)) == NULL) {
        isc_throw(MySQLError, "failed to get zone: " << zone_name);
    }

    const int rows = mysql_num_rows(res);
    MYSQL_ROW row;
    if (rows == 1 && (row = mysql_fetch_row(res)) != NULL && row[0] != NULL) {
        const int zone_id = atoi(row[0]);
        mysql_free_result(res);
        return (pair<bool, int>(true, zone_id));
    }
    mysql_free_result(res);
    return (pair<bool, int>(false, -1));
}

pair<bool, int>
MySQLAccessor::getZone(const string& rname, string& zname_txt) const {
    const string query_txt = getzone_query1_ + rname + getzone_query2_;
    MYSQL_RES* res;
    if (mysql_query(mysql_, query_txt.c_str()) != 0 ||
        (res = mysql_store_result(mysql_)) == NULL) {
        isc_throw(MySQLError, "failed to get zone: " << rname);
    }

    const int rows = mysql_num_rows(res);
    MYSQL_ROW row;
    if (rows == 1 && (row = mysql_fetch_row(res)) != NULL && row[0] != NULL) {
        const int zone_id = atoi(row[0]);
        zname_txt = row[1];
        mysql_free_result(res);
        return (pair<bool, int>(true, zone_id));
    }
    mysql_free_result(res);
    return (pair<bool, int>(false, -1));
}

class MySQLAccessor::Context : public DatabaseAccessor::IteratorContext {
public:
    // Construct an iterator for records with a specific name. When constructed
    // this way, the getNext() call will copy all fields except name
    Context(const boost::shared_ptr<const MySQLAccessor>& accessor, int id,
            const std::string& name) :
        accessor_(accessor), res_(NULL), row_(NULL)
    {
        string query_txt(accessor_->any_query1_);
        query_txt += lexical_cast<string>(id);
        query_txt += accessor_->any_query2_;
        query_txt += name;
        query_txt.push_back('\'');

        if (mysql_query(accessor_->mysql_, query_txt.c_str()) != 0 ||
            (res_ = mysql_use_result(accessor_->mysql_)) == NULL)
        {
            isc_throw(MySQLError, "MySQL query failure for " << query_txt);
        }
    }

    virtual bool getNext(std::string (&)[COLUMN_COUNT]) {
        isc_throw(NotImplemented, "not implemented");
    }
    virtual bool next() {
        row_ = mysql_fetch_row(res_);
        return (row_ != NULL);
    }
    virtual std::string getColumnAsText(RecordColumns column) {
        return (row_[column]);
    }
    virtual int getColumnAsInt(RecordColumns column) {
        return (lexical_cast<int>(row_[column]));
    }

    virtual ~Context() {
        finalize();
    }

private:
    void finalize() {
        if (res_ != NULL) {
            mysql_free_result(res_);
        }
    }
    boost::shared_ptr<const MySQLAccessor> accessor_;
    MYSQL_RES* res_;
    MYSQL_ROW row_;
};

//
// Methods to retrieve the various iterators
//
DatabaseAccessor::IteratorContextPtr
MySQLAccessor::getRecords(const string& name, int id, bool subdomains) const {
    if (subdomains) {
        isc_throw(NotImplemented, "subdomain search not yet implemented");
    }
    return (IteratorContextPtr(new Context(shared_from_this(), id, name)));
}

DatabaseAccessor::IteratorContextPtr
MySQLAccessor::getNSEC3Records(const string&, int) const {
    isc_throw(NotImplemented, "not implemented");
}

DatabaseAccessor::IteratorContextPtr
MySQLAccessor::getAllRecords(int) const {
    isc_throw(NotImplemented, "not implemented");
}


/// \brief Difference Iterator
// ... not yet

DatabaseAccessor::IteratorContextPtr
MySQLAccessor::getDiffs(int, uint32_t, uint32_t) const {
    isc_throw(NotImplemented, "not implemented");
}

pair<bool, int>
MySQLAccessor::startUpdateZone(const string& zone_name, const bool replace) {
    const pair<bool, int> zone_info(getZone(zone_name));
    if (!zone_info.first) {
        return (zone_info);
    }

    if (mysql_query(mysql_, "START TRANSACTION") != 0) {
        isc_throw(MySQLError, "failed to start a transaction");
    }

    if (replace) {
        // First, clear all current data from tables.
        const string flush_query = string("DELETE FROM records WHERE zone_id=")
            + lexical_cast<string>(zone_info.second);
        if (mysql_query(mysql_, flush_query.c_str()) != 0) {
            // XXX should rollback
            isc_throw(MySQLError, "failed to delete zone records");
        }
    }
    updated_zone_id_ = zone_info.second;
    return (zone_info);
}

void
MySQLAccessor::startTransaction() {
    isc_throw(NotImplemented, "not yet implemented");
}

void
MySQLAccessor::commit() {
    if (mysql_query(mysql_, "COMMIT") != 0) {
        isc_throw(MySQLError, "failed to commit a transaction");
    }
    updated_zone_id_ = -1;
}

void
MySQLAccessor::rollback() {
    if (mysql_query(mysql_, "ROLLBACK") != 0) {
        isc_throw(MySQLError, "failed to rollback a transaction");
    }
    updated_zone_id_ = -1;
}

void
MySQLAccessor::addRecordToZone(const string (&columns)[ADD_COLUMN_COUNT]) {
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
    if (mysql_query(mysql_, add_query.c_str()) != 0) {
        isc_throw(MySQLError,
                  "failed to add a record to MySQL DB: " << add_query);
    }
}

void
MySQLAccessor::addNSEC3RecordToZone(
    const string (&)[ADD_NSEC3_COLUMN_COUNT])
{
    isc_throw(NotImplemented, "not implemented");
}

void
MySQLAccessor::deleteRecordInZone(const string (&/*params*/)[DEL_PARAM_COUNT]) {
    isc_throw(NotImplemented, "not yet implemented");
}

void
MySQLAccessor::deleteNSEC3RecordInZone(
    const string (&/*params*/)[DEL_PARAM_COUNT])
{
    isc_throw(NotImplemented, "not implemented");
}

void
MySQLAccessor::addRecordDiff(int, uint32_t,
                             DiffOperation,
                             const string (&)[DIFF_PARAM_COUNT])
{
    isc_throw(NotImplemented, "not implemented");
}

string
MySQLAccessor::findPreviousName(int, const string&)
    const
{
    isc_throw(NotImplemented, "not implemented");
}

string
MySQLAccessor ::findPreviousNSEC3Hash(int, const string&)
    const
{
    isc_throw(NotImplemented, "not implemented");
}

} // end of namespace datasrc
} // end of namespace isc
