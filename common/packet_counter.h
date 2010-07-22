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

#ifndef __PACKET_COUNTER_H
#define __PACKET_COUNTER_H

#include <stdlib.h>

#include <csignal>
#include <iostream>
#include <vector>

/// \brief Counts Packets
///
/// Keeps the count of packets received and sent.  Although a fairly trivial
/// function, this class also declares an exit handler to print the counts
/// when the program exits via a signal.

class PacketCounter {
public:
    /// \brief Constructor
    ///
    /// Provided as a convenience for ease of use, this will register the
    /// signal handler if not already registered and reset the counts to 0.
    PacketCounter() {
        if (! handler_registered_) {

            // Register the exit handler and SIGTERM handler.
            signal(SIGTERM, sigtermHandler);
            handler_registered_ = true;
        }
        send_count_ = receive_count_ = 0;
        send2_count_ = receive2_count_ = 0;
    }

    /// \brief Increments Send Counter
    ///
    /// Adds 1 to the "send operation" counter and returns the new value.
    static int incrementSend() {
        return ++send_count_;
    }

    /// \brief Increments Send Counter
    ///
    /// Adds 1 to the "send operation 2" counter and returns the new value.
    static int incrementSend2() {
        return ++send2_count_;
    }

    /// \brief Increments Receive Counter
    ///
    /// Adds 1 to the "receive operation" counter.
    static int incrementReceive() {
        return ++receive_count_;
    }

    /// \brief Increments Receive Counter
    ///
    /// Adds 1 to the "receive operation 2" counter.
    static int incrementReceive2() {
        return ++receive2_count_;
    }

    /// \brief SIGTERM Handler
    ///
    /// This is the SIGTERM handler which prints out the statistics and
    /// then exits the program.
    static void sigtermHandler(int sig) {
        std::cout << "send() calls:        " << send_count_ << "\n"
                  << "receive() calls:     " << receive_count_ << "\n";

        // Print out additional counters only if either is greater than
        // zero (simplifies output for programs that don't use them).
        if (send2_count_ or receive2_count_) {
            std::cout << "send() calls (2):    " << send2_count_ << "\n"
                      << "receive() calls (2): " << receive2_count_ << "\n";
        }
        exit(0);
    }

private:
    static int  send_count_;        //< Count of packets sent
    static int  receive_count_;     //< Count of packets received
    static int  send2_count_;       //< Second count of packets sent
    static int  receive2_count_;    //< Second count of packets received
    static bool handler_registered_;//< Set true when handlers are registered
};

#endif // __PACKET_COUNTER_H
