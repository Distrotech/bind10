SUBDIRS = testdata .

AM_CPPFLAGS = -I$(top_srcdir)/src/lib -I$(top_builddir)/src/lib
AM_CPPFLAGS += -I$(top_builddir)/src/lib/dns -I$(top_srcdir)/src/lib/dns
AM_CPPFLAGS += $(BOOST_INCLUDES)
AM_CPPFLAGS += -DTEST_DATA_DIR=\"$(abs_srcdir)/testdata\"
AM_CPPFLAGS += -DTEST_DATA_BUILDDIR=\"$(abs_builddir)\"

AM_CXXFLAGS = $(B10_CXXFLAGS)

if USE_SHARED_MEMORY
# See src/lib/util/Makefile.am.  We need this because some tests directly
# use the Boost managed_mapped_file.  It's not ideal to deal with it as
# a module-wide flag, but considering it's for tests and only for limited
# environments, it would still be better to introduce more complicated setup.
AM_CXXFLAGS += $(BOOST_MAPPED_FILE_CXXFLAG)
endif

if USE_STATIC_LINK
AM_LDFLAGS = -static
endif

CLEANFILES = *.gcno *.gcda

TESTS_ENVIRONMENT = \
	$(LIBTOOL) --mode=execute $(VALGRIND_COMMAND)

TESTS =
if HAVE_GTEST
TESTS += run_unittests

run_unittests_SOURCES = run_unittests.cc
run_unittests_SOURCES += zone_loader_util.h zone_loader_util.cc
run_unittests_SOURCES += rdata_serialization_unittest.cc
run_unittests_SOURCES += rdataset_unittest.cc
run_unittests_SOURCES += domaintree_unittest.cc
run_unittests_SOURCES += treenode_rrset_unittest.cc
run_unittests_SOURCES += zone_table_unittest.cc
run_unittests_SOURCES += zone_data_unittest.cc
run_unittests_SOURCES += zone_finder_unittest.cc
run_unittests_SOURCES += ../../tests/faked_nsec3.h ../../tests/faked_nsec3.cc
run_unittests_SOURCES += memory_segment_mock.h
run_unittests_SOURCES += segment_object_holder_unittest.cc
run_unittests_SOURCES += memory_client_unittest.cc
run_unittests_SOURCES += rrset_collection_unittest.cc
run_unittests_SOURCES += zone_data_loader_unittest.cc
run_unittests_SOURCES += zone_data_updater_unittest.cc
run_unittests_SOURCES += zone_table_segment_mock.h
run_unittests_SOURCES += zone_table_segment_unittest.cc

if USE_SHARED_MEMORY
run_unittests_SOURCES += zone_table_segment_mapped_unittest.cc
endif

run_unittests_SOURCES += zone_writer_unittest.cc

run_unittests_CPPFLAGS = $(AM_CPPFLAGS) $(GTEST_INCLUDES)
run_unittests_LDFLAGS  = $(AM_LDFLAGS)  $(GTEST_LDFLAGS)

run_unittests_LDADD = $(top_builddir)/src/lib/datasrc/libb10-datasrc.la
run_unittests_LDADD += $(top_builddir)/src/lib/dns/libb10-dns++.la
run_unittests_LDADD += $(top_builddir)/src/lib/cc/libb10-cc.la
run_unittests_LDADD += $(top_builddir)/src/lib/util/unittests/libutil_unittests.la
run_unittests_LDADD += $(top_builddir)/src/lib/util/libb10-util.la
run_unittests_LDADD += $(top_builddir)/src/lib/testutils/libb10-testutils.la
run_unittests_LDADD += $(top_builddir)/src/lib/exceptions/libb10-exceptions.la
run_unittests_LDADD += $(top_builddir)/src/lib/log/libb10-log.la
run_unittests_LDADD += $(GTEST_LDADD)
endif

noinst_PROGRAMS = $(TESTS)
