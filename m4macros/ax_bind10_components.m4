dnl @synopsis AX_BIND10_COMPONENTS
dnl
dnl Select what BIND10 components to build and install.
dnl
dnl reallyall includes experimental components

AC_DEFUN([AX_BIND10_COMPONENTS], [

AC_ARG_ENABLE(components,
  AC_HELP_STRING(--enable-components=FEATURES-LIST,Space-separated
list of components to enable | "all" | "reallyall"),[
    for i in $enableval; do
      if test "$i" = "all" -o "$i" = "reallyall" ; then
        components_selection=$i
      else
        i=`echo $i | sed 's/-/_/g'`
        eval "enable_$i=yes"
      fi
    done
  ])

AC_ARG_ENABLE(dhcp,
 AC_HELP_STRING(--enable-dhcp,Build and install the DHCP components),
  [enable_dhcp=$enableval], [
   if test "$components_selection" = "all" -o \
            "$components_selection" = "reallyall" ; then
     enable_dhcp=yes
     enable_libdhcp=yes
   fi
])

AM_CONDITIONAL(ENABLE_DHCP, [test "$enable_dhcp" = "yes"])
AC_SUBST(ENABLE_DHCP)
if test "x$enable_dhcp" = "xyes" ; then
  enable_libdhcp=yes
  components_list="$components_list dhcp"
fi

AC_ARG_ENABLE(libdhcp,
 AC_HELP_STRING(--enable-libdhcp,Build and install the dhcp libraries),
  [enable_libdhcp=$enableval], [
   if test "$components_selection" = "all" -o \
            "$components_selection" = "reallyall" ; then
     enable_libdhcp=yes
   fi
])
AM_CONDITIONAL(ENABLE_LIBDHCP, [test "$enable_libdhcp" = "yes"])
AC_SUBST(ENABLE_LIBDHCP)
if test "x$enable_libdhcp" = "xyes" ; then
  components_list="$components_list libdhcp"
fi

AC_ARG_ENABLE(resolver,
  [AC_HELP_STRING([--enable-experimental-resolver],
  [Build and install the experimental resolver [default=no]])],
  [enable_resolver=$enableval], [
   if test "$components_selection" = "reallyall" ; then
     enable_resolver=yes
   fi
])

AM_CONDITIONAL([ENABLE_RESOLVER], [test "$enable_resolver" = "yes"])
AC_SUBST(ENABLE_RESOLVER)
if test "x$enable_resolver" = "xyes" ; then
  components_list="$components_list resolver"
fi

AC_ARG_ENABLE(statistics,
 AC_HELP_STRING(--enable-statistics,Build and install stats components),
  [enable_statistics=$enableval], [
   if test "$components_selection" = "all" -o \
            "$components_selection" = "reallyall" ; then
     enable_statistics=yes
   fi
])
AM_CONDITIONAL(ENABLE_STATISTICS, [test "$enable_statistics" = "yes"])
AC_SUBST(ENABLE_STATISTICS)
if test "x$enable_statistics" = "xyes" ; then
  components_list="$components_list statistics"
fi

])dnl AX_BIND10_COMPONENTS
