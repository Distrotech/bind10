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

#include <exceptions/exceptions.h>

#include <dns/messagerenderer.h>
#include <dns/labelsequence.h>
#include <dns/name.h>
#include <dns/rrset.h>
#include <dns/rrtype.h>
#include <dns/rrttl.h>
#include <dns/rdata.h>

#include <datasrc/rdataset.h>
#include <datasrc/memory_segment.h>
#include <datasrc/memory_datasrc2.h>
#include <datasrc/treenode_rrset.h>
#include <datasrc/compress_table.h>

#include <boost/bind.hpp>
#include <boost/interprocess/offset_ptr.hpp>

#include <string>

using namespace isc::dns;
using namespace isc::dns::rdata;

namespace isc {
namespace datasrc {
namespace experimental {
namespace internal {
class CompressOffsetTable;

TreeNodeRRset::TreeNodeRRset(
    RRClass rrclass,
    const RBNode<datasrc::internal::RdataSet>& node,
    const datasrc::internal::RdataSet& rdset) :
    rrclass_(rrclass), node_(&node), rdset_(&rdset),
    offset_table_(NULL)
{
}

bool
TreeNodeRRset::isSameKind(const AbstractRRset& other) const {
    // This code is an optimisation for comparing
    // TreeNodeRRsets. However, in doing this optimisation,
    // semantically the code is not "is same kind" but is instead
    // "is identical object" in the case where TreeNodeRRsets are compared.
    const TreeNodeRRset* rb = dynamic_cast<const TreeNodeRRset*>(&other);
    if (rb != NULL) {
        // Need wildcard/DNAME consideration
        return ((node_ == rb->node_) && (rdset_->type == rb->rdset_->type));
    } else {
        return (AbstractRRset::isSameKind(other));
    }
}

const Name&
TreeNodeRRset::getName() const {
    isc_throw(NotImplemented, "TreeNodeRRset::getName not implemented");
}

const RRType&
TreeNodeRRset::getType() const {
    isc_throw(NotImplemented, "TreeNodeRRset::getType not implemented");
}

const RRTTL&
TreeNodeRRset::getTTL() const {
    isc_throw(NotImplemented, "TreeNodeRRset::getTTL not implemented");
}

void
TreeNodeRRset::setName(const isc::dns::Name&) {
    isc_throw(NotImplemented, "TreeNodeRRset::setName not implemented");
}

void
TreeNodeRRset::setTTL(const isc::dns::RRTTL&) {
    isc_throw(NotImplemented, "TreeNodeRRset::setTTL not implemented");
}

std::string
TreeNodeRRset::toText() const {
    return ("<not implemented>\n");
}

void
renderName(AbstractMessageRenderer* renderer,
           CompressOffsetTable* offset_table, const DomainNode* node,
           unsigned int attributes)
{
    size_t nlen;
    const bool compressible =
        ((attributes &
          datasrc::internal::RdataFieldSpec::COMPRESSIBLE_NAME) != 0); 

    uint16_t offset = CompressOffsetTable::OFFSET_NOTFOUND;
    while (!node->isAbsolute() &&
           ((offset = offset_table->find(node)) ==
            CompressOffsetTable::OFFSET_NOTFOUND || !compressible))
    {
        if (offset == CompressOffsetTable::OFFSET_NOTFOUND) {
            offset_table->insert(node, renderer->getLength());
        }
        const uint8_t* np = node->getLabelSequence().getData(&nlen);
        renderer->writeData(np, nlen);
        node = node->getUpperNode();
    }
    if (node->isAbsolute()) {
        const uint8_t* np = node->getLabelSequence().getData(&nlen);
        renderer->writeData(np, nlen);
        if (nlen > 1) {
            offset_table->insert(node, renderer->getLength());
        }
    } else {
        assert(compressible);
        assert(offset < Name::COMPRESS_POINTER_MARK16);
        renderer->writeUint16(Name::COMPRESS_POINTER_MARK16 | offset);
    }
}

unsigned int
TreeNodeRRset::toWire(AbstractMessageRenderer& renderer) const {
    unsigned int n = 0;
    const LabelSequence owner_seq =
        node_->getAbsoluteLabelSequence(namebuf_, offsetbuf_);

    const size_t limit = renderer.getLengthLimit();
    const size_t n_rdata = rdset_->getRdataCount();
    assert(n_rdata > 0);
    datasrc::internal::RdataIterator rd_it(
        RRType(rdset_->type), n_rdata, rdset_->getNameBuf(), NULL, NULL,
        boost::bind(renderName, &renderer, offset_table_, _1, _2),
        boost::bind(datasrc::internal::renderData, &renderer, _1, _2));
    while (!rd_it.isLast()) {
        const size_t pos0 = renderer.getLength();
        assert(pos0 < 65536);

        renderName(&renderer, offset_table_, node_,
                   datasrc::internal::RdataFieldSpec::COMPRESSIBLE_NAME);
        renderer.writeUint16(rdset_->type);
        rrclass_.toWire(renderer);
        renderer.writeData(&rdset_->ttl, sizeof(rdset_->ttl));

        const size_t pos = renderer.getLength();
        renderer.skip(sizeof(uint16_t)); // leave the space for RDLENGTH
        rd_it.action();
        renderer.writeUint16At(renderer.getLength() - pos - sizeof(uint16_t),
                               pos);

        if (limit > 0 && renderer.getLength() > limit) {
            // truncation is needed
            renderer.trim(renderer.getLength() - pos0);
            return (n);
        }
        ++n;
    }

    return (n);
}

unsigned int
TreeNodeRRset::toWire(util::OutputBuffer&) const {
    isc_throw(NotImplemented, "TreeNodeRRset::toWire buffer not implemented");
}

void
TreeNodeRRset::addRdata(dns::rdata::ConstRdataPtr) {
    isc_throw(NotImplemented, "TreeNodeRRset::addRdata not implemented");
}

void
TreeNodeRRset::addRdata(const dns::rdata::Rdata&) {
    isc_throw(NotImplemented, "TreeNodeRRset::addRdata not implemented");
}

RdataIteratorPtr
TreeNodeRRset::getRdataIterator() const {
    isc_throw(NotImplemented,
              "TreeNodeRRset::getRdataIterator not implemented");
}

RRsetPtr
TreeNodeRRset::getRRsig() const {
    return (RRsetPtr());
}

void
TreeNodeRRset::addRRsig(const dns::rdata::ConstRdataPtr&) {
    isc_throw(NotImplemented, "TreeNodeRRset::addRRsig not implemented");
}

void
TreeNodeRRset::addRRsig(const dns::rdata::RdataPtr&) {
    isc_throw(NotImplemented, "TreeNodeRRset::addRRsig not implemented");
}

void
TreeNodeRRset::addRRsig(const AbstractRRset&) {
    isc_throw(NotImplemented, "TreeNodeRRset::addRRsig not implemented");
}

void
TreeNodeRRset::addRRsig(const dns::ConstRRsetPtr&) {
    isc_throw(NotImplemented, "TreeNodeRRset::addRRsig not implemented");
}

void
TreeNodeRRset::addRRsig(const dns::RRsetPtr&) {
    isc_throw(NotImplemented, "TreeNodeRRset::addRRsig not implemented");
}

void
TreeNodeRRset::removeRRsig() {
    isc_throw(NotImplemented, "TreeNodeRRset::removeRRsig not implemented");
}

} // internal
} // experimental
} // datasrc
} // isc
