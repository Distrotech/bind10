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
#include <fstream>

#include <boost/algorithm/string.hpp>

#include "logger.h"


// Logs information to the logfile.  This method just handles file access
// before calling logInfo to do the work.
void
Logger::log() {
    if (logfile_.length() == 0) {

        // Null string, so log to stdout.
        logInfo(std::cout);
    }
    else {

        // Open the file.
        std::ofstream logging;
        logging.open(logfile_.c_str(), std::ios::out | std::ios::app);

        // Log the information
        logInfo(logging);

        // ... and tidy up
        logging.close();
    }

    return;
}


// Logs the information to the current file log file.
void
Logger::logInfo(std::ostream& output)
{
    output << boost::posix_time::to_iso_extended_string(tstart_)
        << "," << boost::posix_time::to_iso_extended_string(tend_)
        << "," << count_ << "," << size_
        << "," << burst_ << "," << margin_ << "," << lost_;

    // Calculate the difference in time and print interval and time per
    // packet.  (Note that as used, this is the time to receive count_ packets;
    // if "lost" is greater than zero, more that count_ packets will have been
    // sent to get to this figure.)
    boost::posix_time::time_duration interval = tend_ - tstart_;
    double dinterval = interval.total_seconds() +
        (interval.total_microseconds() / 1.0e6);

    output << "," << dinterval << "," << (dinterval / count_) << "\n";

    return;
}

// Stream output for the logger

std::ostream& operator<<(std::ostream& output, Logger& logger) {
    logger.logInfo(output);
    return output;
}
