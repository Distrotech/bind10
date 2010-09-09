check_PROGRAMS = test

test_SOURCES  = test.cc
test_SOURCES += test_utilities.cc

AM_CPPFLAGS = -I$(top_srcdir)/common $(BOOSTLOCINC)

AM_LDFLAGS = $(BOOSTLOCLIB) -lboost_system -lgtest

test_LDADD = $(top_srcdir)/common/libcommon.a
