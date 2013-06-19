dnl -*- shell-script -*-
dnl
dnl Process this file with autoconf to produce a configure script.
dnl

AC_REVISION($Revision: 1.1 $)
AC_INIT(nocc.c)
AC_PREREQ(2.12)

AC_GNU_SOURCE

AC_CANONICAL_HOST
AC_CANONICAL_TARGET
AM_CONFIG_HEADER(config.h)
AM_CONFIG_HEADER(include/noccconfig.h)

AM_INIT_AUTOMAKE([nocc], [0.1.6])

dnl Checks for programs.
AC_PROG_CC
AM_PROG_CC_STDC
AC_PROG_MAKE_SET
AC_PROG_RANLIB

dnl AC_CHECK_PROG(GPERF,gperf,gperf)

dnl internationalisation
AM_GNU_GETTEXT

AM_C_PROTOTYPES

dnl Checks for libraries.

dnl Checks for header files.
dnl AC_PATH_XTRA
AC_HEADER_STDC
AC_CHECK_HEADERS(unistd.h stdlib.h string.h stdarg.h sys/types.h fcntl.h malloc.h pwd.h sys/mman.h time.h)

dnl check for terminal library, borrowed from octave's config
unset tcap
for termlib in ncurses curses termcap terminfo termlib; do
 AC_CHECK_LIB(${termlib}, tputs, [tcap="-l$termlib"])
 test -n "$tcap" && break
done

dnl check for readline/history libraries for interactive stuff
AC_CHECK_HEADER(readline/readline.h,[have_readline_h=yes],[have_readline_h=no])
AC_CHECK_HEADER(readline/history.h,[have_history_h=yes],[have_history_h=no])
AC_CHECK_LIB(readline,readline,[have_libreadline=yes],[have_libreadline=no])

dnl Checks for typedefs, structures, and compiler characteristics.

dnl Checks for library functions.
AC_FUNC_VPRINTF
dnl AC_CHECK_FUNCS(select)
AC_CHECK_FUNCS(getpwuid)

AC_ARG_ENABLE(debug,[  --enable-debug          enable compiler debugging (default disabled)],enable_opt_debug=yes,enable_opt_debug=no)


dnl Compilation flags
EXTRA_CFLAGS="-Wall -ggdb -O0 -fno-strict-aliasing"


dnl The default target is a virtual transputer
target_cpu="ETC/T800"
target_os="unknown"
target_vendor="unknown"

build_dir="$(pwd)"

dnl Check for gcrypt
AM_PATH_LIBGCRYPT([1.1.0],[usegcrypt=yes],[usegcrypt=no])

dnl AC_CHECK_HEADER(openssl/ssl.h,[have_ssl_h=yes],[have_ssl_h=no])
dnl AC_CHECK_HEADER(openssl/sha.h,[have_sha_h=yes],[have_sha_h=no])
dnl AC_SEARCH_LIBS(SHA1,ssl,[have_libssl=yes],[have_libssl=no])

AC_CHECK_HEADER(expat.h,[have_expat_h=yes],[have_expat_h=no])
AC_SEARCH_LIBS(XML_Parse,expat,[have_libexpat=yes],[have_libexpat=no])
AC_SEARCH_LIBS(dlsym,dl,[have_lib_dl=yes],[have_lib_dl=no])

AC_PATH_TOOL(LD,ld)
AC_PATH_TOOL(GPERF,gperf)
AC_PATH_TOOL(GPROLOG,gprolog)
AC_PATH_TOOL(GDB,gdb)
AC_PATH_TOOL(WGET,wget)

AC_DEFINE_UNQUOTED([HOST_CPU],"$host_cpu",[host CPU type])
AC_DEFINE_UNQUOTED([HOST_OS],"$host_os",[host OS])
AC_DEFINE_UNQUOTED([HOST_VENDOR],"$host_vendor",[host vendor])
AC_DEFINE_UNQUOTED([TARGET_CPU],"$target_cpu",[target CPU type])
AC_DEFINE_UNQUOTED([TARGET_OS],"$target_os",[target OS])
AC_DEFINE_UNQUOTED([TARGET_VENDOR],"$target_vendor",[target vendor])

AC_DEFINE_UNQUOTED([BUILD_PREFIX],"$prefix",[build prefix])


if test $usegcrypt = yes; then
 AC_DEFINE([USE_LIBGCRYPT],1,[define to use libgcrypt])
 AC_SEARCH_LIBS(gpg_strerror,gpg-error,,)
fi

dnl if test $have_sha_h = yes; then
dnl  if test $have_libssl = yes; then
dnl   AC_DEFINE([USE_LIBSSL],1,[define to use SSL library])
dnl  fi
dnl fi

AC_ARG_WITH([kroc], AC_HELP_STRING([--with-kroc=...],[Path to KRoC script for CCCSP back-end]), [kroc=$withval], [kroc=none])

if test "x$kroc" = "xnone"; then
  kroc_path=none
else
  kroc_path="$kroc"

  if test -x "$kroc_path"; then
    # Note: will need to change here if KRoC's version output changes
    kroc_version=$(($kroc_path --version 2>&1 || echo 'KRoC version 0.0.0') | head -1 | sed 's/^KRoC version \([[^ ]]*\).*$/\1/')

    AC_MSG_CHECKING([for KRoC version])
    AC_MSG_RESULT($kroc_version)
  else
    kroc_path=none
  fi

  AC_MSG_CHECKING([for location of KRoC script])
  AC_MSG_RESULT($kroc_path)

fi

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

if test $have_libreadline = yes; then
 if test $have_readline_h = yes; then
  if test $have_history_h = yes; then
   AC_DEFINE([USE_LIBREADLINE],1,[define to use GNU readline for interactive operation])
   EXTRA_CFLAGS="$EXTRA_CFLAGS -lreadline $tcap"
  fi
 fi
fi

# additional compilation flag substitutions
AC_SUBST(EXTRA_CFLAGS)

# some substitutions for paths
AC_SUBST(build_dir)
AC_SUBST(kroc_path)

AC_OUTPUT([Makefile include/Makefile misc/Makefile lexer/Makefile parser/Makefile extn/Makefile xml/Makefile frontend/Makefile backend/Makefile target/Makefile lang/Makefile tests/Makefile addons/Makefile nocc.specs.xml])


