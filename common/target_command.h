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

#ifndef __TARGET_COMMAND_H
#define __TARGET_COMMAND_H

/// \brief Client Command Line Parsing.
///
/// This class takes care of parsing the command line for the target program
/// of the reception tests.  Regardless of how the target is invoked, the
/// options are the same.  Internally the class is just a wrapper around the
/// boost::program_options library.

#include <stdint.h>

#include <string>
#include <iostream>

#include <boost/program_options.hpp>

class TargetCommand {
public:

    /// \brief Parses the command-line arguments.
    ///
    /// \param argc Size of the command-line argument array.
    /// \param argv Command-line argument array.
    ///
    /// \exception Exception thrown if the parsing fails (e.g. invalid port
    /// number).
    TargetCommand(int argc, char** argv);

    /// \brief Prints usage information to the specified stream.
    /// \param output Output stream to which usage should be printed.
    void usage(std::ostream& output) const;

    // Accessor methods

    bool getHelp() const {
        return vm_.count("help");
    }

    /// \return The port on the target to which to connect.
    uint16_t getPort() const {
        return port_;
    }

    /// \return The burst size, the number of packets the target processes at
    /// a time.
    uint32_t getBurst() const {
        return burst_;
    }

    /// \return Current setting of debug flag
    uint32_t getDebug() const {
        return debug_;
    }

    /// \return Memory size (in kB)
    uint32_t getMemorySize() const {
        return memsize_;
    }

    /// \return The queue number for receiving information from the client
    uint32_t getQueue() const {
        return queue_;
    }

    /// \return File spec of the worker to run. A number of worker
    /// processes equal to the queue count are started
    std::string getWorker() const {
        return worker_;
    }

private:
    /// \brief Parse command line.
    /// Sets up all the command-line options (and their defaults) and parses
    /// the command line.
    ///
    /// \param argc Size of the command-line argument array.
    /// \param argv Command-line argument array.
    /// \param vm BOOST program options variable map.  On exit this will hold
    /// information from the parsed command line.
    void parseCommandLine(int argc, char** argv);

private:
    uint32_t        debug_;     //< Debug value
    uint16_t        port_;      //< Port number on target to which to connect
    uint32_t        queue_;     //< Queue number on which to receive data
    uint32_t        burst_;     //< Burst size (for processing)
    uint32_t        memsize_;   //< Memory size (in kB)
    std::string     worker_;    //< Worker process
    boost::program_options::options_description desc_; //< Options description structure
    boost::program_options::variables_map vm_;         //< Maps variables to values
};

#endif // __TARGET_COMMAND_H
