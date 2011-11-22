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

#ifndef PERFDHCP_H
#define PERFDHCP_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Set up network communications to talk to a DHCP server.
 * This sets up sockets and sets the variables used by dhcpSend().
 *
 * Input variables:
 * send_port is the local port from which to send communications.
 * recv_port is the local port on which to listen for communications.
 *     If null, the receive socket is made the same as the send socket.
 * server_port is the port on the server to communicate with.
 */
void dhcpSetup(const char* send_port, const char* recv_port,
               const char *server_port);

/*
 * Shut down connections opened by dhcpSetup()
 */
void netShutdown(void);

/*
 * dhcpSend: Send a DHCP packet.
 *
 * Input variables:
 * dhcp_packet: DHCP message to send.
 * num_octets: Size of message.
 *
 * Return value:
 * If the send succeeds, 1.
 * If it fails (in full or part), an error message is printed to stderr and 0
 * is returned.
 */
int dhcpSend(const unsigned char* msg, size_t num_octets);

/*
 * dhcpReceive: Receive a DHCP packet.
 *
 * Input variables:
 *
 * If msgsize is nonzero, it gives the size of the receive buffer.  If it is
 * zero, the receive buffer is taken to have the size of either a hdcpv6_packet
 * or a dhcp_packet, depending on whether v6 operation is in effect.
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
int dhcpReceive(void* msg, size_t msgsize);

#ifdef __cplusplus
}
#endif

#endif
