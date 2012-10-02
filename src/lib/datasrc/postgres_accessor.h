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


#ifndef __DATASRC_POSTGRES_ACCESSOR_H
#define __DATASRC_POSTGRES_ACCESSOR_H

#include <datasrc/database.h>
#include <datasrc/data_source.h>

#include <exceptions/exceptions.h>

#include <libpq-fe.h>

#include <boost/enable_shared_from_this.hpp>
#include <boost/scoped_ptr.hpp>
#include <string>

#include <cc/data.h>

namespace isc {
namespace dns {
class RRClass;
}

namespace datasrc {

/**
 * \brief Low-level database error
 *
 * This exception is thrown when the MySQL library complains about something.
 * It might mean corrupt database file, invalid request or that something is
 * rotten in the library.
 */
class PostgresError : public DataSourceError {
public:
    PostgresError(const char* file, size_t line, const char* what) :
        DataSourceError(file, line, what) {}
};

class PostgresAccessor :
        public DatabaseAccessor,
        public boost::enable_shared_from_this<PostgresAccessor>
{
public:
    PostgresAccessor(const std::string& rrclass);

    ~PostgresAccessor();

    virtual boost::shared_ptr<DatabaseAccessor> clone();

    virtual std::pair<bool, int> getZone(const std::string& name) const;

    virtual std::pair<bool, int> getZone(const std::string& rname,
                                         std::string& zname_txt) const;

    virtual IteratorContextPtr getRecords(const std::string& name,
                                          int id,
                                          bool subdomains = false) const;

    virtual IteratorContextPtr getNSEC3Records(const std::string& hash,
                                               int id) const;

    virtual IteratorContextPtr getAllRecords(int id) const;

    virtual IteratorContextPtr
    getDiffs(int id, uint32_t start, uint32_t end) const;


    virtual std::pair<bool, int> startUpdateZone(const std::string& zone_name,
                                                 bool replace);

    virtual void startTransaction();

    virtual void commit();

    virtual void rollback();

    virtual void addRecordToZone(
        const std::string (&columns)[ADD_COLUMN_COUNT]);

    virtual void addNSEC3RecordToZone(
        const std::string (&columns)[ADD_NSEC3_COLUMN_COUNT]);

    virtual void deleteRecordInZone(
        const std::string (&params)[DEL_PARAM_COUNT]);

    virtual void deleteNSEC3RecordInZone(
        const std::string (&params)[DEL_PARAM_COUNT]);

    virtual void addRecordDiff(
        int zone_id, uint32_t serial, DiffOperation operation,
        const std::string (&params)[DIFF_PARAM_COUNT]);

    virtual const std::string& getDBName() const { return (database_name_); }

    virtual std::string findPreviousName(int zone_id, const std::string& rname)
        const;

    virtual std::string findPreviousNSEC3Hash(int zone_id,
                                              const std::string& hash) const;

private:
    /// \brief Postgres implementation of IteratorContext for all records
    class Context;

    PGconn* pgconn_;
    const std::string database_name_;
    const std::string rrclass_;
    const std::string rrclass_code_;
    int updated_zone_id_;
};

/// \brief Creates an instance of the Postgres datasource client
///
/// Currently the configuration passed here will be ignored.
///
/// This configuration setup is currently under discussion and will change in
/// the near future.
///
/// \param config The configuration for the datasource instance
/// \param error This string will be set to an error message if an error occurs
///              during initialization
/// \return An instance of the Postgres datasource client, or NULL if there was
///         an error
extern "C" DataSourceClient* createInstance(isc::data::ConstElementPtr config,
                                            std::string& error);

/// \brief Destroy the instance created by createInstance()
extern "C" void destroyInstance(DataSourceClient* instance);

}
}

#endif  // __DATASRC_POSTGRES_ACCESSOR_H

// Local Variables:
// mode: c++
// End:
