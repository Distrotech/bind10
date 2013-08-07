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

#include <resolver/bench/landlord.h>

#include <util/threads/sync.h>
#include <util/threads/thread.h>

#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#include <boost/shared_ptr.hpp>

#include <vector>
#include <algorithm>

using std::vector;
using isc::util::thread::CondVar;
using isc::util::thread::Mutex;
using isc::util::thread::Thread;

namespace isc {
namespace resolver {
namespace bench {

template<class T, class Container>
class LandlordResolver::Queue {
public:
    // Constructor.
    Queue() :
        shutdown_(false)
    {}
    typedef T basetype;
    typedef Container Values;
    // Push single value (and wake up if something is waiting on pop()).
    void push(const T& value) {
        Values values;
        values.push_back(value);
        push(values);
    }
    // Push multiple values (and wake up if something is waiting on pop()).
    void push(const Values& values) {
        if (values.empty()) {
            return; // NOP
        }
        // Copy the data into the queue
        Mutex::Locker locker(mutex_);
        values_.insert(values_.end(), values.begin(), values.end());
        // Wake all threads waiting on new data
        condvar_.broadcast();
    }
    // Signal that we are expected to shutdown. This is out of band.
    void shutdown() {
        Mutex::Locker locker(mutex_);
        shutdown_ = true;
        condvar_.broadcast();
    }
    // Get some values from the queue. The values are put into where.
    // It gets maximum of max values, but there may be less. If there
    // are no values and wait is true, it blocks until some values appear.
    bool pop(Values &where, size_t max, bool wait = true) {
        Mutex::Locker locker(mutex_);
        while (values_.empty() && wait && !shutdown_) {
            condvar_.wait(mutex_);
        }
        size_t amount = std::min(max, values_.size());
        where.insert(where.end(), values_.begin(), values_.begin() + amount);
        values_.erase(values_.begin(), values_.begin() + amount);
        return !shutdown_;
    }
private:
    Values values_;
    Mutex mutex_;
    CondVar condvar_;
    bool shutdown_;
};

LandlordResolver::LandlordResolver(size_t count, size_t batch_size,
                                   size_t worker_count) :
    interface_(count),
    peasants_queue_(new FQueue),
    cache_queue_(new FQueue),
    send_queue_(new FQueue),
    upstream_queue_(new FQueue),
    read_batch_size_(batch_size),
    worker_batch_(batch_size),
    worker_count_(worker_count),
    total_count_(count),
    outstanding_(0)
{ }

// Destructor. Make sure it is created where we know how to destroy threads
// (not in the header file)
LandlordResolver::~LandlordResolver() {}

void
LandlordResolver::completedUpstream(FakeQueryPtr query) {
    outstanding_ -= 1;
    downstream_queue_.push_back(query);
}

bool
LandlordResolver::checkUpstream(bool block) {
    // Should we check for some incoming queries?
    // Only if we may block and if there are some expected.
    if (block && outstanding_ > 0) {
        interface_.processEvents();
        block = false; // We already blocked (possibly)
    }
    // Get the queries to be sent out and do so
    vector<FakeQueryPtr> out;
    out.reserve(worker_batch_);
    bool running = upstream_queue_->pop(out, worker_batch_, block);
    BOOST_FOREACH(const FakeQueryPtr& q, out) {
        assert(q->nextTask() == Upstream);
        q->performTask(boost::bind(&LandlordResolver::completedUpstream,
                                   this, q));
    }
    outstanding_ += out.size();
    if (out.size() == worker_batch_) {
        // We handled a full batch. There may be more, handle them now.
        return checkUpstream(false);
    }
    // Send all received queries to the workers for processing
    peasants_queue_->push(downstream_queue_);
    downstream_queue_.clear();
    // The result.
    return running || !out.empty();
}

void
LandlordResolver::read_iface() {
    vector<FakeQueryPtr> queries;
    queries.reserve(read_batch_size_);
    FakeQueryPtr q;
    while ((q = interface_.receiveQuery())) {
        queries.push_back(q);
        if (queries.size() >= read_batch_size_) {
            peasants_queue_->push(queries);
            queries.clear();
            queries.reserve(read_batch_size_);
        }
        checkUpstream(false);
    }
    peasants_queue_->push(queries);
    while (checkUpstream(true)) {}
}

namespace {

void stepDone(bool *flag) {
    *flag = true;
}

}

void
LandlordResolver::work() {
    vector<FakeQueryPtr> queries, writes, sends, upstreams;
    queries.reserve(worker_batch_);
    writes.reserve(worker_batch_);
    sends.reserve(worker_batch_);
    upstreams.reserve(worker_batch_);
    while (peasants_queue_->pop(queries, worker_batch_) || !queries.empty()) {
        BOOST_FOREACH(const FakeQueryPtr& q, queries) {
            while (true) {
                switch (q->nextTask()) {
                    case Compute: // Workers do computations
                    case CacheRead: {// With RCU, there'd be lock-less read, so workers can do that too.
                        bool done = false;
                        q->performTask(boost::bind(&stepDone, &done));
                        assert(done); // There should be nothing that should wait
                        assert(!q->done()); // Queries are done once Send is done, which is elsewhere
                        break; // Try next task
                    }
                    // These tasks are delegated to the landlords
                    case CacheWrite:
                        writes.push_back(q);
                        goto WORK_DONE;
                    case Send:
                        sends.push_back(q);
                        goto WORK_DONE;
                    case Upstream:
                        upstreams.push_back(q);
                        goto WORK_DONE;
                }
            }
            WORK_DONE:;
        }
        queries.clear();
        cache_queue_->push(writes);
        writes.clear();
        send_queue_->push(sends);
        sends.clear();
        upstream_queue_->push(upstreams);
        if (!upstreams.empty()) {
            // Wake up the interface main loop if it was sleeping.
            interface_.wakeup();
        }
        upstreams.clear();
    }
}

void
LandlordResolver::cache() {
    vector<FakeQueryPtr> queries;
    queries.reserve(worker_batch_);
    while (cache_queue_->pop(queries, worker_batch_) || !queries.empty()) {
        BOOST_FOREACH(const FakeQueryPtr& q, queries) {
            assert(q->nextTask() == CacheWrite);
            bool done = false;
            q->performTask(boost::bind(&stepDone, &done));
            assert(done); // Cache write is synchronous
        }
        // Put them all to the workers again to process
        peasants_queue_->push(queries);
        queries.clear();
    }
}

void
LandlordResolver::send() {
    vector<FakeQueryPtr> queries;
    queries.reserve(worker_batch_);
    size_t count = 0;
    while (count < total_count_) {
        send_queue_->pop(queries, worker_batch_);
        BOOST_FOREACH(const FakeQueryPtr& q, queries) {
            assert(q->nextTask() == Send);
            bool done = false;
            q->performTask(boost::bind(&stepDone, &done));
            assert(done); // Cache write is synchronous
            assert(q->done());
            count ++;
        }
        queries.clear();
    }
    // All was sent to the user, so shut down
    peasants_queue_->shutdown();
    cache_queue_->shutdown();
    send_queue_->shutdown();
    upstream_queue_->shutdown();
}

size_t
LandlordResolver::run() {
    vector<Thread *> threads;
    threads.push_back(
        new Thread(boost::bind(&LandlordResolver::read_iface, this)));
    for (size_t i = 0; i < worker_count_; ++i) {
        threads.push_back(new Thread(boost::bind(&LandlordResolver::work,
                                                 this)));
    }
    threads.push_back(new Thread(boost::bind(&LandlordResolver::cache, this)));
    threads.push_back(new Thread(boost::bind(&LandlordResolver::send, this)));

    // Wait for all threads to terminate
    BOOST_FOREACH(Thread *t, threads) {
        t->wait();
        delete t;
    }
    return (total_count_);
}

}
}
}
