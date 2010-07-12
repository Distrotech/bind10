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

#ifndef __CLIENT_COMMAND_H
#define __CLIENT_COMMAND_H

/// \brief Client Command Line Parsing.
///
/// This class takes care of parsing the command line for the receptionist
/// client program.  Options are parsed, defaults substituted, and the values
/// made available.  Internally the class is just a wrapper aroung the
/// boost::program_options library.

#include <stdint.h>

#include <string>
#include <iostream>

#include <boost/program_options.hpp>

class ClientCommand {
public:

    /// \brief Parses the command-line arguments.
    ///
    /// \param argc Size of the command-line argument array.
    /// \param argv Command-line argument array.
    ///
    /// \exception Exception thrown if the parsing fails (e.g. invalid port
    /// number).
    ClientCommand(int argc, char** argv);

    /// \brief Prints usage information to the specified stream.
    /// \param output Output stream to which usage should be printed.
    void usage(std::ostream& output) const;

    // Accessor methods

    /// \return The IP address of the server.
    std::string getAddress() const {
        return address_;
    }

    /// \return The burst size.  This is not used in the client, but is reported
    /// in the log file - it is a way for server parameters to be records.
    uint32_t getBurst() {
        return burst_;
    }

    /// \return The total number of packets to send in a run.
    uint32_t getCount() const {
        return count_;
    }

    /// \return Whether or not help option was given
    bool getHelp() const {
        return vm_.count("help");
    }
        

    /// \return The name of the log file.
    std::string getLogfile() const {
        return logfile_;
    }

    /// \return The size of the packets to send.
    uint16_t getPktsize() const {
        return pktsize_;
    }

    /// \return The port on the server to which to connect.
    uint16_t getPort() const {
        return port_;
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
    std::string     address_;   //< IP address of the server
    uint32_t        burst_;     //< Burst size (for record only)
    uint32_t        count_;     //< Total number of packets to send
    std::string     logfile_;   //< Name of the log file
    uint16_t        pktsize_;   //< Maximum size of each packet
    uint16_t        port_;      //< Port number on server to which to connect
    boost::program_options::options_description desc_; //< Options description structure
    boost::program_options::variables_map vm_;         //< Maps variables to values
};



#endif // __CLIENT_COMMAND_H
