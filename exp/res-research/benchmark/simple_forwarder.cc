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

#include <util/io/sockaddr_util.h>

#include <iostream>

#include <cassert>
#include <cstdlib>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <unistd.h>

#include <sys/select.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

using namespace std;
using namespace isc::util::io::internal;

namespace {
unsigned long recv_count = 0;
unsigned long resp_count = 0;
unsigned long recvresp_count = 0;
unsigned long forward_count = 0;

void
sigterm_handler(int) {
    cout << recv_count << " packets received, "
         << forward_count << " forwarded, "
         << recvresp_count << " response received, "
         << resp_count << " responded" << endl;
    exit(0);
}

void
usage() {
    cout << "usage: simple_forwarder [-p port] [-P fwd_port] "
        "[-Q fwd_local_port] [-s size] [-S addr]\n";
    cout << "       -p port: specifies the receiving port (default 5300)\n";
    cout << "       -s size: specifies the size of responses (default 512)\n";
    cout << "       -P port: specifies the remote port for forwarding "
        "(default 5302)\n";
    cout << "       -Q port: specifies the local port for forwarding "
        "(default 5301)\n";
    cout << "       -S addr: specifies the remote address for forwarding "
        "(default 127.0.0.1)\n";
    exit(1);
}
}

int
main(int argc, char** argv) {
    int ch;
    const char* recvport = "5300";
    const char* fwd_localport = "5301";
    const char* fwdport = "5302";
    const char* fwdaddr = "127.0.0.1";
    size_t resp_size = 512;
    int rcvbuf_set = 0;         // unspecified
    while ((ch = getopt(argc, argv, "b:hp:P:Q:s:S:")) != -1) {
        switch (ch) {
        case 'b':
            rcvbuf_set = atoi(optarg);
            break;
        case 'p':
            recvport = optarg;
            break;
        case 'P':
            fwdport = optarg;
            break;
        case 'Q':
            fwd_localport = optarg;
            break;
        case 's':
            resp_size = atoi(optarg);
            break;
        case 'S':
            fwdaddr = optarg;
            break;
        case 'h':
            usage();
            break;
        }
    }

    argc -= optind;
    argv += optind;
    if (argc != 0) {
        usage();
    }

    // Create "server" socket that accepts queries
    struct addrinfo hints;
    struct addrinfo* res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    hints.ai_flags = AI_NUMERICSERV | AI_PASSIVE;
    const int error = getaddrinfo(NULL, recvport, &hints, &res);
    if (error != 0) {
        cerr << "getaddrinfo failed: " << gai_strerror(error) << endl;
        return (1);
    }
    const int s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (s < 0) {
        cout << "socket failed " << strerror(errno) << endl;
        return (1);
    }
    if (bind(s, res->ai_addr, res->ai_addrlen) != 0) {
        cout << "bind failed " << strerror(errno) << endl;
        return (1);
    }
    freeaddrinfo(res);

    int rcvbuf;
    socklen_t optlen = sizeof(rcvbuf);
    if (getsockopt(s, SOL_SOCKET, SO_RCVBUF, &rcvbuf, &optlen) != 0) {
        cout << "getsockopt failed " << strerror(errno) << endl;
        return (1);
    }
    cout << "current receive socket buffer=" << rcvbuf << endl;
    if (rcvbuf_set > 0) {
        if (setsockopt(s, SOL_SOCKET, SO_RCVBUF, &rcvbuf_set,
                       sizeof(rcvbuf_set)) != 0) {
            cout << "setsockopt failed " << strerror(errno) << endl;
            return (1);
        }
        cout << "receive socket buffer reset to=" << rcvbuf_set << endl;
    }

    // Create forwarding socket
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    hints.ai_flags = AI_NUMERICHOST | AI_NUMERICSERV;
    const int error2 = getaddrinfo(fwdaddr, fwdport, &hints, &res);
    if (error2 != 0) {
        cerr << "getaddrinfo failed for forward addr/port: "
             << gai_strerror(error2) << endl;
        return (1);
    }
    const int s_fwd = socket(res->ai_family, res->ai_socktype,
                             res->ai_protocol);
    if (s_fwd < 0) {
        cout << "socket failed " << strerror(errno) << endl;
        return (1);
    }
    struct sockaddr_storage fwd_ss;
    memcpy(&fwd_ss, res->ai_addr, res->ai_addrlen);
    const socklen_t fwd_salen = res->ai_addrlen;
    freeaddrinfo(res);

    // Bind the forwarding socket
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    hints.ai_flags = AI_NUMERICSERV | AI_PASSIVE;
    const int error3 = getaddrinfo(NULL, fwd_localport, &hints, &res);
    if (error3 != 0) {
        cerr << "getaddrinfo failed: " << gai_strerror(error3) << endl;
        return (1);
    }
    if (bind(s_fwd, res->ai_addr, res->ai_addrlen) != 0) {
        cout << "bind (local forward port) failed " << strerror(errno) << endl;
        return (1);
    }
    freeaddrinfo(res);

    signal(SIGTERM, sigterm_handler);
    signal(SIGINT, sigterm_handler);

    struct sockaddr_storage from_ss;
    socklen_t fromlen = sizeof(from_ss);
    struct sockaddr_storage from_fwd_ss;
    uint8_t recvbuf[4096];
    assert(sizeof(recvbuf) >= resp_size);

    fd_set fdset_base;
    FD_ZERO(&fdset_base);
    FD_SET(s, &fdset_base);
    FD_SET(s_fwd, &fdset_base);
    const int max_fd = std::max(s, s_fwd);
    while (true) {
        fd_set fdset = fdset_base;
        const int n = select(max_fd + 1, &fdset, NULL, NULL, NULL);
        if (n == -1) {
            cout << "select failed " << strerror(errno) << endl;
            return (1);
        }

        // This is a "normal query" from a client
        if (FD_ISSET(s, &fdset)) {
            int cc = recvfrom(s, recvbuf, sizeof(recvbuf), 0,
                              convertSockAddr(&from_ss), &fromlen);
            if (cc <= 0) {
                cout << "unexpected empty result on recvfrom" << endl;
                return (1);
            }
            ++recv_count;

            // emulating "90% cache hit": 10% of queries are forwarded, and
            // the rest are responded immediately.
            if ((recv_count % 10) == 9) {
                cc = sendto(s_fwd, recvbuf, cc, 0, convertSockAddr(&fwd_ss),
                            fwd_salen);
                if (cc < 0) {
                    cout << "unexpected result on sendto for forward" << endl;
                    return (1);
                }
                ++forward_count;
            } else {
                cc = sendto(s, recvbuf, resp_size, 0,
                            convertSockAddr(&from_ss), fromlen);
                if (cc < 0 || cc != resp_size) {
                    cout << "unexpected result on sendto";
                    if (cc < 0) {
                        cout << ": " << strerror(errno);
                    }
                    cout << endl;
                    return (1);
                }
                ++resp_count;
            }
        }

        struct sockaddr* from_fwd = convertSockAddr(&from_fwd_ss);
        socklen_t fromfwd_len = sizeof(from_fwd_ss);
        if (FD_ISSET(s_fwd, &fdset)) {
            // If we get a response from the "remote server", send it back
            // to the client
            int cc = recvfrom(s_fwd, recvbuf, sizeof(recvbuf), 0,
                              from_fwd, &fromfwd_len);
            if (cc <= 0) {
                cout << "unexpected empty result on recvfrom" << endl;
                return (1);
            }
            ++recvresp_count;
            if (recv_count == 0) {
                cout << "skip responding" << endl;
            } else {
                cc = sendto(s, recvbuf, resp_size, 0,
                            convertSockAddr(&from_ss), fromlen);
                if (cc < 0 || cc != resp_size) {
                    cout << "unexpected result on sendto 2";
                    if (cc < 0) {
                        cout << ": " << strerror(errno);
                    }
                    cout << endl;
                    return (1);
                }
            }
            ++resp_count;
        }
    }
    return (0);
}
