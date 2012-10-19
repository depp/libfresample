#!/bin/sh
set -e

python tests/genmake.py --autoconf
aclocal
autoconf
libtoolize
autoheader
automake --add-missing
