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


struct TAG_tnode;
struct TAG_compops;
struct TAG_langops;
struct TAG_prescope;
struct TAG_scope;
struct TAG_typecheck;
struct TAG_target;


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

/*{{{  tnode_t definition*/
typedef struct TAG_tnode {
	ntdef_t *tag;
	struct TAG_lexfile *org_file;
	int org_line;

	DYNARRAY (void *, items);		/* general subnotes/name-nodes/hook-nodes */
	DYNARRAY (void *, chooks);		/* compiler hooks */
} tnode_t;


/*}}}*/
/*{{{  compops_t (compiler operations)*/
typedef struct TAG_compops {
	int (*prescope)(tnode_t **, struct TAG_prescope *);		/* called before scoping */
	int (*scopein)(tnode_t **, struct TAG_scope *);			/* scopes in declarations made by this node */
	int (*scopeout)(tnode_t **, struct TAG_scope *);		/* scopes out declarations made by this node */
	int (*typecheck)(tnode_t *, struct TAG_typecheck *);		/* type-checks this node */
	tnode_t *(*typeactual)(tnode_t *, tnode_t *, tnode_t *, struct TAG_typecheck *);	/* tests whether one type is valid as an "actual" for another */
	tnode_t *(*gettype)(tnode_t *, tnode_t *);			/* returns the type of this node (second param is a "default" type) */
	int (*bytesfor)(tnode_t *);					/* returns the number of bytes required for something */
	int (*fetrans)(tnode_t **);					/* performs front-end transforms */
	int (*betrans)(tnode_t **, struct TAG_target *);		/* performs back-end transforms for target */
	int (*premap)(tnode_t **, struct TAG_map *);			/* performs pre-mapping for target */
	int (*namemap)(tnode_t **, struct TAG_map *);			/* performs name-mapping for target */
	int (*precode)(tnode_t **, struct TAG_codegen *);		/* performs pre-codegen for target */
	int (*codegen)(tnode_t *, struct TAG_codegen *);		/* performs code-generation for target */
} compops_t;


/*}}}*/
/*{{{  langops_t (language operations)*/
typedef struct TAG_langops {
	int (*getdescriptor)(tnode_t *, char **);			/* gets a descriptor string for the given node */
} langops_t;
/*}}}*/
/*{{{  chook_t definition*/
typedef struct TAG_chook {
	int id;
	char *name;

	void *(*chook_copy)(void *);
	void (*chook_free)(void *);
	void (*chook_dumptree)(tnode_t *, void *, int, FILE *);
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
extern void tnode_dumpnodetypes (FILE *stream);

extern compops_t *tnode_newcompops (void);
extern void tnode_freecompops (compops_t *cops);

extern langops_t *tnode_newlangops (void);
extern void tnode_freelangops (langops_t *lops);

extern chook_t *tnode_newchook (const char *name);
extern chook_t *tnode_lookupchookbyname (const char *name);
extern void *tnode_getchook (tnode_t *t, chook_t *ch);
extern void tnode_setchook (tnode_t *t, chook_t *ch, void *hook);

extern int tnode_bytesfor (tnode_t *t);


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

