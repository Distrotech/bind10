dnl @synopsis AX_BIND10_COMPONENTS
dnl
dnl Select what BIND10 components to build and install.
dnl
dnl This macro possibly sets:

AC_DEFUN([AX_BIND10_COMPONENTS], [

build_experimental_resolver=no
AC_ARG_ENABLE(experimental-resolver,
  [AC_HELP_STRING([--enable-experimental-resolver],
  [enable building of the experimental resolver [default=no]])],
  [build_experimental_resolver=$enableval])
AM_CONDITIONAL([BUILD_EXPERIMENTAL_RESOLVER], [test "$build_experimental_resolver" = "yes"])
if test "$build_experimental_resolver" = "yes"; then
   BUILD_EXPERIMENTAL_RESOLVER=yes
else
   BUILD_EXPERIMENTAL_RESOLVER=no
fi
AC_SUBST(BUILD_EXPERIMENTAL_RESOLVER)
])dnl AX_BIND10_COMPONENTS
