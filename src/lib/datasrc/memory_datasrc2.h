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

#ifndef __MEMORY_DATA_SOURCE2_H
#define __MEMORY_DATA_SOURCE2_H 1

#include <datasrc/zonetable.h>
#include <datasrc/rdataset.h>
#include <datasrc/client.h>
#include <datasrc/memory_segment.h>
#include <datasrc/rbtree2.h>
#include <datasrc/treenode_rrset.h>
#include <datasrc/rdataset.h>
#include <datasrc/rdata_encoder.h>

#include <boost/interprocess/offset_ptr.hpp>
#include <boost/noncopyable.hpp>
#include <boost/function.hpp>
#include <boost/pool/object_pool.hpp>

#include <string>

namespace isc {
namespace dns {
class Name;
class RRsetList;
};

namespace datasrc {
namespace experimental {

// The tree stores domains
typedef RBTree<datasrc::internal::RdataSet> DomainTree;
typedef RBNode<datasrc::internal::RdataSet> DomainNode;
typedef RBNode<const datasrc::internal::RdataSet> ConstDomainNode;
typedef boost::interprocess::offset_ptr<RBNode<datasrc::internal::RdataSet> > DomainNodePtr;
typedef boost::interprocess::offset_ptr<const RBNode<datasrc::internal::RdataSet> > ConstDomainNodePtr;

typedef boost::interprocess::offset_ptr<datasrc::internal::RdataSet> RdataSetPtr;
typedef boost::interprocess::offset_ptr<const datasrc::internal::RdataSet> ConstRdataSetPtr;

typedef std::pair<const DomainNode*,
                  const datasrc::internal::RdataSet*> RRsetPair;

// need to be referenced in the implementation
typedef boost::function<void(dns::ConstRRsetPtr)> LoadCallback;

/// This class is non copyable.
class InMemoryZoneFinder : boost::noncopyable, public ZoneFinder {
private:
    /// \brief In-memory version of finder context.
    ///
    /// The implementation (and any specialized interface) is completely local
    /// to the InMemoryZoneFinder class, so it's defined as private
    struct TreeNodeResultContext;
    class Context;

    ///
    /// \name Constructors and Destructor.
public:
    InMemoryZoneFinder(const isc::dns::RRClass& rrclass,
                       const isc::dns::Name& origin);

    /// The destructor.
    virtual ~InMemoryZoneFinder();

    /// \brief Returns the origin of the zone.
    virtual isc::dns::Name getOrigin() const;

    /// \brief Returns the class of the zone.
    virtual isc::dns::RRClass getClass() const;

    /// \brief Find an RRset in the datasource
    virtual ZoneFinderContextPtr find(const isc::dns::Name& name,
                                      const isc::dns::RRType& type,
                                      const FindOptions options =
                                      FIND_DEFAULT);

    virtual ZoneFinderContextPtr findAll(
        const isc::dns::Name& name,
        std::vector<isc::dns::ConstRRsetPtr>& target,
        const FindOptions options = FIND_DEFAULT);

    virtual FindNSEC3Result
    findNSEC3(const isc::dns::Name& name, bool recursive);

    result::Result add(const isc::dns::ConstRRsetPtr& rrset);

    struct NullRRset : public InvalidParameter {
        NullRRset(const char* file, size_t line, const char* what) :
            InvalidParameter(file, line, what)
        {}
    };

    struct AddError : public InvalidParameter {
        AddError(const char* file, size_t line, const char* what) :
            InvalidParameter(file, line, what)
        {}
    };

    const std::string getFileName() const;

    void load(const std::string& filename);
    void load(ZoneIterator& iterator);

private:
    struct ZoneData;

    TreeNodeResultContext
    find(const dns::Name& name, dns::RRType type,
         std::vector<dns::ConstRRsetPtr>* target,
         const FindOptions options) const;

    void addValidation(const dns::ConstRRsetPtr rrset) const;
    void addFromLoad(const dns::ConstRRsetPtr& set, ZoneData* zone_data);
    result::Result add(const dns::ConstRRsetPtr& rrset, ZoneData& zone_data);
    void contextCheck(const dns::AbstractRRset& rrset,
                      datasrc::internal::ConstRdataSetPtr rdset_head) const;

    TreeNodeResultContext createFindResult(Result code, RRsetPair rrset,
                                           bool wild = false) const;

    // Common process for zone load.
    // rrset_installer is a functor that takes another functor as an argument,
    // and expected to call the latter for each RRset of the zone.  How the
    // sequence of the RRsets is generated depends on the internal
    // details  of the loader: either from a textual master file or from
    // another data source.
    // filename is the file name of the master file or empty if the zone is
    // loaded from another data source.
    void load(const std::string& filename,
              boost::function<void(LoadCallback)> rrset_installer);

    dns::RRClass zone_class_;
    dns::Name origin_;
    std::string file_name_;
    MemorySegment memory_segment_;
    datasrc::internal::RdataEncoder rdata_encoder_;

    ZoneData* zone_data_;
    mutable boost::object_pool<internal::TreeNodeRRset> rrset_pool_;
};


class InMemoryClient : public DataSourceClient {
public:
    InMemoryClient();
    ~InMemoryClient();

    virtual unsigned int getZoneCount() const;
    result::Result addZone(ZoneFinderPtr zone_finder);
    virtual FindResult findZone(const isc::dns::Name& name) const;
    virtual ZoneIteratorPtr getIterator(const isc::dns::Name& name,
                                        bool separate_rrs = false) const;
    virtual ZoneUpdaterPtr getUpdater(const isc::dns::Name& name,
                                      bool replace, bool journaling = false)
        const;
    virtual std::pair<ZoneJournalReader::Result, ZoneJournalReaderPtr>
    getJournalReader(const isc::dns::Name& zone, uint32_t begin_serial,
                     uint32_t end_serial) const;

private:
    // TODO: Do we still need the PImpl if nobody should manipulate this class
    // directly any more (it should be handled through DataSourceClient)?
    class InMemoryClientImpl;
    InMemoryClientImpl* impl_;
};
}
}
}
#endif  // __DATA_SOURCE_MEMORY_H
// Local Variables:
// mode: c++
// End:
