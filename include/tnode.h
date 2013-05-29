/*
 *	tnode.h -- parse tree structure for nocc
 *	Copyright (C) 2005-2008 Fred Barnes <frmb@kent.ac.uk>
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


#define TNF_NONE		0x0000
#define TNF_LONGPROC		0x0001		/* long process (e.g. SEQ/PAR, body into subnode 1) */
#define TNF_LONGDECL		0x0002		/* long declaration (e.g. PROC, body into subnode 2, in-scope into subnode 3) */
#define TNF_SHORTDECL		0x0004		/* short declaration (e.g. variable, in-scope body into subnode 2) */
#define TNF_TRANSPARENT		0x0008		/* "transparent" node (e.g. library-info) */
#define TNF_LANGMASK		0xfff0

#define NTF_NONE		0x0000
#define NTF_NAMEMAPTYPEINDECL	0x0001		/* indicates that when name-mapping a declaration with this type, map the type as well */
#define NTF_PRECODETYPEINDECL	0x0002		/* indicates that when pre-coding a declaration with this type, pre-code the type as well */
#define NTF_CODEGENTYPEINDECL	0x0004		/* indicates that when code-generating a declaration with this type, code-generate the type as well */
#define NTF_LANGMASK		0xfff0


struct TAG_tnode;
struct TAG_compops;
struct TAG_langops;
struct TAG_prescope;
struct TAG_scope;
struct TAG_typecheck;
struct TAG_target;
struct TAG_fetrans;
struct TAG_betrans;
struct TAG_treecheckdef;
struct TAG_fhandle;

/*{{{  enum copycontrol_e*/
typedef enum ENUM_copycontrol {
	COPY_ALIAS = 0x00,
	COPY_SUBS = 0x01,
	COPY_HOOKS = 0x02,
	COPY_CHOOKS = 0x04
} copycontrol_e;

/*}}}*/
/*{{{  tndef_t definition (type of node)*/
typedef struct TAG_tndef {
	char *name;
	int idx;
	int nsub;		/* number of subnodes */
	int nname;		/* number of name-nodes */
	int nhooks;		/* number of hook-nodes */

	void *(*hook_copy)(void *);
	void *(*hook_copyoralias)(void *, copycontrol_e (*)(struct TAG_tnode *));
	void (*hook_free)(void *);
	void (*hook_dumptree)(struct TAG_tnode *, void *, int, struct TAG_fhandle *);
	void (*hook_dumpstree)(struct TAG_tnode *, void *, int, struct TAG_fhandle *);
	void (*hook_postwalktree)(struct TAG_tnode *, void *, void (*)(struct TAG_tnode *, void *), void *);
	void (*hook_prewalktree)(struct TAG_tnode *, void *, int (*)(struct TAG_tnode *, void *), void *);
	void (*hook_modprewalktree)(struct TAG_tnode **, void *, int (*)(struct TAG_tnode **, void *), void *);
	void (*hook_modprepostwalktree)(struct TAG_tnode **, void *, int (*)(struct TAG_tnode **, void *), int (*)(struct TAG_tnode **, void *), void *);

	void (*prefreetree)(struct TAG_tnode *);

	struct TAG_compops *ops;		/* tree operations */
	struct TAG_langops *lops;		/* language-oriented tree operations */
	struct TAG_treecheckdef *tchkdef;	/* tree-check definition linkage */

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
struct TAG_origin;

/*{{{  srclocn_t definition*/
typedef struct TAG_srclocn {
	struct TAG_lexfile *org_file;
	int org_line;
} srclocn_t;

/*}}}*/
/*{{{  tnode_t definition*/
typedef struct TAG_tnode {
	ntdef_t *tag;
	srclocn_t *org;

	DYNARRAY (void *, items);		/* general subnotes/name-nodes/hook-nodes */
	DYNARRAY (void *, chooks);		/* compiler hooks */
} tnode_t;


/*}}}*/
/*{{{  enum compops_e*/
typedef enum ENUM_compops {
	COPS_INVALID = 0,
	COPS_ONCREATE = 1,			/* 1: tnode_t ** -> int */
	COPS_PRESCOPE = 2,			/* 2: tnode_t **, prescope_t * -> int */
	COPS_SCOPEIN = 3,			/* 2: tnode_t **, scope_t * -> int */
	COPS_SCOPEOUT = 4,			/* 2: tnode_t **, scope_t * -> int */
	COPS_TYPECHECK = 5,			/* 2: tnode_t *, typecheck_t * -> int */
	COPS_CONSTPROP = 6,			/* 1: tnode_t ** -> int */
	COPS_TYPERESOLVE = 7,			/* 2: tnode_t **, typecheck_t * -> int */
	COPS_PRECHECK = 8,			/* 1: tnode_t * -> int */
	COPS_POSTUSAGECHECK = 9,		/* 1: tnode_t ** -> int */
	COPS_TRACESCHECK = 10,			/* 2: tnode_t *, tchk_state_t * -> int */
	COPS_MOBILITYCHECK = 11,		/* 2: tnode_t *, mchk_state_t * -> int */
	COPS_POSTCHECK = 12,			/* 2: tnode_t **, postcheck_t * -> int */
	COPS_FETRANS = 13,			/* 2: tnode_t **, fetrans_t * -> int */
	COPS_BETRANS = 14,			/* 2: tnode_t **, betrans_t * -> int */
	COPS_PREMAP = 15,			/* 2: tnode_t **, map_t * -> int */
	COPS_NAMEMAP = 16,			/* 2: tnode_t **, map_t * -> int */
	COPS_BEMAP = 17,			/* 2: tnode_t **, map_t * -> int */
	COPS_PREALLOCATE = 18,			/* 2: tnode_t *, target_t * -> int */
	COPS_PRECODE = 19,			/* 2: tnode_t **, codegen_t * -> int */
	COPS_CODEGEN = 20,			/* 2: tnode_t *, codegen_t * -> int */
	COPS_LBETRANS = 21,			/* 2: tnode_t **, betrans_t * -> int */
	COPS_LPREMAP = 22,			/* 2: tnode_t **, map_t * -> int */
	COPS_LNAMEMAP = 23,			/* 2: tnode_t **, map_t * -> int */
	COPS_LBEMAP = 24,			/* 2: tnode_t **, map_t * -> int */
	COPS_LPREALLOCATE = 25,			/* 2: tnode_t *, target_t * -> int */
	COPS_LPRECODE = 26,			/* 2: tnode_t **, codegen_t * -> int */
	COPS_LCODEGEN = 27,			/* 2: tnode_t *, codegen_t * -> int */
	COPS_MAX = 256
} compops_e;

#define COPS_LAST COPS_CODEGEN

/*}}}*/
/*{{{  compopt_t,compops_t (compiler operations)*/

typedef struct TAG_compop {
	char *name;
	compops_e opno;
	int nparams;
	int dotrace;
	struct TAG_origin *origin;
} compop_t;

typedef struct TAG_compops {
	struct TAG_compops *next;
	DYNARRAY (void *, opfuncs);
	int passthrough;
} compops_t;


/*}}}*/
/*{{{  enum langops_e*/
typedef enum ENUM_langops {
	LOPS_INVALID = 0,
	LOPS_GETDESCRIPTOR = 1,			/* 2: tnode_t *, char ** -> int */
	LOPS_GETNAME = 2,			/* 2: tnode_t *, char ** -> int */
	LOPS_DO_USAGECHECK = 3,			/* 2: tnode_t *, uchk_state_t * -> int */
	LOPS_TYPEACTUAL = 4,			/* 4: tnode_t *, tnode_t *, tnode_t *, typecheck_t * -> tnode_t * */
	LOPS_TYPEREDUCE = 5,			/* 1: tnode_t * -> tnode_t * */
	LOPS_CANTYPECAST = 6,			/* 2: tnode_t *, tnode_t * -> int */
	LOPS_GETTYPE = 7,			/* 2: tnode_t *, tnode_t * -> tnode_t * */
	LOPS_GETSUBTYPE = 8,			/* 2: tnode_t *, tnode_t * -> tnode_t * */
	LOPS_BYTESFOR = 9,			/* 2: tnode_t *, target_t * -> int */
	LOPS_ISSIGNED = 10,			/* 2: tnode_t *, target_t * -> int */
	LOPS_ISCONST = 11,			/* 1: tnode_t * -> int */
	LOPS_ISVAR = 12,			/* 1: tnode_t * -> int */
	LOPS_ISTYPE = 13,			/* 1: tnode_t * -> int */
	LOPS_ISCOMPLEX = 14,			/* 2: tnode_t *, int -> int */
	LOPS_CONSTVALOF = 15,			/* 2: tnode_t *, void * -> int */
	LOPS_CONSTSIZEOF = 16,			/* 1: tnode_t * -> int */
	LOPS_VALBYREF = 17,			/* 1: tnode_t * -> int */
	LOPS_INITSIZES = 18,			/* 7: tnode_t *, tnode_t *, int *, int *, int *, int *, map_t * -> int */
	LOPS_INITIALISING_DECL = 19,		/* 3: tnode_t *, tnode_t *, codegen_t * -> int */
	LOPS_CODEGEN_TYPEACTION = 20,		/* 3: tnode_t *, tnode_t *, codegen_t * -> int */
	LOPS_CODEGEN_TYPERANGECHECK = 21,	/* 2: tnode_t *, codegen_t * -> int */
	LOPS_CODEGEN_ALTPREENABLE = 22,		/* 3: tnode_t *, int, codegen_t * -> int */
	LOPS_CODEGEN_ALTENABLE = 23,		/* 3: tnode_t *, int, codegen_t * -> int */
	LOPS_CODEGEN_ALTDISABLE = 24,		/* 4: tnode_t *, int, int, codegen_t * -> int */
	LOPS_PREMAP_TYPEFORVARDECL = 25,	/* 3: tnode_t *, tnode_t *, map_t * -> int */
	LOPS_RETYPECONST = 26,			/* 2: tnode_t *, tnode_t * -> tnode_t * */
	LOPS_DIMTREEOF = 27,			/* 1: tnode_t * -> tnode_t * */
	LOPS_HIDDENPARAMSOF = 28,		/* 1: tnode_t * -> tnode_t * */
	LOPS_HIDDENSLOTSOF = 29,		/* 1: tnode_t * -> int */
	LOPS_TYPEHASH = 30,			/* 3: tnode_t *, int, void * -> int */
	LOPS_TYPETYPE = 31,			/* 1: tnode_t * -> typecat_e */
	LOPS_GETBASENAME = 32,			/* 1: tnode_t * -> tnode_t * */
	LOPS_GETFIELDNAME = 33,			/* 1: tnode_t * -> tnode_t * */
	LOPS_ISCOMMUNICABLE = 34,		/* 1: tnode_t * -> int */
	LOPS_PROTOCOLTOTYPE = 35,		/* 1: tnode_t * -> tnode_t * */
	LOPS_GETTAGS = 36,			/* 1: tnode_t * -> tnode_t * */
	LOPS_NAMEOF = 37,			/* 1: tnode_t * -> name_t * */
	LOPS_TRACESPECOF = 38,			/* 1: tnode_t * -> tnode_t * */
	LOPS_DIMTREEOF_NODE = 39,		/* 2: tnode_t *, tnode_t * -> tnode_t * */
	LOPS_BYTESFORPARAM = 40,		/* 2: tnode_t *, target_t * -> int */
	LOPS_GETCTYPEOF = 41,			/* 2: tnode_t *, char ** -> int */
	LOPS_KNOWNSIZEOF = 42,			/* 1: tnode_t * -> int */
	LOPS_GUESSTLP = 43,			/* 1: tnode_t * -> int */
	LOPS_ISADDRESSABLE = 44,		/* 1: tnode_t * -> int */
	LOPS_MAX = 256
} langops_e;

#define LOPS_LAST LOPS_GETCTYPEOF

/*}}}*/
/*{{{  langop_t, langops_t (language operations)*/
typedef struct TAG_langop {
	char *name;
	langops_e opno;
	int nparams;
	int dotrace;
	struct TAG_origin *origin;
} langop_t;

typedef struct TAG_langops {
	struct TAG_langops *next;
	DYNARRAY (void *, opfuncs);
	int passthrough;
} langops_t;

/*}}}*/
/*{{{  chookflags_e type (compiler-hook flags)*/
typedef enum ENUM_chookflags {
	CHOOK_NONE = 0x0000,
	CHOOK_AUTOPROMOTE = 0x0001,
} chookflags_e;

/*}}}*/
/*{{{  chook_t definition*/
typedef struct TAG_chook {
	int id;
	char *name;
	chookflags_e flags;

	void *(*chook_copy)(void *);
	void (*chook_free)(void *);
	void (*chook_dumptree)(tnode_t *, void *, int, struct TAG_fhandle *);
	void (*chook_dumpstree)(tnode_t *, void *, int, struct TAG_fhandle *);
} chook_t;


/*}}}*/


extern int tnode_init (void);
extern int tnode_shutdown (void);

extern srclocn_t *tnode_newsrclocn (struct TAG_lexfile *lf, int line);
extern srclocn_t *tnode_cursrclocn (struct TAG_lexfile *lf);

#define SLOCN(lf) tnode_cursrclocn(lf)
#define SLOCL(lf,line) tnode_newsrclocn(lf,line)
#define SLOCI tnode_newsrclocn(lexer_internal(__FILE__),__LINE__)

extern tndef_t *tnode_newnodetype (char *name, int *idx, int nsub, int nname, int nhooks, int flags);
extern ntdef_t *tnode_newnodetag (char *name, int *idx, tndef_t *type, int flags);
extern tndef_t *tnode_lookupnodetype (char *name);
extern ntdef_t *tnode_lookupnodetag (char *name);
extern tndef_t *tnode_lookupornewnodetype (char *name, int *idx, int nsub, int nname, int nhooks, int flags);
extern ntdef_t *tnode_lookupornewnodetag (char *name, int *idx, tndef_t *type, int flags);

extern void tnode_changetag (tnode_t *t, ntdef_t *newtag);
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
extern void tnode_modpostwalktree (tnode_t **t, int (*fcn)(tnode_t **, void *), void *arg);
extern void tnode_modprepostwalktree (tnode_t **t, int (*prefcn)(tnode_t **, void *), int (*postfcn)(tnode_t **, void *), void *arg);

extern tnode_t *tnode_new (ntdef_t *tag, srclocn_t *src);
extern tnode_t *tnode_newfrom (ntdef_t *tag, tnode_t *src);
extern tnode_t *tnode_create (ntdef_t *tag, srclocn_t *src, ...);
extern tnode_t *tnode_createfrom (ntdef_t *tag, tnode_t *src, ...);
extern tnode_t *tnode_copyoraliastree (tnode_t *t, copycontrol_e (*cora_fcn)(tnode_t *));
extern tnode_t *tnode_copytree (tnode_t *t);
extern int tnode_substitute (tnode_t **tptr, tnode_t *curptr, tnode_t *newptr);
extern void tnode_free (tnode_t *t);
extern void tnode_dumptree (tnode_t *t, int indent, struct TAG_fhandle *stream);
extern void tnode_dumpstree (tnode_t *t, int indent, struct TAG_fhandle *stream);
extern void tnode_dumpnodetypes (struct TAG_fhandle *stream);

extern int tnode_setcompop (compops_t *cops, char *name, int nparams, int (*fcn)(compops_t *, ...));
extern int tnode_setcompop_bottom (compops_t *cops, char *name, int nparams, int (*fcn)(compops_t *, ...));
extern int tnode_hascompop (compops_t *cops, char *name);
extern int tnode_callcompop (compops_t *cops, char *name, int nparams, ...);
extern int tnode_hascompop_i (compops_t *cops, int idx);
extern int tnode_callcompop_i (compops_t *cops, int idx, int nparams, ...);
extern int tnode_newcompop (char *name, compops_e opno, int nparams, struct TAG_origin *origin);
extern compop_t *tnode_findcompop (char *name);
extern void tnode_dumpcompops (compops_t *cops, struct TAG_fhandle *stream);

#define COMPOPTYPE(X) ((int (*)(compops_t *, ...))(X))

extern int tnode_setlangop (langops_t *lops, char *name, int nparams, int (*fcn)(langops_t *, ...));
extern int tnode_haslangop (langops_t *lops, char *name);
extern int tnode_calllangop (langops_t *lops, char *name, int nparams, ...);
extern int tnode_haslangop_i (langops_t *lops, int idx);
extern int tnode_calllangop_i (langops_t *lops, int idx, int nparams, ...);
extern int tnode_newlangop (char *name, langops_e opno, int nparams, struct TAG_origin *origin);
extern langop_t *tnode_findlangop (char *name);

#define LANGOPTYPE(X) ((int (*)(langops_t *, ...))(X))

extern compops_t *tnode_newcompops (void);
extern compops_t *tnode_newcompops_passthrough (void);
extern void tnode_freecompops (compops_t *cops);
extern compops_t *tnode_insertcompops (compops_t *nextcops);
extern compops_t *tnode_removecompops (compops_t *cops);

extern langops_t *tnode_newlangops (void);
extern langops_t *tnode_newlangops_passthrough (void);
extern void tnode_freelangops (langops_t *lops);
extern langops_t *tnode_insertlangops (langops_t *nextlops);
extern langops_t *tnode_removelangops (langops_t *lops);

extern chook_t *tnode_newchook (const char *name);
extern chook_t *tnode_lookupchookbyname (const char *name);
extern chook_t *tnode_lookupornewchook (const char *name);
extern void *tnode_getchook (tnode_t *t, chook_t *ch);
extern int tnode_haschook (tnode_t *t, chook_t *ch);
extern void tnode_setchook (tnode_t *t, chook_t *ch, void *hook);
extern void tnode_clearchook (tnode_t *, chook_t *ch);
extern void tnode_dumpchooks (struct TAG_fhandle *stream);

extern int tnode_promotechooks (tnode_t *tsource, tnode_t *tdest);
extern char *tnode_copytextlocationof (tnode_t *t);
extern char *tnode_statictextlocationof (tnode_t *t);

extern int tnode_bytesfor (tnode_t *t, struct TAG_target *target);
extern int tnode_issigned (tnode_t *t, struct TAG_target *target);
extern int tnode_knownsizeof (tnode_t *t);

extern void tnode_message (tnode_t *t, const char *fmt, ...) __attribute__ ((format (printf, 2, 3)));
extern void tnode_warning (tnode_t *t, const char *fmt, ...) __attribute__ ((format (printf, 2, 3)));
extern void tnode_error (tnode_t *t, const char *fmt, ...) __attribute__ ((format (printf, 2, 3)));

extern void tnode_dumpsnodetypes (struct TAG_fhandle *stream);
extern void tnode_dumpsnodetags (struct TAG_fhandle *stream);


/* access routines */

#ifdef TREE_ACCESS_CHECK
#else	/* !TREE_ACCESS_CHECK */
#define CHECKREAD(N,T)	(N)
#define CHECKWRITE(N,T)	(N)
#endif	/* !TREE_ACCESS_CHECK */


/*{{{  generic tree macros*/
#define TagOf(N)		(N)->tag
#define TypeOf(N)		(N)->tag->ndef
#define OrgOf(N)		(N)->org
// #define OrgFileOf(N)		(N)->org_file
// #define OrgLineOf(N)		(N)->org_line

#define SetTag(N,V)		(N)->tag = (V)
// #define SetOrgFile(N,V)		(N)->org_file = (V)
// #define SetOrgLine(N,V)		(N)->org_line = (V)
/*}}}*/


#endif	/* !__TNODE_H */

