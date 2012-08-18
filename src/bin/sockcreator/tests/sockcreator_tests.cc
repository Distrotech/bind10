// Copyright (C) 2011-2012  Internet Systems Consortium, Inc. ("ISC")
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

#include "../sockcreator.h"

#include <util/unittests/fork.h>
#include <util/io/socket.h>

#include <boost/lexical_cast.hpp>
#include <gtest/gtest.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <iostream>
#include <cstring>
#include <cerrno>

using namespace isc::socket_creator;
using namespace isc::util::unittests;
using namespace isc::util::io;

// The tests check both TCP and UDP sockets on IPv4 and IPv6.
//
// Essentially we need to check all four combinations of TCP/UDP and IPv4/IPv6.
// The different address families (IPv4/IPv6) require different structures to
// hold the address information, and so some common code is in the form of
// templates (or overloads), parameterised on the structure type.
//
// The protocol is determined by an integer (SOCK_STREAM or SOCK_DGRAM) so
// cannot be templated in the same way.  Relevant check functions are
// selected manually.

namespace {

// Set IP-version-specific fields.

void
setAddressFamilyFields(sockaddr_in* address) {
    address->sin_family = AF_INET;
    address->sin_addr.s_addr = INADDR_ANY;
}

void
setAddressFamilyFields(sockaddr_in6* address) {
    address->sin6_family = AF_INET6;
    address->sin6_addr = in6addr_loopback;
}

// Socket has been opened, peform a check on it.  The sole argument is the
// socket descriptor.  The TCP check is the same regardless of the address
// family.  The UDP check requires that the socket address be obtained so
// is parameterised on the type of structure required to hold the address.

void
tcpCheck(const socket_type socknum) {
    // Sufficient to be able to listen on the socket.
    EXPECT_EQ(0, listen(socknum, 1));
}

template <typename ADDRTYPE>
void
udpCheck(const socket_type socknum) {
    // UDP testing is more complicated than TCP: send a packet to ourselves and
    // see if it arrives.

    // Get details of the socket so that we can use it as the target of the
    // sendto().
    ADDRTYPE addr;
    memset(&addr, 0, sizeof(addr));
    sockaddr* addr_ptr = reinterpret_cast<sockaddr*>(&addr);
    socklen_t len = sizeof(addr);
    ASSERT_EQ(0, getsockname(socknum, addr_ptr, &len));

    // Send the packet to ourselves and check we receive it.
    ASSERT_EQ(5, sendto(socknum, "test", 5, 0, addr_ptr, sizeof(addr))) <<
        "Send failed with error " << strerror(errno) << " on socket " <<
        socknum;
    char buffer[5];
    ASSERT_EQ(5, recv(socknum, buffer, 5, 0)) <<
        "Recv failed with error " << strerror(errno) << " on socket " <<
        socknum;
    EXPECT_STREQ("test", buffer);
}

// The check function (tcpCheck/udpCheck) is passed as a parameter to the test
// code, so provide a conveniet typedef.
typedef void (*socket_check_t)(const socket_type);

// Address-family-specific socket checks.
//
// The first argument is used to select the overloaded check function.
// The other argument is the socket descriptor number.

// IPv4 check
void addressFamilySpecificCheck(const sockaddr_in*, const socket_type,
                                const int) {
}

// IPv6 check
void addressFamilySpecificCheck(const sockaddr_in6*, const socket_type socknum,
                                const int s_type)
{
    int options;
    socklen_t len = sizeof(options);
    EXPECT_EQ(0, getsockopt(socknum, IPPROTO_IPV6, IPV6_V6ONLY, &options,
                            &len));
    EXPECT_NE(0, options);
    if (s_type == SOCK_DGRAM) {
    // Some more checks for UDP - MTU
#ifdef IPV6_USE_MIN_MTU        /* RFC 3542, not too common yet*/
        // use minimum MTU
        EXPECT_EQ(0, getsockopt(socknum, IPPROTO_IPV6, IPV6_USE_MIN_MTU,
                                &options, &len)) << strerror(errno);
        EXPECT_NE(0, options);
#else
        // We do not check for the IPV6_MTU, because while setting works (eg.
        // the packets are fragmented correctly), the getting does not. If
        // we try to getsockopt it, an error complaining it can't be accessed
        // on unconnected socket is returned. If we try to connect it, the
        // MTU of the interface is returned, not the one we set. So we live
        // in belief it works because we examined the packet dump.
#if defined(IPV6_MTU_DISCOVER) && defined(IPV6_PMTUDISC_DONT)
        // Turned off Path MTU discovery on IPv6/UDP sockets?
        EXPECT_EQ(0, getsockopt(socknum, IPPROTO_IPV6, IPV6_MTU_DISCOVER,
                                &options, &len)) << strerror(errno);
        EXPECT_EQ(IPV6_PMTUDISC_DONT, options);
#endif
#endif
    }
}

// Just ignore the fd and pretend success. We close invalid fds in the tests.
int
closeIgnore(socket_type) {
    return (0);
}

bool bind_failed(false);

// Generic version of the socket test.  It creates the socket and checks that
// it is a valid descriptor.  The family-specific check functions are called
// to check that the socket is valid.  The function is parameterised according
// to the structure used to hold the address.
//
// Arguments:
// socket_type Type of socket to create (SOCK_DGRAM or SOCK_STREAM)
// socket_check Associated check function - udpCheck() or tcpCheck()
template <typename ADDRTYPE>
void testAnyCreate(int s_type, socket_check_t socket_check) {

    // Create the socket.
    ADDRTYPE addr;
    memset(&addr, 0, sizeof(addr));
    setAddressFamilyFields(&addr);
    sockaddr* addr_ptr = reinterpret_cast<sockaddr*>(&addr);
    const socket_type socket = getSock(s_type, addr_ptr, &bind_failed,
                                       sizeof(addr), closeIgnore);
    ASSERT_NE(socket, invalid_socket) <<
        "Couldn't create socket: failed with " <<
        "return code " << socket << " and error " << strerror(errno);

    // Perform socket-type-specific testing.
    socket_check(socket);

    // Do address-family-independent
    int options;
    socklen_t len = sizeof(options);
    EXPECT_EQ(0, getsockopt(socket, SOL_SOCKET, SO_REUSEADDR, &options, &len));
    EXPECT_NE(0, options);

    // ...and the address-family specific tests.
    addressFamilySpecificCheck(&addr, socket, s_type);

    // Tidy up and exit.
    EXPECT_EQ(0, close(socket));
}


// Several tests to ensure we can create the sockets.
TEST(get_sock, udp4_create) {
    testAnyCreate<sockaddr_in>(SOCK_DGRAM, udpCheck<sockaddr_in>);
}

TEST(get_sock, tcp4_create) {
    testAnyCreate<sockaddr_in>(SOCK_STREAM, tcpCheck);
}

TEST(get_sock, udp6_create) {
    testAnyCreate<sockaddr_in6>(SOCK_DGRAM, udpCheck<sockaddr_in6>);
}

TEST(get_sock, tcp6_create) {
    testAnyCreate<sockaddr_in6>(SOCK_STREAM, tcpCheck);
}

bool close_called(false);

// You can use it as a close mockup. If you care about checking if it was really
// called, you can use the close_called variable. But set it to false before the
// test.
int closeCall(socket_type socket) {
    close(socket);
    close_called = true;
    return (0);
}

// Ask the get_sock function for some nonsense and test if it is able to report
// an error.
TEST(get_sock, fail_with_nonsense) {
    sockaddr addr;
    memset(&addr, 0, sizeof(addr));
    close_called = false;
    
    ASSERT_EQ(invalid_socket,
              getSock(0, &addr, &bind_failed, sizeof addr, closeCall));
    ASSERT_FALSE(close_called); // The "socket" call should have failed already
    ASSERT_FALSE(bind_failed);
}

// Bind should have failed here
TEST(get_sock, fail_with_bind) {
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = 1;
    // No host should have this address on the interface, so it should not be
    // possible to bind it.
    addr.sin_addr.s_addr = inet_addr("192.0.2.1");
    close_called = false;
    ASSERT_EQ(invalid_socket, getSock(SOCK_STREAM,
                                      reinterpret_cast<sockaddr*>(&addr),
                                      &bind_failed,
                                      sizeof addr, closeCall));
    ASSERT_TRUE(close_called); // The "socket" call should have failed already
    ASSERT_TRUE(bind_failed);
}

// The main run() function in the socket creator takes three functions to
// get the socket, send information to it, and close it.  These allow for
// alternatives to the system functions to be used for testing.

// Replacement getSock() function.
// The return value indicates the result of checks and is encoded.  Using LSB
// bit numbering (least-significant bit is bit 0) then:
//
// bit 0: 1 if "type" is known, 0 otherwise
// bit 1: 1 for UDP, 0 for TCP
// bit 2: 1 if address family is known, 0 otherwise
// bit 3: 1 for IPv6, 0 for IPv4
// bit 4: 1 if port passed was valid
//
// Other possible return values are:
//
// invalid_socket: The simulated socket() or bind() call has failed
socket_type
getSockDummy(const int type, struct sockaddr* addr, bool* bf,
             const socklen_t, const close_t) {
    socket_type result = 0;
    int port = 0;

    // Validate type field
    switch (type) {
        case SOCK_STREAM:
            result |= 0x01;
            break;

        case SOCK_DGRAM:
            result |= 0x03;
            break;
    }

    // Validate address family
    switch (addr->sa_family) {
        case AF_INET:
            result |= 0x04;
            port = reinterpret_cast<sockaddr_in*>(addr)->sin_port;
            break;

        case AF_INET6:
            result |= 0x0C;
            port = reinterpret_cast<sockaddr_in6*>(addr)->sin6_port;
            break;
    }

    // The port should be 0xffff. If it's not, we change the result.
    // The port of 0xbbbb means bind should fail and 0xcccc means
    // socket should fail.
    if (port != 0xffff) {
        errno = 0;
        if (port == 0xbbbb) {
            *bf = true;
            return (invalid_socket);
        } else if (port == 0xcccc) {
            *bf = false;
            return (invalid_socket);
        } else {
            result |= 0x10;
        }
    }
    return (result);
}

// Dummy send function - return data (the result of getSock()) to the destination.
int
send_SdDummy(const socket_type destination, const socket_type what) {
    // Make sure it is 1 byte so we know the length. We do not use more during
    // the test anyway.  And even with the LS bute, we can distinguish between
    // the different results.
    const char sd_data = what & 0xff;
    const bool status = write_data(destination, &sd_data, sizeof(sd_data));
    return (status ? 0 : -1);
}

// Generic test that it works, with various inputs and outputs.
// It uses different functions to create the socket and send it and pass
// data to it and check it returns correct data back, to see if the run()
// parses the commands correctly.
void runTest(const char* input_data, const size_t input_size,
             const char* output_data, const size_t output_size,
             bool should_succeed = true,
             const close_t test_close = closeIgnore,
             const send_sd_t send_sd = send_SdDummy)
{
    // Prepare the input feeder and output checker processes.  The feeder
    // process sends data from the client to run() and the checker process
    // reads the response and checks it against the expected response.
    socket_type input_sd = 0;
    const pid_t input = provide_input(&input_sd, input_data, input_size);
    ASSERT_NE(-1, input) << "Couldn't start input feeder";

    socket_type output_sd = 0;
    const pid_t output = check_output(&output_sd, output_data, output_size);
    ASSERT_NE(-1, output) << "Couldn't start output checker";

    // Run the body
    if (should_succeed) {
        EXPECT_NO_THROW(run(input_sd, output_sd, getSockDummy, send_sd,
                            test_close));
    } else {
        EXPECT_THROW(run(input_sd, output_sd, getSockDummy, send_sd,
                         test_close), isc::socket_creator::SocketCreatorError);
    }

    // Close the pipes
    close(input_sd);
    close(output_sd);

    // Check the subprocesses say everything is OK too
    EXPECT_TRUE(process_ok(input));
    EXPECT_TRUE(process_ok(output));
}


// Check it terminates successfully when asked to.
TEST(run, terminate) {
    runTest("T", 1, NULL, 0);
}

// Check it rejects incorrect input.
TEST(run, bad_input) {
    runTest("XXX", 3, "FI", 2, false);
}

// Check it correctly parses query stream to create sockets.
TEST(run, sockets) {
    runTest(
        // Commands:
        "SU4\xff\xff\0\0\0\0"   // IPv4 UDP socket, port 0xffffff, address 0.0.0.0
        "ST4\xff\xff\0\0\0\0"   // IPv4 TCP socket, port 0xffffff, address 0.0.0.0
        "ST6\xff\xff\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
                                // IPv6 UDP socket, port 0xffffff, address ::
        "SU6\xff\xff\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
                                // IPv6 TCP socket, port 0xffffff, address ::
        "T",                    // ... and terminate
        9 + 9 + 21 + 21 + 1,    // Length of command string
        "S\x07S\x05S\x0dS\x0f", // Response ("S" + LS byte of getSock() return)
        8);                     // Length of response
}


// Check if failures of get_socket are handled correctly.
TEST(run, bad_sockets) {
    // We need to construct the answer, but it depends on int length.  We expect
    // two failure answers in this test, each answer comprising two characters
    // followed by the (int) errno value.
    char result[2 * (2 + sizeof(int))];

    // We expect the errno parts to be zero but the characters to depend on the
    // exact failure.
    memset(result, 0, sizeof(result));
    strcpy(result, "EB");
    strcpy(result + 2 + sizeof(int), "ES");

    // Run the test
    runTest(
        "SU4\xbb\xbb\0\0\0\0"   // Port number will trigger simulated bind() fail
        "SU4\xcc\xcc\0\0\0\0"   // Port number will trigger simulated socket() fail
        "T",                    // Terminate
        19,                     // Length of command string
        result, sizeof(result));
}

// A close that fails.  (This causes an abort.)
int
closeFail(socket_type) {
    return (-1);
}

TEST(run, cant_close) {
    runTest("SU4\xff\xff\0\0\0\0", 9,
            "S\x07", 2,
            false, closeFail);
}

// A send of the socket descriptor that fails.  In this case we expect
// the client to receive the "S" indicating that the descriptor is
// being sent and nothing else.  This causes an abort.
int
sendSDFail(const socket_type, const socket_type) {
    return (SOCKET_SYSTEM_ERROR);
}

TEST(run, cant_send_sd) {
    runTest("SU4\xff\xff\0\0\0\0", 9,
            "S", 1,
            false, closeIgnore, sendSDFail);
}

}   // Anonymous namespace
