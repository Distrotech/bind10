AM_CPPFLAGS = -I$(top_srcdir)/src/lib -I$(top_builddir)/src/lib
AM_CPPFLAGS += $(BOOST_INCLUDES)

AM_CXXFLAGS = $(B10_CXXFLAGS)

if USE_STATIC_LINK
AM_LDFLAGS = -static
endif

CLEANFILES = *.gcno *.gcda

pkglibexecdir = $(libexecdir)/@PACKAGE@

pkglibexec_PROGRAMS = b10-memmgr

b10_memmgr_SOURCES = main.cc

b10_memmgr_LDADD = $(top_builddir)/src/lib/dns/libb10-dns++.la
b10_memmgr_LDADD += $(top_builddir)/src/lib/util/libb10-util.la
b10_memmgr_LDADD += $(top_builddir)/src/lib/exceptions/libb10-exceptions.la
b10_memmgr_LDADD += $(top_builddir)/src/lib/datasrc/libb10-datasrc.la
b10_memmgr_LDADD += $(top_builddir)/src/lib/log/libb10-log.la
