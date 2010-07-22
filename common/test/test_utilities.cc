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

#include <gtest/gtest.h>
#include <boost/asio.hpp>

#include "udp_buffer.h"
#include "utilities.h"

namespace ip = boost::asio::ip;

// Test that the conversions can be performed successfully.

TEST(CommonTest, CopyBytes16bit) {

    // Determine what endian machine we are on
    union {
        uint16_t    word;
        uint8_t     bytes[2];
    } test;

    test.word = 1 + 256 * 2;
    bool little_endian = (test.bytes[0] == 1);

    // 16-bit tests: from word to bytes
    uint16_t shortword = test.word;
    uint8_t  bytes[2] = {0, 0};
    Utilities::CopyBytes(shortword, bytes);

    if (little_endian) {
        EXPECT_EQ(1, bytes[0]);
        EXPECT_EQ(2, bytes[1]);
    } else {
        EXPECT_EQ(1, bytes[1]);
        EXPECT_EQ(2, bytes[0]);
    }

    // ... and the reverse
    shortword = 0;
    Utilities::CopyBytes(bytes, shortword);
    EXPECT_EQ(test.word, shortword);

    return;
}

TEST(CommonTest, CopyBytes32bit) {

    // Determine what endian machine we are on
    union {
        uint32_t    word;
        uint8_t     bytes[2];
    } test;

    test.word = 1 + 256 * (2 + 256 * (3 + 256 * (4)));
    bool little_endian = (test.bytes[0] == 1);

    // 32-bit tests: from word to bytes
    uint32_t longword = test.word;
    uint8_t  bytes[4] = {0, 0, 0, 0};
    Utilities::CopyBytes(longword, bytes);

    if (little_endian) {
        EXPECT_EQ(1, bytes[0]);
        EXPECT_EQ(2, bytes[1]);
        EXPECT_EQ(3, bytes[2]);
        EXPECT_EQ(4, bytes[3]);
    } else {
        EXPECT_EQ(1, bytes[3]);
        EXPECT_EQ(2, bytes[2]);
        EXPECT_EQ(3, bytes[1]);
        EXPECT_EQ(4, bytes[0]);
    }

    // ... and the reverse
    longword = 0;
    Utilities::CopyBytes(bytes, longword);
    EXPECT_EQ(test.word, longword);

    return;
}
