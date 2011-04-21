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
#include <algorithm>
#include <cassert>

#include <stdarg.h>
#include <stdio.h>
#include <boost/lexical_cast.hpp>

#include <log/debug_levels.h>
#include <log/root_logger_name.h>
#include <log/logger.h>
#include <log/logger_impl.h>
#include <log/message_dictionary.h>
#include <log/message_types.h>
#include <log/root_logger_name.h>

#include <util/strutil.h>

using namespace std;

namespace isc {
namespace log {

// Static initializations

LoggerImpl::LoggerInfoMap LoggerImpl::logger_info_;
LoggerImpl::LoggerInfoMap LoggerImpl::bind9_info_;
LoggerImpl::LoggerInfo LoggerImpl::root_logger_info_(isc::log::INFO, 0);

// Constructor
LoggerImpl::LoggerImpl(const std::string& name, bool) : mctx_(0), lctx_(0), lcfg_(0)
{
    // Regardless of anything else, category is the unadorned name of
    // the logger.
    category_ = name;

    // Set up the module and category.
    categories_[0].name = strdup(category_.c_str());
    categories_[0].id = 0;
    categories_[1].name = NULL;
    categories_[1].id = 0;

    modules_[0].name = strdup(getRootLoggerName().c_str());
    modules_[0].id = 0;
    modules_[1].name = NULL;

    // The next set of code is concerned with static initialization.  This
    // logger is instantiated with a given name.  If no other logger with this
    // name has been created, we initialize the BIND 9 logging for that name.
    // Otherwise we can omit the step.

    bool initialized = false;

    if (name == getRootLoggerName()) {
        // We are the root logger.
        is_root_ = true;
        name_ = name;

        // See if it has already been initialized.
        initialized = root_logger_info_.init;
        root_logger_info_.init = true;

    } else {

        // Not the root logger.  Create full name for this logger.
        is_root_ = false;
        name_ = getRootLoggerName() + "." + name;

        // Has a copy of this module already been initialized?
        LoggerInfoMap::iterator i = bind9_info_.find(name_);
        if (i != bind9_info_.end()) {
            // Yes!
            initialized = true;
        } else {

            // No - add information to the map.
            initialized = false;
            bind9_info_[name_] =
                LoggerInfo(isc::log::INFO, MIN_DEBUG_LEVEL, true);
        }
    }

    if (! initialized) {
        bind9LogInit();
    }
}

// Do BIND 9 Logging initialization

void
LoggerImpl::bind9LogInit() {

    if ((isc_mem_create(0, 0, &mctx_) != ISC_R_SUCCESS) ||
        (isc_log_create(mctx_, &lctx_, &lcfg_) != ISC_R_SUCCESS)) {
        std::cout << "Unable to create BIND 9 context\n";
    }

    isc_log_registercategories(lctx_, categories_);
    isc_log_registermodules(lctx_, modules_);
}


// Destructor. (Here because of virtual declaration.)

LoggerImpl::~LoggerImpl() {
    // Free up space for BIND 9 strings
    free(const_cast<void*>(static_cast<const void*>(modules_[0].name))); modules_[0].name = NULL;
    free(const_cast<void*>(static_cast<const void*>(categories_[0].name))); categories_[0].name = NULL;
}

// Set the severity for logging.

void
LoggerImpl::setSeverity(isc::log::Severity severity, int dbglevel) {

    // Silently coerce the debug level into the valid range of 0 to 99

    int debug_level = max(MIN_DEBUG_LEVEL, min(MAX_DEBUG_LEVEL, dbglevel));
    if (is_root_) {

        // Can only set severity for the root logger, you can't disable it.
        // Any attempt to do so is silently ignored.
        if (severity != isc::log::DEFAULT) {
            root_logger_info_ = LoggerInfo(severity, debug_level);
        }

    } else if (severity == isc::log::DEFAULT) {

        // Want to set to default; this means removing the information
        // about this logger from the logger_info_ if it is set.
        LoggerInfoMap::iterator i = logger_info_.find(name_);
        if (i != logger_info_.end()) {
            logger_info_.erase(i);
        }

    } else {

        // Want to set this information
        logger_info_[name_] = LoggerInfo(severity, debug_level);
    }
}

// Return severity level

isc::log::Severity
LoggerImpl::getSeverity() {

    if (is_root_) {
        return (root_logger_info_.severity);
    }
    else {
        LoggerInfoMap::iterator i = logger_info_.find(name_);
        if (i != logger_info_.end()) {
           return ((i->second).severity);
        }
        else {
            return (isc::log::DEFAULT);
        }
    }
}

// Get effective severity.  Either the current severity or, if not set, the
// severity of the root level.

isc::log::Severity
LoggerImpl::getEffectiveSeverity() {

    if (!is_root_ && !logger_info_.empty()) {

        // Not root logger and there is at least one item in the info map for a
        // logger.
        LoggerInfoMap::iterator i = logger_info_.find(name_);
        if (i != logger_info_.end()) {

            // Found, so return the severity.
            return ((i->second).severity);
        }
    }

    // Must be the root logger, or this logger is defaulting to the root logger
    // settings.
    return (root_logger_info_.severity);
}

// Get the debug level.  This returns 0 unless the severity is DEBUG.

int
LoggerImpl::getDebugLevel() {

    if (!is_root_ && !logger_info_.empty()) {

        // Not root logger and there is something in the map, check if there
        // is a setting for this one.
        LoggerInfoMap::iterator i = logger_info_.find(name_);
        if (i != logger_info_.end()) {

            // Found, so return the debug level.
            if ((i->second).severity == isc::log::DEBUG) {
                return ((i->second).dbglevel);
            } else {
                return (0);
            }
        }
    }

    // Must be the root logger, or this logger is defaulting to the root logger
    // settings.
    if (root_logger_info_.severity == isc::log::DEBUG) {
        return (root_logger_info_.dbglevel);
    } else {
        return (0);
    }
}

// The code for isXxxEnabled is quite simple and is in the header.  The only
// exception is isDebugEnabled() where we have the complication of the debug
// levels.

bool
LoggerImpl::isDebugEnabled(int dbglevel) {

    if (!is_root_ && !logger_info_.empty()) {

        // Not root logger and there is something in the map, check if there
        // is a setting for this one.
        LoggerInfoMap::iterator i = logger_info_.find(name_);
        if (i != logger_info_.end()) {

            // Found, so return the debug level.
            if ((i->second).severity <= isc::log::DEBUG) {
                return ((i->second).dbglevel >= dbglevel);
            } else {
                return (false); // Nothing lower than debug
            }
        }
    }

    // Must be the root logger, or this logger is defaulting to the root logger
    // settings.
    if (root_logger_info_.severity <= isc::log::DEBUG) {
        return (root_logger_info_.dbglevel >= dbglevel);
    } else {
       return (false);
    }
}

// Output a general message

void
LoggerImpl::output(const char* sev_text, const MessageID& ident,
    va_list ap)
{
    char message[512];      // Should be large enough for any message

    // Obtain text of the message and substitute arguments.
    const string format = MessageDictionary::globalDictionary().getText(ident);
    vsnprintf(message, sizeof(message), format.c_str(), ap);

    // Get the time in a struct tm format, and convert to text
    time_t t_time;
    time(&t_time);
    struct tm* tm_time = localtime(&t_time);

    char chr_time[32];
    (void) strftime(chr_time, sizeof(chr_time), "%Y-%m-%d %H:%M:%S", tm_time);

    // Now output.
    std::cout << chr_time << " " << sev_text << " [" << getName() << "] " <<
        ident << ", " << message << "\n";
}

} // namespace log
} // namespace isc
