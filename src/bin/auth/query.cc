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

#include <dns/message.h>
#include <dns/rcode.h>
#include <dns/rdataclass.h>

#include <datasrc/client.h>

#include <auth/query.h>

#include <algorithm>            // for std::max
#include <vector>
#include <boost/foreach.hpp>
#include <boost/bind.hpp>
#include <boost/function.hpp>

using namespace std;
using namespace isc::dns;
using namespace isc::datasrc;
using namespace isc::dns::rdata;

namespace isc {
namespace datasrc {
class FindContext {
public:
    FindContext(ZoneFinder& finder_param,
                const Name& qname_param,
                ZoneFinder::Result code_param,
                ConstRRsetPtr rrset_param, bool dnssec_param) :
        code(code_param), rrset(rrset_param),
        finder(finder_param), qname(qname_param), dnssec(dnssec_param)
    {}
    const ZoneFinder::Result code;
    const ConstRRsetPtr rrset;

    // Find 'additional' RRsets based on the find() result.
    void getAdditional(const vector<RRType>& requested_types,
                       vector<ConstRRsetPtr>& result);

    // Called for NXDOMAIN + NSEC-signed case.  Return an NSEC that proves
    // there's no matching wildcard for the original qname.
    //
    // Default version: construct the best possible wildcard name from qname
    // and the NSEC that proves non-existent of the name, and internally calls
    // find(NSEC) for that name.
    //
    // Optimized in-memory version: it should know the closest encloser in
    // the previous search and can identify the wildcard name and its previous
    // name, then subsequently its NSEC.
    ConstRRsetPtr getNoWildcardNSEC();

    // Called for NXDOMAIN/NXRRSET cases and return NSEC/NSEC3 RRsets
    // for the corresponding DNSSEC proof.  It (will) consist of small helper
    // private methods for specific cases (NSEC or NSEC3, NXDOMAIN or NXRRSET).
    //
    // - NSEC3 + NXDOMAIN
    //   Default version: use findNSEC3() to get closest encloser proof, use
    //     the result to identify the possible wildcard, and use findNSEC3()
    //     to get proof for its non-existence.
    //   Optimized in-memory version: it should know the closest encloser in
    //     the original search.  get corresponding NSEC3 directly, construct
    //     next closer and wildcard from the closest encloser, calculate hash
    //     and get the covering NSEC3.
    void getNegativeProof(vector<ConstRRsetPtr>& proofs);

private:
    void getAdditionalAddrs(const Name& name,
                            const vector<RRType>& requested_types,
                            vector<ConstRRsetPtr>& result,
                            ZoneFinder::FindOptions options);

    uint8_t getClosestEncloserProof(const Name& name,
                                    vector<ConstRRsetPtr>& proofs,
                                    bool exact_ok, bool add_closest = true);
    ConstRRsetPtr getNSEC3ForName(const Name& name, bool match);

private:
    ZoneFinder& finder;
    const Name& qname;
    const bool dnssec;
};

void
FindContext::getAdditional(const vector<RRType>& requested_types,
                           vector<ConstRRsetPtr>& result)
{
    RdataIteratorPtr rdata_iterator(rrset->getRdataIterator());
    ZoneFinder::FindOptions options = ZoneFinder::FIND_DEFAULT;
    if (dnssec) {
        options = options | ZoneFinder::FIND_DNSSEC;
    }
    for (; !rdata_iterator->isLast(); rdata_iterator->next()) {
        const Rdata& rdata(rdata_iterator->getCurrent());

        if (rrset->getType() == RRType::NS()) {
            // Need to perform the search in the "GLUE OK" mode.
            const generic::NS& ns = dynamic_cast<const generic::NS&>(rdata);
            getAdditionalAddrs(ns.getNSName(), requested_types, result,
                               options | ZoneFinder::FIND_GLUE_OK);
        } else if (rrset->getType() == RRType::MX()) {
            const generic::MX& mx(dynamic_cast<const generic::MX&>(rdata));
            getAdditionalAddrs(mx.getMXName(), requested_types, result,
                               options);
        }
    }
}

void
FindContext::getAdditionalAddrs(const Name& name,
                                const vector<RRType>& requested_types,
                                vector<ConstRRsetPtr>& result_rrsets,
                                ZoneFinder::FindOptions options)
{
    // Out of zone name
    const NameComparisonResult cmp = finder.getOrigin().compare(name);
    if ((cmp.getRelation() != NameComparisonResult::SUPERDOMAIN) &&
        (cmp.getRelation() != NameComparisonResult::EQUAL)) {
        return;
    }

    vector<RRType>::const_iterator it = requested_types.begin();
    vector<RRType>::const_iterator it_end = requested_types.end();
    for (; it != it_end; ++it) {
        ZoneFinder::FindResult result = finder.find(name, *it, options);
        if (result.code == ZoneFinder::SUCCESS) {
            result_rrsets.push_back(result.rrset);
        }
    }
}

ConstRRsetPtr
FindContext::getNoWildcardNSEC() {
    // Next, identify the best possible wildcard name that would match
    // the query name.  It's the longer common suffix with the qname
    // between the owner or the next domain of the NSEC that proves NXDOMAIN,
    // prefixed by the wildcard label, "*".  For example, for query name
    // a.b.example.com, if the NXDOMAIN NSEC is
    // b.example.com. NSEC c.example.com., the longer suffix is b.example.com.,
    // and the best possible wildcard is *.b.example.com.  If the NXDOMAIN
    // NSEC is a.example.com. NSEC c.b.example.com., the longer suffix
    // is the next domain of the NSEC, and we get the same wildcard name.
    const int qlabels = qname.getLabelCount();
    const int olabels = qname.compare(rrset->getName()).getCommonLabels();
    const int nlabels = qname.compare(
        dynamic_cast<const generic::NSEC&>(rrset->getRdataIterator()->
                                           getCurrent()).
        getNextName()).getCommonLabels();
    const int common_labels = std::max(olabels, nlabels);
    const Name wildname(Name("*").concatenate(qname.split(qlabels -
                                                          common_labels)));

    // Confirm the wildcard doesn't exist (this should result in NXDOMAIN;
    // otherwise we shouldn't have got NXDOMAIN for the original query in
    // the first place).
    const ZoneFinder::FindResult fresult =
        finder.find(wildname, RRType::NSEC(), ZoneFinder::FIND_DNSSEC);
    if (fresult.code != ZoneFinder::NXDOMAIN || !fresult.rrset ||
        fresult.rrset->getRdataCount() == 0) {
        return (ConstRRsetPtr());
    }

    return (fresult.rrset);
}

void
FindContext::getNegativeProof(vector<ConstRRsetPtr>& proofs) {
    switch (code) {
    case ZoneFinder::NXDOMAIN: { // should eventually go to a method
        // Firstly get the NSEC3 proves for Closest Encloser Proof
        // See Section 7.2.1 of RFC 5155.
        const uint8_t closest_labels =
            getClosestEncloserProof(qname, proofs, false);
        // Next, construct the wildcard name at the closest encloser, i.e.,
        // '*' followed by the closest encloser, and get NSEC3 for it.
        const Name wildname(Name("*").concatenate(
                                qname.split(qname.getLabelCount() -
                                             closest_labels)));
        proofs.push_back(getNSEC3ForName(wildname, false));
        break;
    }
    default:
        assert(false);
    }
}

uint8_t
FindContext::getClosestEncloserProof(const Name& name,
                                     vector<ConstRRsetPtr>& proofs,
                                     bool exact_ok, bool add_closest)
{
    const ZoneFinder::FindNSEC3Result result = finder.findNSEC3(name, true);

    // Validity check (see the method description).  Note that a completely
    // broken findNSEC3 implementation could even return NULL RRset in
    // closest_proof.  We don't explicitly check such case; addRRset() will
    // throw an exception, and it will be converted to SERVFAIL at the caller.
    if (!exact_ok && !result.next_proof) {
        isc_throw(isc::auth::Query::BadNSEC3,
                  "Matching NSEC3 found for a non existent name: " << name);
    }

    if (add_closest) {
        proofs.push_back(result.closest_proof);
    }
    if (result.next_proof) {
        proofs.push_back(result.next_proof);
    }
    return (result.closest_labels);
}

ConstRRsetPtr
FindContext::getNSEC3ForName(const Name& name, bool match) {
    const ZoneFinder::FindNSEC3Result result = finder.findNSEC3(name, false);

    // See the comment for addClosestEncloserProof().  We don't check a
    // totally bogus case where closest_proof is NULL here.
    if (match != result.matched) {
        isc_throw(isc::auth::Query::BadNSEC3, "Unexpected "
                  << (result.matched ? "matching" : "covering")
                  << " NSEC3 found for " << name);
    }
    return (result.closest_proof);
}

} // namespace datasrc
} // namespace isc

namespace isc {
namespace auth {

void
Query::addAdditional(FindContext& ctx) {
    vector<RRType> needed_types;
    needed_types.push_back(RRType::A());
    needed_types.push_back(RRType::AAAA());

    vector<ConstRRsetPtr> additionals;
    ctx.getAdditional(needed_types, additionals);

    vector<ConstRRsetPtr>::const_iterator it = additionals.begin();
    vector<ConstRRsetPtr>::const_iterator it_end = additionals.end();
    for (; it != it_end; ++it) {
        if ((qtype_ == (*it)->getType() || qtype_ == RRType::ANY()) &&
            qname_ == (*it)->getName()) {
            continue;
        }
        response_.addRRset(Message::SECTION_ADDITIONAL,
                           boost::const_pointer_cast<AbstractRRset>(*it),
                           dnssec_);
    }
}

void
Query::addSOA(ZoneFinder& finder) {
    ZoneFinder::FindResult soa_result = finder.find(finder.getOrigin(),
                                                    RRType::SOA(),
                                                    dnssec_opt_);
    if (soa_result.code != ZoneFinder::SUCCESS) {
        isc_throw(NoSOA, "There's no SOA record in zone " <<
            finder.getOrigin().toText());
    } else {
        /*
         * FIXME:
         * The const-cast is wrong, but the Message interface seems
         * to insist.
         */
        response_.addRRset(Message::SECTION_AUTHORITY,
            boost::const_pointer_cast<AbstractRRset>(soa_result.rrset), dnssec_);
    }
}

// Note: unless the data source client implementation or the zone content
// is broken, 'ctx.rrset' should be a valid NSEC RR.  Likewise, the call to
// find() in this method should result in NXDOMAIN and an NSEC RR that proves
// the non existent of matching wildcard.  If these assumptions aren't met
// due to a buggy data source implementation or a broken zone, we'll let
// underlying libdns++ modules throw an exception, which would result in
// either an SERVFAIL response or just ignoring the query.  We at least prevent
// a complete crash due to such broken behavior.
void
Query::addNXDOMAINProofByNSEC(FindContext& ctx) {
    if (ctx.rrset->getRdataCount() == 0) {
        isc_throw(BadNSEC, "NSEC for NXDOMAIN is empty");
    }

    // Add the NSEC proving NXDOMAIN to the authority section.
    response_.addRRset(Message::SECTION_AUTHORITY,
                       boost::const_pointer_cast<AbstractRRset>(ctx.rrset),
                       dnssec_);

    ConstRRsetPtr wnsec = ctx.getNoWildcardNSEC();
    if (!wnsec) {
        isc_throw(BadNSEC, "Unexpected result for wildcard NXDOMAIN proof");
    }

    // Add the (no-) wildcard proof only when it's different from the NSEC
    // that proves NXDOMAIN; sometimes they can be the same.
    // Note: name comparison is relatively expensive.  When we are at the
    // stage of performance optimization, we should consider optimizing this
    // for some optimized data source implementations.
    if (ctx.rrset->getName() != wnsec->getName()) {
        response_.addRRset(Message::SECTION_AUTHORITY,
                           boost::const_pointer_cast<AbstractRRset>(wnsec),
                           dnssec_);
    }
}

uint8_t
Query::addClosestEncloserProof(ZoneFinder& finder, const Name& name,
                               bool exact_ok, bool add_closest)
{
    const ZoneFinder::FindNSEC3Result result = finder.findNSEC3(name, true);

    // Validity check (see the method description).  Note that a completely
    // broken findNSEC3 implementation could even return NULL RRset in
    // closest_proof.  We don't explicitly check such case; addRRset() will
    // throw an exception, and it will be converted to SERVFAIL at the caller.
    if (!exact_ok && !result.next_proof) {
        isc_throw(BadNSEC3, "Matching NSEC3 found for a non existent name: "
                  << qname_);
    }

    if (add_closest) {
        response_.addRRset(Message::SECTION_AUTHORITY,
                           boost::const_pointer_cast<AbstractRRset>(
                               result.closest_proof),
                           dnssec_);
    }
    if (result.next_proof) {
        response_.addRRset(Message::SECTION_AUTHORITY,
                           boost::const_pointer_cast<AbstractRRset>(
                               result.next_proof),
                           dnssec_);
    }
    return (result.closest_labels);
}

void
Query::addNSEC3ForName(ZoneFinder& finder, const Name& name, bool match) {
    const ZoneFinder::FindNSEC3Result result = finder.findNSEC3(name, false);

    // See the comment for addClosestEncloserProof().  We don't check a
    // totally bogus case where closest_proof is NULL here.
    if (match != result.matched) {
        isc_throw(BadNSEC3, "Unexpected "
                  << (result.matched ? "matching" : "covering")
                  << " NSEC3 found for " << name);
    }
    response_.addRRset(Message::SECTION_AUTHORITY,
                       boost::const_pointer_cast<AbstractRRset>(
                           result.closest_proof),
                       dnssec_);
}

void
Query::addNXDOMAINProofByNSEC3(ZoneFinder& finder) {
    // Firstly get the NSEC3 proves for Closest Encloser Proof
    // See Section 7.2.1 of RFC 5155.
    const uint8_t closest_labels =
        addClosestEncloserProof(finder, qname_, false);

    // Next, construct the wildcard name at the closest encloser, i.e.,
    // '*' followed by the closest encloser, and add NSEC3 for it.
    const Name wildname(Name("*").concatenate(
               qname_.split(qname_.getLabelCount() - closest_labels)));
    addNSEC3ForName(finder, wildname, false);
}

void
Query::addWildcardProof(ZoneFinder& finder,
                        const ZoneFinder::FindResult& db_result)
{
    if (db_result.isNSECSigned()) {
        // Case for RFC4035 Section 3.1.3.3.
        //
        // The query name shouldn't exist in the zone if there were no wildcard
        // substitution.  Confirm that by specifying NO_WILDCARD.  It should
        // result in NXDOMAIN and an NSEC RR that proves it should be returned.
        const ZoneFinder::FindResult fresult =
            finder.find(qname_, RRType::NSEC(),
                        dnssec_opt_ | ZoneFinder::NO_WILDCARD);
        if (fresult.code != ZoneFinder::NXDOMAIN || !fresult.rrset ||
            fresult.rrset->getRdataCount() == 0) {
            isc_throw(BadNSEC,
                      "Unexpected NSEC result for wildcard proof");
        }
        response_.addRRset(Message::SECTION_AUTHORITY,
                           boost::const_pointer_cast<AbstractRRset>(
                               fresult.rrset),
                           dnssec_);
    } else if (db_result.isNSEC3Signed()) {
        // Case for RFC 5155 Section 7.2.6.
        //
        // Note that the closest encloser must be the immediate ancestor
        // of the matching wildcard, so NSEC3 for its next closer (and only
        // that NSEC3) is what we are expected to provided per the RFC (if
        // this assumption isn't met the zone is broken anyway).
        addClosestEncloserProof(finder, qname_, false, false);
    }
}

void
Query::addWildcardNXRRSETProof(ZoneFinder& finder, ConstRRsetPtr nsec) {
    // There should be one NSEC RR which was found in the zone to prove
    // that there is not matched <QNAME,QTYPE> via wildcard expansion.
    if (nsec->getRdataCount() == 0) {
        isc_throw(BadNSEC, "NSEC for WILDCARD_NXRRSET is empty");
    }
    
    const ZoneFinder::FindResult fresult =
        finder.find(qname_, RRType::NSEC(),
                    dnssec_opt_ | ZoneFinder::NO_WILDCARD);
    if (fresult.code != ZoneFinder::NXDOMAIN || !fresult.rrset ||
        fresult.rrset->getRdataCount() == 0) {
        isc_throw(BadNSEC, "Unexpected result for no match QNAME proof");
    }
   
    if (nsec->getName() != fresult.rrset->getName()) {
        // one NSEC RR proves wildcard_nxrrset that no matched QNAME.
        response_.addRRset(Message::SECTION_AUTHORITY,
                           boost::const_pointer_cast<AbstractRRset>(fresult.rrset),
                           dnssec_);
    }
}

void
Query::addDS(ZoneFinder& finder, const Name& dname) {
    ZoneFinder::FindResult ds_result =
        finder.find(dname, RRType::DS(), dnssec_opt_);
    if (ds_result.code == ZoneFinder::SUCCESS) {
        response_.addRRset(Message::SECTION_AUTHORITY,
                           boost::const_pointer_cast<AbstractRRset>(
                               ds_result.rrset),
                           dnssec_);
    } else if (ds_result.code == ZoneFinder::NXRRSET &&
               ds_result.isNSECSigned()) {
        addNXRRsetProof(finder, ds_result);
    } else if (ds_result.code == ZoneFinder::NXRRSET &&
               ds_result.isNSEC3Signed()) {
        // Add no DS proof with NSEC3 as specified in RFC 5155 Section 7.2.7.
        addClosestEncloserProof(finder, dname, true);
    } else {
        // Any other case should be an error
        isc_throw(BadDS, "Unexpected result for DS lookup for delegation");
    }
}

void
Query::addNXRRsetProof(ZoneFinder& finder,
                       const ZoneFinder::FindResult& db_result)
{
    if (db_result.isNSECSigned() && db_result.rrset) {
        response_.addRRset(Message::SECTION_AUTHORITY,
                           boost::const_pointer_cast<AbstractRRset>(
                               db_result.rrset),
                           dnssec_);
        if (db_result.isWildcard()) {
            addWildcardNXRRSETProof(finder, db_result.rrset);
        }
    } else if (db_result.isNSEC3Signed() && !db_result.isWildcard()) {
        if (qtype_ == RRType::DS()) {
            // RFC 5155, Section 7.2.4.  Add either NSEC3 for the qname or
            // closest (provable) encloser proof in case of optout.
            addClosestEncloserProof(finder, qname_, true);
        } else {
            // RFC 5155, Section 7.2.3.  Just add NSEC3 for the qname.
            addNSEC3ForName(finder, qname_, true);
        }
    } else if (db_result.isNSEC3Signed() && db_result.isWildcard()) {
        // Case for RFC 5155 Section 7.2.5: add closest encloser proof for the
        // qname, construct the matched wildcard name and add NSEC3 for it.
        const uint8_t closest_labels =
            addClosestEncloserProof(finder, qname_, false);
        const Name wname = Name("*").concatenate(
            qname_.split(qname_.getLabelCount() - closest_labels));
        addNSEC3ForName(finder, wname, true);
    }
}

void
Query::addAuthAdditional(ZoneFinder& finder) {
    // Fill in authority and addtional sections.
    ZoneFinder::FindResult ns_result =
        finder.find(finder.getOrigin(), RRType::NS(), dnssec_opt_);

    // zone origin name should have NS records
    if (ns_result.code != ZoneFinder::SUCCESS) {
        isc_throw(NoApexNS, "There's no apex NS records in zone " <<
                finder.getOrigin().toText());
    } else {
        response_.addRRset(Message::SECTION_AUTHORITY,
            boost::const_pointer_cast<AbstractRRset>(ns_result.rrset),
                           dnssec_);
        // Handle additional for authority section
        FindContext ctx(finder, finder.getOrigin(), ns_result.code,
                        ns_result.rrset, dnssec_);
        addAdditional(ctx);
    }
}

namespace {
class RRsetInserter {
public:
    RRsetInserter(Message& msg, Message::Section section, bool dnssec) :
        msg_(msg), section_(section), dnssec_(dnssec)
    {}
    void operator()(const ConstRRsetPtr& rrset) {
        msg_.addRRset(section_,
                      boost::const_pointer_cast<AbstractRRset>(rrset),
                      dnssec_);
    }

private:
    Message& msg_;
    const Message::Section section_;
    const bool dnssec_;
};
}

namespace {
// A simple wrapper for DataSourceClient::findZone().  Normally we can simply
// check the closest zone to the qname, but for type DS query we need to
// look into the parent zone.  Nevertheless, if there is no "parent" (i.e.,
// the qname consists of a single label, which also means it's the root name),
// we should search the deepest zone we have (which should be the root zone;
// otherwise it's a query error).
DataSourceClient::FindResult
findZone(const DataSourceClient& client, const Name& qname, RRType qtype) {
    if (qtype != RRType::DS() || qname.getLabelCount() == 1) {
        return (client.findZone(qname));
    }
    return (client.findZone(qname.split(1)));
}
}

void
Query::process() {
    // Found a zone which is the nearest ancestor to QNAME
    const DataSourceClient::FindResult result = findZone(datasrc_client_,
                                                         qname_, qtype_);

    // If we have no matching authoritative zone for the query name, return
    // REFUSED.  In short, this is to be compatible with BIND 9, but the
    // background discussion is not that simple.  See the relevant topic
    // at the BIND 10 developers's ML:
    // https://lists.isc.org/mailman/htdig/bind10-dev/2010-December/001633.html
    if (result.code != result::SUCCESS &&
        result.code != result::PARTIALMATCH) {
        // If we tried to find a "parent zone" for a DS query and failed,
        // we may still have authority at the child side.  If we do, the query
        // has to be handled there.
        if (qtype_ == RRType::DS() && qname_.getLabelCount() > 1 &&
            processDSAtChild()) {
            return;
        }
        response_.setHeaderFlag(Message::HEADERFLAG_AA, false);
        response_.setRcode(Rcode::REFUSED());
        return;
    }
    ZoneFinder& zfinder = *result.zone_finder;

    // We have authority for a zone that contain the query name (possibly
    // indirectly via delegation).  Look into the zone.
    response_.setHeaderFlag(Message::HEADERFLAG_AA);
    response_.setRcode(Rcode::NOERROR());
    std::vector<ConstRRsetPtr> target;
    boost::function0<ZoneFinder::FindResult> find;
    const bool qtype_is_any = (qtype_ == RRType::ANY());
    if (qtype_is_any) {
        find = boost::bind(&ZoneFinder::findAll, &zfinder, qname_,
                           boost::ref(target), dnssec_opt_);
    } else {
        find = boost::bind(&ZoneFinder::find, &zfinder, qname_, qtype_,
                           dnssec_opt_);
    }
    ZoneFinder::FindResult db_result(find());
    FindContext ctx(zfinder, qname_, db_result.code, db_result.rrset, dnssec_);

    switch (db_result.code) {
        case ZoneFinder::DNAME: {
            // First, put the dname into the answer
            response_.addRRset(Message::SECTION_ANSWER,
                boost::const_pointer_cast<AbstractRRset>(db_result.rrset),
                dnssec_);
            /*
             * Empty DNAME should never get in, as it is impossible to
             * create one in master file.
             *
             * FIXME: Other way to prevent this should be done
             */
            assert(db_result.rrset->getRdataCount() > 0);
            // Get the data of DNAME
            const rdata::generic::DNAME& dname(
                dynamic_cast<const rdata::generic::DNAME&>(
                db_result.rrset->getRdataIterator()->getCurrent()));
            // The yet unmatched prefix dname
            const Name prefix(qname_.split(0, qname_.getLabelCount() -
                db_result.rrset->getName().getLabelCount()));
            // If we put it together, will it be too long?
            // (The prefix contains trailing ., which will be removed
            if (prefix.getLength() - Name::ROOT_NAME().getLength() +
                dname.getDname().getLength() > Name::MAX_WIRE) {
                /*
                 * In case the synthesized name is too long, section 4.1
                 * of RFC 2672 mandates we return YXDOMAIN.
                 */
                response_.setRcode(Rcode::YXDOMAIN());
                return;
            }
            // The new CNAME we are creating (it will be unsigned even
            // with DNSSEC, the DNAME is signed and it can be validated
            // by that)
            RRsetPtr cname(new RRset(qname_, db_result.rrset->getClass(),
                RRType::CNAME(), db_result.rrset->getTTL()));
            // Construct the new target by replacing the end
            cname->addRdata(rdata::generic::CNAME(qname_.split(0,
                qname_.getLabelCount() -
                db_result.rrset->getName().getLabelCount()).
                concatenate(dname.getDname())));
            response_.addRRset(Message::SECTION_ANSWER, cname, dnssec_);
            break;
        }
        case ZoneFinder::CNAME:
            /*
             * We don't do chaining yet. Therefore handling a CNAME is
             * mostly the same as handling SUCCESS, but we didn't get
             * what we expected. It means no exceptions in ANY or NS
             * on the origin (though CNAME in origin is probably
             * forbidden anyway).
             *
             * So, just put it there.
             */
            response_.addRRset(Message::SECTION_ANSWER,
                boost::const_pointer_cast<AbstractRRset>(db_result.rrset),
                dnssec_);

            // If the answer is a result of wildcard substitution,
            // add a proof that there's no closer name.
            if (dnssec_ && db_result.isWildcard()) {
                addWildcardProof(*result.zone_finder,db_result);
            }
            break;
        case ZoneFinder::SUCCESS:
            if (qtype_is_any) {
                // If quety type is ANY, insert all RRs under the domain
                // into answer section.
                BOOST_FOREACH(ConstRRsetPtr rrset, target) {
                    response_.addRRset(Message::SECTION_ANSWER,
                        boost::const_pointer_cast<AbstractRRset>(rrset),
                                       dnssec_);
                    // Handle additional for answer section
                    FindContext ctx_any(zfinder, qname_, db_result.code, rrset,
                                        dnssec_);
                    addAdditional(ctx_any);
                }
            } else {
                response_.addRRset(Message::SECTION_ANSWER,
                    boost::const_pointer_cast<AbstractRRset>(db_result.rrset),
                    dnssec_);
                // Handle additional for answer section
                addAdditional(ctx);
            }
            // If apex NS records haven't been provided in the answer
            // section, insert apex NS records into the authority section
            // and AAAA/A RRS of each of the NS RDATA into the additional
            // section.
            if (qname_ != result.zone_finder->getOrigin() ||
                db_result.code != ZoneFinder::SUCCESS ||
                (qtype_ != RRType::NS() && !qtype_is_any))
            {
                addAuthAdditional(*result.zone_finder);
            }

            // If the answer is a result of wildcard substitution,
            // add a proof that there's no closer name.
            if (dnssec_ && db_result.isWildcard()) {
                addWildcardProof(*result.zone_finder,db_result);
            }
            break;
        case ZoneFinder::DELEGATION:
            // If a DS query resulted in delegation, we also need to check
            // if we are an authority of the child, too.  If so, we need to
            // complete the process in the child as specified in Section
            // 2.2.1.2. of RFC3658.
            if (qtype_ == RRType::DS() && processDSAtChild()) {
                return;
            }

            response_.setHeaderFlag(Message::HEADERFLAG_AA, false);
            response_.addRRset(Message::SECTION_AUTHORITY,
                boost::const_pointer_cast<AbstractRRset>(db_result.rrset),
                dnssec_);
            // If DNSSEC is requested, see whether there is a DS
            // record for this delegation.
            if (dnssec_) {
                addDS(*result.zone_finder, db_result.rrset->getName());
            }
            addAdditional(ctx);
            break;
        case ZoneFinder::NXDOMAIN:
            response_.setRcode(Rcode::NXDOMAIN());
            addSOA(*result.zone_finder);
            if (dnssec_) {
                if (db_result.isNSECSigned() && db_result.rrset) {
                    addNXDOMAINProofByNSEC(ctx);
                } else if (db_result.isNSEC3Signed()) {
                    vector<ConstRRsetPtr> proofs;
                    ctx.getNegativeProof(proofs);
                    for_each(proofs.begin(), proofs.end(),
                             RRsetInserter(response_,
                                           Message::SECTION_AUTHORITY,
                                           dnssec_));
                }
            }
            break;
        case ZoneFinder::NXRRSET:
            addSOA(*result.zone_finder);
            if (dnssec_) {
                addNXRRsetProof(zfinder, db_result);
            }
            break;
        default:
            // This is basically a bug of the data source implementation,
            // but could also happen in the middle of development where
            // we try to add a new result code.
            isc_throw(isc::NotImplemented, "Unknown result code");
            break;
    }
}

bool
Query::processDSAtChild() {
    const DataSourceClient::FindResult zresult =
        datasrc_client_.findZone(qname_);

    if (zresult.code != result::SUCCESS) {
        return (false);
    }

    // We are receiving a DS query at the child side of the owner name,
    // where the DS isn't supposed to belong.  We should return a "no data"
    // response as described in Section 3.1.4.1 of RFC4035 and Section
    // 2.2.1.1 of RFC 3658.  find(DS) should result in NXRRSET, in which
    // case (and if DNSSEC is required) we also add the proof for that,
    // but even if find() returns an unexpected result, we don't bother.
    // The important point in this case is to return SOA so that the resolver
    // that happens to contact us can hunt for the appropriate parent zone
    // by seeing the SOA.
    response_.setHeaderFlag(Message::HEADERFLAG_AA);
    response_.setRcode(Rcode::NOERROR());
    addSOA(*zresult.zone_finder);
    const ZoneFinder::FindResult ds_result =
        zresult.zone_finder->find(qname_, RRType::DS(), dnssec_opt_);
    if (ds_result.code == ZoneFinder::NXRRSET) {
        if (dnssec_) {
            addNXRRsetProof(*zresult.zone_finder, ds_result);
        }
    }

    return (true);
}

}
}
