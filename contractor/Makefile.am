bin_PROGRAMS = contractor

AM_CPPFLAGS = -I$(top_srcdir)/common $(BOOSTLOCINC)

AM_LDFLAGS  = $(BOOSTLOCLIB)
AM_LDFLAGS += -lboost_program_options -lboost_system

contractor_LDADD = $(top_srcdir)/common/libcommon.a

contractor_SOURCES  = contractor.cc
