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

#ifndef __MEMORY_DATA_SOURCE_H
#define __MEMORY_DATA_SOURCE_H 1

#include <string>

#include <datasrc/client.h>
#include <datasrc/zonetable.h>

namespace isc {
namespace dns {
class Name;
class RRsetList;
};

namespace datasrc {

/// A derived zone class intended to be used with the memory data source.
class MemoryZoneHandle : public ZoneHandle {
    ///
    /// \name Constructors and Destructor.
    ///
    /// \b Note:
    /// The copy constructor and the assignment operator are intentionally
    /// defined as private, making this class non copyable.
    //@{
private:
    MemoryZoneHandle(const MemoryZoneHandle& source);
    MemoryZoneHandle& operator=(const MemoryZoneHandle& source);
public:
    /// \brief Constructor from zone parameters.
    ///
    /// This constructor internally involves resource allocation, and if
    /// it fails, a corresponding standard exception will be thrown.
    /// It never throws an exception otherwise.
    ///
    /// \param rrclass The RR class of the zone.
    /// \param origin The origin name of the zone.
    MemoryZoneHandle(const isc::dns::RRClass& rrclass,
                     const isc::dns::Name& origin);

    /// The destructor.
    virtual ~MemoryZoneHandle();
    //@}

    /// \brief Returns the origin of the zone.
    virtual const isc::dns::Name& getOrigin() const;

    /// \brief Returns the class of the zone.
    virtual const isc::dns::RRClass& getClass() const;

    /// \brief Looks up an RRset in the zone.
    ///
    /// See documentation in \c Zone.
    ///
    /// It returns NULL pointer in case of NXDOMAIN and NXRRSET,
    /// and also SUCCESS if target is not NULL(TYPE_ANY query).
    /// (the base class documentation does not seem to require that).
    virtual FindResult find(const isc::dns::Name& name,
                            const isc::dns::RRType& type,
                            isc::dns::RRsetList* target = NULL,
                            const FindOptions options = FIND_DEFAULT) const;

    /// \brief Inserts an rrset into the zone.
    ///
    /// It puts another RRset into the zone.
    ///
    /// Except for NullRRset and OutOfZone, this method does not guarantee
    /// strong exception safety (it is currently not needed, if it is needed
    /// in future, it should be implemented).
    ///
    /// \throw NullRRset \c rrset is a NULL pointer.
    /// \throw OutOfZone The owner name of \c rrset is outside of the
    /// origin of the zone.
    /// \throw AddError Other general errors.
    /// \throw Others This method might throw standard allocation exceptions.
    ///
    /// \param rrset The set to add.
    /// \return SUCCESS or EXIST (if an rrset for given name and type already
    ///    exists).
    result::Result add(const isc::dns::ConstRRsetPtr& rrset);

    /// \brief RRSet out of zone exception.
    ///
    /// This is thrown if addition of an RRset that doesn't belong under the
    /// zone's origin is requested.
    struct OutOfZone : public InvalidParameter {
        OutOfZone(const char* file, size_t line, const char* what) :
            InvalidParameter(file, line, what)
        { }
    };

    /// \brief RRset is NULL exception.
    ///
    /// This is thrown if the provided RRset parameter is NULL.
    struct NullRRset : public InvalidParameter {
        NullRRset(const char* file, size_t line, const char* what) :
            InvalidParameter(file, line, what)
        { }
    };

    /// \brief General failure exception for \c add().
    ///
    /// This is thrown against general error cases in adding an RRset
    /// to the zone.
    ///
    /// Note: this exception would cover cases for \c OutOfZone or
    /// \c NullRRset.  We'll need to clarify and unify the granularity
    /// of exceptions eventually.  For now, exceptions are added as
    /// developers see the need for it.
    struct AddError : public InvalidParameter {
        AddError(const char* file, size_t line, const char* what) :
            InvalidParameter(file, line, what)
        { }
    };

    /// Return the master file name of the zone
    ///
    /// This method returns the name of the zone's master file to be loaded.
    /// The returned string will be an empty unless the zone has successfully
    /// loaded a zone.
    ///
    /// This method should normally not throw an exception.  But the creation
    /// of the return string may involve a resource allocation, and if it
    /// fails, the corresponding standard exception will be thrown.
    ///
    /// \return The name of the zone file loaded in the zone, or an empty
    /// string if the zone hasn't loaded any file.
    const std::string getFileName() const;

    /// \brief Load zone from masterfile.
    ///
    /// This loads data from masterfile specified by filename. It replaces
    /// current content. The masterfile parsing ability is kind of limited,
    /// see isc::dns::masterLoad.
    ///
    /// This throws isc::dns::MasterLoadError if there is problem with loading
    /// (missing file, malformed, it contains different zone, etc - see
    /// isc::dns::masterLoad for details).
    ///
    /// In case of internal problems, OutOfZone, NullRRset or AssertError could
    /// be thrown, but they should not be expected. Exceptions caused by
    /// allocation may be thrown as well.
    ///
    /// If anything is thrown, the previous content is preserved (so it can
    /// be used to update the data, but if user makes a typo, the old one
    /// is kept).
    ///
    /// \param filename The master file to load.
    ///
    /// \todo We may need to split it to some kind of build and commit/abort.
    ///     This will probably be needed when a better implementation of
    ///     configuration reloading is written.
    void load(const std::string& filename);

    /// Exchanges the content of \c this zone with that of the given \c zone.
    ///
    /// This method never throws an exception.
    ///
    /// \param zone Another \c MemoryZoneHandle object which is to be swapped
    /// with
    /// \c this zone.
    void swap(MemoryZoneHandle& zone);

private:
    /// \name Hidden private data
    //@{
    struct MemoryZoneHandleImpl;
    MemoryZoneHandleImpl* impl_;
    //@}
};

/// \brief A data source that uses in memory dedicated backend.
///
/// The \c MemoryDataSourceClient class represents a data source and provides a
/// basic interface to help DNS lookup processing. For a given domain
/// name, its \c findZone() method searches the in memory dedicated backend
/// for the zone that gives a longest match against that name.
///
/// The in memory dedicated backend are assumed to be of the same RR class,
/// but the \c MemoryDataSourceClient class does not enforce the assumption through
/// its interface.
/// For example, the \c addZone() method does not check if the new zone is of
/// the same RR class as that of the others already in the dedicated backend.
/// It is caller's responsibility to ensure this assumption.
///
/// <b>Notes to developer:</b>
///
/// For now, we don't make it a derived class of AbstractDataSrc because the
/// interface is so different (we'll eventually consider this as part of the
/// generalization work).
///
/// The addZone() method takes a (Boost) shared pointer because it would be
/// inconvenient to require the caller to maintain the ownership of zones,
/// while it wouldn't be safe to delete unnecessary zones inside the dedicated
/// backend.
///
/// The findZone() method takes a domain name and returns the best matching \c
/// MemoryZoneHandle in the form of (Boost) shared pointer, so that it can provide
/// the general interface for all data sources.
class MemoryDataSourceClient : public DataSourceClient {
public:
    ///
    /// \name Constructors and Destructor.
    ///
    /// \b Note:
    /// The copy constructor and the assignment operator are intentionally
    /// defined as private, making this class non copyable.
    //@{
private:
    MemoryDataSourceClient(const MemoryDataSourceClient& source);
    MemoryDataSourceClient& operator=(const MemoryDataSourceClient& source);

public:
    /// Default constructor.
    ///
    /// This constructor internally involves resource allocation, and if
    /// it fails, a corresponding standard exception will be thrown.
    /// It never throws an exception otherwise.
    MemoryDataSourceClient();

    /// The destructor.
    ~MemoryDataSourceClient();
    //@}

    virtual void open(isc::data::ConstElementPtr config);

    /// Return the number of zones stored in the data source.
    ///
    /// This method never throws an exception.
    ///
    /// \return The number of zones stored in the data source.
    unsigned int getZoneCount() const;

    /// Add a \c Zone to the \c MemoryDataSourceClient.
    ///
    /// \c Zone must not be associated with a NULL pointer; otherwise
    /// an exception of class \c InvalidParameter will be thrown.
    /// If internal resource allocation fails, a corresponding standard
    /// exception will be thrown.
    /// This method never throws an exception otherwise.
    ///
    /// \param zone A \c Zone object to be added.
    /// \return \c result::SUCCESS If the zone is successfully
    /// added to the memory data source.
    /// \return \c result::EXIST The memory data source already
    /// stores a zone that has the same origin.
    result::Result addZone(ZoneHandlePtr zone);

    /// Find a \c Zone that best matches the given name in the \c MemoryDataSourceClient.
    ///
    /// It searches the internal storage for a \c Zone that gives the
    /// longest match against \c name, and returns the result in the
    /// form of a \c FindResult object as follows:
    /// - \c code: The result code of the operation.
    ///   - \c result::SUCCESS: A zone that gives an exact match
    //    is found
    ///   - \c result::PARTIALMATCH: A zone whose origin is a
    //    super domain of \c name is found (but there is no exact match)
    ///   - \c result::NOTFOUND: For all other cases.
    /// - \c zone: A "Boost" shared pointer to the found \c Zone object if one
    //  is found; otherwise \c NULL.
    ///
    /// This method never throws an exception.
    ///
    /// \param name A domain name for which the search is performed.
    /// \return A \c FindResult object enclosing the search result (see above).
    virtual FindResult findZone(const isc::dns::Name& name) const;

    virtual ZoneIteratorPtr createZoneIterator(const isc::dns::Name&) const {
        return (ZoneIteratorPtr());
    }

    // not implemented yet
    virtual ZoneUpdaterPtr startUpdateZone(const isc::dns::Name&, bool) {
        return (ZoneUpdaterPtr());
    }

private:
    class MemoryDataSourceClientImpl;
    MemoryDataSourceClientImpl* impl_;
};
}
}
#endif  // __DATA_SOURCE_MEMORY_H
// Local Variables:
// mode: c++
// End:
