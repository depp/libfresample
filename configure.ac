AC_PREREQ(2.61)
AC_INIT([libfresample], [0.1], [depp@zdome.net])
AC_CONFIG_SRCDIR([lib/resample.c])
AC_CONFIG_FILES([Makefile tests/Makefile])
AC_CONFIG_FILES([tests/test.sh], [chmod +x tests/test.sh])

dnl We should be using AC_CANONICAL_HOST
dnl but that requires config.guess, config.sub, and install-sh
dnl which are installed by automake, but we don't use automake.
dnl We either write our own script to install them, or add them
dnl to the repo and try to keep them up to date, both of which
dnl require some work.  This is ten times easier.
AC_DEFUN([GET_UNAME], [
  $1=$(uname $2 2>/dev/null) || $1=unknown
  $1="${$1:-unknown}"
])
GET_UNAME([UNAME_S], [-s])
GET_UNAME([UNAME_M], [-m])
AC_SUBST([UNAME_S])
AC_SUBST([UNAME_M])

AC_ARG_ENABLE([universal],
  AS_HELP_STRING([--enable-universal], [enable OS X universal build]))

AC_PROG_CC

AC_CHECK_LIB([m], [cos])

dnl Enable compiler warnings if requested
AC_ARG_ENABLE(warnings,
  AS_HELP_STRING([--enable-warnings], [enable warnings for GCC]),
  [enable_warnings=$enableval], [enable_warnings=no])
if test "x$enable_warnings" != xno ; then
  CWARN="-Wall -Wextra -Wpointer-arith -Wno-sign-compare -Wwrite-strings -Wmissing-prototypes -Wstrict-prototypes"
  warnings=yes
  if test "x$enable_warnings" = xerror ; then
    CWARN="$CWARN -Werror"
    warnings='yes, as errors'
  fi
else
  warnings='no'
fi
AC_SUBST([CWARN])

dnl LFR_CHECK_CFLAGS (ID, DESC, CFLAGS, PROLOGUE, BODY)
dnl Try to compile program with PROLOGUE and BODY,
dnl and then try again with each set of CFLAGS
dnl If linking is successful, ACTION-IF-TRUE is called with
dnl 'lfr_cflags' set to the necessary additional CFLAGS.
dnl Store the results in the config.arch.mak file.
AC_DEFUN([LFR_CHECK_CFLAGS],[
AC_MSG_CHECKING([for $2 support])
lfr_save_CFLAGS=$CFLAGS
AC_LANG_CONFTEST([AC_LANG_PROGRAM([$4], [$5])])
lfr_res=no
for lfr_cflags in '' $3; do
  CFLAGS="$lfr_cflags $lfr_save_CFLAGS"
  AC_LINK_IFELSE([], [
    if test -z "$lfr_cflags"; then
      lfr_res="yes"
    else
      lfr_res=$lfr_cflags
    fi
    break
  ])
done
AC_MSG_RESULT([$lfr_res])
rm conftest.$ac_ext
CFLAGS=$lfr_save_CFLAGS
AS_IF([test "$lfr_res" != no],
  [AS_IF([test "$lfr_res" = "yes"],
         [lfr_cflags=''], [lfr_cflags=$lfr_res])
   dnl AC_DEFINE(AS_TR_CPP([HAVE_$1]), [1], [Define if $2 is supported.])
   cat >>$lfr_config <<EOF
AS_TR_CPP([$1])_ENABLED := 1
AS_TR_CPP([$1])_CFLAGS := $lfr_cflags
EOF
], [cat >>$lfr_config <<EOF
AS_TR_CPP([$1])_ENABLED := 0
EOF
])
])

if test "x$enable_universal" = xyes ; then
  if test "x$UNAME_S" = xDarwin ; then
    lfr_archs="i386 x86_64 ppc"
  else
    AC_ERROR([Cannot do universal build except on Darwin.])
  fi
  lfr_universal=yes
  if test -z "$ARCHS" ; then
    lfr_auto_arch=yes
  else
    lfr_archs=$ARCHS
    lfr_auto_arch=no
  fi
else
  lfr_universal=no
  lfr_archs=default
  ARCHS=
fi

lfr_narch_CFLAGS=$CFLAGS
lfr_narch_LDFLAGS=$LDFLAGS
ARCHS=
for lfr_arch in $lfr_archs ; do
  if test $lfr_universal = yes ; then
    CFLAGS="-arch $lfr_arch $lfr_narch_CFLAGS"
    LDFLAGS="-arch $lfr_arch $lfr_narch_LDFLAGS"
    AC_MSG_CHECKING([for $lfr_arch architecture])
    AC_LANG_CONFTEST([AC_LANG_PROGRAM([$4], [$5])])
    AC_LINK_IFELSE([], [
      AC_MSG_RESULT([yes])
    ], [
      AC_MSG_RESULT([no])
      if test $lfr_auto_arch = yes ; then
        continue
      else
        AC_ERROR([cannot compile for $lfr_arch architecture.])
      fi
    ])
    lfr_config=config.$lfr_arch.mak
  else
    lfr_config=config.mak
  fi
  ARCHS="$ARCHS $lfr_arch"

  cat >$lfr_config <<EOF
UNAME_S := $UNAME_S
UNAME_M := $UNAME_M
CC := $CC
CFLAGS := $CFLAGS
CPPFLAGS := $CPPFLAGS $DEFS
CWARN := $CWARN
LDFLAGS := $LDFLAGS
LIBS := $LIBS
EOF

  LFR_CHECK_CFLAGS(
    [sse2], [SSE2], [-msse2],
    [[#include <emmintrin.h>]],
    [[__m128i x = _mm_set1_epi32(15)]])
  
  LFR_CHECK_CFLAGS(
    [altivec], [AltiVec],
    ['-faltivec' '-DINCLUDE_ALTIVEC_H' '-maltivec -DINCLUDE_ALTIVEC_H'],
    [[
#ifdef INCLUDE_ALTIVEC_H
#include <altivec.h>
#endif
    ]], [[vector signed int x = vec_splat_s32(5)]])

done

if test $lfr_universal != yes ; then
  ARCHS=
else
  if test -z "$ARCHS" ; then
    AC_ERROR([cannot create universal binary.])
  fi
fi

AC_SUBST([ARCHS])

AC_OUTPUT
