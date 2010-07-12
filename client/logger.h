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
/// Start, End, Size, Count, Burst, Elapsed, Average [, "Comment"]
///
/// Where
/// - Start is the date/time at which the first packet was sent, in the format
///   YYYY-MM-DDTHH:MM:SS.ssss
/// - End is the date/time the last packet was received, in the same format.
/// - Size is the size of each data packet sent in bytes.
/// - Count is the total number of packets sent.
/// - Burst is the number of packets the server must receive before
///   processing them (i.e. the server processes the packets in blocks of
///   "burst").
///
/// The next two fields are derived fields:
/// - Elapsed is the difference between start and end times (in seconds).
/// - Average is the average elapsed time per packet, given by the total
///   elapsed time divided by the number of packets.
///
/// Finally there is an optional field:
/// - Comment is a comment associated with the entry.  The comment is written
///   surrounted by double-quate characters, with any embedded double-quote
///   characters doubled. (e.g. the string abc"def is written as "abc""def".)

class Logger {
public:

    /// \brief Stores all parameters
    ///
    /// The constructor allows the storage of some of the parameters.
    /// \param logfile Name of the file to which the log is written.  If null,
    /// the output is written to stdout.
    /// \param count Number of packets sent in the test (default = 0).
    /// \param pktsize Size of each packet (default = 0).
    /// \param burst Burst size of the packet (default = 0)
    /// \param comment Optional comment (default = no comment).
    Logger(const std::string& logfile = "", int count = 0, int pktsize = 0,
        int burst = 0, const std::string& comment = "") :
        logfile_(logfile), count_(count), pktsize_(pktsize),
        burst_(burst), comment_(comment), tstart_(), tend_()
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
    /// \param pktsize Size of packets to be sent.
    void setPktsize(int pktsize) {
        pktsize_ = pktsize;
    }

    /// \return Current record of packet size
    int getPktsize() const {
        return (pktsize_);
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

    /// \brief Set comment associated with the test
    ///
    /// \param comment Comment to be associated with the text.  Any embedded
    /// quotes are doubled, and the comment will be written with surrounding
    /// quotes.  Set to the empty string ("") to disable commenting.
    void setComment(const std::string& comment) {
        comment_ = comment;
    }

    /// \return Current comment to be associated with the test
    std::string getComment() const {
        return (comment_);
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

private:

    /// \brief formats and writes the log entry
    ///
    /// log() opens the log file and calls this method to format and write
    /// the data.
    /// \param output Output stream on which the data is written.
    void logInfo(std::ostream& output);

private:
    std::string logfile_;                   //< Current log file
    int count_;                             //< Count of packets in this test
    int pktsize_;                           //< Packet size
    int burst_;                             //< Burst size
    std::string comment_;                   //< Optional comment
    boost::posix_time::ptime    tstart_;    //< Start time of test
    boost::posix_time::ptime    tend_;      //< End time of test
};

#endif // __LOGGER_H
