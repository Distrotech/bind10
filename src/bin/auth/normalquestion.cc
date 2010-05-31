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

// $Id$

#include <vector>

#include <dns/buffer.h>
#include <dns/exceptions.h>
#include <dns/messagerenderer.h>
#include <dns/name.h>
#include <dns/rrtype.h>
#include <dns/rrclass.h>

#include "normalquestion.h"

using namespace std;
using namespace isc::dns;

namespace isc {
namespace dns {
class NormalQuestionImpl {
public:
    NormalQuestionImpl();
    ~NormalQuestionImpl();
    void clear();
    vector<unsigned char> qname_data_;
    vector<unsigned char> offsets_;
    bool isEmpty() const { return (qname_data_.empty()); }
    mutable Name* qname_;
    RRType qtype_;
    RRClass qclass_;
};

NormalQuestionImpl::NormalQuestionImpl() : qname_(NULL), qtype_(0), qclass_(0) {
    qname_data_.reserve(Name::MAX_WIRE);
    offsets_.reserve(Name::MAX_LABELS);
}

NormalQuestionImpl::~NormalQuestionImpl() {
    clear();
}

inline void
NormalQuestionImpl::clear() {
    delete qname_;
    qname_ = NULL;
    qtype_ = RRType(0);
    qclass_ = RRClass(0);
    qname_data_.clear();
    offsets_.clear();
}

NormalQuestion::NormalQuestion() : impl_(new NormalQuestionImpl)
{}

NormalQuestion::~NormalQuestion() {
    delete impl_;
}

void
NormalQuestion::fromWire(InputBuffer& buffer) {
    assert(impl_->isEmpty());

    uint8_t label_len;
    do {
        // In the normal query, there shouldn't be a compression pointer,
        // so the parse can be very simple.
        label_len = buffer.readUint8();
        if (label_len > Name::MAX_LABELLEN) {
            isc_throw(DNSMessageFORMERR,
                      "Invalid label length for a question: " << label_len);
        }
        impl_->offsets_.push_back(impl_->qname_data_.size());
        impl_->qname_data_.push_back(label_len);
        for (int i = 0; i < label_len; ++i) {
            impl_->qname_data_.push_back(buffer.readUint8());
        }
        if (impl_->qname_data_.size() > Name::MAX_LABELLEN) {
            isc_throw(DNSMessageFORMERR, "Invalid qname (too long): " <<
                      impl_->qname_data_.size());
        }
    } while (label_len > 0);

    impl_->qtype_ = RRType(buffer);
    impl_->qclass_ = RRClass(buffer);

    // XXX: should reset the qname_data_ and offsets_ on exception.
}

void
NormalQuestion::clear() {
    impl_->clear();
}

const RRType&
NormalQuestion::getType() const {
    assert(!impl_->isEmpty());
    return (impl_->qtype_);
}

const RRClass&
NormalQuestion::getClass() const {
    assert(!impl_->isEmpty());
    return (impl_->qclass_);
}

const Name&
NormalQuestion::getName() const {
    assert(!impl_->isEmpty());
    if (impl_->qname_ == NULL) {
        InputBuffer b(&impl_->qname_data_[0], impl_->qname_data_.size());
        impl_->qname_ = new Name(b);
    }
    return (*impl_->qname_);
}

void
NormalQuestion::setLabelSequenceToQName(LabelSequence& sequence) const {
    sequence.set(impl_->qname_data_.size(), &impl_->qname_data_[0],
                 impl_->offsets_.size(), &impl_->offsets_[0]); 
}

string
NormalQuestion::toText() const {
    assert(!impl_->isEmpty());

    LabelSequence sequence;
    setLabelSequenceToQName(sequence);
    return (sequence.toText() + " " + impl_->qclass_.toText() + " " +
            impl_->qtype_.toText() + "\n");
}

unsigned int
NormalQuestion::toWire(MessageRenderer& renderer) const {
    assert(!impl_->isEmpty());

    // Note: we don't try to compress the name.  This should be okay per se,
    // because in a "normal" question, compression shouldn't happen for the
    // qname.  However, this also means we exclude the labels in qname as
    // compression target.  We should revisit this point.
    renderer.writeData(&impl_->qname_data_[0], impl_->qname_data_.size());
    impl_->qtype_.toWire(renderer);
    impl_->qclass_.toWire(renderer);

    return (1);
}

unsigned int
NormalQuestion::toWire(OutputBuffer& buffer) const {
    assert(!impl_->isEmpty());

    buffer.writeData(&impl_->qname_data_[0], impl_->qname_data_.size());
    impl_->qtype_.toWire(buffer);
    impl_->qclass_.toWire(buffer);

    return (1);
}

}
}
