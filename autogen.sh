#!/bin/sh
set -e

python tests/genmake.py --autoconf
python genmake.py
aclocal
autoconf
autoheader
