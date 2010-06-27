AM_CPPFLAGS = -I$(top_srcdir)/src/lib -I$(top_builddir)/src/lib
AM_CPPFLAGS += -I$(top_srcdir)/src/bin -I$(top_builddir)/src/bin

CLEANFILES = *.gcno *.gcda

noinst_PROGRAMS = list_bench
list_bench_SOURCES = list_bench.cc
list_bench_LDADD = $(top_builddir)/src/lib/exceptions/libexceptions.la

