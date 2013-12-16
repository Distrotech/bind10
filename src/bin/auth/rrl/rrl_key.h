// Copyright (C) 2013  Internet Systems Consortium, Inc. ("ISC")
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

#ifndef AUTH_RRL_KEY_H
#define AUTH_RRL_KEY_H 1

#include <auth/rrl/rrl_response_type.h>

#include <dns/labelsequence.h>
#include <dns/rrtype.h>
#include <dns/rrclass.h>

#include <boost/functional/hash.hpp>

#include <cstring>

#include <stdint.h>

namespace isc {
namespace asiolink {
class IOEndpoint;
}

namespace auth {
namespace rrl {
namespace detail {

/// \brief Lookup key into the RRL table.
///
/// This class computes a lookup key into an RRL table for a DNS
/// response using the client's IP address, the QNAME, QTYPE, QCLASS and
/// type of response (success, NXDOMAIN or some error). The client's IP
/// address is used after masking off suffix bits so that only initial
/// IPV4-PREFIX-LENGTH or IPV6-PREFIX-LENGTH bits are used for lookup
/// key computation. IPV6-PREFIX-LENGTH allows a maximum value of 64
/// (which should approximate most IPv6 victim target networks).
///
/// Implementation note: For comparisons, assignment, etc. this class
/// does direct object-size \c memcpy(), memcmp(), etc.
class RRLKey {
public:
    /// \brief For requirements of STL containers.  We don't directly
    /// use keys constructed by this.
    RRLKey() {}

    /// \brief Constructor
    ///
    /// Note: Because other methods of this class do memory operations
    /// on this object's allocation directly, all of the object's
    /// allocation must be initialized by this constructor.
    ///
    /// \param client_address The address of the client. This is masked
    /// using the \c ipv4_mask or \c ipv6_masks fields before key
    /// computation. Note that only a maximum of 64 bits are allowed
    /// in the client address prefix (see the class description).
    /// \param qtype \c RRType corresponding to the QTYPE field.
    /// \param qname \c LabelSequence corresponding to the QNAME field.
    /// \param qclass \c RRClass corresponding to the QCLASS field.
    /// \param resp_type Type of response that was generated for the query.
    /// \param ipv4_mask IPv4 address mask for \c client_address.
    /// \param ipv6_masks IPv6 address mask for \c client_address.
    /// \param hash_seed A seed used in key computation.
    RRLKey(const asiolink::IOEndpoint& client_addr, const dns::RRType& qtype,
           const dns::LabelSequence* qname, const dns::RRClass& qclass,
           ResponseType resp_type, uint32_t ipv4_mask,
           const uint32_t ipv6_masks[4], uint32_t hash_seed);

    /// \brief Assignment operator.
    ///
    /// Note: This does a \c memcpy() of the entire object's allocation
    /// region. For this reason, the constructor must not leave any of
    /// the memory uninitialized.
    RRLKey& operator=(const RRLKey& source) {
        std::memcpy(this, &source, sizeof(*this));
        return (*this);
    }

    /// \brief Equals operator.
    ///
    /// Note: This does a \c memcmp() of the entire object's allocation
    /// region. For this reason, the constructor must not leave any of
    /// the memory uninitialized.
    bool operator==(const RRLKey& other) const {
        return (std::memcmp(this, &other, sizeof(*this)) == 0);
    }

    /// \brief Returns the hash value for this \c RRLKey.
    size_t getHash() const {
        const uint8_t* cp = static_cast<const uint8_t*>(
            static_cast<const void*>(this));
        return (boost::hash_range(cp, cp + sizeof(*this)));
    }

    /// \brief Returns the response type (success, NXDOMAIN or some
    /// error)
    ResponseType getResponseType() const { return (rtype_); }

private:
    uint32_t ip_[2];            // client IP prefix, up to 64 bits
    uint32_t qname_hash_;
    uint16_t qtype_;            // qtype code value
    uint8_t ipv6_ : 1;          // used for logging
    uint8_t qclass_ : 7;        // least 7 bits of qclass code value
    ResponseType rtype_;
};

} // namespace detail
} // namespace rrl
} // namespace auth
} // namespace isc

#endif // AUTH_RRL_KEY_H

// Local Variables:
// mode: c++
// End:
