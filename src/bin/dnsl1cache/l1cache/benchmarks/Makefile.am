AM_CPPFLAGS = -I$(top_srcdir)/src/lib -I$(top_builddir)/src/lib
AM_CPPFLAGS += -I$(top_srcdir)/src/bin -I$(top_builddir)/src/bin
AM_CPPFLAGS += $(BOOST_INCLUDES)

AM_CXXFLAGS = $(B10_CXXFLAGS)

if USE_STATIC_LINK
AM_LDFLAGS = -static
endif

CLEANFILES = *.gcno *.gcda

noinst_PROGRAMS = query_bench
query_bench_SOURCES = query_bench.cc
query_bench_SOURCES += ../../dnsl1cache_messages.cc
query_bench_SOURCES += ../../dnsl1cache_messages.h
query_bench_SOURCES += ../../logger.cc ../../logger.h

query_bench_LDADD = $(top_builddir)/src/lib/dns/libb10-dns++.la
query_bench_LDADD += $(top_builddir)/src/lib/util/libb10-util.la
query_bench_LDADD += $(top_builddir)/src/lib/exceptions/libb10-exceptions.la
query_bench_LDADD += $(top_builddir)/src/lib/bench/libb10-bench.la
query_bench_LDADD += $(top_builddir)/src/lib/config/libb10-cfgclient.la
query_bench_LDADD += $(top_builddir)/src/lib/cc/libb10-cc.la
query_bench_LDADD += $(top_builddir)/src/lib/log/libb10-log.la
query_bench_LDADD += $(top_builddir)/src/lib/asiolink/libb10-asiolink.la
query_bench_LDADD += $(top_builddir)/src/lib/asiodns/libb10-asiodns.la
query_bench_LDADD += $(builddir)/../libl1cache.la
