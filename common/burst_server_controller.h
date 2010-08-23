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

#ifndef __BURST_SERVER_CONTROLLER_H
#define __BURST_SERVER_CONTROLLER_H

#include <stdint.h>
#include <list>
#include <utility>

#include "controller.h"
#include "communicator.h"
#include "udp_buffer.h"

/// \brief Burst Server Controller
///
/// This is a controller for the server processes.  The run() method (which
/// never returns) loops, reading a set of packets from the input stream and
/// adding them to a queue.  When the requisite number of packets have been
/// reached (the burst parameter), it calculates the CRCs and adds that to
/// the packet, then it sends all the packets to the outpur stream.

class BurstServerController : public Controller {
public:

    /// \brief Constructor
    ///
    /// \param burst Burst value.  The server will read this number of packets
    /// before passing them to the sender.
    BurstServerController(uint32_t burst, uint32_t memsize) : Controller(),
        burst_(burst), memsize_(memsize)
        {}
    virtual ~BurstServerController() {}

    /// \brief Runs the controller
    ///
    /// Loops reading burst_ packets, appending a checksum, then sending
    // them back.  It never returns.
    /// \param receive_communicator Communicator used to communicate
    /// with the process sending this server packets.
    /// \param send_communicator Communicator used to communicate with
    /// the upstream process (to when packets are sent after processing).
    /// This may be the same as the receive communicator.
    virtual void run(Communicator& receive_communicator,
         Communicator& send_communicator);

private:
    uint32_t        burst_;             //< Burst limit
    uint32_t        memsize_;           //< Memory size

    /// \brief Touches Memory
    ///
    /// Runs over the memory specified and modifes every 1024th location.  In
    /// this way all memory is pulled into the working set (and hence cache).
    /// This is a way of checking how memory size affects the scheduling
    /// performance.
    ///
    /// The action is put into an external function to ensure that it is not
    /// optimised away.
    ///
    /// \param memory Pointer to allocated memory
    /// \param size Amount of memory allocated in kB.
    void touchMemory(char* memory, uint32_t size);
};



#endif // __BURST_SERVER_CONTROLLER_H
