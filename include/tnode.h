/*
 *	tnode.h -- parse tree structure for nocc
 *	Copyright (C) 2005 Fred Barnes <frmb@kent.ac.uk>
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

#ifndef __TNODE_H
#define __TNODE_H


#define TNF_NONE	0x0000
#define TNF_LONGPROC	0x0001		/* long process (e.g. SEQ/PAR, body into subnode 1) */
#define TNF_LONGDECL	0x0002		/* long declaration (e.g. PROC, body into subnode 2, in-scope into subnode 3) */
#define TNF_SHORTDECL	0x0004		/* short declaration (e.g. variable, in-scope body into subnode 2) */
#define TNF_TRANSPARENT	0x0008		/* "transparent" node (e.g. library-info) */
#define TNF_LANGMASK	0xfff0

#define NTF_NONE	0x0000
#define NTF_LANGMASK	0xfff0


struct TAG_tnode;
struct TAG_compops;
struct TAG_langops;
struct TAG_prescope;
struct TAG_scope;
struct TAG_typecheck;
struct TAG_target;
struct TAG_fetrans;
struct TAG_betrans;

/*{{{  tndef_t definition (type of node)*/
typedef struct TAG_tndef {
	char *name;
	int idx;
	int nsub;		/* number of subnodes */
	int nname;		/* number of name-nodes */
	int nhooks;		/* number of hook-nodes */

	void *(*hook_copy)(void *);
	void (*hook_free)(void *);
	void (*hook_dumptree)(struct TAG_tnode *, void *, int, FILE *);
	void (*hook_dumpstree)(struct TAG_tnode *, void *, int, FILE *);
	void (*hook_postwalktree)(struct TAG_tnode *, void *, void (*)(struct TAG_tnode *, void *), void *);
	void (*hook_prewalktree)(struct TAG_tnode *, void *, int (*)(struct TAG_tnode *, void *), void *);
	void (*hook_modprewalktree)(struct TAG_tnode **, void *, int (*)(struct TAG_tnode **, void *), void *);
	void (*hook_modprepostwalktree)(struct TAG_tnode **, void *, int (*)(struct TAG_tnode **, void *), int (*)(struct TAG_tnode **, void *), void *);

	void (*prefreetree)(struct TAG_tnode *);

	struct TAG_compops *ops;	/* tree operations */
	struct TAG_langops *lops;	/* language-oriented tree operations */

	int tn_flags;
} tndef_t;
/*}}}*/
/*{{{  ntdef_t definition (node tag)*/
typedef struct TAG_ntdef {
	char *name;
	int idx;
	tndef_t *ndef;
	int nt_flags;
} ntdef_t;
/*}}}*/


struct TAG_lexfile;
struct TAG_name;
struct TAG_map;
struct TAG_codegen;
struct TAG_uchk_state;

/*{{{  tnode_t definition*/
typedef struct TAG_tnode {
	ntdef_t *tag;
	struct TAG_lexfile *org_file;
	int org_line;

	DYNARRAY (void *, items);		/* general subnotes/name-nodes/hook-nodes */
	DYNARRAY (void *, chooks);		/* compiler hooks */
} tnode_t;


/*}}}*/
/*{{{  enum compops_e*/
typedef enum ENUM_compops {
	COPS_INVALID = 0,
	COPS_PRESCOPE = 1,
	COPS_SCOPEIN = 2,
	COPS_SCOPEOUT = 3,
	COPS_TYPECHECK = 4,
	COPS_CONSTPROP = 5,
	COPS_PRECHECK = 6,
	COPS_FETRANS = 7,
	COPS_BETRANS = 8,
	COPS_PREMAP = 9,
	COPS_NAMEMAP = 10,
	COPS_BEMAP = 11,
	COPS_PREALLOCATE = 12,
	COPS_PRECODE = 13,
	COPS_CODEGEN = 14,
	COPS_MAX = 256
} compops_e;

#define COPS_LAST COPS_CODEGEN

/*}}}*/
/*{{{  compopt_t,compops_t (compiler operations)*/

typedef struct TAG_compop {
	char *name;
	compops_e opno;
	int nparams;
	void *origin;
} compop_t;

typedef struct TAG_compops {
	struct TAG_compops *next;
	DYNARRAY (void *, opfuncs);
#if 0
	int (*prescope)(tnode_t **, struct TAG_prescope *);		/* called before scoping */
	int (*scopein)(tnode_t **, struct TAG_scope *);			/* scopes in declarations made by this node */
	int (*scopeout)(tnode_t **, struct TAG_scope *);		/* scopes out declarations made by this node */
	int (*typecheck)(tnode_t *, struct TAG_typecheck *);		/* type-checks this node */
	int (*constprop)(tnode_t **);					/* performs constant-propagation on the node (mod-post-walk) */
	int (*precheck)(tnode_t *);					/* performs pre-checks on the node */
	int (*fetrans)(tnode_t **, struct TAG_fetrans *);		/* performs front-end transforms */
	int (*betrans)(tnode_t **, struct TAG_betrans *);		/* performs back-end transforms for target */
	int (*premap)(tnode_t **, struct TAG_map *);			/* performs pre-mapping for target */
	int (*namemap)(tnode_t **, struct TAG_map *);			/* performs name-mapping for target */
	int (*bemap)(tnode_t **, struct TAG_map *);			/* performs back-end-mapping for target */
	int (*preallocate)(tnode_t *, struct TAG_target *);		/* performs pre-allocations for target */
	int (*precode)(tnode_t **, struct TAG_codegen *);		/* performs pre-codegen for target */
	int (*codegen)(tnode_t *, struct TAG_codegen *);		/* performs code-generation for target */
#endif
} compops_t;


/*}}}*/
/*{{{  enum langops_e*/
typedef enum ENUM_langops {
	LOPS_INVALID = 0,
	LOPS_GETDESCRIPTOR = 1,
	LOPS_GETNAME = 2,
	LOPS_DO_USAGECHECK = 3,
	LOPS_TYPEACTUAL = 4,
	LOPS_TYPEREDUCE = 5,
	LOPS_GETTYPE = 6,
	LOPS_BYTESFOR = 7,
	LOPS_ISSIGNED = 8,
	LOPS_ISCONST = 9,
	LOPS_ISCOMPLEX = 10,
	LOPS_CONSTVALOF = 11,
	LOPS_VALBYREF = 12,
	LOPS_INITSIZES = 13,
	LOPS_INITIALISING_DECL = 14,
	LOPS_CODEGEN_TYPEACTION = 15
} langops_e;


/*}}}*/
/*{{{  langops_t (language operations)*/
typedef struct TAG_langops {
	struct TAG_langops *next;

	int (*getdescriptor)(tnode_t *, char **);			/* gets a descriptor string for the given node */
	int (*getname)(tnode_t *, char **);				/* gets the name of a node (for error reporting) */
	int (*do_usagecheck)(tnode_t *, struct TAG_uchk_state *);	/* does usage-checking for a node */
	tnode_t *(*typeactual)(tnode_t *, tnode_t *, tnode_t *, struct TAG_typecheck *);	/* tests whether one type is valid as an "actual" for another */
	tnode_t *(*typereduce)(tnode_t *);				/* returns the reduced type */
	tnode_t *(*gettype)(tnode_t *, tnode_t *);			/* returns the type of this node (second param is a "default" type) */
	int (*bytesfor)(tnode_t *, struct TAG_target *);		/* returns the number of bytes required for something (target given if available) */
	int (*issigned)(tnode_t *, struct TAG_target *);		/* returns the "signedness" of something (target given if available) */
	int (*isconst)(tnode_t *);					/* returns non-zero if the node is a known constant (returns width) */
	int (*iscomplex)(tnode_t *, int);				/* returns non-zero if the node (typically an expr) is considered "complex" (e.g. non-const constructors, function instances, etc.) */
	int (*constvalof)(tnode_t *, void *);				/* gets constant value for the given node (assigns to pointed-at space) */
	int (*valbyref)(tnode_t *);					/* returns non-zero if VAL of this is treated as a reference (wide types) */
	int (*initsizes)(tnode_t *, tnode_t *, int *, int *, int *, int *, struct TAG_map *);	/* returns special allocation sizing for types (type, declnode, wssize, vssize, mssize, indir, map-data) */
	int (*initialising_decl)(tnode_t *, tnode_t *, struct TAG_map *);	/* called when mapping to hook in initialiser code */

	int (*codegen_typeaction)(tnode_t *, tnode_t *, struct TAG_codegen *);	/* handle type-specific action (assignment, input, output) */
} langops_t;


/*}}}*/
/*{{{  chook_t definition*/
typedef struct TAG_chook {
	int id;
	char *name;

	void *(*chook_copy)(void *);
	void (*chook_free)(void *);
	void (*chook_dumptree)(tnode_t *, void *, int, FILE *);
	void (*chook_dumpstree)(tnode_t *, void *, int, FILE *);
} chook_t;


/*}}}*/


extern void tnode_init (void);
extern void tnode_shutdown (void);

extern tndef_t *tnode_newnodetype (char *name, int *idx, int nsub, int nname, int nhooks, int flags);
extern ntdef_t *tnode_newnodetag (char *name, int *idx, tndef_t *type, int flags);
extern tndef_t *tnode_lookupnodetype (char *name);
extern ntdef_t *tnode_lookupnodetag (char *name);

extern void tnode_setnthsub (tnode_t *t, int i, tnode_t *subnode);
extern void tnode_setnthname (tnode_t *t, int i, struct TAG_name *name);
extern void tnode_setnthhook (tnode_t *t, int i, void *hook);
extern tnode_t *tnode_nthsubof (tnode_t *t, int i);
extern tnode_t **tnode_subnodesof (tnode_t *, int *nnodes);
extern struct TAG_name *tnode_nthnameof (tnode_t *t, int i);
extern void *tnode_nthhookof (tnode_t *t, int i);
extern tnode_t **tnode_nthsubaddr (tnode_t *t, int i);
extern struct TAG_name **tnode_nthnameaddr (tnode_t *t, int i);
extern void **tnode_nthhookaddr (tnode_t *t, int i);

extern int tnode_tnflagsof (tnode_t *);
extern int tnode_ntflagsof (tnode_t *t);

extern void tnode_postwalktree (tnode_t *t, void (*fcn)(tnode_t *, void *), void *arg);
extern void tnode_prewalktree (tnode_t *t, int (*fcn)(tnode_t *, void *), void *arg);
extern void tnode_modprewalktree (tnode_t **t, int (*fcn)(tnode_t **, void *), void *arg);
extern void tnode_modprepostwalktree (tnode_t **t, int (*prefcn)(tnode_t **, void *), int (*postfcn)(tnode_t **, void *), void *arg);

extern tnode_t *tnode_new (ntdef_t *tag, struct TAG_lexfile *lf);
extern tnode_t *tnode_newfrom (ntdef_t *tag, tnode_t *src);
extern tnode_t *tnode_create (ntdef_t *tag, struct TAG_lexfile *lf, ...);
extern tnode_t *tnode_createfrom (ntdef_t *tag, tnode_t *src, ...);
extern tnode_t *tnode_copytree (tnode_t *t);
extern void tnode_free (tnode_t *t);
extern void tnode_dumptree (tnode_t *t, int indent, FILE *stream);
extern void tnode_dumpstree (tnode_t *t, int indent, FILE *stream);
extern void tnode_dumpnodetypes (FILE *stream);

extern int tnode_setcompop (compops_t *cops, char *name, int nparams, int (*fcn)(compops_t *, ...));
extern int tnode_hascompop (compops_t *cops, char *name);
extern int tnode_callcompop (compops_t *cops, char *name, int nparams, ...);
extern int tnode_hascompop_i (compops_t *cops, int idx);
extern int tnode_callcompop_i (compops_t *cops, int idx, int nparams, ...);
extern int tnode_newcompop (char *name, compops_e opno, int nparams, void *origin);
extern compop_t *tnode_findcompop (char *name);

#define COMPOPTYPE(X) ((int (*)(compops_t *, ...))(X))

extern compops_t *tnode_newcompops (void);
extern void tnode_freecompops (compops_t *cops);
extern compops_t *tnode_insertcompops (compops_t *nextcops);
extern compops_t *tnode_removecompops (compops_t *cops);

extern langops_t *tnode_newlangops (void);
extern void tnode_freelangops (langops_t *lops);
extern langops_t *tnode_insertlangops (langops_t *nextlops);
extern langops_t *tnode_removelangops (langops_t *lops);

extern chook_t *tnode_newchook (const char *name);
extern chook_t *tnode_lookupchookbyname (const char *name);
extern chook_t *tnode_lookupornewchook (const char *name);
extern void *tnode_getchook (tnode_t *t, chook_t *ch);
extern void tnode_setchook (tnode_t *t, chook_t *ch, void *hook);
extern void tnode_clearchook (tnode_t *, chook_t *ch);
extern void tnode_dumpchooks (FILE *stream);

extern int tnode_bytesfor (tnode_t *t, struct TAG_target *target);
extern int tnode_issigned (tnode_t *t, struct TAG_target *target);

extern void tnode_warning (tnode_t *t, const char *fmt, ...);
extern void tnode_error (tnode_t *t, const char *fmt, ...);


/* access routines */

#ifdef TREE_ACCESS_CHECK
#else	/* !TREE_ACCESS_CHECK */
#define CHECKREAD(N,T)	(N)
#define CHECKWRITE(N,T)	(N)
#endif	/* !TREE_ACCESS_CHECK */


/*{{{  generic tree macros*/
#define TagOf(N)		(N)->tag
#define TypeOf(N)		(N)->tag->ndef
#define OrgFileOf(N)		(N)->org_file
#define OrgLineOf(N)		(N)->org_line

#define SetTag(N,V)		(N)->tag = (V)
#define SetOrgFile(N,V)		(N)->org_file = (V)
#define SetOrgLine(N,V)		(N)->org_line = (V)
/*}}}*/


#endif	/* !__TNODE_H */

