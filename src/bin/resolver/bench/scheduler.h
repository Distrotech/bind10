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

#ifndef RESOLVER_BENCH_SCHEDULER_H
#define RESOLVER_BENCH_SCHEDULER_H

#include <resolver/bench/fake_resolution.h>

#include <boost/function.hpp>
#include <boost/coroutine/all.hpp>

namespace isc {
namespace resolver {
namespace bench {

class FakeInterface;

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
// Function to handle one query
typedef boost::function<void(const FakeQueryPtr&, Coroutine::caller_type&)>
    QueryHandler;

// Run the coroutine scheduler, launch one coroutine on each query.
void
coroutineScheduler(const QueryHandler& handler, FakeInterface& interface);

}
}
}

#endif
