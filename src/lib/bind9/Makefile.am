SUBDIRS = . 

AM_CFLAGS   = -I$(top_srcdir)/src/lib/bind9
AM_CFLAGS  += -I$(top_srcdir)/src/lib/bind9/include
AM_CFLAGS  += -I$(top_srcdir)/src/lib/bind9/unix/include
AM_CFLAGS  += -I$(top_srcdir)/src/lib/bind9/nothreads/include
AM_CFLAGS  += -I$(top_srcdir)/src/lib/bind9/x86_32/include
AM_CFLAGS  += -Wformat -Wpointer-arith -Wmissing-prototypes

CLEANFILES = *.gcno *.gcda

lib_LTLIBRARIES = libbind9.la
libbind9_la_SOURCES  =
# libbind9_la_SOURCES += app_api.c
libbind9_la_SOURCES += assertions.c
libbind9_la_SOURCES += backtrace.c
libbind9_la_SOURCES += backtrace-emptytbl.c
libbind9_la_SOURCES += base32.c
libbind9_la_SOURCES += base64.c
libbind9_la_SOURCES += bitstring.c
libbind9_la_SOURCES += buffer.c
libbind9_la_SOURCES += bufferlist.c
libbind9_la_SOURCES += commandline.c
# libbind9_la_SOURCES += entropy.c
libbind9_la_SOURCES += error.c
libbind9_la_SOURCES += event.c
# libbind9_la_SOURCES += fsaccess.c
libbind9_la_SOURCES += hash.c
libbind9_la_SOURCES += heap.c
libbind9_la_SOURCES += hex.c
libbind9_la_SOURCES += hmacmd5.c
libbind9_la_SOURCES += hmacsha.c
libbind9_la_SOURCES += httpd.c
libbind9_la_SOURCES += inet_aton.c
# libbind9_la_SOURCES += inet_ntop.c
# libbind9_la_SOURCES += inet_pton.c
libbind9_la_SOURCES += iterated_hash.c
libbind9_la_SOURCES += lex.c
# libbind9_la_SOURCES += lfsr.c
libbind9_la_SOURCES += lib.c
libbind9_la_SOURCES += log.c
libbind9_la_SOURCES += md5.c
# libbind9_la_SOURCES += mem_api.c
libbind9_la_SOURCES += mem.c
libbind9_la_SOURCES += mutexblock.c
libbind9_la_SOURCES += netaddr.c
libbind9_la_SOURCES += netscope.c
libbind9_la_SOURCES += ondestroy.c
libbind9_la_SOURCES += parseint.c
libbind9_la_SOURCES += portset.c
libbind9_la_SOURCES += print.c
libbind9_la_SOURCES += quota.c
libbind9_la_SOURCES += radix.c
libbind9_la_SOURCES += random.c
libbind9_la_SOURCES += ratelimiter.c
libbind9_la_SOURCES += refcount.c
libbind9_la_SOURCES += region.c
libbind9_la_SOURCES += result.c
libbind9_la_SOURCES += rwlock.c
libbind9_la_SOURCES += serial.c
libbind9_la_SOURCES += sha1.c
libbind9_la_SOURCES += sha2.c
libbind9_la_SOURCES += sockaddr.c
# libbind9_la_SOURCES += socket_api.c
libbind9_la_SOURCES += stats.c
libbind9_la_SOURCES += string.c
libbind9_la_SOURCES += strtoul.c
libbind9_la_SOURCES += symtab.c
# libbind9_la_SOURCES += task_api.c
libbind9_la_SOURCES += task.c
libbind9_la_SOURCES += taskpool.c
# libbind9_la_SOURCES += timer_api.c
libbind9_la_SOURCES += timer.c
libbind9_la_SOURCES += version.c
libbind9_la_SOURCES += unix/app.c
libbind9_la_SOURCES += unix/dir.c
libbind9_la_SOURCES += unix/entropy.c
libbind9_la_SOURCES += unix/errno2result.c
libbind9_la_SOURCES += unix/file.c
libbind9_la_SOURCES += unix/fsaccess.c
# libbind9_la_SOURCES += unix/ifiter_getifaddrs.c
# libbind9_la_SOURCES += unix/ifiter_ioctl.c
# libbind9_la_SOURCES += unix/ifiter_sysctl.c
libbind9_la_SOURCES += unix/interfaceiter.c
# libbind9_la_SOURCES += unix/ipv6.c
libbind9_la_SOURCES += unix/keyboard.c
libbind9_la_SOURCES += unix/net.c
libbind9_la_SOURCES += unix/os.c
libbind9_la_SOURCES += unix/resource.c
libbind9_la_SOURCES += unix/socket.c
libbind9_la_SOURCES += unix/stdio.c
libbind9_la_SOURCES += unix/stdtime.c
libbind9_la_SOURCES += unix/strerror.c
libbind9_la_SOURCES += unix/syslog.c
libbind9_la_SOURCES += unix/time.c
libbind9_la_SOURCES += nls/msgcat.c

# Note: the ordering matters: -Wno-... must follow -Wextra (defined in
# B10_CXXFLAGS)
liblog_la_CXXFLAGS = $(AM_CXXFLAGS)
if USE_GXX
liblog_la_CXXFLAGS += -Wno-unused-parameter
endif
if USE_CLANGPP
# Same for clang++, but we need to turn off -Werror completely.
liblog_la_CXXFLAGS += -Wno-error
endif
libbind9_la_CPPFLAGS = $(AM_CPPFLAGS)
libbind9_la_CFLAGS = $(AM_CFLAGS)
