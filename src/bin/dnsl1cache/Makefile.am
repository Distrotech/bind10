SUBDIRS = log l1cache .

AM_CPPFLAGS = -I$(top_srcdir)/src/lib -I$(top_builddir)/src/lib
AM_CPPFLAGS += -I$(top_srcdir)/src/bin -I$(top_builddir)/src/bin
AM_CPPFLAGS += $(BOOST_INCLUDES)

AM_CXXFLAGS = $(B10_CXXFLAGS)

if USE_STATIC_LINK
AM_LDFLAGS = -static
endif

CLEANFILES = *.gcno *.gcda dnsl1cache.spec spec_config.h

pkglibexecdir = $(libexecdir)/@PACKAGE@

pkglibexec_PROGRAMS = b10-dnsl1cache

b10_dnsl1cache_SOURCES = dnsl1cache_main.cc
b10_dnsl1cache_SOURCES += app_runner.h app_runner.cc
b10_dnsl1cache_SOURCES += logger.h logger.cc

b10_dnsl1cache_LDADD = $(top_builddir)/src/lib/dns/libb10-dns++.la
b10_dnsl1cache_LDADD += $(top_builddir)/src/lib/util/libb10-util.la
b10_dnsl1cache_LDADD += $(top_builddir)/src/lib/config/libb10-cfgclient.la
b10_dnsl1cache_LDADD += $(top_builddir)/src/lib/cc/libb10-cc.la
b10_dnsl1cache_LDADD += $(top_builddir)/src/lib/exceptions/libb10-exceptions.la
b10_dnsl1cache_LDADD += $(top_builddir)/src/lib/asiolink/libb10-asiolink.la
b10_dnsl1cache_LDADD += $(top_builddir)/src/lib/log/libb10-log.la
b10_dnsl1cache_LDADD += $(top_builddir)/src/lib/asiodns/libb10-asiodns.la
b10_dnsl1cache_LDADD += $(top_builddir)/src/lib/server_common/libb10-server-common.la
b10_dnsl1cache_LDADD += $(top_builddir)/src/bin/dnsl1cache/l1cache/libl1cache.la

dnsl1cache.spec: dnsl1cache.spec.pre
	$(SED) -e "s|@@LOCALSTATEDIR@@|$(localstatedir)|" dnsl1cache.spec.pre >$@

spec_config.h: spec_config.h.pre
	$(SED) -e "s|@@LOCALSTATEDIR@@|$(localstatedir)|" spec_config.h.pre >$@

# dnsl1cache_messages.h dnsl1cache_messages.cc: dnsl1cache_messages.mes
# 	$(top_builddir)/src/lib/log/compiler/message $(top_srcdir)/src/bin/dnsl1cache/dnsl1cache_messages.mes

# BUILT_SOURCES = spec_config.h dnsl1cache_messages.h dnsl1cache_messages.cc
BUILT_SOURCES = spec_config.h

nodist_b10_dnsl1cache_SOURCES = dnsl1cache_messages.h dnsl1cache_messages.cc
EXTRA_DIST = dnsl1cache_messages.mes
