bin_PROGRAMS = server

AM_CPPFLAGS = -I$(top_srcdir)/common $(BOOSTLOCINC)

AM_LDFLAGS  = $(BOOSTLOCLIB)
AM_LDFLAGS += -lboost_program_options-mt -lboost_system-mt -lboost_thread-mt

server_LDADD = $(top_srcdir)/common/libcommon.a

server_SOURCES  = server.cc
