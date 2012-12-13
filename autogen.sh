#! /bin/sh

echo "*** Running autoheader"
autoheader || exit 1
echo "*** Running aclocal -I m4"
aclocal -I m4 || exit 1
echo "*** Running automake --add-missing --copy"
automake --add-missing --copy || exit 1
echo "*** Running autoconf"
autoconf || exit 1
./configure  ${*}

