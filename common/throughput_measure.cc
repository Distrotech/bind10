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

#include <iostream>
#include <iomanip>
#include <boost/date_time.hpp>
#include "throughput_measure.h"

volatile bool ThroughputMeasure::exit_timer_ = false;

/// Constructor.

ThroughputMeasure::ThroughputMeasure(uint32_t interval) : interval_(interval)
{
    // Start the thread in the background, but only if the interval is
    // greater than zero.
    if (interval_ > 0) {
        timer_thread_ = boost::shared_ptr<boost::thread>(
           new boost::thread(ThroughputMeasure::timerLoop, boost::ref(*this)));
    }
}

/// Main timer loop

void ThroughputMeasure::timerLoop(ThroughputMeasure& measure) {

    // Store the last count and time.

    boost::posix_time::ptime last_time =    // Last time loop ran
        boost::posix_time::microsec_clock::universal_time();
    boost::posix_time::ptime current_time = last_time;
                                            // This time loop ran
    unsigned long last_count = PacketCounter::getReceiveCount();
                                            // Previous count of packets
    unsigned long current_count = last_count; // Current count of packets
    boost::asio::io_service io_;            // I/O Service object
    boost::asio::deadline_timer timer(io_); // Timer object

    // Now loop, waiting the delay, printing statistics and repeating.
    while (! exit_timer_) {
        timer.expires_from_now(boost::posix_time::seconds(measure.interval_));
        timer.wait();

        // Timer expired, get the stats.
        current_time = boost::posix_time::microsec_clock::universal_time();
        current_count = PacketCounter::getReceiveCount();

        // Print the statistics
        boost::posix_time::time_duration interval = current_time - last_time;
        double dinterval = interval.total_microseconds() / 1.0e6;
        double dcount = static_cast<double>(current_count - last_count);
        std::cout << boost::posix_time::to_iso_extended_string(current_time)
            << "," << current_count
            << "," << dinterval
            << "," << (current_count - last_count)
            << "," << (dcount / dinterval)
            << std::endl;


        // Store the current time in the previous entry
        last_time = current_time;
        last_count = current_count;
    }
}
