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

#include "sockcreator.h"

#include <util/io/socket.h>
#include <util/io/sockaddr_util.h>

#include <cerrno>
#include <string.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

using namespace isc::util::io;
using namespace isc::util::io::internal;
using namespace isc::socket_creator;

namespace {

// Simple wrappers for read_data/write_data that throw an exception on error.
void
readMessage(const socket_type s, void* where, const size_t length) {
    if (read_data(s, where, length) < length) {
        isc_throw(ReadError, "Error reading from socket creator client");
    }
}

void
writeMessage(const socket_type s, const void* what, const size_t length) {
    if (!write_data(s, what, length)) {
        isc_throw(WriteError, "Error writing to socket creator client");
    }
}

// Exit on a protocol error after informing the client of the problem.
void
protocolError(const socket_type s, const char reason = 'I') {

    // Tell client we have a problem
    char message[2];
    message[0] = 'F';
    message[1] = reason;
    writeMessage(s, message, sizeof(message));

    // ... and exit
    isc_throw(ProtocolError, "Fatal error, reason: " << reason);
}

// Return appropriate socket type constant for the socket type requested.
// The output_sd argument is required to report a protocol error.
int
getSocketType(const char type_code, const socket_type output_sd) {
    int s_type = 0;
    switch (type_code) {
        case 'T':
            s_type = SOCK_STREAM;
            break;

        case 'U':
            s_type = SOCK_DGRAM;
            break;

        default:
            protocolError(output_sd);   // Does not return
    }
    return (s_type);
}

// Convert return status from getSock() to a character to be sent back to
// the caller.
char
getErrorCode(const bool bind_failed) {
    char error_code = ' ';
    if (bind_failed) {
            error_code = 'B';
    } else {
            error_code = 'S';
    }
    return (error_code);
}


// Handle the request from the client.
//
// Reads the type and family of socket required, creates the socket and returns
// it to the client.
//
// The arguments passed (and the exceptions thrown) are the same as those for
// run().
void
handleRequest(const socket_type input_sd, const socket_type output_sd,
              const get_sock_t get_sock, const send_sd_t send_sd_fun,
              const close_t close_fun)
{
    // Read the message from the client
    char type[2];
    readMessage(input_sd, type, sizeof(type));

    // Decide what type of socket is being asked for
    const int s_type = getSocketType(type[0], output_sd);

    // Read the address they ask for depending on what address family was
    // specified.
    sockaddr* addr = NULL;
    size_t addr_len = 0;
    sockaddr_in addr_in;
    sockaddr_in6 addr_in6;
    switch (type[1]) { // The address family

        // The casting to apparently incompatible types is required by the
        // C low-level interface.

        case '4':
            addr = convertSockAddr(&addr_in);
            addr_len = sizeof(addr_in);
            memset(&addr_in, 0, sizeof(addr_in));
            addr_in.sin_family = AF_INET;
            readMessage(input_sd, &addr_in.sin_port, sizeof(addr_in.sin_port));
            readMessage(input_sd, &addr_in.sin_addr.s_addr,
                        sizeof(addr_in.sin_addr.s_addr));
            break;

        case '6':
            addr = convertSockAddr(&addr_in6);
            addr_len = sizeof(addr_in6);
            memset(&addr_in6, 0, sizeof(addr_in6));
            addr_in6.sin6_family = AF_INET6;
            readMessage(input_sd, &addr_in6.sin6_port,
                        sizeof(addr_in6.sin6_port));
            readMessage(input_sd, &addr_in6.sin6_addr.s6_addr,
                        sizeof(addr_in6.sin6_addr.s6_addr));
            break;

        default:
            protocolError(output_sd);
    }

    // Obtain the socket
    bool bind_failed;
    const socket_type result = get_sock(s_type, addr, &bind_failed,
                                        addr_len, close_fun);
    if (result != invalid_socket) {
        // Got the socket, send it to the client.
        writeMessage(output_sd, "S", 1);
        if (send_sd_fun(output_sd, result) != 0) {
            // Error.  Close the socket (ignore any error from that operation)
            // and abort.
            close_fun(result);
            isc_throw(InternalError, "Error sending descriptor");
        }

        // Successfully sent the socket, so free up resources we still hold
        // for it.
        if (close_fun(result) == socket_error_retval) {
            isc_throw(InternalError, "Error closing socket");
        }
    } else {
        // Error.  Tell the client.
        char error_message[2];
        error_message[0] = 'E';
        error_message[1] = getErrorCode(bind_failed);
        writeMessage(output_sd, error_message, sizeof(error_message));

        // ...and append the reason code to the error message
        const int error_number = errno;
        writeMessage(output_sd, &error_number, sizeof(error_number));
    }
}

// Sets the MTU related flags for IPv6 UDP sockets.
// It is borrowed from bind-9 lib/isc/unix/socket.c and modified
// to compile here.
//
// The function returns false if it fails or true on success
bool
mtu(socket_type s) {
#ifdef IPV6_USE_MIN_MTU        /* RFC 3542, not too common yet*/
    const int on(1);
    // use minimum MTU
    if (setsockopt(s, IPPROTO_IPV6, IPV6_USE_MIN_MTU, &on, sizeof(on)) < 0) {
        return (false);
    }
#else // Try the following as fallback
#ifdef IPV6_MTU
    // Use minimum MTU on systems that don't have the IPV6_USE_MIN_MTU
    const int mtu = 1280;
    if (setsockopt(s, IPPROTO_IPV6, IPV6_MTU, &mtu, sizeof(mtu)) < 0) {
        return (false);
    }
#endif
#if defined(IPV6_MTU_DISCOVER) && defined(IPV6_PMTUDISC_DONT)
    // Turn off Path MTU discovery on IPv6/UDP sockets.
    const int action = IPV6_PMTUDISC_DONT;
    if (setsockopt(s, IPPROTO_IPV6, IPV6_MTU_DISCOVER, &action,
                   sizeof(action)) < 0) {

        return (false);
    }
#endif
#endif
    return (true);
}

// This one closes the socket if result is false. Used not to leak socket
// on error.
socket_type
maybeClose(const bool result,
           const socket_type socket,
           const close_t close_fun)
{
    if (!result) {
        if (close_fun(socket) == socket_error_retval) {
            isc_throw(InternalError, "Error closing socket");
        }
        return (invalid_socket);
    }
    return (socket);
}

} // Anonymous namespace

namespace isc {
namespace socket_creator {

// Get the socket and bind to it.
socket_type
getSock(const int type, struct sockaddr* bind_addr, bool* bind_failed,
        const socklen_t addr_len, const close_t close_fun)
{
    *bind_failed = false;
    const socket_type sock = socket(bind_addr->sa_family, type, 0);
    if (sock == invalid_socket) {
        return (invalid_socket);
    }
    const int on = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1) {
        // This is part of the binding process, so it's a bind error
	*bind_failed = true;
        return (maybeClose(false, sock, close_fun));
    }
    if (bind_addr->sa_family == AF_INET6 &&
        setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, &on, sizeof(on)) == -1) {
        // This is part of the binding process, so it's a bind error
	*bind_failed = true;
        return (maybeClose(false, sock, close_fun));
    }
    if (bind(sock, bind_addr, addr_len) == -1) {
        *bind_failed = true;
        return (maybeClose(false, sock, close_fun));
    }
    if (type == SOCK_DGRAM && bind_addr->sa_family == AF_INET6) {
        // Set some MTU flags on IPv6 UDP sockets.
        *bind_failed = true;
        return (maybeClose(mtu(sock), sock, close_fun));
    }
    return (sock);
}

// Main run loop.
void
run(const socket_type input_sd, const socket_type output_sd,
    get_sock_t get_sock, send_sd_t send_sd_fun, close_t close_fun)
{
    for (;;) {
        char command;
        readMessage(input_sd, &command, sizeof(command));
        switch (command) {
            case 'S':   // The "get socket" command
                handleRequest(input_sd, output_sd, get_sock,
                              send_sd_fun, close_fun);
                break;

            case 'T':   // The "terminate" command
                return;

            default:    // Don't recognise anything else
                protocolError(output_sd);
        }
    }
}

} // End of the namespaces
}
