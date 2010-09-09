bin_PROGRAMS = worker

AM_CPPFLAGS = -I$(top_srcdir)/common $(BOOSTLOCINC)

AM_LDFLAGS  = $(BOOSTLOCLIB) -lboost_program_options -lboost_system

worker_LDADD = $(top_srcdir)/common/libcommon.a

worker_SOURCES  = worker.cc
