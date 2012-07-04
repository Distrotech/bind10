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

// qid_gen defines a generator for query id's
//
// We probably want to merge this with the weighted random in the nsas
// (and other parts where we need randomness, perhaps another thing
// for a general libutil?)

#define ISC_LIBUTIL_EXPORT

#include <config.h>
#include <stdint.h>

#include <util/random/qid_gen.h>

#ifdef _WIN32
#include <time.h>
#else
#include <sys/time.h>
#endif

namespace isc {
namespace util {
namespace random {

QidGenerator qid_generator_instance;

QidGenerator&
QidGenerator::getInstance() {
    return (qid_generator_instance);
}

QidGenerator::QidGenerator() : dist_(0, 65535),
                               vgen_(generator_, dist_)
{
    seed();
}

void
QidGenerator::seed() {
#ifdef _WIN32
    FILETIME now;
    GetSystemTimeAsFileTime(&now);
    generator_.seed(now.dwLowDateTime + now.dwHighDateTime);
#else
    struct timeval tv;
    gettimeofday(&tv, 0);
    generator_.seed((tv.tv_sec * 1000000) + tv.tv_usec);
#endif
}

uint16_t
QidGenerator::generateQid() {
    return (vgen_());
}


} // namespace random
} // namespace util
} // namespace isc
