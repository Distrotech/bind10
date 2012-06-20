AM_CPPFLAGS = -I$(top_srcdir)/src/lib -I$(top_builddir)/src/lib
AM_CXXFLAGS = $(B10_CXXFLAGS)

noinst_LTLIBRARIES = libb10util_unittests.la
libb10util_unittests_la_SOURCES = fork.h fork.cc resolver.h
libb10util_unittests_la_SOURCES += newhook.h newhook.cc
libb10util_unittests_la_SOURCES += testdata.h testdata.cc
if HAVE_GTEST
libb10util_unittests_la_SOURCES += resource.h resource.cc
libb10util_unittests_la_SOURCES += run_all.h run_all.cc
libb10util_unittests_la_SOURCES += textdata.h
endif

# For now, this isn't needed for libb10util_unittests
EXTRA_DIST = mock_socketsession.h

libb10util_unittests_la_CPPFLAGS = $(AM_CPPFLAGS)
if HAVE_GTEST
libb10util_unittests_la_CPPFLAGS += $(GTEST_INCLUDES)
endif

libb10util_unittests_la_LIBADD  = $(top_builddir)/src/lib/util/libb10util.la
libb10util_unittests_la_LIBADD += $(top_builddir)/src/lib/util/io/libb10util_io.la
libb10util_unittests_la_LIBADD += $(top_builddir)/src/lib/exceptions/libexceptions.la

CLEANFILES = *.gcno *.gcda
