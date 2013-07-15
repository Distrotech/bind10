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

#include <resolver/bench/naive_resolver.h>

#include <util/threads/sync.h>

#include <vector>
#include <algorithm>

using std::vector;
using isc::util::thread::CondVar;
using isc::util::thread::Mutex;

namespace {

template<class T, class Container = std::vector<T> >
class Queue {
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
        // Copy the data into the queue
        Mutex::Locker locker(mutex_);
        values_.insert(values_.end(), values.begin(), values.end());
        // Wake all threads waiting on new data
        condvar_.broadcast();
    }
    // Signal that we are expected to shutdown. This is out of baund.
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

}

namespace isc {
namespace resolver {
namespace bench {

}
}
}
