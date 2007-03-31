## Process this file with automake to produce Makefile.in

@SET_MAKE@

AUTOMAKE_OPTIONS=foreign
GPERF=@GPERF@

noinst_LIBRARIES = libmisc.a

INCLUDES=-I@srcdir@/../include

libmisc_a_SOURCES=gperf_options.h gperf_keywords.h gperf_xmlkeys.h gperf_transinstr.h gperf_landeflookup.h support.c options.c fcnlib.c keywords.c xmlkeys.c transinstr.c langdeflookup.c crypto.c

EXTRA_DIST=options.gperf keywords.gperf xmlkeys.gperf transinstr.gperf langdeflookup.gperf

CFLAGS=@CFLAGS@ @EXTRA_CFLAGS@


gperf_options.h:	options.gperf
			$(GPERF) < options.gperf > gperf_options.h

gperf_keywords.h:	keywords.gperf
			$(GPERF) < keywords.gperf > gperf_keywords.h

gperf_xmlkeys.h:	xmlkeys.gperf
			$(GPERF) < xmlkeys.gperf > gperf_xmlkeys.h

gperf_transinstr.h:	transinstr.gperf
			$(GPERF) < transinstr.gperf > gperf_transinstr.h

gperf_langdeflookup.h:	langdeflookup.gperf
			$(GPERF) < langdeflookup.gperf > gperf_langdeflookup.h

