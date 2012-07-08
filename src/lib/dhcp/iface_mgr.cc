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

#define ISC_LIBDHCP_EXPORT

#include <config.h>
#include <sstream>
#include <fstream>
#include <string.h>
#ifdef _WIN32
#include <ws2tcpip.h>
#include <iphlpapi.h>
#else
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#endif

#include <dhcp/dhcp4.h>
#include <dhcp/dhcp6.h>
#include <dhcp/iface_mgr.h>
#include <exceptions/exceptions.h>
#include <util/io/pktinfo_utilities.h>

using namespace std;
using namespace isc::asiolink;
using namespace isc::util::io::internal;

namespace isc {
namespace dhcp {

/// IfaceMgr is a singleton implementation
IfaceMgr* IfaceMgr::instance_ = 0;

void
IfaceMgr::instanceCreate() {
    if (instance_) {
        // no need to do anything. Instance is already created.
        // Who called it again anyway? Uh oh. Had to be us, as
        // this is private method.
        return;
    }
    instance_ = new IfaceMgr();
}

IfaceMgr&
IfaceMgr::instance() {
    if (instance_ == 0) {
        instanceCreate();
    }
    return (*instance_);
}

IfaceMgr::Iface::Iface(const std::string& name, int ifindex)
    :name_(name), ifindex_(ifindex), mac_len_(0), hardware_type_(0),
     flag_loopback_(false), flag_up_(false), flag_running_(false),
     flag_multicast_(false), flag_broadcast_(false), flags_(0)
{
    memset(mac_, 0, sizeof(mac_));
}

std::string
IfaceMgr::Iface::getFullName() const {
    ostringstream tmp;
    tmp << name_ << "/" << ifindex_;
    return (tmp.str());
}

std::string
IfaceMgr::Iface::getPlainMac() const {
    ostringstream tmp;
    tmp.fill('0');
    tmp << hex;
    for (size_t i = 0; i < mac_len_; i++) {
        tmp.width(2);
        tmp <<  static_cast<int>(mac_[i]);
        if (i < mac_len_-1) {
            tmp << ":";
        }
    }
    return (tmp.str());
}

void IfaceMgr::Iface::setMac(const uint8_t* mac, size_t len) {
    if (len > IfaceMgr::MAX_MAC_LEN) {
        isc_throw(OutOfRange, "Interface " << getFullName()
                  << " was detected to have link address of length "
                  << len << ", but maximum supported length is "
                  << IfaceMgr::MAX_MAC_LEN);
    }
    mac_len_ = len;
    memcpy(mac_, mac, len);
}

bool IfaceMgr::Iface::delAddress(const isc::asiolink::IOAddress& addr) {
    for (AddressCollection::iterator a = addrs_.begin();
         a!=addrs_.end(); ++a) {
        if (*a==addr) {
            addrs_.erase(a);
            return (true);
        }
    }
    return (false);
}

#ifdef _WIN32
bool IfaceMgr::Iface::delSocket(SOCKET sockfd)
#else
bool IfaceMgr::Iface::delSocket(int sockfd)
#endif
{
    list<SocketInfo>::iterator sock = sockets_.begin();
    while (sock!=sockets_.end()) {
        if (sock->sockfd_ == sockfd) {
#ifdef _WIN32
            closesocket(sockfd);
#else
            close(sockfd);
#endif
            sockets_.erase(sock);
            return (true); //socket found
        }
        ++sock;
    }
    return (false); // socket not found
}

IfaceMgr::IfaceMgr()
#ifdef _WIN32
    :control_buf_len_(WSA_CMSG_SPACE(sizeof(struct in6_pktinfo))),
#else
    :control_buf_len_(CMSG_SPACE(sizeof(struct in6_pktinfo))),
#endif
     control_buf_(new char[control_buf_len_]),
     session_socket_(INVALID_SOCKET), session_callback_(NULL)
{

    cout << "IfaceMgr initialization." << endl;

    try {
        // required for sending/receiving packets
        // let's keep it in front, just in case someone
        // wants to send anything during initialization

        // control_buf_ = boost::scoped_array<char>();

        detectIfaces();

    } catch (const std::exception& ex) {
        cout << "IfaceMgr creation failed:" << ex.what() << endl;

        // TODO Uncomment this (or call LOG_FATAL) once
        // interface detection is implemented. Otherwise
        // it is not possible to run tests in a portable
        // way (see detectIfaces() method).
        throw;
    }
}

void IfaceMgr::closeSockets() {
    for (IfaceCollection::iterator iface = ifaces_.begin();
         iface != ifaces_.end(); ++iface) {

        for (SocketCollection::iterator sock = iface->sockets_.begin();
             sock != iface->sockets_.end(); ++sock) {
            cout << "Closing socket " << sock->sockfd_ << endl;
#ifdef _WIN32
            closesocket(sock->sockfd_);
#else
            close(sock->sockfd_);
#endif
        }
        iface->sockets_.clear();
    }

}

IfaceMgr::~IfaceMgr() {
    // control_buf_ is deleted automatically (scoped_ptr)
    control_buf_len_ = 0;

    closeSockets();
}

void IfaceMgr::stubDetectIfaces() {
    string ifaceName;
    const string v4addr("127.0.0.1"), v6addr("::1");

    // This is a stub implementation for interface detection. Actual detection
    // is faked by detecting loopback interface (lo or lo0). It will eventually
    // be removed once we have actual implementations for all supported systems.

    cout << "Interface detection is not implemented on this Operating System yet. "
         << endl;

    try {
        if (if_nametoindex("lo") > 0) {
            ifaceName = "lo";
            // this is Linux-like OS
        } else if (if_nametoindex("lo0") > 0) {
            ifaceName = "lo0";
            // this is BSD-like OS
        } else {
            // we give up. What OS is this, anyway? Solaris? Hurd?
            isc_throw(NotImplemented,
                      "Interface detection on this OS is not supported.");
        }

        Iface iface(ifaceName, if_nametoindex(ifaceName.c_str()));
        iface.flag_up_ = true;
        iface.flag_running_ = true;

        // note that we claim that this is not a loopback. iface_mgr tries to open a
        // socket on all interaces that are up, running and not loopback. As this is
        // the only interface we were able to detect, let's pretend this is a normal
        // interface.
        iface.flag_loopback_ = false;
        iface.flag_multicast_ = true;
        iface.flag_broadcast_ = true;
        iface.setHWType(HWTYPE_ETHERNET);

        iface.addAddress(IOAddress(v4addr));
        iface.addAddress(IOAddress(v6addr));
        addInterface(iface);

        cout << "Detected interface " << ifaceName << "/" << v4addr << "/"
             << v6addr << endl;
    } catch (const std::exception&) {
        // TODO: deallocate whatever memory we used
        // not that important, since this function is going to be
        // thrown away as soon as we get proper interface detection
        // implemented

        // TODO Do LOG_FATAL here
        std::cerr << "Interface detection failed." << std::endl;
        throw;
    }
}

bool IfaceMgr::openSockets4(const uint16_t port) {
#ifdef _WIN32
    SOCKET sock;
#else
    int sock;
#endif
    int count = 0;

    for (IfaceCollection::iterator iface=ifaces_.begin();
         iface!=ifaces_.end();
         ++iface) {

        cout << "Trying opening socket on interface " << iface->getFullName() << endl;

        if (iface->flag_loopback_ ||
            !iface->flag_up_ ||
            !iface->flag_running_) {
            cout << "Interface " << iface->getFullName()
                 << " not suitable: is loopback, is down or not running" << endl;
            continue;
        }

        AddressCollection addrs = iface->getAddresses();

        for (AddressCollection::iterator addr= addrs.begin();
             addr != addrs.end();
             ++addr) {

            // skip IPv6 addresses
            if (addr->getFamily() != AF_INET) {
                continue;
            }

            sock = openSocket(iface->getName(), *addr, port);
            if (sock == INVALID_SOCKET) {
                cout << "Failed to open unicast socket." << endl;
                return (false);
            }

            count++;
        }
    }
    return (count > 0);

}

bool IfaceMgr::openSockets6(const uint16_t port) {
#ifdef _WIN32
    SOCKET sock;
#else
    int sock;
#endif
    int count = 0;

    for (IfaceCollection::iterator iface=ifaces_.begin();
         iface!=ifaces_.end();
         ++iface) {

        if (iface->flag_loopback_ ||
            !iface->flag_up_ ||
            !iface->flag_running_) {
            continue;
        }

        AddressCollection addrs = iface->getAddresses();

        for (AddressCollection::iterator addr= addrs.begin();
             addr != addrs.end();
             ++addr) {

            // skip IPv4 addresses
            if (addr->getFamily() != AF_INET6) {
                continue;
            }

            sock = openSocket(iface->getName(), *addr, port);
            if (sock == INVALID_SOCKET) {
                cout << "Failed to open unicast socket." << endl;
                return (false);
            }

            // Binding socket to unicast address and then joining multicast group
            // works well on Mac OS (and possibly other BSDs), but does not work
            // on Linux.
            if ( !joinMulticast(sock, iface->getName(),
                                string(ALL_DHCP_RELAY_AGENTS_AND_SERVERS) ) ) {
#ifdef _WIN32
                closesocket(sock);
#else
                close(sock);
#endif
                isc_throw(Unexpected, "Failed to join " << ALL_DHCP_RELAY_AGENTS_AND_SERVERS
                          << " multicast group.");
            }

            count++;

            /// @todo: Remove this ifdef once we start supporting BSD systems.
#if defined(OS_LINUX)
            // To receive multicast traffic, Linux requires binding socket to
            // a multicast group. That in turn doesn't work on NetBSD.

            int sock2 = openSocket(iface->getName(),
                                   IOAddress(ALL_DHCP_RELAY_AGENTS_AND_SERVERS),
                                   port);
            if (sock2<0) {
                isc_throw(Unexpected, "Failed to open multicast socket on "
                          << " interface " << iface->getFullName());
                iface->delSocket(sock); // delete previously opened socket
            }
#endif
        }
    }
    return (count > 0);
}

void
IfaceMgr::printIfaces(std::ostream& out /*= std::cout*/) {
    for (IfaceCollection::const_iterator iface=ifaces_.begin();
         iface!=ifaces_.end();
         ++iface) {

        const AddressCollection& addrs = iface->getAddresses();

        out << "Detected interface " << iface->getFullName()
            << ", hwtype=" << iface->getHWType()
            << ", mac=" << iface->getPlainMac();
        out << ", flags=" << hex << iface->flags_ << dec << "("
            << (iface->flag_loopback_?"LOOPBACK ":"")
            << (iface->flag_up_?"UP ":"")
            << (iface->flag_running_?"RUNNING ":"")
            << (iface->flag_multicast_?"MULTICAST ":"")
            << (iface->flag_broadcast_?"BROADCAST ":"")
            << ")" << endl;
        out << "  " << addrs.size() << " addr(s):";

        for (AddressCollection::const_iterator addr = addrs.begin();
             addr != addrs.end(); ++addr) {
            out << "  " << addr->toText();
        }
        out << endl;
    }
}

IfaceMgr::Iface*
IfaceMgr::getIface(int ifindex) {
    for (IfaceCollection::iterator iface=ifaces_.begin();
         iface!=ifaces_.end();
         ++iface) {
        if (iface->getIndex() == ifindex)
            return (&(*iface));
    }

    return (NULL); // not found
}

IfaceMgr::Iface*
IfaceMgr::getIface(const std::string& ifname) {
    for (IfaceCollection::iterator iface=ifaces_.begin();
         iface!=ifaces_.end();
         ++iface) {
        if (iface->getName() == ifname)
            return (&(*iface));
    }

    return (NULL); // not found
}

#ifdef _WIN32
SOCKET
#else
int
#endif
IfaceMgr::openSocket(const std::string& ifname, const IOAddress& addr,
                     const uint16_t port) {
    Iface* iface = getIface(ifname);
    if (!iface) {
        isc_throw(BadValue, "There is no " << ifname << " interface present.");
    }
    switch (addr.getFamily()) {
    case AF_INET:
        return openSocket4(*iface, addr, port);
    case AF_INET6:
        return openSocket6(*iface, addr, port);
    default:
        isc_throw(BadValue, "Failed to detect family of address: "
                  << addr.toText());
    }
}

#ifdef _WIN32
SOCKET
#else
int
#endif
IfaceMgr::openSocket4(Iface& iface, const IOAddress& addr, uint16_t port) {

    cout << "Creating UDP4 socket on " << iface.getFullName()
         << " " << addr.toText() << "/port=" << port << endl;

    struct sockaddr_in addr4;
    memset(&addr4, 0, sizeof(sockaddr));
    addr4.sin_family = AF_INET;
    addr4.sin_port = htons(port);

    addr4.sin_addr.s_addr = htonl(addr);
    //addr4.sin_addr.s_addr = 0; // anyaddr: this will receive 0.0.0.0 => 255.255.255.255 traffic
    // addr4.sin_addr.s_addr = 0xffffffffu; // broadcast address. This will receive 0.0.0.0 => 255.255.255.255 as well

#ifdef _WIN32
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);
#else
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
#endif
    if (sock == INVALID_SOCKET) {
        isc_throw(Unexpected, "Failed to create UDP6 socket.");
    }

#ifdef _WIN32
    if (::bind(sock, (struct sockaddr *)&addr4, sizeof(addr4)) < 0) {
        closesocket(sock);
        isc_throw(Unexpected, "Failed to bind socket " << sock << " to " << addr.toText()
                  << "/port=" << port);
    }
#else
    if (bind(sock, (struct sockaddr *)&addr4, sizeof(addr4)) < 0) {
        close(sock);
        isc_throw(Unexpected, "Failed to bind socket " << sock << " to " << addr.toText()
                  << "/port=" << port);
    }
#endif

    // if there is no support for IP_PKTINFO, we are really out of luck
    // it will be difficult to undersand, where this packet came from
#if defined(IP_PKTINFO)
    int flag = 1;
    if (setsockopt(sock, IPPROTO_IP, IP_PKTINFO,
                   (char *) &flag, sizeof(flag)) != 0) {
#ifdef _WIN32
        closesocket(sock);
#else
        close(sock);
#endif
        isc_throw(Unexpected, "setsockopt: IP_PKTINFO: failed.");
    }
#endif

    cout << "Created socket " << sock << " on " << iface.getName() << "/" <<
        addr.toText() << "/port=" << port << endl;

    SocketInfo info(sock, addr, port);
    iface.addSocket(info);

    return (sock);
}

#ifdef _WIN32
SOCKET
#else
int
#endif
IfaceMgr::openSocket6(Iface& iface, const IOAddress& addr, uint16_t port) {

    cout << "Creating UDP6 socket on " << iface.getFullName()
         << " " << addr.toText() << "/port=" << port << endl;

    struct sockaddr_in6 addr6;
    memset(&addr6, 0, sizeof(addr6));
    addr6.sin6_family = AF_INET6;
    addr6.sin6_port = htons(port);
    if (addr.toText() != "::1")
      addr6.sin6_scope_id = if_nametoindex(iface.getName().c_str());

    memcpy(&addr6.sin6_addr,
           addr.getAddress().to_v6().to_bytes().data(),
           sizeof(addr6.sin6_addr));
#ifdef HAVE_SA_LEN
    addr6.sin6_len = sizeof(addr6);
#endif

    // TODO: use sockcreator once it becomes available

    // make a socket
#ifdef _WIN32
    SOCKET sock = socket(AF_INET6, SOCK_DGRAM, 0);
#else
    int sock = socket(AF_INET6, SOCK_DGRAM, 0);
#endif
    if (sock == INVALID_SOCKET) {
        isc_throw(Unexpected, "Failed to create UDP6 socket.");
    }

    // Set the REUSEADDR option so that we don't fail to start if
    // we're being restarted.
    int flag = 1;
#ifndef _WIN32
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
                   (char *)&flag, sizeof(flag)) < 0) {
        close(sock);
        isc_throw(Unexpected, "Can't set SO_REUSEADDR option on dhcpv6 socket.");
    }
#endif

#ifdef _WIN32
    if (::bind(sock, (struct sockaddr *)&addr6, sizeof(addr6)) < 0) {
        closesocket(sock);
        isc_throw(Unexpected, "Failed to bind socket " << sock << " to " << addr.toText()
                  << "/port=" << port);
    }
#else
    if (bind(sock, (struct sockaddr *)&addr6, sizeof(addr6)) < 0) {
        close(sock);
        isc_throw(Unexpected, "Failed to bind socket " << sock << " to " << addr.toText()
                  << "/port=" << port);
    }
#endif

#ifdef IPV6_RECVPKTINFO
    // RFC3542 - a new way
    if (setsockopt(sock, IPPROTO_IPV6, IPV6_RECVPKTINFO,
                   (char *) &flag, sizeof(flag)) != 0) {
#ifdef _WIN32
        closesocket(sock);
#else
        close(sock);
#endif
        isc_throw(Unexpected, "setsockopt: IPV6_RECVPKTINFO failed.");
    }
#else
    // RFC2292 - an old way
    if (setsockopt(sock, IPPROTO_IPV6, IPV6_PKTINFO,
                   (char *) &flag, sizeof(flag)) != 0) {
#ifdef _WIN32
        closesocket(sock);
#else
        close(sock);
#endif
        isc_throw(Unexpected, "setsockopt: IPV6_PKTINFO: failed.");
    }
#endif

    // multicast stuff
    if (addr.getAddress().to_v6().is_multicast()) {
        // both mcast (ALL_DHCP_RELAY_AGENTS_AND_SERVERS and ALL_DHCP_SERVERS)
        // are link and site-scoped, so there is no sense to join those groups
        // with global addresses.

        if ( !joinMulticast( sock, iface.getName(),
                         string(ALL_DHCP_RELAY_AGENTS_AND_SERVERS) ) ) {
#ifdef _WIN32
            closesocket(sock);
#else
            close(sock);
#endif
            isc_throw(Unexpected, "Failed to join " << ALL_DHCP_RELAY_AGENTS_AND_SERVERS
                      << " multicast group.");
        }
    }

    cout << "Created socket " << sock << " on " << iface.getName() << "/" <<
        addr.toText() << "/port=" << port << endl;

    SocketInfo info(sock, addr, port);
    iface.addSocket(info);

    return (sock);
}

bool
IfaceMgr::joinMulticast(
#ifdef _WIN32
                        SOCKET sock,
#else
                        int sock,
#endif
                        const std::string& ifname, const std::string & mcast) {

    struct ipv6_mreq mreq;

    if (inet_pton(AF_INET6, mcast.c_str(),
                  &mreq.ipv6mr_multiaddr) <= 0) {
        cout << "Failed to convert " << ifname
             << " to IPv6 multicast address." << endl;
        return (false);
    }

    mreq.ipv6mr_interface = if_nametoindex(ifname.c_str());
    if (setsockopt(sock, IPPROTO_IPV6, IPV6_JOIN_GROUP,
                   (char *)&mreq, sizeof(mreq)) < 0) {
        cout << "Failed to join " << mcast << " multicast group." << endl;
        return (false);
    }

    cout << "Joined multicast " << mcast << " group." << endl;

    return (true);
}

bool
IfaceMgr::send(const Pkt6Ptr& pkt) {
    int result;

    Iface* iface = getIface(pkt->getIface());
    if (!iface) {
        isc_throw(BadValue, "Unable to send Pkt6. Invalid interface ("
                  << pkt->getIface() << ") specified.");
    }

    memset(&control_buf_[0], 0, control_buf_len_);


    // Set the target address we're sending to.
    sockaddr_in6 to;
    memset(&to, 0, sizeof(to));
    to.sin6_family = AF_INET6;
    to.sin6_port = htons(pkt->getRemotePort());
    memcpy(&to.sin6_addr,
           pkt->getRemoteAddr().getAddress().to_v6().to_bytes().data(),
           16);
    to.sin6_scope_id = pkt->getIndex();

    // Initialize our message header structure.
#ifdef _WIN32
    WSAMSG m;
#else
    struct msghdr m;
#endif
    memset(&m, 0, sizeof(m));
#ifdef _WIN32
    m.name = (struct sockaddr *)&to;
    m.namelen = sizeof(to);
#else
    m.msg_name = (struct sockaddr *)&to;
    m.msg_namelen = sizeof(to);
#endif

    // Set the data buffer we're sending. (Using this wacky
    // "scatter-gather" stuff... we only have a single chunk
    // of data to send, so we declare a single vector entry.)

    // As v structure is a C-style is used for both sending and
    // receiving data, it is shared between sending and receiving
    // (sendmsg and recvmsg). It is also defined in system headers,
    // so we have no control over its definition. To set iov_base
    // (defined as void*) we must use const cast from void *.
    // Otherwise C++ compiler would complain that we are trying
    // to assign const void* to void*.
#ifdef _WIN32
    WSABUF v;
#else
    struct iovec v;
#endif
    memset(&v, 0, sizeof(v));
#ifdef _WIN32
    v.buf = (char *)(pkt->getBuffer().getData());
    v.len = pkt->getBuffer().getLength();
    m.lpBuffers = &v;
    m.dwBufferCount = 1;
#else
    v.iov_base = const_cast<void *>(pkt->getBuffer().getData());
    v.iov_len = pkt->getBuffer().getLength();
    m.msg_iov = &v;
    m.msg_iovlen = 1;
#endif

    // Setting the interface is a bit more involved.
    //
    // We have to create a "control message", and set that to
    // define the IPv6 packet information. We could set the
    // source address if we wanted, but we can safely let the
    // kernel decide what that should be.
#ifdef _WIN32
    m.Control.buf = &control_buf_[0];
    m.Control.len = control_buf_len_;
    WSACMSGHDR *cmsg = WSA_CMSG_FIRSTHDR(&m);
    cmsg->cmsg_level = IPPROTO_IPV6;
    cmsg->cmsg_type = IPV6_PKTINFO;
    cmsg->cmsg_len = WSA_CMSG_LEN(sizeof(struct in6_pktinfo));
    struct in6_pktinfo *pktinfo = convertPktInfo6(WSA_CMSG_DATA(cmsg));
#else
    m.msg_control = &control_buf_[0];
    m.msg_controllen = control_buf_len_;
    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&m);
    cmsg->cmsg_level = IPPROTO_IPV6;
    cmsg->cmsg_type = IPV6_PKTINFO;
    cmsg->cmsg_len = CMSG_LEN(sizeof(struct in6_pktinfo));
    struct in6_pktinfo *pktinfo = convertPktInfo6(CMSG_DATA(cmsg));
#endif
    memset(pktinfo, 0, sizeof(struct in6_pktinfo));
    pktinfo->ipi6_ifindex = pkt->getIndex();
#ifdef _WIN32
    m.Control.len = cmsg->cmsg_len;
#else
    m.msg_controllen = cmsg->cmsg_len;
#endif

    pkt->updateTimestamp();

#ifdef _WIN32
    DWORD bsent;
    if (WSASendMsg(getSocket(*pkt), &m, 0, &bsent, NULL, NULL) == SOCKET_ERROR)
        result = -1;
    else
        result = (int) bsent;
#else
    result = sendmsg(getSocket(*pkt), &m, 0);
#endif
    if (result < 0) {
        isc_throw(Unexpected, "Pkt6 send failed: sendmsg() returned " << result);
    }
    cout << "Sent " << pkt->getBuffer().getLength() << " bytes over socket " << getSocket(*pkt)
         << " on " << iface->getFullName() << " interface: "
         << " dst=[" << pkt->getRemoteAddr().toText() << "]:" << pkt->getRemotePort()
         << ", src=" << pkt->getLocalAddr().toText() << "]:" << pkt->getLocalPort()
         << endl;

    return (result >= 0);
}

bool
IfaceMgr::send(const Pkt4Ptr& pkt)
{

    Iface* iface = getIface(pkt->getIface());
    if (!iface) {
        isc_throw(BadValue, "Unable to send Pkt4. Invalid interface ("
                  << pkt->getIface() << ") specified.");
    }

    memset(&control_buf_[0], 0, control_buf_len_);


    // Set the target address we're sending to.
    sockaddr_in to;
    memset(&to, 0, sizeof(to));
    to.sin_family = AF_INET;
    to.sin_port = htons(pkt->getRemotePort());
    to.sin_addr.s_addr = htonl(pkt->getRemoteAddr());

#ifdef _WIN32
    WSAMSG m;
#else
    struct msghdr m;
#endif
    // Initialize our message header structure.
    memset(&m, 0, sizeof(m));
#ifdef _WIN32
    m.name = (struct sockaddr *)&to;
    m.namelen = sizeof(to);
#else
    m.msg_name = (struct sockaddr *)&to;
    m.msg_namelen = sizeof(to);
#endif

    // Set the data buffer we're sending. (Using this wacky
    // "scatter-gather" stuff... we only have a single chunk
    // of data to send, so we declare a single vector entry.)
#ifdef _WIN32
    WSABUF v;
#else
    struct iovec v;
#endif
    memset(&v, 0, sizeof(v));
    // iov_base field is of void * type. We use it for packet
    // transmission, so this buffer will not be modified.
#ifdef _WIN32
    v.buf = (char *)(pkt->getBuffer().getData());
    v.len = pkt->getBuffer().getLength();
    m.lpBuffers = &v;
    m.dwBufferCount = 1;
#else
    v.iov_base = const_cast<void *>(pkt->getBuffer().getData());
    v.iov_len = pkt->getBuffer().getLength();
    m.msg_iov = &v;
    m.msg_iovlen = 1;
#endif

    // call OS-specific routines (like setting interface index)
    os_send4(m, control_buf_, control_buf_len_, pkt);

    cout << "Trying to send " << pkt->getBuffer().getLength() << " bytes to "
         << pkt->getRemoteAddr().toText() << ":" << pkt->getRemotePort()
         << " over socket " << getSocket(*pkt) << " on interface "
         << getIface(pkt->getIface())->getFullName() << endl;

    pkt->updateTimestamp();

#ifdef _WIN32
    DWORD bsent;
    int result = -1;
    if (WSASendMsg(getSocket(*pkt), &m, 0, &bsent, NULL, NULL) != SOCKET_ERROR)
        result = (int) bsent;
#else
    int result = sendmsg(getSocket(*pkt), &m, 0);
#endif
    if (result < 0) {
        isc_throw(Unexpected, "Pkt4 send failed.");
    }

    cout << "Sent " << pkt->getBuffer().getLength() << " bytes over socket " << getSocket(*pkt)
         << " on " << iface->getFullName() << " interface: "
         << " dst=" << pkt->getRemoteAddr().toText() << ":" << pkt->getRemotePort()
         << ", src=" << pkt->getLocalAddr().toText() << ":" << pkt->getLocalPort()
         << endl;

    return (result >= 0);
}


boost::shared_ptr<Pkt4>
IfaceMgr::receive4(uint32_t timeout) {

    const SocketInfo* candidate = 0;
    IfaceCollection::const_iterator iface;

    fd_set sockets;
    FD_ZERO(&sockets);
    int maxfd = 0;

    stringstream names;

    /// @todo: marginal performance optimization. We could create the set once
    /// and then use its copy for select(). Please note that select() modifies
    /// provided set to indicated which sockets have something to read.
    for (iface = ifaces_.begin(); iface != ifaces_.end(); ++iface) {

        for (SocketCollection::const_iterator s = iface->sockets_.begin();
             s != iface->sockets_.end(); ++s) {

            // Only deal with IPv4 addresses.
            if (s->addr_.getFamily() == AF_INET) {
                names << s->sockfd_ << "(" << iface->getName() << ") ";

                // Add this socket to listening set
                FD_SET(s->sockfd_, &sockets);
#ifndef _WIN32
                if (maxfd < s->sockfd_) {
                    maxfd = s->sockfd_;
                }
#endif
            }
        }
    }

    // if there is session socket registered...
    if (session_socket_ != INVALID_SOCKET) {
        // at it to the set as well
        FD_SET(session_socket_, &sockets);
#ifndef _WIN32
        if (maxfd < session_socket_)
            maxfd = session_socket_;
#endif
        names << session_socket_ << "(session)";
    }

    /// @todo: implement sub-second precision one day
    struct timeval select_timeout;
    select_timeout.tv_sec = timeout;
    select_timeout.tv_usec = 0;

    cout << "Trying to receive data on sockets: " << names.str()
         << ". Timeout is " << timeout << " seconds." << endl;
    int result = select(maxfd + 1, &sockets, NULL, NULL, &select_timeout);
    cout << "select returned " << result << endl;

    if (result == 0) {
        // nothing received and timeout has been reached
        return (Pkt4Ptr()); // NULL
    } else if (result < 0) {
#ifdef _WIN32
        cout << "Socket read error: " << strerror(WSAGetLastError()) << endl;
#else
        cout << "Socket read error: " << strerror(errno) << endl;
#endif

        /// @todo: perhaps throw here?
        return (Pkt4Ptr()); // NULL
    }

    // Let's find out which socket has the data
    if ((session_socket_ != INVALID_SOCKET) && (FD_ISSET(session_socket_, &sockets))) {
        // something received over session socket
        cout << "BIND10 command or config available over session socket." << endl;

        if (session_callback_) {
            // in theory we could call io_service.run_one() here, instead of
            // implementing callback mechanism, but that would introduce
            // asiolink dependency to libdhcp++ and that is something we want
            // to avoid (see CPE market and out long term plans for minimalistic
            // implementations.
            session_callback_();
        }

        return (Pkt4Ptr()); // NULL
    }

    // Let's find out which interface/socket has the data
    for (iface = ifaces_.begin(); iface != ifaces_.end(); ++iface) {
        for (SocketCollection::const_iterator s = iface->sockets_.begin();
             s != iface->sockets_.end(); ++s) {
            if (FD_ISSET(s->sockfd_, &sockets)) {
                candidate = &(*s);
                break;
            }
        }
        if (candidate) {
            break;
        }
    }

    if (!candidate) {
        cout << "Received data over unknown socket." << endl;
        return (Pkt4Ptr()); // NULL
    }

    cout << "Trying to receive over UDP4 socket " << candidate->sockfd_ << " bound to "
         << candidate->addr_.toText() << "/port=" << candidate->port_ << " on "
         << iface->getFullName() << endl;

    // Now we have a socket, let's get some data from it!
    struct sockaddr_in from_addr;
    uint8_t buf[RCVBUFSIZE];

    memset(&control_buf_[0], 0, control_buf_len_);
    memset(&from_addr, 0, sizeof(from_addr));

    // Initialize our message header structure.
#ifdef _WIN32
    GUID WSARecvMsg_GUID = WSAID_WSARECVMSG;
    LPFN_WSARECVMSG WSARecvMsg;
    WSAMSG m;
#else
    struct msghdr m;
#endif
    memset(&m, 0, sizeof(m));

    // Point so we can get the from address.
#ifdef _WIN32
    m.name = (struct sockaddr *)&from_addr;
    m.namelen = sizeof(from_addr);
#else
    m.msg_name = (struct sockaddr *)&from_addr;
    m.msg_namelen = sizeof(from_addr);
#endif

#ifdef _WIN32
    WSABUF v;
    v.buf = (char *)buf;
    v.len = RCVBUFSIZE;
    m.lpBuffers = &v;
    m.dwBufferCount = 1;
#else
    struct iovec v;
    v.iov_base = static_cast<void*>(buf);
    v.iov_len = RCVBUFSIZE;
    m.msg_iov = &v;
    m.msg_iovlen = 1;
#endif

    // Getting the interface is a bit more involved.
    //
    // We set up some space for a "control message". We have
    // previously asked the kernel to give us packet
    // information (when we initialized the interface), so we
    // should get the destination address from that.
#ifdef _WIN32
    m.Control.buf = &control_buf_[0];
    m.Control.len = control_buf_len_;
#else
    m.msg_control = &control_buf_[0];
    m.msg_controllen = control_buf_len_;
#endif

#ifdef _WIN32
    DWORD brecv;
    if ((WSAIoctl(candidate->sockfd_,
                  SIO_GET_EXTENSION_FUNCTION_POINTER,
                  &WSARecvMsg_GUID, sizeof(WSARecvMsg_GUID),
                  &WSARecvMsg, sizeof(WSARecvMsg),
                  &brecv, NULL, NULL) == SOCKET_ERROR) ||
        (WSARecvMsg(candidate->sockfd_, &m,
                    &brecv, NULL, NULL) == SOCKET_ERROR))
        result = -1;
    else
        result = (int) brecv;
#else
    result = recvmsg(candidate->sockfd_, &m, 0);
#endif
    if (result < 0) {
        cout << "Failed to receive UDP4 data." << endl;
        return (Pkt4Ptr()); // NULL
    }

    // We have all data let's create Pkt4 object.
    Pkt4Ptr pkt = Pkt4Ptr(new Pkt4(buf, result));

    pkt->updateTimestamp();

    unsigned int ifindex = iface->getIndex();

    IOAddress from(htonl(from_addr.sin_addr.s_addr));
    uint16_t from_port = htons(from_addr.sin_port);

    // Set receiving interface based on information, which socket was used to
    // receive data. OS-specific info (see os_receive4()) may be more reliable,
    // so this value may be overwritten.
    pkt->setIndex(ifindex);
    pkt->setIface(iface->getName());
    pkt->setRemoteAddr(from);
    pkt->setRemotePort(from_port);
    pkt->setLocalPort(candidate->port_);

    if (!os_receive4(m, pkt)) {
        cout << "Unable to find pktinfo" << endl;
        return (boost::shared_ptr<Pkt4>()); // NULL
    }

    cout << "Received " << result << " bytes from " << from.toText()
         << "/port=" << from_port
         << " sent to " << pkt->getLocalAddr().toText() << " over interface "
         << iface->getFullName() << endl;

    return (pkt);
}

Pkt6Ptr IfaceMgr::receive6() {
    uint8_t buf[RCVBUFSIZE];

    memset(&control_buf_[0], 0, control_buf_len_);
    struct sockaddr_in6 from;
    memset(&from, 0, sizeof(from));

    // Initialize our message header structure.
#ifdef _WIN32
    GUID WSARecvMsg_GUID = WSAID_WSARECVMSG;
    LPFN_WSARECVMSG WSARecvMsg;
    WSAMSG m;
#else
    struct msghdr m;
#endif
    memset(&m, 0, sizeof(m));

    // Point so we can get the from address.
#ifdef _WIN32
    m.name = (struct sockaddr *)&from;
    m.namelen = sizeof(from);
#else
    m.msg_name = (struct sockaddr *)&from;
    m.msg_namelen = sizeof(from);
#endif

    // Set the data buffer we're receiving. (Using this wacky
    // "scatter-gather" stuff... but we that doesn't really make
    // sense for us, so we use a single vector entry.)
#ifdef _WIN32
    WSABUF v;
#else
    struct iovec v;
#endif
    memset(&v, 0, sizeof(v));
#ifdef _WIN32
    v.buf = (char *)buf;
    v.len = RCVBUFSIZE;
    m.lpBuffers = &v;
    m.dwBufferCount = 1;
#else
    v.iov_base = static_cast<void*>(buf);
    v.iov_len = RCVBUFSIZE;
    m.msg_iov = &v;
    m.msg_iovlen = 1;
#endif

    // Getting the interface is a bit more involved.
    //
    // We set up some space for a "control message". We have
    // previously asked the kernel to give us packet
    // information (when we initialized the interface), so we
    // should get the destination address from that.
#ifdef _WIN32
    m.Control.buf = &control_buf_[0];
    m.Control.len = control_buf_len_;
#else
    m.msg_control = &control_buf_[0];
    m.msg_controllen = control_buf_len_;
#endif

    /// TODO: Need to move to select() and pool over
    /// all available sockets. For now, we just take the
    /// first interface and use first socket from it.
    IfaceCollection::const_iterator iface = ifaces_.begin();
    const SocketInfo* candidate = 0;
    while (iface != ifaces_.end()) {
        for (SocketCollection::const_iterator s = iface->sockets_.begin();
             s != iface->sockets_.end(); ++s) {
            if (s->addr_.getFamily() != AF_INET6) {
                continue;
            }
            if (s->addr_.getAddress().to_v6().is_multicast()) {
                candidate = &(*s);
                break;
            }
            if (!candidate) {
                candidate = &(*s); // it's not multicast, but it's better than nothing
            }
        }
        if (candidate) {
            break;
        }
        ++iface;
    }
    if (iface == ifaces_.end()) {
        isc_throw(Unexpected, "No suitable IPv6 interfaces detected. Can't receive anything.");
    }

    if (!candidate) {
        isc_throw(Unexpected, "Interface " << iface->getFullName()
                  << " does not have any sockets open.");
    }

    cout << "Trying to receive over UDP6 socket " << candidate->sockfd_ << " bound to "
         << candidate->addr_.toText() << "/port=" << candidate->port_ << " on "
         << iface->getFullName() << endl;
#ifdef _WIN32
    DWORD brecv;
    int result = -1;
    if ((WSAIoctl(candidate->sockfd_,
                  SIO_GET_EXTENSION_FUNCTION_POINTER,
                  &WSARecvMsg_GUID, sizeof(WSARecvMsg_GUID),
                  &WSARecvMsg, sizeof(WSARecvMsg),
                  &brecv, NULL, NULL) != SOCKET_ERROR) &&
        (WSARecvMsg(candidate->sockfd_, &m,
                    &brecv, NULL, NULL) != SOCKET_ERROR))
        result = (int) brecv;
#else
    int result = recvmsg(candidate->sockfd_, &m, 0);
#endif

    struct in6_addr to_addr;
    memset(&to_addr, 0, sizeof(to_addr));

    int ifindex = -1;
    if (result >= 0) {
        struct in6_pktinfo* pktinfo = NULL;


        // If we did read successfully, then we need to loop
        // through the control messages we received and
        // find the one with our destination address.
        //
        // We also keep a flag to see if we found it. If we
        // didn't, then we consider this to be an error.
        bool found_pktinfo = false;
#ifdef _WIN32
        WSACMSGHDR* cmsg = WSA_CMSG_FIRSTHDR(&m);
#else
        struct cmsghdr* cmsg = CMSG_FIRSTHDR(&m);
#endif
        while (cmsg != NULL) {
            if ((cmsg->cmsg_level == IPPROTO_IPV6) &&
                (cmsg->cmsg_type == IPV6_PKTINFO)) {
#ifdef _WIN32
                pktinfo = convertPktInfo6(WSA_CMSG_DATA(cmsg));
#else
                pktinfo = convertPktInfo6(CMSG_DATA(cmsg));
#endif
                to_addr = pktinfo->ipi6_addr;
                ifindex = pktinfo->ipi6_ifindex;
                found_pktinfo = true;
                break;
            }
#ifdef _WIN32
            cmsg = WSA_CMSG_NXTHDR(&m, cmsg);
#else
            cmsg = CMSG_NXTHDR(&m, cmsg);
#endif
        }
        if (!found_pktinfo) {
            cout << "Unable to find pktinfo" << endl;
            return (Pkt6Ptr()); // NULL
        }
    } else {
        cout << "Failed to receive data." << endl;
        return (Pkt6Ptr()); // NULL
    }

    // Let's create a packet.
    Pkt6Ptr pkt;
    try {
        pkt = Pkt6Ptr(new Pkt6(buf, result));
    } catch (const std::exception&) {
        cout << "Failed to create new packet." << endl;
        return (Pkt6Ptr()); // NULL
    }

    pkt->updateTimestamp();

    pkt->setLocalAddr(IOAddress::from_bytes(AF_INET6,
                      reinterpret_cast<const uint8_t*>(&to_addr)));
    pkt->setRemoteAddr(IOAddress::from_bytes(AF_INET6,
                       reinterpret_cast<const uint8_t*>(&from.sin6_addr)));
    pkt->setRemotePort(ntohs(from.sin6_port));
    pkt->setIndex(ifindex);

    Iface* received = getIface(pkt->getIndex());
    if (received) {
        pkt->setIface(received->getName());
    } else {
        cout << "Received packet over unknown interface (ifindex="
             << pkt->getIndex() << ")." << endl;
        return (boost::shared_ptr<Pkt6>()); // NULL
    }

    /// @todo: Move this to LOG_DEBUG
    cout << "Received " << pkt->getBuffer().getLength() << " bytes over "
         << pkt->getIface() << "/" << pkt->getIndex() << " interface: "
         << " src=" << pkt->getRemoteAddr().toText()
         << ", dst=" << pkt->getLocalAddr().toText()
         << endl;

    return (pkt);
}

#ifdef _WIN32
SOCKET
#else
int
#endif
IfaceMgr::getSocket(const isc::dhcp::Pkt6& pkt) {
    Iface* iface = getIface(pkt.getIface());
    if (iface == NULL) {
        isc_throw(BadValue, "Tried to find socket for non-existent interface "
                  << pkt.getIface());
    }

    SocketCollection::const_iterator s;
    for (s = iface->sockets_.begin(); s != iface->sockets_.end(); ++s) {
        if ((s->family_ == AF_INET6) &&
            (!s->addr_.getAddress().to_v6().is_multicast())) {
            return (s->sockfd_);
        }
        /// @todo: Add more checks here later. If remote address is
        /// not link-local, we can't use link local bound socket
        /// to send data.
    }

    isc_throw(Unexpected, "Interface " << iface->getFullName()
              << " does not have any suitable IPv6 sockets open.");
}

#ifdef _WIN32
SOCKET
#else
int
#endif
IfaceMgr::getSocket(isc::dhcp::Pkt4 const& pkt) {
    Iface* iface = getIface(pkt.getIface());
    if (iface == NULL) {
        isc_throw(BadValue, "Tried to find socket for non-existent interface "
                  << pkt.getIface());
    }

    SocketCollection::const_iterator s;
    for (s = iface->sockets_.begin(); s != iface->sockets_.end(); ++s) {
        if (s->family_ == AF_INET) {
            return (s->sockfd_);
        }
        /// TODO: Add more checks here later. If remote address is
        /// not link-local, we can't use link local bound socket
        /// to send data.
    }

    isc_throw(Unexpected, "Interface " << iface->getFullName()
              << " does not have any suitable IPv4 sockets open.");
}

} // end of namespace isc::dhcp
} // end of namespace isc
