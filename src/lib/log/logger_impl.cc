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

#include <algorithm>
#include <cassert>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <boost/lexical_cast.hpp>

#include <log/debug_levels.h>
#include <log/root_logger_name.h>
#include <log/logger.h>
#include <log/logger_impl.h>
#include <log/message_dictionary.h>
#include <log/message_types.h>
#include <log/root_logger_name.h>

#include <isc/log.h>

#include <util/strutil.h>

using namespace std;

namespace isc {
namespace log {

// Static variables.  To avoid the static initialization fiasco, all the
// static variables are accessed through functions which declare them static
// inside. (The variables are created the first time the function is called.)

LoggerImpl::LoggerInfo&
LoggerImpl::rootLoggerInfo() {
    static LoggerInfo root_logger_info(isc::log::INFO, 0);
    return root_logger_info;
}

LoggerImpl::LoggerInfoMap&
LoggerImpl::loggerInfo() {
    static LoggerInfoMap logger_info;
    return logger_info;
}

LoggerImpl::LoggerInfoMap&
LoggerImpl::bind9Info() {
    static LoggerInfoMap bind9_info;
    return bind9_info;
}

// The following is a temporary function purely for the validation of BIND 9
// logging in BIND 10.  It is used in the unit tests to determine if the
// default channel - write to stderr and print all information - has been
// created.

bool&
LoggerImpl::channelCreated() {
    static bool created(false);
    return created;
}

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
        initialized = rootLoggerInfo().init;
        rootLoggerInfo().init = true;

    } else {

        // Not the root logger.  Create full name for this logger.
        is_root_ = false;
        name_ = getRootLoggerName() + "." + name;

        // Has a copy of this module already been initialized?
        LoggerInfoMap::iterator i = bind9Info().find(name_);
        if (i != bind9Info().end()) {
            // Yes!
            initialized = true;
        } else {

            // No - add information to the map.
            initialized = false;
            bind9Info()[name_].reset(
                new LoggerInfo(isc::log::INFO, MIN_DEBUG_LEVEL, true));
        }
    }

    if (! initialized) {
        bind9LogInit();

        // Create a default channel (called "def_channel") if required.  This
        // logs to stdout and prints all information (time, module, category
        if (! channelCreated()) {
            isc_logdestination destination;
            memset(&destination, 0, sizeof(destination));
            destination.file.stream = stdout;
            if (! lcfg_) {
                std::cerr << "Logging config is NULL\n";
            }
            isc_result_t result = isc_log_createchannel(lcfg_, "def_channel",
                ISC_LOG_TOFILEDESC, ISC_LOG_INFO, &destination, ISC_LOG_PRINTALL);
            if (result != ISC_R_SUCCESS) {
                std::cout << "ERROR: Unable to create def_channel\n";
            }

            result = isc_log_usechannel(lcfg_, "def_channel", NULL, NULL);
            if (result != ISC_R_SUCCESS) {
                std::cout << "ERROR: Unable to use def_channel\n";
            }
        }
                
        // Save memory and logging context for later retrieval
        if (is_root_) {
            rootLoggerInfo().lctx = lctx_;
            rootLoggerInfo().lcfg = lcfg_;
            rootLoggerInfo().mctx = mctx_;
        } else {
            bind9Info()[name_]->lctx = lctx_;
            bind9Info()[name_]->lcfg = lcfg_;
            bind9Info()[name_]->mctx = mctx_;
        }
    } else {
        // Restore context from saved BIND 9 information
        mctx_ = bind9Info()[name_]->mctx;
        lctx_ = bind9Info()[name_]->lctx;
        lcfg_ = bind9Info()[name_]->lcfg;
    }

}

// Do BIND 9 Logging initialization

void
LoggerImpl::bind9LogInit() {

    if ((isc_mem_create(0, 0, &mctx_) != ISC_R_SUCCESS) ||
        (isc_log_create(mctx_, &lctx_, &lcfg_) != ISC_R_SUCCESS)) {
        std::cout << "ERROR: Unable to create BIND 9 context\n";
    }

    isc_log_registercategories(lctx_, categories_);
    isc_log_registermodules(lctx_, modules_);

}


// Destructor. (Here because of virtual declaration.)

LoggerImpl::~LoggerImpl() {
    // Free up space for BIND 9 strings
    free(const_cast<void*>(static_cast<const void*>(modules_[0].name))); modules_[0].name = NULL;
    free(const_cast<void*>(static_cast<const void*>(categories_[0].name))); categories_[0].name = NULL;

    // Free up the logging context
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
            rootLoggerInfo() = LoggerInfo(severity, debug_level);
        }

    } else if (severity == isc::log::DEFAULT) {

        // Want to set to default; this means removing the information
        // about this logger from the loggerInfo() if it is set.
        LoggerInfoMap::iterator i = loggerInfo().find(name_);
        if (i != loggerInfo().end()) {
            loggerInfo().erase(i);
        }

    } else {

        // Want to set this information
        loggerInfo()[name_].reset(new LoggerInfo(severity, debug_level));
    }
}

// Return severity level

isc::log::Severity
LoggerImpl::getSeverity() {

    if (is_root_) {
        return (rootLoggerInfo().severity);
    }
    else {
        LoggerInfoMap::iterator i = loggerInfo().find(name_);
        if (i != loggerInfo().end()) {
           return ((i->second)->severity);
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

    if (!is_root_ && !loggerInfo().empty()) {

        // Not root logger and there is at least one item in the info map for a
        // logger.
        LoggerInfoMap::iterator i = loggerInfo().find(name_);
        if (i != loggerInfo().end()) {

            // Found, so return the severity.
            return ((i->second)->severity);
        }
    }

    // Must be the root logger, or this logger is defaulting to the root logger
    // settings.
    return (rootLoggerInfo().severity);
}

// Get the debug level.  This returns 0 unless the severity is DEBUG.

int
LoggerImpl::getDebugLevel() {

    if (!is_root_ && !loggerInfo().empty()) {

        // Not root logger and there is something in the map, check if there
        // is a setting for this one.
        LoggerInfoMap::iterator i = loggerInfo().find(name_);
        if (i != loggerInfo().end()) {

            // Found, so return the debug level.
            if ((i->second)->severity == isc::log::DEBUG) {
                return ((i->second)->dbglevel);
            } else {
                return (0);
            }
        }
    }

    // Must be the root logger, or this logger is defaulting to the root logger
    // settings.
    if (rootLoggerInfo().severity == isc::log::DEBUG) {
        return (rootLoggerInfo().dbglevel);
    } else {
        return (0);
    }
}

// The code for isXxxEnabled is quite simple and is in the header.  The only
// exception is isDebugEnabled() where we have the complication of the debug
// levels.

bool
LoggerImpl::isDebugEnabled(int dbglevel) {

    if (!is_root_ && !loggerInfo().empty()) {

        // Not root logger and there is something in the map, check if there
        // is a setting for this one.
        LoggerInfoMap::iterator i = loggerInfo().find(name_);
        if (i != loggerInfo().end()) {

            // Found, so return the debug level.
            if ((i->second)->severity <= isc::log::DEBUG) {
                return ((i->second)->dbglevel >= dbglevel);
            } else {
                return (false); // Nothing lower than debug
            }
        }
    }

    // Must be the root logger, or this logger is defaulting to the root logger
    // settings.
    if (rootLoggerInfo().severity <= isc::log::DEBUG) {
        return (rootLoggerInfo().dbglevel >= dbglevel);
    } else {
       return (false);
    }
}

// Output message of a specific severity.
void
LoggerImpl::debug(const MessageID& ident, va_list ap) {
    output(ISC_LOG_DEBUG(getDebugLevel()), ident, ap);
}

void
LoggerImpl::info(const MessageID& ident, va_list ap) {
    output(ISC_LOG_INFO, ident, ap);
}

void
LoggerImpl::warn(const MessageID& ident, va_list ap) {
    output(ISC_LOG_WARNING, ident, ap);
}

void
LoggerImpl::error(const MessageID& ident, va_list ap) {
    output(ISC_LOG_ERROR, ident, ap);
}

void
LoggerImpl::fatal(const MessageID& ident, va_list ap) {
    output(ISC_LOG_CRITICAL, ident, ap);
}


// Output a general message

void
LoggerImpl::output(int sev, const MessageID& ident, va_list ap) {

    // Obtain text of the message and substitute arguments.
    const string format = static_cast<string>(ident) + ", " + MessageDictionary::globalDictionary().getText(ident);

    // Write the message
    isc_log_vwrite(lctx_, &categories_[0], &modules_[0], sev, format.c_str(), ap);
}

} // namespace log
} // namespace isc
