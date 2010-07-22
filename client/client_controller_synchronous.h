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

#ifndef __CLIENT_CONTROLLER_SYNCHRONOUS
#define __CLIENT_CONTROLLER_SYNCHRONOUS

#include <boost/shared_array.hpp>

#include "client_communicator.h"
#include "client_controller.h"
#include "logger.h"


/// \brief Runs the Tests
///
/// The ClientControllerSynchronous class in the client is the module that
/// actually carries out the synchronous tests.  It creates a buffer of the
/// specified size and fills it with data.  It then enters a loop, sending and
/// receiving data.  On every packet received, the following checks are done:
/// - Data is 4 bytes longer than sent
/// - Data received is same as data sent (except those four bytes)
/// - Last 4-byte word is the correct checksum

class ClientControllerSynchronous : public ClientController {
public:

    /// \brief Stores parameters
    ///
    /// Stores the parameters of the test.
    /// \param count Number of packets to send in the test.
    /// \patam burst Burst size of the communication
    /// \param size Size of each packet.
    ClientControllerSynchronous(long count, long burst, uint16_t size) :
        ClientController(count, burst, size, 0)
    {}
    virtual ~ClientControllerSynchronous() {}

    /// \brief Runs the test synchronously
    ///
    /// Runs the test by constructing the data arrays, then looping, sending
    /// and receiving data.  Data is sent in bursts (the size of each burst
    /// being specified in the constructor). After each burst, the client waits
    /// for the packets to be returned before sending the next set of packets.
    /// At the end of the test, information is appended to the logfile.
    /// \param communicator Communicator to be used for the test.
    /// \param logger Logger used to time the test.
    virtual void runTest(ClientCommunicator& communicator, Logger& logger);

    /// \brief Synchronous Sending task
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
};

#endif // __CLIENT_CONTROLLER_SYNCHRONOUS
