SUBDIRS = . benchmarks

AM_CPPFLAGS = -I$(top_srcdir)/src/lib -I$(top_builddir)/src/lib
AM_CPPFLAGS += -I$(top_srcdir)/src/bin -I$(top_builddir)/src/bin
AM_CPPFLAGS += $(BOOST_INCLUDES)

AM_CXXFLAGS = $(B10_CXXFLAGS)

CLEANFILES = *.gcno *.gcda

noinst_LTLIBRARIES = libl1cache.la

libl1cache_la_SOURCES = cache_srv.cc cache_srv.h
libl1cache_la_SOURCES += l1hash.cc l1hash.h
libl1cache_la_SOURCES += message_handler.cc
