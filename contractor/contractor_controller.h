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

#ifndef __CONTRACTOR_CONTROLLER_H
#define __CONTRACTOR_CONTROLLER_H

#include <stdint.h>
#include <list>
#include <utility>

#include "communicator.h"
#include "controller.h"

/// \brief Contractor Controller
///
/// This controller interfaces with the intermediary process via a message
/// queue.  It reads "burst" packets from the intermediary, then forwards them
/// to the contractor.  It then reads the same number of packets from the
/// contractor before returning them to the controller.

class ContractorController : public Controller {
public:

    /// \brief Constructor
    ///
    /// \param burst Burst value.  The target will read this number of packets
    /// before passing them to the original sender.
    ContractorController(uint32_t burst) : Controller(), burst_(burst), queue_()
        {}
    virtual ~ContractorController() {}

    /// \brief Runs the controller
    ///
    /// Loops reading burst_ packets, appending a checksum, then sending
    // them back.  It never returns.
    /// \param downstream_communicator Used to communicate with the downstream
    /// process, in this case the intermediary.
    /// \param upstream_communicator Unused; the caller should pass in the same
    /// communicator as passed for the downstream communicator.
    virtual void run(Communicator& downstream_communicator,
        Communicator& upstream_communicator);

private:
    uint32_t             burst_;    //< Burst limit
    std::list<UdpBuffer> queue_;    //< Queue of pending packets
};

#endif // __CONTRACTOR_CONTROLLER_H
