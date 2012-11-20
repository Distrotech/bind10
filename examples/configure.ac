#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.59])
AC_INIT(bind10-examples, 20120817, bind10-dev@isc.org)
AC_CONFIG_SRCDIR([README])
AM_INIT_AUTOMAKE
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CXX
AC_LANG([C++])

# Checks for BIND 10 headers and libraries
AX_ISC_BIND10

# We use -R, -rpath etc so the resulting program will be more likekly to
# "just work" by default.  Embedding a specific library path is a controversial
# practice, though; if you don't like it you can remove the following setting.
if test "x$BIND10_RPATH" != "x"; then
   LDFLAGS="$LDFLAGS $BIND10_RPATH"
fi

# For the example host program, we require some socket API library
# and the BIND 10 DNS library.

# In practice, these are specific to Solaris, but wouldn't do any harm for
# others except for the checking overhead.
AC_SEARCH_LIBS(inet_pton, [nsl])
AC_SEARCH_LIBS(recvfrom, [socket])

if test "x$BIND10_DNS_LIB" = "x"; then
   AC_MSG_ERROR([unable to find BIND 10 DNS library needed to build 'host'])
fi

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL

# Checks for Python
m4_define([_AM_PYTHON_INTERPRETER_LIST],
          [python python3.3 python3.2 python3.1 python3])
AC_ARG_WITH([pythonpath],
AC_HELP_STRING([--with-pythonpath=PATH],
  [specify an absolute path to python executable when automatic version check (incorrectly) fails]),
  [python_path="$withval"], [python_path="auto"])
if test "$python_path" = auto; then
	AM_PATH_PYTHON([3.1])
else
	# Older versions of automake can't handle python3 well.  This is an
	# in-house workaround for them.
	PYTHON=$python_path
	AC_SUBST(PYTHON)
	PYTHON_VERSION=[`$PYTHON -c "import sys; sys.stdout.write(sys.version[:3])"`]
	if test `echo "$PYTHON_VERSION >= 3.1" | bc` != 1 ; then
		AC_MSG_ERROR(["Python version too old: $PYTHON_VERSION, need 3.1 or higher"])
	fi
	AC_SUBST(PYTHON_VERSION)
	PYTHONPATH='${exec_prefix}/lib/python'$PYTHON_VERSION'/site-packages'
	AC_SUBST(PYTHONPATH)
fi

AC_CONFIG_FILES([Makefile
                 host/Makefile
                 host/host.py])

AC_OUTPUT
