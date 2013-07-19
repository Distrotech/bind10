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

#include <resolver/bench/scheduler.h>

#include <boost/bind.hpp>

namespace isc {
namespace resolver {
namespace bench {

namespace {

void
handleCoroutine(Coroutine* cor, size_t* outstanding);

void
resumeCoroutine(Coroutine* cor, size_t* outstanding)
{
    // The coroutine is ready to run again. So do so.
    (*cor)();
    // Coroutine returned to us again. Check if it is still alive.
    // This is not true recursion (with handleCoroutine) -- we call
    // handleCoroutine, but that one does not call us directly, it
    // schedules it trought the main loop. So the handleCoroutine
    // that scheduled us is no longer on stack, so there's no risk
    // of stack overflow.
    handleCoroutine(cor, outstanding);
}

void
handleCoroutine(Coroutine* cor, size_t* outstanding)
{
    if (*cor) {
        // The coroutine is still alive. That means it returned some
        // work to be done asynchronously, we need to schedule it.

        // The return value of get() is a functor to be called with
        // a callback to reschedule the coroutine once the work is done.
        cor->get()(boost::bind(&resumeCoroutine, cor, outstanding));
    } else {
        // The coroutine terminated, the query is handled.
        delete cor;
        --*outstanding;
    }
}

}

void
coroutineScheduler(const QueryHandler& handler, FakeInterface& interface) {
    FakeQueryPtr query;
    size_t outstanding = 0;
    // Receive the queries and create coroutines for them.
    while ((query = interface.receiveQuery())) {
        ++outstanding;
        // Create the coroutine and run the first part.
        Coroutine *cor = new Coroutine(boost::bind(handler, query, _1));
        // Returned from the coroutine.
        handleCoroutine(cor, &outstanding);
    }
    // Wait for all the queries to complete
    while (outstanding > 0) {
        interface.processEvents();
    }
}

}
}
}
