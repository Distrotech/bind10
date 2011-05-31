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

#include <mysql/mysql.h>

#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>

#include <string>
#include <vector>

#include <datasrc/data_source.h>
#include <datasrc/client.h>
#include <datasrc/mysql_conn.h>

using namespace std;
using namespace boost;

namespace isc {
namespace datasrc {

MySQLConnection::MySQLConnection() : conn_(mysql_init(NULL)),
                                     current_res_(NULL)
{
    const char* host = "localhost";
    const char* user = "bind";
    const char* passwd = "version10";
    const char* dbname = "bind";
    unsigned int port = 0;
    const char* socket = NULL;
    if (mysql_real_connect(conn_, host, user, passwd, dbname, port, socket, 0)
        == NULL) {
        mysql_close(conn_);
        isc_throw(DataSourceError, "failed to open a MySQLConnection");
    }
}

MySQLConnection::~MySQLConnection() {
    resetQuery();
    mysql_close(conn_);
}

DataSrc::Result
MySQLConnection::getZone(const string& name, int& zone_id) const {
    // Note: in this experiment, skip escaping for brevity.
    const string getzone_query = string("SELECT id FROM zones WHERE name='") +
        name + string("';");

    MYSQL_RES* res;
    if (mysql_query(conn_, getzone_query.c_str()) != 0 ||
        (res = mysql_store_result(conn_)) == NULL) {
        return (DataSrc::ERROR);
    }

    const int rows = mysql_num_rows(res);
    MYSQL_ROW row;
    if (rows == 1 && (row = mysql_fetch_row(res)) != NULL && row[0] != NULL) {
        zone_id = atoi(row[0]);
    }
    mysql_free_result(res);
    return ((rows == 1) ? DataSrc::SUCCESS : DataSrc::ERROR);
}

void
MySQLConnection::searchForRecords(int zone_id, const string& name,
                                  bool match_subdomain)
{
    const string base_query = string("SELECT rdtype, ttl, sigtype, rdata "
                                       "FROM records WHERE zone_id=") +
        lexical_cast<string>(zone_id) + string(" AND name");
    string query = base_query;
    if (match_subdomain) {
        query += string(" LIKE '%") + name + string("';");
    } else {
        query += string("='") + name + string("';");
    }
    if (mysql_query(conn_, query.c_str()) != 0 ||
        (current_res_ = mysql_use_result(conn_)) == NULL) {
        isc_throw(DataSourceError, "failed to search MySQL DB: " << query);
    }
}

DataSrc::Result
MySQLConnection::getNextRecord(vector<string>& columns) {
    assert(current_res_ != NULL);
    MYSQL_ROW row = mysql_fetch_row(current_res_);
    if (row == NULL || mysql_num_fields(current_res_) < 4) {
        resetQuery();
        return (DataSrc::ERROR);
    }
    columns.clear();
    for (int column = 0; column < 4; ++column) {
        columns.push_back(row[column] ? row[column] : "");
    }
    return (DataSrc::SUCCESS);
}

void
MySQLConnection::traverseZone(int zone_id) {
    const string query = string("SELECT name, rdtype, ttl, rdata "
                                "FROM records WHERE zone_id=") +
        lexical_cast<string>(zone_id) + string(";");
    if (mysql_query(conn_, query.c_str()) != 0 ||
        (current_res_ = mysql_use_result(conn_)) == NULL) {
        isc_throw(DataSourceError,
                  "failed to start traversing a zone in MySQL DB: " << query);
    }
}

string
MySQLConnection::getPreviousName(int /*zone_id*/, const string& /*name*/) const {
    return ("");
}

void
MySQLConnection::startUpdateZoneTransaction(int zone_id, bool replace) {
    if (mysql_query(conn_, "START TRANSACTION;") != 0) {
        isc_throw(DataSourceError, "failed to start a transaction");
    }
    if (replace) {
        const string flush_query = string("DELETE FROM records WHERE zone_id=")
            + lexical_cast<string>(zone_id) + ";";
        if (mysql_query(conn_, flush_query.c_str()) != 0) {
            isc_throw(DataSourceError, "failed to delete zone records");
        }
    }
}

void
MySQLConnection::commitUpdateZoneTransaction() {
    if (mysql_query(conn_, "COMMIT;") != 0) {
        isc_throw(DataSourceError, "failed to start a transaction");
    }
}

void
MySQLConnection::addRecordToZone(int zone_id,
                                 const vector<string>& record_params)
{
    string add_query = string("INSERT INTO records "
                              "(zone_id, name, rname, ttl, rdtype, "
                              "sigtype, rdata) VALUES(") +
        lexical_cast<string>(zone_id);
        
    BOOST_FOREACH(const string& param, record_params) {
        add_query += ", ";
        if (param.size() > 0) {
            add_query.push_back('\'');
            add_query += param;
            add_query.push_back('\'');
        } else {
            add_query += "NULL";
        }
    }
    add_query += ");";
    if (mysql_query(conn_, add_query.c_str()) != 0) {
        isc_throw(DataSourceError,
                  "failed to add a record to MySQL DB: " << add_query);
    }
}

void
MySQLConnection::resetQuery() {
    if (current_res_ != NULL) {
        mysql_free_result(current_res_);
        current_res_ = NULL;
    }
}

}
}
