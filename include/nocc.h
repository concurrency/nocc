/*
 *	nocc.h -- global definitions for nocc
 *	Copyright (C) 2004 Fred Barnes <frmb@kent.ac.uk>
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

extern void nocc_internal (char *fmt, ...);
extern void nocc_fatal (char *fmt, ...);
extern void nocc_error (char *fmt, ...);
extern void nocc_warning (char *fmt, ...);
extern void nocc_message (char *fmt, ...);
extern void nocc_outerrmsg (char *string);

extern int nocc_dooption (char *optstr);

/* need dynamic-arrays in here */
#include "support.h"

/* this holds global options for the compiler */
typedef struct TAG_compopts {
	int verbose;
	int notmainmodule;
	int dmemdump;
	int dumpspecs;
	int dumptree;
	char *dumptreeto;
	int dumpgrammar;
	int dumpdfas;
	int dumpnames;
	int dumptargets;
	int dumpvarmaps;
	int debugparser;
	int doaliascheck;
	int dousagecheck;
	int dodefcheck;
	int stoppoint;
	char *specsfile;
	char *outfile;
	char *savenameddfa[2];
	char *savealldfas;
	DYNARRAY (char *, epath);
	DYNARRAY (char *, ipath);
	DYNARRAY (char *, lpath);
	char *maintainer;
	char *target_str;
	char *target_cpu;
	char *target_os;
	char *target_vendor;
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


extern char *progname;
extern compopts_t compopts;

#endif	/* !__NOCC_H */

