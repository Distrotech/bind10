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

#ifndef __CLIENT_CONTROLLER_ASYNCHRONOUS
#define __CLIENT_CONTROLLER_ASYNCHRONOUS


#include "client_communicator.h"
#include "client_controller.h"
#include "logger.h"
#include "token_bucket.h"


/// \brief Control Asynchronous Tests
///
/// The ClientControllerAsynchronous class is the class that runs the
/// asynchronous tests.  Essentiallyit sends data to the server until the
/// server returns the specified number of packets then stops.

class ClientControllerAsynchronous : public ClientController {
public:

    /// \brief Stores parameters
    ///
    /// Stores the parameters of the test.
    /// \param count Number of packets to send in the test.
    /// \patam burst Burst size of the communication
    /// \param size Size of each packet.
    /// \param margin Margin allowed for lost packets
    ClientControllerAsynchronous(long count, long burst, uint16_t size,
        long margin) :
        ClientController(count, burst, size, margin), lost_(0)

    {}
    virtual ~ClientControllerAsynchronous() {}

    /// \brief Runs the test asynchronously
    ///
    /// Runs the test by constructing the data arrays, then sending data as
    /// fast as it can until the number of outstanding packets hits the burst
    /// limit + lost count.  At that point sending stops until some packets are
    /// returned.  The test runs until the number of packets specified by the
    /// count constructor argument are returned.
    ///
    /// The burst parameter sets the window to match the client - if the client
    /// is using a burst of 8 (for example), then a response is only expected
    /// after 8 packets have been sent.  The "lost" parameter extends this
    /// window by an amount equal to the number of packets expected to be lost
    /// in the test.  This copes with the case where a packet is lost; in the
    /// above example, if any of the packets is lost, the client will hang
    /// waiting for "burst" packets before it can start responding, but the
    /// client won't send another packet because it has hit the maximum.
    /// The longer a test runs the larger this should be.
    /// \param communicator Communicator to be used for the test.
    /// \param logger Logger used to time the test.
    virtual void runTest(ClientCommunicator& communicator, Logger& logger);

    /// \brief Synchronous Sending task
    ///
    /// This sends the given number of packets to the server.
    /// \param communicator Interface to the I/O system
    /// \param synchronizer Token bucket object used to synchronize between the
    /// send and receive threads.
    void sendTask(ClientCommunicator& communicator, TokenBucket& synchronizer);

    /// \brief Receiving task
    ///
    /// Receives the specified number of packets from the server and compares
    /// the contents with the packets sent.
    /// \param communicator Interface to the I/O system
    /// \param synchronizer Token bucket object used to synchronize between the
    /// send and receive threads.
    void receiveTask(ClientCommunicator& communicator,
            TokenBucket& synchronizer);

private:
    uint32_t    lost_;          //< Count of lost packets
};

#endif // __CLIENT_CONTROLLER_ASYNCHRONOUS
