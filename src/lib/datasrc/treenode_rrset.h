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

#ifndef __TREENODE_RRSET_H
#define __TREENODE_RRSET_H

#include <dns/name.h>
#include <dns/rrtype.h>
#include <dns/rrclass.h>
#include <dns/rrttl.h>
#include <dns/rrset.h>

#include <datasrc/rbtree2.h>
#include <datasrc/rdataset.h>

#include <boost/noncopyable.hpp>

#include <string>

namespace isc {
namespace datasrc {
namespace experimental {
namespace internal {
class CompressOffsetTable;

class TreeNodeRRset : public isc::dns::AbstractRRset, boost::noncopyable {

public:
    /// \brief Usual Constructor
    ///
    /// Creates an TreeNodeRRset from the pointer to the RRset passed to it.
    ///
    /// \param rrset Pointer to underlying RRset encapsulated by this object.
    TreeNodeRRset(dns::RRClass rrclass,
                  const RBNode<datasrc::internal::RdataSet>& node,
                  const datasrc::internal::RdataSet& rdset,
                  CompressOffsetTable* offset_table) :
        rrclass_(rrclass), node_(&node), rdset_(&rdset),
        offset_table_(offset_table)
    {}

    /// \brief Destructor
    virtual ~TreeNodeRRset() {}

    // Derived interfaces

    virtual unsigned int getRdataCount() const {
        return (rdset_->getRdataCount());
    }

    virtual const isc::dns::Name& getName() const;

    virtual const isc::dns::RRClass& getClass() const {
        return (rrclass_);
    }

    virtual const isc::dns::RRType& getType() const;

    virtual const isc::dns::RRTTL& getTTL() const;

    virtual void setName(const isc::dns::Name&);

    virtual void setTTL(const isc::dns::RRTTL&);

    virtual std::string toText() const;

    virtual bool isSameKind(const AbstractRRset& other) const;

    virtual unsigned int toWire(dns::AbstractMessageRenderer& renderer) const;

    virtual unsigned int toWire(util::OutputBuffer& buffer) const;

    virtual void addRdata(dns::rdata::ConstRdataPtr);

    virtual void addRdata(const dns::rdata::Rdata&);

    virtual isc::dns::RdataIteratorPtr getRdataIterator() const;

    virtual isc::dns::RRsetPtr getRRsig() const;

    // With all the RRsig methods, we have the problem that we store the
    // underlying RRset using a ConstRRsetPtr - a pointer to a "const" RRset -
    // but we need to modify it by adding or removing an RRSIG.  We overcome
    // this by temporarily violating the "const" nature of the RRset to add the
    // data.

    virtual void addRRsig(const dns::rdata::ConstRdataPtr& rdata);

    virtual void addRRsig(const dns::rdata::RdataPtr& rdata);

    virtual void addRRsig(const AbstractRRset& sigs);

    virtual void addRRsig(const dns::ConstRRsetPtr& sigs);

    virtual void addRRsig(const dns::RRsetPtr& sigs);

    virtual void removeRRsig();

    const datasrc::internal::RdataSet& getRdataSet() const {
        return (*rdset_);
    }

private:
    const dns::RRClass rrclass_;
    const RBNode<datasrc::internal::RdataSet>* node_;
    const datasrc::internal::RdataSet* rdset_;
    CompressOffsetTable* offset_table_;

    mutable uint8_t namebuf_[dns::Name::MAX_WIRE];
    mutable uint8_t offsetbuf_[dns::Name::MAX_LABELS];
};

typedef boost::shared_ptr<const TreeNodeRRset> ConstTreeNodeRRsetPtr;

}   // namespace internal
}   // namespace experimental
}   // namespace datasrc
}   // namespace isc

#endif  // __TREENODE_RRSET_H

// Local Variables:
// mode: c++
// End:
