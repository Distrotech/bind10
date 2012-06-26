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

#ifndef _RDATE_ENCODER_H
#define _RDATE_ENCODER_H 1

#include <dns/name.h>
#include <dns/messagerenderer.h>
#include <dns/rdata.h>
#include <dns/rrtype.h>

#include <datasrc/rbtree2.h> // ugly, but okay for experiments

#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/interprocess/offset_ptr.hpp>

#include <stdint.h>

namespace isc {
namespace datasrc {
namespace internal {

struct RdataSet;

typedef experimental::RBNode<datasrc::internal::RdataSet> DomainNode;
typedef boost::interprocess::offset_ptr<
    experimental::RBNode<datasrc::internal::RdataSet> > DomainNodePtr;
typedef boost::interprocess::offset_ptr<
    const experimental::RBNode<datasrc::internal::RdataSet> >
ConstDomainNodePtr;

struct RdataFieldSpec {
    enum Type { NAME = 0, FIXED_LEN_DATA, VAR_LEN_DATA };
    static const unsigned int COMPRESSIBLE_NAME = 0x1;
    static const unsigned int ADDITIONAL_NAME = 0x2;

    const Type type;                  // NAME, FIXED_LEN_DATA, VAR_LEN_DATA
    const unsigned int attributes;    // NAME only, COMPRESSIBLE, ADDITIONAL
    const uint16_t len;               // FIXED_LEN_DATA only
};

struct RdataEncodeSpec {
    const uint16_t n_fields;
    const uint16_t n_names;
    const uint16_t n_varlens;
    const RdataFieldSpec* field_spec;
};

class RdataEncoder : boost::noncopyable {
public:
    RdataEncoder();
    ~RdataEncoder();

    void addRdata(const dns::rdata::Rdata& rdata);

    void construct(dns::RRType type);

    // Return the size of storage for the entire data in the encoder.
    // only valid after construct.
    size_t getStorageLength() const;

    // encode domain names.  return the total # of bytes used.
    // only valid after construct.
    // Ugly intrusion of modularity, but for experiments...
    typedef boost::function<
        DomainNodePtr(const dns::Name*)> NamePtrCreator;
    size_t encodeNames(DomainNodePtr* ptr_buf,
                       size_t n_bufs, NamePtrCreator creator_fn) const;

    // encode lengths of variable length fields.  return the total # of fields.
    // only valid after construct.
    size_t encodeLengths(uint16_t* field_buf, size_t n_bufs)
        const;

    // encode generic data.  return the total # of bytes used.
    // only valid after construct.
    size_t encodeData(uint8_t* data_buf, size_t bufsize) const;

    // Reset internal state for reuse
    void clear();

private:
    size_t n_data_;             // # of RDATAs stored in the encoder
    size_t n_varlen_fields_;    // # of variable-len fields stored
    size_t name_data_len_;      // size for storing name data
    size_t other_data_len_;     // size for storing other data

    // list of length of var-len fields
    std::vector<uint16_t> data_lengths_;
    // offset in the internal buffer to each data field
    std::vector<std::pair<const uint8_t*, size_t> > data_offsets_;

    // Current encode spec determined by the RR type given to construct.
    const struct RdataEncodeSpec* encode_spec_;

    class RdataFieldComposer;
    RdataFieldComposer* composer_;

    // This is essentially temporary workspace, and should be usable in
    // cost methods
    mutable uint8_t noffset_placeholder_[dns::Name::MAX_LABELS];
};

void
renderName(dns::AbstractMessageRenderer* renderer,
           const DomainNode* ptr_buf,
           unsigned int attributes);

void
renderData(dns::AbstractMessageRenderer* renderer,
           const uint8_t* data, size_t len);

// Return the size of encoded lengths and data, temporary function.
size_t
getEncodedDataSize(dns::RRType type, uint16_t n_rdata, const uint8_t* buf);

// An iterator on the encoded list of RDATAs, maybe allowing random access too
class RdataIterator {
public:
    typedef boost::function<void(const DomainNode*, unsigned int)> NameAction;
    typedef boost::function<void(const uint8_t*, size_t)> DataAction;

    RdataIterator(dns::RRType type, uint16_t n_rdata,
                  const ConstDomainNodePtr* names,
                  const uint16_t* lengths, const uint8_t* encoded_data,
                  NameAction name_action, DataAction data_action);

    void action();
    bool isLast() const {
        return (rdata_count_ == n_rdata_);
    }

private:
    const struct RdataEncodeSpec* const encode_spec_;
    const size_t n_rdata_;
    size_t rdata_count_;
    const ConstDomainNodePtr* const names_begin_;
    const ConstDomainNodePtr* names_;
    const uint16_t* const lengths_begin_;
    const uint16_t* lengths_;
    const uint8_t* const data_begin_;
    const uint8_t* data_;
    NameAction name_action_;
    DataAction data_action_;
};

}
}
}

#endif  // _RDATE_ENCODER_H

// Local Variables:
// mode: c++
// End:
