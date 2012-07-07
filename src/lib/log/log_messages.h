// File created from log_messages.mes on Thu Jul  7 15:32:06 2011

#ifndef __LOG_MESSAGES_H
#define __LOG_MESSAGES_H

#include <log/dll.h>
#include <log/message_types.h>

namespace isc {
namespace log {

extern const ISC_LIBLOG_API isc::log::MessageID LOG_BAD_DESTINATION;
extern const ISC_LIBLOG_API isc::log::MessageID LOG_BAD_SEVERITY;
extern const ISC_LIBLOG_API isc::log::MessageID LOG_BAD_STREAM;
extern const ISC_LIBLOG_API isc::log::MessageID LOG_DUPLICATE_MESSAGE_ID;
extern const ISC_LIBLOG_API isc::log::MessageID LOG_DUPLICATE_NAMESPACE;
extern const ISC_LIBLOG_API isc::log::MessageID LOG_INPUT_OPEN_FAIL;
extern const ISC_LIBLOG_API isc::log::MessageID LOG_INVALID_MESSAGE_ID;
extern const ISC_LIBLOG_API isc::log::MessageID LOG_NAMESPACE_EXTRA_ARGS;
extern const ISC_LIBLOG_API isc::log::MessageID LOG_NAMESPACE_INVALID_ARG;
extern const ISC_LIBLOG_API isc::log::MessageID LOG_NAMESPACE_NO_ARGS;
extern const ISC_LIBLOG_API isc::log::MessageID LOG_NO_MESSAGE_ID;
extern const ISC_LIBLOG_API isc::log::MessageID LOG_NO_MESSAGE_TEXT;
extern const ISC_LIBLOG_API isc::log::MessageID LOG_NO_SUCH_MESSAGE;
extern const ISC_LIBLOG_API isc::log::MessageID LOG_OPEN_OUTPUT_FAIL;
extern const ISC_LIBLOG_API isc::log::MessageID LOG_PREFIX_EXTRA_ARGS;
extern const ISC_LIBLOG_API isc::log::MessageID LOG_PREFIX_INVALID_ARG;
extern const ISC_LIBLOG_API isc::log::MessageID LOG_READING_LOCAL_FILE;
extern const ISC_LIBLOG_API isc::log::MessageID LOG_READ_ERROR;
extern const ISC_LIBLOG_API isc::log::MessageID LOG_UNRECOGNISED_DIRECTIVE;
extern const ISC_LIBLOG_API isc::log::MessageID LOG_WRITE_ERROR;

} // namespace log
} // namespace isc

#endif // __LOG_MESSAGES_H
