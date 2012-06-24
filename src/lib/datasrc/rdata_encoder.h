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
#include <dns/rdata.h>
#include <dns/rrtype.h>

#include <boost/noncopyable.hpp>

#include <stdint.h>

namespace isc {
namespace datasrc {
namespace internal {

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

    void encodeLengths(uint16_t* field_buf) const;

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

    class RdataFieldComposer;
    RdataFieldComposer* composer_;
};

}
}
}

#endif  // _RDATE_ENCODER_H

// Local Variables:
// mode: c++
// End:
