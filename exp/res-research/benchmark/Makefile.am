AM_CPPFLAGS = -I$(top_builddir)/src/lib -I$(top_srcdir)/src/lib

AM_CXXFLAGS = $(B10_CXXFLAGS)

if USE_STATIC_LINK
AM_LDFLAGS = -static
endif

CLEANFILES = *.gcno *.gcda

noinst_PROGRAMS = simple_receiver simple_responder simple_forwarder

simple_receiver_SOURCES = simple_receiver.cc
simple_responder_SOURCES = simple_responder.cc
simple_forwarder_SOURCES = simple_forwarder.cc
