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

#include "interprocess_sync_file.h"

#include <string>
#include <utility>
#include <map>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <assert.h>

namespace isc {
namespace util {

namespace {
// The size_t counts the number of current InterprocessSyncFile
// objects with the same task name. The pthread_mutex_t is the lock that
// should be first acquired by every thread when locking.
typedef std::pair<size_t,pthread_mutex_t> SyncMapData;
typedef std::map<std::string,SyncMapData*> SyncMap;

pthread_mutex_t sync_map_mutex = PTHREAD_MUTEX_INITIALIZER;
SyncMap sync_map;
}

class SyncMapMutex {
public:
    SyncMapMutex() :
        locked_(false),
        last_(0)
    {
    }

    bool lock() {
        if (locked_) {
            return (true);
        }

        last_ = pthread_mutex_lock(&sync_map_mutex);
        if (last_ == 0) {
            locked_ = true;
        }

        return (locked_);
    }

    bool unlock() {
        if (!locked_) {
            return (true);
        }

        last_ = pthread_mutex_unlock(&sync_map_mutex);
        if (last_ == 0) {
            locked_ = false;
        }

        return (!locked_);
    }

    int getLastStatus() {
         return (last_);
    }

    ~SyncMapMutex() {
        if (locked_) {
            if (!unlock()) {
                isc_throw(isc::InvalidOperation,
                          "Error unlocking SyncMapMutex: "
                          << strerror(getLastStatus()));
            }
        }
    }

private:
    bool locked_;
    int last_;
};

InterprocessSyncFile::InterprocessSyncFile(const std::string& task_name) :
    InterprocessSync(task_name), fd_(-1)
{
    SyncMapMutex mutex;
    if (!mutex.lock()) {
        isc_throw(isc::InvalidOperation,
                  "Error locking SyncMapMutex: "
                  << strerror(mutex.getLastStatus()));
    }

    SyncMap::iterator it = sync_map.find(task_name);
    SyncMapData* data;
    if (it != sync_map.end()) {
        data = it->second;
    } else {
        // No data was found in the map, so create and insert one.
        data = new SyncMapData;
        data->first = 0;
        pthread_mutex_init(&data->second, NULL);
        sync_map[task_name] = data;
    }

    // Increment number of users for this task_name.
    data->first++;

    // `mutex` is automatically unlocked during destruction when basic
    // block is exited.
}

InterprocessSyncFile::~InterprocessSyncFile() {
    if (fd_ != -1) {
        // This will also release any applied locks.
        close(fd_);
        // The lockfile will continue to exist, and we must not delete
        // it.
    }

    SyncMapMutex mutex;
    if (!mutex.lock()) {
        isc_throw(isc::InvalidOperation,
                  "Error locking SyncMapMutex: "
                  << strerror(mutex.getLastStatus()));
    }

    SyncMap::iterator it = sync_map.find(task_name_);
    assert(it != sync_map.end());

    SyncMapData* data = it->second;
    assert(data->first > 0);

    data->first--;
    if (data->first == 0) {
        sync_map.erase(it);
        pthread_mutex_destroy(&data->second);
        delete data;
    }

    // `mutex` is automatically unlocked during destruction when basic
    // block is exited.
}

bool
InterprocessSyncFile::do_lock(int cmd, short l_type) {
    // Open lock file only when necessary (i.e., here). This is so that
    // if a default InterprocessSync object is replaced with another
    // implementation, it doesn't attempt any opens.
    if (fd_ == -1) {
        std::string lockfile_path = LOCKFILE_DIR;

        const char* const env = getenv("B10_FROM_BUILD");
        if (env != NULL) {
            lockfile_path = env;
        }

        const char* const env2 = getenv("B10_FROM_BUILD_LOCALSTATEDIR");
        if (env2 != NULL) {
            lockfile_path = env2;
        }

        const char* const env3 = getenv("B10_LOCKFILE_DIR_FROM_BUILD");
        if (env3 != NULL) {
            lockfile_path = env3;
        }

        lockfile_path += "/" + task_name_ + "_lockfile";

        // Open the lockfile in the constructor so it doesn't do the access
        // checks every time a message is logged.
        const mode_t mode = umask(0111);
        fd_ = open(lockfile_path.c_str(), O_CREAT | O_RDWR, 0660);
        umask(mode);

        if (fd_ == -1) {
            isc_throw(InterprocessSyncFileError,
                      "Unable to use interprocess sync lockfile: " +
                      lockfile_path);
        }
    }

    struct flock lock;

    memset(&lock, 0, sizeof (lock));
    lock.l_type = l_type;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 1;

    return (fcntl(fd_, cmd, &lock) == 0);
}

bool
InterprocessSyncFile::lock() {
    if (is_locked_) {
        return (true);
    }

    SyncMapMutex mutex;
    if (!mutex.lock()) {
        isc_throw(isc::InvalidOperation,
                  "Error locking SyncMapMutex: "
                  << strerror(mutex.getLastStatus()));
    }

    SyncMap::iterator it = sync_map.find(task_name_);
    assert(it != sync_map.end());
    SyncMapData* data = it->second;

    if (!mutex.unlock()) {
        isc_throw(isc::InvalidOperation,
                  "Error unlocking SyncMapMutex: "
                  << strerror(mutex.getLastStatus()));
    }

    // First grab the thread lock...
    if (pthread_mutex_lock(&data->second) != 0) {
        return (false);
    }

    // ... then the file lock.
    if (do_lock(F_SETLKW, F_WRLCK)) {
        is_locked_ = true;
        return (true);
    }

    int ret = pthread_mutex_unlock(&data->second);
    if (ret != 0) {
        isc_throw(isc::InvalidOperation,
                  "Error unlocking InterprocessSyncFile mutex: "
                  << strerror(ret));
    }

    return (false);
}

bool
InterprocessSyncFile::tryLock() {
    if (is_locked_) {
        return (true);
    }

    SyncMapMutex mutex;
    if (!mutex.lock()) {
        isc_throw(isc::InvalidOperation,
                  "Error locking SyncMapMutex: "
                  << strerror(mutex.getLastStatus()));
    }

    SyncMap::iterator it = sync_map.find(task_name_);
    assert(it != sync_map.end());
    SyncMapData* data = it->second;

    if (!mutex.unlock()) {
        isc_throw(isc::InvalidOperation,
                  "Error unlocking SyncMapMutex: "
                  << strerror(mutex.getLastStatus()));
    }

    // First grab the thread lock...
    if (pthread_mutex_trylock(&data->second) != 0) {
        return (false);
    }

    // ... then the file lock.
    if (do_lock(F_SETLK, F_WRLCK)) {
        is_locked_ = true;
        return (true);
    }

    int ret = pthread_mutex_unlock(&data->second);
    if (ret != 0) {
        isc_throw(isc::InvalidOperation,
                  "Error unlocking InterprocessSyncFile mutex: "
                  << strerror(ret));
    }

    return (false);
}

bool
InterprocessSyncFile::unlock() {
    if (!is_locked_) {
        return (true);
    }

    // First release the file lock...
    if (do_lock(F_SETLKW, F_UNLCK) == 0) {
        return (false);
    }

    SyncMapMutex mutex;
    if (!mutex.lock()) {
        isc_throw(isc::InvalidOperation,
                  "Error locking SyncMapMutex: "
                  << strerror(mutex.getLastStatus()));
    }

    SyncMap::iterator it = sync_map.find(task_name_);
    assert(it != sync_map.end());
    SyncMapData* data = it->second;

    if (!mutex.unlock()) {
        isc_throw(isc::InvalidOperation,
                  "Error unlocking SyncMapMutex: "
                  << strerror(mutex.getLastStatus()));
    }

    int ret = pthread_mutex_unlock(&data->second);
    if (ret != 0) {
        isc_throw(isc::InvalidOperation,
                  "Error unlocking InterprocessSyncFile mutex: "
                  << strerror(ret));
    }

    is_locked_ = false;
    return (true);
}

} // namespace util
} // namespace isc
