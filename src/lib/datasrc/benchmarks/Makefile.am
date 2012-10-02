AM_CPPFLAGS = -I$(top_srcdir)/src/lib -I$(top_builddir)/src/lib
AM_CPPFLAGS += $(BOOST_INCLUDES)

AM_CXXFLAGS = $(B10_CXXFLAGS)

if USE_STATIC_LINK
AM_LDFLAGS = -static
endif

CLEANFILES = *.gcno *.gcda

noinst_PROGRAMS = sqlite3_bench mysql_bench postgres_bench datasrc_loader

sqlite3_bench_SOURCES = sqlite3_bench.cc

sqlite3_bench_CPPFLAGS = $(AM_CPPFLAGS) $(SQLITE_CFLAGS)
sqlite3_bench_CPPFLAGS += $(AM_CPPFLAGS) $(SQLITE_CFLAGS)
sqlite3_bench_LDADD = $(top_builddir)/src/lib/datasrc/libb10-datasrc.la
sqlite3_bench_LDADD += $(SQLITE_LIBS)

mysql_bench_SOURCES = mysql_bench.cc
mysql_bench_CPPFLAGS = $(AM_CPPFLAGS)
mysql_bench_CPPFLAGS += -I/usr/local/Cellar/mysql/5.5.25a/include
mysql_bench_LDFLAGS = -L/usr/local/Cellar/mysql/5.5.25a/lib
mysql_bench_LDADD = $(top_builddir)/src/lib/datasrc/libb10-datasrc.la
mysql_bench_LDADD += -lmysqlclient

postgres_bench_SOURCES = postgres_bench.cc
postgres_bench_CPPFLAGS = $(AM_CPPFLAGS)
postgres_bench_CPPFLAGS += -I/usr/local/include
postgres_bench_LDFLAGS = -L/usr/local/lib
postgres_bench_LDADD = $(top_builddir)/src/lib/datasrc/libb10-datasrc.la
postgres_bench_LDADD += -lpq

datasrc_loader_SOURCES = datasrc_loader.cc
datasrc_loader_LDADD = $(top_builddir)/src/lib/datasrc/libb10-datasrc.la
datasrc_loader_LDADD += $(top_builddir)/src/lib/log/libb10-log.la
