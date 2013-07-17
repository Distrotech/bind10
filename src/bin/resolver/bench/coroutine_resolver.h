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

#ifndef RESOLVER_BENCH_COROUTINE_H
#define RESOLVER_BENCH_COROUTINE_H

#include <resolver/bench/fake_resolution.h>

#include <boost/noncopyable.hpp>

#include <vector>

namespace isc {
namespace resolver {
namespace bench {

/// \brief Implementation of the resolver using coroutines.
///
/// Paralelism of waiting for upstream queries is done by executing
/// coroutines. The cache is RCU-based.
class CoroutineResolver : boost::noncopyable {
public:
    /// \brief Constructor.
    ///
    /// Initializes the interfaces and queries in them.
    CoroutineResolver(size_t query_count, size_t thread_count);
    /// \brief Destructor
    ~CoroutineResolver();
    /// \brief Run the benchmark.
    size_t run();
private:
    // Run one thread on given interface.
    void run_instance(FakeInterface* interface);
    std::vector<FakeInterface*> interfaces_;
    const size_t thread_count_;
    const size_t total_count_;
};

}
}
}

#endif
