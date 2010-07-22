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

// Runs the test

#include <algorithm>
#include <iostream>
#include <iomanip>
#include <arpa/inet.h>
#include <utility>

#include <boost/cast.hpp>
#include <boost/crc.hpp>
#include <boost/cast.hpp>

#include "client_communicator.h"
#include "client_controller.h"
#include "debug.h"
#include "defaults.h"
#include "logger.h"
#include "packet_counter.h"
#include "utilities.h"



// Runs the tests and records the results in the logger.
void
ClientController::run(ClientCommunicator& communicator, Logger& logger) {

    logger.setSize(size_);
    setUp();

    // Actually execute the test
    runTest(communicator, logger);

    return;
}


// Sets up the data for the test.
void
ClientController::setUp() {

    // Initialize the data in the packet being sent.
    snd_buffer_.init(size_);

    // Now calculate the CRC check and put onto the end of the send buffer.
    uint32_t crc = Utilities::Crc(snd_buffer_.data(), snd_buffer_.dataSize());
    snd_buffer_.setCrc(crc);

    // Duplicate the template send buffer for use as the receive buffer.
    // The data areas will be separate.
    rcv_buffer_ = snd_buffer_.clone();

    // Initialize for counting packets (this initializes static counters).
    PacketCounter counter;

    return;
}
