// Copyright (C) 2009  Internet Systems Consortium, Inc. ("ISC")
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

#include <cctype>
#include <cassert>
#include <set>
#include <vector>

#include <util/buffer.h>
#include <dns/name.h>
#include <dns/messagerenderer.h>

using namespace std;
using namespace isc::util;

namespace isc {
namespace dns {

///
/// \brief The \c MessageRendererImpl class is the actual implementation of
/// \c MessageRenderer.
///
/// The implementation is hidden from applications.  We can refer to specific
/// members of this class only within the implementation source file.
///
struct MessageRenderer::MessageRendererImpl {
    static const size_t BUCKETS = 64;
    static const size_t RESERVED_ITEMS = 16;
    static const uint16_t NO_OFFSET = 65535; // used as a marker of 'not found'

    struct OffsetItem {
        OffsetItem(const LabelSequence labels_param, uint16_t offset_param) :
            labels(labels_param), offset(offset_param)
        {}
        LabelSequence labels;
        uint16_t offset;
    };

    /// \brief Constructor from an output buffer.
    ///
    MessageRendererImpl() :
        msglength_limit_(512), truncated_(false),
        compress_mode_(MessageRenderer::CASE_INSENSITIVE)
    {
        for (size_t i = 0; i < BUCKETS; ++i) {
            table_[i].reserve(RESERVED_ITEMS);
        }
        names_.reserve(BUCKETS);
    }

    struct SequenceComp {
        SequenceComp(const LabelSequence& target, bool case_sensitive) :
            target_(target), case_sensitive_(case_sensitive)
        {}
        bool operator()(const OffsetItem& item) const {
            return (item.labels.equals(target_, case_sensitive_));
        }
    private:
        const LabelSequence& target_;
        bool case_sensitive_;
    };

    uint16_t findOffset(const LabelSequence& sequence) const {
        const bool case_sensitive = (compress_mode_ ==
                                     MessageRenderer::CASE_SENSITIVE);
        const size_t bucket = (sequence.getHash(case_sensitive) % BUCKETS);
        vector<OffsetItem>::const_reverse_iterator found =
            find_if(table_[bucket].rbegin(), table_[bucket].rend(),
                    SequenceComp(sequence, case_sensitive));
        if (found != table_[bucket].rend()) {
            return (found->offset);
        }
        return (NO_OFFSET);
    }

    void addOffset(const LabelSequence& sequence, uint16_t offset) {
        const bool case_sensitive = (compress_mode_ ==
                                     MessageRenderer::CASE_SENSITIVE);
        const size_t bucket = (sequence.getHash(case_sensitive) % BUCKETS);
        table_[bucket].push_back(OffsetItem(sequence, offset));
    }

    void writeName(const LabelSequence& sequence0, const bool compress,
                   const Name* name);

    /// The maximum length of rendered data that can fit without
    /// truncation.
    uint16_t msglength_limit_;
    /// A boolean flag that indicates truncation has occurred while rendering
    /// the data.
    bool truncated_;
    /// The name compression mode.
    CompressMode compress_mode_;

    vector<OffsetItem> table_[BUCKETS];
    vector<Name> names_;
};

MessageRenderer::MessageRenderer(OutputBuffer& buffer) :
    AbstractMessageRenderer(buffer),
    impl_(new MessageRendererImpl)
{}

MessageRenderer::~MessageRenderer() {
    delete impl_;
}

void
MessageRenderer::clear() {
    AbstractMessageRenderer::clear();
    impl_->msglength_limit_ = 512;
    impl_->truncated_ = false;
    impl_->compress_mode_ = CASE_INSENSITIVE;

    for (size_t i = 0; i < MessageRendererImpl::BUCKETS; ++i) {
        if (impl_->table_[i].size() > MessageRendererImpl::RESERVED_ITEMS) {
            impl_->table_[i].reserve(MessageRendererImpl::RESERVED_ITEMS);
            vector<MessageRendererImpl::OffsetItem>(impl_->table_[i].begin(),
                                                    impl_->table_[i].end()).
                swap(impl_->table_[i]);
        }
        impl_->table_[i].clear();
    }
    if (impl_->names_.size() > MessageRendererImpl::BUCKETS) {
        impl_->names_.reserve(MessageRendererImpl::BUCKETS);
        vector<Name>(impl_->names_.begin(), impl_->names_.end()).
            swap(impl_->names_);
    }
    impl_->names_.clear();
}

size_t
MessageRenderer::getLengthLimit() const {
    return (impl_->msglength_limit_);
}

void
MessageRenderer::setLengthLimit(const size_t len) {
    impl_->msglength_limit_ = len;
}

bool
MessageRenderer::isTruncated() const {
    return (impl_->truncated_);
}

void
MessageRenderer::setTruncated() {
    impl_->truncated_ = true;
}

MessageRenderer::CompressMode
MessageRenderer::getCompressMode() const {
    return (impl_->compress_mode_);
}

void
MessageRenderer::setCompressMode(const CompressMode mode) {
    impl_->compress_mode_ = mode;
}

void
MessageRenderer::writeName(const LabelSequence& sequence0,
                           const bool compress, const Name* name)
{
    LabelSequence sequence = sequence0; // we need to modify it internally
    const unsigned int labels = sequence.getOffsetLength();

    unsigned int i;
    uint16_t ptr_offset = MessageRendererImpl::NO_OFFSET;
    for (i = 0; i < labels; ++i) {
        if (sequence.getDataLength() == 1) { // trailing dot.
            ++i;
            break;
        }
        ptr_offset = impl_->findOffset(sequence);
        if (ptr_offset != MessageRendererImpl::NO_OFFSET) {
            break;
        }
        sequence.split(1);
    }

    // Record the current offset before extending the buffer.
    size_t offset = getLength();
    // Write uncompress part...
    if (compress) {
        if (i > 0) {
            sequence = sequence0;
            if (i < labels) {
                sequence.split(-(labels - i));
            }
            writeData(&sequence.getData()[sequence.getOffsets()[0]],
                      sequence.getDataLength());
        }
    } else {
        writeData(sequence.getData(), sequence0.getDataLength());
    }
    if (compress && ptr_offset != MessageRendererImpl::NO_OFFSET) {
        // ...and compression pointer if available.
        ptr_offset |= Name::COMPRESS_POINTER_MARK16;
        writeUint16(ptr_offset);
    }

    // Finally, add the newly rendered name and its ancestors that
    // have not been in the set.
    if (i > 0) {
        if (name != NULL) {
            impl_->names_.push_back(*name);
            impl_->names_.back().setLabelSequence(sequence);
        } else {
            sequence = sequence0;
        }

        unsigned int j = 0;
        do {
            const size_t llen =
                sequence.getData()[sequence.getOffsets()[0]] + 1;
            if (llen == 1) {        // root name.  no need to store it.
                break;
            }
            if (offset > Name::MAX_COMPRESS_POINTER) {
                break;
            }
            impl_->addOffset(sequence, offset);
            offset += llen;
            sequence.split(1);
        } while (++j < i);
    }
}

void
MessageRenderer::writeName(const LabelSequence& sequence, const bool compress)
{
    writeName(sequence, compress, NULL);
}

void
MessageRenderer::writeName(const Name& name, const bool compress) {
    LabelSequence sequence;
    name.setLabelSequence(sequence);
    writeName(sequence, compress, &name);
}

void
AbstractMessageRenderer::clear() {
    buffer_.clear();
}

}
}
