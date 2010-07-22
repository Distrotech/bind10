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

#ifndef __LOGGER_H
#define __LOGGER_H

#include <iostream>
#include <string>

#include <boost/date_time.hpp>

/// \brief Logs timing information.
///
/// The logger class is used to record timings etc. for the receptionist
/// tests.  After initialization, the methods start() and end() are used to
/// mark the start and end of tests, and the log() method used to write the
/// data to a file.
///
/// The format of this file is:
///
/// Start, End, Size, Count, Burst, Margin, Lost, Elapsed, Average
///
/// Where
/// - Start is the date/time at which the first packet was sent, in the format
///   YYYY-MM-DDTHH:MM:SS.ssss
/// - End is the date/time the last packet was received, in the same format.
/// - Size is the size of each data packet sent in bytes.
/// - Count is the total number of packets sent.
///
/// Then come three fields that indicate how the test was conducted.
/// - Burst is the number of packets the server must receive before
///   processing them (i.e. the server processes the packets in blocks of
///   "burst").  In the synchronous test, the client sends "burst" packets,
///   then waits to receive the same number before sending the next set.  The
///   use of this parameter in asynchronous tests is described in "Margin"
///   below.
/// - Margin is ignored in synchronous tests (and is output as zero).  In
///   asynchronous tests, up to "burst" packets are sent to the server before
///   the client *must* stop sending and await a packet in return.  This causes
///   problems if a packet is lost, e.g. if "burst" is set to 4, and the fourth
///   packet doesn't reach the server, the server will not do anything until
///   it receives another packet and the client will not send any more packets
///   until it receives one from the server.  "margin" is the number of packets
///   to account for this - the client may have up to "burst" + "margin"
///   packets outstanding before it must stop and await a reply from the server.
/// - Lost is the number of lost packets reported by the client.  Each packet
///   is numbered and sent in sequence; the client receive thread detects
///   missing sequence numbers and counts the number of lost packets.
///
///
/// The next two fields are derived fields:
/// - Elapsed is the difference between start and end times (in seconds).
/// - Average is the average elapsed time per packet, given by the total
///   elapsed time divided by the number of packets.

class Logger {
public:

    /// \brief Stores all parameters
    ///
    /// The constructor allows the storage of some of the parameters.
    /// \param logfile Name of the file to which the log is written.  If null,
    /// the output is written to stdout.
    /// \param count Number of packets sent in the test (default = 0).
    /// \param size Size of each packet (default = 0).
    /// \param burst Burst size of the packet (default = 0)
    /// \param comment Optional comment (default = no comment).
    Logger(const std::string& logfile = "", int count = 0, int size = 0,
        int burst = 0, int margin = 0) :
        logfile_(logfile), count_(count), size_(size), burst_(burst),
        margin_(margin), lost_(0), tstart_(), tend_()
    {}

    // Access methods.

    /// \brief Sets the output logfile.
    ///
    /// \param logfile Name of the logfile.  If the empty string, output will
    /// be written to stdout.
    void setLogfile(const std::string& logfile) {
        logfile_ = logfile;
    }

    /// \return Name of the current logfile
    std::string getLogfile() const {
        return (logfile_);
    }

    /// \brief Sets the count of packets in the test.
    ///
    /// \param count Number of packets sent in the test.
    void setCount(int count) {
        count_ = count;
    }

    /// \return Current record of count of packets to be sent
    int getCount() const {
        return (count_);
    }

    /// \brief Sets the packet size
    ///
    /// \param size Size of packets to be sent.
    void setSize(int size) {
        size_ = size;
    }

    /// \return Current record of packet size
    int getSize() const {
        return (size_);
    }

    /// \brief Sets the burst size
    ///
    /// \param burst Number of packets to be sent before server will
    /// take action on them.
    void setBurst(int burst) {
        burst_ = burst;
    }

    /// \return Current record of burst size
    int getBurst() const {
        return burst_;
    }

    /// \brief Sets the lost packet margin
    ///
    /// \param margin Number of packets in addition to "burst" that can be
    /// outstanding in asynchronous tests.
    void setMargin(int margin) {
        margin_ = margin;
    }

    /// \return Current record of margin
    int getMargin() const {
        return margin_;
    }

    /// \brief Sets the count of lost packets
    ///
    /// \param lost Number of packets lost in the test
    void setLost(int lost) {
        lost_ = lost;
    }

    /// \return Current vound of lost packets
    int getLost() const {
        return lost_;
    }
    /// \brief Logs the start time.
    void start() {
        tstart_ = boost::posix_time::microsec_clock::universal_time();
    }

    /// \brief Logs the end time.
    void end() {
        tend_ = boost::posix_time::microsec_clock::universal_time();
    }

    /// \brief Writes the log entry
    void log();

    /// \brief formats and writes the log entry
    ///
    /// log() opens the log file and calls this method to format and write
    /// the data.
    /// \param output Output stream on which the data is written.
    void logInfo(std::ostream& output);

private:
    std::string logfile_;                   //< Current log file
    int count_;                             //< Count of packets in this test
    int size_;                              //< Packet size
    int burst_;                             //< Burst size
    int margin_;                            //< Margin for lost packets
    int lost_;                              //< Count of lost packets
    boost::posix_time::ptime    tstart_;    //< Start time of test
    boost::posix_time::ptime    tend_;      //< End time of test
};

std::ostream& operator<<(std::ostream& output, Logger& logger);

#endif // __LOGGER_H
