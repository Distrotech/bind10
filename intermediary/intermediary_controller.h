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

#ifndef __INTERMEDIARY_CONTROLLER_H
#define __INTERMEDIARY_CONTROLLER_H

#include <stdint.h>

#include "communicator.h"
#include "controller.h"
#include "udp_buffer.h"

/// \brief Control the Action of the Intermediary
///
/// The intermediary receives packets from the client, appends UDP endpoint
/// information to them, and forwards them to the contractor process.  It
/// then reads packets back from the contractor, extracts the endpoint
/// information, and returns the packet to the client.
///
/// The intermediary acts in batches.  It reads "burst" packets from the
/// client before forwarding that number of packets to the processor.  Similarly
/// it receives that number of packets from the processor before returning them
/// to the client.

class IntermediaryController : public Controller {
public:

    /// \brief Constructor
    ///
    /// \param burst Burst value.  The intermediary will read this number of
    /// packets before passing them to the sender.
    IntermediaryController(uint32_t burst) : Controller(), burst_(burst)
        {}
    virtual ~IntermediaryController() {}

    /// \brief Runs the controller
    ///
    /// Loops reading burst_ packets, appending a checksum, then sending
    // them back.  It never returns.
    /// \param communicator Communicator to use for I/O to the client
    /// \param communicator Communicator to use for I/O to the processor
    virtual void run(Communicator& client_communicator,
        Communicator& processor_communicator);

private:
    uint32_t        burst_;             //< Burst limit
};



#endif // __INTERMEDIARY_CONTROLLER_H
