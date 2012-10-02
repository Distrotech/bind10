SUBDIRS = memory . tests benchmarks

AM_CPPFLAGS = -I$(top_srcdir)/src/lib -I$(top_builddir)/src/lib
AM_CPPFLAGS += -I$(top_srcdir)/src/lib/dns -I$(top_builddir)/src/lib/dns
AM_CPPFLAGS += $(BOOST_INCLUDES)
AM_CPPFLAGS += $(SQLITE_CFLAGS)

AM_CXXFLAGS = $(B10_CXXFLAGS)

pkglibdir = $(libexecdir)/@PACKAGE@/backends

datasrc_config.h: datasrc_config.h.pre
	$(SED) -e "s|@@PKGLIBDIR@@|$(pkglibdir)|" datasrc_config.h.pre >$@

static.zone: static.zone.pre
	$(SED) -e "s|@@VERSION_STRING@@|$(PACKAGE_STRING)|" $(srcdir)/static.zone.pre >$@
	$(SED) -e 's/\(.*\)/AUTHORS.BIND.	0	CH	TXT	"\1"/' $(top_srcdir)/AUTHORS >>$@

CLEANFILES = *.gcno *.gcda datasrc_messages.h datasrc_messages.cc
CLEANFILES += datasrc_config.h
CLEANFILES += static.zone

lib_LTLIBRARIES = libb10-datasrc.la
libb10_datasrc_la_SOURCES = data_source.h data_source.cc
libb10_datasrc_la_SOURCES += static_datasrc.h static_datasrc.cc
libb10_datasrc_la_SOURCES += sqlite3_datasrc.h sqlite3_datasrc.cc
libb10_datasrc_la_SOURCES += query.h query.cc
libb10_datasrc_la_SOURCES += cache.h cache.cc
libb10_datasrc_la_SOURCES += rbnode_rrset.h
libb10_datasrc_la_SOURCES += rbtree.h
libb10_datasrc_la_SOURCES += zonetable.h zonetable.cc
libb10_datasrc_la_SOURCES += zone.h zone_finder_context.cc
libb10_datasrc_la_SOURCES += result.h
libb10_datasrc_la_SOURCES += logger.h logger.cc
libb10_datasrc_la_SOURCES += client.h iterator.h
libb10_datasrc_la_SOURCES += database.h database.cc
libb10_datasrc_la_SOURCES += factory.h factory.cc
libb10_datasrc_la_SOURCES += client_list.h client_list.cc
libb10_datasrc_la_SOURCES += memory_datasrc.h memory_datasrc.cc
nodist_libb10_datasrc_la_SOURCES = datasrc_messages.h datasrc_messages.cc
libb10_datasrc_la_LDFLAGS = -no-undefined -version-info 1:0:1

pkglib_LTLIBRARIES =  sqlite3_ds.la memory_ds.la static_ds.la mysql_ds.la
pkglib_LTLIBRARIES +=  postgres_ds.la

sqlite3_ds_la_SOURCES = sqlite3_accessor.h sqlite3_accessor.cc
sqlite3_ds_la_SOURCES += sqlite3_accessor_link.cc
sqlite3_ds_la_LDFLAGS = -module -avoid-version
sqlite3_ds_la_LDFLAGS += -no-undefined -version-info 1:0:0
sqlite3_ds_la_LIBADD = $(top_builddir)/src/lib/exceptions/libb10-exceptions.la
sqlite3_ds_la_LIBADD += libb10-datasrc.la
sqlite3_ds_la_LIBADD += $(SQLITE_LIBS)

mysql_ds_la_SOURCES = mysql_accessor.h mysql_accessor.cc
mysql_ds_la_SOURCES += mysql_accessor_link.cc
mysql_ds_la_CPPFLAGS = $(AM_CPPFLAGS) -I/usr/local/Cellar/mysql/5.5.25a/include
mysql_ds_la_LDFLAGS = -module -avoid-version
mysql_ds_la_LDFLAGS += -no-undefined -version-info 1:0:0
mysql_ds_la_LDFLAGS += -L/usr/local/Cellar/mysql/5.5.25a/lib
mysql_ds_la_LIBADD = $(top_builddir)/src/lib/exceptions/libb10-exceptions.la
mysql_ds_la_LIBADD += $(top_builddir)/src/lib/dns/libb10-dns++.la
mysql_ds_la_LIBADD += $(top_builddir)/src/lib/cc/libb10-cc.la
mysql_ds_la_LIBADD += libb10-datasrc.la
mysql_ds_la_LIBADD += -lmysqlclient

postgres_ds_la_SOURCES = postgres_accessor.h postgres_accessor.cc
postgres_ds_la_SOURCES += postgres_accessor_link.cc
postgres_ds_la_CPPFLAGS = $(AM_CPPFLAGS)
postgres_ds_la_LDFLAGS = -module -avoid-version
postgres_ds_la_LDFLAGS += -no-undefined -version-info 1:0:0
postgres_ds_la_LDFLAGS += -L/usr/local/lib
postgres_ds_la_LIBADD = $(top_builddir)/src/lib/exceptions/libb10-exceptions.la
postgres_ds_la_LIBADD += $(top_builddir)/src/lib/dns/libb10-dns++.la
postgres_ds_la_LIBADD += $(top_builddir)/src/lib/cc/libb10-cc.la
postgres_ds_la_LIBADD += libb10-datasrc.la
postgres_ds_la_LIBADD += -lpq

memory_ds_la_SOURCES = memory_datasrc_link.cc
memory_ds_la_LDFLAGS = -module -avoid-version
memory_ds_la_LIBADD = $(top_builddir)/src/lib/exceptions/libb10-exceptions.la
memory_ds_la_LIBADD += libb10-datasrc.la

static_ds_la_SOURCES = static_datasrc_link.cc
static_ds_la_LDFLAGS = -module -avoid-version
static_ds_la_LIBADD = $(top_builddir)/src/lib/exceptions/libb10-exceptions.la
static_ds_la_LIBADD += libb10-datasrc.la

libb10_datasrc_la_LIBADD = $(top_builddir)/src/lib/exceptions/libb10-exceptions.la
libb10_datasrc_la_LIBADD += $(top_builddir)/src/lib/dns/libb10-dns++.la
libb10_datasrc_la_LIBADD += $(top_builddir)/src/lib/log/libb10-log.la
libb10_datasrc_la_LIBADD += $(top_builddir)/src/lib/cc/libb10-cc.la
libb10_datasrc_la_LIBADD += memory/libdatasrc_memory.la # convenience library
libb10_datasrc_la_LIBADD += $(SQLITE_LIBS)

BUILT_SOURCES = datasrc_config.h datasrc_messages.h datasrc_messages.cc
datasrc_messages.h datasrc_messages.cc: Makefile datasrc_messages.mes
	$(top_builddir)/src/lib/log/compiler/message $(top_srcdir)/src/lib/datasrc/datasrc_messages.mes

EXTRA_DIST = datasrc_messages.mes static.zone.pre

zonedir = $(pkgdatadir)
zone_DATA = static.zone
