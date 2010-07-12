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

#ifndef __WORKER_CONTROLLER_H
#define __WORKER_CONTROLLER_H

#include <stdint.h>
#include <list>

#include "communicator.h"
#include "controller.h"
#include "udp_buffer.h"

/// \brief Control the Action of the Worker
///
/// The target is a simple object with two threads, one for listening and
/// one for sending.  The listener will read up to "burst" packets (specified
/// on the command line) and place them in a queue.  It will then invoke
/// the sender.  The sender will send those packets back to the caller.
///
/// The listener stores packets on a private queue.  When the burst limit is
/// reached, it puts it on the senders queue and wakes up the sender if it
/// is paused.

class WorkerController : public Controller {
public:

    /// \brief Constructor
    ///
    /// \param burst Burst value.  The target will read this number of packets
    /// before passing them to the sender.
    WorkerController(uint32_t burst) : Controller(), burst_(burst), queue_()
        {}
    virtual ~WorkerController() {}

    /// \brief Runs the controller
    ///
    /// Loops reading burst_ packets, appending a checksum, then sending
    // them back.  It never returns.
    /// \param communicator Communicator to use for I/O
    virtual void run(Communicator& receptionist_communicator,
         Communicator& client_communicator);

private:
    uint32_t             burst_;    //< Burst limit
    std::list<UdpBuffer> queue_;    //< Queue of pending packets
};



#endif // __WORKER_CONTROLLER_H
