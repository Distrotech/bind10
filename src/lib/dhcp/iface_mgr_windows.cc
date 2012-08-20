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

#define B10_LIBDHCP_EXPORT

#include <config.h>

#ifdef _WIN32

#include <dhcp/iface_mgr.h>
#include <exceptions/exceptions.h>

using namespace std;
using namespace isc;
using namespace isc::asiolink;
using namespace isc::dhcp;

namespace isc {
namespace dhcp {

void
IfaceMgr::detectIfaces() {
    /// @todo do the actual detection on Windows. Currently just calling
    /// stub implementation.
    stubDetectIfaces();
}

void IfaceMgr::os_send4(WSAMSG& m,
                        boost::scoped_array<char>& control_buf,
                        size_t control_buf_len,
                        const Pkt4Ptr& pkt) {
    // Setting the interface is a bit more involved.
    //
    // We have to create a "control message", and set that to
    // define the IPv4 packet information. We could set the
    // source address if we wanted, but we can safely let the
    // kernel decide what that should be.
    m.Control.buf = &control_buf[0];
    m.Control.len = control_buf_len;
    WSACMSGHDR* cmsg = WSA_CMSG_FIRSTHDR(&m);
    cmsg->cmsg_level = IPPROTO_IP;
    cmsg->cmsg_type = IP_PKTINFO;
    cmsg->cmsg_len = WSA_CMSG_LEN(sizeof(struct in_pktinfo));
    struct in_pktinfo* pktinfo =(struct in_pktinfo *)WSA_CMSG_DATA(cmsg);
    memset(pktinfo, 0, sizeof(struct in_pktinfo));
    pktinfo->ipi_ifindex = pkt->getIndex();
    m.Control.len = cmsg->cmsg_len;
}

bool IfaceMgr::os_receive4(WSAMSG& m, Pkt4Ptr& pkt) {
    WSACMSGHDR* cmsg;
    struct in_pktinfo* pktinfo;
    struct in_addr to_addr;

    memset(&to_addr, 0, sizeof(to_addr));

    cmsg = WSA_CMSG_FIRSTHDR(&m);
    while (cmsg != NULL) {
        if ((cmsg->cmsg_level == IPPROTO_IP) &&
            (cmsg->cmsg_type == IP_PKTINFO)) {
            pktinfo = (struct in_pktinfo*)WSA_CMSG_DATA(cmsg);

            pkt->setIndex(pktinfo->ipi_ifindex);
            pkt->setLocalAddr(IOAddress(htonl(pktinfo->ipi_addr.s_addr)));
            return (true);

            // This field is useful, when we are bound to unicast
            // address e.g. 192.0.2.1 and the packet was sent to
            // broadcast. This will return broadcast address, not
            // the address we are bound to.

            // IOAddress tmp(htonl(pktinfo->ipi_spec_dst.s_addr));
            // cout << "The other addr is: " << tmp.toText() << endl;

            // Perhaps we should uncomment this:
            // to_addr = pktinfo->ipi_spec_dst;
        }
        cmsg = WSA_CMSG_NXTHDR(&m, cmsg);
    }

    return (false);
}

} // end of isc::dhcp namespace
} // end of dhcp namespace

#endif
