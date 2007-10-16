## Process this file with automake to produce Makefile.in

@SET_MAKE@

noinst_LIBRARIES = libparser.a

AUTOMAKE_OPTIONS=foreign

INCLUDES=-I@srcdir@/../include

libparser_a_SOURCES=parser.c dfa.c dfaerror.c treecheck.c treeops.c trlang.c traceslang.c

CFLAGS=@CFLAGS@ @EXTRA_CFLAGS@

