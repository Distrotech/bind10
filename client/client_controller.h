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
#include "udp_buffer.h"


/// \brief Runs the Tests
///
/// The ClientController is the base for the synchronous and asynchronous
/// controller classes that run the tests.  It merely handles the storing of
/// parameters and the setup of data.

class ClientController : private boost::noncopyable {
public:

    /// \brief Stores parameters
    ///
    /// Stores the parameters of the test.
    /// \param count Number of packets to send in the test.
    /// \patam burst Burst size of the communication
    /// \param size Size of each packet.
    /// \param margin Margin to allow for asynchronous loss
    ClientController(long count, long burst, uint16_t size, long margin) :
        count_(count), burst_(burst), size_(size), margin_(margin),
        snd_buffer_(), rcv_buffer_()
    {}
    virtual ~ClientController() {}

    /// \brief Runs the Test
    ///
    /// Handles the initialization of the data and the logging.  The clock is
    /// started, runTest() called and, when it returns, the clock is stopped.
    /// \param communicator Communicator to be used for the test.
    /// \param logger Logger used to start and stop the clock
    virtual void run(ClientCommunicator& communicator, Logger& logger);

    /// \brief Runs the Class-Specific test
    ///
    /// Pure virtual method that must be overridden by each subclass controller.
    /// \param communicator Communicator to be used for the test.
    /// \param logger Logger object to be used for the test.
    virtual void runTest(ClientCommunicator& communicator, Logger& logger) = 0;

    /// \brief Sets up for the test
    ///
    /// Allocates the buffers and initialises the data.
    virtual void setUp();

protected:
    uint16_t    size_;              //< Size of each packet (max 64K)

    // The following three elements are "long" (rather than unsigned long)
    // as they may be used in calculations involving a signed number.

    long        burst_;             //< Burst size of the test
    long        count_;             //< Total number of packets to send
    long        margin_;            //< Margin packets (asynch mode only)
    UdpBuffer   snd_buffer_;        //< Data being sent
    UdpBuffer   rcv_buffer_;        //< Data being received
};

#endif // __CLIENT_CONTROLLER_H
