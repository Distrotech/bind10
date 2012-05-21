// Copyright (C) 2011  Internet Systems Consortium, Inc. ("ISC")
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
// PERFORMANCE OF THIS SOFTWARE

#include <iostream>
#include <iomanip>
#include <algorithm>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <boost/lexical_cast.hpp>
#include <boost/static_assert.hpp>

#include <log4cplus/configurator.h>

#include <log/logger.h>
#include <log/logger_impl.h>
#include <log/logger_level.h>
#include <log/logger_level_impl.h>
#include <log/logger_name.h>
#include <log/message_dictionary.h>
#include <log/message_types.h>

#include <util/strutil.h>

// Note: as log4cplus and the BIND 10 logger have many concepts in common, and
// thus many similar names, to disambiguate types we don't "use" the log4cplus
// namespace: instead, all log4cplus types are explicitly qualified.

using namespace std;

namespace isc {
namespace log {

// Constructor.  The setting of logger_ must be done when the variable is
// constructed (instead of being left to the body of the function); at least
// one compiler requires that all member variables be constructed before the
// constructor is run, but log4cplus::Logger (the type of logger_) has no
// default constructor.
LoggerImpl::LoggerImpl(const string& name) : name_(expandLoggerName(name)),
    logger_(log4cplus::Logger::getInstance(name_))
{
    lockfile_path_ = LOCKFILE_DIR;

    const char* const env = getenv("B10_FROM_SOURCE");
    if (env != NULL) {
        lockfile_path_ = env;
    }

    const char* const env2 = getenv("B10_FROM_SOURCE_LOCALSTATEDIR");
    if (env2 != NULL) {
        lockfile_path_ = env2;
    }

    lockfile_path_ += "/logger_lockfile";

    // Open the lockfile in the constructor so it doesn't do the access
    // checks every time a message is logged.
    mode_t mode = umask(0111);
    lock_fd_ = open(lockfile_path_.c_str(), O_CREAT | O_RDWR, 0666);
    umask(mode);

    if (lock_fd_ == -1) {
        LOG4CPLUS_ERROR(logger_, "Unable to use logger lockfile: " +
                        lockfile_path_);
    }
}

// Destructor. (Here because of virtual declaration.)

LoggerImpl::~LoggerImpl() {
    if (lock_fd_ != -1) {
        close(lock_fd_);
    }
    // The lockfile will continue to exist, as we mustn't delete it.
}

// Set the severity for logging.
void
LoggerImpl::setSeverity(isc::log::Severity severity, int dbglevel) {
    Level level(severity, dbglevel);
    logger_.setLogLevel(LoggerLevelImpl::convertFromBindLevel(level));
}

// Return severity level
isc::log::Severity
LoggerImpl::getSeverity() {
    Level level = LoggerLevelImpl::convertToBindLevel(logger_.getLogLevel());
    return level.severity;
}

// Return current debug level (only valid if current severity level is DEBUG).
int
LoggerImpl::getDebugLevel() {
    Level level = LoggerLevelImpl::convertToBindLevel(logger_.getLogLevel());
    return level.dbglevel;
}

// Get effective severity.  Either the current severity or, if not set, the
// severity of the root level.
isc::log::Severity
LoggerImpl::getEffectiveSeverity() {
    Level level = LoggerLevelImpl::convertToBindLevel(logger_.getChainedLogLevel());
    return level.severity;
}

// Return effective debug level (only valid if current effective severity level
// is DEBUG).
int
LoggerImpl::getEffectiveDebugLevel() {
    Level level = LoggerLevelImpl::convertToBindLevel(logger_.getChainedLogLevel());
    return level.dbglevel;
}


// Output a general message
string*
LoggerImpl::lookupMessage(const MessageID& ident) {
    return (new string(string(ident) + " " +
                       MessageDictionary::globalDictionary().getText(ident)));
}

void
LoggerImpl::outputRaw(const Severity& severity, const string& message) {
    // Use a lock file for mutual exclusion from other processes to
    // avoid log messages getting interspersed

    struct flock lock;
    int status;

    if (lock_fd_ != -1) {
        // Acquire the exclusive lock
        memset(&lock, 0, sizeof lock);
        lock.l_type = F_WRLCK;
        lock.l_whence = SEEK_SET;
        lock.l_start = 0;
        lock.l_len = 1;
        status = fcntl(lock_fd_, F_SETLKW, &lock);
        if (status != 0) {
            LOG4CPLUS_ERROR(logger_, "Unable to lock logger lockfile: " +
                            lockfile_path_);
            return;
        }

        // Log the message
        switch (severity) {
        case DEBUG:
            LOG4CPLUS_DEBUG(logger_, message);
            break;

        case INFO:
            LOG4CPLUS_INFO(logger_, message);
            break;

        case WARN:
            LOG4CPLUS_WARN(logger_, message);
            break;

        case ERROR:
            LOG4CPLUS_ERROR(logger_, message);
            break;

        case FATAL:
            LOG4CPLUS_FATAL(logger_, message);
        }

        // Release the exclusive lock
        memset(&lock, 0, sizeof lock);
        lock.l_type = F_UNLCK;
        lock.l_whence = SEEK_SET;
        lock.l_start = 0;
        lock.l_len = 1;
        status = fcntl(lock_fd_, F_SETLKW, &lock);
        if (status != 0) {
            LOG4CPLUS_ERROR(logger_, "Unable to unlock logger lockfile: " +
                            lockfile_path_);
        }
    }
}

} // namespace log
} // namespace isc
