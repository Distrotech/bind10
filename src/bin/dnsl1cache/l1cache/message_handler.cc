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

using namespace isc::dns;
using isc::asiolink::IOMessage;
using isc::util::OutputBuffer;
using isc::util::InputBuffer;
using isc::asiodns::DNSServer;

namespace isc {
namespace dnsl1cache {

#if 0
// This is a derived class of \c DNSLookup, to serve as a
// callback in the asiolink module.  It calls
// AuthSrv::processMessage() on a single DNS message.
class MessageLookup : public DNSLookup {
public:
    MessageLookup() {}
    virtual void operator()(const IOMessage& io_message,
                            MessagePtr message,
                            MessagePtr, // Not used here
                            OutputBufferPtr buffer,
                            DNSServer* server) const
    {
        server_->processMessage(io_message, *message, *buffer, server);
    }
private:
    * server_;
};

class MessageAnswer : public DNSAnswer {
public:
    virtual void operator()(const IOMessage&, MessagePtr,
                            MessagePtr, OutputBufferPtr) const
    {}
};
#endif

class MessageHandler::MessageHandlerImpl {
public:
    MessageHandlerImpl() : cache_table_(NULL) {}
    void process(const IOMessage& io_message, Message& message,
                 OutputBuffer& buffer, DNSServer* server);

    DNSL1HashTable* cache_table_;
private:
    uint8_t labels_buf_[LabelSequence::MAX_SERIALIZED_LENGTH];
};

void
MessageHandler::MessageHandlerImpl::process(
    const IOMessage& io_message, Message& message,
    OutputBuffer& buffer, DNSServer* server)
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
    //const Name qname(request_buffer);
    //const LabelSequence qry_labels(qname);
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

    // Render Question section.  We know we don't have to compress.
    size_t nlen;
    const uint8_t* ndata = qry_labels.getData(&nlen);
    buffer.writeData(ndata, nlen);
    qtype.toWire(buffer);
    qclass.toWire(buffer);

    // Copy rest of the data.  Names are already compressed.
    buffer.writeData(entry->getDataBuf(entry->getOffsetBuf(
                                           qry_labels.getSerializedLength())),
                     entry->data_len_);
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
                        asiodns::DNSServer* server)
{
    impl_->process(io_message, message, buffer, server);
}

void
MessageHandler::setCache(DNSL1HashTable* cache_table) {
    impl_->cache_table_ = cache_table;
}

}
}