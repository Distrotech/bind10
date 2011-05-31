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

#ifndef __CLIENT_H
#define __CLIENT_H 1

#include <boost/shared_ptr.hpp>

#include <dns/rrset.h>

#include <cc/data.h>

#include <datasrc/zone.h>

namespace isc {
namespace dns {
class Name;
class RRset;
}
namespace datasrc {
class ZoneIterator;
typedef boost::shared_ptr<ZoneIterator> ZoneIteratorPtr;

class ZoneUpdater;
typedef boost::shared_ptr<ZoneUpdater> ZoneUpdaterPtr;

class DataSourceClient;
typedef boost::shared_ptr<DataSourceClient> DataSourceClientPtr;

class ZoneIterator {
protected:
    ZoneIterator() {}
public:
    virtual ~ZoneIterator() {}
    virtual isc::dns::ConstRRsetPtr getNextRRset() = 0;
};

class ZoneUpdater {
protected:
    ZoneUpdater() {}
public:
    virtual ~ZoneUpdater() {}
    virtual void addRRset(const isc::dns::RRset& rrset) = 0;
    virtual void commit() = 0;
};

class DataSourceClient {
protected:
    DataSourceClient() {}
public:
    /// \brief A helper structure to represent the search result of
    /// <code>MemoryDataSourceClient::find()</code>.
    ///
    /// This is a straightforward pair of the result code and a share pointer
    /// to the found zone to represent the result of \c find().
    /// We use this in order to avoid overloading the return value for both
    /// the result code ("success" or "not found") and the found object,
    /// i.e., avoid using \c NULL to mean "not found", etc.
    ///
    /// This is a simple value class with no internal state, so for
    /// convenience we allow the applications to refer to the members
    /// directly.
    ///
    /// See the description of \c find() for the semantics of the member
    /// variables.
    struct FindResult {
        FindResult(result::Result param_code, const ZoneHandlePtr param_zone) :
            code(param_code), zone(param_zone)
        {}
        const result::Result code;
        const ZoneHandlePtr zone;
    };

    virtual ~DataSourceClient() {}
#ifdef notyet
    virtual open();             // all or only for a particular zone
    virtual reopen();           // same
#endif
    // "param" should eventually be more generic.
    // might better be integrated to the constructor, but at the same time
    // it would be good if we had an option of delaying the actual open.
    virtual void open(isc::data::ConstElementPtr config) = 0;

    // eventually this will need to be non const member function
    // (unfortunately)
    virtual FindResult findZone(const isc::dns::Name& name) const = 0;

    // This will create a separate DB connection, etc
    // iterator should probably be per RR basis (or RRset but may not be
    // unique)
    virtual ZoneIteratorPtr createZoneIterator(const isc::dns::Name& name)
        const = 0;

    // This may have an option to use a separate DB connection, etc
    // note that in the original design of #374 we cannot parallelize updating
    // multiple (different) zones.
    virtual ZoneUpdaterPtr startUpdateZone(const isc::dns::Name& zone_name,
                                           bool replace) = 0;

#ifdef notyet
    virtual void dumpZone();    // synchronous, asynchronous
#endif
};
#ifdef sample
// make it responsible for relatively a smaller set of things.  in particular,
// exclude creation or deletion of zone from a table
//
// Each derived class internally holds specific handle like database
// connection.  That handle will be used for short term operations.
// For operations that (can) take longer time, dedicated sub-clients for
// the particular operation will be created from this client.
//
// maybe can be used with a cache, but that maybe passed to findRRset.
class DataSourceClient {
protected:
    DataSourceClient() {}
public:
    virtual ~DataSourceClient() {}
    virtual open();             // all or only for a particular zone
    virtual reopen();           // same
    virtual findZone();
    virtual findRRset();        // maybe should be moved to a separate class
    virtual void dumpZone();    // synchronous, asynchronous

    // This will create a separate DB connection, etc
    // iterator should probably be per RR basis (or RRset but may not be
    // unique)
    virtual createZoneIterator();

    // This will create a separate DB connection, etc
    // note that in the original design of #374 we cannot parallelize updating
    // multiple (different) zones.
    virtual ZoneUpdater beginUpdateZone(bool replace);
};

class DataBaseDataSourceClient : public DataSourceClient {
public:
    virtual void findRRset();
private:
    DataBaseConnection* conn_;
};

class MemoryDataSourceClient : public DataSourceClient {
};

class ZoneHandle {
public:
    virtual isc::dns::Name& getOrigin() const = 0;
    virtual findRRset(const isc::dns::Name& qname, isc::dns::RRType qtype) = 0;
};

class MemoryZoneHandle : ZoneHandle {
public:
    virtual isc::dns::Name& getOrigin() const;
    virtual findRRset(const isc::dns::Name& qname, isc::dns::RRType qtype);
};

class DatabaseZoneHandle : ZoneHandle {
};
#endif
}
}

#endif  // __CLIENT_H

// Local Variables: 
// mode: c++
// End: 
