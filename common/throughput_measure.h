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

#ifndef __THROUGHPUT_MEASURE_H
#define __THROUGHPUT_MEASURE_H

#include <stdint.h>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/shared_ptr.hpp>

#include "packet_counter.h"

/// \brief Class for Measuring Throughput
///
/// This class is invoked by the intermediary/receptionist/server process and
/// on a regular basis prints the number of packets processed and time.
/// By steadily ramping up the number of queries (by invoking more clients),
/// a plateau will be reached which represents the maximum throughput.
///
/// In implementation, all that is needed is the instantiation of the class.
/// It sets up a separate thread that handles the output of the data.

class ThroughputMeasure {
public:

    /// Sets up a separate thread to print out time and packet counts.
    /// \param interval Output interval - how frequently the count is output.
    ThroughputMeasure(uint32_t interval);

    /// \brief Main Timer Loop
    ///
    /// Prints out the statistics every "interval" seconds to std::cout.
    /// This is a static method to allow its operation in a separate thread.
    /// \param measure Reference to the current object
    static void timerLoop(ThroughputMeasure& measure);

    /// \brief TerminateTimer
    ///
    /// Causes the timer to terminate.  Note that this waits for
    /// at least "interval" seconds (where "interval" is the value passed to
    /// the constructor) to guarantee that the timer thread has terminated.
    void terminateTimer() {
        exit_timer_ = true;
        sleep(interval_ + 1);
    }

private:
    uint32_t             interval_;     ///< Output interval
    boost::shared_ptr<boost::thread>  timer_thread_;
                                        ///< Output thread
    static volatile bool exit_timer_;   ///< true to cause timer loop to exit
};

#endif // __THROUGHPUT_MEASURE_H
