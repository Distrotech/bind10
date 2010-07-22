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

#include <algorithm>
#include "token_bucket.h"

// Makes tokens available to the pool and wakes up the consumer.

void
TokenBucket::addToken(unsigned int count) {

    boost::lock_guard<boost::mutex> lock(mutex_);

    // Up the token count and wake the consumer.  If there are still not
    // enough tokens available, the consumer will re-enter the wait.
    available_ += count;
    cond_.notify_one();

    return;
}

// Removes tokens from the pool.  If there are not enough tokens available,
// the call will block.
//
// The call can also return if the producer has sent a notification, in which
// case the return value will be true.

bool
TokenBucket::removeToken(unsigned int count) {

    boost::unique_lock<boost::mutex> lock(mutex_);

    while ((! notify_) && (available_ < count)) {
        cond_.wait(lock);
    }

    // If the wait was broken as a result of the notification, return
    // immediately; otherwise it must have been broken because the required
    // number of tokens are available. (And if both events happened, the
    // notification takes priority.)
    if (! notify_) {
        available_ -= count;
    }

    return notify_;
}


// Sets the notification flag and wakes the consumer.  Note that once the
// flag is set, all calls to removeToken() will return immediately until it
// is reset.

void TokenBucket::notify(bool flag) {
    notify_ = true;
    addToken(0);    // Has the side-effect of waking the consumer
};
