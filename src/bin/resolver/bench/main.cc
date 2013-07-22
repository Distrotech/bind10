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

#ifdef BOOST_COROUTINES
#include <resolver/bench/coroutine_resolver.h>
#include <resolver/bench/layers.h>
#endif
#include <resolver/bench/naive_resolver.h>

#include <bench/benchmark.h>

#include <iostream>

using namespace std;

const size_t count = 1000; // TODO: We may want to read this from argv.

int main(int, const char**) {
#ifdef BOOST_COROUTINES
    for (size_t i = 2; i < 5; ++i) { //fanout
        for (size_t j = 1; j <= 10; ++j) { // Number of workers
            cout << "Layered cache with " << j << " work processes and " <<
                "fanout of " << i << endl;
            isc::resolver::bench::LayerResolver layer_resolver(::count, j, i);
            isc::bench::BenchMark<isc::resolver::bench::LayerResolver>
                (1, layer_resolver, true);
        }
    }
    for (size_t i = 1; i <= 10; ++i) {
        cout << "Coroutine resolver with " << i << " threads" << endl;
        isc::resolver::bench::CoroutineResolver coroutine_resolver(::count, i);
        isc::bench::BenchMark<isc::resolver::bench::CoroutineResolver>
            (1, coroutine_resolver, true);
    }
#endif
    // Run the naive implementation
    isc::resolver::bench::NaiveResolver naive_resolver(::count);
    isc::bench::BenchMark<isc::resolver::bench::NaiveResolver>
        (1, naive_resolver, true);
    return 0;
}
