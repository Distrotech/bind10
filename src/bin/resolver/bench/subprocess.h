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

#ifndef RESOLVER_BENCH_SUBPROCESS_H
#define RESOLVER_BENCH_SUBPROCESS_H

#include <boost/function.hpp>
#include <boost/noncopyable.hpp>

#include <unistd.h>

namespace isc {
namespace resolver {
namespace bench {

/// \brief Helper to run a subprocess.
class Subprocess : boost::noncopyable {
public:
    /// \brief Run a function in subprocess.
    ///
    /// This will fork and run the provided function in the sub
    /// process. A communication socket is created and one end
    /// is passed to the function. The other is provided by
    /// the channel() method to the parent.
    Subprocess(const boost::function<void(int)>& main);
    /// \brief Wait for the subprocess to terminate.
    ~Subprocess();
    /// A file descriptor for socket to communicate with the
    /// function.
    int channel() const {
        return (channel_);
    }
    /// \brief Send some data, abort on error.
    void send(const void* data, size_t amount);
    /// \brief Read that amount of data (or to EOF). Aborts on error.
    std::string read(size_t amount);
private:
    int channel_;
    pid_t pid;
};

}
}
}

#endif
