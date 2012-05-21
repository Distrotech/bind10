// Copyright (C) 2012  Internet Systems Consortium, Inc. ("ISC")
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

#include "file_lock.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

namespace isc {
namespace util {

file_lock::~file_lock() {
  unlock();
}

bool
file_lock::lock() {
    if (is_locked_) {
        return true;
    }

    if (fd_ != -1) {
        struct flock lock;
        int status;

        // Acquire the exclusive lock
        memset(&lock, 0, sizeof lock);
        lock.l_type = F_WRLCK;
        lock.l_whence = SEEK_SET;
        lock.l_start = 0;
        lock.l_len = 1;

        status = fcntl(fd_, F_SETLKW, &lock);
        if (status == 0) {
            is_locked_ = true;
            return true;
        }
    }

    return false;
}

bool
file_lock::unlock() {
    if (!is_locked_) {
        return true;
    }

    if (fd_ != -1) {
        struct flock lock;
        int status;

        // Release the exclusive lock
        memset(&lock, 0, sizeof lock);
        lock.l_type = F_UNLCK;
        lock.l_whence = SEEK_SET;
        lock.l_start = 0;
        lock.l_len = 1;

        status = fcntl(fd_, F_SETLKW, &lock);
        if (status == 0) {
            is_locked_ = false;
            return true;
        }
    }

    return false;
}

} // namespace util
} // namespace isc
