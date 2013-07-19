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
#include <resolver/bench/dummy_work.h>
#include <resolver/bench/scheduler.h>

#include <asiolink/local_socket.h>

#include <boost/bind.hpp>
#include <boost/foreach.hpp>

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

namespace {

struct Command {
    size_t levels;
    size_t workload;
};

void
sendQuery(const FakeQueryPtr& query, int cache_comm, int cache_depth) {
    if (cache_depth == 0) {
        // No caches to send to, ignore
        return;
    }
    // Fill in the command
    Command command;
    command.levels = cache_depth;
    command.workload = query->nextTaskSize();
    // Prepare the buffer
    uint8_t buffer[sizeof command + 1];
    buffer[0] = 'W';
    memcpy(buffer + 1, &command, sizeof command);
    // A blocking write, please. We assume it is written at one go.
    // If that proved false, the below assert would catch it and we
    // would then fix the code.
    ssize_t result = send(cache_comm, buffer, sizeof buffer, 0);
    assert(result == sizeof buffer);
}

void
doneTask(bool* done) {
    *done = true;
}

// Handle one query
void
handleQuery(FakeQueryPtr query, Coroutine::caller_type& scheduler,
            int cache_comm, size_t cache_depth)
{
    while (!query->done()) {
        Task task = query->nextTask();
        switch (task) {
            case CacheWrite:
            case CacheRead:
            {
                // Send to the upper layers, if any
                sendQuery(query, cache_comm,
                          task == CacheWrite ? cache_depth :
                          query->cacheDepth());
                // Perform the local write
                bool done = false;
                query->performTask(boost::bind(&doneTask, &done));
                assert(done); // Write to cache is synchronous.
                break;
            }
            case Upstream:
                // Schedule sending to upstream and get resumed afterwards.
                scheduler(boost::bind(&FakeQuery::performTask, query.get(),
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
LayerResolver::worker(size_t count, int channel) {
    FakeInterface interface(count); // TODO Initialize the cache depth.
    // We are ready
    ssize_t result = send(channel, "R", 1, 0);
    assert(result == 1);
    // Wait for the run signal
    char buffer;
    result = recv(channel, &buffer, 1, 0);
    assert(result == 1);
    assert(buffer == 'R');
    // Run the handling of queries
    coroutineScheduler(boost::bind(&handleQuery, _1, _2, channel,
                                   interface.layers()), interface);

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
    Child(const boost::function<void(int)>& main,
          isc::asiolink::IOService& service, int parent) :
        subprocess_(main),
        socket_(service, subprocess_.channel()),
        parent_(parent),
        signal_buffer_('\0')
    {}
    Subprocess subprocess_;
    asiolink::LocalSocket socket_;
    int parent_;
    char signal_buffer_;
    Command command_buffer_;
    // Read signal from the child (eg. command header)
    void signalRead(const std::string& error) {
        assert(error == "");
        if (signal_buffer_ == 'F') {
            // The child finished it's work. Don't re-schedule
            // read.
            return;
        } else if (signal_buffer_ == 'W') {
            // Write to cache. Read parameters.
            socket_.asyncRead(boost::bind(&Child::commandRead, this, _1),
                              &command_buffer_, sizeof command_buffer_);
        } else {
            assert(0); // Unknown command
        }
    }
    // Read the command itself and perform it.
    void commandRead(const std::string& error) {
        assert(error == "");
        for (size_t i = 0; i < command_buffer_.workload; ++i) {
            dummy_work();
        }
        // Send the command to upper cache, if it should go there.
        if (-- command_buffer_.levels > 0) {
            const size_t bufsize = sizeof signal_buffer_ +
                sizeof command_buffer_;
            uint8_t sendbuf[bufsize];
            memcpy(sendbuf, "W", sizeof signal_buffer_);
            memcpy(sendbuf + sizeof signal_buffer_, &command_buffer_,
                   sizeof command_buffer_);
            // We expect this is small enough and will fit all at once.
            // If this assumption is not met, assert will catch it and
            // we can fix that code.
            ssize_t result = send(parent_, sendbuf, bufsize, 0);
            assert(result == bufsize);
        }
        // Schedule next command
        socket_.asyncRead(boost::bind(&Child::signalRead, this, _1),
                          &signal_buffer_, sizeof signal_buffer_);
    }
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
        isc::asiolink::IOService service;
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
                                                 fanout, _1), service,
                                     channel);
            children.push_back(child);
            const std::string& ready = child->subprocess_.read(1);
            assert(ready == "R");

            // Schedule reading of command from the child
            child->socket_.asyncRead(boost::bind(&Child::signalRead, child,
                                                 _1),
                                     &child->signal_buffer_,
                                     sizeof child->signal_buffer_);

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
        // Signal all the children to start
        BOOST_FOREACH(Child* c, children) {
            result = send(c->subprocess_.channel(), "R", 1, 0);
            assert(result == 1);
        }
        service.run(); // Run until all the children finish.
        // Signal we finished
        result = send(channel, "F", 1, 0);
        assert(result == 1);
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
