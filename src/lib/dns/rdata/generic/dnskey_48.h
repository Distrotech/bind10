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

#include <stdint.h>

#include <string>

#include <dns/name.h>
#include <dns/rrtype.h>
#include <dns/rrttl.h>
#include <dns/rdata.h>
#include <dns/master_lexer.h>

// BEGIN_HEADER_GUARD

// BEGIN_BUNDY_NAMESPACE

// BEGIN_COMMON_DECLARATIONS
// END_COMMON_DECLARATIONS

// BEGIN_RDATA_NAMESPACE

struct DNSKEYImpl;

class DNSKEY : public Rdata {
public:
    // BEGIN_COMMON_MEMBERS
    // END_COMMON_MEMBERS
    DNSKEY& operator=(const DNSKEY& source);
    ~DNSKEY();

    ///
    /// Specialized methods
    ///

    /// \brief Returns the key tag
    ///
    /// \throw bundy::OutOfRange if the key data for RSA/MD5 is too short
    /// to support tag extraction.
    uint16_t getTag() const;

    uint16_t getFlags() const;
    uint8_t getAlgorithm() const;

private:
    DNSKEYImpl* constructFromLexer(bundy::dns::MasterLexer& lexer);

    DNSKEYImpl* impl_;
};

// END_RDATA_NAMESPACE
// END_BUNDY_NAMESPACE
// END_HEADER_GUARD

// Local Variables: 
// mode: c++
// End: 
