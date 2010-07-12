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

#ifndef __SERVER_CONTROLLER_H
#define __SERVER_CONTROLLER_H

#include <stdint.h>
#include <list>
#include <utility>

#include "controller.h"
#include "communicator.h"

/// \brief Server Controller
///
/// This is a controller for the server process.  The run() method (which never
/// returns) loops, reading a set of packets from the input stream, processing
/// them, and writing them to the output stream.  The size of the packet is set
/// by the "burst" parameter, passed to the constructor.
///
/// The processing of packets in this instance involves appending a checksum
/// to the data.

class ServerController : public Controller {
public:

    /// \brief Constructor
    ///
    /// \param burst Burst value.  The server will read this number of packets
    /// before passing them to the sender.
    ServerController(uint32_t burst) : Controller(), burst_(burst), queue_()
        {}
    virtual ~ServerController() {}

    /// \brief Runs the controller
    ///
    /// Loops reading burst_ packets, appending a checksum, then sending
    // them back.  It never returns.
    /// \param downstream_communicator Communicator used to communicate
    /// with the downstream process (in this case the client).
    /// \param upstream_communicator Communicator used to communicate with
    /// the upstream process.  In this case there is no upstream process and
    /// it is unsed.  The caller should pass in the same communicator as
    /// the downstream process one.
    virtual void run(Communicator& downstream_communicator,
         Communicator& upstream_communicator);

private:
    uint32_t        burst_;             //< Burst limit
    std::list<UdpBuffer> queue_;        //< Send/receive queue
};



#endif // __SERVER_CONTROLLER_H
