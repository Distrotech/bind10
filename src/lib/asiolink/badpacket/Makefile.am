SUBDIRS = .

AM_CPPFLAGS = -I$(top_srcdir)/src/lib -I$(top_builddir)/src/lib
AM_CPPFLAGS += -I$(top_srcdir)/src/lib/log -I$(top_builddir)/src/lib/log
AM_CPPFLAGS += $(BOOST_INCLUDES)

AM_CXXFLAGS = $(B10_CXXFLAGS)

if USE_STATIC_LINK
AM_LDFLAGS = -static
endif

CLEANFILES = *.gcno *.gcda

noinst_PROGRAMS  = badpacket
badpacket_SOURCES  = badpacket.cc
badpacket_SOURCES += header_flags_scan.cc header_flags_scan.h

badpacket_LDADD  = $(top_builddir)/src/lib/asiolink/libasiolink.la
badpacket_LDADD += $(top_builddir)/src/lib/dns/libdns++.la
badpacket_LDADD += $(top_builddir)/src/lib/log/liblog.la
badpacket_LDADD += $(top_builddir)/src/lib/exceptions/libexceptions.la

