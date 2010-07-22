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

#ifndef __EXCEPTION_H
#define __EXCEPTION_H

#include <string>

#include <boost/lexical_cast.hpp>

/// \brief General exception class
///
/// Little more than a shell, this stores the reason for the exception and
/// provides a way to access it.  All programs in the receptionist test suite
/// use it unless there is a reason for a more complication exception
/// structure.

class Exception {
public:

    /// \brief Constructs an exception with no reason.
    Exception(const char* why = "") : reason_(why) {}

    /// \brief Constructs an exception giving a reason.
    /// \param why Reason for the exception.
    Exception(const std::string& why) : reason_(why) {}

    /// \brief Constructs an exception giving boost a reason and a parameter
    /// \param why Reason for the exception.
    /// \param cause Error parameter.  The string ": <parameter>" is appended
    /// to the reason before storing it.
    Exception(const std::string& why, const std::string& cause) : reason_(why) {
        reason_ += ": " + cause;
    }

    /// \brief Constructs an exception giving bost a reason and an error code.
    /// \param why Reason for the exception.
    /// \param errcode Error code.  The string ": error code <n>" is appended
    /// to the reason before storing it.
    Exception(const std::string& why, int errcode) : reason_(why) {
        reason_ += ": error code " + boost::lexical_cast<std::string>(errcode);
    }

    /// \brief Return the reason for the exception.
    /// \return Reason for the exception.
    virtual std::string getReason() const {
        return reason_;
    }

private:
    std::string     reason_;    //< Reason for the exception
};

/// \brief Exception thrown when an operation times out
/// 
/// Specialisation of Exception to flag that a timeout has occurred.

class Timeout : public Exception {
public:
    Timeout(const char* why = "Timeout has expired") : Exception(why)
    {}
};

/// \brief Exception thrown if a data packet is too large
///
/// Thrown when a data packet is constructed if the size requested is above
/// the maximum.

class PacketTooLarge : public Exception {
public:
    PacketTooLarge(const char* why = "Requested packet size is too large") :
        Exception(why)
    {}
};

#endif // __EXCEPTION_H
