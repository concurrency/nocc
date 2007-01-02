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

/* this holds global options for the compiler */
typedef struct TAG_compopts {
	/* general options */
	char *progpath;			/* argv[0] */
	int verbose;
	int notmainmodule;
	struct TAG_cmd_option *dohelp;
	int doaliascheck;
	int dousagecheck;
	int dodefcheck;

	/* debugging */
	int dmemdump;
	int dumpspecs;
	int dumptree;
	char *dumptreeto;
	int dumpstree;
	char *dumpstreeto;
	int dumpgrammar;
	int dumpgrules;
	int dumpdfas;
	int dumpnames;
	int dumptargets;
	int dumpvarmaps;
	int dumpnodetypes;
	int dumpchooks;
	int dumpextns;
	int dumpfolded;
	int dumptracemem;
	int debugparser;
	int stoppoint;
	int traceparser;
	int tracetypecheck;
	int treecheck;
	char *savenameddfa[2];
	char *savealldfas;

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

	/* signing/hashing */
	char *hashalgo;
	char *privkey;

	/* helper programs */
	char *gperf_p;
	char *gprolog_p;
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
	WALK_CODEGEN = 0x00000400
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

/* this can be called by extensions (and other code) to add passes to the compiler */
extern int nocc_addcompilerpass (const char *name, void *origin, const char *other, int before, int (*pfcn)(void *), comppassarg_t parg, int stopat, int *eflagptr);

/* this is used to add an XML namespace to the compiler -- for prefixing on dumped XML output -- only for top-level tree-dumps */
extern int nocc_addxmlnamespace (const char *name, const char *uri);


#endif	/* !__NOCC_H */

