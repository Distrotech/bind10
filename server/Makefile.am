bin_PROGRAMS = server

AM_CPPFLAGS = -I$(top_srcdir)/common

AM_LDFLAGS  = -lboost_program_options -lboost_system -lboost_thread

server_LDADD = $(top_srcdir)/common/libcommon.a

server_SOURCES  = server.cc
