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

#include <resolver/bench/layers.h>
#include <resolver/bench/subprocess.h>

#include <boost/bind.hpp>

#include <sys/types.h>
#include <sys/socket.h>

namespace isc {
namespace resolver {
namespace bench {

LayerResolver::LayerResolver(size_t count, size_t worker_count,
                             size_t fanout) :
    total_count_(count),
    top(new Subprocess(boost::bind(&LayerResolver::spawn, this, count,
                                   worker_count, fanout, _1)))
{
    // Wait for the subprocess to become ready
    const std::string& ready = top->read(1);
    assert(ready == "R");
}

LayerResolver::~LayerResolver() {
    delete top;
}

void
LayerResolver::worker(size_t count, int channel) {
    FakeInterface interface(count);
    // We are ready
    ssize_t result = send(channel, "R", 1, 0);
    assert(result == 1);
    // Wait for the run signal
    char buffer;
    result = recv(channel, &buffer, 1, 0);
    assert(result == 1);
    assert(buffer == 'R');

    // TODO: Do the work
    // Signal we are finished
    result = send(channel, "F", 1, 0);
    assert(result == 1);
}

namespace {

// Single child subprocess. May be a smaller cache or a worker.
// We don't really care about private/public here, this is to make
// it hold together.
class Child : boost::noncopyable {
public:
    Child(const boost::function<void(int)>& main) :
        subprocess_(main)
    {}
    Subprocess subprocess_;
};

}

void
LayerResolver::spawn(size_t count, size_t worker_count, size_t fanout,
                     int channel)
{
    if (worker_count == 1) {
        // We are the single worker
        worker(count, channel);
    } else {
        if (fanout > worker_count) {
            // Just correction, so we don't spawn more children than needed
            fanout = worker_count;
        }
        std::vector<Child*> children;
        // How much of these was satisfied
        size_t count_handled = 0;
        size_t workers_handled = 0;
        for (size_t i = 0; i < fanout; ++i) {
            // How many to assign to this child
            size_t count_local = (count * (i+1)) / fanout - count_handled;
            size_t workers_local = (count * (i+1)) / fanout - workers_handled;
            // Initialize the child and make sure it is ready
            Child* child = new Child(boost::bind(&LayerResolver::spawn, this,
                                                 count_local, workers_local,
                                                 fanout, _1));
            children.push_back(child);
            const std::string& ready = child->subprocess_.read(1);
            assert(ready == "R");

            count_handled += count_local;
            workers_handled += workers_local;
        }
        // All the children started, we are ready. Signal it and wait for
        // signal to launch.
        ssize_t result = send(channel, "R", 1, 0);
        assert(result == 1);
        char buffer;
        result = recv(channel, &buffer, 1, 0);
        assert(result == 1);
        assert(buffer == 'R');
    }
}

size_t
LayerResolver::run() {
    // Send "Run" signal
    top->send("R", 1);
    // Wait for "Finished" signal
    const std::string& response = top->read(1);
    assert(response == "F");
    return (total_count_);
}

}
}
}
