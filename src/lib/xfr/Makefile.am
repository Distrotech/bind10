SUBDIRS = . 

AM_CPPFLAGS = -I$(top_srcdir)/src/lib -I$(top_builddir)/src/lib
AM_CPPFLAGS += -I$(top_srcdir)/src/lib/dns -I$(top_builddir)/src/lib/dns
AM_CPPFLAGS += -I$(top_srcdir)/ext
if GCC_WERROR_OK
AM_CPPFLAGS += -Werror
endif

CLEANFILES = *.gcno *.gcda

lib_LTLIBRARIES = libxfr.la
libxfr_la_SOURCES = xfrout_server.h xfrout_server.cc 
libxfr_la_SOURCES += xfrout_client.h xfrout_client.cc 
libxfr_la_SOURCES += fd_share.h fd_share.cc
