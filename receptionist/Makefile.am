bin_PROGRAMS = receptionist

AM_CPPFLAGS = -I$(top_srcdir)/common $(BOOSTLOCINC)

AM_LDFLAGS  = $(BOOSTLOCLIB) -lboost_program_options-mt -lboost_system-mt

receptionist_LDADD = $(top_srcdir)/common/libcommon.a

receptionist_SOURCES  = receptionist.cc
receptionist_SOURCES += receptionist_controller.cc
