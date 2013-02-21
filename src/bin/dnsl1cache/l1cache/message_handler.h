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

#ifndef MESSAGE_HANDLER_H
#define MESSAGE_HANDLER_H 1

#include <util/buffer.h>
#include <dns/dns_fwd.h>
#include <asiolink/asiolink.h>
#include <asiodns/dns_server.h>
#include <asiodns/dns_lookup.h>

#include <boost/noncopyable.hpp>

#include <ctime>

namespace isc {
namespace dnsl1cache {

class DNSL1HashTable;

class MessageHandler : boost::noncopyable {
public:
    MessageHandler();
    ~MessageHandler();

    void process(const asiolink::IOMessage& io_message,
                 dns::Message& message, util::OutputBuffer& buffer,
                 asiodns::DNSServer* server, std::time_t now,
                 std::vector<asiodns::DNSLookup::Buffer>* buffers);
    void setCache(DNSL1HashTable* cache_table);
    void setRRRotation(bool enable);

private:
    class MessageHandlerImpl;
    MessageHandlerImpl* impl_;
};

} // namespace dnsl1cache
} // namespace isc
#endif // MESSAGE_HANDLER_H

// Local Variables:
// mode: c++
// End:
