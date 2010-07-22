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

#ifndef __TOKEN_BUCKET_H
#define __TOKEN_BUCKET_H

#include <boost/thread.hpp>

/// \brief TokenBucket Class
///
/// Provides synchronization between two threads (a producer and a consumer)
/// in a process.
///
/// There are considered to be a fixed number of tokens available (in a
/// "bucket", hence the name).  The consumer thread removes these tokens and
/// will block when there are no more available. The producer thread adds tokens
/// back to the bucket and wakes up the consumer if it is sleeping.
///
/// To use, both threads must be passed the same instance of this class, which
/// is initialized with the initial number of tokens.  The producer calls
/// addToken() to make tokens available the the consumer calls removeToken()
/// to use some.
///
/// One additional capability of this class - to help with the implementation of
/// testing - is that it is possible for the producer to signal the consumer
/// that a special situation has occurred and wake the latter up if waiting.
///
/// As an additional feature, the consumer thread is also able to pass an
/// indication to the producer thread that it wishes the latter to terminate.

class TokenBucket {
public:

    /// \brief Constructor
    /// \param limit Maximum number of tokens in the pool
    TokenBucket(int initial) : available_(initial), notify_(false)
    {}

    /// \brief Add Token to Pool
    ///
    /// Called by the producer thread, this adds one or more tokens to the
    /// bucket and wakes up the consumer thread.  Note that it can add more
    /// tokens to the bucket than were there initially.
    /// \param tokens Number of tokens to add to the bucket (defaults to 1).
    void addToken(unsigned int count = 1);

    /// \brief Remove Token from Pool
    ///
    /// Removes one or more tokens from the pool and blocks if there are not
    /// enough available.  The fuction can also return if the producer has
    /// sent a notification to the consumer; it this case no assumption can be
    /// made as to whether the tokens have been removed.
    /// \param tokens Number of tokens to remove (default = 1).  The method
    /// will block until this number of tokens is available.
    /// \returns false in the normal case, but true if the producer thread has
    /// signalled a notification to the consumer.
    bool removeToken(unsigned int count = 1);

    /// \brief Sends Notification to Consumer
    ///
    /// Sets or resets the notification flag.  If the former, the consumer is
    /// woken if it is waiting in a call to removeToken().
    /// \param notify Notification flag true to set (the default), false to
    /// reset.
    void notify(bool flag = true);

private:
    volatile unsigned int available_;  //< Number of tokens available
    volatile bool notify_;             //< Notification flag
    boost::mutex  mutex_;              //< Protects condition variable
    boost::condition_variable cond_;   //< Condition variable
};

#endif // __TOKEN_BUCKET_H
