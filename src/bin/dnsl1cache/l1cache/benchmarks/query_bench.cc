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

#include <config.h>

#include <bench/benchmark.h>
#include <bench/benchmark_util.h>

#include <dnsl1cache/l1cache/message_handler.h>
#include <dnsl1cache/l1cache/l1hash.h>

#include <util/buffer.h>

#include <dns/message.h>
#include <dns/name.h>
#include <dns/question.h>
#include <dns/rrclass.h>

#include <log/logger_support.h>

#include <util/unittests/mock_socketsession.h>

#include <asiodns/dns_server.h>
#include <asiolink/asiolink.h>

#include <boost/shared_ptr.hpp>

#include <cstdlib>
#include <iostream>
#include <vector>

using namespace isc::dnsl1cache;
using namespace isc::dns;
using namespace isc::bench;
using isc::util::OutputBuffer;
using isc::util::InputBuffer;
using isc::asiodns::DNSServer;
using isc::asiolink::IOEndpoint;
using isc::asiolink::IOSocket;
using isc::asiolink::IOMessage;
using isc::asiolink::IOAddress;
using std::cout;
using std::endl;

namespace {
// Just something to pass as the server to resume
class DummyServer : public DNSServer {
    public:
        virtual void operator()(asio::error_code, size_t) {}
        virtual void resume(const bool) {}
        virtual DNSServer* clone() {
            return (new DummyServer(*this));
        }
};

class QueryBenchMark {
private:
    typedef boost::shared_ptr<MessageHandler> MessageHandlerPtr;
    typedef boost::shared_ptr<const IOEndpoint> IOEndpointPtr;
    typedef boost::shared_ptr<Message> MessagePtr;
public:
    QueryBenchMark(DNSL1HashTable* cache_table, const BenchQueries& queries,
                   Message& query_message, OutputBuffer& buffer,
                   bool debug) :
        msg_handler_(new MessageHandler),
        queries_(queries),
        query_message_(query_message),
        response_message_(new Message(Message::PARSE)),
        buffer_(buffer),
        dummy_socket(IOSocket::getDummyUDPSocket()),
        dummy_endpoint(IOEndpointPtr(IOEndpoint::create(IPPROTO_UDP,
                                                        IOAddress("192.0.2.1"),
                                                        53210))),
        debug_(debug)
    {
        msg_handler_->setCache(cache_table);
    }
public:
    unsigned int run() {
        BenchQueries::const_iterator query;
        const BenchQueries::const_iterator query_end = queries_.end();
        DummyServer server;
        for (query = queries_.begin(); query != query_end; ++query) {
            IOMessage io_message(&(*query)[0], (*query).size(), dummy_socket,
                                 *dummy_endpoint);
            query_message_.clear(Message::PARSE);
            buffer_.clear();
            msg_handler_->process(io_message, query_message_, buffer_,
                                  &server);

            if (debug_) {
                response_message_->clear(Message::PARSE);
                InputBuffer ib(buffer_.getData(), buffer_.getLength());
                response_message_->fromWire(ib);
                std::cout << *response_message_ << std::endl;
            }
        }

        return (queries_.size());
    }

private:
    MessageHandlerPtr msg_handler_;
    const BenchQueries& queries_;
    Message& query_message_;
    MessagePtr response_message_;
    OutputBuffer& buffer_;
    IOSocket& dummy_socket;
    IOEndpointPtr dummy_endpoint;
    const bool debug_;
};

const int ITERATION_DEFAULT = 1;

void
usage() {
    std::cerr << "Usage: query_bench [-d] [-n iterations] "
        "cache_file query_datafile\n"
        "  -d Enable debug logging to stdout\n"
        "  -n Number of iterations per test case (default: "
              << ITERATION_DEFAULT << ")\n"
        "  cache_file: cache data\n"
        "  query_datafile: queryperf style input data"
              << std::endl;
    exit (1);
}
}

int
main(int argc, char* argv[]) {
    int ch;
    int iteration = ITERATION_DEFAULT;
    bool debug_log = false;
    while ((ch = getopt(argc, argv, "dn:")) != -1) {
        switch (ch) {
        case 'n':
            iteration = atoi(optarg);
            break;
        case 'd':
            debug_log = true;
            break;
        default:
            usage();
        }
    }
    argc -= optind;
    argv += optind;
    if (argc < 2) {
        usage();
    }

    const char* const cache_file = argv[0];
    const char* const query_data_file = argv[1];

    // By default disable logging to avoid unwanted noise.
    initLogger("query-bench", debug_log ? isc::log::DEBUG : isc::log::NONE,
               isc::log::MAX_DEBUG_LEVEL, NULL);

    try {
        BenchQueries queries;
        loadQueryData(query_data_file, queries, RRClass::IN());
        OutputBuffer buffer(4096);
        Message message(Message::PARSE);

        cout << "Parameters:" << endl;
        cout << "  Iterations: " << iteration << endl;
        cout << "  Cache data: file=" << cache_file << endl;
        cout << "  Query data: file=" << query_data_file << " ("
             << queries.size() << " queries)" << endl << endl;

        DNSL1HashTable cache_table(cache_file);
        BenchMark<QueryBenchMark>(iteration,
                                  QueryBenchMark(&cache_table, queries,
                                                 message, buffer, debug_log));
    } catch (const std::exception& ex) {
        cout << "Test unexpectedly failed: " << ex.what() << endl;
        return (1);
    }

    return (0);
}
