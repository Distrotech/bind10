/*
 * Copyright (C) 2011  Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <net/if_arp.h>
#include <ifaddrs.h>
#include <strings.h>
#include <string.h>
#include <stdarg.h>
#include <netdb.h>
#include <errno.h>
#include <time.h>
#include "perfdhcp.h"
#include "cloptions.h"
#include "dkdebug.h"
#include "dhcp.h"
#define FLEXIBLE_ARRAY_MEMBER 500
#include "dhcp6.h"

static int socketSetup(int addr_fam, const char* localAddr, const char* port,
                       const char* type, struct sockaddr_storage* l_addr);
static struct addrinfo* getAddrs(int addr_fam, const char* hostname,
                                 const char* port);
static void printAddrInfo(FILE* f, const struct addrinfo* addr);
static char* addrAndPortToNames(const struct sockaddr_storage* addr, char* buf,
                                size_t bufsize);
static void printSockaddr6Info(FILE* f, const struct sockaddr_in6* sa);

#define ADDR_NAME_BUFSIZE (NI_MAXHOST + 30)

static struct sockaddr_storage send_laddr;     // send socket local end details
static struct sockaddr_storage recv_laddr;     // recv socket local end details
static int send_fd = -1, recv_fd = -1;         // Sockets for DHCP server comm
static struct addrinfo* server_addr;           // DHCP server address

/*
 * Set up network communications to talk to a DHCP server.
 * This sets up sockets and sets the globals used by dhcpSend().
 *
 * Input variables:
 * sendPort is the local port from which to send communications.
 * recvPort is the local port on which to listen for communications.
 *     If null, the receive socket is made the same as the send socket.
 * serverPort is the port on the server to communicate with.
 *
 * Globals (see above):
 * Sets: server_addr, send_fd, send_laddr, recv_fd and recv_laddr.
 */
void
dhcpSetup(const char* sendPort, const char* recvPort, const char *serverPort) {
    const char* localAddr = getLocalName();
    int v6 = isV6();
    int family = v6 ? AF_INET6 : AF_INET;

    if (!isV6() && localAddr == NULL) {
        localAddr = "0.0.0.0";
    }
    send_fd = socketSetup(family, localAddr, sendPort, "Send",
                          &send_laddr);
    if (recvPort == NULL) {
        recv_fd = send_fd;
        memcpy(&recv_laddr, &send_laddr, sizeof(recv_laddr));

    } else {
        recv_fd = socketSetup(family, localAddr, recvPort, "Recv",
                              &recv_laddr);
    }
    server_addr = getAddrs(family, getServer(), serverPort);
}

/*
 * dhcpSend: Send a DHCP packet.
 *
 * Input variables:
 * dhcp_packet: DHCP message to send.
 * num_octets: Size of message.
 *
 * Globals:
 * send_fd: Socket to send message on.
 * server_addr: Remote address to send message to.
 * send_laddr: Local address of socket, for informational messages only.
 *
 * Return value:
 * If the send succeeds, 1.
 * If it fails (in full or part), an error message is printed to stderr and 0
 * is returned.
 */
int
dhcpSend(const unsigned char* msg, size_t num_octets) {
    ssize_t num_written;                // Octets written to socket
    char addrbuf[ADDR_NAME_BUFSIZE];    // For information messages
    char addrbuf2[ADDR_NAME_BUFSIZE];   // For information messages

    if (dk_set(DK_MSG)) {
        fprintf(stderr,
                "Sending %zu octets to socket fd %u, local %s remote %s",
                num_octets, send_fd,
                addrAndPortToNames((struct sockaddr_storage*)&send_laddr,
                                   addrbuf, sizeof(addrbuf)),
                addrAndPortToNames((struct sockaddr_storage*)server_addr,
				   addrbuf2, sizeof(addrbuf2)));
    }
    num_written = sendto(send_fd, msg, num_octets, 0, server_addr->ai_addr,
                         sizeof(struct sockaddr_storage));
    if (num_written < 0) {
        fprintf(stderr, "%s: Send failed: %s\n", PROGNAME, strerror(errno));
        return (0);
    }
    if ((size_t) num_written != num_octets) {
        fprintf(stderr, "%s: Only %zd of %zu octets written\n", PROGNAME,
                num_written, num_octets);
        return (0);
    }
    return (1);
}

/*
 * dhcpReceive: Receive a DHCP packet.
 *
 * Input variables:
 *
 * If msgsize is nonzero, it gives the size of the receive buffer.  If it is
 * zero, the reeive buffer is taken to have the size of either a hdcpv6_packet
 * or a dhcp_packet, depending on whether v6 operation is in effect.
 *
 * Globals:
 * recv_fd is the socket to receive on.
 * recv_laddr is the socket's address, solely for informational messages.
 *
 * If the receive fails, an error message is printed to stderr.
 *
 * Output variables:
 * msg points to storage for the received message.
 *
 * Return value:
 * The return value from recvfrom: -1 on error, 0 if remote closed, else the
 * number of octets received.
 */
int
dhcpReceive(void* msg, size_t msgsize) {
    ssize_t octetsReceived;             // Number of octets received
    struct sockaddr_storage sourceAddr; // Address of sender
    socklen_t addrSize;                 // size of above
    char addrbuf[ADDR_NAME_BUFSIZE];    // For informational messages

    if (dk_set(DK_SOCK)) {
        fprintf(stderr, "Waiting for response on socket fd %u, %s", recv_fd,
                 addrAndPortToNames(&recv_laddr, addrbuf, sizeof(addrbuf)));
    }
    addrSize = sizeof(sourceAddr);
    if (msgsize == 0)
        msgsize = isV6() ? sizeof(struct dhcpv6_packet) :
                sizeof(struct dhcp_packet);
    octetsReceived = recvfrom(recv_fd, msg, msgsize, 0,
                              (struct sockaddr*)&sourceAddr, &addrSize);
    if (dk_set(DK_MSG)) {
        fprintf(stderr, "Got %zd octets from fd %u, %s", octetsReceived,
                recv_fd,
                addrAndPortToNames(&sourceAddr, addrbuf, sizeof(addrbuf)));
    }
    if (octetsReceived < 0) {
        fprintf(stderr, "%s: Receive failed: %s\n", PROGNAME, strerror(errno));
    } else if (octetsReceived == 0) {
        fprintf(stderr, "%s: Remote closed connection.\n", PROGNAME);
    }
    return (octetsReceived);
}

/*
 * Create a socket for communication with DHCP server:
 * - Create socket
 * - Bind it to given local address and UDP port
 *
 * Input variables:
 * addr_fam is the address family to use, e.g. AF_INET
 * localAddr is the local address to bind to.
 * If it is null and the address family is AF_INET6, the local address, flow
 *     info, and scope are instead taken from l_addr.
 *
 * port is the port to bind to.
 *
 * type is a string giving the purpose of the socket, for verbose output.
 *
 * Output variables:
 * Socket details are stored in l_addr.
 *
 * Return value: The network fd.
 *
 * On error, a message is printed to stderr and the program exits with status
 * 1.
 */
static int
socketSetup(int addr_fam, const char* localAddr, const char* port,
            const char* type, struct sockaddr_storage* l_addr) {
    char addrbuf[ADDR_NAME_BUFSIZE];    // Buffer for error messages
    int net_fd;                         // Socket
    struct addrinfo* addrs;             // Port and possibly local addr

    addrs = getAddrs(addr_fam, localAddr, port);
    if (addr_fam == AF_INET6 && localAddr == NULL) {
        if (dk_set(DK_SOCK)) {
            fprintf(stderr, "local address:\n");
            printSockaddr6Info(stderr, (struct sockaddr_in6*)l_addr);
        }
        memcpy(&((struct sockaddr_in6*)addrs->ai_addr)->sin6_addr,
               &((struct sockaddr_in6*)l_addr)->sin6_addr,
               sizeof(struct in6_addr));
        ((struct sockaddr_in6*)addrs->ai_addr)->sin6_flowinfo =
            ((struct sockaddr_in6*)l_addr)->sin6_flowinfo;
        ((struct sockaddr_in6*)addrs->ai_addr)->sin6_scope_id =
            ((struct sockaddr_in6*)l_addr)->sin6_scope_id;
    }
    if (dk_set(DK_SOCK)) {
        printAddrInfo(stderr, addrs);
        fprintf(stderr, "Creating socket from addrinfo:\n");
        printAddrInfo(stderr, addrs);
    }
    net_fd = socket(addrs->ai_family, addrs->ai_socktype, addrs->ai_protocol);
    if (net_fd < 0) {
        fprintf(stderr, "%s: socket creation failed: %s\n", PROGNAME,
                strerror(errno));
        exit(1);
    }
    if (bind(net_fd, addrs->ai_addr, addrs->ai_addrlen) == -1) {
        int s_errno = errno;
        fprintf(stderr, "%s: Could not bind to %s: %s\n", PROGNAME,
                addrAndPortToNames((struct sockaddr_storage*)addrs->ai_addr,
                addrbuf, sizeof(addrbuf)), strerror(s_errno));
        exit(1);
    }
    if (dk_set(DK_SOCK)) {
        fprintf(stderr, "%s fd %d bound to %s\n", type, net_fd,
                 addrAndPortToNames((struct sockaddr_storage*)addrs->ai_addr,
                                    addrbuf, sizeof(addrbuf)));
    }
    memcpy(l_addr, addrs->ai_addr, sizeof(struct sockaddr_storage));
    freeaddrinfo(addrs);
    return (net_fd);
}

/*
 * getAddrs: generate an addrinfo list from a given hostname and UDP port.
 * If getaddrinfo() fails for any reason with the provided information, an
 * error message is printed to stderr and the program exits with status 2.
 *
 * Input variables:
 * hostname: The host name to look up.  This can be either a name or an IPv4
 *     dotted-quad address, or null to not fill in the address.
 * port: The port to include in addrinfo.  This can be either a service name or
 *     an ASCII decimal number, or null to not fill in the port number.
 *
 * Globals:
 * PROGNAME, for error messages.
 *
 * Return value:
 * A pointer to the addrinfo list.  This must be freed by the caller with
 * freeaddrinfo().
 */
static struct addrinfo*
getAddrs(int addr_fam, const char* hostname, const char* port) {
    struct addrinfo* ai;        // list produced by getaddrinfo
    struct addrinfo hints;      // hints for getaddrinfo
    int ret;                    // return value from getaddrinfo

    memset(&hints, '\0', sizeof(hints));
    hints.ai_family = addr_fam;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

    ret = getaddrinfo(hostname, port, &hints, &ai);
    if (ret != 0 || ai == NULL) {
        /*
         * getaddrinfo as tested returns error if lookup fails,
         * but the man page doesn't quite promise that.
         * But be extra sure we always have an answer if we return.
         */
        fprintf(stderr, "%s: getaddrinfo: %s/%s: %s\n", PROGNAME,
                hostname == NULL ? "" : hostname, port == NULL ? "" : port,
                ret != 0 ? gai_strerror(ret) : "hostname/port lookup failure");
        exit(2);
    }
    return (ai);
}

/*
 * Print details of a socket address
 *
 * Input variables:
 *
 * stream is the stdio stream to print to.
 *
 * addr contains the information to display.
 */
static void
printAddrInfo(FILE* stream, const struct addrinfo* addr) {
    fprintf(stream, "Addrinfo:\n");
    fprintf(stream, "flags: 0x%x;  family: %d;  socktype: %d;  proto: %d;\n",
            addr->ai_flags, addr->ai_family, addr->ai_socktype,
            addr->ai_protocol);
    fprintf(stream, "addrlen: %u;  addr: %p;  canonname: %s;  next: %p\n",
            addr->ai_addrlen, addr->ai_addr, addr->ai_canonname,
            addr->ai_next);
    if (addr->ai_family == AF_INET6) {
        printSockaddr6Info(stream, (struct sockaddr_in6*)addr->ai_addr);
    }
}

/*
 * addrAndPortToNames(): Convert the address and port associated with a socket
 * into a hostname and port name and store them in a buffer.
 *
 * Input variables:
 * addr is the socket to operate on.
 * bufsize is the size of the buffer.
 *
 * Output variables:
 * buf is the buffer to store the names in.
 *
 * Return value:
 * buf is returned.
 * If getnameinfo fails, buf will contain "untranslatable".  In general, this
 * shouldn't happen; in the case of lookup failure for a component, its numeric
 * value will be used.
 *
 * TODO: Use gai_strerror() to report errors.
 */
static char*
addrAndPortToNames(const struct sockaddr_storage* addr, char* buf,
                   size_t bufsize) {
    char servbuf[30];   // Port name

    if (getnameinfo((struct sockaddr*)addr, sizeof(struct sockaddr_storage),
                    buf, bufsize, servbuf, 30, 0) != 0) {
        strncpy(buf, "untranslatable", bufsize-1);
    } else {
        size_t len = strlen(buf);
        if (len < bufsize) {
            snprintf(buf + len, bufsize - len, " port %s", servbuf);
        }
    }
    return (buf);
}

/*
 * Print information about an IPv6 socket.
 */
static void
printSockaddr6Info(FILE* f, const struct sockaddr_in6* sa) {
    char addrbuf[ADDR_NAME_BUFSIZE];    // For host & port name

    fprintf(f, "IPv6 sockaddr info:\n");
    fprintf(f, "family: %u;  flowinfo: 0x%x;  scope-id: %u  addr: %s\n",
            sa->sin6_family, sa->sin6_flowinfo, sa->sin6_scope_id,
            addrAndPortToNames((struct sockaddr_storage*)sa, addrbuf,
                               sizeof(addrbuf)));
}
