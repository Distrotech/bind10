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

#define B10_LIBXFR_EXPORT

#include <config.h>

#include <cstdlib>
#include <cstring>
#include <iostream>

// for some IPC/network system calls in asio/detail/pipe_select_interrupter.hpp
#ifndef _WIN32
#include <unistd.h>
#endif
#include <asio.hpp>

#include <util/io/fd_share.h>
#include <xfr/xfrout_client.h>

using namespace std;
using namespace isc::util::io;
#ifndef _WIN32
using asio::local::stream_protocol;
#else
#include <boost/lexical_cast.hpp>
using asio::ip::tcp;
#define stream_protocol tcp
#endif

namespace isc {
namespace xfr {

struct XfroutClientImpl {
    XfroutClientImpl(const string& file);
    const std::string file_path_;
    asio::io_service io_service_;
    // The socket used to communicate with the xfrout server.
    stream_protocol::socket socket_;
};

XfroutClientImpl::XfroutClientImpl(const string& file) :
    file_path_(file), socket_(io_service_)
{}

XfroutClient::XfroutClient(const string& file) :
    impl_(new XfroutClientImpl(file))
{}

XfroutClient::~XfroutClient() {
    delete impl_;
}

void
XfroutClient::connect() {
    try {
#ifndef _WIN32
        stream_protocol::endpoint ep(impl_->file_path_);
#else
        asio::ip::address addr;
        if (impl_->file_path_.find("v4_") == 0)
            addr = asio::ip::address_v4::loopback();
        else if (impl_->file_path_.find("v6_") == 0)
            addr = asio::ip::address_v6::loopback();
        else
            isc_throw(XfroutError, "bad endpoint: " << impl_->file_path_);
        uint16_t port = boost::lexical_cast<uint16_t>(impl_->file_path_.substr(3));
        stream_protocol::endpoint ep(addr, port);
#endif
        impl_->socket_.connect(ep);
#ifdef _WIN32
    } catch (const boost::bad_lexical_cast&) {
        isc_throw(XfroutError, "bad port in endpoint: " << impl_->file_path_);
#endif
    } catch (const asio::system_error& err) {
        isc_throw(XfroutError, "socket connect failed for " <<
                  impl_->file_path_ << ": " << err.what());
    }
}

void
XfroutClient::disconnect() {
    asio::error_code err;
    impl_->socket_.close(err);
    if (err) {
        isc_throw(XfroutError, "close socket failed: " << err.message());
    }
}

int
XfroutClient::sendXfroutRequestInfo(
#ifdef _WIN32
                                    const SOCKET tcp_sock,
#else
                                    const int tcp_sock,
#endif
                                    const void* const msg_data,
                                    const uint16_t msg_len)
{
    if (send_fd(impl_->socket_.native(), tcp_sock) < 0) {
        isc_throw(XfroutError,
                  "Failed to send the socket file descriptor "
                  "to xfrout module");
    }

    // TODO: this shouldn't be blocking send, even though it's unlikely to
    // block.
    // Converting the 16-bit word to network byte order.

    // Splitting msg_len below performs something called a 'narrowing
    // conversion' (conversion of uint16_t to uint8_t). C++0x (and GCC
    // 4.7) requires explicit casting when a narrowing conversion is
    // performed. For reference, see 8.5.4/6 of n3225.
    const uint8_t lenbuf[2] = { static_cast<uint8_t>(msg_len >> 8),
                                static_cast<uint8_t>(msg_len & 0xff) };
#ifndef _WIN32
    if (send(impl_->socket_.native(), lenbuf, sizeof(lenbuf), 0) !=
        sizeof(lenbuf)) {
        isc_throw(XfroutError,
                  "failed to send XFR request length to xfrout module");
    }
    if (send(impl_->socket_.native(), msg_data, msg_len, 0) != msg_len) {
        isc_throw(XfroutError,
                  "failed to send XFR request data to xfrout module");
    }
#else
    if (send(impl_->socket_.native(),
             (const char *)lenbuf,
             sizeof(lenbuf), 0) != sizeof(lenbuf)) {
        isc_throw(XfroutError,
                  "failed to send XFR request length to xfrout module");
    }
    if (send(impl_->socket_.native(),
             (const char *)msg_data,
             msg_len, 0) != msg_len) {
        isc_throw(XfroutError,
                  "failed to send XFR request data to xfrout module");
    }
#endif

    return (0);
}

} // End for xfr
} // End for isc

