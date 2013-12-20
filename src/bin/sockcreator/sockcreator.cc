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

#include <util/io/fd.h>
#include <util/io/sockaddr_util.h>

#include <dhcp/protocol_util.h>

#include <cerrno>
#include <string.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <linux/filter.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>

using namespace isc::util::io;
using namespace isc::util::io::internal;
using namespace isc::socket_creator;
using namespace isc::dhcp;

namespace {

// Simple wrappers for read_data/write_data that throw an exception on error.
void
readMessage(const int fd, void* where, const size_t length) {
    if (read_data(fd, where, length) < length) {
        isc_throw(ReadError, "Error reading from socket creator client");
    }
}

void
writeMessage(const int fd, const void* what, const size_t length) {
    if (!write_data(fd, what, length)) {
        isc_throw(WriteError, "Error writing to socket creator client");
    }
}

// Exit on a protocol error after informing the client of the problem.
void
protocolError(const int fd, const char reason = 'I') {

    // Tell client we have a problem
    char message[2];
    message[0] = 'F';
    message[1] = reason;
    writeMessage(fd, message, sizeof(message));

    // ... and exit
    isc_throw(ProtocolError, "Fatal error, reason: " << reason);
}

// Return appropriate socket type constant for the socket type requested.
// The output_fd argument is required to report a protocol error.
int
getSocketType(const char type_code, const int output_fd) {
    int socket_type = 0;
    switch (type_code) {
        case 'T':
            socket_type = SOCK_STREAM;
            break;

        case 'U':
            socket_type = SOCK_DGRAM;
            break;

        default:
            protocolError(output_fd);   // Does not return
    }
    return (socket_type);
}

// Convert return status from getSock() to a character to be sent back to
// the caller.
char
getErrorCode(const int status) {
    char error_code = ' ';
    switch (status) {
        case -1:
            error_code = 'S';
            break;

        case -2:
            error_code = 'B';
            break;

        default:
            isc_throw(InternalError, "Error creating socket");
    }
    return (error_code);
}

// Handle the filter request from the client.
//
// Reads the filter port and iface required, creates the raw socket and
// returns it to the client.
//
// The arguments passed (and the exceptions thrown) are the same as
// those for run().
void
handleFilterSocketRequest(const int input_fd, const int output_fd,
                          const get_filter_sock_t get_filter_sock,
                          const send_fd_t send_fd_fun,
                          const close_t close_fun)
{
    unsigned short port;
    readMessage(input_fd, &port, sizeof(port));

    int iface;
    readMessage(input_fd, &iface, sizeof(iface));

    const int result = get_filter_sock(port, iface, close_fun);
    if (result >= 0) {
        // Got the socket, send it to the client.
        writeMessage(output_fd, "S", 1);
        if (send_fd_fun(output_fd, result) != 0) {
            // Error.  Close the socket (ignore any error from that operation)
            // and abort.
            close_fun(result);
            isc_throw(InternalError, "Error sending descriptor");
        }

        // Successfully sent the socket, so free up resources we still hold
        // for it.
        if (close_fun(result) == -1) {
            isc_throw(InternalError, "Error closing socket");
        }
    } else {
        // Error.  Tell the client.
        char error_message[2];
        error_message[0] = 'E';
        error_message[1] = getErrorCode(result);
        writeMessage(output_fd, error_message, sizeof(error_message));

        // ...and append the reason code to the error message
        const int error_number = errno;
        writeMessage(output_fd, &error_number, sizeof(error_number));
    }
}

// Handle the socket request from the client.
//
// Reads the type and family of socket required, creates the socket and returns
// it to the client.
//
// The arguments passed (and the exceptions thrown) are the same as those for
// run().
void
handleSocketRequest(const int input_fd, const int output_fd,
                    const get_sock_t get_sock, const send_fd_t send_fd_fun,
                    const close_t close_fun)
{
    // Read the message from the client
    char type[2];
    readMessage(input_fd, type, sizeof(type));

    // Decide what type of socket is being asked for
    const int sock_type = getSocketType(type[0], output_fd);

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
            readMessage(input_fd, &addr_in.sin_port, sizeof(addr_in.sin_port));
            readMessage(input_fd, &addr_in.sin_addr.s_addr,
                        sizeof(addr_in.sin_addr.s_addr));
            break;

        case '6':
            addr = convertSockAddr(&addr_in6);
            addr_len = sizeof(addr_in6);
            memset(&addr_in6, 0, sizeof(addr_in6));
            addr_in6.sin6_family = AF_INET6;
            readMessage(input_fd, &addr_in6.sin6_port,
                        sizeof(addr_in6.sin6_port));
            readMessage(input_fd, &addr_in6.sin6_addr.s6_addr,
                        sizeof(addr_in6.sin6_addr.s6_addr));
            break;

        default:
            protocolError(output_fd);
    }

    // Obtain the socket
    const int result = get_sock(sock_type, addr, addr_len, close_fun);
    if (result >= 0) {
        // Got the socket, send it to the client.
        writeMessage(output_fd, "S", 1);
        if (send_fd_fun(output_fd, result) != 0) {
            // Error.  Close the socket (ignore any error from that operation)
            // and abort.
            close_fun(result);
            isc_throw(InternalError, "Error sending descriptor");
        }

        // Successfully sent the socket, so free up resources we still hold
        // for it.
        if (close_fun(result) == -1) {
            isc_throw(InternalError, "Error closing socket");
        }
    } else {
        // Error.  Tell the client.
        char error_message[2];
        error_message[0] = 'E';
        error_message[1] = getErrorCode(result);
        writeMessage(output_fd, error_message, sizeof(error_message));

        // ...and append the reason code to the error message
        const int error_number = errno;
        writeMessage(output_fd, &error_number, sizeof(error_number));
    }
}

// Sets the MTU related flags for IPv6 UDP sockets.
// It is borrowed from bind-9 lib/isc/unix/socket.c and modified
// to compile here.
//
// The function returns -2 if it fails or the socket file descriptor
// on success (for convenience, so the result can be just returned).
int
mtu(int fd) {
#ifdef IPV6_USE_MIN_MTU        /* RFC 3542, not too common yet*/
    const int on(1);
    // use minimum MTU
    if (setsockopt(fd, IPPROTO_IPV6, IPV6_USE_MIN_MTU, &on, sizeof(on)) < 0) {
        return (-2);
    }
#else // Try the following as fallback
#ifdef IPV6_MTU
    // Use minimum MTU on systems that don't have the IPV6_USE_MIN_MTU
    const int mtu = 1280;
    if (setsockopt(fd, IPPROTO_IPV6, IPV6_MTU, &mtu, sizeof(mtu)) < 0) {
        return (-2);
    }
#endif
#if defined(IPV6_MTU_DISCOVER) && defined(IPV6_PMTUDISC_DONT)
    // Turn off Path MTU discovery on IPv6/UDP sockets.
    const int action = IPV6_PMTUDISC_DONT;
    if (setsockopt(fd, IPPROTO_IPV6, IPV6_MTU_DISCOVER, &action,
                   sizeof(action)) < 0) {

        return (-2);
    }
#endif
#endif
    return (fd);
}

// This one closes the socket if result is negative. Used not to leak socket
// on error.
int maybeClose(const int result, const int socket, const close_t close_fun) {
    if (result < 0) {
        if (close_fun(socket) == -1) {
            isc_throw(InternalError, "Error closing socket");
        }
    }
    return (result);
}

} // Anonymous namespace

namespace isc {
namespace socket_creator {

/// The following structure defines a Berkely Packet Filter program to perform
/// packet filtering. The program operates on Ethernet packets.  To help with
/// interpretation of the program, for the types of Ethernet packets we are
/// interested in, the header layout is:
///
///   6 bytes  Destination Ethernet Address
///   6 bytes  Source Ethernet Address
///   2 bytes  Ethernet packet type
///
///  20 bytes  Fixed part of IP header
///  variable  Variable part of IP header
///
///   2 bytes  UDP Source port
///   2 bytes  UDP destination port
///   4 bytes  Rest of UDP header
///
/// @todo We may want to extend the filter to receive packets sent
/// to the particular IP address assigned to the interface or
/// broadcast address.
struct sock_filter dhcp_sock_filter [] = {
    // Make sure this is an IP packet: check the half-word (two bytes)
    // at offset 12 in the packet (the Ethernet packet type).  If it
    // is, advance to the next instruction.  If not, advance 8
    // instructions (which takes execution to the last instruction in
    // the sequence: "drop it").
    BPF_STMT(BPF_LD + BPF_H + BPF_ABS, ETHERNET_PACKET_TYPE_OFFSET),
    BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, ETHERTYPE_IP, 0, 8),

    // Make sure it's a UDP packet.  The IP protocol is at offset
    // 9 in the IP header so, adding the Ethernet packet header size
    // of 14 bytes gives an absolute byte offset in the packet of 23.
    BPF_STMT(BPF_LD + BPF_B + BPF_ABS,
             ETHERNET_HEADER_LEN + IP_PROTO_TYPE_OFFSET),
    BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, IPPROTO_UDP, 0, 6),

    // Make sure this isn't a fragment by checking that the fragment
    // offset field in the IP header is zero.  This field is the
    // least-significant 13 bits in the bytes at offsets 6 and 7 in
    // the IP header, so the half-word at offset 20 (6 + size of
    // Ethernet header) is loaded and an appropriate mask applied.
    BPF_STMT(BPF_LD + BPF_H + BPF_ABS, ETHERNET_HEADER_LEN + IP_FLAGS_OFFSET),
    BPF_JUMP(BPF_JMP + BPF_JSET + BPF_K, 0x1fff, 4, 0),

    // Get the IP header length.  This is achieved by the following
    // (special) instruction that, given the offset of the start
    // of the IP header (offset 14) loads the IP header length.
    BPF_STMT(BPF_LDX + BPF_B + BPF_MSH, ETHERNET_HEADER_LEN),

    // Make sure it's to the right port.  The following instruction
    // adds the previously extracted IP header length to the given
    // offset to locate the correct byte.  The given offset of 16
    // comprises the length of the Ethernet header (14) plus the offset
    // of the UDP destination port (2) within the UDP header.
    BPF_STMT(BPF_LD + BPF_H + BPF_IND, ETHERNET_HEADER_LEN + UDP_DEST_PORT),
    // The following instruction tests against the default DHCP server port,
    // but the action port is actually set in PktFilterLPF::openSocket().
    // N.B. The code in that method assumes that this instruction is at
    // offset 8 in the program.  If this is changed, openSocket() must be
    // updated.
    BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, DHCP4_SERVER_PORT, 0, 1),

    // If we passed all the tests, ask for the whole packet.
    BPF_STMT(BPF_RET + BPF_K, (u_int)-1),

    // Otherwise, drop it.
    BPF_STMT(BPF_RET + BPF_K, 0),
};

// Get the socket and bind to it.
int
getFilterSock(const unsigned short port, const int iface,
              const close_t close_fun) {
    const int sock = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (sock == -1) {
        return (-1);
    }

    // Create socket filter program. This program will only allow incoming UDP
    // traffic which arrives on the specific (DHCP) port). It will also filter
    // out all fragmented packets.
    struct sock_fprog filter_program;
    memset(&filter_program, 0, sizeof(filter_program));

    filter_program.filter = dhcp_sock_filter;
    filter_program.len = sizeof(dhcp_sock_filter) / sizeof(struct sock_filter);
    // Override the default port value.
    dhcp_sock_filter[8].k = port;
    // Apply the filter.
    if (setsockopt(sock, SOL_SOCKET, SO_ATTACH_FILTER, &filter_program,
                   sizeof(filter_program)) < 0) {
        // This is part of the binding process, so it's a bind error
        return (maybeClose(-2, sock, close_fun));
    }

    struct sockaddr_ll sa;
    memset(&sa, 0, sizeof(sockaddr_ll));
    sa.sll_family = AF_PACKET;
    sa.sll_ifindex = iface;

    // For raw sockets we construct IP headers on our own, so we don't bind
    // socket to IP address but to the interface. We will later use the
    // Linux Packet Filtering to filter out these packets that we are
    // interested in.
    if (bind(sock, reinterpret_cast<const struct sockaddr*>(&sa),
             sizeof(sa)) < 0) {
        return (maybeClose(-2, sock, close_fun));
    }

    return (sock);
}

// Get the socket and bind to it.
int
getSock(const int type, struct sockaddr* bind_addr, const socklen_t addr_len,
        const close_t close_fun) {
    const int sock = socket(bind_addr->sa_family, type, 0);
    if (sock == -1) {
        return (-1);
    }
    const int on = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1) {
        // This is part of the binding process, so it's a bind error
        return (maybeClose(-2, sock, close_fun));
    }
    if (bind_addr->sa_family == AF_INET6 &&
        setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, &on, sizeof(on)) == -1) {
        // This is part of the binding process, so it's a bind error
        return (maybeClose(-2, sock, close_fun));
    }
    if (bind(sock, bind_addr, addr_len) == -1) {
        return (maybeClose(-2, sock, close_fun));
    }
    if (type == SOCK_DGRAM && bind_addr->sa_family == AF_INET6) {
        // Set some MTU flags on IPv6 UDP sockets.
        return (maybeClose(mtu(sock), sock, close_fun));
    }
    return (sock);
}

// Main run loop.
void
run(const int input_fd, const int output_fd,
    get_filter_sock_t get_filter_sock, get_sock_t get_sock,
    send_fd_t send_fd_fun, close_t close_fun)
{
    for (;;) {
        char command;
        readMessage(input_fd, &command, sizeof(command));
        switch (command) {
            case 'F':   // The "get filter socket" command
                handleFilterSocketRequest(input_fd, output_fd, get_filter_sock,
                                          send_fd_fun, close_fun);
                break;

            case 'S':   // The "get socket" command
                handleSocketRequest(input_fd, output_fd, get_sock,
                                    send_fd_fun, close_fun);
                break;

            case 'T':   // The "terminate" command
                return;

            default:    // Don't recognise anything else
                protocolError(output_fd);
        }
    }
}

} // End of the namespaces
}
