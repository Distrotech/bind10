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

#include <exceptions/exceptions.h>

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
#include <asiodns/dns_lookup.h>
#include <asiolink/asiolink.h>

#include <boost/shared_ptr.hpp>
#include <boost/lexical_cast.hpp>

#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include <vector>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netdb.h>

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
                   bool debug, size_t expected_rate, bool do_rotate,
                   bool use_scatter_send, int sock, const sockaddr* sa,
                   socklen_t salen) :
        msg_handler_(new MessageHandler),
        queries_(queries),
        query_message_(query_message),
        response_message_(new Message(Message::PARSE)),
        buffer_(buffer),
        dummy_socket(IOSocket::getDummyUDPSocket()),
        dummy_endpoint(IOEndpointPtr(IOEndpoint::create(IPPROTO_UDP,
                                                        IOAddress("192.0.2.1"),
                                                        53210))),
        debug_(debug), now_(std::time(NULL)), total_count_(0),
        expected_rate_(expected_rate),
        buffers_(use_scatter_send ? &buffers_storage_ : NULL),
        sock_(sock), sa_(sa), salen_(salen)
    {
        msg_handler_->setCache(cache_table);
        msg_handler_->setRRRotation(do_rotate);
        std::memset(&mh_, 0, sizeof(mh_));
        mh_.msg_iov = iov_;
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
            if (++total_count_ == expected_rate_) {
                ++now_;
                total_count_ = 0;
            }
            if (buffers_) {
                buffers_->clear();
            }
            if (sock_ != -1) {
                socklen_t salen = sizeof(ss_);
                recvfrom(sock_, buf_, sizeof(buf_), 0,
                         static_cast<sockaddr*>(static_cast<void*>(&ss_)),
                         &salen);
            }
            msg_handler_->process(io_message, query_message_, buffer_,
                                  &server, now_, buffers_);
            if (sock_ != -1) {
                if (buffers_) {
                    mh_.msg_name = const_cast<sockaddr*>(sa_);
                    mh_.msg_namelen = salen_;
                    mh_.msg_iovlen = buffers_->size();
                    Buffers::iterator iter = buffers_->begin();
                    Buffers::iterator const iter_end = buffers_->end();
                    for (int i = 0; iter != iter_end; ++iter, ++i) {
                        iov_[i].iov_base = const_cast<void*>(iter->first);
                        iov_[i].iov_len = iter->second;
                    }
                    sendmsg(sock_, &mh_, 0);
                } else {
                    sendto(sock_, buffer_.getData(), buffer_.getLength(), 0,
                           sa_, salen_);
                }
            }
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
    std::time_t now_;
    size_t total_count_;
    const size_t expected_rate_;
    typedef std::vector<isc::asiodns::DNSLookup::Buffer> Buffers;
    Buffers buffers_storage_;
    Buffers* buffers_;
    char buf_[512];       // dummy buffer for recvfrom
    msghdr mh_;
    iovec iov_[64];
    const int sock_;
    struct sockaddr_storage ss_;
    const sockaddr* const sa_;
    const socklen_t salen_;
};

class BenchmarkError : public isc::Exception {
public:
    BenchmarkError(const char* file, size_t line, const char* what) :
        isc::Exception(file, line, what) {}
};

int
networkSetup(const char* addr, const char* port, const char* local_port,
             void* sa_storage, size_t salen)
{
    addrinfo hints;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    hints.ai_flags = AI_NUMERICHOST | AI_NUMERICSERV;

    addrinfo *res;
    const int error = getaddrinfo(addr, port, &hints, &res);
    if (error != 0) {
        isc_throw(BenchmarkError, "getaddrinfo failed: "
                  << gai_strerror(error));
    }
    assert(res->ai_addrlen <= salen);
    std::memcpy(sa_storage, res->ai_addr, res->ai_addrlen);
    salen = res->ai_addrlen;
    freeaddrinfo(res);

    hints.ai_flags |= AI_PASSIVE;
    const int error2 = getaddrinfo(NULL, local_port, &hints, &res);
    if (error2 != 0) {
        isc_throw(BenchmarkError, "getaddrinfo for passive failed: "
                  << gai_strerror(error2));
    }
    int s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (s < 0) {
        freeaddrinfo(res);
        isc_throw(BenchmarkError, "failed to open socket: "
                  << std::strerror(errno));
    }
    if (bind(s, res->ai_addr, res->ai_addrlen) < 0) {
        freeaddrinfo(res);
        close(s);
        isc_throw(BenchmarkError, "failed to bind socket: "
                  << std::strerror(errno));
    }
    freeaddrinfo(res);

    const int on = 1;
    if (ioctl(s, FIONBIO, &on) < 0) {
        close(s);
        isc_throw(BenchmarkError, "failed to make socket non-blocking: "
                  << std::strerror(errno));
    }

    return (s);
}

const int ITERATION_DEFAULT = 1;
const size_t EXPECTED_RATE_DEFAULT = 50000;
const char* const CLIENT_ADDRESS_DEFAULT = "127.0.0.1";
const char* const CLIENT_PORT_DEFAULT = "5301";
const char* const SERVER_PORT_DEFAULT = "5300";

void
usage() {
    std::cerr << "Usage: query_bench [-a address] [-d] [-n iterations] [-N] "
        "[-r expected rate] [-p port] [-P port] [-R] "
        "cache_file query_datafile\n"
        "  -a Specify the 'client' address when -N is specified (default: "
              << CLIENT_ADDRESS_DEFAULT << ")\n"
        "  -d Enable debug logging to stdout\n"
        "  -n Number of iterations per test case (default: "
              << ITERATION_DEFAULT << ")\n"
        "  -N Include network I/O\n"
        "  -p Specify the 'client' port when -N is specified (default: "
              << CLIENT_PORT_DEFAULT << ")\n"
        "  -P Specify the 'server' port when -N is specified (default: "
              << SERVER_PORT_DEFAULT << ")\n"
        "  -r Expected query rate/s, adjust TTL update frequency (default: "
              << EXPECTED_RATE_DEFAULT << ")\n"
        "  -R rotate answers\n"
        "  -s Emulate scatter-send mode\n"
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
    size_t expected_rate = EXPECTED_RATE_DEFAULT;
    bool debug_log = false;
    bool use_scatter_send = false;
    bool do_rotate = false;
    bool use_netio = false;
    const char* client_addr = CLIENT_ADDRESS_DEFAULT;
    const char* client_port = CLIENT_PORT_DEFAULT;
    const char* server_port = SERVER_PORT_DEFAULT;
    while ((ch = getopt(argc, argv, "a:dn:Nr:p:P:Rs")) != -1) {
        switch (ch) {
        case 'a':
            client_addr = optarg;
            break;
        case 'n':
            iteration = atoi(optarg);
            break;
        case 'N':
            use_netio = true;
            break;
        case 'd':
            debug_log = true;
            break;
        case 'p':
            client_port = optarg;
            break;
        case 'P':
            server_port = optarg;
            break;
        case 'r':
            expected_rate = boost::lexical_cast<size_t>(optarg);
            break;
        case 'R':
            do_rotate = true;
            break;
        case 's':
            use_scatter_send = true;
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
        int s = -1;
        sockaddr_storage ss;
        socklen_t salen = sizeof(ss);
        const sockaddr* sa = NULL;
        if (use_netio) {
            s = networkSetup(client_addr, client_port, server_port, &ss,
                             salen);
            sa = static_cast<const sockaddr*>(static_cast<const void*>(&ss));
        }

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
                                                 message, buffer, debug_log,
                                                 expected_rate, do_rotate,
                                                 use_scatter_send, s, sa,
                                                 salen));
    } catch (const std::exception& ex) {
        cout << "Test unexpectedly failed: " << ex.what() << endl;
        return (1);
    }

    return (0);
}
