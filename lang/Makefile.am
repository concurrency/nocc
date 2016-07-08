## Process this file with automake to produce Makefile.in

@SET_MAKE@

noinst_LIBRARIES = liblang.a

AUTOMAKE_OPTIONS=foreign

AM_CPPFLAGS = -I@srcdir@/../include

liblang_a_SOURCES=tnode.c names.c feunit.c prescope.c scope.c typecheck.c langops.c langdef.c \
		constprop.c constraint.c precheck.c aliascheck.c usagecheck.c library.c defcheck.c \
		tracescheck.c mobilitycheck.c postcheck.c fetrans.c metadata.c valueset.c

CFLAGS=@CFLAGS@ @EXTRA_CFLAGS@

