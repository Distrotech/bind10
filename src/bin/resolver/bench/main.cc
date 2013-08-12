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
#include <resolver/bench/landlord.h>

#include <bench/benchmark.h>

#include <iostream>

const size_t count = 10000; // TODO: We may want to read this from argv.

using namespace std;

int main(int, const char**) {
    for (size_t i = 1; i < 10; ++ i) {
        for (size_t j = 1; j < 10; ++ j) {
            size_t size = j * 25;
            cout << "Landlord with " << i << " workers and " << size << " batch size " << endl;
            isc::resolver::bench::LandlordResolver landlord(::count, size, i);
            isc::bench::BenchMark<isc::resolver::bench::LandlordResolver>
                (1, landlord, true);
        }
    }
    // Run the naive implementation
    isc::resolver::bench::NaiveResolver naive_resolver(::count);
    isc::bench::BenchMark<isc::resolver::bench::NaiveResolver>
        (1, naive_resolver, true);
    return 0;
}
