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

#include <dns/name.h>

#include <datasrc/zonetable.h>
#include <datasrc/memory_segment.h>
#include <datasrc/rbtree2.h>

#include <boost/interprocess/offset_ptr.hpp>

#include <cassert>
#include <vector>

using namespace std;
using namespace isc::dns;

namespace isc {
namespace datasrc {

namespace {
struct ZoneFinderWrapper {
    ZoneFinderWrapper(ZoneFinderPtr finder_param) : finder(finder_param)
    {}
    ZoneFinderPtr finder;
};
}

/// \short Private data and implementation of ZoneTable
struct ZoneTable::ZoneTableImpl {
    // Type aliases to make it shorter
    typedef experimental::RBTree<ZoneFinderWrapper> ZoneTree;
    typedef experimental::RBNode<ZoneFinderWrapper> ZoneNode;

    ZoneTableImpl() : zones_(segment_) {}
    ~ZoneTableImpl() {
        vector<ZoneFinderWrapper *>::iterator it = wrappers_.begin();
        vector<ZoneFinderWrapper *>::iterator it_end = wrappers_.end();
        for (; it != it_end; ++it) {
            delete *it;
        }
    }

    MemorySegment segment_;
    // The actual storage
    ZoneTree zones_;
    vector<ZoneFinderWrapper *> wrappers_;

    /*
     * The implementation methods are here and just wrap-called in the
     * ZoneTable. We have variables locally (without impl_->), have
     * type aliases, etc. And they will get inlined anyway.
     */

    // Implementation of ZoneTable::addZone
    result::Result addZone(ZoneFinderPtr zone) {
        // Sanity check
        if (!zone) {
            isc_throw(InvalidParameter,
                      "Null pointer is passed to ZoneTable::addZone()");
        }

        // Get the node where we put the zone
        ZoneNode* node(NULL);
        switch (zones_.insert(zone->getOrigin(), &node)) {
            // This is OK
            case ZoneTree::SUCCESS:
            case ZoneTree::ALREADYEXISTS:
                break;
            // Can Not Happen
            default:
                assert(0);
        }
        // Can Not Happen
        assert(node);

        // Is it empty? We either just created it or it might be nonterminal
        if (node->isEmpty()) {
            ZoneFinderWrapper* wrapper(new ZoneFinderWrapper(zone));
            wrappers_.push_back(wrapper);
            node->setData(boost::interprocess::offset_ptr<ZoneFinderWrapper>(
                              wrapper));
            return (result::SUCCESS);
        } else { // There's something there already
            return (result::EXIST);
        }
    }

    // Implementation of ZoneTable::findZone
    ZoneTable::FindResult findZone(const Name& name) const {
        ZoneNode *node(NULL);
        result::Result my_result;

        // Translate the return codes
        switch (zones_.find(name, &node)) {
            case ZoneTree::EXACTMATCH:
                my_result = result::SUCCESS;
                break;
            case ZoneTree::PARTIALMATCH:
                my_result = result::PARTIALMATCH;
                break;
            // We have no data there, so translate the pointer to NULL as well
            case ZoneTree::NOTFOUND:
                return (FindResult(result::NOTFOUND, ZoneFinderPtr()));
            // Can Not Happen
            default:
                assert(0);
                // Because of warning
                return (FindResult(result::NOTFOUND, ZoneFinderPtr()));
        }

        // Can Not Happen (remember, NOTFOUND is handled)
        assert(node);

        return (FindResult(my_result, node->getData()->finder));
    }
};

ZoneTable::ZoneTable() : impl_(new ZoneTableImpl)
{}

ZoneTable::~ZoneTable() {
    delete impl_;
}

result::Result
ZoneTable::addZone(ZoneFinderPtr zone) {
    return (impl_->addZone(zone));
}

result::Result
ZoneTable::removeZone(const Name&) {
    // TODO Implement
    assert(0);
    // This should not ever be returned, the assert should kill us by now
    return (result::SUCCESS);
}

ZoneTable::FindResult
ZoneTable::findZone(const Name& name) const {
    return (impl_->findZone(name));
}

} // end of namespace datasrc
} // end of namespace isc
