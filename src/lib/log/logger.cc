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

#include <stdarg.h>
#include <stdio.h>

#include <log/logger.h>
#include <log/logger_impl.h>
#include <log/logger_name.h>
#include <log/logger_support.h>
#include <log/message_dictionary.h>
#include <log/message_types.h>

#include <util/strutil.h>

using namespace std;

namespace isc {
namespace log {

// Initialize underlying logger, but only if logging has been initialized.
void Logger::initLoggerImpl() {
    if (isLoggingInitialized()) {
        loggerptr_ = new LoggerImpl(name_);
    } else {
        isc_throw(LoggingNotInitialized, "attempt to access logging function "
                  "before logging has been initialized");
    }
}

// Destructor.

Logger::~Logger() {
    delete loggerptr_;
}

// Get Name of Logger

std::string
Logger::getName() {
    return (getLoggerPtr()->getName());
}

// Set the severity for logging.

void
Logger::setSeverity(isc::log::Severity severity, int dbglevel) {
    getLoggerPtr()->setSeverity(severity, dbglevel);
}

// Return the severity of the logger.

isc::log::Severity
Logger::getSeverity() {
    return (getLoggerPtr()->getSeverity());
}

// Get Effective Severity Level for Logger

isc::log::Severity
Logger::getEffectiveSeverity() {
    return (getLoggerPtr()->getEffectiveSeverity());
}

// Debug level (only relevant if messages of severity DEBUG are being logged).

int
Logger::getDebugLevel() {
    return (getLoggerPtr()->getDebugLevel());
}

// Effective debug level (only relevant if messages of severity DEBUG are being
// logged).

int
Logger::getEffectiveDebugLevel() {
    return (getLoggerPtr()->getEffectiveDebugLevel());
}

// Check on the current severity settings

bool
Logger::isDebugEnabled(const MessageID& id, int dbglevel) {
    return (getLoggerPtr()->isDebugEnabled(id, dbglevel));
}

bool
Logger::isInfoEnabled(const MessageID& id) {
    return (getLoggerPtr()->isInfoEnabled(id));
}

bool
Logger::isWarnEnabled(const MessageID& id) {
    return (getLoggerPtr()->isWarnEnabled(id));
}

bool
Logger::isErrorEnabled(const MessageID& id) {
    return (getLoggerPtr()->isErrorEnabled(id));
}

bool
Logger::isFatalEnabled(const MessageID& id) {
    return (getLoggerPtr()->isFatalEnabled(id));
}

// Format a message: looks up the message text in the dictionary and formats
// it, replacing tokens with arguments.
//
// Owing to the use of variable arguments, this must be inline (hence the
// definition of the macro).  Also note that it expects that the message buffer
// "message" is declared in the compilation unit.

// Output methods

void
Logger::output(const Severity& severity, const std::string& message) {
    getLoggerPtr()->outputRaw(severity, message);
}

Logger::Formatter
Logger::debug(int dbglevel, const isc::log::MessageID& ident) {
    if (isDebugEnabled(ident, dbglevel)) {
        return (Formatter(DEBUG, getLoggerPtr()->lookupMessage(ident),
                          this));
    } else {
        return (Formatter());
    }
}

Logger::Formatter
Logger::info(const isc::log::MessageID& ident) {
    if (isInfoEnabled(ident)) {
        return (Formatter(INFO, getLoggerPtr()->lookupMessage(ident),
                          this));
    } else {
        return (Formatter());
    }
}

Logger::Formatter
Logger::warn(const isc::log::MessageID& ident) {
    if (isWarnEnabled(ident)) {
        return (Formatter(WARN, getLoggerPtr()->lookupMessage(ident),
                          this));
    } else {
        return (Formatter());
    }
}

Logger::Formatter
Logger::error(const isc::log::MessageID& ident) {
    if (isErrorEnabled(ident)) {
        return (Formatter(ERROR, getLoggerPtr()->lookupMessage(ident),
                          this));
    } else {
        return (Formatter());
    }
}

Logger::Formatter
Logger::fatal(const isc::log::MessageID& ident) {
    if (isFatalEnabled(ident)) {
        return (Formatter(FATAL, getLoggerPtr()->lookupMessage(ident),
                          this));
    } else {
        return (Formatter());
    }
}

std::map<MessageID, bool>&
Logger::overrides() {
    return getLoggerPtr()->overrides_;
}

// Comparison (testing only)

bool
Logger::operator==(Logger& other) {
    return (*getLoggerPtr() == *other.getLoggerPtr());
}

} // namespace log
} // namespace isc
