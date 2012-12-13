#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT([proxy],[2.0],[])
AC_CONFIG_SRCDIR([src/main.c])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE(1.9 dist-bzip2 foreign no-define nostdinc)

AM_MAINTAINER_MODE
AC_GNU_SOURCE
AC_PREFIX_DEFAULT([/usr/local/proxy])

test "$prefix" = "NONE" && prefix="${ac_default_prefix}"
test "$sysconfdir" = "\${prefix}/etc" && sysconfdir="${prefix}/etc"

AC_DEFINE_UNQUOTED([SYSCONFDIR], "${sysconfdir}", [Config directory])

#AC_CONFIG_AUX_DIR(.)


# Checks for programs.
#AC_PROG_LIBTOOL

AC_PROG_CC
AC_PROG_RANLIB
if test "$GCC" = "yes"
then CFLAGS="$CXXFLAGS -Wall -ggdb"
fi
AC_CANONICAL_HOST

PKG_PROG_PKG_CONFIG

dnl
dnl Libevent check
dnl
dnl AX_FIND_LIBEV

AX_CHECK_LIBRARY([LIBE], [libev/ev.h], [ev], [],
                        [echo "Unable to find libev"])

AC_SUBST(LIBEV_INCLUDE)
AC_SUBST(LIBEV_LIBS)

dnl AC_CHECK_HEADERS(tchdb.h)


WL_STATIC="-Wl,-static"
WL_DY="-Wl,-dy"
AC_SUBST([WL_STATIC])
AC_SUBST([WL_DY])

AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT
