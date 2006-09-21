## Process this file with automake to produce Makefile.in

@SET_MAKE@

noinst_LIBRARIES = libfrontend.a

AUTOMAKE_OPTIONS=foreign

INCLUDES=-I@srcdir@/../include

libfrontend_a_SOURCES=occampi_fe.c occampi_lexer.c occampi_parser.c occampi_primproc.c occampi_cnode.c \
		occampi_snode.c occampi_decl.c occampi_type.c occampi_action.c occampi_lit.c \
		occampi_instance.c occampi_dtype.c occampi_oper.c occampi_function.c occampi_mobiles.c \
		occampi_initial.c occampi_asm.c occampi_traces.c occampi_mwsync.c \
		mcsp_fe.c mcsp_lexer.c mcsp_parser.c mcsp_process.c mcsp_snode.c mcsp_oper.c mcsp_decl.c \
		mcsp_instance.c mcsp_cnode.c \
		rcxb_fe.c rcxb_lexer.c rcxb_parser.c rcxb_program.c \
		mwsync.c \
		hopp_fe.c hopp_lexer.c


CFLAGS=@CFLAGS@ @EXTRA_CFLAGS@

