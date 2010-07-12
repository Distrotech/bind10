bin_PROGRAMS = worker

AM_CPPFLAGS = -I$(top_srcdir)/common

AM_LDFLAGS  = -lboost_program_options -lboost_system

worker_LDADD = $(top_srcdir)/common/libcommon.a

worker_SOURCES  = worker.cc
worker_SOURCES += worker_controller.cc
