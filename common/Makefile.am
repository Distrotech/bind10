lib_LIBRARIES = libcommon.a

libcommon_a_SOURCES  = children.cc children.h
libcommon_a_SOURCES += debug.cc debug.h debug_flags.h
libcommon_a_SOURCES += burst_server_controller.cc burst_server_controller.h
libcommon_a_SOURCES += burst_server_controller2.cc
libcommon_a_SOURCES += msgq_communicator.cc msgq_communicator.h
libcommon_a_SOURCES += msgq_communicator_client.cc msgq_communicator_client.h
libcommon_a_SOURCES += msgq_communicator_server.cc msgq_communicator_server.h
libcommon_a_SOURCES += packet_counter.cc packet_counter.h
libcommon_a_SOURCES += target_command.cc target_command.h
libcommon_a_SOURCES += udp_buffer.cc udp_buffer.h
libcommon_a_SOURCES += udp_communicator.cc udp_communiator.h
libcommon_a_SOURCES += utilities.cc utilities.h
libcommon_a_SOURCES += throughput_measure.cc throughput_measure.h

AM_CPPFLAGS = $(BOOSTLOCINC)
AM_LDFLAGS  = $(BOOSTLOCLIB) -lboost_system

bin_PROGRAMS = queue_clear
queue_clear_SOURCES  = queue_clear.cc
