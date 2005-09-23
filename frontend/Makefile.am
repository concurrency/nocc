## Process this file with automake to produce Makefile.in

@SET_MAKE@

noinst_LIBRARIES = libfrontend.a

AUTOMAKE_OPTIONS=foreign

INCLUDES=-I@srcdir@/../include

libfrontend_a_SOURCES=occampi_lexer.c occampi_parser.c occampi_primproc.c occampi_cnode.c \
		occampi_decl.c occampi_type.c occampi_action.c occampi_lit.c occampi_instance.c \
		occampi_dtype.c occampi_oper.c occampi_initial.c


CFLAGS=@CFLAGS@ @EXTRA_CFLAGS@

