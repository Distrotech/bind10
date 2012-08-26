AM_CPPFLAGS = -I$(top_srcdir)/src/lib -I$(top_builddir)/src/lib
AM_CPPFLAGS += $(BOOST_INCLUDES)

AM_CXXFLAGS = $(B10_CXXFLAGS)

if USE_STATIC_LINK
AM_LDFLAGS = -static
endif

CLEANFILES = *.gcno *.gcda

noinst_PROGRAMS = sqlite3_bench

sqlite3_bench_SOURCES = sqlite3_bench.cc

sqlite3_bench_CPPFLAGS = $(AM_CPPFLAGS) $(SQLITE_CFLAGS)
sqlite3_bench_LDADD = $(top_builddir)/src/lib/datasrc/libb10-datasrc.la
sqlite3_bench_LDADD += $(SQLITE_LIBS)
