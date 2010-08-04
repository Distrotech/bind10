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

#ifndef __DEBUG_FLAGS_H
#define __DEBUG_FLAGS_H

/// \brief Flags for Debugging
///
/// Each flag is a bit pattern, and operations take place if the specified
/// bit is set.  This is a separate class to avoig having the class that
/// provides debug functionality knowing about the rest of the program.
/// That knowledge is placed in this (very simple) class.

class DebugFlags {
public:
    enum {
        LOG_OPERATION = 0x0001,     ///< Log each send/receive operation
        PRINT_IP = 0x0002,          ///< Print IP information
        LOST_PACKETS = 0x0004       ///< Log lost packets
    };
};

#endif // __DEBUG_FLAGS_H
