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
#include <boost/coroutine/all.hpp>

using isc::util::thread::Thread;
using isc::util::thread::Mutex;

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

namespace {

// This is the callback passed to the AsyncWork. It'll be provided by
// the scheduler and it will restore the coroutine.
typedef boost::function<void()> SimpleCallback;
// A bit of asynchronous work to be performed. The coroutine will
// return this functor to the scheduler. The scheduler will execute
// it and pass it a callback that should be executed once the work
// is done, to reschedule.
typedef boost::function<void(SimpleCallback)> AsyncWork;
// A single coroutine, doing some work.
typedef boost::coroutines::coroutine<AsyncWork()> Coroutine;

void
doneTask(bool* flag) {
    *flag = true;
}

void
performUpstream(const FakeQueryPtr& query, Mutex* mutex,
                const SimpleCallback& callback)
{
    Mutex::Locker locker(*mutex);
    query->performTask(callback);
}

// Handle one query on the given interface. The cache is locked
// by the cache mutex (for write), sending of the upstream query
// by the upstream_mutex.
void handleQuery(FakeQueryPtr query, Mutex& cache_mutex, Mutex& upstream_mutex,
                 Coroutine::caller_type& scheduler)
{
    while (!query->done()) {
        switch (query->nextTask()) {
            case CacheWrite: {
                // We need to lock the cache when writing (but not for reading)
                Mutex::Locker locker(cache_mutex);
                bool done = false;
                query->performTask(boost::bind(&doneTask, &done));
                assert(done); // Write to cache is synchronous.
                break;
            }
            case Upstream:
                // Schedule sending to upstream and get resumed afterwards.

                // This could probably be done with nested boost::bind, but
                // that'd get close to unreadable, so we are more conservative
                // and use a function.
                scheduler(boost::bind(&performUpstream, query, &upstream_mutex,
                                      _1));
                // Good, answer returned now. Continue processing.
                break;
            default:
                // Nothing special here. We just perform the task.
                bool done = false;
                query->performTask(boost::bind(&doneTask, &done));
                assert(done);
                break;
        }
    }
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
