dnl -*- shell-script -*-
dnl
dnl Process this file with autoconf to produce a configure script.
dnl

AC_REVISION($Revision: 1.1 $)
AC_INIT(nocc.c)
AC_PREREQ(2.12)

AC_CANONICAL_HOST
AC_CANONICAL_TARGET
AM_CONFIG_HEADER(config.h)
AM_CONFIG_HEADER(include/noccconfig.h)

AM_INIT_AUTOMAKE([nocc], [0.1.4])

dnl Checks for programs.
AC_PROG_CC
AM_PROG_CC_STDC
AC_PROG_MAKE_SET
AC_PROG_RANLIB

AC_CHECK_PROG(GPERF,gperf,gperf)

dnl internationalisation
AM_GNU_GETTEXT

AM_C_PROTOTYPES

dnl Checks for libraries.

dnl Checks for header files.
dnl AC_PATH_XTRA
AC_HEADER_STDC
AC_CHECK_HEADERS(unistd.h stdlib.h string.h stdarg.h sys/types.h fcntl.h malloc.h pwd.h sys/mman.h time.h)

dnl Checks for typedefs, structures, and compiler characteristics.

dnl Checks for library functions.
AC_FUNC_VPRINTF
dnl AC_CHECK_FUNCS(select)
AC_CHECK_FUNCS(getpwuid)

AC_ARG_ENABLE(debug,[  --enable-debug          enable compiler debugging (default disabled)],enable_opt_debug=yes,enable_opt_debug=no)


dnl Compilation flags
EXTRA_CFLAGS="-Wall -ggdb -fno-strict-aliasing"
AC_SUBST(EXTRA_CFLAGS)


dnl The default target is a virtual transputer
target_cpu="ETC/T800"
target_os="unknown"
target_vendor="unknown"

dnl Check for gcrypt
AM_PATH_LIBGCRYPT([1.1.0],[usegcrypt=yes],[usegcrypt=no])

dnl AC_CHECK_HEADER(openssl/ssl.h,[have_ssl_h=yes],[have_ssl_h=no])
dnl AC_CHECK_HEADER(openssl/sha.h,[have_sha_h=yes],[have_sha_h=no])
dnl AC_SEARCH_LIBS(SHA1,ssl,[have_libssl=yes],[have_libssl=no])

AC_CHECK_HEADER(expat.h,[have_expat_h=yes],[have_expat_h=no])
AC_SEARCH_LIBS(XML_Parse,expat,[have_libexpat=yes],[have_libexpat=no])
AC_SEARCH_LIBS(dlsym,dl,[have_lib_dl=yes],[have_lib_dl=no])

AC_CHECK_TOOL(LD,ld)


AC_DEFINE_UNQUOTED([HOST_CPU],"$host_cpu",[host CPU type])
AC_DEFINE_UNQUOTED([HOST_OS],"$host_os",[host OS])
AC_DEFINE_UNQUOTED([HOST_VENDOR],"$host_vendor",[host vendor])
AC_DEFINE_UNQUOTED([TARGET_CPU],"$target_cpu",[target CPU type])
AC_DEFINE_UNQUOTED([TARGET_OS],"$target_os",[target OS])
AC_DEFINE_UNQUOTED([TARGET_VENDOR],"$target_vendor",[target vendor])

AC_DEFINE_UNQUOTED([BUILD_PREFIX],"$prefix",[build prefix])


if test $usegcrypt = yes; then
 AC_DEFINE([USE_LIBGCRYPT],1,[define to use libgcrypt])
fi

dnl if test $have_sha_h = yes; then
dnl  if test $have_libssl = yes; then
dnl   AC_DEFINE([USE_LIBSSL],1,[define to use SSL library])
dnl  fi
dnl fi

if test $have_expat_h = yes; then
 if test $have_libexpat = yes; then
  AC_DEFINE([USE_LIBEXPAT],1,[define to use expat XML library])
 else
  AC_DEFINE([USE_BUILTINXML],1,[define to use built-in XML library])
 fi
fi

if test $enable_opt_debug = yes; then
 AC_DEFINE([DEBUG],1,[define to enable compiler debugging])
fi

if test $have_lib_dl = yes; then
 AC_DEFINE([LIBDL],1,[define to enable use of dynamic linker])
fi

AC_OUTPUT([Makefile include/Makefile misc/Makefile lexer/Makefile parser/Makefile extn/Makefile xml/Makefile frontend/Makefile backend/Makefile target/Makefile lang/Makefile tests/Makefile addons/Makefile])

