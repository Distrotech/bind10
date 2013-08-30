dnl @synopsis AX_ASIO_FOR_BIND10
dnl
dnl Test for the Asio header files intended to be used within BIND 10
dnl
dnl If no path to the installed Asio header files is given via the
dnl --with-asio-include option,  the macro searchs under
dnl /usr/local /usr/pkg /opt /opt/local directories.
dnl If it cannot detect any workable path for Asio, this macro treats it
dnl as a fatal error (so it cannot be called if the availability of Asio
dnl is optional).
dnl
dnl This macro calls:
dnl
dnl   AC_SUBST(ASIO_INCLUDES)
dnl

AC_DEFUN([AX_ASIO_FOR_BIND10], [
AC_LANG_SAVE
AC_LANG([C++])

#
# Configure Asio header path
#
# If explicitly specified, use it.
AC_ARG_WITH([asio-include],
  AC_HELP_STRING([--with-asio-include=PATH],
    [specify exact directory for Asio headers]),
    [asio_include_path="$withval"])

# If not specified, try some common paths.
if test -z "$with_asio_include"; then
AC_MSG_CHECKING([for asio headers])
	asiodirs="/usr/local /usr/pkg /opt /opt/local"
	for d in $asiodirs
	do
		if test -f $d/include/asio/error_code.hpp; then
			asio_include_path=$d/include
			break
		fi
	done
fi
AC_MSG_RESULT($asio_include_path)

# TODO: this does not work, since Boost does not provide error_code.h
#AC_ARG_WITH([boost-asio],
#  AC_HELP_STRING([--with-boost-asio]),
#    [boost_asio="$withval"])

# Check the path with some specific headers.
if test "${asio_include_path}" ; then
	ASIO_INCLUDES="-I${asio_include_path}"
fi
#if test "${boost_asio}" = "yes" ; then
#	ASIO_INCLUDES="$ASIO_INCLUDES $BOOST_INCLUDES"
#fi
CPPFLAGS_SAVED="$CPPFLAGS"
CPPFLAGS="$CPPFLAGS $ASIO_INCLUDES"

AC_CHECK_HEADERS([asio.hpp asio/deadline_timer.hpp asio/error.hpp asio/error_code.hpp asio/ip/udp.hpp asio/system_error.hpp],,
  AC_MSG_ERROR([Missing required header file.]))

AC_SUBST(ASIO_INCLUDES)

CPPFLAGS="$CPPFLAGS_SAVED"
AC_LANG_RESTORE
])dnl AX_ASIO_FOR_BIND10
