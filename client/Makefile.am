bin_PROGRAMS = client

AM_CPPFLAGS  = -I$(top_srcdir)/common $(BOOSTLOCINC)

AM_LDFLAGS   = $(BOOSTLOCLIB)
AM_LDFLAGS  += -lboost_program_options-mt -lboost_system-mt -lboost_thread-mt

client_SOURCES  = client.cc 
client_SOURCES += client_command.cc client_command.h
client_SOURCES += client_communicator.cc client_communicator.h
client_SOURCES += client_controller.cc client_controller.h
client_SOURCES += client_controller_synchronous.cc client_controller_synchronous.h
client_SOURCES += client_controller_asynchronous.cc client_controller_asynchronous.h
client_SOURCES += logger.cc logger.h
client_SOURCES += token_bucket.cc token_bucket.h

client_LDADD = $(top_srcdir)/common/libcommon.a
