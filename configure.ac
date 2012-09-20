AC_PREREQ(2.61)
AC_INIT([libfresample], [0.1], [depp@zdome.net])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([lib/s16.h])
AC_CONFIG_FILES([Makefile])

AC_PROG_CC
AC_PROG_RANLIB

AC_CHECK_LIB([m], [cos])

AC_ARG_ENABLE(warnings,
  AS_HELP_STRING([--enable-warnings], [enable warnings for GCC]),
  [enable_warnings=$enableval], [enable_warnings=no])
if test "x$enable_warnings" != xno ; then
  CWARN="-Wall -Wextra -Wpointer-arith -Wno-sign-compare -Wwrite-strings -Wmissing-prototypes -Wstrict-prototypes"
  warnings=yes
  if test "x$enable_warnings" == xerror ; then
    CWARN="$CWARN -Werror"
    warnings='yes, as errors'
  fi
else
  warnings='no'
fi
AC_SUBST([CWARN])

AC_OUTPUT
