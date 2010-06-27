// Copyright (C) 2010  Internet Systems Consortium, Inc. ("ISC")
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

// $Id$

#include <unistd.h>

#include <algorithm>
#include <iostream>
#include <vector>
#include <list>

#include <boost/shared_ptr.hpp>

#include <exceptions/exceptions.h>

#include <bench/benchmark.h>
#include <bench/benchmark_util.h>

using namespace std;
using namespace isc;
using namespace isc::bench;

namespace {
struct ListEntry;
typedef boost::shared_ptr<ListEntry> ListEntryPtr;

struct ListEntry {
    ListEntry(const int value) :
        value_(value), prev_(ListEntryPtr()), next_(ListEntryPtr())
    {}
    int value_;
    list<ListEntryPtr>::iterator entry_iter_; // used in std::list version
    ListEntryPtr prev_;         // used in in-house version
    ListEntryPtr next_;         // used in in-house version
};

// This is a base class of various list implementation strategies.  It defines
// commonly used definitions in the specific subclasses.
//
// Each subclass defines its own run() method, where we move list elements to
// the head, from the head to the end of the data vector, and from the end to
// the head of the data vector.  After that, the list should be back to the
// original state.  In tearDown() we check if this invariant is met.
class ListBenchMark {
public:
    ListBenchMark(const size_t list_size) : list_size_(list_size) {
        for (int i = 0; i < list_size_; ++i) {
            ListEntryPtr entry = ListEntryPtr(new ListEntry(i));
            data_.push_back(entry);
            entry->entry_iter_ = list_.insert(list_.end(), entry);
        }
    }
    const size_t list_size_;
    vector<ListEntryPtr> data_;
    list<ListEntryPtr> list_;   // std::list version
    ListEntryPtr head_;         // in-house version
    ListEntryPtr tail_;         // in-house version
    
    void tearDown() {
        vector<ListEntryPtr>::const_iterator iter;
        list<ListEntryPtr>::const_iterator liter = list_.begin();
        for (iter = data_.begin(); iter != data_.end(); ++iter, ++liter) {
            if ((*iter)->entry_iter_ != liter) {
                isc_throw(Unexpected, "invariant assumption failure");
            }
        }
        data_.clear();
        list_.clear();
    }
};

class InHouseListBenchMark : public ListBenchMark {
public:
    InHouseListBenchMark(size_t list_size) : ListBenchMark(list_size) {}
    void setUp() {
        vector<ListEntryPtr>::iterator iter;
        for (iter = data_.begin(); iter != data_.end(); ++iter) {
            pushFront(*iter);
        }
    }
    void pushFront(ListEntryPtr& entry) {
        if (head_) {
            entry->next_ = head_;
            head_->prev_ = entry;
            head_ = entry;
        } else {
            entry->next_ = head_;
            head_ = entry;
            tail_ = entry;
        }
    }
    unsigned int run() {
        for_each(data_.begin(), data_.end(), MoveToFront(head_, tail_));
        for_each(data_.rbegin(), data_.rend(), MoveToFront(head_, tail_));
        return (data_.size() * 2);
    }
    void tearDown() {
        vector<ListEntryPtr>::iterator iter = data_.begin();
        for (ListEntryPtr entry = head_; entry; entry = entry->next_, ++iter) {
            if (*iter != entry) {
                isc_throw(Unexpected, "invariant assumption failure");
            }
        }
    }

    class MoveToFront {
    public:
        MoveToFront(ListEntryPtr& head, ListEntryPtr& tail) :
            h_(head), t_(tail) {}
        void operator()(const ListEntryPtr& entry) {
            if (entry == h_) {
                return;
            }
            if (entry->prev_) {
                entry->prev_->next_ = entry->next_;
            }
            if (entry->next_) {
                entry->next_->prev_ = entry->prev_;
            }
            if (entry == t_) {
                t_ = entry->prev_;
                t_->next_ = ListEntryPtr();
            }
            h_->prev_ =entry;
            entry->prev_ = ListEntryPtr();
            entry->next_ = h_;
            h_ = entry;
        }
        ListEntryPtr& h_;
        ListEntryPtr& t_;
    };
};

class STLListBenchMark : public ListBenchMark {
public:
    STLListBenchMark(size_t list_size) : ListBenchMark(list_size) {}
    unsigned int run() {
        for_each(data_.begin(), data_.end(), MoveToFront(list_));
        for_each(data_.rbegin(), data_.rend(), MoveToFront(list_));
        return (data_.size() * 2);
    }

    class MoveToFront {
    public:
        MoveToFront(list<ListEntryPtr>& list) : l_(list) {}
        void operator()(const ListEntryPtr& entry) {
            if (entry->entry_iter_ == l_.begin()) {
                return;
            }
            l_.erase(entry->entry_iter_);
            l_.push_front(entry);
            entry->entry_iter_ = l_.begin();
        }
        list<ListEntryPtr>& l_;
    };
};

class STLListSpliceBenchMark : public ListBenchMark {
public:
    STLListSpliceBenchMark(size_t list_size) : ListBenchMark(list_size) {}
    unsigned int run() {
        for_each(data_.begin(), data_.end(), MoveToFront(list_));
        for_each(data_.rbegin(), data_.rend(), MoveToFront(list_));
        return (data_.size() * 2);
    }

    class MoveToFront {
    public:
        MoveToFront(list<ListEntryPtr>& list) : l_(list) {}
        void operator()(const ListEntryPtr& entry) {
            if (entry->entry_iter_ == l_.begin()) {
                return;
            }
            l_.splice(l_.begin(), l_, entry->entry_iter_);
            entry->entry_iter_ = l_.begin();
        }
        list<ListEntryPtr>& l_;
    };
};
}

namespace isc {
namespace bench {
template <>
void
BenchMark<STLListBenchMark>::tearDown() {
    target_.tearDown();
}

template <>
void
BenchMark<STLListSpliceBenchMark>::tearDown() {
    target_.tearDown();
}

template <>
void
BenchMark<InHouseListBenchMark>::setUp() {
    target_.setUp();
}

template <>
void
BenchMark<InHouseListBenchMark>::tearDown() {
    target_.tearDown();
}
}
}

namespace {
const int DEFAULT_ITERATION = 10000;
const int DEFAULT_LIST_SIZE = 10000;

void
usage() {
    cerr << "Usage: list_bench [-n iterations] [-s list_size]" << endl;
    exit (1);
}
}

int
main(int argc, char* argv[]) {
    int ch;
    int iteration = DEFAULT_ITERATION;
    size_t list_size = DEFAULT_LIST_SIZE;

    while ((ch = getopt(argc, argv, "n:p:s:")) != -1) {
        switch (ch) {
        case 'n':
            iteration = atoi(optarg);
            break;
        case 's':
            list_size = atoi(optarg);
            break;
        case '?':
        default:
            usage();
        }
    }
    argc -= optind;
    argv += optind;
    if (argc > 0) {
        usage();
    }

    cout << "Move-to-front benchmark using std::list with erace/push" << endl;
    STLListBenchMark list_bench1(list_size);
    BenchMark<STLListBenchMark> bench1(iteration, list_bench1);
    bench1.run();

    cout << "Move-to-front benchmark using std::list with splice" << endl;
    STLListSpliceBenchMark list_bench2(list_size);
    BenchMark<STLListSpliceBenchMark> bench2(iteration, list_bench2);
    bench2.run();

    cout << "Move-to-front benchmark using in-house list implementation"
         << endl;
    InHouseListBenchMark list_bench3(list_size);
    BenchMark<InHouseListBenchMark> bench3(iteration, list_bench3);
    bench3.run();

    return (0);
}
