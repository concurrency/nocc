/*
 *	tnode.c -- parser node functions
 *	Copyright (C) 2004-2005 Fred Barnes <frmb@kent.ac.uk>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/types.h>

#include "nocc.h"
#include "support.h"
#include "version.h"
#include "symbols.h"
#include "keywords.h"
#include "lexer.h"
#include "tnode.h"
#if 0
#include "parser.h"
#include "parsepriv.h"
#endif
#include "lexpriv.h"
#include "occampi.h"
#include "names.h"

/*{{{  private stuff*/
STATICSTRINGHASH (tndef_t *, nodetypes, 5);
STATICDYNARRAY (tndef_t *, anodetypes);

STATICSTRINGHASH (ntdef_t *, nodetags, 6);
STATICDYNARRAY (ntdef_t *, anodetags);

STATICSTRINGHASH (chook_t *, comphooks, 4);
STATICDYNARRAY (chook_t *, acomphooks);

/* forwards */
static void tnode_isetindent (FILE *stream, int indent);

/*}}}*/

/*{{{  constant node hook functions*/
/*{{{  static void tnode_const_hookfree (void *hook)*/
/*
 *	hook-free function for constant nodes
 */
static void tnode_const_hookfree (void *hook)
{
	sfree (hook);
	return;
}
/*}}}*/
/*{{{  static void *tnode_const_hookcopy (void *hook)*/
/*
 *	hook-copy function for constant nodes
 */
static void *tnode_const_hookcopy (void *hook)
{
	/* constant hook-nodes are always 32-bytes ...  :) */
	void *copy = smalloc (32);

	memcpy (copy, hook, 32);
	return copy;
}
/*}}}*/
/*{{{  static void tnode_const_hookdumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	hook-dumptree function for constant nodes
 */
static void tnode_const_hookdumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	tnode_isetindent (stream, indent);
	fprintf (stream, "<hook ptr=\"0x%8.8x\" />\n", (unsigned int)hook);

	return;
}
/*}}}*/
/*}}}*/
/*{{{  list node hook functions*/
/*
 *	note: lists are slightly opaque DYNARRY's,  cur and max occupy the first
 *	two elements.
 */

/*{{{  static void tnode_list_hookfree (void *hook)*/
/*
 *	hook-free function for list nodes
 */
static void tnode_list_hookfree (void *hook)
{
	tnode_t **array = (tnode_t **)hook;
	int *cur, *max;
	int i;

	if (!array) {
		/* nothing to do! */
		return;
	}
	cur = (int *)hook;
	max = (int *)hook + 1;
	array += 2;

	for (i=0; i<*cur; i++) {
		tnode_free (array[i]);
	}
	*cur = 0;
	sfree (hook);

	return;
}
/*}}}*/
/*{{{  static void *tnode_list_hookcopy (void *hook)*/
/*
 *	hook-copy function for list nodes
 */
static void *tnode_list_hookcopy (void *hook)
{
	tnode_t **array = (tnode_t **)hook;
	tnode_t **narray;
	void *rhook;
	int *cur, *max;
	int *nacur, *namax;
	int i;

	if (!array) {
		return NULL;
	}
	cur = (int *)array;
	max = (int *)array + 1;
	array += 2;

	narray = (tnode_t **)smalloc ((*max + 2) * sizeof (tnode_t *));
	nacur = (int *)narray;
	namax = (int *)narray + 1;
	*nacur = *cur;
	*namax = *max;
	rhook = (void *)narray;
	narray += 2;

	for (i=0; i<*cur; i++) {
		narray[i] = tnode_copytree (array[i]);
	}
	for (; i<*namax; i++) {
		narray[i] = NULL;
	}

	return rhook;
}
/*}}}*/
/*{{{  static void tnode_list_hookdumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	hook-dumptree function for a list
 */
static void tnode_list_hookdumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	tnode_t **array = (tnode_t **)hook;
	int *cur, *max;
	int i;

	if (!array) {
		/* nothing to do! */
		return;
	}
	cur = (int *)hook;
	max = (int *)hook + 1;
	array += 2;

	for (i=0; i<*cur; i++) {
		tnode_dumptree (array[i], indent, stream);
	}

	return;
}
/*}}}*/
/*{{{  static void tnode_list_hookpostwalktree (tnode_t *node, void *hook, void (*func)(tnode_t *, void *), void *voidptr)*/
/*
 *	hook-postwalktree function for a list
 */
static void tnode_list_hookpostwalktree (tnode_t *node, void *hook, void (*func)(tnode_t *, void *), void *voidptr)
{
	tnode_t **array = (tnode_t **)hook;
	int *cur, *max;
	int i;

	if (!array) {
		/* nothing to do! */
		return;
	}
	cur = (int *)hook;
	max = (int *)hook + 1;
	array += 2;

	for (i=0; i<*cur; i++) {
		tnode_postwalktree (array[i], func, voidptr);
	}

	return;
}
/*}}}*/
/*{{{  static void tnode_list_hookprewalktree (tnode_t *node, void *hook, int (*func)(tnode_t *, void *), void *voidptr)*/
/*
 *	hook-prewalktree function for a list
 */
static void tnode_list_hookprewalktree (tnode_t *node, void *hook, int (*func)(tnode_t *, void *), void *voidptr)
{
	tnode_t **array = (tnode_t **)hook;
	int *cur, *max;
	int i;

	if (!array) {
		/* nothing to do! */
		return;
	}
	cur = (int *)hook;
	max = (int *)hook + 1;
	array += 2;

	for (i=0; i<*cur; i++) {
		tnode_prewalktree (array[i], func, voidptr);
	}

	return;
}
/*}}}*/
/*{{{  static void tnode_list_hookmodprewalktree (tnode_t **node, void *hook, int (*func)(tnode_t **, void *), void *voidptr)*/
/*
 *	hook-prewalktree function for a list
 */
static void tnode_list_hookmodprewalktree (tnode_t **node, void *hook, int (*func)(tnode_t **, void *), void *voidptr)
{
	tnode_t **array = (tnode_t **)hook;
	int *cur, *max;
	int i;

	if (!array) {
		/* nothing to do! */
		return;
	}
	cur = (int *)hook;
	max = (int *)hook + 1;
	array += 2;

	for (i=0; i<*cur; i++) {
		tnode_modprewalktree (array + i, func, voidptr);
	}

	return;
}
/*}}}*/
/*{{{  static void tnode_list_hookmodprepostwalktree (tnode_t **node, void *hook, int (*prefunc)(tnode_t **, void *), void (*postfunc)(tnode_t **, void *), void *voidptr)*/
/*
 *	hook-prewalktree function for a list
 */
static void tnode_list_hookmodprepostwalktree (tnode_t **node, void *hook, int (*prefunc)(tnode_t **, void *), int (*postfunc)(tnode_t **, void *), void *voidptr)
{
	tnode_t **array = (tnode_t **)hook;
	int *cur, *max;
	int i;

	if (!array) {
		/* nothing to do! */
		return;
	}
	cur = (int *)hook;
	max = (int *)hook + 1;
	array += 2;

	for (i=0; i<*cur; i++) {
		tnode_modprepostwalktree (array + i, prefunc, postfunc, voidptr);
	}

	return;
}
/*}}}*/
/*}}}*/

/*{{{  void tnode_init (void)*/
/*
 *	initialises node handler
 */
void tnode_init (void)
{
	tndef_t *tnd;
	ntdef_t *ntd;
	int i;

	stringhash_init (nodetypes);
	dynarray_init (anodetypes);
	stringhash_init (nodetags);
	dynarray_init (anodetags);

	stringhash_init (comphooks);
	dynarray_init (acomphooks);

	/* setup the static node types */
	i = -1;
	tnd = tnode_newnodetype ("nonodetype", &i, 0, 0, 0, 0);
	i = -1;
	ntd = tnode_newnodetag ("nonode", &i, tnd, 0);

	i = -1;
	tnd = tnode_newnodetype ("namenode", &i, 0, 1, 0, 0);
	i = -1;
	ntd = tnode_newnodetag ("name", &i, tnd, 0);

	i = -1;
	tnd = tnode_newnodetype ("constnode", &i, 0, 0, 1, 0);
	tnd->hook_free = tnode_const_hookfree;
	tnd->hook_copy = tnode_const_hookcopy;
	tnd->hook_dumptree = tnode_const_hookdumptree;
	i = -1;
	ntd = tnode_newnodetag ("intlit", &i, tnd, 0);

	i = -1;
	tnd = tnode_newnodetype ("listnode", &i, 0, 0, 1, 0);
	tnd->hook_free = tnode_list_hookfree;
	tnd->hook_copy = tnode_list_hookcopy;
	tnd->hook_dumptree = tnode_list_hookdumptree;
	tnd->hook_postwalktree = tnode_list_hookpostwalktree;
	tnd->hook_prewalktree = tnode_list_hookprewalktree;
	tnd->hook_modprewalktree = tnode_list_hookmodprewalktree;
	tnd->hook_modprepostwalktree = tnode_list_hookmodprepostwalktree;
	i = -1;
	ntd = tnode_newnodetag ("list", &i, tnd, 0);

	return;
}
/*}}}*/
/*{{{  void tnode_shutdown (void)*/
/*
 *	shuts-down node handler
 */
void tnode_shutdown (void)
{
	return;
}
/*}}}*/

/*{{{  tndef_t *tnode_newnodetype (char *name, int *idx, int nsub, int nname, int nhooks, int flags)*/
/*
 *	creates a new node-type
 */
tndef_t *tnode_newnodetype (char *name, int *idx, int nsub, int nname, int nhooks, int flags)
{
	tndef_t *tnd;

	tnd = tnode_lookupnodetype (name);
	if (!tnd) {
		tnd = (tndef_t *)smalloc (sizeof (tndef_t));
		tnd->name = string_dup (name);
		if (*idx > -1) {
			/* want specific placement in the array */
			if (*idx >= DA_CUR (anodetypes)) {
				/* not a problem */
				dynarray_setsize (anodetypes, *idx + 1);
			} else if (DA_NTHITEM (anodetypes, *idx)) {
				/* something already here -- won't evict it */
				int i;

				for (i=0; (i < DA_CUR (anodetypes)) && DA_NTHITEM (anodetypes, i); i++);
				if (i == DA_CUR (anodetypes)) {
					dynarray_setsize (anodetypes, i + 1);
				}
				nocc_warning ("tnode_newnodetype(): wanted index %d for node type [%s], got %d instead.", *idx, name, i);
				*idx = i;
			} /* else free :) */
		} else {
			/* search for free slot */
			int i;

			for (i=0; (i < DA_CUR (anodetypes)) && DA_NTHITEM (anodetypes, i); i++);
			if (i == DA_CUR (anodetypes)) {
				dynarray_setsize (anodetypes, i + 1);
			}
			*idx = i;
		}
		tnd->idx = *idx;
		tnd->nsub = nsub;
		tnd->nname = nname;
		tnd->nhooks = nhooks;

		tnd->hook_copy = NULL;
		tnd->hook_free = NULL;
		tnd->hook_dumptree = NULL;
		tnd->hook_postwalktree = NULL;
		tnd->hook_prewalktree = NULL;

		tnd->prefreetree = NULL;

		tnd->tn_flags = flags;

		/* add to stringhash / array */
		stringhash_insert (nodetypes, tnd, tnd->name);
		DA_SETNTHITEM (anodetypes, tnd->idx, tnd);
	}

	return tnd;
}
/*}}}*/
/*{{{  ntdef_t *tnode_newnodetag (char *name, int *idx, tndef_t *type, int flags)*/
/*
 *	creates a new node tag
 */
ntdef_t *tnode_newnodetag (char *name, int *idx, tndef_t *type, int flags)
{
	ntdef_t *ntd;

	ntd = tnode_lookupnodetag (name);
	if (!ntd) {
		ntd = (ntdef_t *)smalloc (sizeof (ntdef_t));
		ntd->name = string_dup (name);
		if (*idx > -1) {
			/* want specific placement in the array */
			if (*idx >= DA_CUR (anodetags)) {
				/* not a problem */
				dynarray_setsize (anodetags, *idx + 1);
			} else if (DA_NTHITEM (anodetags, *idx)) {
				/* something already here -- won't evict it */
				int i;

				for (i=0; (i < DA_CUR (anodetags)) && DA_NTHITEM (anodetags, i); i++);
				if (i == DA_CUR (anodetags)) {
					dynarray_setsize (anodetags, i + 1);
				}
				nocc_warning ("tnode_newnodetag(): wanted index %d for node tag [%s], got %d instead.", *idx, name, i);
				*idx = i;
			} /* else free :) */
		} else {
			/* search for free slot */
			int i;

			for (i=0; (i < DA_CUR (anodetags)) && DA_NTHITEM (anodetags, i); i++);
			if (i == DA_CUR (anodetags)) {
				dynarray_setsize (anodetags, i + 1);
			}
			*idx = i;
		}
		ntd->idx = *idx;
		ntd->ndef = type;
		ntd->nt_flags = flags;

		/* add to stringhash / array */
		stringhash_insert (nodetags, ntd, ntd->name);
		DA_SETNTHITEM (anodetags, ntd->idx, ntd);
	}

	return ntd;
}
/*}}}*/
/*{{{  tndef_t *tnode_lookupnodetype (char *name)*/
/*
 *	looks up a node-type (by name)
 */
tndef_t *tnode_lookupnodetype (char *name)
{
	tndef_t *tnd;

	tnd = stringhash_lookup (nodetypes, name);
	return tnd;
}
/*}}}*/
/*{{{  ntdef_t *tnode_lookupnodetag (char *name)*/
/*
 *	looks up a node tag (by name)
 */
ntdef_t *tnode_lookupnodetag (char *name)
{
	ntdef_t *ntd;

	ntd = stringhash_lookup (nodetags, name);
	return ntd;
}
/*}}}*/

/*{{{  void tnode_setnthsub (tnode_t *t, int i, tnode_t *subnode)*/
/*
 *	sets the nth subnode of a treenode
 */
void tnode_setnthsub (tnode_t *t, int i, tnode_t *subnode)
{
	tndef_t *tnd;

	if (!t) {
		nocc_internal ("tnode_setnthsub(): null node!");
		return;
	}
	tnd = t->tag->ndef;
	if (i >= tnd->nsub) {
		nocc_internal ("tnode_setnthsub(): attempt to set subnode %d of %d", i, tnd->nsub);
		return;
	}

	DA_SETNTHITEM (t->items, i, (void *)subnode);
	return;
}
/*}}}*/
/*{{{  void tnode_setnthname (tnode_t *t, int i, struct TAG_name *name)*/
/*
 *	sets the nth name in a tree-node
 */
void tnode_setnthname (tnode_t *t, int i, struct TAG_name *name)
{
	tndef_t *tnd;

	if (!t) {
		nocc_internal ("tnode_setnthname(): null node!");
		return;
	}
	tnd = t->tag->ndef;
	if (i >= tnd->nname) {
		nocc_internal ("tnode_setnthname(): attempt to set name %d of %d", i, tnd->nname);
		return;
	}

	DA_SETNTHITEM (t->items, tnd->nsub + i, (void *)name);
	return;
}
/*}}}*/
/*{{{  void tnode_setnthhook (tnode_t *t, int i, void *hook)*/
/*
 *	sets the nth hook in a tree-node
 */
void tnode_setnthhook (tnode_t *t, int i, void *hook)
{
	tndef_t *tnd;

	if (!t) {
		nocc_internal ("tnode_setnthhook(): null node!");
		return;
	}
	tnd = t->tag->ndef;
	if (i >= tnd->nhooks) {
		nocc_internal ("tnode_setnthhook(): attempt to set hook %d of %d", i, tnd->nhooks);
		return;
	}

	DA_SETNTHITEM (t->items, tnd->nsub + tnd->nname + i, hook);
	return;
}
/*}}}*/
/*{{{  tnode_t *tnode_nthsubof (tnode_t *t, int i)*/
/*
 *	returns the nth subnode of a tree-node
 */
tnode_t *tnode_nthsubof (tnode_t *t, int i)
{
	tndef_t *tnd;
	
	if (!t) {
		nocc_internal ("tnode_nthsubof(): null node!");
		return NULL;
	}
	tnd = t->tag->ndef;
	if (i >= tnd->nsub) {
		nocc_internal ("tnode_nthsubof(): attempt to get subnode %d of %d", i, tnd->nsub);
		return NULL;
	}

	return (tnode_t *)DA_NTHITEM (t->items, i);
}
/*}}}*/
/*{{{  tnode_t **tnode_subnodesof (tnode_t *t, int *nnodes)*/
/*
 *	returns subnodes of a tree-node
 */
tnode_t **tnode_subnodesof (tnode_t *t, int *nnodes)
{
	tndef_t *tnd;

	if (!t) {
		nocc_internal ("tnode_subnodesof(): null node!");
		return NULL;
	}
	tnd = t->tag->ndef;
	*nnodes = tnd->nsub;
	return (tnode_t **)DA_PTR (t->items);
}
/*}}}*/
/*{{{  name_t *tnode_nthnameof (tnode_t *t, int i)*/
/*
 *	returns the nth name of a tree-node
 */
name_t *tnode_nthnameof (tnode_t *t, int i)
{
	tndef_t *tnd;
	
	if (!t) {
		nocc_internal ("tnode_nthnameof(): null node!");
		return NULL;
	}
	tnd = t->tag->ndef;
	if (i >= tnd->nname) {
		nocc_internal ("tnode_nthnameof(): attempt to get name %d of %d", i, tnd->nname);
		return NULL;
	}

	return (name_t *)DA_NTHITEM (t->items, tnd->nsub + i);
}
/*}}}*/
/*{{{  void *tnode_nthhookof (tnode_t *t, int i)*/
/*
 *	returns the nth hook of a tree-node
 */
void *tnode_nthhookof (tnode_t *t, int i)
{
	tndef_t *tnd;
	
	if (!t) {
		nocc_internal ("tnode_nthhookof(): null node!");
		return NULL;
	}
	tnd = t->tag->ndef;
	if (i >= tnd->nhooks) {
		nocc_internal ("tnode_nthhookof(): attempt to get hook %d of %d", i, tnd->nhooks);
		return NULL;
	}

	return (void *)DA_NTHITEM (t->items, tnd->nsub + tnd->nname + i);
}
/*}}}*/
/*{{{  tnode_t **tnode_nthsubaddr (tnode_t *t, int i)*/
/*
 *	returns the nth subnode address of a tree-node
 */
tnode_t **tnode_nthsubaddr (tnode_t *t, int i)
{
	tndef_t *tnd;
	
	if (!t) {
		nocc_internal ("tnode_nthsubaddr(): null node!");
		return NULL;
	}
	tnd = t->tag->ndef;
	if (i >= tnd->nsub) {
		nocc_internal ("tnode_nthsubaddr(): attempt to get subnode %d of %d", i, tnd->nsub);
		return NULL;
	}

	return (tnode_t **)&(DA_NTHITEM (t->items, i));
}
/*}}}*/
/*{{{  name_t **tnode_nthnameaddr (tnode_t *t, int i)*/
/*
 *	returns the nth name address of a tree-node
 */
name_t **tnode_nthnameaddr (tnode_t *t, int i)
{
	tndef_t *tnd;
	
	if (!t) {
		nocc_internal ("tnode_nthnameaddr(): null node!");
		return NULL;
	}
	tnd = t->tag->ndef;
	if (i >= tnd->nname) {
		nocc_internal ("tnode_nthnameaddr(): attempt to get name %d of %d", i, tnd->nname);
		return NULL;
	}

	return (name_t **)&(DA_NTHITEM (t->items, tnd->nsub + i));
}
/*}}}*/
/*{{{  void **tnode_nthhookaddr (tnode_t *t, int i)*/
/*
 *	returns the nth hook address of a tree-node
 */
void **tnode_nthhookaddr (tnode_t *t, int i)
{
	tndef_t *tnd;
	
	if (!t) {
		nocc_internal ("tnode_nthhookaddr(): null node!");
		return NULL;
	}
	tnd = t->tag->ndef;
	if (i >= tnd->nhooks) {
		nocc_internal ("tnode_nthhookaddr(): attempt to get hook %d of %d", i, tnd->nhooks);
		return NULL;
	}

	return (void **)&(DA_NTHITEM (t->items, tnd->nsub + tnd->nname + i));
}
/*}}}*/

/*{{{  int tnode_tnflagsof (tnode_t *t)*/
/*
 *	returns the node-type flags for a particular node
 */
int tnode_tnflagsof (tnode_t *t)
{
	if (!t) {
		nocc_internal ("tnode_tnflagsof(): NULL node!");
		return 0;
	}
	if (!(t->tag)) {
		nocc_internal ("tnode_tnflagsof(): node has NULL tag!");
		return 0;
	}
	if (!(t->tag->ndef)) {
		nocc_internal ("tnode_tnflagsof(): node-tag has NULL type!");
		return 0;
	}
	return t->tag->ndef->tn_flags;
}
/*}}}*/
/*{{{  int tnode_ntflagsof (tnode_t *t)*/
/*
 *	returns the node-tag flags for a particular node
 */
int tnode_ntflagsof (tnode_t *t)
{
	if (!t) {
		nocc_internal ("tnode_ntflagsof(): NULL node!");
		return 0;
	}
	if (!(t->tag)) {
		nocc_internal ("tnode_ntflagsof(): node has NULL tag!");
		return 0;
	}
	return t->tag->nt_flags;
}
/*}}}*/

/*{{{  void tnode_postwalktree (tnode_t *t, void (*fcn)(tnode_t *, void *), void *arg)*/
/*
 *	performs a tree-walk.  calls the requested function on each node after
 *	the subnodes have been walked -- not expected to be used much, but it's here..
 */
void tnode_postwalktree (tnode_t *t, void (*fcn)(tnode_t *, void *), void *arg)
{
	int i;
	tndef_t *tnd;

	if (!t || !fcn) {
		return;
	}

	tnd = t->tag->ndef;
	/* walk subnodes */
	for (i=0; i<tnd->nsub; i++) {
		tnode_t *sub = (tnode_t *)DA_NTHITEM (t->items, i);

		if (sub) {
			tnode_postwalktree (sub, fcn, arg);
		}
	}
	/* walk hooks if applicable */
	if (tnd->hook_prewalktree) {
		for (i=(tnd->nsub + tnd->nname); i<DA_CUR (t->items); i++) {
			void *hook = DA_NTHITEM (t->items, i);

			tnd->hook_postwalktree (t, hook, fcn, arg);
		}
	}
	fcn (t, arg);
	return;
}
/*}}}*/
/*{{{  void tnode_prewalktree (tnode_t *t, int (*fcn)(tnode_t *, void *), void *arg)*/
/*
 *	performs a tree-walk.  calls the requested function on each node before
 *	the node is walked.  If the function returns zero, no subtrees are walked.
 */
void tnode_prewalktree (tnode_t *t, int (*fcn)(tnode_t *, void *), void *arg)
{
	int i;

	if (!t || !fcn) {
		return;
	}
	i = fcn (t, arg);
	if (i) {
		tndef_t *tnd = t->tag->ndef;

		/* walk subnodes */
		for (i=0; i<tnd->nsub; i++) {
			tnode_t *sub = (tnode_t *)DA_NTHITEM (t->items, i);

			if (sub) {
				tnode_prewalktree (sub, fcn, arg);
			}
		}
		/* walk hooks if applicable */
		if (tnd->hook_prewalktree) {
			for (i=(tnd->nsub + tnd->nname); i<DA_CUR (t->items); i++) {
				void *hook = DA_NTHITEM (t->items, i);

				tnd->hook_prewalktree (t, hook, fcn, arg);
			}
		}
	}
	return;
}
/*}}}*/
/*{{{  void tnode_modprewalktree (tnode_t **t, int (*fcn)(tnode_t **, void *), void *arg)*/
/*
 *	performs a tree-walk.  calls the requested function on each node before
 *	the node is walked.  If the function returns zero, no subtrees are walked.
 */
void tnode_modprewalktree (tnode_t **t, int (*fcn)(tnode_t **, void *), void *arg)
{
	int i;

	if (!t || !*t || !fcn) {
		return;
	}
	i = fcn (t, arg);
	if (i) {
		tndef_t *tnd = (*t)->tag->ndef;

		/* walk subnodes */
		for (i=0; i<tnd->nsub; i++) {
			tnode_t **sub = (tnode_t **)&(DA_NTHITEM ((*t)->items, i));

			if (*sub) {
				tnode_modprewalktree (sub, fcn, arg);
			}
		}
		/* walk hooks if applicable */
		if (tnd->hook_modprewalktree) {
			for (i=(tnd->nsub + tnd->nname); i<DA_CUR ((*t)->items); i++) {
				void *hook = DA_NTHITEM ((*t)->items, i);

				tnd->hook_modprewalktree (t, hook, fcn, arg);
			}
		}
	}
	return;
}
/*}}}*/
/*{{{  void tnode_modprepostwalktree (tnode_t **t, int (*prefcn)(tnode_t **, void *), int (*postfcn)(tnode_t **, void *), void *arg)*/
/*
 *	performs a tree-walk.  calls the requested "prefcn" function on each node before
 *	the node is walked.  If the function returns zero, no subtrees are walked.
 *	calls "postfcn" after subnode walk.
 */
void tnode_modprepostwalktree (tnode_t **t, int (*prefcn)(tnode_t **, void *), int (*postfcn)(tnode_t **, void *), void *arg)
{
	int i;

	if (!t || !*t || !prefcn || !postfcn) {
		return;
	}
	i = prefcn (t, arg);
	if (i) {
		tndef_t *tnd = (*t)->tag->ndef;

		/* walk subnodes */
		for (i=0; i<tnd->nsub; i++) {
			tnode_t **sub = (tnode_t **)&(DA_NTHITEM ((*t)->items, i));

			if (*sub) {
				tnode_modprepostwalktree (sub, prefcn, postfcn, arg);
			}
		}
		/* walk hooks if applicable */
		if (tnd->hook_modprewalktree) {
			for (i=(tnd->nsub + tnd->nname); i<DA_CUR ((*t)->items); i++) {
				void *hook = DA_NTHITEM ((*t)->items, i);

				tnd->hook_modprepostwalktree (t, hook, prefcn, postfcn, arg);
			}
		}
	}
	postfcn (t, arg);
	return;
}
/*}}}*/


/*{{{  tnode_t *tnode_new (ntdef_t *tag, lexfile_t *lf)*/
/*
 *	allocates a new tree-node
 */
tnode_t *tnode_new (ntdef_t *tag, lexfile_t *lf)
{
	tnode_t *tmp;

	tmp = (tnode_t *)smalloc (sizeof (tnode_t));
	memset (tmp, 0, sizeof (tnode_t));
	tmp->tag = tag;
	tmp->org_file = lf;
	if (lf) {
		tmp->org_line = lf->lineno;
	} else {
		tmp->org_line = 0;
	}

	dynarray_init (tmp->items);
	dynarray_setsize (tmp->items, tag->ndef->nsub + tag->ndef->nname + tag->ndef->nhooks);

	return tmp;
}
/*}}}*/
/*{{{  tnode_t *tnode_newfrom (ntdef_t *tag, tnode_t *src)*/
/*
 *	allocates a new tree-node
 */
tnode_t *tnode_newfrom (ntdef_t *tag, tnode_t *src)
{
	tnode_t *tmp;

	tmp = (tnode_t *)smalloc (sizeof (tnode_t));
	memset (tmp, 0, sizeof (tnode_t));
	tmp->tag = tag;
	tmp->org_file = src->org_file;
	tmp->org_line = src->org_line;

	dynarray_init (tmp->items);
	dynarray_setsize (tmp->items, tag->ndef->nsub + tag->ndef->nname + tag->ndef->nhooks);
	dynarray_init (tmp->chooks);
	if (DA_CUR (acomphooks)) {
		dynarray_setsize (tmp->chooks, DA_CUR (acomphooks));
	}

	return tmp;
}
/*}}}*/
/*{{{  tnode_t *tnode_create (ntdef_t *tag, lexfile_t *lf, ...)*/
/*
 *	creates a new tree-node, with arguments supplied
 */
tnode_t *tnode_create (ntdef_t *tag, lexfile_t *lf, ...)
{
	va_list ap;
	int i;
	tnode_t *tmp;

	tmp = (tnode_t *)smalloc (sizeof (tnode_t));
	memset (tmp, 0, sizeof (tnode_t));
	tmp->tag = tag;
	tmp->org_file = lf;
	if (lf) {
		tmp->org_line = lf->lineno;
	} else {
		tmp->org_line = 0;
	}

	dynarray_init (tmp->items);
	dynarray_setsize (tmp->items, tag->ndef->nsub + tag->ndef->nname + tag->ndef->nhooks);
	dynarray_init (tmp->chooks);
	if (DA_CUR (acomphooks)) {
		dynarray_setsize (tmp->chooks, DA_CUR (acomphooks));
	}

	va_start (ap, lf);
	/* should have everything supplied here.. */
	for (i=0; i<DA_CUR (tmp->items); i++) {
		if (i < tag->ndef->nsub) {
			tnode_t *arg = va_arg (ap, tnode_t *);

			DA_SETNTHITEM (tmp->items, i, (void *)arg);
		} else if (i < (tag->ndef->nsub + tag->ndef->nname)) {
			name_t *arg = va_arg (ap, name_t *);

			DA_SETNTHITEM (tmp->items, i, (void *)arg);
		} else {
			void *arg = va_arg (ap, void *);

			DA_SETNTHITEM (tmp->items, i, arg);
		}
	}
	va_end (ap);
	
	return tmp;
}
/*}}}*/
/*{{{  tnode_t *tnode_createfrom (ntdef_t *tag, tnode_t *src, ...)*/
/*
 *	creates a new tree-node, with arguments supplied
 */
tnode_t *tnode_createfrom (ntdef_t *tag, tnode_t *src, ...)
{
	va_list ap;
	int i;
	tnode_t *tmp;

	tmp = (tnode_t *)smalloc (sizeof (tnode_t));
	memset (tmp, 0, sizeof (tnode_t));
	tmp->tag = tag;
	tmp->org_file = src->org_file;
	tmp->org_line = src->org_line;

	dynarray_init (tmp->items);
	dynarray_setsize (tmp->items, tag->ndef->nsub + tag->ndef->nname + tag->ndef->nhooks);
	dynarray_init (tmp->chooks);
	if (DA_CUR (acomphooks)) {
		dynarray_setsize (tmp->chooks, DA_CUR (acomphooks));
	}

	va_start (ap, src);
	/* should have everything supplied here.. */
	for (i=0; i<DA_CUR (tmp->items); i++) {
		if (i < tag->ndef->nsub) {
			tnode_t *arg = va_arg (ap, tnode_t *);

			DA_SETNTHITEM (tmp->items, i, (void *)arg);
		} else if (i < (tag->ndef->nsub + tag->ndef->nname)) {
			name_t *arg = va_arg (ap, name_t *);

			DA_SETNTHITEM (tmp->items, i, (void *)arg);
		} else {
			void *arg = va_arg (ap, void *);

			DA_SETNTHITEM (tmp->items, i, arg);
		}
	}
	va_end (ap);
	
	return tmp;
}
/*}}}*/
/*{{{  void tnode_free (tnode_t *t)*/
/*
 *	frees a tree-node (recursively)
 */
void tnode_free (tnode_t *t)
{
	int i;
	tndef_t *tnd;

	if (!t) {
		return;
	}
	tnd = t->tag->ndef;
	if (tnd->prefreetree) {
		tnd->prefreetree (t);
	}
	for (i=0; i<DA_CUR (t->items); i++) {
		if (i < tnd->nsub) {
			/* sub-nodes to free */
			tnode_t *subnode = (tnode_t *)DA_NTHITEM (t->items, i);

			if (subnode) {
				tnode_free (subnode);
			}
		} else if (i >= (tnd->nsub + tnd->nname)) {
			/* hook-node to free */
			void *hook = DA_NTHITEM (t->items, i);

			if (hook && tnd->hook_free) {
				tnd->hook_free (hook);
			} else if (hook) {
				nocc_warning ("tnode_free(): freeing hook [%s:%s] 0x%8.8x with sfree().", tnd->name, t->tag->name, (unsigned int)hook);
				sfree (hook);
			}
		}
		/* types and names don't take up any space here.. */
	}
	for (i=0; i<DA_CUR (t->chooks); i++) {
		chook_t *ch = DA_NTHITEM (acomphooks, i);
		void *chc = DA_NTHITEM (t->chooks, i);

		if (ch && chc && ch->chook_free) {
			ch->chook_free (chc);
		} else if (ch && chc) {
			nocc_warning ("tnode_free(): freeing compiler-hook (%s) [%s:%s] 0x%8.8x with sfree().", ch->name, tnd->name, t->tag->name, (unsigned int)chc);
			sfree (chc);
		}
	}
	/* FIXME: links in name-nodes back to declarations */

	dynarray_trash (t->items);
	dynarray_trash (t->chooks);
	sfree (t);

	return;
}
/*}}}*/
/*{{{  tnode_t *tnode_copytree (tnode_t *t)*/
/*
 *	copies a tree
 */
tnode_t *tnode_copytree (tnode_t *t)
{
	tnode_t *tmp = NULL;
	tndef_t *tnd;
	int i;

	if (!t) {
		return NULL;
	}
	tmp = tnode_new (t->tag, NULL);
	tmp->org_file = t->org_file;
	tmp->org_line = t->org_line;

	tnd = t->tag->ndef;
	for (i=0; i<DA_CUR (t->items); i++) {
		if (i < tnd->nsub) {
			tnode_t *subcopy = tnode_copytree (DA_NTHITEM (t->items, i));

			DA_SETNTHITEM (tmp->items, i, subcopy);
		} else if (i >= (tnd->nsub + tnd->nname)) {
			/* hook-node, duplicate... */
			void *hook = DA_NTHITEM (t->items, i);

			if (tnd->hook_copy) {
				hook = tnd->hook_copy (hook);
			}
			DA_SETNTHITEM (tmp->items, i, hook);
		} else {
			DA_SETNTHITEM (tmp->items, i, DA_NTHITEM (t->items, i));
		}
	}

	/* don't forget to do compiler hooks */
#if 0
fprintf (stderr, "tnode_copytree(): copying [%s], num chooks = %d\n", t->tag->name, DA_CUR (t->chooks));
#endif
	if (DA_CUR (tmp->chooks) < DA_CUR (t->chooks)) {
		dynarray_setsize (tmp->chooks, DA_CUR (acomphooks));
	}
	for (i=0; i<DA_CUR (t->chooks); i++) {
		chook_t *ch = DA_NTHITEM (acomphooks, i);
		void *chc = DA_NTHITEM (t->chooks, i);

		if (ch && chc && ch->chook_copy) {
			chc = ch->chook_copy (chc);
		}
		DA_SETNTHITEM (tmp->chooks, i, chc);
	}
	/* populate remaining hooks */
	for (; i<DA_CUR (acomphooks); i++) {
		dynarray_add (tmp->chooks, NULL);
	}

	return tmp;
}
/*}}}*/
/*{{{  static void tnode_isetindent (FILE *stream, int indent)*/
/*
 *	sets indentation level (debug output)
 */
static void tnode_isetindent (FILE *stream, int indent)
{
	int i;

	if (indent) {
		for (i=0; i<indent; i++) {
			fprintf (stream, "    ");
		}
	}

	return;
}
/*}}}*/
/*{{{  void tnode_dumptree (tnode_t *t, int indent, FILE *stream)*/
/*
 *	dumps a parse tree
 */
void tnode_dumptree (tnode_t *t, int indent, FILE *stream)
{
	int i;
	tndef_t *tnd;

	tnode_isetindent (stream, indent);
	if (!t) {
		fprintf (stream, "<nullnode />\n");
		return;
	}
	tnd = t->tag->ndef;

	fprintf (stream, "<%s type=\"%s\" origin=\"%s:%d\" addr=\"0x%8.8x\">\n", tnd->name, t->tag->name, (t->org_file) ? t->org_file->fnptr : "(none)", t->org_line, (unsigned int)t);
	for (i=0; i<DA_CUR (t->items); i++) {
		if (i < tnd->nsub) {
			/* subnode */
			tnode_dumptree ((tnode_t *)DA_NTHITEM (t->items, i), indent + 1, stream);
		} else if (i < (tnd->nsub + tnd->nname)) {
			/* name */
			name_dumpname ((name_t *)DA_NTHITEM (t->items, i), indent + 1, stream);
		} else {
			/* hook */
			if (tnd->hook_dumptree) {
				tnd->hook_dumptree (t, DA_NTHITEM (t->items, i), indent + 1, stream);
			} else {
				tnode_isetindent (stream, indent + 1);
				fprintf (stream, "<hook addr=\"0x%8.8x\" />\n", (unsigned int)(DA_NTHITEM (t->items, i)));
			}
		}
	}
	for (i=0; i<DA_CUR (t->chooks); i++) {
		/* compiler hooks */
		chook_t *ch = DA_NTHITEM (acomphooks, i);
		void *chc = DA_NTHITEM (t->chooks, i);

		if (ch && chc && ch->chook_dumptree) {
			ch->chook_dumptree (t, chc, indent + 1, stream);
		} else if (ch && chc) {
			tnode_isetindent (stream, indent + 1);
			fprintf (stream, "<chook id=\"%s\" addr=\"0x%8.8x\" />\n", ch->name, (unsigned int)chc);
		}
	}
	tnode_isetindent (stream, indent);
	fprintf (stream, "</%s>\n", tnd->name);

	return;
}
/*}}}*/
/*{{{  void tnode_dumpnodetypes (FILE *stream)*/
/*
 *	dumps the various node-types and tags loaded
 */
void tnode_dumpnodetypes (FILE *stream)
{
	int i;

	for (i=0; i<DA_CUR (anodetypes); i++) {
		tndef_t *tnd = DA_NTHITEM (anodetypes, i);

		fprintf (stream, "node type [%s].  %d subnode(s), %d name(s), %d hook(s)\n", tnd->name, tnd->nsub, tnd->nname, tnd->nhooks);
		fprintf (stream, "     hook_copy=%p, hook_free=%p, hook_dumptree=%p, prefreetree=%p.\n", tnd->hook_copy, tnd->hook_free, tnd->hook_dumptree, tnd->prefreetree);
	}
	for (i=0; i<DA_CUR (anodetags); i++) {
		ntdef_t *ntd = DA_NTHITEM (anodetags, i);

		fprintf (stream, "tag name [%s] type [%s]\n", ntd->name, ntd->ndef->name);
	}

	return;
}
/*}}}*/


/*{{{  compops_t *tnode_newcompops (void)*/
/*
 *	creates a new compops_t structure
 */
compops_t *tnode_newcompops (void)
{
	compops_t *cops = (compops_t *)smalloc (sizeof (compops_t));

	cops->prescope = NULL;
	cops->scopein = NULL;
	cops->scopeout = NULL;
	cops->typecheck = NULL;
	cops->typeactual = NULL;
	cops->gettype = NULL;
	cops->bytesfor = NULL;
	cops->fetrans = NULL;
	cops->betrans = NULL;
	cops->premap = NULL;
	cops->namemap = NULL;
	cops->preallocate = NULL;
	cops->precode = NULL;
	cops->codegen = NULL;

	return cops;
}
/*}}}*/
/*{{{  void tnode_freecompops (compops_t *cops)*/
/*
 *	frees a compops_t structure
 */
void tnode_freecompops (compops_t *cops)
{
	if (!cops) {
		nocc_internal ("tnode_freecompops(): called with NULL argument!");
		return;
	}
	sfree (cops);
	return;
}
/*}}}*/


/*{{{  langops_t *tnode_newlangops (void)*/
/*
 *	creates a new langops_t structure
 */
langops_t *tnode_newlangops (void)
{
	langops_t *lops = (langops_t *)smalloc (sizeof (langops_t));

	lops->getdescriptor = NULL;

	return lops;
}
/*}}}*/
/*{{{  void tnode_freelangops (langops_t *lops)*/
/*
 *	frees a langops_t structure
 */
void tnode_freelangops (langops_t *lops)
{
	if (!lops) {
		nocc_internal ("tnode_freelangops(): called with NULL argument!");
		return;
	}
	sfree (lops);
	return;
}
/*}}}*/


/*{{{  chook_t *tnode_newchook (const char *name)*/
/*
 *	allocates a new compiler-hook
 */
chook_t *tnode_newchook (const char *name)
{
	chook_t *ch;

	ch = stringhash_lookup (comphooks, name);
	if (ch) {
		nocc_internal ("tnode_newchook(): compiler hook for [%s] already allocated!", name);
		return ch;
	}
	ch = (chook_t *)smalloc (sizeof (chook_t));
	ch->name = string_dup (name);
	ch->id = DA_CUR (acomphooks);
	ch->chook_copy = NULL;
	ch->chook_free = NULL;
	ch->chook_dumptree = NULL;

	stringhash_insert (comphooks, ch, ch->name);
	dynarray_add (acomphooks, ch);

	return ch;
}
/*}}}*/
/*{{{  chook_t *tnode_lookupchookbyname (const char *name)*/
/*
 *	looks up a compiler hook by name
 */
chook_t *tnode_lookupchookbyname (const char *name)
{
	chook_t *ch;

	ch = stringhash_lookup (comphooks, name);
	return ch;
}
/*}}}*/
/*{{{  void *tnode_getchook (tnode_t *t, chook_t *ch)*/
/*
 *	returns a compiler-hook node for some tree-node
 */
void *tnode_getchook (tnode_t *t, chook_t *ch)
{
	if (!ch || !t) {
		nocc_internal ("tnode_getchook(): null chook or tree!");
		return NULL;
	}
	if (ch->id >= DA_CUR (t->chooks)) {
		/* no hook yet */
		return NULL;
	}

	return DA_NTHITEM (t->chooks, ch->id);
}
/*}}}*/
/*{{{  void tnode_setchook (tnode_t *t, chook_t *ch, void *hook)*/
/*
 *	sets a compiler-hook node for some tree-node
 */
void tnode_setchook (tnode_t *t, chook_t *ch, void *hook)
{
	int i;

	if (!ch || !t) {
		nocc_internal ("tnode_setchook(): null chook or tree!");
	}
	for (i=DA_CUR (t->chooks); i <= ch->id; i++) {
		dynarray_add (t->chooks, NULL);
	}

	if (DA_NTHITEM (t->chooks, ch->id)) {
		if (ch->chook_free) {
			ch->chook_free (DA_NTHITEM (t->chooks, ch->id));
		}
	}
	DA_SETNTHITEM (t->chooks, ch->id, hook);

	return;
}
/*}}}*/


/*{{{  int tnode_bytesfor (tnode_t *t)*/
/*
 *	returns the number of "bytes-for" a tree-node
 */
int tnode_bytesfor (tnode_t *t)
{
	if (t && t->tag->ndef->ops && t->tag->ndef->ops->bytesfor) {
		return t->tag->ndef->ops->bytesfor (t);
	}
	return -1;		/* don't know */
}
/*}}}*/


