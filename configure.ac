#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

#AC_PREREQ([2.63])
AC_INIT([receptionist], [0.1], [stephen@isc.org])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_CONFIG_SRCDIR([client/client_communicator.h])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CXX
AC_PROG_RANLIB

# Checks for libraries.
AC_ARG_WITH([boost],
   [AS_HELP_STRING([--with-boost=<loc>], [Specify location of boost root (if not /usr)])],
   [
      AC_CHECK_FILE([$with_boost/lib/libboost_system-mt.a],
      [], [AC_MSG_FAILURE([Unable to find boost libraries in location specified])])
   ],
   [
      AC_CHECK_FILE([/usr/lib/libboost_system.a],
      [], [AC_MSG_FAILURE([Unable to find boost libraries in /usr/lib])])
   ])

BOOSTLOCLIB=
AS_IF([test "x$with_boost" != "x"],
   [AC_SUBST([BOOSTLOCLIB], [-L$with_boost/lib])], [])

BOOSTLOCINC=
AS_IF([test "x$with_boost" != "x"],
   [AC_SUBST([BOOSTLOCINC], [-I$with_boost/include])], [])

# Checks for header files.
AC_CHECK_HEADERS([stdint.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T
AC_TYPE_UINT8_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T

# Checks for library functions.

AC_CONFIG_FILES([Makefile
                 common/Makefile
                 common/test/Makefile
                 client/Makefile
                 contractor/Makefile
                 intermediary/Makefile
                 receptionist/Makefile
                 server/Makefile
                 worker/Makefile])
AC_OUTPUT
