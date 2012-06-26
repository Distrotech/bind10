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

#include <dns/name.h>
#include <dns/rrclass.h>
#include <dns/rrtype.h>
#include <dns/rrset.h>
#include <dns/masterload.h>

#include <datasrc/rdata_encoder.h>
#include <datasrc/logger.h>
#include <datasrc/iterator.h>
#include <datasrc/rbtree2.h>
#include <datasrc/treenode_rrset.h>
#include <datasrc/memory_segment.h>
#include <datasrc/rdataset.h>
#include <datasrc/memory_datasrc2.h>

#include <boost/interprocess/offset_ptr.hpp>
#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#include <boost/shared_ptr.hpp>

#include <exception>
#include <utility>

using namespace std;
using namespace isc::dns;
using namespace isc::dns::rdata;
using boost::shared_ptr;

namespace isc {
namespace datasrc {

using internal::RdataSet;

namespace experimental {

using internal::TreeNodeRRset;
using internal::ConstTreeNodeRRsetPtr;

namespace {
// Some type aliases

/*
 * Each domain consists of some RRsets. They will be looked up by the
 * RRType.
 *
 * The use of map is questionable with regard to performance - there'll
 * be usually only few RRsets in the domain, so the log n benefit isn't
 * much and a vector/array might be faster due to its simplicity and
 * continuous memory location. But this is unlikely to be a performance
 * critical place and map has better interface for the lookups, so we use
 * that.
 */

// In the following dedicated namespace we define a few application-specific
// RBNode flags.  We use a separate namespace so we can consolidate the
// definition in a single place, which would hopefully reduce the risk of
// collisions.
// (Note: it's within an unnamed namespace, so effectively private.)
namespace domain_flag {
// This flag indicates the node is at a "wildcard level" (in short, it means
// one of the node's immediate child is a wildcard).  See addWildcards()
// for more details.
const DomainNode::Flags WILD = DomainNode::FLAG_USER1;

// This flag is used for additional record shortcut.  If a node has this
// flag, it's under a zone cut for a delegation to a child zone.
// Note: for a statically built zone this information is stable, but if we
// change the implementation to be dynamically modifiable, it may not be
// realistic to keep this flag update for all affected nodes, and we may
// have to reconsider the mechanism.
const DomainNode::Flags GLUE = DomainNode::FLAG_USER2;

// This flag indicates the node is generated as a result of wildcard
// expansion.  In this implementation, this flag can be set only in
// the separate auxiliary tree of ZoneData (see the structure description).
const DomainNode::Flags WILD_EXPANDED = DomainNode::FLAG_USER3;
};

// Simple search helper on node's Rdata
ConstRdataSetPtr
findRdataSet(RRType rrtype, ConstRdataSetPtr rdset_head) {
    for (ConstRdataSetPtr rdset = rdset_head; rdset; rdset = rdset->next) {
        if (rdset->type == rrtype.getCode()) {
            return (rdset);
        }
    }
    return (NULL);
}

ConstRdataSetPtr
findExRdataSet(RRType rrtype, ConstRdataSetPtr rdset_head) {
    for (ConstRdataSetPtr rdset = rdset_head; rdset; rdset = rdset->next) {
        if (rdset->type != rrtype.getCode()) {
            return (rdset);
        }
    }
    return (NULL);
}
} // unnamed namespace

// Actual zone data: Essentially a set of zone's RRs.  This is defined as
// a separate structure so that it'll be replaceable on reload.
struct InMemoryZoneFinder::ZoneData {
    ZoneData(MemorySegment& segment, const Name& origin) :
        domains_(segment, true),
        origin_node_(NULL),
        nsec_signed_(false)
    {
        // We create the node for origin (it needs to exist anyway in future)
        DomainNode* origin_node = NULL;
        domains_.insert(origin, &origin_node);
        origin_node_ = origin_node;
    }

    struct FindNodeResult {
        // Bitwise flags to represent supplemental information of the
        // search result:
        // Search resulted in a wildcard match.
        static const unsigned int FIND_WILDCARD = 1;
        // Search encountered a zone cut due to NS but continued to look for
        // a glue.
        static const unsigned int FIND_ZONECUT = 2;

        FindNodeResult(ZoneFinder::Result code_param,
                       RRsetPair rrset_param,
                       unsigned int flags_param = 0) :
            code(code_param), rrset(rrset_param), flags(flags_param)
        {}
        const ZoneFinder::Result code;
        const RRsetPair rrset;
        const unsigned int flags;
    };

    FindNodeResult findNode(const LabelSequence& labels,
                            RBTreeNodeChain<RdataSet>& node_path,
                            ZoneFinder::FindOptions options) const;

    // The main data (name + RRsets)
    DomainTree domains_;

    // Shortcut to the origin node, which should always exist
    DomainNodePtr origin_node_;
    bool nsec_signed_; // True if there's at least one NSEC record

    // A helper method for NSEC-signed zones.  It searches the zone for
    // the "closest" NSEC corresponding to the search context stored in
    // node_path (it should contain sufficient information to identify the
    // previous name of the query name in the zone).  In some cases the
    // immediate closest name may not have NSEC (when it's under a zone cut
    // for glue records, or even when the zone is partly broken), so this
    // method continues the search until it finds a name that has NSEC,
    // and returns the one found first.  Due to the prerequisite (see below),
    // it should always succeed.
    //
    // node_path must store valid search context (in practice, it's expected
    // to be set by findNode()); otherwise the underlying RBTree implementation
    // throws.
    //
    // If the zone is not considered NSEC-signed or DNSSEC records were not
    // required in the original search context (specified in options), this
    // method doesn't bother to find NSEC, and simply returns NULL.  So, by
    // definition of "NSEC-signed", when it really tries to find an NSEC it
    // should succeed; there should be one at least at the zone origin.
    RRsetPair
    getClosestNSEC(RBTreeNodeChain<RdataSet>& node_path,
                   ZoneFinder::FindOptions options) const;
};

RRsetPair
InMemoryZoneFinder::ZoneData::getClosestNSEC(
    RBTreeNodeChain<RdataSet>& node_path,
    ZoneFinder::FindOptions options) const
{
    if (!nsec_signed_ || (options & ZoneFinder::FIND_DNSSEC) == 0) {
        return (RRsetPair(NULL, NULL));
    }

    const DomainNode* prev_node;
    while ((prev_node = domains_.previousNode(node_path)) != NULL) {
        if (!prev_node->isEmpty()) {
            ConstRdataSetPtr found =
                findRdataSet(RRType::NSEC(), prev_node->getData());
            if (found) {
                return (RRsetPair(prev_node, found.get()));
            }
        }
    }
    // This must be impossible and should be an internal bug.
    // See the description at the method declaration.
    assert(false);
    // Even though there is an assert here, strict compilers
    // will still need some return value.
    return (RRsetPair(NULL, NULL));
}

/// Maintain intermediate data specific to the search context used in
/// \c find().
///
/// It will be passed to \c cutCallback() (see below) and record a possible
/// zone cut node and related RRset (normally NS or DNAME).
struct FindState {
    FindState(bool glue_ok) :
        zonecut_node_(NULL),
        dname_node_(NULL),
        glue_ok_(glue_ok)
    {}

    // These will be set to a domain node of the highest delegation point,
    // if any.  In fact, we could use a single variable instead of both.
    // But then we would need to distinquish these two cases by something
    // else and it seemed little more confusing when this was written.
    const DomainNode* zonecut_node_;
    const DomainNode* dname_node_;

    // Delegation RdataSet (NS or DNAME), if found.
    const RdataSet* rdataset_;

    // Whether to continue search below a delegation point.
    // Set at construction time.
    const bool glue_ok_;
};

// A callback called from possible zone cut nodes and nodes with DNAME.
// This will be passed from findNode() to \c RBTree::find().
bool cutCallback(const DomainNode& node, FindState* state) {
    // We need to look for DNAME first, there's allowed case where
    // DNAME and NS coexist in the apex. DNAME is the one to notice,
    // the NS is authoritative, not delegation (corner case explicitly
    // allowed by section 3 of 2672)
    ConstRdataSetPtr found_dname = findRdataSet(RRType::DNAME(),
                                                node.getData());
    if (found_dname) {
        LOG_DEBUG(logger, DBG_TRACE_DETAILED, DATASRC_MEM_DNAME_ENCOUNTERED);
        state->dname_node_ = &node;
        state->rdataset_ = found_dname.get();
        // No more processing below the DNAME (RFC 2672, section 3
        // forbids anything to exist below it, so there's no need
        // to actually search for it). This is strictly speaking
        // a different way than described in 4.1 of that RFC,
        // but because of the assumption in section 3, it has the
        // same behaviour.
        return (true);
    }

    // Look for NS
    ConstRdataSetPtr found_ns = findRdataSet(RRType::NS(), node.getData());
    if (found_ns) {
        // We perform callback check only for the highest zone cut in the
        // rare case of nested zone cuts.
        if (state->zonecut_node_ != NULL) {
            return (false);
        }

        LOG_DEBUG(logger, DBG_TRACE_DETAILED, DATASRC_MEM_NS_ENCOUNTERED);

        // BIND 9 checks if this node is not the origin.  That's probably
        // because it can support multiple versions for dynamic updates
        // and IXFR, and it's possible that the callback is called at
        // the apex and the DNAME doesn't exist for a particular version.
        // It cannot happen for us (at least for now), so we don't do
        // that check.
        state->zonecut_node_ = &node;
        state->rdataset_ = found_ns.get();

        // Unless glue is allowed the search stops here, so we return
        // false; otherwise return true to continue the search.
        return (!state->glue_ok_);
    }

    // This case should not happen because we enable callback only
    // when we add an RR searched for above.
    assert(0);
    // This is here to avoid warning (therefore compilation error)
    // in case assert is turned off. Otherwise we could get "Control
    // reached end of non-void function".
    return (false);
}

// Implementation notes: this method identifies an RBT node that best matches
// the give name in terms of DNS query handling.  In many cases,
// DomainTree::find() will result in EXACTMATCH or PARTIALMATCH (note that
// the given name is generally expected to be contained in the zone, so
// even if it doesn't exist, it should at least match the zone origin).
// If it finds an exact match, that's obviously the best one.  The partial
// match case is more complicated.
//
// We first need to consider the case where search hits a delegation point,
// either due to NS or DNAME.  They are indicated as either dname_node_ or
// zonecut_node_ being non NULL.  Usually at most one of them will be
// something else than NULL (it might happen both are NULL, in which case we
// consider it NOT FOUND). There's one corner case when both might be
// something else than NULL and it is in case there's a DNAME under a zone
// cut and we search in glue OK mode ‒ in that case we don't stop on the
// domain with NS and ignore it for the answer, but it gets set anyway. Then
// we find the DNAME and we need to act by it, therefore we first check for
// DNAME and then for NS. In all other cases it doesn't matter, as at least
// one of them is NULL.
//
// Next, we need to check if the RBTree search stopped at a node for a
// subdomain of the search name (so the comparison result that stopped the
// search is "SUPERDOMAIN"), it means the stopping node is an empty
// non-terminal node.  In this case the search name is considered to exist
// but no data should be found there.
//
// If none of above is the case, we then consider whether there's a matching
// wildcard.  DomainTree::find() records the node if it encounters a
// "wildcarding" node, i.e., the immediate ancestor of a wildcard name
// (e.g., wild.example.com for *.wild.example.com), and returns it if it
// doesn't find any node that better matches the query name.  In this case
// we'll check if there's indeed a wildcard below the wildcarding node.
//
// Note, first, that the wildcard is checked after the empty
// non-terminal domain case above, because if that one triggers, it
// means we should not match according to 4.3.3 of RFC 1034 (the query
// name is known to exist).
//
// Before we try to find a wildcard, we should check whether there's
// an existing node that would cancel the wildcard match.  If
// DomainTree::find() stopped at a node which has a common ancestor
// with the query name, it might mean we are comparing with a
// non-wildcard node. In that case, we check which part is common. If
// we have something in common that lives below the node we got (the
// one above *), then we should cancel the match according to section
// 4.3.3 of RFC 1034 (as the name between the wildcard domain and the
// query name is known to exist).
//
// If there's no node below the wildcarding node that shares a common ancestor
// of the query name, we can conclude the wildcard is the best match.
// We'll then identify the wildcard node via an incremental search.  Note that
// there's no possibility that the query name is at an empty non terminal
// node below the wildcarding node at this stage; that case should have been
// caught above.
//
// If none of the above succeeds, we conclude the name doesn't exist in
// the zone.
InMemoryZoneFinder::ZoneData::FindNodeResult
InMemoryZoneFinder::ZoneData::findNode(const LabelSequence& labels,
                                       RBTreeNodeChain<RdataSet>& node_path,
                                       ZoneFinder::FindOptions options) const
{
    DomainNode* node = NULL;
    FindState state((options & ZoneFinder::FIND_GLUE_OK) != 0);

    const DomainTree::Result result =
        domains_.find(labels, &node, node_path, cutCallback, &state);
    const unsigned int zonecut_flag =
        (state.zonecut_node_ != NULL) ? FindNodeResult::FIND_ZONECUT : 0;
    if (result == DomainTree::EXACTMATCH) {
        return (FindNodeResult(ZoneFinder::SUCCESS,
                               RRsetPair(node, state.rdataset_),
                               zonecut_flag));
    } else if (result == DomainTree::PARTIALMATCH) {
        assert(node != NULL);
        if (state.dname_node_ != NULL) { // DNAME
            LOG_DEBUG(logger, DBG_TRACE_DATA, DATASRC_MEM_DNAME_FOUND).
                arg(state.dname_node_->getAbsoluteName());
            return (FindNodeResult(ZoneFinder::DNAME,
                                   RRsetPair(state.dname_node_,
                                             state.rdataset_)));
        }
        if (state.zonecut_node_ != NULL) { // DELEGATION due to NS
            LOG_DEBUG(logger, DBG_TRACE_DATA, DATASRC_MEM_DELEG_FOUND).
                arg(state.zonecut_node_->getAbsoluteName());
            return (FindNodeResult(ZoneFinder::DELEGATION,
                                   RRsetPair(state.zonecut_node_,
                                             state.rdataset_)));
        }
        if (node_path.getLastComparisonResult().getRelation() ==
            NameComparisonResult::SUPERDOMAIN) { // empty node, so NXRRSET
            LOG_DEBUG(logger, DBG_TRACE_DATA, DATASRC_MEM_SUPER_STOP).
                arg(labels);
            return (FindNodeResult(ZoneFinder::NXRRSET,
                                   getClosestNSEC(node_path, options)));
        }
        if (node->getFlag(domain_flag::WILD) && // maybe a wildcard, check only
            (options & ZoneFinder::NO_WILDCARD) == 0) { // if not disabled.
            if (node_path.getLastComparisonResult().getRelation() ==
                NameComparisonResult::COMMONANCESTOR) {
                // Wildcard canceled.  Treat it as NXDOMAIN.
                LOG_DEBUG(logger, DBG_TRACE_DATA,
                          DATASRC_MEM_WILDCARD_CANCEL).arg(labels);
                return (FindNodeResult(ZoneFinder::NXDOMAIN,
                                       getClosestNSEC(node_path, options)));
            }
            // Now the wildcard should be the best match.
            // XXX: this is expensive.
            const Name wildcard(Name("*").concatenate(
                                    node_path.getAbsoluteName()));

            // Clear the node_path so that we don't keep incorrect (NSEC)
            // context
            node_path.clear();
            DomainTree::Result result(domains_.find(wildcard, &node,
                                                    node_path));
            // Otherwise, why would the domain_flag::WILD be there if
            // there was no wildcard under it?
            assert(result == DomainTree::EXACTMATCH);
            return (FindNodeResult(ZoneFinder::SUCCESS,
                                   RRsetPair(node, state.rdataset_),
                                   FindNodeResult::FIND_WILDCARD |
                                   zonecut_flag));
        }
        // Nothing really matched.
        LOG_DEBUG(logger, DBG_TRACE_DATA, DATASRC_MEM_NOT_FOUND).arg(labels);
        return (FindNodeResult(ZoneFinder::NXDOMAIN,
                               getClosestNSEC(node_path, options)));
    } else {
        // If the name is neither an exact or partial match, it is
        // out of bailiwick, which is considered an error.
        isc_throw(OutOfZone, labels << " not in origin " <<
                  origin_node_->getAbsoluteName());
    }
}

///
/// Searching Zones
///

namespace {
shared_ptr<TreeNodeRRset>
createRRsetPtr(boost::object_pool<TreeNodeRRset>* pool, RRClass rrclass,
               const DomainNode& node, const RdataSet& rdset)
{
    TreeNodeRRset* p = pool->construct(rrclass, node, rdset);
    if (p == NULL) {
        throw bad_alloc();
    }
    return (shared_ptr<TreeNodeRRset>(
                p, boost::bind(&boost::object_pool<TreeNodeRRset>::destroy,
                               pool, _1)));
}
}

// Specialized version of ZoneFinder::ResultContext, which specifically
// holds rrset in the form of RBNodeRRset.
struct InMemoryZoneFinder::TreeNodeResultContext {
    /// \brief Constructor
    ///
    /// The first three parameters correspond to those of
    /// ZoneFinder::ResultContext.  If node is non NULL, it specifies the
    /// found in the tree search.
    TreeNodeResultContext(ZoneFinder::Result code_param,
                          ConstTreeNodeRRsetPtr rrset_param,
                          ZoneFinder::FindResultFlags flags_param,
                          const DomainNode* node,
                          const ZoneData& zone_data_param) :
        code(code_param), rrset(rrset_param), flags(flags_param),
        found_node(node), zone_data(zone_data_param)
    {}

    const ZoneFinder::Result code;
    const ConstTreeNodeRRsetPtr rrset;
    const ZoneFinder::FindResultFlags flags;
    const DomainNode* const found_node;
    const ZoneData& zone_data;
};

class InMemoryZoneFinder::Context : public ZoneFinder::Context {
public:
    /// \brief Constructor.
    ///
    /// Note that we don't have a specific constructor for the findAll() case.
    /// For (successful) type ANY query, found_node points to the
    /// corresponding RB node, which is recorded within this specialized
    /// context.
    Context(InMemoryZoneFinder& finder, ZoneFinder::FindOptions options,
            const TreeNodeResultContext& result) :
        ZoneFinder::Context(finder, options,
                            ResultContext(result.code, result.rrset,
                                          result.flags)),
        options_(options), rrset_(result.rrset),
        found_node_(result.found_node), finder_(finder)
    {}

protected:
    virtual void getAdditionalImpl(const vector<RRType>& requested_types,
                                   vector<ConstRRsetPtr>& result)
    {
        if (!rrset_) {
            // In this case this context should encapsulate the result of
            // findAll() and found_node_ should point to a valid answer node.
            if (found_node_ == NULL || found_node_->isEmpty()) {
                isc_throw(isc::Unexpected,
                          "Invalid call to in-memory getAdditional: caller's "
                          "bug or broken zone");
            }
            for (ConstRdataSetPtr rdset = found_node_->getData();
                 rdset;
                 rdset = rdset->next)
            {
                getAdditionalForRdataset(*rdset, requested_types, result,
                                         options_);
            }
        } else {
            getAdditionalForRdataset(rrset_->getRdataSet(), requested_types,
                                     result, options_);
        }
    }

private:
    // Retrieve additional RRsets for a given RRset associated in the context.
    // The process is straightforward: it examines the link to
    // AdditionalNodeInfo vector (if set), and find RRsets of the requested
    // type for each node.
    void
    getAdditionalForRdataset(const RdataSet& rdset,
                             const vector<RRType>& /*requested_types*/,
                             vector<ConstRRsetPtr>& /*result*/,
                             ZoneFinder::FindOptions orig_options) const
    {
        ZoneFinder::FindOptions options = ZoneFinder::FIND_DEFAULT;
        if ((orig_options & ZoneFinder::FIND_DNSSEC) != 0) {
            options = options | ZoneFinder::FIND_DNSSEC;
        }

        datasrc::internal::RdataIterator rd_it(
            RRType(rdset.type), rdset.getRdataCount(),
            rdset.getNameBuf(), NULL, NULL,
            NULL,
#ifdef notyet
            boost::bind(&Context::findAdditional, this, &requested_types,
                        &result, options, _1, _2),
#endif
            NULL);
        while (!rd_it.isLast()) {
            rd_it.action();
        }
    }

    void
    findAdditional(const vector<RRType>* requested_types,
                   vector<ConstRRsetPtr>* result,
                   ZoneFinder::FindOptions options,
                   const uint8_t* encoded_ndata, unsigned int attributes) const
    {
        if ((attributes &
             datasrc::internal::RdataFieldSpec::ADDITIONAL_NAME) == 0) {
            return;
        }
        const size_t nlen = encoded_ndata[0];
        const size_t olen = encoded_ndata[1];
        const LabelSequence seq(encoded_ndata + 2, encoded_ndata + 2 + nlen,
                                olen);
        RBTreeNodeChain<RdataSet> node_path;
        const ZoneData::FindNodeResult node_result =
            finder_.zone_data_->findNode(seq, node_path,
                                         options | ZoneFinder::FIND_GLUE_OK);

        // Note: ignore corner case conditions like wildcard or GLUE_OK or not
        const vector<RRType>::const_iterator it_end = requested_types->end();
        if (node_result.code == SUCCESS) {
            const DomainNode* node = node_result.rrset.first;
            for (ConstRdataSetPtr rdset = node->getData();
                 rdset;
                 rdset = rdset->next)
            {
                for (vector<RRType>::const_iterator it =
                         requested_types->begin();
                     it != it_end;
                     ++it) {
                    if (RRType(rdset->type) == (*it)) {
                        ConstTreeNodeRRsetPtr rrset =
                            createRRsetPtr(&finder_.rrset_pool_,
                                           finder_.zone_class_, *node, *rdset);
                        result->push_back(rrset);
                    }
                }
            }
        }
        return;
    }

    const ZoneFinder::FindOptions options_;
    const ConstTreeNodeRRsetPtr rrset_;
    const DomainNode* const found_node_;
    const InMemoryZoneFinder& finder_;
};

InMemoryZoneFinder::TreeNodeResultContext
InMemoryZoneFinder::createFindResult(Result code, RRsetPair rrset, 
                                     bool wild) const
{
    FindResultFlags flags = RESULT_DEFAULT;
    if (wild) {
        flags = flags | RESULT_WILDCARD;
    }
    if (code == NXRRSET || code == NXDOMAIN || wild) {
#if 0
        if (zone_data_->nsec3_data_) {
            flags = flags | RESULT_NSEC3_SIGNED;
        }
#endif
        if (zone_data_->nsec_signed_) {
            flags = flags | RESULT_NSEC_SIGNED;
        }
    }
    if (rrset.first != NULL && rrset.second != NULL) {
        return (TreeNodeResultContext(
                    code, createRRsetPtr(&rrset_pool_, zone_class_,
                                         *rrset.first, *rrset.second), flags,
                    rrset.first, *zone_data_));
    }
    return (TreeNodeResultContext(code, ConstTreeNodeRRsetPtr(), flags,
                                  rrset.first, *zone_data_));
}

// Implementation of InMemoryZoneFinder::find
InMemoryZoneFinder::TreeNodeResultContext
InMemoryZoneFinder::find(
    const Name& name, RRType type, std::vector<ConstRRsetPtr>* target,
    const FindOptions options) const
{
    LOG_DEBUG(logger, DBG_TRACE_BASIC, DATASRC_MEM_FIND).arg(name).
        arg(type);

    // Get the node.  All other cases than an exact match are handled
    // in findNode().  We simply construct a result structure and return.
    RBTreeNodeChain<RdataSet> node_path; // findNode will fill in this
    const ZoneData::FindNodeResult node_result =
        zone_data_->findNode(LabelSequence(name), node_path, options);
    if (node_result.code != SUCCESS) {
        return (createFindResult(node_result.code, node_result.rrset));
    }

    // We've found an exact match, may or may not be a result of wildcard.
    const DomainNode* node = node_result.rrset.first;
    assert(node != NULL);
    const bool rename = ((node_result.flags &
                          ZoneData::FindNodeResult::FIND_WILDCARD) != 0);

    // If there is an exact match but the node is empty, it's equivalent
    // to NXRRSET.
    if (node->isEmpty()) {
        LOG_DEBUG(logger, DBG_TRACE_DATA, DATASRC_MEM_DOMAIN_EMPTY).
            arg(name);
        return (createFindResult(NXRRSET,
                                 zone_data_->getClosestNSEC(node_path,
                                                            options),
                                 rename));
    }

    ConstRdataSetPtr found;

    // For simplicity for now
    if (rename) {
        LOG_DEBUG(logger, DBG_TRACE_DETAILED, DATASRC_MEM_RENAME).
            arg(node->getAbsoluteName()).arg(name);
        isc_throw(isc::NotImplemented, "wildcard substation not supported");
    }

    // If the node callback is enabled, this may be a zone cut.  If it
    // has a NS RR, we should return a delegation, but not in the apex.
    // There is one exception: the case for DS query, which should always
    // be considered in-zone lookup.
    if (node->getFlag(DomainNode::FLAG_CALLBACK) &&
        node != zone_data_->origin_node_.get() && type != RRType::DS()) {
        found = findRdataSet(RRType::NS(), node->getData());
        if (found) {
            LOG_DEBUG(logger, DBG_TRACE_DATA,
                      DATASRC_MEM_EXACT_DELEGATION).arg(name);
            return (createFindResult(DELEGATION,
                                     RRsetPair(node, found.get())));
        }
    }

    // handle type any query
    if (target != NULL) {
        // Empty domain will be handled as NXRRSET by normal processing
        for (found = node->getData(); found; found = found->next) {
            target->push_back(createRRsetPtr(&rrset_pool_, zone_class_, *node,
                                             *found));
        }
        LOG_DEBUG(logger, DBG_TRACE_DATA, DATASRC_MEM_ANY_SUCCESS).
            arg(name);
        return (createFindResult(SUCCESS, RRsetPair(NULL, NULL), rename));
    }

    ConstRdataSetPtr found_cname;
    for (found = node->getData(); found; found = found->next) {
        if (RRType(found->type) == type) {
            // Good, it is here
            LOG_DEBUG(logger, DBG_TRACE_DATA, DATASRC_MEM_SUCCESS).arg(name).
                arg(type);
            return (createFindResult(SUCCESS, RRsetPair(node, found.get()),
                                     rename));
        }
        // We should have ensured CNAME and other type shouldn't coexist
        if (RRType(found->type) == RRType::CNAME()) {
            found_cname = found;
        }
    }

    if (found_cname) {
        // We've found CNAME, and that's the best one.
        LOG_DEBUG(logger, DBG_TRACE_DATA, DATASRC_MEM_CNAME).arg(name);
        return (createFindResult(CNAME, RRsetPair(node, found.get()), rename));
    }

    // No exact match or CNAME.  Get NSEC if necessary and return NXRRSET.
    if (zone_data_->nsec_signed_ && (options & FIND_DNSSEC) != 0 &&
        (found = findRdataSet(RRType::NSEC(), node->getData()))) {
        return (createFindResult(NXRRSET, RRsetPair(node, found.get()),
                                 rename));
    }
    return (createFindResult(NXRRSET, RRsetPair(NULL, NULL), rename));
}

///
/// Loading Zones
///
namespace {
// Add the necessary magic for any wildcard contained in 'name'
// (including itself) to be found in the zone.
//
// In order for wildcard matching to work correctly in find(),
// we must ensure that a node for the wildcarding level exists in the
// backend RBTree.
// E.g. if the wildcard name is "*.sub.example." then we must ensure
// that "sub.example." exists and is marked as a wildcard level.
// Note: the "wildcarding level" is for the parent name of the wildcard
// name (such as "sub.example.").
//
// We also perform the same trick for empty wild card names possibly
// contained in 'name' (e.g., '*.foo.example' in 'bar.*.foo.example').
void
addWildcards(DomainTree& domains, const Name& name, const Name& origin) {
    Name wname(name);
    const unsigned int labels(wname.getLabelCount());
    const unsigned int origin_labels(origin.getLabelCount());
    for (unsigned int l = labels;
         l > origin_labels;
         --l, wname = wname.split(1)) {
        if (wname.isWildcard()) {
            LOG_DEBUG(logger, DBG_TRACE_DATA, DATASRC_MEM_ADD_WILDCARD).
                arg(name);
            // Ensure a separate level exists for the "wildcarding" name,
            // and mark the node as "wild".
            DomainNode* node;
            DomainTree::Result result(domains.insert(wname.split(1),
                                                     &node));
            assert(result == DomainTree::SUCCESS ||
                   result == DomainTree::ALREADYEXISTS);
            node->setFlag(domain_flag::WILD);

            // Ensure a separate level exists for the wildcard name.
            // Note: for 'name' itself we do this later anyway, but the
            // overhead should be marginal because wildcard names should
            // be rare.
            result = domains.insert(wname, &node);
            assert(result == DomainTree::SUCCESS ||
                   result == DomainTree::ALREADYEXISTS);
        }
    }
}
} // unnamed namespace

/*
 * Does some checks in context of the data that are already in the zone.
 * Currently checks for forbidden combinations of RRsets in the same
 * domain (CNAME+anything, DNAME+NS).
 *
 * If such condition is found, it throws AddError.
 */
void
InMemoryZoneFinder::contextCheck(const AbstractRRset& rrset,
                                 ConstRdataSetPtr rdset_head) const
{
    // Ensure CNAME and other type of RR don't coexist for the same
    // owner name except with NSEC, which is the only RR that can coexist
    // with CNAME (and also RRSIG, which is handled separately)
    if (rrset.getType() == RRType::CNAME()) {
        
        if (findExRdataSet(RRType::NSEC(), rdset_head)) { 
            LOG_ERROR(logger, DATASRC_MEM_CNAME_TO_NONEMPTY).
                arg(rrset.getName());
            isc_throw(AddError, "CNAME can't be added with other data for "
                      << rrset.getName());
        }
    } else if (rrset.getType() != RRType::NSEC() &&
               findRdataSet(RRType::CNAME(), rdset_head)) {
        LOG_ERROR(logger, DATASRC_MEM_CNAME_COEXIST).arg(rrset.getName());
        isc_throw(AddError, "CNAME and " << rrset.getType() <<
                  " can't coexist for " << rrset.getName());
    }

    /*
     * Similar with DNAME, but it must not coexist only with NS and only in
     * non-apex domains.
     * RFC 2672 section 3 mentions that it is implied from it and RFC 2181
     */
    if (rrset.getName() != origin_ &&
        // Adding DNAME, NS already there
        ((rrset.getType() == RRType::DNAME() &&
          findRdataSet(RRType::NS(), rdset_head)) ||
         // Adding NS, DNAME already there
         (rrset.getType() == RRType::NS() &&
          findRdataSet(RRType::DNAME(), rdset_head))))
    {
        LOG_ERROR(logger, DATASRC_MEM_DNAME_NS).arg(rrset.getName());
        isc_throw(AddError, "DNAME can't coexist with NS in non-apex "
                  "domain " << rrset.getName());
    }
}

// Validate rrset before adding it to the zone.  If something is wrong
// it throws an exception.  It doesn't modify the zone, and provides
// the strong exception guarantee.
void
InMemoryZoneFinder::addValidation(const ConstRRsetPtr rrset) const {
    if (!rrset) {
        isc_throw(NullRRset, "The rrset provided is NULL");
    }
    if (rrset->getRdataCount() == 0) {
        isc_throw(AddError, "The rrset provided is empty: " <<
                  rrset->getName() << "/" << rrset->getType());
    }
    // Check for singleton RRs. It should probably handled at a different
    // layer in future.
    if ((rrset->getType() == RRType::CNAME() ||
         rrset->getType() == RRType::DNAME()) &&
        rrset->getRdataCount() > 1)
    {
        // XXX: this is not only for CNAME or DNAME. We should generalize
        // this code for all other "singleton RR types" (such as SOA) in a
        // separate task.
        LOG_ERROR(logger, DATASRC_MEM_SINGLETON).arg(rrset->getName()).
            arg(rrset->getType());
        isc_throw(AddError, "multiple RRs of singleton type for "
                  << rrset->getName());
    }
    // NSEC3/NSEC3PARAM is not a "singleton" per protocol, but this
    // implementation requests it be so at the moment.
    if ((rrset->getType() == RRType::NSEC3() ||
         rrset->getType() == RRType::NSEC3PARAM()) &&
        rrset->getRdataCount() > 1) {
        isc_throw(AddError, "Multiple NSEC3/NSEC3PARAM RDATA is given for "
                  << rrset->getName() << " which isn't supported");
    }

    NameComparisonResult compare(origin_.compare(rrset->getName()));
    if (compare.getRelation() != NameComparisonResult::SUPERDOMAIN &&
        compare.getRelation() != NameComparisonResult::EQUAL)
    {
        LOG_ERROR(logger, DATASRC_MEM_OUT_OF_ZONE).arg(rrset->getName()).
            arg(origin_);
        isc_throw(OutOfZone, "The name " << rrset->getName() <<
                  " is not contained in zone " << origin_);
    }

    // Some RR types do not really work well with a wildcard.
    // Even though the protocol specifically doesn't completely ban such
    // usage, we refuse to load a zone containing such RR in order to
    // keep the lookup logic simpler and more predictable.
    // See RFC4592 and (for DNAME) draft-ietf-dnsext-rfc2672bis-dname
    // for more technical background.  Note also that BIND 9 refuses
    // NS at a wildcard, so in that sense we simply provide compatible
    // behavior.
    if (rrset->getName().isWildcard()) {
        if (rrset->getType() == RRType::NS()) {
            LOG_ERROR(logger, DATASRC_MEM_WILDCARD_NS).
                arg(rrset->getName());
            isc_throw(AddError, "Invalid NS owner name (wildcard): " <<
                      rrset->getName());
        }
        if (rrset->getType() == RRType::DNAME()) {
            LOG_ERROR(logger, DATASRC_MEM_WILDCARD_DNAME).
                arg(rrset->getName());
            isc_throw(AddError, "Invalid DNAME owner name (wildcard): " <<
                      rrset->getName());
        }
    }

    // Owner names of NSEC3 have special format as defined in RFC5155,
    // and cannot be a wildcard name or must be one label longer than
    // the zone origin.  While the RFC doesn't prohibit other forms of
    // names, no sane zone would have such names for NSEC3.
    // BIND 9 also refuses NSEC3 at wildcard.
    if (rrset->getType() == RRType::NSEC3() &&
        (rrset->getName().isWildcard() ||
         rrset->getName().getLabelCount() !=
         origin_.getLabelCount() + 1)) {
        LOG_ERROR(logger, DATASRC_BAD_NSEC3_NAME).
            arg(rrset->getName());
        isc_throw(AddError, "Invalid NSEC3 owner name: " <<
                  rrset->getName());
    }
}

/*
 * Implementation of longer methods. We put them here, because the
 * access is without the impl_-> and it will get inlined anyway.
 */
result::Result
InMemoryZoneFinder::add(const ConstRRsetPtr& rrset, ZoneData& zone_data) {
    // Sanitize input.  This will cause an exception to be thrown
    // if the input RRset is empty.
    addValidation(rrset);

    // OK, can add the RRset.
    LOG_DEBUG(logger, DBG_TRACE_DATA, DATASRC_MEM_ADD_RRSET).
        arg(rrset->getName()).arg(rrset->getType()).arg(origin_);

    if (rrset->getType() == RRType::NSEC3()) {
        isc_throw(isc::NotImplemented, "adding NSEC3 isn't supported");
    }

    // RRSIGs are special in various points, so we handle it in a
    // separate dedicated method.
    if (rrset->getType() == RRType::RRSIG()) {
        // Ignore RRSIG for now.
        return (result::SUCCESS);
    }


    // Add wildcards possibly contained in the owner name to the domain
    // tree.
    // Note: this can throw an exception, breaking strong exception
    // guarantee.  (see also the note for contextCheck() below).
    addWildcards(zone_data.domains_, rrset->getName(), origin_);

    // Get the node
    DomainNode* node;
    DomainTree::Result result = zone_data.domains_.insert(rrset->getName(),
                                                          &node);
    // Just check it returns reasonable results
    assert((result == DomainTree::SUCCESS ||
            result == DomainTree::ALREADYEXISTS) && node!= NULL);

    // Now get the domain
    RdataSetPtr const rdset_head = node->getData(); // current head
    if (rdset_head) {
        // Checks related to the surrounding data.
        // Note: when the check fails and the exception is thrown, it may
        // break strong exception guarantee.  At the moment we prefer
        // code simplicity and don't bother to introduce complicated
        // recovery code.
        contextCheck(*rrset, rdset_head);

        // Try inserting the rrset there
        if (findRdataSet(rrset->getType(), rdset_head)) {
            // The RRSet of given type was already there
            return (result::EXIST);
        }
    }

    // Note: this is not exception safe.
    RdataSetPtr rdset = RdataSet::allocate(memory_segment_, rdata_encoder_,
                                           NULL, rrset, ConstRRsetPtr());
    if (rdset_head) {
        // Append the new one at the end of list; normally in a zone file
        // minor records are placed later.
        RdataSetPtr prev = rdset_head;
        for (; prev->next; prev = prev->next) {
            ;
        }
        prev->next = rdset;
    } else {
        node->setData(rdset);
    }
    
    // Ok, we just put it in

    // If this RRset creates a zone cut at this node, mark the node
    // indicating the need for callback in find().
    if (rrset->getType() == RRType::NS() &&
        rrset->getName() != origin_) {
        node->setFlag(DomainNode::FLAG_CALLBACK);
        // If it is DNAME, we have a callback as well here
    } else if (rrset->getType() == RRType::DNAME()) {
        node->setFlag(DomainNode::FLAG_CALLBACK);
    }

    // If we've added NSEC3PARAM at zone origin, set up NSEC3 specific
    // data or check consistency with already set up parameters.
    if (rrset->getType() == RRType::NSEC3PARAM() &&
        rrset->getName() == origin_) {
#ifdef notyet
        // We know rrset has exactly one RDATA
        const generic::NSEC3PARAM& param =
            dynamic_cast<const generic::NSEC3PARAM&>(
                rrset->getRdataIterator()->getCurrent());

        if (!zone_data.nsec3_data_) {
            zone_data.nsec3_data_.reset(
                new ZoneData::NSEC3Data(param));
        } else if (!zone_data.nsec3_data_->hash_->match(param)) {
            isc_throw(AddError, "NSEC3PARAM with inconsistent "
                      "parameters: " << rrset->toText());
        }
#endif
    } else if (rrset->getType() == RRType::NSEC()) {
        // If it is NSEC signed zone, so we put a flag there
        // (flag is enough)
        zone_data.nsec_signed_ = true;
    }
    return (result::SUCCESS);
}

/*
 * Same as above, but it checks the return value and if it already exists,
 * it throws.
 */
void
InMemoryZoneFinder::addFromLoad(const ConstRRsetPtr& set, ZoneData* zone_data)
{
    switch (add(set, *zone_data)) {
    case result::EXIST:
        LOG_ERROR(logger, DATASRC_MEM_DUP_RRSET).
            arg(set->getName()).arg(set->getType());
        isc_throw(dns::MasterLoadError, "Duplicate rrset: " <<
                  set->toText());
    case result::SUCCESS:
        return;
    default:
        assert(0);
    }
}

InMemoryZoneFinder::InMemoryZoneFinder(const RRClass& zone_class,
                                       const Name& origin) :
    zone_class_(zone_class), origin_(origin), zone_data_(NULL)
{
    LOG_DEBUG(logger, DBG_TRACE_BASIC, DATASRC_MEM_CREATE).arg(origin).
        arg(zone_class);
}

InMemoryZoneFinder::~InMemoryZoneFinder() {
    LOG_DEBUG(logger, DBG_TRACE_BASIC, DATASRC_MEM_DESTROY).arg(getOrigin()).
        arg(getClass());
}

Name
InMemoryZoneFinder::getOrigin() const {
    return (origin_);
}

RRClass
InMemoryZoneFinder::getClass() const {
    return (zone_class_);
}

ZoneFinderContextPtr
InMemoryZoneFinder::find(const Name& name, const RRType& type,
                         const FindOptions options)
{
    return (ZoneFinderContextPtr(
                new Context(*this, options, find(name, type, NULL, options))));
}

ZoneFinderContextPtr
InMemoryZoneFinder::findAll(const Name& name,
                            std::vector<ConstRRsetPtr>& target,
                            const FindOptions options)
{
    return (ZoneFinderContextPtr(
                new Context(*this, options, find(name, RRType::ANY(),
                                                 &target, options))));
}

ZoneFinder::FindNSEC3Result
InMemoryZoneFinder::findNSEC3(const Name&, bool) {
    isc_throw(isc::NotImplemented, "findNSEC3 doesn't work for now");
}

result::Result
InMemoryZoneFinder::add(const ConstRRsetPtr& rrset) {
    return (add(rrset, *zone_data_));
}

namespace {
// A wrapper for dns::masterLoad used by load() below.  Essentially it
// converts the two callback types.  Note the mostly redundant wrapper of
// boost::bind.  It converts function<void(ConstRRsetPtr)> to
// function<void(RRsetPtr)> (masterLoad() expects the latter).  SunStudio
// doesn't seem to do this conversion if we just pass 'callback'.
void
masterLoadWrapper(const char* const filename, const Name& origin,
                  const RRClass& zone_class, LoadCallback callback)
{
    masterLoad(filename, origin, zone_class, boost::bind(callback, _1));
}

// The installer called from Impl::load() for the iterator version of load().
void
generateRRsetFromIterator(ZoneIterator* iterator, LoadCallback callback) {
    ConstRRsetPtr rrset;
    vector<ConstRRsetPtr> rrsigs; // placeholder for RRSIGs until "commitable".

    // The current internal implementation assumes an RRSIG is always added
    // after the RRset they cover.  So we store any RRSIGs in 'rrsigs' until
    // it's safe to add them; based on our assumption if the owner name
    // changes, all covered RRsets of the previous name should have been
    // installed and any pending RRSIGs can be added at that point.  RRSIGs
    // of the last name from the iterator must be added separately.
    while ((rrset = iterator->getNextRRset()) != NULL) {
        if (!rrsigs.empty() && rrset->getName() != rrsigs[0]->getName()) {
            BOOST_FOREACH(ConstRRsetPtr sig_rrset, rrsigs) {
                callback(sig_rrset);
            }
            rrsigs.clear();
        }
        if (rrset->getType() == RRType::RRSIG()) {
            rrsigs.push_back(rrset);
        } else {
            callback(rrset);
        }
    }

    BOOST_FOREACH(ConstRRsetPtr sig_rrset, rrsigs) {
        callback(sig_rrset);
    }
}
}

void
InMemoryZoneFinder::load(const string& filename,
                         boost::function<void(LoadCallback)> rrset_installer)
{
    auto_ptr<ZoneData> tmp(new ZoneData(memory_segment_, origin_));

    rrset_installer(boost::bind(&InMemoryZoneFinder::addFromLoad, this, _1,
                                tmp.get()));

    // If it went well, put it inside
    file_name_ = filename;
    delete zone_data_;
    zone_data_ = tmp.release();
}

void
InMemoryZoneFinder::load(const std::string& filename) {
    LOG_DEBUG(logger, DBG_TRACE_BASIC, DATASRC_MEM_LOAD).arg(getOrigin()).
        arg(filename);

    load(filename,
         boost::bind(masterLoadWrapper, filename.c_str(), getOrigin(),
                     getClass(), _1));
}

void
InMemoryZoneFinder::load(ZoneIterator& iterator) {
    load(string(), boost::bind(generateRRsetFromIterator, &iterator, _1));
}

const string
InMemoryZoneFinder::getFileName() const {
    return (file_name_);
}

/// Implementation details for \c InMemoryClient hidden from the public
/// interface.
///
/// For now, \c InMemoryClient only contains a \c ZoneTable object, which
/// consists of (pointers to) \c InMemoryZoneFinder objects, we may add more
/// member variables later for new features.
class InMemoryClient::InMemoryClientImpl {
public:
    InMemoryClientImpl() : zone_count(0) {}
    unsigned int zone_count;
    ZoneTable zone_table;
};

InMemoryClient::InMemoryClient() : impl_(new InMemoryClientImpl)
{}

InMemoryClient::~InMemoryClient() {
    delete impl_;
}

unsigned int
InMemoryClient::getZoneCount() const {
    return (impl_->zone_count);
}

result::Result
InMemoryClient::addZone(ZoneFinderPtr zone_finder) {
    if (!zone_finder) {
        isc_throw(InvalidParameter,
                  "Null pointer is passed to InMemoryClient::addZone()");
    }

    LOG_DEBUG(logger, DBG_TRACE_BASIC, DATASRC_MEM_ADD_ZONE).
        arg(zone_finder->getOrigin()).arg(zone_finder->getClass().toText());

    const result::Result result = impl_->zone_table.addZone(zone_finder);
    if (result == result::SUCCESS) {
        ++impl_->zone_count;
    }
    return (result);
}

InMemoryClient::FindResult
InMemoryClient::findZone(const isc::dns::Name& name) const {
    LOG_DEBUG(logger, DBG_TRACE_DATA, DATASRC_MEM_FIND_ZONE).arg(name);
    ZoneTable::FindResult result(impl_->zone_table.findZone(name));
    return (FindResult(result.code, result.zone));
}

ZoneIteratorPtr
InMemoryClient::getIterator(const Name&, bool) const {
    isc_throw(isc::NotImplemented,
              "iterator attempt on in memory data source");
}

ZoneUpdaterPtr
InMemoryClient::getUpdater(const isc::dns::Name&, bool, bool) const {
    isc_throw(isc::NotImplemented, "Update attempt on in memory data source");
}

pair<ZoneJournalReader::Result, ZoneJournalReaderPtr>
InMemoryClient::getJournalReader(const isc::dns::Name&, uint32_t,
                                 uint32_t) const
{
    isc_throw(isc::NotImplemented, "Journaling isn't supported for "
              "in memory data source");
}

} // end of experimental
} // end of namespace datasrc
} // end of namespace isc
