## -*- makefile -*-
##
## Process this file with automake to produce Makefile.in
##
## $Source:$
##
## $Id:$
##


VPATH = @srcdir@:@srcdir@/include:@srcdir@/misc
INCLUDES=-I@srcdir@/include

## Cannot use ansi2knr because it is under GPL
## AUTOMAKE_OPTIONS=ansi2knr
## config.guess and config.sub have special exceptions
AUTOMAKE_OPTIONS=foreign

bin_PROGRAMS = nocc

nocc_SOURCES = nocc.c version.c
nocc_LDADD = lexer/liblexer.a parser/libparser.a extn/libextn.a xml/libxml.a frontend/libfrontend.a target/libtarget.a backend/libbackend.a lang/liblang.a misc/libmisc.a parser/libparser.a

SUBDIRS = include misc lexer parser extn xml frontend target backend lang tests addons

CFLAGS = @CFLAGS@ @EXTRA_CFLAGS@ @LIBGCRYPT_CFLAGS@ -DSYSCONFDIR=\"$(sysconfdir)\"
LIBS = @LIBS@ @LIBGCRYPT_LIBS@ -Xlinker --export-dynamic

sysconf_DATA = nocc.specs.xml

EXTRA_DIST = README CHANGELOG TODO AUTHORS extras
