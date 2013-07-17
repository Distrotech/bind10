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

#include <resolver/bench/coroutine_resolver.h>

#include <util/threads/thread.h>

#include <boost/foreach.hpp>
#include <boost/bind.hpp>

using isc::util::thread::Thread;

namespace isc {
namespace resolver {
namespace bench {

CoroutineResolver::CoroutineResolver(size_t count, size_t thread_count) :
    thread_count_(thread_count),
    // Due to rounding errors, we may handle slighly less queries. It will
    // not be significant, but we won't lie about how much we handled.
    total_count_(count / thread_count_ * thread_count_)
{
    for (size_t i = 0; i < thread_count; ++i) {
        interfaces_.push_back(new FakeInterface(count / thread_count));
    }
}

CoroutineResolver::~CoroutineResolver() {
    BOOST_FOREACH(FakeInterface* i, interfaces_) {
        delete i;
    }
}

void
CoroutineResolver::run_instance(FakeInterface*) {

}

size_t
CoroutineResolver::run() {
    std::vector<Thread*> threads;
    // Start a thread for each instance.
    BOOST_FOREACH(FakeInterface* i, interfaces_) {
        threads.push_back(
            new Thread(boost::bind(&CoroutineResolver::run_instance,
                                   this, i)));
    }
    // Wait for all the threads to finish.
    BOOST_FOREACH(Thread* t, threads) {
        t->wait();
        delete t;
    }
    return total_count_;
}

}
}
}
