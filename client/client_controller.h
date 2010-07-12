// Copyright (C) 2010  Internet Systems Consortium, Inc. ("ISC")
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

// $Id$

#ifndef __CLIENT_CONTROLLER_H
#define __CLIENT_CONTROLLER_H

#include <boost/shared_array.hpp>

#include "client_communicator.h"
#include "logger.h"


/// \brief Runs the Tests
///
/// The ClientController class in the client is the module that actually carries out
/// the tests.  It creates a buffer of the specified size and fills it with
/// random data.  It then enters a loop, sending and receiving data.  On
/// every packet received, the following checks are done:
/// - Data is 4 bytes longer than sent
/// - Data received is same as data sent (except those four bytes)
/// - Last 4-byte word is the correct checksum

class ClientController : private boost::noncopyable {
public:

    /// \brief Stores parameters
    ///
    /// Stores the parameters of the test.
    /// \param count Number of packets to send in the test.
    /// \patam burst Burst size of the communication
    /// \param pktsize Size of each packet.
    ClientController(uint32_t count, uint32_t burst, uint16_t pktsize) :
        count_(count), burst_(burst), pktsize_(pktsize)
    {}

    /// \brief Runs the test
    ///
    /// Runs the test by constructing the data arrays, then looping, sending
    /// and receiving data.  Data is sent in bursts (the size of each burst
    /// being specified in the constructor). After each burst, the client waits
    /// for the packets to be returned before sending the next set of packets.
    /// At the end of the test, information is appended to the logfile.
    /// \param communicator Communicator to be used for the test.
    /// \param logger Logger object to be used for the test.
    void run(ClientCommunicator& communicator, Logger& logger);

    /// \brief Sending task
    ///
    /// This sends the given number of packets to the server.
    /// \param communicator Interface to the I/O system
    /// \param npacket Number of packets to send
    void sendTask(ClientCommunicator& communicator, int npacket);

    /// \brief Receiving task
    ///
    /// Receives the specified number of packets from the server and compares
    /// the contents with the packets sent.
    /// \param communicator Interface to the I/O system
    /// \param Number of packets expected
    void receiveTask(ClientCommunicator& communicator, int npacket);


private:
    /// \brief Sets up for the test
    ///
    /// Allocates the buffers and initialises the data.
    void setUp();

    /// \brief Runs the test
    ///
    /// Actually performs the testing and logging of the result.
    void runTest(ClientCommunicator& communicator);

private:
    uint16_t    pktsize_;           //< Size of each packet
    uint32_t    burst_;             //< Burst size of the test
    uint32_t    count_;             //< Total number of packets to send
    boost::shared_array<uint8_t> snd_buffer_;   //< Data being sent
    boost::shared_array<uint8_t> rcv_buffer_;   //< Data being received
};

#endif // __CLIENT_CONTROLLER_H
