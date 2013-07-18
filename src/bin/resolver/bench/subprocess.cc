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

#include <resolver/bench/subprocess.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <cstdlib>
#include <iostream>
#include <cerrno>

using namespace std;

namespace isc {
namespace resolver {
namespace bench {

Subprocess::Subprocess(const boost::function<void(int)>& main) :
    channel_(-1),
    pid(-1)
{
    int sockets[2];
    int result = socketpair(AF_UNIX, SOCK_STREAM, 0, sockets);
    assert(result == 0);
    pid = fork();
    assert(pid != -1);
    if (pid) {
        // In the parent. Close the child side of socket, return to caller.
        close(sockets[1]);
        channel_ = sockets[0];
        return;
    } else {
        // In the child. We close the parent side of socket and run the
        // main function. Then we terminate the process.
        close(sockets[0]);
        try {
            main(sockets[1]);
            close(sockets[1]);
            exit(0);
        } catch (const exception& e) {
            cerr << "Exception: " << e.what() << endl;
        } catch (...) {
            cerr << "Unknown exception" << endl;
        }
        abort();
    }
}

Subprocess::~Subprocess() {
    int result = waitpid(pid, NULL, 0);
    assert(result != -1);
}

void
Subprocess::send(const void* data, size_t size) {
    const uint8_t* buffer = (const uint8_t*) data;
    while (size > 0) {
        ssize_t result = ::send(channel(), buffer, size, 0);
        if (result == -1) {
            assert(errno == EINTR);
        } else {
            size -= result;
            buffer += result;
        }
    }
}

std::string
Subprocess::read(size_t size) {
    char target[size + 1];
    target[size] = '\0';
    char* position = target;
    while (size > 0) {
        ssize_t result = recv(channel(), position, size, 0);
        if (result == -1) {
            assert(errno == EINTR);
        } else if (result == 0) {
            // EOF. Terminate sting.
            *position = 0;
            break;
        } else {
            position += result;
            size -= result;
        }
    }
    return (target); // Convert from char* to string
}

}
}
}
