lib_LIBRARIES = libcommon.a

libcommon_a_SOURCES  = msgq_communicator.cc
libcommon_a_SOURCES += target_command.cc
libcommon_a_SOURCES += udp_communicator.cc
libcommon_a_SOURCES += utilities.cc

AM_LDFLAGS  = -lboost_system

bin_PROGRAMS = queue_clear
queue_clear_SOURCES  = queue_clear.cc
