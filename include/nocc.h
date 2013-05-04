/*
 *	nocc.h -- global definitions for nocc
 *	Copyright (C) 2004-2006 Fred Barnes <frmb@kent.ac.uk>
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __NOCC_H
#define __NOCC_H

extern void nocc_xinternal (char *fmt, ...) __attribute__ ((format (printf, 1, 2)));
extern void nocc_pinternal (char *fmt, const char *file, const int line, ...) __attribute__ ((format (printf, 1, 4)));
extern void nocc_fatal (char *fmt, ...) __attribute__ ((format (printf, 1, 2)));
extern void nocc_serious (char *fmt, ...) __attribute__ ((format (printf, 1, 2)));
extern void nocc_error (char *fmt, ...) __attribute__ ((format (printf, 1, 2)));
extern void nocc_warning (char *fmt, ...) __attribute__ ((format (printf, 1, 2)));
extern void nocc_message (char *fmt, ...) __attribute__ ((format (printf, 1, 2)));
extern void nocc_outerrmsg (char *string);
extern void nocc_cleanexit (void);

extern int nocc_dooption (char *optstr);
extern int nocc_dooption_arg (char *optstr, void *arg);

#if 1
#define nocc_internal(FMT,ARGS...) nocc_pinternal((FMT),__FILE__,__LINE__,## ARGS)
#else
#define nocc_internal(FMT,ARGS...) nocc_xinternal((FMT),## ARGS)
#endif

/* need dynamic-arrays in here */
#include "support.h"

struct TAG_cmd_option;
struct TAG_origin;

/* this holds global options for the compiler */
typedef struct TAG_compopts {
	/* general options */
	char *progpath;			/* argv[0] */
	int verbose;
	int notmainmodule;
	struct TAG_cmd_option *dohelp;
	int doaliascheck;
	int dousagecheck;
	int dopostusagecheck;
	int dodefcheck;
	int dotracescheck;
	int domobilitycheck;

	/* debugging */
	int dmemdump;
	int dumpspecs;
	int dumptree;
	char *dumptreeto;
	int dumpstree;
	char *dumpstreeto;
	int dumplexers;
	int dumpgrammar;
	int dumpgrules;
	int dumpdfas;
	int dumpfcns;
	int dumpnames;
	int dumptargets;
	int dumpvarmaps;
	int dumpnodetypes;
	int dumpchooks;
	int dumpextns;
	int dumpnodetags;
	int dumpsnodetypes;
	int dumpsnodetags;
	int dumpfolded;
	int dumptracemem;
	int debugparser;
	int stoppoint;
	int traceparser;
	int tracenamespaces;
	int tracetypecheck;
	int traceconstprop;
	int traceprecode;
	char *tracecompops;
	char *tracelangops;
	int tracetracescheck;
	int treecheck;
	int interactive;
	char *savenameddfa[2];
	char *savealldfas;
	int fatalgdb;
	int fatalsegv;

	/* general paths */
	char *specsfile;
	char *outfile;
	DYNARRAY (char *, epath);
	DYNARRAY (char *, ipath);
	DYNARRAY (char *, lpath);
	DYNARRAY (char *, eload);

	/* compiler settings */
	char *maintainer;
	char *target_str;
	char *target_cpu;
	char *target_os;
	char *target_vendor;
	int default_target;

	/* signing/hashing */
	char *hashalgo;
	char *privkey;
	DYNARRAY (char *, trustedkeys);

	/* helper programs */
	char *gperf_p;
	char *gprolog_p;
	char *gdb_p;
	char *wget_p;

	/* additional things */
	char *cachedir;
} compopts_t;


/* various tree-walks performed by the compiler (bitfields) */
typedef enum ENUM_treewalk {
	WALK_PRESCOPE = 0x00000001,
	WALK_SCOPE = 0x00000002,
	WALK_TYPECHECK = 0x00000004,
	WALK_PRECHECK = 0x00000008,
	WALK_ALIASCHECK = 0x00000010,
	WALK_USAGECHECK = 0x00000020,
	WALK_DEFCHECK = 0x00000040,
	WALK_PREALLOCATE = 0x00000080,
	WALK_ALLOCATE = 0x00000100,
	WALK_PRECODE = 0x00000200,
	WALK_CODEGEN = 0x00000400,
	WALK_TRACESCHECK = 0x00000800,
	WALK_MOBILITYCHECK = 0x00001000
} treewalk_t;

/* used to identify arguments passed to compiler-passes */
typedef enum ENUM_comppassarg {
	CPASS_INVALID = 0x00000000,
	CPASS_TREE = 0x00000001,
	CPASS_TREEPTR = 0x00000002,
	CPASS_LANGPARSER = 0x00000004,
	CPASS_FILENAME = 0x00000008,
	CPASS_LEXFILE = 0x00000010,
	CPASS_TARGET = 0x00000020
} comppassarg_t;

extern char *progname;
extern compopts_t compopts;

struct TAG_fhandle;

/* this can be called by extensions (and other code) to add passes to the compiler */
extern int nocc_addcompilerpass (const char *name, struct TAG_origin *origin, const char *other, int before, int (*pfcn)(void *), comppassarg_t parg, int stopat, int *eflagptr);
extern int nocc_laststopat (void);

/* this is used to add initialisation functions, called after other initialisations but before extensions are loaded */
extern int nocc_addcompilerinitfunc (const char *name, struct TAG_origin *origin, int (*ifcn)(void *), void *arg);

/* this is used to add an XML namespace to the compiler -- for prefixing on dumped XML output -- only for top-level tree-dumps (and some helper routines) */
extern int nocc_addxmlnamespace (const char *name, const char *uri);
extern char *nocc_lookupxmlnamespace (const char *name);
extern int nocc_dumpxmlnamespaceheaders (struct TAG_fhandle *stream);
extern int nocc_dumpxmlnamespacefooters (struct TAG_fhandle *stream);

/* this can be used to change the default target */
extern int nocc_setdefaulttarget (const char *tcpu, const char *tvendor, const char *tos);

/* used to driving the compiler from a lower-level (e.g. interaction handlers) */
struct TAG_compcxt;			/* compiler context */
struct TAG_lexfile;
struct TAG_tnode;

extern int nocc_runfepasses (struct TAG_lexfile **lexers, struct TAG_tnode **trees, int count, int *exitmode);


#endif	/* !__NOCC_H */

