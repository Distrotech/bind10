bin_PROGRAMS = client

AM_CPPFLAGS = -I$(top_srcdir)/common

AM_LDFLAGS  = -lboost_program_options -lboost_system -lboost_thread

client_SOURCES  = client.cc 
client_SOURCES += client_command.cc
client_SOURCES += client_communicator.cc
client_SOURCES += client_controller.cc
client_SOURCES += logger.cc

client_LDADD = $(top_srcdir)/common/libcommon.a
