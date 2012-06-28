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

#include <iostream>

#include <cassert>
#include <cstdlib>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <unistd.h>

#include <sys/errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

using namespace std;

namespace {
unsigned long recv_count = 0;

void
sigterm_handler(int) {
    cout << "# of received packets=" << recv_count << endl;
    exit(0);
}
}

int
main(int argc, char** argv) {
    int ch;
    const char* recvport = "5300";
    size_t resp_size = 512;
    while ((ch = getopt(argc, argv, "p:s:")) != -1) {
        switch (ch) {
        case 'p':
            recvport = optarg;
            break;
        case 's':
            resp_size = atoi(optarg);
            break;
        }
    }

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

    signal(SIGTERM, sigterm_handler);
    signal(SIGINT, sigterm_handler);

    const int s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (s < 0) {
        cout << "socket failed " << strerror(errno) << endl;
        return (1);
    }
    if (bind(s, res->ai_addr, res->ai_addrlen) != 0) {
        cout << "bind failed " << strerror(errno) << endl;
        return (1);
    }
    struct sockaddr_storage from_ss;
    uint8_t recvbuf[512];
    assert(sizeof(recvbuf) >= resp_size);
    while (true) {
        struct sockaddr* from = (struct sockaddr*)&from_ss;
        socklen_t fromlen = sizeof(from_ss);
        int cc = recvfrom(s, recvbuf, sizeof(recvbuf), 0,
                                from, &fromlen);
        if (cc == 0) {
            cout << "unexpected empty result on recvfrom" << endl;
            return (1);
        }
        if (cc < 0) {
            cout << "recvfrom failed: " << strerror(errno) << endl;
            return (1);
        }
        cc = sendto(s, recvbuf, resp_size, 0, from, fromlen);
        if (cc < 0) {
            cout << "sendto failed: " << strerror(errno) << endl;
            return (1);
        }
        if (cc != resp_size) {
            cout << "unexpected result on sendto" << endl;
            return (1);
        }
        ++recv_count;
    }
    return (0);
}
