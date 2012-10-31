// Copyright (C) 2011  Internet Systems Consortium, Inc. ("ISC")
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

#include <cryptolink.h>
#include <cryptolink/crypto_hmac.h>

#include <boost/scoped_ptr.hpp>

#include <openssl/hmac.h>

#include <cstring>

namespace {
const EVP_MD*
getOpenSSLHashAlgorithm(isc::cryptolink::HashAlgorithm algorithm) {
    switch (algorithm) {
    case isc::cryptolink::MD5:
        return (EVP_md5());
        break;
    case isc::cryptolink::SHA1:
        return (EVP_sha1());
        break;
    case isc::cryptolink::SHA256:
        return (EVP_sha256());
        break;
    case isc::cryptolink::SHA224:
        return (EVP_sha224());
        break;
    case isc::cryptolink::SHA384:
        return (EVP_sha384());
        break;
    case isc::cryptolink::SHA512:
        return (EVP_sha512());
        break;
    case isc::cryptolink::UNKNOWN_HASH:
        return (0);
        break;
    }
    // compiler should have prevented us to reach this, since we have
    // no default. But we need a return value anyway
    return (0);
}

template<typename T>
struct SecBuf {
public:
    typedef typename std::vector<T>::iterator iterator;

    typedef typename std::vector<T>::const_iterator const_iterator;

    explicit SecBuf() : vec_(std::vector<T>()) {}

    explicit SecBuf(size_t n, const T& value = T()) :
        vec_(std::vector<T>(n, value))
    {}

    SecBuf(iterator first, iterator last) :
        vec_(std::vector<T>(first, last))
    {}

    SecBuf(const_iterator first, const_iterator last) :
        vec_(std::vector<T>(first, last))
    {}

    SecBuf(const std::vector<T>& x) : vec_(x) {}

    ~SecBuf() {
        std::memset(&vec_[0], 0, vec_.capacity() * sizeof(T));
    };

    iterator begin() { return (vec_.begin()); };

    const_iterator begin() const { return (vec_.begin()); };

    iterator end() { return (vec_.end()); };

    const_iterator end() const { return (vec_.end()); };

    size_t size() const { return (vec_.size()); };

    void resize(size_t sz) { vec_.resize(sz); };

    SecBuf& operator=(const SecBuf& x) {
        if (&x != *this) {
            vec_ = x.vec_;
        }
        return (*this);
    };

    T& operator[](size_t n) { return (vec_[n]); };

    const T& operator[](size_t n) const { return (vec_[n]); };

private:
    std::vector<T> vec_;
};

} // local namespace

namespace isc {
namespace cryptolink {

class HMACImpl {
public:
    explicit HMACImpl(const void* secret, size_t secret_len,
                      const HashAlgorithm hash_algorithm) {
        const EVP_MD* algo = getOpenSSLHashAlgorithm(hash_algorithm);
        if (algo == 0) {
            isc_throw(UnsupportedAlgorithm,
                      "Unknown hash algorithm: " <<
                      static_cast<int>(hash_algorithm));
        }
        if (secret_len == 0) {
            isc_throw(BadKey, "Bad HMAC secret length: 0");
        }

        md_.reset(new HMAC_CTX);
        HMAC_CTX_init(md_.get());

        HMAC_Init_ex(md_.get(), secret,
                     static_cast<int>(secret_len),
                     algo, NULL);
    }

  ~HMACImpl() { if (md_) HMAC_CTX_cleanup(md_.get()); }

    size_t getOutputLength() const {
        int size = HMAC_size(md_.get());
        if (size < 0) {
            isc_throw(isc::cryptolink::LibraryError, "EVP_MD_CTX_size");
        }
        return (static_cast<size_t>(size));
    }

    void update(const void* data, const size_t len) {
        HMAC_Update(md_.get(), static_cast<const unsigned char*>(data), len);
    }

    void sign(isc::util::OutputBuffer& result, size_t len) {
        size_t size = getOutputLength();
        SecBuf<unsigned char> digest(size);
        HMAC_Final(md_.get(), &digest[0], NULL);
        if (len == 0 || len > size) {
            len = size;
        }
        result.writeData(&digest[0], len);
    }

    void sign(void* result, size_t len) {
        size_t size = getOutputLength();
        SecBuf<unsigned char> digest(size);
        HMAC_Final(md_.get(), &digest[0], NULL);
        if (len > size) {
            len = size;
        }
        std::memcpy(result, &digest[0], len);
    }

    std::vector<uint8_t> sign(size_t len) {
        size_t size = getOutputLength();
        SecBuf<unsigned char> digest(size);
        HMAC_Final(md_.get(), &digest[0], NULL);
        if (len != 0 && len < size) {
            digest.resize(len);
        }
        return (std::vector<uint8_t>(digest.begin(), digest.end()));
    }

    bool verify(const void* sig, size_t len) {
        size_t size = getOutputLength();
        if (len != 0 && len < size / 2) {
            return (false);
        }
        SecBuf<unsigned char> digest(size);
        HMAC_Final(md_.get(), &digest[0], NULL);
        if (len == 0 || len > size) {
            len = size;
        }
        return (std::memcmp(&digest[0], sig, len) == 0);
    }

private:
    boost::scoped_ptr<HMAC_CTX> md_;
};

HMAC::HMAC(const void* secret, size_t secret_length,
           const HashAlgorithm hash_algorithm)
{
    impl_ = new HMACImpl(secret, secret_length, hash_algorithm);
}

HMAC::~HMAC() {
    delete impl_;
}

size_t
HMAC::getOutputLength() const {
    return (impl_->getOutputLength());
}

void
HMAC::update(const void* data, const size_t len) {
    impl_->update(data, len);
}

void
HMAC::sign(isc::util::OutputBuffer& result, size_t len) {
    impl_->sign(result, len);
}

void
HMAC::sign(void* result, size_t len) {
    impl_->sign(result, len);
}

std::vector<uint8_t>
HMAC::sign(size_t len) {
    return impl_->sign(len);
}

bool
HMAC::verify(const void* sig, const size_t len) {
    return (impl_->verify(sig, len));
}

void
signHMAC(const void* data, const size_t data_len, const void* secret,
         size_t secret_len, const HashAlgorithm hash_algorithm,
         isc::util::OutputBuffer& result, size_t len)
{
    boost::scoped_ptr<HMAC> hmac(
        CryptoLink::getCryptoLink().createHMAC(secret,
                                               secret_len,
                                               hash_algorithm));
    hmac->update(data, data_len);
    hmac->sign(result, len);
}


bool
verifyHMAC(const void* data, const size_t data_len, const void* secret,
           size_t secret_len, const HashAlgorithm hash_algorithm,
           const void* sig, const size_t sig_len)
{
    boost::scoped_ptr<HMAC> hmac(
        CryptoLink::getCryptoLink().createHMAC(secret,
                                               secret_len,
                                               hash_algorithm));
    hmac->update(data, data_len);
    return (hmac->verify(sig, sig_len));
}

void
deleteHMAC(HMAC* hmac) {
    delete hmac;
}

} // namespace cryptolink
} // namespace isc
