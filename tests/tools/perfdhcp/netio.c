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
#include "netio.h"
#include "misc.h"
#include "cloptions.h"
#include "dkdebug.h"
#include "dhcp.h"
#define FLEXIBLE_ARRAY_MEMBER 500
#include "dhcp6.h"

static int socketSetup(int addr_fam, const char* local_addr, const char* port,
                       const char* type, struct sockaddr_storage* l_addr);
static struct addrinfo* getAddresses(int addr_fam, const char* hostname,
                                 const char* port);
static void printAddrInfo(FILE* f, const struct addrinfo* addr);
static char* addrAndPortToNames(const struct sockaddr_storage* addr, char* buf,
                                size_t bufsize);
static void printSockaddr6Info(FILE* f, const struct sockaddr_in6* sa);
static int getLinkLocalAddr(const char if_name[], struct sockaddr_storage* addr);

#define ADDR_NAME_BUFSIZE (NI_MAXHOST + 30)

static struct sockaddr_storage send_laddr;     // send socket local end details
static struct sockaddr_storage recv_laddr;     // recv socket local end details
static int send_fd = -1;     // Socket on which to send messages to DHCP server
static recv_fd = -1;      // Socket on which to receive messages to DHCP server
static struct addrinfo* server_addr = NULL;    // DHCP server address

/*
 * Set up network communications to talk to a DHCP server.
 * This sets up sockets and sets the file-scoped variables used by dhcpSend().
 *
 * File-scoped variables:
 * Sets: server_addr, send_fd, send_laddr, recv_fd and recv_laddr.
 */
void
dhcpSetup(const char* send_port, const char* recv_port, const char *server_port) {
    const char* local_addr = NULL;       // Local name to bind to
    const char* interface_name = NULL;   // Interface to get link-local address from
    int family = isV6() ? AF_INET6 : AF_INET;

    if (isV6()) {
        interface_name = getLocalName();     
        if (interface_name != NULL) {
            getLinkLocalAddr(interface_name, &send_laddr);
            memcpy(&recv_laddr, &send_laddr, sizeof(recv_laddr));
            // Do not set local_addr, so that socketSetup() will take
            // address etc. from send_laddr.
        } else {
            local_addr = "::";   // IPv6 anylocal
        }
    } else {
        local_addr = getLocalName();     
        if (local_addr == NULL) {
            local_addr = "0.0.0.0";
        }
    }
    send_fd = socketSetup(family, local_addr, send_port, "Send",
                          &send_laddr);
    if (recv_port == NULL) {
        recv_fd = send_fd;
        memcpy(&recv_laddr, &send_laddr, sizeof(recv_laddr));

    } else {
        recv_fd = socketSetup(family, local_addr, recv_port, "Recv",
                              &recv_laddr);
    }
    server_addr = getAddresses(family, getServer(), server_port);
}

/*
 * Shut down connections opened by dhcpSetup()
 */
void
netShutdown(void) {
    if (send_fd != -1) {
        close(send_fd);
    }
    if (recv_fd != -1 && recv_fd != send_fd) {
        close(recv_fd);
    }
    recv_fd = send_fd = -1;
    if (server_addr != NULL) {
        freeaddrinfo(server_addr);
        server_addr = NULL;
    }
}

/*
 * dhcpSend: Send a DHCP packet.
 *
 * File-scoped variables:
 * send_fd: Socket to send message on.
 * server_addr: Remote address to send message to.
 * send_laddr: Local address of socket, for informational messages only.
 */
int
dhcpSend(const unsigned char* msg, size_t num_octets) {
    ssize_t num_written;                // Octets written to socket
    char addrbuf[ADDR_NAME_BUFSIZE];    // For information messages
    char addrbuf2[ADDR_NAME_BUFSIZE];   // For information messages

    if (dk_set(DK_MSG)) {
        fprintf(stderr,
                "Sending %zu octets to socket fd %u, local %s remote %s\n",
                num_octets, send_fd,
                addrAndPortToNames((struct sockaddr_storage*)&send_laddr,
                                   addrbuf, sizeof(addrbuf)),
                addrAndPortToNames((struct sockaddr_storage*)server_addr,
				   addrbuf2, sizeof(addrbuf2)));
    }
    num_written = sendto(send_fd, msg, num_octets, 0, server_addr->ai_addr,
                         sizeof(struct sockaddr_storage));
    if (num_written < 0) {
        reporterr("Send failed: %s", strerror(errno));
        return (0);
    }
    if ((size_t) num_written != num_octets) {
        reporterr("Only %zd of %zu octets written", num_written, num_octets);
        return (0);
    }
    return (1);
}

/*
 * dhcpReceive: Receive a DHCP packet.
 *
 * File-scoped variables:
 * recv_fd is the socket to receive on.
 * recv_laddr is the socket's address, solely for informational messages.
 *
 */
int
dhcpReceive(void* msg, size_t msgsize) {
    ssize_t octets_received;             // Number of octets received
    struct sockaddr_storage source_addr; // Address of sender
    socklen_t addrsize;                 // size of above
    char addrbuf[ADDR_NAME_BUFSIZE];    // For informational messages

    if (dk_set(DK_SOCK)) {
        fprintf(stderr, "Waiting for response on socket fd %u, %s\n", recv_fd,
                 addrAndPortToNames(&recv_laddr, addrbuf, sizeof(addrbuf)));
    }
    addrsize = sizeof(source_addr);
    if (msgsize == 0) {
        msgsize = isV6() ? sizeof(struct dhcpv6_packet) :
                sizeof(struct dhcp_packet);
    }
    octets_received = recvfrom(recv_fd, msg, msgsize, 0,
                              (struct sockaddr*)&source_addr, &addrsize);
    if (dk_set(DK_MSG)) {
        fprintf(stderr, "Got %zd octets from fd %u, %s\n", octets_received,
                recv_fd,
                addrAndPortToNames(&source_addr, addrbuf, sizeof(addrbuf)));
    }
    if (octets_received < 0) {
        reporterr("Receive failed: %s", strerror(errno));
    } else if (octets_received == 0) {
        reporterr("Remote closed connection.");
    }
    return (octets_received);
}

/*
 * Create a socket for communication with DHCP server:
 * - Create socket
 * - Bind it to given local address and UDP port
 *
 * Input variables:
 * addr_fam is the address family to use, e.g. AF_INET
 * local_addr is the local address to bind to.
 * If it is null and the address family is AF_INET6, the local address, flow
 *     info, and scope are instead taken from l_addr.  This allows the flow
 *     info and scope to be specified.
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
socketSetup(int addr_fam, const char* local_addr, const char* port,
            const char* type, struct sockaddr_storage* l_addr) {
    char addrbuf[ADDR_NAME_BUFSIZE];    // Buffer for error messages
    int net_fd;                         // Socket
    struct addrinfo* addrs;             // Port and possibly local addr

    addrs = getAddresses(addr_fam, local_addr, port);
    if (addr_fam == AF_INET6 && local_addr == NULL) {
        if (dk_set(DK_SOCK)) {
            fprintf(stderr, "local address:\n");
            printSockaddr6Info(stderr, (struct sockaddr_in6*)l_addr);
        }

        // Get the local address, flow info, and scope from l_addr
        memcpy(&((struct sockaddr_in6*)addrs->ai_addr)->sin6_addr,
               &((struct sockaddr_in6*)l_addr)->sin6_addr,
               sizeof(struct in6_addr));
        ((struct sockaddr_in6*)addrs->ai_addr)->sin6_flowinfo =
            ((struct sockaddr_in6*)l_addr)->sin6_flowinfo;
        ((struct sockaddr_in6*)addrs->ai_addr)->sin6_scope_id =
            ((struct sockaddr_in6*)l_addr)->sin6_scope_id;
    }
    if (dk_set(DK_SOCK)) {
        fprintf(stderr, "Creating socket from addrinfo:\n");
        printAddrInfo(stderr, addrs);
    }
    net_fd = socket(addrs->ai_family, addrs->ai_socktype, addrs->ai_protocol);
    if (net_fd < 0) {
        reporterr("socket creation failed: %s", strerror(errno));
        exit(1);
    }
    if (bind(net_fd, addrs->ai_addr, addrs->ai_addrlen) == -1) {
        int s_errno = errno;
        reporterr("Could not bind to %s: %s", addrAndPortToNames((struct
                  sockaddr_storage*)addrs->ai_addr, addrbuf, sizeof(addrbuf)),
                  strerror(s_errno));
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
 * getAddresses: generate an addrinfo list from a given hostname and UDP port.
 * If getaddrinfo() fails for any reason with the provided information, an
 * error message is printed to stderr and the program exits with status 2.
 *
 * Input variables:
 * hostname: The host name to look up.  This can be a name, an IPv4 dotted-quad
 *     address, an IPv6 hexadecimal address, or null to not fill in the
 *     address.
 * port: The port to include in addrinfo.  This can be either a service name or
 *     an ASCII decimal number, or null to not fill in the port number.
 *
 * Return value:
 * A pointer to the addrinfo list.  This must be freed by the caller with
 * freeaddrinfo().
 */
static struct addrinfo*
getAddresses(int addr_fam, const char* hostname, const char* port) {
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
         * Be extra sure we always have an answer if we return.
         */
        reporterr("getaddrinfo: host %s port %s: %s",
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

/*
 * Find the link-local address for an interface.
 *
 * Input variables:
 * if_name is the name of the interface to search for.
 *
 * Output variables:
 * The link-local address for the interface is stored in addr.
 *
 * Return value:
 * 1 on success, 0 if no link-local address is found.
 *
 * If retrieval of the interface address list fails, an error message is
 * printed and the program is exited with status 2.
 */
static int
getLinkLocalAddr(const char if_name[], struct sockaddr_storage* addr) {
    struct ifaddrs* ifaddr;     // interface address list
    struct ifaddrs* ifa;        // For iterating through above

    if (getifaddrs(&ifaddr) == -1) {
        reporterr("Could not get interface addresses: %s", strerror(errno));
        exit(2);
    }

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr->sa_family == AF_INET6 && strcmp(ifa->ifa_name, if_name) == 0 &&
                (ntohs(((struct sockaddr_in6*)ifa->ifa_addr)->sin6_addr.__in6_u.__u6_addr16[0]) & 0xffc0) == 0xfe80) {
            break;
        }
    }
    if (ifa != NULL) {
        memcpy(addr, ifa->ifa_addr, sizeof(struct sockaddr_storage));
    }
    freeifaddrs(ifaddr);
    return (ifa != NULL);
}
