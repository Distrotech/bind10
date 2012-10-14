AM_CPPFLAGS = -I$(top_srcdir)/src/lib -I$(top_builddir)/src/lib
AM_CPPFLAGS += -I$(top_srcdir)/src/bin -I$(top_builddir)/src/bin
AM_CPPFLAGS += $(BOOST_INCLUDES)

AM_CXXFLAGS = $(B10_CXXFLAGS)

if USE_STATIC_LINK
AM_LDFLAGS = -static
endif

CLEANFILES = *.gcno *.gcda memmgr.spec spec_config.h

pkglibexecdir = $(libexecdir)/@PACKAGE@

bin_PROGRAMS = b10-compilezone
pkglibexec_PROGRAMS = b10-memmgr

b10_compilezone_SOURCES = compile_zone.cc

b10_compilezone_LDADD = $(top_builddir)/src/lib/dns/libb10-dns++.la
b10_compilezone_LDADD += $(top_builddir)/src/lib/util/libb10-util.la
b10_compilezone_LDADD += $(top_builddir)/src/lib/exceptions/libb10-exceptions.la
b10_compilezone_LDADD += $(top_builddir)/src/lib/datasrc/libb10-datasrc.la
b10_compilezone_LDADD += $(top_builddir)/src/lib/log/libb10-log.la

b10_memmgr_SOURCES = memmgr_main.cc
b10_memmgr_SOURCES += app_runner.h app_runner.cc
b10_memmgr_SOURCES += memmgr.h memmgr.cc
b10_memmgr_SOURCES += logger.h logger.cc

b10_memmgr_LDADD = $(top_builddir)/src/lib/dns/libb10-dns++.la
b10_memmgr_LDADD += $(top_builddir)/src/lib/util/libb10-util.la
b10_memmgr_LDADD += $(top_builddir)/src/lib/config/libb10-cfgclient.la
b10_memmgr_LDADD += $(top_builddir)/src/lib/cc/libb10-cc.la
b10_memmgr_LDADD += $(top_builddir)/src/lib/exceptions/libb10-exceptions.la
b10_memmgr_LDADD += $(top_builddir)/src/lib/datasrc/libb10-datasrc.la
b10_memmgr_LDADD += $(top_builddir)/src/lib/asiolink/libb10-asiolink.la
b10_memmgr_LDADD += $(top_builddir)/src/lib/log/libb10-log.la

memmgr.spec: memmgr.spec.pre
	$(SED) -e "s|@@LOCALSTATEDIR@@|$(localstatedir)|" memmgr.spec.pre >$@

spec_config.h: spec_config.h.pre
	$(SED) -e "s|@@LOCALSTATEDIR@@|$(localstatedir)|" spec_config.h.pre >$@

memmgr_messages.h memmgr_messages.cc: memmgr_messages.mes
	$(top_builddir)/src/lib/log/compiler/message $(top_srcdir)/src/bin/memmgr/memmgr_messages.mes

BUILT_SOURCES = spec_config.h memmgr_messages.h memmgr_messages.cc

nodist_b10_memmgr_SOURCES = memmgr_messages.h memmgr_messages.cc
EXTRA_DIST = memmgr_messages.mes
