// Copyright (C) 2011  Internet Systems Consortium, Inc. ("ISC")
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

#include <util/io/socket.h>
#include <util/io/socket_share.h>
#include <util/networking.h>

#include <util/unittests/fork.h>

#include <gtest/gtest.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <cstdio>

using namespace isc::util;
using namespace isc::util::io;
using namespace isc::util::unittests;

namespace {

// We test that we can transfer a pipe over other pipe
TEST(SDShare, transfer) {
    // Get a pipe and fork
    int pipes[2];
    ASSERT_NE(-1, socketpair(AF_UNIX, SOCK_STREAM, 0, pipes));
    pid_t sender(fork());
    ASSERT_NE(-1, sender);
    if(sender) { // We are in parent
        // Close the other side of pipe, we want only writible one
        EXPECT_NE(-1, close(pipes[0]));
        // Get a process to check data
        socket_type sd(invalid_socket);
        pid_t checker(check_output(&sd, "data", 4));
        ASSERT_NE(-1, checker);
        // Now, send the socket descriptor, close it and close the pipe
        EXPECT_NE(-1, send_socket(pipes[1], sd));
        EXPECT_NE(-1, close(pipes[1]));
        EXPECT_NE(socket_error_retval, closesocket(sd));
        // Check both subprocesses ended well
        EXPECT_TRUE(process_ok(sender));
        EXPECT_TRUE(process_ok(checker));
    } else { // We are in child. We do not use ASSERT here
        // Close the write end, we only read
        if(close(pipes[1])) {
            exit(1);
        }
        // Get the socket descriptor
        socket_type sd(invalid_socket);
        if (recv_socket(pipes[0], &sd) == -1) {
            exit(1);
        }
        // This pipe is not needed
        if(close(pipes[0])) {
            exit(1);
        }
        // Send "data" trough the received socket, close it and be done
        if(!write_data(sd, "data", 4) ||
           closesocket(sd) == socket_error_retval) {
            exit(1);
        }
        exit(0);
    }
}

}
