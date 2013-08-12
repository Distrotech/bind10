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

#ifndef RESOLVER_BENCH_LANDLORD_H
#define RESOLVER_BENCH_LANDLORD_H

#include <resolver/bench/fake_resolution.h>

#include <boost/scoped_ptr.hpp>

namespace isc {
namespace resolver {
namespace bench {

/// \brief Landlord implementation of the resolver
///
/// A shared resource is maintained by separate process with input and
/// output queues. Cache is RCU.
class LandlordResolver {
public:
    /// \brief Constructor. Initializes the data.
    LandlordResolver(size_t query_count, size_t batch_size,
                     size_t worker_count);
    ~ LandlordResolver();
    /// \brief Run the resolution.
    size_t run();
private:
    void read_iface();
    void work();
    void cache();
    void send();
    bool checkUpstream(bool block);
    void completedUpstream(FakeQueryPtr query);

    FakeInterface interface_;

    template<class T, class Container = std::vector<T> > class Queue;
    typedef Queue<FakeQueryPtr> FQueue;
    boost::scoped_ptr<FQueue> peasants_queue_, cache_queue_,
        send_queue_, upstream_queue_;
    std::vector<FakeQueryPtr> downstream_queue_;

    const size_t read_batch_size_, worker_batch_;
    const size_t worker_count_;
    const size_t total_count_;
    size_t outstanding_;
};

}
}
}

#endif
