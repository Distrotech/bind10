bin_PROGRAMS = intermediary

AM_CPPFLAGS = -I$(top_srcdir)/common $(BOOSTLOCINC)

AM_LDFLAGS  = $(BOOSTLOCLIB)
AM_LDFLAGS += -lboost_program_options-mt -lboost_system-mt -lboost_thread-mt

intermediary_LDADD = $(top_srcdir)/common/libcommon.a

intermediary_SOURCES  = intermediary.cc
intermediary_SOURCES += intermediary_controller.cc
