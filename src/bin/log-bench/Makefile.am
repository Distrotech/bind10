AM_CPPFLAGS  = -I$(top_srcdir)/src/lib -I$(top_builddir)/src/lib
AM_CPPFLAGS += $(BOOST_INCLUDES)

AM_CXXFLAGS = $(B10_CXXFLAGS)

if USE_STATIC_LINK
AM_LDFLAGS = -static
endif

CLEANFILES  = *.gcno *.gcda log_message.h log_message.cc

BUILT_SOURCES = log_message.cc log_message.h

log_message.h log_message.cc: log_message.mes
	$(top_builddir)/src/lib/log/compiler/message $(top_srcdir)/src/bin/log-bench/log_message.mes

noinst_PROGRAMS = logbench
logbench_SOURCES  = log-bench.cc
logbench_SOURCES += log.h log.cc
logbench_SOURCES += log_message.h log_message.cc

logbench_LDADD  = $(top_builddir)/src/lib/log/liblog.la
logbench_LDADD += $(top_builddir)/src/lib/bench/libbench.la
