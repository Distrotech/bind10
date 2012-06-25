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

#include <dns/name.h>
#include <dns/labelsequence.h>
#include <dns/messagerenderer.h>
#include <dns/rrtype.h>
#include <dns/rdata.h>

#include <datasrc/rdata_encoder.h>

#include <boost/bind.hpp>
#include <boost/static_assert.hpp>

#include <cassert>
#include <exception>
#include <utility>
#include <vector>

using namespace std;
using namespace isc::dns;
using namespace isc::dns::rdata;

namespace isc {
namespace datasrc {
namespace internal {

namespace {
// generic (or unknown) variable length field
const RdataFieldSpec opaque_specs[] = {{RdataFieldSpec::VAR_LEN_DATA, 0, 0}};
const RdataFieldSpec single_ipv4_specs[] = {{RdataFieldSpec::FIXED_LEN_DATA, 0,
                                             4}};
const RdataFieldSpec single_ipv6_specs[] = {{RdataFieldSpec::FIXED_LEN_DATA, 0,
                                             16}};
// generic form of a single name field
const RdataFieldSpec single_name_specs[] = {{RdataFieldSpec::NAME,
                                       RdataFieldSpec::COMPRESSIBLE_NAME |
                                       RdataFieldSpec::ADDITIONAL_NAME, 0}};
// SOAR specific
const RdataFieldSpec soa_specs[] = {{RdataFieldSpec::NAME,
                                     RdataFieldSpec::COMPRESSIBLE_NAME |
                                     RdataFieldSpec::ADDITIONAL_NAME, 0},
                                    {RdataFieldSpec::NAME,
                                     RdataFieldSpec::COMPRESSIBLE_NAME |
                                     RdataFieldSpec::ADDITIONAL_NAME, 0},
                                    {RdataFieldSpec::FIXED_LEN_DATA, 0, 20}};
// MX specific
const RdataFieldSpec mx_specs[] = {{RdataFieldSpec::FIXED_LEN_DATA, 0, 2},
                                   {RdataFieldSpec::NAME,
                                    RdataFieldSpec::COMPRESSIBLE_NAME |
                                    RdataFieldSpec::ADDITIONAL_NAME, 0}};

struct RdataEncodeSpec encode_spec_list[] = {
    // #fields, #names, field spec list
    {1, 0, 0, opaque_specs},       // #0
    {1, 0, 0, single_ipv4_specs},  // #1: A
    {1, 1, 0, single_name_specs},  // #2: NS
    {1, 0, 1, opaque_specs},       // #3
    {1, 0, 1, opaque_specs},       // #4
    {1, 1, 0, single_name_specs},  // #5: CNAME
    {3, 2, 0, soa_specs},          // #6
    {1, 0, 1, opaque_specs},       // #7
    {1, 0, 1, opaque_specs},       // #8
    {1, 0, 1, opaque_specs},       // #9
    {1, 0, 1, opaque_specs},       // #10
    {1, 0, 1, opaque_specs},       // #11
    {1, 1, 0, single_name_specs},  // #12: PTR
    {1, 0, 1, opaque_specs},       // #13
    {1, 0, 1, opaque_specs},       // #14
    {2, 0, 0, mx_specs},           // #15
    {1, 0, 1, opaque_specs},       // #16: TXT
    {1, 0, 1, opaque_specs},       // #17
    {1, 0, 1, opaque_specs},       // #18
    {1, 0, 1, opaque_specs},       // #19
    {1, 0, 1, opaque_specs},       // #20
    {1, 0, 1, opaque_specs},       // #21
    {1, 0, 1, opaque_specs},       // #22
    {1, 0, 1, opaque_specs},       // #23
    {1, 0, 1, opaque_specs},       // #24
    {1, 0, 1, opaque_specs},       // #25
    {1, 0, 1, opaque_specs},       // #26
    {1, 0, 1, opaque_specs},       // #27
    {1, 0, 0, single_ipv6_specs}   // #28: AAAA
};

const size_t encode_spec_list_size =
    sizeof(encode_spec_list) / sizeof(encode_spec_list[0]);
BOOST_STATIC_ASSERT(encode_spec_list_size == 29);

} // end of unnamed namespace

// This class is used to divide the content of RDATA into \c RdataField
// fields via message rendering logic.
// The idea is to identify domain name fields in the writeName() method,
// and determine whether they are compressible using the "compress"
// parameter.
// Other types of data are simply copied into the internal buffer, and
// consecutive such fields are combined into a single \c RdataField field.
//
// Technically, this use of inheritance may be considered a violation of
// Liskov Substitution Principle in that it doesn't actually compress domain
// names, and some of the methods are not expected to be used.
// In fact, skip() or trim() may not be make much sense in this context.
// Nevertheless we keep this idea at the moment.  Since the usage is limited
// (it's only used within this file, and only used with \c Rdata variants),
// it's hopefully an acceptable practice.
class RdataEncoder::RdataFieldComposer : public AbstractMessageRenderer {
public:
    RdataFieldComposer() :
        truncated_(false), length_limit_(65535),
        mode_(CASE_INSENSITIVE), last_data_pos_(0)
    {}
    virtual ~RdataFieldComposer() {}
    virtual bool isTruncated() const { return (truncated_); }
    virtual size_t getLengthLimit() const { return (length_limit_); }
    virtual CompressMode getCompressMode() const { return (mode_); }
    virtual void setTruncated() { truncated_ = true; }
    virtual void setLengthLimit(size_t len) { length_limit_ = len; }
    virtual void setCompressMode(CompressMode mode) { mode_ = mode; }
    virtual void writeName(const LabelSequence&, bool) {}
    virtual void writeName(const Name& name, bool) {
        extendData();
        const size_t cur_pos = getLength();
        name.toWire(getBuffer());
        names_.push_back(pair<size_t, Name>(cur_pos, name));
        last_data_pos_ = getLength();
    }

    virtual void clear() {
        AbstractMessageRenderer::clear();
        data_positions_.clear();
        names_.clear();
        last_data_pos_ = 0;
    }

    vector<pair<size_t, size_t> > data_positions_;
    vector<pair<size_t, Name> > names_;

    // We use generic write* methods, with the exception of writeName.
    // So new data can arrive without us knowing it, this considers all new
    // data to be just data and extends the fields to take it into account.
    void extendData() {
        // No news, return to work
        const size_t cur_pos = getLength();
        if (cur_pos == last_data_pos_) {
            return;
        }

        // We added this much data from last time
        data_positions_.push_back(
            pair<size_t, size_t>(last_data_pos_, cur_pos - last_data_pos_));
        last_data_pos_ = cur_pos;
    }

private:
    bool truncated_;
    size_t length_limit_;
    CompressMode mode_;
    size_t last_data_pos_;
};

const RdataEncodeSpec&
getRdataEncodeSpec(RRType type) {
    static const RdataEncodeSpec generic_spec = {1, 0, 1, opaque_specs};
    if (type.getCode() < encode_spec_list_size) {
        return (encode_spec_list[type.getCode()]);
    }
    return (generic_spec);
}

RdataEncoder::RdataEncoder() :
    n_data_(0), n_varlen_fields_(0), name_data_len_(0), other_data_len_(0),
    encode_spec_(NULL), composer_(new RdataFieldComposer)
{
}

RdataEncoder::~RdataEncoder() {
    delete composer_;
}

void
RdataEncoder::clear() {
    composer_->clear();
    n_data_ = 0;
    n_varlen_fields_ = 0;
    name_data_len_ = 0;
    other_data_len_ = 0;
    encode_spec_ = NULL;
    data_lengths_.clear();
    data_offsets_.clear();
}

void
RdataEncoder::addRdata(const Rdata& rdata) {
    RdataFieldComposer field_composer;

    rdata.toWire(*composer_);
    composer_->extendData();    // make sure this is the end of data
    ++n_data_;
}

void
RdataEncoder::construct(RRType type) {
    vector<pair<size_t, size_t> >::const_iterator it_data =
        composer_->data_positions_.begin();
    vector<pair<size_t, size_t> >::const_iterator const it_data_end =
        composer_->data_positions_.end();
    vector<pair<size_t, Name> >::const_iterator it_name =
        composer_->names_.begin();
    vector<pair<size_t, Name> >::const_iterator const it_name_end =
        composer_->names_.end();

    encode_spec_ = &getRdataEncodeSpec(type);
    if (n_data_ * encode_spec_->n_names != composer_->names_.size()) {
        throw runtime_error("assumption failure: # of names mismatch");
    }
    size_t cur_pos = 0;
    n_varlen_fields_ = 0;
    size_t i;
    for (i = 0; i < n_data_; ++i) {
        for (size_t field = 0; field < encode_spec_->n_fields; ++field) {
            const RdataFieldSpec& field_spec = encode_spec_->field_spec[field];
            switch (field_spec.type) {
            case RdataFieldSpec::NAME:
            {
                if (it_name == it_name_end) {
                    throw runtime_error("assumption failure on # of names");
                }
                if (cur_pos != it_name->first) {
                    throw runtime_error("assumption failure on name position");
                }
                const Name& name = it_name->second;
                cur_pos += name.getLength();

                const LabelSequence seq(name);
                // 2-byte "header" + name and offset data
                name_data_len_ += (2 + seq.getDataLength() +
                                   seq.getLabelCount());

                ++it_name;
                break;
            }
            case RdataFieldSpec::FIXED_LEN_DATA:
            case RdataFieldSpec::VAR_LEN_DATA:
                if (it_data == it_data_end || cur_pos != it_data->first) {
                    throw runtime_error("assumption failure on # of data");
                }
                if (field_spec.type == RdataFieldSpec::FIXED_LEN_DATA) {
                    if (it_data->second != field_spec.len) {
                        // XXX this assumption is not actually correct, but
                        // should hold in most cases.  for prototype it should
                        // be okay.
                        throw runtime_error(
                            "assumption failure on fix-len data");
                    }
                } else {
                    ++n_varlen_fields_;
                }
                const size_t len = it_data->second;
                if (len > 0xffff) {
                    throw runtime_error("assumption failure: data too large");
                }
                const uint8_t* dp =
                    static_cast<const uint8_t*>(composer_->getData()) +
                    it_data->first;
                data_offsets_.push_back(
                    pair<const uint8_t*, size_t>(dp, len));
                cur_pos += len;
                other_data_len_ += len;
                ++it_data;
            }
        }
    }
    if (i != n_data_) {
        throw runtime_error("assumption failure: not all RDATAs are parsed");
    }
}

size_t
RdataEncoder::getStorageLength() const {
    // In this implementation, it's total data len +
    // sizeof(uint16_t) * # of variable-length data fields
    return (n_varlen_fields_ * sizeof(uint16_t) +
            name_data_len_ + other_data_len_);
}

size_t
RdataEncoder::encodeLengths(uint16_t* field_buf, size_t n_bufs) const {
    assert(n_varlen_fields_ <= n_bufs);

    vector<pair <const uint8_t*, size_t> >::const_iterator it_dataoff =
        data_offsets_.begin();
    vector<pair<const uint8_t*, size_t> >::const_iterator const
        it_dataoff_end = data_offsets_.end();
    size_t n_fields = 0;
    for (size_t i = 0; i < n_data_; ++i) {
        for (size_t field = 0; field < encode_spec_->n_fields; ++field) {
            const RdataFieldSpec& field_spec = encode_spec_->field_spec[field];
            if (field_spec.type == RdataFieldSpec::FIXED_LEN_DATA ||
                field_spec.type == RdataFieldSpec::VAR_LEN_DATA) {
                assert(it_dataoff != it_dataoff_end);
                if (field_spec.type == RdataFieldSpec::VAR_LEN_DATA) {
                    *field_buf = it_dataoff->second;
                    ++n_fields;
                }
                ++field_buf;
                ++it_dataoff;
            }
        }
    }

    assert(n_fields == n_varlen_fields_);
    return (n_fields);
}

size_t
RdataEncoder::encodeData(uint8_t* const data_buf, size_t bufsize) const {
    assert(name_data_len_ + other_data_len_ <= bufsize);

    vector<pair<const uint8_t*, size_t> >::const_iterator it_data =
        data_offsets_.begin();
    vector<pair<const uint8_t*, size_t> >::const_iterator const it_data_end =
        data_offsets_.end();
    vector<pair<size_t, Name> >::const_iterator it_name =
        composer_->names_.begin();
    vector<pair<size_t, Name> >::const_iterator const it_name_end =
        composer_->names_.end();

    vector<pair <const uint8_t*, size_t> >::const_iterator it_dataoff =
        data_offsets_.begin();
    vector<pair<const uint8_t*, size_t> >::const_iterator const
        it_dataoff_end = data_offsets_.end();

    uint8_t* dp = data_buf;
    uint8_t* const dp_end = data_buf + bufsize;
    for (size_t i = 0; i < n_data_; ++i) {
        for (size_t field = 0; field < encode_spec_->n_fields; ++field) {
            const RdataFieldSpec& field_spec = encode_spec_->field_spec[field];
            if (field_spec.type == RdataFieldSpec::NAME) {
                assert(it_name != it_name_end);
                const LabelSequence seq(it_name->second);
                size_t nlen;
                const uint8_t* np = seq.getData(&nlen);
                size_t olen;
                const uint8_t* op = seq.getOffsetData(&olen,
                                                      noffset_placeholder_);
                assert(dp + nlen + olen + 2 <= dp_end);
                *dp++ = nlen;
                *dp++ = olen;
                memcpy(dp, np, nlen);
                dp += nlen;
                memcpy(dp, op, olen);
                dp += olen;

                ++it_name;
            } else {            // fixed or variable size len
                assert(it_data != it_data_end);
                const size_t len = it_data->second;
                assert(dp + len <= dp_end);
                memcpy(dp, it_data->first, len);
                dp += len;
                ++it_data;
            }
        }
    }
    assert(it_data == it_data_end);
    assert(it_name == it_name_end);
    return (dp - data_buf);
}

void
renderName(dns::AbstractMessageRenderer* renderer,
           const uint8_t* encoded_ndata, unsigned int attributes)
{
    const size_t nlen = encoded_ndata[0];
    const size_t olen = encoded_ndata[1];
    renderer->writeName(LabelSequence(encoded_ndata + 2,
                                      encoded_ndata + 2 + nlen, olen),
                        (attributes & RdataFieldSpec::COMPRESSIBLE_NAME) != 0);
}

void
renderData(dns::AbstractMessageRenderer* renderer,
           const uint8_t* data, size_t len)
{
    renderer->writeData(data, len);
}

namespace {
void
addNameLen(size_t* total, const uint8_t* encoded_ndata, unsigned int) {
    const size_t nlen = encoded_ndata[0];
    const size_t olen = encoded_ndata[1];
    *total += (2 + nlen + olen);
}

void
addDataLen(size_t* total, const uint8_t*, size_t len) {
    *total += len;
}
}

size_t
getEncodedDataSize(dns::RRType type, uint16_t n_rdata, const uint8_t* buf) {
    const RdataEncodeSpec& encode_spec(getRdataEncodeSpec(type));
    const size_t n_len_fields = encode_spec.n_varlens * n_rdata;

    size_t data_len = 0;
    RdataIterator rd_it(type, n_rdata,
                        reinterpret_cast<const uint16_t*>(buf),
                        buf + n_len_fields * sizeof(uint16_t),
                        boost::bind(addNameLen, &data_len, _1, _2),
                        boost::bind(addDataLen, &data_len, _1, _2));
    while (!rd_it.isLast()) {
        rd_it.action();
    }
    return (data_len + n_len_fields * sizeof(uint16_t));
}

namespace {
const uint8_t*
getDataPos(const RdataEncodeSpec& encode_spec,
           const uint16_t* lengths, const uint8_t* data, uint16_t n_rdata)
{
    if (data != NULL) {
        return (data);
    }
    const size_t n_len_fields = encode_spec.n_varlens * n_rdata;
    return (reinterpret_cast<const uint8_t*>(lengths + n_len_fields));
}
}

RdataIterator::RdataIterator(RRType type, uint16_t n_rdata,
                             const uint16_t* lengths,
                             const uint8_t* encoded_data,
                             NameAction name_action, DataAction data_action) :
    encode_spec_(&getRdataEncodeSpec(type)),
    n_rdata_(n_rdata), rdata_count_(0), lengths_begin_(lengths),
    lengths_(lengths),
    data_begin_(getDataPos(*encode_spec_, lengths, encoded_data, n_rdata)),
    data_(data_begin_), name_action_(name_action), data_action_(data_action)
{
}

void
RdataIterator::action() {
    const uint8_t* dp = data_;

    size_t lcount = 0;          // counter for lengths
    for (size_t field = 0; field < encode_spec_->n_fields; ++field) {
        const RdataFieldSpec& field_spec = encode_spec_->field_spec[field];
        if (field_spec.type == RdataFieldSpec::NAME) {
            const size_t nlen = dp[0];
            const size_t olen = dp[1];
            name_action_(dp, field_spec.attributes);
            dp += 2 + nlen + olen;
        } else {
            const size_t len =
                (field_spec.type == RdataFieldSpec::FIXED_LEN_DATA) ?
                field_spec.len : lengths_[lcount++];
            data_action_(dp, len);
            dp += len;
        }
    }

    rdata_count_ += 1;
    lengths_ += lcount;
    data_ = dp;
}

} // internal
} // datasrc
} // isc
