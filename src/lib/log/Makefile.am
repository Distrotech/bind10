SUBDIRS = interprocess . compiler tests

AM_CPPFLAGS = -I$(top_builddir)/src/lib -I$(top_srcdir)/src/lib
AM_CPPFLAGS += $(BOOST_INCLUDES)
AM_CPPFLAGS += -DTOP_BUILDDIR=\"${abs_top_builddir}\"

CLEANFILES = *.gcno *.gcda

lib_LTLIBRARIES = libb10-log.la
libb10_log_la_SOURCES  =
libb10_log_la_SOURCES += logimpl_messages.cc logimpl_messages.h
libb10_log_la_SOURCES += log_dbglevels.h
libb10_log_la_SOURCES += log_formatter.h log_formatter.cc
libb10_log_la_SOURCES += logger.cc logger.h
libb10_log_la_SOURCES += logger_impl.cc logger_impl.h
libb10_log_la_SOURCES += logger_level.h
libb10_log_la_SOURCES += logger_level.cc logger_level.h
libb10_log_la_SOURCES += logger_level_impl.cc logger_level_impl.h
libb10_log_la_SOURCES += logger_manager.cc logger_manager.h
libb10_log_la_SOURCES += logger_manager_impl.cc logger_manager_impl.h
libb10_log_la_SOURCES += logger_name.cc logger_name.h
libb10_log_la_SOURCES += logger_specification.h
libb10_log_la_SOURCES += logger_support.cc logger_support.h
libb10_log_la_SOURCES += logger_unittest_support.cc logger_unittest_support.h
libb10_log_la_SOURCES += log_messages.cc log_messages.h
libb10_log_la_SOURCES += macros.h
libb10_log_la_SOURCES += message_dictionary.cc message_dictionary.h
libb10_log_la_SOURCES += message_exception.h
libb10_log_la_SOURCES += message_initializer.cc message_initializer.h
libb10_log_la_SOURCES += message_reader.cc message_reader.h
libb10_log_la_SOURCES += message_types.h
libb10_log_la_SOURCES += output_option.cc output_option.h
libb10_log_la_SOURCES += buffer_appender_impl.cc buffer_appender_impl.h

EXTRA_DIST  = logging.dox
EXTRA_DIST += logimpl_messages.mes
EXTRA_DIST += log_messages.mes

# Note: the ordering matters: -Wno-... must follow -Wextra (defined in
# B10_CXXFLAGS)
libb10_log_la_CXXFLAGS = $(AM_CXXFLAGS)
if USE_GXX
libb10_log_la_CXXFLAGS += -Wno-unused-parameter
endif
libb10_log_la_CPPFLAGS = $(AM_CPPFLAGS) $(LOG4CPLUS_INCLUDES)
libb10_log_la_LIBADD   = $(top_builddir)/src/lib/util/libb10-util.la
libb10_log_la_LIBADD  += interprocess/libb10-log_interprocess.la
libb10_log_la_LIBADD  += $(LOG4CPLUS_LIBS)
libb10_log_la_LDFLAGS = -no-undefined -version-info 1:0:0

# Specify the headers for copying into the installation directory tree. User-
# written libraries only need the definitions for logger.h and dependencies.
libb10_log_includedir = $(pkgincludedir)/log
libb10_log_include_HEADERS = \
    log_formatter.h \
    logger.h \
    logger_level.h \
    macros.h \
    message_types.h

