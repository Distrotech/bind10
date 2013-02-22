// Copyright (C) 2013  Internet Systems Consortium, Inc. ("ISC")
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

#include <dnsl1cache/l1cache/message_handler.h>
#include <dnsl1cache/logger.h>
#include <dnsl1cache/l1cache/l1hash.h>

#include <exceptions/exceptions.h>

#include <util/buffer.h>

#include <log/logger_support.h>

#include <dns/message.h>
#include <dns/labelsequence.h>
#include <dns/rrtype.h>
#include <dns/rrclass.h>
#include <dns/opcode.h>

#include <asiolink/asiolink.h>

#include <asiodns/dns_server.h>

#include <ctime>
#include <vector>
#include <utility>

using namespace isc::dns;
using isc::asiolink::IOMessage;
using isc::util::OutputBuffer;
using isc::util::InputBuffer;
using isc::asiodns::DNSServer;
using isc::asiodns::DNSLookup;

namespace isc {
namespace dnsl1cache {

class MessageHandler::MessageHandlerImpl {
public:
    MessageHandlerImpl() :
        cache_table_(NULL), rotate_rr_(false), rotate_count_(0)
    {}
    void process(const IOMessage& io_message, Message& message,
                 OutputBuffer& buffer, DNSServer* server, std::time_t now,
                 std::vector<DNSLookup::Buffer>* buffers);

    DNSL1HashTable* cache_table_;
    bool rotate_rr_;
private:
    uint8_t labels_buf_[LabelSequence::MAX_SERIALIZED_LENGTH];
    size_t rotate_count_;       // for RR rotation
};

inline uint32_t
ntohUint32(const uint8_t* cp) {
    return ((static_cast<uint32_t>(cp[0]) << 24) |
            (static_cast<uint32_t>(cp[1]) << 16) |
            (static_cast<uint32_t>(cp[2]) << 8) |
            (static_cast<uint32_t>(cp[3])));
}

void
MessageHandler::MessageHandlerImpl::process(
    const IOMessage& io_message, Message& message,
    OutputBuffer& buffer, DNSServer* server,
    std::time_t now, std::vector<asiodns::DNSLookup::Buffer>* buffers)
{
    InputBuffer request_buffer(io_message.getData(), io_message.getDataSize());

    // First, check the header part.  If we fail even for the base header,
    // just drop the message.
    message.clear(Message::PARSE);
    try {
        message.parseHeader(request_buffer);

        // Ignore all responses.
        if (message.getHeaderFlag(Message::HEADERFLAG_QR)) {
            LOG_DEBUG(logger, DBGLVL_COMMAND_DATA,
                      DNSL1CACHE_RESPONSE_RECEIVED);
            server->resume(false);
            return;
        }
    } catch (const Exception& ex) {
        LOG_DEBUG(logger, DBGLVL_COMMAND_DATA, DNSL1CACHE_HEADER_PARSE_FAIL)
                  .arg(ex.what());
        server->resume(false);
        return;
    }

    if (message.getOpcode() != Opcode::QUERY() ||
        message.getRRCount(Message::SECTION_QUESTION) != 1) {
        // Should return NOTIMP or something
        server->resume(false);
        return;
    }

    const LabelSequence qry_labels(request_buffer, labels_buf_);
    const RRType qtype(request_buffer);
    const RRClass qclass(request_buffer);
    LOG_DEBUG(logger, DBGLVL_TRACE_DETAIL, DNSL1CACHE_RECEIVED_QUERY).
        arg(qry_labels).arg(qclass).arg(qtype);
    if (qclass != RRClass::IN()) {
        isc_throw(Unexpected, "experimental assumption failure");
    }

    if (cache_table_ == NULL) {
        // Should return SERVFAIL or something
        server->resume(false);
        return;
    }

    DNSL1HashEntry* entry = cache_table_->find(qry_labels, qtype);
    if (entry == NULL) {
        isc_throw(Unexpected, "cache entry not found: experimental assumption "
                  "failure: " << qry_labels << "/" << qtype);
    }

    // Copy the part of DNS header (we know qdcount is 1 here)
    buffer.writeData(io_message.getData(), 6);
    // Adjust flags and rcode (set response and RA flags)
    request_buffer.setPosition(2);
    uint16_t flag_code = request_buffer.readUint16();
    flag_code |= (0x8080 | entry->rcode_.getCode());
    buffer.writeUint16At(flag_code, 2);
    buffer.writeUint16(entry->ancount_);
    buffer.writeUint16(entry->nscount_);
    buffer.writeUint16(entry->adcount_);
    if (buffers) {
        buffers->push_back(DNSLookup::Buffer(buffer.getData(), 12));
    }

    // Render Question section.  We know we don't have to compress it.
    size_t nlen;
    const uint8_t* ndata = qry_labels.getData(&nlen);
    if (buffers) {
        const uint8_t* question =
            static_cast<const uint8_t*>(io_message.getData()) + 12;
        buffers->push_back(DNSLookup::Buffer(question, nlen + 4));
    } else {
        buffer.writeData(ndata, nlen);
        qtype.toWire(buffer);
        qclass.toWire(buffer);
    }

    // Adjust TTLs, if necessary.
    uint16_t* offp =
        entry->getOffsetBuf(qry_labels.getSerializedLength());
    uint8_t* dp = static_cast<uint8_t*>(entry->getDataBuf(offp));
    uint8_t* const dp_beg = dp;
    const std::time_t elapsed = now - entry->last_used_time_;
    if (elapsed > 0) {
        entry->last_used_time_ = now;
    }

    const bool do_rotate =
        (rotate_rr_ &&
         (entry->data_len_ & DNSL1HashEntry::FLAG_ROTATABLE) != 0);
    const uint16_t data_len = (entry->data_len_ & DNSL1HashEntry::MASK_OFFSET);
    size_t cur_data_len = 0;
    const uint8_t* cur_data_beg = dp_beg;
    uint32_t ttl_val = 0;
    if (elapsed > 0 || do_rotate) {
        const size_t rr_count = entry->ancount_ + entry->nscount_ +
            entry->adcount_;
        for (size_t i = 0; i < rr_count; ++i) {
            if (elapsed > 0) {
                // skip TYPE AND CLASS
                uint8_t* ttlp = dp_beg + offp[i * 2 + 1] + 4;
                ttl_val = ntohUint32(ttlp) - elapsed;
                *ttlp++ = ((ttl_val & 0xff000000) >> 24);
                *ttlp++ = ((ttl_val & 0x00ff0000) >> 16);
                *ttlp++ = ((ttl_val & 0x0000ff00) >> 8);
                *ttlp = (ttl_val & 0x000000ff);
            }
            if (do_rotate) {
                const bool rotate_start =
                    ((offp[i * 2] & (DNSL1HashEntry::FLAG_START_RRSET |
                                     DNSL1HashEntry::FLAG_ROTATABLE)) ==
                     (DNSL1HashEntry::FLAG_START_RRSET |
                      DNSL1HashEntry::FLAG_ROTATABLE));
                if (!rotate_start) {
                    const uint8_t* next_off =
                        dp_beg + ((i == rr_count - 1) ? data_len :
                                  (offp[(i + 1) * 2] &
                                   DNSL1HashEntry::MASK_OFFSET));
                    cur_data_len = next_off - cur_data_beg;
                    continue;
                }
                // Flush the data so far
                if (cur_data_len > 0) {
                    if (buffers) {
                        buffers->push_back(DNSLookup::Buffer(cur_data_beg,
                                                             cur_data_len));
                    } else {
                        buffer.writeData(cur_data_beg, cur_data_len);
                    }
                }

                // Do rotate write
                // First, identify the beginning of next RRset (or end of data)
                const size_t beg_rr = i; // remember the current index
                while (++i < rr_count &&
                       (offp[i * 2] & DNSL1HashEntry::FLAG_START_RRSET) == 0) {
                    ;
                }
                const uint8_t* next_off =
                    dp_beg + ((i == rr_count) ? data_len :
                              (offp[i * 2] & DNSL1HashEntry::MASK_OFFSET));
                // Determine the shift count
                const size_t n_rrs = i - beg_rr;
                if (elapsed > 0) {
                    for (size_t j = 1; j < n_rrs; ++j) {
                        uint8_t* ttlp =
                            dp_beg + offp[(j + beg_rr) * 2 + 1] + 4;
                        *ttlp++ = ((ttl_val & 0xff000000) >> 24);
                        *ttlp++ = ((ttl_val & 0x00ff0000) >> 16);
                        *ttlp++ = ((ttl_val & 0x0000ff00) >> 8);
                        *ttlp = (ttl_val & 0x000000ff);
                    }
                }
                if ((rotate_count_ % n_rrs) == 0) {
                    // A bit of optimization: no need to rotate in this case.
                    const uint8_t* d =
                        dp_beg + (offp[beg_rr * 2] &
                                  DNSL1HashEntry::MASK_OFFSET);
                    if (buffers) {
                        buffers->push_back(DNSLookup::Buffer(d, next_off - d));
                    } else {
                        buffer.writeData(d, next_off - d);
                    }
                    cur_data_beg = next_off;
                    cur_data_len = 0;
                    continue;
                }
                for (size_t j = 0; j < n_rrs; ++j) {
                    // Identify the length of the owner name field
                    const size_t noff =
                        (offp[(beg_rr + j) * 2] & DNSL1HashEntry::MASK_OFFSET);
                    const size_t nlen = offp[(beg_rr + j) * 2 + 1] - noff;

                    // Identify the position and length of the rest of the data
                    // (rotated)
                    const size_t data_idx = (j + rotate_count_) % n_rrs;
                    const uint8_t* rdp =
                        dp_beg + offp[(beg_rr + data_idx) * 2 + 1];
                    const uint8_t* rdp_end =
                        (data_idx == n_rrs - 1) ? next_off :
                        dp_beg +
                        (offp[(beg_rr + data_idx + 1) * 2] &
                         DNSL1HashEntry::MASK_OFFSET);

                    if (buffers) {
                        buffers->push_back(DNSLookup::Buffer(dp_beg + noff,
                                                             nlen));
                        buffers->push_back(DNSLookup::Buffer(
                                               rdp, rdp_end - rdp));
                    } else {
                        buffer.writeData(dp_beg + noff, nlen);
                        buffer.writeData(rdp, rdp_end - rdp);
                    }
                }

                cur_data_beg = next_off;
                cur_data_len = 0;
            }
        }
    }
    if (cur_data_len > 0) {
        if (buffers) {
            buffers->push_back(DNSLookup::Buffer(cur_data_beg, cur_data_len));
        } else {
            buffer.writeData(cur_data_beg, cur_data_len);
        }
    }

    // Copy rest of the data.  Names are already compressed.
    if (do_rotate) {
        ++rotate_count_;
    } else {
        if (buffers) {
            buffers->push_back(DNSLookup::Buffer(dp_beg, data_len));
        } else {
            buffer.writeData(dp_beg, data_len);
        }
    }

    server->resume(true);
}

MessageHandler::MessageHandler() : impl_(new MessageHandlerImpl)
{}

MessageHandler::~MessageHandler() {
    delete impl_;
}

void
MessageHandler::process(const asiolink::IOMessage& io_message,
                        dns::Message& message, util::OutputBuffer& buffer,
                        asiodns::DNSServer* server, std::time_t now,
                        std::vector<asiodns::DNSLookup::Buffer>* buffers)
{
    try {
        impl_->process(io_message, message, buffer, server, now, buffers);
    } catch (const Exception& ex) {
        LOG_DEBUG(logger, DBGLVL_COMMAND_DATA, DNSL1CACHE_MESSAGE_PROCESS_FAIL)
                  .arg(ex.what());
        server->resume(false);
    }
}

void
MessageHandler::setCache(DNSL1HashTable* cache_table) {
    impl_->cache_table_ = cache_table;
}

void
MessageHandler::setRRRotation(bool enable) {
    impl_->rotate_rr_ = enable;
}

}
}
