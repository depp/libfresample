#!/bin/sh
set -e

python tests/genmake.py --autoconf
aclocal
autoconf
if test -x "`which libtoolize`" ; then
    libtoolize
elif test -x "`which glibtoolize`" ; then
    glibtoolize
else
    echo 1>&2 'error: libtoolize is required'
    exit 1
fi
autoheader
automake --add-missing
