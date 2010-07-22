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

#ifndef __DEFAULTS_H
#define __DEFAULTS_H

#include <stdint.h>
#include <string>

/// \brief Command-line defaults.
/// This files holds default values for the command-line options of the various
/// test programs.  They are placed in this header file so as to be in a single
/// place.

// Command-line defaults

static const std::string CL_DEF_ADDRESS = "127.0.0.1";
static const uint32_t CL_DEF_BURST = 1;
static const uint32_t CL_DEF_COUNT = 256;
static const uint32_t CL_DEF_DEBUG = 0;
static const std::string CL_DEF_LOGFILE = "";
static const uint32_t CL_DEF_LOST = 4;
static const uint32_t CL_DEF_OUTSTANDING = 8;
static const uint32_t CL_DEF_PERCENT = 100;
static const uint16_t CL_DEF_PKTSIZE = 8192;
static const uint16_t CL_DEF_PORT = 5400;

/// \brief UDP/IP defaults.

static const int SCKT_MAX_SND_SIZE = 1024 * 256;
static const int SCKT_MAX_RCV_SIZE = 1024 * 256;

/// \brief Miscellaneous

static const size_t UDP_BUFFER_SIZE = 65536 - 64;   // Allows for UDP overhead

/// \brief Message Queue Names

static const std::string QUEUE_UNPROCESSED_NAME = "rcp_unprocessed_queue";
static const std::string QUEUE_PROCESSED_NAME = "rcp_processed_queue";
static size_t QUEUE_MAX_MESSAGE_SIZE = UDP_BUFFER_SIZE;
static size_t QUEUE_MAX_MESSAGE_COUNT = 128;    // = 8MB total
static unsigned int QUEUE_PRIORITY = 0;         // Messages have this priority



#endif // __DEFAULTS_H
