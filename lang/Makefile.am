## Process this file with automake to produce Makefile.in

@SET_MAKE@

noinst_LIBRARIES = liblang.a

AUTOMAKE_OPTIONS=foreign

INCLUDES=-I@srcdir@/../include

liblang_a_SOURCES=tnode.c names.c prescope.c scope.c typecheck.c langops.c precheck.c aliascheck.c usagecheck.c defcheck.c fetrans.c

CFLAGS=@CFLAGS@ @EXTRA_CFLAGS@

