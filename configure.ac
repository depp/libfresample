AC_PREREQ(2.61)
AC_INIT([libfresample], [0.1], [depp@zdome.net])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([lib/resample.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile])

AC_PROG_CC
AC_PROG_RANLIB

AC_CHECK_LIB([m], [cos])

dnl Enable compiler warnings if requested
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

dnl LFR_CHECK_CFLAGS (ID, DESC, CFLAGS, PROLOGUE, BODY,
dnl                   ACTION-IF-TRUE, ACTION-IF-FALSE)
dnl Try to compile program with PROLOGUE and BODY,
dnl and then try again with each set of CFLAGS
dnl If linking is successful, ACTION-IF-TRUE is called with
dnl 'lfr_cflags' set to the necessary additional CFLAGS.
dnl Otherwise, ACTION-IF-FALSE is called.
dnl Additionally, HAVE_ID is defined both for the C preprocessor
dnl and as a substitution variable.
AC_DEFUN([LFR_CHECK_CFLAGS],[
AS_VAR_PUSHDEF([lfr_Search], [lfr_cflags_cv_$1])dnl
AC_CACHE_CHECK([for $2 support], [lfr_Search],
[lfr_save_CFLAGS=$CFLAGS
AC_LANG_CONFTEST([AC_LANG_PROGRAM([$4], [$5])])
for lfr_cflags in '' $3; do
  if test -z "$lfr_cflags"; then
    lfr_res="yes"
  else
    lfr_res=$lfr_cflags
  fi
  CFLAGS="$lfr_cflags $lfr_save_CFLAGS"
  AC_LINK_IFELSE([], [AS_VAR_SET([lfr_Search], [$lfr_res])])
  AS_VAR_SET_IF([lfr_Search], [break])
done
AS_VAR_SET_IF([lfr_Search], , [AS_VAR_SET([lfr_Search], [no])])
rm conftest.$ac_ext
CFLAGS=$lfr_save_CFLAGS])
AS_VAR_COPY([lfr_res], [lfr_Search])
AS_IF([test "$lfr_res" != no],
  [AS_IF([test "$lfr_res" = "yes"],
         [lfr_cflags=''], [lfr_cflags=$lfr_res])
   AC_DEFINE(AS_TR_CPP([HAVE_$1]), [1], [Define if $2 is supported.])
   AC_SUBST(AS_TR_CPP([HAVE_$1]), [1])
   $6],
  [AC_SUBST(AS_TR_CPP([HAVE_$1]), [0])
   $7])
AS_VAR_POPDEF([lfr_Search])])

LFR_CHECK_CFLAGS(
  [sse2], [SSE2], [-msse2],
  [[#include <emmintrin.h>]],
  [[__m128i x = _mm_set1_epi32(15)]],
  [], [])

LFR_CHECK_CFLAGS(
  [altivec], [AltiVec],
  ['-DINCLUDE_ALTIVEC_H' '-faltivec' '-maltivec -DINCLUDE_ALTIVEC_H'],
  [[
#ifdef INCLUDE_ALTIVEC_H
#include <altivec.h>
#endif
  ]], [[vector signed int x = vec_splat_s32(5)]],
  [], [])

AC_OUTPUT
