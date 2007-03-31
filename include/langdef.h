/*
 *	langdef.h -- language definitions for NOCC
 *	Copyright (C) 2006-2007 Fred Barnes <frmb@kent.ac.uk>
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
 *	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#ifndef __LANGDEF_H
#define __LANGDEF_H


struct TAG_tnode;
struct TAG_lexfile;
struct TAG_dfattbl;
struct TAG_langparser;

struct TAG_langdef;


typedef enum ENUM_langdefent {
	LDE_INVALID = 0,
	LDE_GRL = 1,
	LDE_RFUNC = 2,
	LDE_DFATRANS = 3,
	LDE_DFABNF = 4,
	LDE_KEYWORD = 5,
	LDE_SYMBOL = 6,
	LDE_DFAERR = 7,
	LDE_TNODE = 8
} langdefent_e;

enum ENUM_dfaerrorreport;

typedef struct TAG_langdefent {
	struct TAG_langdef *ldef;		/* language definition this is in */
	int lineno;				/* line number from the definition */

	langdefent_e type;
	union {
		struct {
			char *name;		/* rule name */
			char *desc;		/* reduction specification (GRL) or reduction name */
		} redex;
		char *dfarule;			/* for DFATRANS and DFABNF */
		char *keyword;			/* for KEYWORD */
		char *symbol;			/* for SYMBOL */
		struct {
			char *dfaname;		/* name of the DFA rule */
			int source;		/* integer for dfaerrorsource_e */
			int rcode;		/* integer for dfaerrorreport_e */
			char *msg;		/* associated error message */
		} dfaerror;
		struct {
			char *name;			/* name of the node-type (not tags) */
			int nsub, nname, nhook;		/* sub-node, name and hook counts */
			DYNARRAY (char *, descs);	/* brief descriptions of the subnodes, names and hooks */
			char *invafter;			/* invalid after this pass in the compiler */
			char *invbefore;		/* invalid before this pass in the compiler */
		} tnode;
	} u;
} langdefent_t;

typedef struct TAG_langdefsec {
	struct TAG_langdef *ldef;		/* associated language definition */
	char *ident;
	DYNARRAY (langdefent_t *, ents);
} langdefsec_t;

typedef struct TAG_langdef {
	char *ident;
	char *desc;
	char *maintainer;
	DYNARRAY (langdefsec_t *, sections);

	langdefsec_t *cursec;			/* used when parsing */
} langdef_t;


extern langdef_t *langdef_readdefs (const char *fname);
extern void langdef_freelangdef (langdef_t *ldef);

extern langdefsec_t *langdef_findsection (langdef_t *ldef, const char *ident);
extern int langdef_hassection (langdef_t *ldef, const char *ident);

extern int langdef_init_tokens (langdefsec_t *lsec, void *origin);
extern int langdef_init_nodes (langdefsec_t *lsec, void *origin);
extern int langdef_reg_reducers (langdefsec_t *lsec);
extern struct TAG_dfattbl **langdef_init_dfatrans (langdefsec_t *lsec, int *ntrans);
extern int langdef_post_setup (langdefsec_t *lsec);
extern int langdef_treecheck_setup (langdef_t *ldef);

extern int langdef_init (void);
extern int langdef_shutdown (void);


#endif	/* !__LANGDEF_H */

