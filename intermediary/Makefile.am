bin_PROGRAMS = intermediary

AM_CPPFLAGS = -I$(top_srcdir)/common

AM_LDFLAGS  = -lboost_program_options -lboost_system

intermediary_LDADD = $(top_srcdir)/common/libcommon.a

intermediary_SOURCES  = intermediary.cc
intermediary_SOURCES += intermediary_controller.cc
