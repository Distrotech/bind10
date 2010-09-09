bin_PROGRAMS = receptionist

AM_CPPFLAGS = -I$(top_srcdir)/common $(BOOSTLOCINC)

AM_LDFLAGS  = $(BOOSTLOCLIB)
AM_LDFLAGS += -lboost_program_options -lboost_thread -lboost_system

receptionist_LDADD = $(top_srcdir)/common/libcommon.a

receptionist_SOURCES  = receptionist.cc
receptionist_SOURCES += receptionist_controller.cc
