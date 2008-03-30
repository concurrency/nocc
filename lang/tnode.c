/*
 *	tnode.c -- parser node functions
 *	Copyright (C) 2004-2007 Fred Barnes <frmb@kent.ac.uk>
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


/*{{{  includes*/
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
#include "origin.h"
#include "symbols.h"
#include "keywords.h"
#include "lexer.h"
#include "tnode.h"
#include "lexpriv.h"
#include "names.h"
#include "target.h"
/*}}}*/

/*{{{  private stuff*/
STATICSTRINGHASH (tndef_t *, nodetypes, 5);
STATICDYNARRAY (tndef_t *, anodetypes);

STATICSTRINGHASH (ntdef_t *, nodetags, 6);
STATICDYNARRAY (ntdef_t *, anodetags);

STATICSTRINGHASH (chook_t *, comphooks, 4);
STATICDYNARRAY (chook_t *, acomphooks);

STATICSTRINGHASH (compop_t *, compops, 4);
STATICDYNARRAY (compop_t *, acompops);
STATICSTRINGHASH (langop_t *, langops, 4);
STATICDYNARRAY (langop_t *, alangops);

STATICSTRINGHASH (char *, tracingcompops, 3);
STATICSTRINGHASH (char *, tracinglangops, 3);

/* forwards */
static void tnode_isetindent (FILE *stream, int indent);
static void tnode_ssetindent (FILE *stream, int indent);

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
/*{{{  static void tnode_const_hookdumpstree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	hook-dumptree function for constant nodes (s-record format)
 */
static void tnode_const_hookdumpstree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	tnode_ssetindent (stream, indent);
	fprintf (stream, "(hook (ptr 0x%8.8x))\n", (unsigned int)hook);

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
/*{{{  static void *tnode_list_hookcopyoralias (void *hook, copycontrol_e (*cora_fcn)(tnode_t *))*/
/*
 *	hook-copy function for list nodes
 */
static void *tnode_list_hookcopyoralias (void *hook, copycontrol_e (*cora_fcn)(tnode_t *))
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
		narray[i] = tnode_copyoraliastree (array[i], cora_fcn);
	}
	for (; i<*namax; i++) {
		narray[i] = NULL;
	}

	return rhook;
}
/*}}}*/
/*{{{  static void *tnode_list_hookcopy (void *hook)*/
/*
 *	hook-copy function for list nodes
 */
static void *tnode_list_hookcopy (void *hook)
{
	return tnode_list_hookcopyoralias (hook, NULL);
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
/*{{{  static void tnode_list_hookdumpstree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	hook-dumptree function for a list (s-record format)
 */
static void tnode_list_hookdumpstree (tnode_t *node, void *hook, int indent, FILE *stream)
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
		tnode_dumpstree (array[i], indent, stream);
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


/*{{{  int tnode_init (void)*/
/*
 *	initialises node handler
 *	returns 0 on success, non-zero on failure
 */
int tnode_init (void)
{
	tndef_t *tnd;
	ntdef_t *ntd;
	int i;

	stringhash_sinit (nodetypes);
	dynarray_init (anodetypes);
	stringhash_sinit (nodetags);
	dynarray_init (anodetags);

	stringhash_sinit (comphooks);
	dynarray_init (acomphooks);

	stringhash_sinit (compops);
	dynarray_init (acompops);
	stringhash_sinit (langops);
	dynarray_init (alangops);

	stringhash_sinit (tracingcompops);
	stringhash_sinit (tracinglangops);

	if (compopts.tracecompops) {
		char *copy = string_dup (compopts.tracecompops);
		char **list;
		
		list = split_string2 (copy, ',', ' ');
		for (i=0; list[i]; i++) {
			stringhash_insert (tracingcompops, list[i], list[i]);
			list[i] = NULL;
		}
		sfree (list);
		sfree (copy);
	}
	if (compopts.tracelangops) {
		char *copy = string_dup (compopts.tracelangops);
		char **list;

		list = split_string2 (copy, ',', ' ');
		for (i=0; list[i]; i++) {
			stringhash_insert (tracinglangops, list[i], list[i]);
			list[i] = NULL;
		}
		sfree (list);
		sfree (copy);
	}

	/*{{{  default compiler operations*/
	tnode_newcompop ("prescope", COPS_PRESCOPE, 2, INTERNAL_ORIGIN);
	tnode_newcompop ("scopein", COPS_SCOPEIN, 2, INTERNAL_ORIGIN);
	tnode_newcompop ("scopeout", COPS_SCOPEOUT, 2, INTERNAL_ORIGIN);
	tnode_newcompop ("typecheck", COPS_TYPECHECK, 2, INTERNAL_ORIGIN);
	tnode_newcompop ("constprop", COPS_CONSTPROP, 1, INTERNAL_ORIGIN);
	tnode_newcompop ("typeresolve", COPS_TYPERESOLVE, 2, INTERNAL_ORIGIN);
	tnode_newcompop ("precheck", COPS_PRECHECK, 1, INTERNAL_ORIGIN);
	tnode_newcompop ("tracescheck", COPS_TRACESCHECK, 2, INTERNAL_ORIGIN);
	tnode_newcompop ("mobilitycheck", COPS_MOBILITYCHECK, 2, INTERNAL_ORIGIN);
	tnode_newcompop ("postcheck", COPS_POSTCHECK, 2, INTERNAL_ORIGIN);
	tnode_newcompop ("fetrans", COPS_FETRANS, 2, INTERNAL_ORIGIN);
	tnode_newcompop ("betrans", COPS_BETRANS, 2, INTERNAL_ORIGIN);
	tnode_newcompop ("premap", COPS_PREMAP, 2, INTERNAL_ORIGIN);
	tnode_newcompop ("namemap", COPS_NAMEMAP, 2, INTERNAL_ORIGIN);
	tnode_newcompop ("bemap", COPS_BEMAP, 2, INTERNAL_ORIGIN);
	tnode_newcompop ("preallocate", COPS_PREALLOCATE, 2, INTERNAL_ORIGIN);
	tnode_newcompop ("precode", COPS_PRECODE, 2, INTERNAL_ORIGIN);
	tnode_newcompop ("codegen", COPS_CODEGEN, 2, INTERNAL_ORIGIN);

	/*}}}*/
	/*{{{  default language operations*/
	tnode_newlangop ("getdescriptor", LOPS_GETDESCRIPTOR, 2, INTERNAL_ORIGIN);
	tnode_newlangop ("getname", LOPS_GETNAME, 2, INTERNAL_ORIGIN);
	tnode_newlangop ("do_usagecheck", LOPS_DO_USAGECHECK, 2, INTERNAL_ORIGIN);
	tnode_newlangop ("typeactual", LOPS_TYPEACTUAL, 4, INTERNAL_ORIGIN);
	tnode_newlangop ("typereduce", LOPS_TYPEREDUCE, 1, INTERNAL_ORIGIN);
	tnode_newlangop ("cantypecast", LOPS_CANTYPECAST, 2, INTERNAL_ORIGIN);
	tnode_newlangop ("gettype", LOPS_GETTYPE, 2, INTERNAL_ORIGIN);
	tnode_newlangop ("getsubtype", LOPS_GETSUBTYPE, 2, INTERNAL_ORIGIN);
	tnode_newlangop ("bytesfor", LOPS_BYTESFOR, 2, INTERNAL_ORIGIN);
	tnode_newlangop ("issigned", LOPS_ISSIGNED, 2, INTERNAL_ORIGIN);
	tnode_newlangop ("isconst", LOPS_ISCONST, 1, INTERNAL_ORIGIN);
	tnode_newlangop ("isvar", LOPS_ISVAR, 1, INTERNAL_ORIGIN);
	tnode_newlangop ("istype", LOPS_ISTYPE, 1, INTERNAL_ORIGIN);
	tnode_newlangop ("iscomplex", LOPS_ISCOMPLEX, 2, INTERNAL_ORIGIN);
	tnode_newlangop ("constvalof", LOPS_CONSTVALOF, 2, INTERNAL_ORIGIN);
	tnode_newlangop ("valbyref", LOPS_VALBYREF, 1, INTERNAL_ORIGIN);
	tnode_newlangop ("initsizes", LOPS_INITSIZES, 7, INTERNAL_ORIGIN);
	tnode_newlangop ("initialising_decl", LOPS_INITIALISING_DECL, 3, INTERNAL_ORIGIN);
	tnode_newlangop ("codegen_typeaction", LOPS_CODEGEN_TYPEACTION, 3, INTERNAL_ORIGIN);
	tnode_newlangop ("codegen_typerangecheck", LOPS_CODEGEN_TYPERANGECHECK, 2, INTERNAL_ORIGIN);
	tnode_newlangop ("codegen_altpreenable", LOPS_CODEGEN_ALTPREENABLE, 3, INTERNAL_ORIGIN);
	tnode_newlangop ("codegen_altenable", LOPS_CODEGEN_ALTENABLE, 3, INTERNAL_ORIGIN);
	tnode_newlangop ("codegen_altdisable", LOPS_CODEGEN_ALTDISABLE, 4, INTERNAL_ORIGIN);
	tnode_newlangop ("premap_typeforvardecl", LOPS_PREMAP_TYPEFORVARDECL, 3, INTERNAL_ORIGIN);
	tnode_newlangop ("retypeconst", LOPS_RETYPECONST, 2, INTERNAL_ORIGIN);
	tnode_newlangop ("dimtreeof", LOPS_DIMTREEOF, 1, INTERNAL_ORIGIN);
	tnode_newlangop ("hiddenparamsof", LOPS_HIDDENPARAMSOF, 1, INTERNAL_ORIGIN);
	tnode_newlangop ("hiddenslotsof", LOPS_HIDDENSLOTSOF, 1, INTERNAL_ORIGIN);
	tnode_newlangop ("typehash", LOPS_TYPEHASH, 3, INTERNAL_ORIGIN);
	tnode_newlangop ("typetype", LOPS_TYPETYPE, 1, INTERNAL_ORIGIN);
	tnode_newlangop ("getbasename", LOPS_GETBASENAME, 1, INTERNAL_ORIGIN);

	/*}}}*/
	/*{{{  setup the static node types*/
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
	tnd->hook_dumpstree = tnode_const_hookdumpstree;
	i = -1;
	ntd = tnode_newnodetag ("intlit", &i, tnd, 0);

	i = -1;
	tnd = tnode_newnodetype ("listnode", &i, 0, 0, 1, 0);
	tnd->hook_free = tnode_list_hookfree;
	tnd->hook_copy = tnode_list_hookcopy;
	tnd->hook_copyoralias = tnode_list_hookcopyoralias;
	tnd->hook_dumptree = tnode_list_hookdumptree;
	tnd->hook_dumpstree = tnode_list_hookdumpstree;
	tnd->hook_postwalktree = tnode_list_hookpostwalktree;
	tnd->hook_prewalktree = tnode_list_hookprewalktree;
	tnd->hook_modprewalktree = tnode_list_hookmodprewalktree;
	tnd->hook_modprepostwalktree = tnode_list_hookmodprepostwalktree;
	i = -1;
	ntd = tnode_newnodetag ("list", &i, tnd, 0);

	/*}}}*/

	return 0;
}
/*}}}*/
/*{{{  int tnode_shutdown (void)*/
/*
 *	shuts-down node handler
 *	returns 0 on success, non-zero on failure
 */
int tnode_shutdown (void)
{
	return 0;
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
		tnd->hook_copyoralias = NULL;
		tnd->hook_free = NULL;
		tnd->hook_dumptree = NULL;
		tnd->hook_dumpstree = NULL;
		tnd->hook_postwalktree = NULL;
		tnd->hook_prewalktree = NULL;

		tnd->prefreetree = NULL;

		tnd->ops = NULL;
		tnd->lops = NULL;
		tnd->tchkdef = NULL;

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

	if (!type) {
		nocc_serious ("tnode_newnodetag(): NULL node type! (creating [%s])", name);
	}
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
/*{{{  tndef_t *tnode_lookupornewnodetype (char *name, int *idx, int nsub, int nname, int nhooks, int flags)*/
/*
 *	creates a new node-type only if it doesn't exist
 */
tndef_t *tnode_lookupornewnodetype (char *name, int *idx, int nsub, int nname, int nhooks, int flags)
{
	tndef_t *tnd = tnode_lookupnodetype (name);

	if (!tnd) {
		return tnode_newnodetype (name, idx, nsub, nname, nhooks, flags);
	} else {
		if ((tnd->nsub != nsub) || (tnd->nname != nname) || (tnd->nhooks != nhooks)) {
			nocc_serious ("tnode_lookupornewnodetype(): already registered node type [%s] has a different structure (%d,%d,%d), specified (%d,%d,%d)", name, tnd->nsub, tnd->nname, tnd->nhooks, nsub, nname, nhooks);
		} else if (tnd->tn_flags != flags) {
			nocc_serious ("tnode_lookupornewnodetype(): already registered node type [%s] has different flags (0x%8.8x), specified (0x%8.8x)", name, (unsigned int)tnd->tn_flags, (unsigned int)flags);
		}
	}
	return tnd;
}
/*}}}*/
/*{{{  ntdef_t *tnode_lookupornewnodetag (char *name, int *idx, tndef_t *type, int flags)*/
/*
 *	creates a new node-tag only if it doesn't exist
 */
ntdef_t *tnode_lookupornewnodetag (char *name, int *idx, tndef_t *type, int flags)
{
	ntdef_t *ntd = tnode_lookupnodetag (name);

	if (!ntd) {
		return tnode_newnodetag (name, idx, type, flags);
	} else {
		if (ntd->ndef != type) {
			nocc_serious ("tnode_lookupornewnodetag(): already registered node tag [%s] has different type (%s), specified (%s)", name,
					ntd->ndef ? ntd->ndef->name : "(unknown)", type ? type->name : "(unknown)");
		}
	}
	return ntd;
}
/*}}}*/

/*{{{  void tnode_changetag (tnode_t *t, ntdef_t *newtag)*/
/*
 *	changes the tag of a node -- makes sure that they are the same type-of-node
 */
void tnode_changetag (tnode_t *t, ntdef_t *newtag)
{
	if (t->tag->ndef != newtag->ndef) {
		nocc_internal ("tnode_changetag(): refusing to change [%s,%s] to [%s,%s]", t->tag->name, t->tag->ndef->name, newtag->name, newtag->ndef->name);
	}
	t->tag = newtag;
	return;
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
		nocc_internal ("tnode_setnthsub(): attempt to set subnode %d of %d in [%s,%s]", i, tnd->nsub, t->tag->name, tnd->name);
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
/*{{{  void tnode_modpostwalktree (tnode_t **t, int (*fcn)(tnode_t **, void *), void *arg)*/
/*
 *	performs a tree-walk, calls the requested function on each node after the node is walked.
 */
void tnode_modpostwalktree (tnode_t **t, int (*fcn)(tnode_t **, void *), void *arg)
{
	int i;
	tndef_t *tnd;

	if (!t || !*t || !fcn) {
		return;
	}
	tnd = (*t)->tag->ndef;

	/* walk subnodes */
	for (i=0; i<tnd->nsub; i++) {
		tnode_t **sub = (tnode_t **)DA_NTHITEMADDR ((*t)->items, i);

		if (*sub) {
			tnode_modpostwalktree (sub, fcn, arg);
		}
	}

	/* walk hooks if applicable */
	if (tnd->hook_modprepostwalktree) {
		for (i=(tnd->nsub + tnd->nname); i<DA_CUR ((*t)->items); i++) {
			void *hook = DA_NTHITEM ((*t)->items, i);

			tnd->hook_modprepostwalktree (t, hook, NULL, fcn, arg);
		}
	}
	fcn (t, arg);
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

	if (!t || !*t || !postfcn) {
		return;
	}
	i = prefcn ? prefcn (t, arg) : 1;
	if (i > 0) {
		tndef_t *tnd = (*t)->tag->ndef;

		/* walk subnodes */
		for (i=0; i<tnd->nsub; i++) {
			tnode_t **sub = (tnode_t **)&(DA_NTHITEM ((*t)->items, i));

			if (*sub) {
				tnode_modprepostwalktree (sub, prefcn, postfcn, arg);
			}
		}
		/* walk hooks if applicable */
		if (tnd->hook_modprepostwalktree) {
			for (i=(tnd->nsub + tnd->nname); i<DA_CUR ((*t)->items); i++) {
				void *hook = DA_NTHITEM ((*t)->items, i);

				tnd->hook_modprepostwalktree (t, hook, prefcn, postfcn, arg);
			}
		}
	}
	if (i >= 0) {
		postfcn (t, arg);
	}
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
/*{{{  tnode_t *tnode_copyoraliastree (tnode_t *t, copycontrol_e (*cora_fcn)(tnode_t *))*/
/*
 *	copies a tree (or aliases it, depending on the result of a function)
 */
tnode_t *tnode_copyoraliastree (tnode_t *t, copycontrol_e (*cora_fcn)(tnode_t *))
{
	tnode_t *tmp = NULL;
	tndef_t *tnd;
	int i;
	copycontrol_e cora;

	if (!t) {
		return NULL;
	}
	cora = cora_fcn ? cora_fcn (t) : (COPY_SUBS | COPY_HOOKS | COPY_CHOOKS);
#if 0
if (cora_fcn) {
	fprintf (stderr, "tnode_copyoraliastree(): flags 0x%2.2x for node (%s,%s)\n", (unsigned int)cora,
			t->tag->ndef->name, t->tag->name);
}
#endif

	if (cora == COPY_ALIAS) {
		return t;
	}

	tmp = tnode_new (t->tag, NULL);
	tmp->org_file = t->org_file;
	tmp->org_line = t->org_line;

	tnd = t->tag->ndef;
	for (i=0; i<DA_CUR (t->items); i++) {
		if (i < tnd->nsub) {
			tnode_t *subcopy;
			
			subcopy = (cora & COPY_SUBS) ? tnode_copyoraliastree (DA_NTHITEM (t->items, i), cora_fcn) : DA_NTHITEM (t->items, i);

			DA_SETNTHITEM (tmp->items, i, subcopy);
		} else if (i >= (tnd->nsub + tnd->nname)) {
			/* hook-node, duplicate... */
			void *hook = DA_NTHITEM (t->items, i);

			if (cora & COPY_HOOKS) {
				if (tnd->hook_copyoralias) {
					hook = tnd->hook_copyoralias (hook, cora_fcn);
				} else if (tnd->hook_copy) {
					hook = tnd->hook_copy (hook);
				}
			}
			DA_SETNTHITEM (tmp->items, i, hook);
		} else {
			DA_SETNTHITEM (tmp->items, i, DA_NTHITEM (t->items, i));
		}
	}

	/* don't forget to do compiler hooks */
#if 0
fprintf (stderr, "tnode_copyoraliastree(): copying [%s], num chooks = %d\n", t->tag->name, DA_CUR (t->chooks));
#endif
	if (DA_CUR (tmp->chooks) < DA_CUR (t->chooks)) {
		dynarray_setsize (tmp->chooks, DA_CUR (acomphooks));
	}
	for (i=0; i<DA_CUR (t->chooks); i++) {
		chook_t *ch = DA_NTHITEM (acomphooks, i);
		void *chc = DA_NTHITEM (t->chooks, i);

		if (ch && chc && (cora & COPY_CHOOKS) && ch->chook_copy) {
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
/*{{{  tnode_t *tnode_copytree (tnode_t *t)*/
/*
 *	copies a tree
 */
tnode_t *tnode_copytree (tnode_t *t)
{
	return tnode_copyoraliastree (t, NULL);
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
/*{{{  static void tnode_ssetindent (FILE *stream, int indent)*/
/*
 *	sets indentation level (debug output in s-records)
 */
static void tnode_ssetindent (FILE *stream, int indent)
{
	int i;

	if (indent) {
		for (i=0; i<indent; i++) {
			fprintf (stream, "  ");
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

	fprintf (stream, "<%s type=\"%s\" origin=\"%s:%d\" addr=\"0x%8.8x\">%s\n", tnd->name, t->tag->name, (t->org_file) ? t->org_file->fnptr : "(none)", t->org_line, (unsigned int)t,
			compopts.dumpfolded ? "<!--{{{-->" : "");
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
	fprintf (stream, "</%s>%s\n", tnd->name, compopts.dumpfolded ? "<!--}}}-->" : "");

	return;
}
/*}}}*/
/*{{{  void tnode_dumpstree (tnode_t *t, int indent, FILE *stream)*/
/*
 *	dumps a parse tree in s-record format
 */
void tnode_dumpstree (tnode_t *t, int indent, FILE *stream)
{
	int i;
	tndef_t *tnd;

	tnode_ssetindent (stream, indent);
	if (!t) {
		fprintf (stream, "null\n");
		return;
	}
	tnd = t->tag->ndef;

	fprintf (stream, "(%s\n", t->tag->name);

	for (i=0; i<DA_CUR (t->items); i++) {
		if (i < tnd->nsub) {
			/* subnode */
			tnode_dumpstree ((tnode_t *)DA_NTHITEM (t->items, i), indent + 1, stream);
		} else if (i < (tnd->nsub + tnd->nname)) {
			/* name */
			name_dumpsname ((name_t *)DA_NTHITEM (t->items, i), indent + 1, stream);
		} else {
			/* hook */
			if (tnd->hook_dumpstree) {
				tnd->hook_dumpstree (t, DA_NTHITEM (t->items, i), indent + 1, stream);
			} else {
				tnode_ssetindent (stream, indent + 1);
				fprintf (stream, "(hook (addr 0x%8.8x))\n", (unsigned int)(DA_NTHITEM (t->items, i)));
			}
		}
	}
	for (i=0; i<DA_CUR (t->chooks); i++) {
		/* compiler hooks */
		chook_t *ch = DA_NTHITEM (acomphooks, i);
		void *chc = DA_NTHITEM (t->chooks, i);

		if (ch && chc && ch->chook_dumpstree) {
			ch->chook_dumpstree (t, chc, indent + 1, stream);
		} else if (ch && chc) {
			tnode_ssetindent (stream, indent + 1);
			fprintf (stream, "(chook (id \"%s\") (addr 0x%8.8x))\n", ch->name, (unsigned int)chc);
		}
	}

	tnode_ssetindent (stream, indent);
	fprintf (stream, ")\n");

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
		fprintf (stream, "     hook_copy=%p, hook_copyoralias=%p, hook_free=%p, hook_dumptree=%p, prefreetree=%p.\n", tnd->hook_copy,
				tnd->hook_copyoralias, tnd->hook_free, tnd->hook_dumptree, tnd->prefreetree);
	}
	for (i=0; i<DA_CUR (anodetags); i++) {
		ntdef_t *ntd = DA_NTHITEM (anodetags, i);

		fprintf (stream, "tag name [%s] type [%s]\n", ntd->name, ntd->ndef->name);
	}

	return;
}
/*}}}*/


/*{{{  static int tnode_callthroughcompops (compops_t *cops, ...)*/
/*
 *	this is a dummy function -- used to indicate that we call-through to something underneath
 */
static int tnode_callthroughcompops (compops_t *cops, ...)
{
	nocc_error ("tnode_callthroughcompops(): shouldn't actually be called!");
	return -1;
}
/*}}}*/
/*{{{  static int tnode_callthroughlangops (langops_t *lops, ...)*/
/*
 *	this is a dummy function -- used to indicate that we call-through to something underneath
 */
static int tnode_callthroughlangops (langops_t *lops, ...)
{
	nocc_error ("tnode_callthroughlangops(): shouldn't actually be called!");
	return -1;
}
/*}}}*/


/*{{{  compops_t *tnode_newcompops (void)*/
/*
 *	creates a new compops_t structure
 */
compops_t *tnode_newcompops (void)
{
	compops_t *cops = (compops_t *)smalloc (sizeof (compops_t));
	int i;

	cops->next = NULL;
	dynarray_init (cops->opfuncs);
	dynarray_setsize (cops->opfuncs, DA_CUR (acompops) + 1);
	for (i=0; i<DA_CUR (cops->opfuncs); i++) {
		DA_SETNTHITEM (cops->opfuncs, i, NULL);
	}

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
	dynarray_trash (cops->opfuncs);
	sfree (cops);
	return;
}
/*}}}*/
/*{{{  compops_t *tnode_insertcompops (compops_t *nextcops)*/
/*
 *	creates a new compops_t structure that calls through to another
 */
compops_t *tnode_insertcompops (compops_t *nextcops)
{
	compops_t *cops = tnode_newcompops ();
	int i;

	cops->next = nextcops;
	for (i=0; (i<DA_CUR (acompops)) && (i<DA_CUR (cops->opfuncs)) && (!nextcops || (i<DA_CUR (nextcops->opfuncs))); i++) {
		compop_t *cop = DA_NTHITEM (acompops, i);

		if (cop && nextcops && DA_NTHITEM (nextcops->opfuncs, i)) {
			DA_SETNTHITEM (cops->opfuncs, i, COMPOPTYPE (tnode_callthroughcompops));
		}
	}

	return cops;
}
/*}}}*/
/*{{{  compops_t *tnode_removecompops (compops_t *cops)*/
/*
 *	removes a compops_t structure that calls through to another
 */
compops_t *tnode_removecompops (compops_t *cops)
{
	compops_t *nextcops = cops->next;

	tnode_freecompops (cops);
	return nextcops;
}
/*}}}*/

/*{{{  int tnode_setcompop (compops_t *cops, char *name, int nparams, int (*fcn)(compops_t *, ...))*/
/*
 *	sets a compiler operation on a compops_t structure by name
 *	returns 0 on success, non-zero on failure
 */
int tnode_setcompop (compops_t *cops, char *name, int nparams, int (*fcn)(compops_t *, ...))
{
	compop_t *cop = stringhash_lookup (compops, name);

	if (!cop) {
		nocc_internal ("tnode_setcompop(): no such compiler operation [%s]", name);
		return -1;
	} else if (cop->nparams != nparams) {
		nocc_error ("tnode_setcompop(): nparams given as %d, expected %d", nparams, cop->nparams);
		return -1;
	}
	if ((int)cop->opno >= DA_CUR (cops->opfuncs)) {
		int i = DA_CUR (cops->opfuncs);

		dynarray_setsize (cops->opfuncs, (int)cop->opno + 1);
		for (; i<DA_CUR (cops->opfuncs); i++) {
			DA_SETNTHITEM (cops->opfuncs, i, NULL);
		}
	}
	DA_SETNTHITEM (cops->opfuncs, (int)cop->opno, (void *)fcn);

	return 0;
}
/*}}}*/
/*{{{  int tnode_setcompop_bottom (compops_t *cops, char *name, int nparams, int (*fcn)(compops_t *, ...))*/
/*
 *	sets a compiler operation on a compops_t structure by name
 *	returns 0 on success, non-zero on failure
 */
int tnode_setcompop_bottom (compops_t *cops, char *name, int nparams, int (*fcn)(compops_t *, ...))
{
	compop_t *cop = stringhash_lookup (compops, name);
	compops_t *cx;

	if (!cop) {
		nocc_internal ("tnode_setcompop_bottom(): no such compiler operation [%s]", name);
		return -1;
	} else if (cop->nparams != nparams) {
		nocc_error ("tnode_setcompop_bottom(): nparams given as %d, expected %d", nparams, cop->nparams);
		return -1;
	}

	/* sets the compiler operation at the "bottom" of a stack -- until we hit one which isn't the last or unset */
	for (cx = cops; cx; cx = cx->next) {
		int lastop = DA_CUR (cx->opfuncs);

		if ((int)cop->opno >= lastop) {
			int i;

			/* not enough opfuncs here anyway, increase */
			dynarray_setsize (cx->opfuncs, (int)cop->opno + 1);
			for (i=lastop; i<DA_CUR (cx->opfuncs); i++) {
				DA_SETNTHITEM (cx->opfuncs, i, NULL);
			}
		}
	}
	for (cx = cops; cx; cx = cx->next) {
		void *xfcn = (void *)DA_NTHITEM (cx->opfuncs, (int)cop->opno);

		if (!xfcn) {
			if (!cx->next) {
				/* last one */
				break;		/* for() */
			} else {
				void *nextfcn = (void *)DA_NTHITEM (cx->next->opfuncs, (int)cop->opno);

				if (nextfcn) {
					/* means we go here */
					break;		/* for() */
				} else {
					/* no next function, so there or lower, put copy-through here */
					DA_SETNTHITEM (cx->opfuncs, (int)cop->opno, (void *)tnode_callthroughcompops);
				}
			}
		} else {
			/* we have a function here, just overwrite it */
			break;		/* for() */
		}
	}

	DA_SETNTHITEM (cx->opfuncs, (int)cop->opno, (void *)fcn);

	return 0;
}
/*}}}*/
/*{{{  int tnode_hascompop (compops_t *cops, char *name)*/
/*
 *	returns non-zero if the specified compops_t structure has an entry for 'name'
 */
int tnode_hascompop (compops_t *cops, char *name)
{
	compop_t *cop = stringhash_lookup (compops, name);

	if (!cops) {
		return 0;
	}
	if (!cop) {
		nocc_internal ("tnode_hascompop(): no such compiler operation [%s]", name);
		return -1;
	}
	if ((int)cop->opno >= DA_CUR (cops->opfuncs)) {
		return 0;
	}
	if (DA_NTHITEM (cops->opfuncs, (int)cop->opno)) {
		return 1;
	}
	return 0;
}
/*}}}*/
/*{{{  static int tnode_icallcompop (compops_t *cops, compop_t *op, va_list ap)*/
/*
 *	internal function to call a compiler operation from the given compops_t structure, passing the given parameters
 *	returns function's return value on success (usually 0 or 1), <0 on failure
 */
static int tnode_icallcompop (compops_t *cops, compop_t *op, va_list ap)
{
	int (*fcn)(compops_t *, ...);
	int r;

	if (op->dotrace) {
		nocc_message ("compoptrace: 0x%8.8x [%s]", (unsigned int)op, op->name);
	}
	fcn = (int (*)(compops_t *, ...))DA_NTHITEM (cops->opfuncs, (int)op->opno);
	while (fcn == COMPOPTYPE (tnode_callthroughcompops)) {
		if (!cops->next) {
			nocc_internal ("tnode_icallcompop(): called operation [%s] ran out of call-through markers!", op->name);
			return -1;
		}
		cops = cops->next;
		if (((int)op->opno >= DA_CUR (cops->opfuncs)) || !DA_NTHITEM (cops->opfuncs, (int)op->opno)) {
			nocc_warning ("tnode_icallcompop(): no such operation [%s] in compops at 0x%8.8x", op->name, (unsigned int)cops);
			return -1;
		}
		fcn = (int (*)(compops_t *, ...))DA_NTHITEM (cops->opfuncs, (int)op->opno);
	}
	
	switch (op->nparams) {
	case 0:
		r = fcn (cops);
		break;
	case 1:
		{
			void *arg0 = va_arg (ap, void *);

			r = fcn (cops, arg0);
		}
		break;
	case 2:
		{
			void *arg0 = va_arg (ap, void *);
			void *arg1 = va_arg (ap, void *);

			r = fcn (cops, arg0, arg1);
		}
		break;
	case 3:
		{
			void *arg0 = va_arg (ap, void *);
			void *arg1 = va_arg (ap, void *);
			void *arg2 = va_arg (ap, void *);

			r = fcn (cops, arg0, arg1, arg2);
		}
		break;
	case 4:
		{
			void *arg0 = va_arg (ap, void *);
			void *arg1 = va_arg (ap, void *);
			void *arg2 = va_arg (ap, void *);
			void *arg3 = va_arg (ap, void *);

			r = fcn (cops, arg0, arg1, arg2, arg3);
		}
		break;
	case 5:
		{
			void *arg0 = va_arg (ap, void *);
			void *arg1 = va_arg (ap, void *);
			void *arg2 = va_arg (ap, void *);
			void *arg3 = va_arg (ap, void *);
			void *arg4 = va_arg (ap, void *);

			r = fcn (cops, arg0, arg1, arg2, arg3, arg4);
		}
		break;
	case 6:
		{
			void *arg0 = va_arg (ap, void *);
			void *arg1 = va_arg (ap, void *);
			void *arg2 = va_arg (ap, void *);
			void *arg3 = va_arg (ap, void *);
			void *arg4 = va_arg (ap, void *);
			void *arg5 = va_arg (ap, void *);

			r = fcn (cops, arg0, arg1, arg2, arg3, arg4, arg5);
		}
		break;
	case 7:
		{
			void *arg0 = va_arg (ap, void *);
			void *arg1 = va_arg (ap, void *);
			void *arg2 = va_arg (ap, void *);
			void *arg3 = va_arg (ap, void *);
			void *arg4 = va_arg (ap, void *);
			void *arg5 = va_arg (ap, void *);
			void *arg6 = va_arg (ap, void *);

			r = fcn (cops, arg0, arg1, arg2, arg3, arg4, arg5, arg6);
		}
		break;
	default:
		nocc_error ("tnode_icallcompop(): asked for %d params, but not that many supported here!", op->nparams);
		r = -1;
		break;
	}

	return r;
}
/*}}}*/
/*{{{  int tnode_callcompop (compops_t *cops, char *name, int nparams, ...)*/
/*
 *	calls a compiler operation from the given compops_t structure by name, passing the given parameters
 *	returns function's return value on success (usually 0 or 1), <0 on failure
 */
int tnode_callcompop (compops_t *cops, char *name, int nparams, ...)
{
	compop_t *cop = stringhash_lookup (compops, name);
	va_list ap;
	int r;

	if (!cop) {
		nocc_internal ("tnode_callcompop(): no such compiler operation [%s]", name);
		return -1;
	} else if (cop->nparams != nparams) {
		nocc_error ("tnode_callcompop(): nparams given as %d, expected %d", nparams, cop->nparams);
		return -1;
	}
	if (((int)cop->opno >= DA_CUR (cops->opfuncs)) || !DA_NTHITEM (cops->opfuncs, (int)cop->opno)) {
		nocc_warning ("tnode_callcompop(): no such operation [%s] in compops at 0x%8.8x", cop->name, (unsigned int)cops);
		return -1;
	}

	va_start (ap, nparams);
	r = tnode_icallcompop (cops, cop, ap);
	va_end (ap);

	return r;
}
/*}}}*/
/*{{{  int tnode_hascompop_i (compops_t *cops, int idx)*/
/*
 *	returns non-zero if the specified compops_t structure has an entry for 'op'
 */
int tnode_hascompop_i (compops_t *cops, int idx)
{
	compop_t *cop;
	
	if ((idx < 0) || (idx >= DA_CUR (acompops))) {
		nocc_error ("tnode_hascompop_i(): no such compiler operation [index %d]", idx);
		return -1;
	} else {
		cop = DA_NTHITEM (acompops, idx);
	}
	if (!cop) {
		nocc_internal ("tnode_haslangop_i(): no such compiler operation [index %d]", idx);
		return -1;
	}
	if ((int)cop->opno >= DA_CUR (cops->opfuncs)) {
		return 0;
	}
	if (DA_NTHITEM (cops->opfuncs, (int)cop->opno)) {
		return 1;
	}
	return 0;
}
/*}}}*/
/*{{{  int tnode_callcompop_i (compops_t *cops, int idx, int nparams, ...)*/
/*
 *	calls a compiler operation from the given compops_t structure by index, passing the given parameters
 *	returns function's return value on success (usually 0 or 1), <0 on failure
 */
int tnode_callcompop_i (compops_t *cops, int idx, int nparams, ...)
{
	compop_t *cop;
	va_list ap;
	int r;

	if ((idx < 0) || (idx >= DA_CUR (acompops))) {
		nocc_error ("tnode_callcompop_i(): no such compiler operation [index %d]", idx);
		return -1;
	} else {
		cop = DA_NTHITEM (acompops, idx);
	}
	if (!cop) {
		nocc_internal ("tnode_callcompop_i(): no such compiler operation [index %d]", idx);
		return -1;
	} else if (cop->nparams != nparams) {
		nocc_error ("tnode_callcompop(): nparams given as %d, expected %d", nparams, cop->nparams);
		return -1;
	}
	if (((int)cop->opno >= DA_CUR (cops->opfuncs)) || !DA_NTHITEM (cops->opfuncs, (int)cop->opno)) {
		nocc_warning ("tnode_callcompop(): no such operation [%s, index %d] in compops at 0x%8.8x", cop->name, idx, (unsigned int)cops);
		return -1;
	}

	va_start (ap, nparams);
	r = tnode_icallcompop (cops, cop, ap);
	va_end (ap);

	return r;
}
/*}}}*/
/*{{{  int tnode_newcompop (char *name, compops_e opno, int nparams, origin_t *origin)*/
/*
 *	creates a new compiler operation with the given name;  if 'opno' is valid (!= COPS_INVALID), setting a preset one
 *	returns index on success, <0 on failure
 */
int tnode_newcompop (char *name, compops_e opno, int nparams, origin_t *origin)
{
	compop_t *cop = stringhash_lookup (compops, name);

	if (cop) {
		nocc_warning ("tnode_newcompop(): already got [%s]", name);
		return (int)cop->opno;
	}
	cop = (compop_t *)smalloc (sizeof (compop_t));
	cop->name = string_dup (name);
	if (opno == COPS_INVALID) {
		/* means select one */
		cop->opno = (compops_e)DA_CUR (acompops);
	} else {
		cop->opno = opno;
	}
	if ((int)cop->opno >= DA_CUR (acompops)) {
		/* need a bit more room */
		int i = DA_CUR (acompops);

		dynarray_setsize (acompops, (int)cop->opno + 1);
#if 0
fprintf (stderr, "tnode_newcompop(): extra clear from %d to %d\n", (int)cop->opno, DA_CUR (acompops));
#endif
		for (; i<DA_CUR (acompops); i++) {
			DA_SETNTHITEM (acompops, i, NULL);
		}
	}
	cop->nparams = nparams;
	cop->dotrace = stringhash_lookup (tracingcompops, name) ? 1 : 0;
	cop->origin = origin;

	stringhash_insert (compops, cop, cop->name);
	DA_SETNTHITEM (acompops, (int)cop->opno, cop);

	return (int)cop->opno;
}
/*}}}*/
/*{{{  compop_t *tnode_findcompop (char *name)*/
/*
 *	finds a compiler operation by name
 *	returns compop_t pointer on success, NULL on failure
 */
compop_t *tnode_findcompop (char *name)
{
	return stringhash_lookup (compops, name);
}
/*}}}*/
/*{{{  void tnode_dumpcompops (compops_t *cops, FILE *stream)*/
/*
 *	dumps contents of the specified compiler-operation (debugging)
 */
void tnode_dumpcompops (compops_t *cops, FILE *stream)
{
	int i;
	compops_t *cx;

	fprintf (stream, "%-25s      ", "compops at:");
	for (cx = cops; cx; cx = cx->next) {
		fprintf (stream, "0x%8.8x  ", (unsigned int)cx);
	}
	fprintf (stream, "\n");
	fprintf (stream, "%-25s      ", "");
	for (cx = cops; cx; cx = cx->next) {
		fprintf (stream, "------------");
	}
	fprintf (stream, "\n");

	for (i=0; i<DA_CUR (acompops); i++) {
		compop_t *cop = DA_NTHITEM (acompops, i);

		if (cop) {
			fprintf (stream, "    %-25s  ", cop->name);

			for (cx = cops; cx; cx = cx->next) {
				if (cop->opno >= DA_CUR (cx->opfuncs)) {
					fprintf (stream, "--          ");
				} else {
					void *fcn = DA_NTHITEM (cx->opfuncs, cop->opno);

					if (fcn == (void *)tnode_callthroughcompops) {
						fprintf (stream, "---->       ");
					} else {
						fprintf (stream, "0x%8.8x  ", (unsigned int)fcn);
					}
				}
			}
			fprintf (stream, "\n");
		}
	}
}
/*}}}*/


/*{{{  langops_t *tnode_newlangops (void)*/
/*
 *	creates a new langops_t structure
 */
langops_t *tnode_newlangops (void)
{
	langops_t *lops = (langops_t *)smalloc (sizeof (langops_t));
	int i;

	lops->next = NULL;
	dynarray_init (lops->opfuncs);
	dynarray_setsize (lops->opfuncs, DA_CUR (alangops) + 1);
	for (i=0; i<DA_CUR (lops->opfuncs); i++) {
		DA_SETNTHITEM (lops->opfuncs, i, NULL);
	}

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
	dynarray_trash (lops->opfuncs);
	sfree (lops);
	return;
}
/*}}}*/
/*{{{  langops_t *tnode_insertlangops (langops_t *nextlops)*/
/*
 *	creates a new langops_t structure that calls through to another
 */
langops_t *tnode_insertlangops (langops_t *nextlops)
{
	langops_t *lops = tnode_newlangops ();
	int i;

	lops->next = nextlops;
	for (i=0; (i<DA_CUR (alangops)) && (i<DA_CUR (lops->opfuncs)) && (!nextlops || (i<DA_CUR (nextlops->opfuncs))); i++) {
		langop_t *lop = DA_NTHITEM (alangops, i);

		if (lop && nextlops && DA_NTHITEM (nextlops->opfuncs, i)) {
			DA_SETNTHITEM (lops->opfuncs, i, LANGOPTYPE (tnode_callthroughlangops));
		}
	}

	return lops;
}
/*}}}*/
/*{{{  langops_t *tnode_removelangops (langops_t *lops)*/
/*
 *	removes a langops_t structure that calls through to another
 */
langops_t *tnode_removelangops (langops_t *lops)
{
	langops_t *nextlops = lops->next;

	tnode_freelangops (lops);
	return nextlops;
}
/*}}}*/

/*{{{  int tnode_setlangop (langops_t *lops, char *name, int nparams, int (*fcn)(langops_t *, ...))*/
/*
 *	sets a language-operation on a node
 *	returns 0 on success, non-zero on failure
 */
int tnode_setlangop (langops_t *lops, char *name, int nparams, int (*fcn)(langops_t *, ...))
{
	langop_t *lop = stringhash_lookup (langops, name);

	if (!lop) {
		nocc_internal ("tnode_setlangop(): no such language operation [%s]", name);
		return -1;
	} else if (lop->nparams != nparams) {
		nocc_error ("tnode_setlangop(): nparams given as %d, expected %d [%s]", nparams, lop->nparams, lop->name);
		return -1;
	}
	if ((int)lop->opno >= DA_CUR (lops->opfuncs)) {
		int i = DA_CUR (lops->opfuncs);

		dynarray_setsize (lops->opfuncs, (int)lop->opno + 1);
		for (; i<DA_CUR (lops->opfuncs); i++) {
			DA_SETNTHITEM (lops->opfuncs, i, NULL);
		}
	}
	DA_SETNTHITEM (lops->opfuncs, (int)lop->opno, (void *)fcn);

	return 0;
}
/*}}}*/
/*{{{  int tnode_haslangop (langops_t *lops, char *name)*/
/*
 *	tests to see whether the specified language-operation is set
 *	returns non-zero if set, zero otherwise
 */
int tnode_haslangop (langops_t *lops, char *name)
{
	langop_t *lop = stringhash_lookup (langops, name);

	if (!lops) {
		return 0;
	}
	if (!lop) {
		nocc_internal ("tnode_haslangop(): no such language operation [%s]", name);
		return -1;
	}
	if ((int)lop->opno >= DA_CUR (lops->opfuncs)) {
		return 0;
	}
	if (DA_NTHITEM (lops->opfuncs, (int)lop->opno)) {
		return 1;
	}
	return 0;
}
/*}}}*/
/*{{{  static int tnode_icalllangop (langops_t *lops, langop_t *op, va_list ap)*/
/*
 *	internal function to call a language operation from the given langops_t structure, passing the given parameters
 *	returns function's return value on success (could be anything), <0 on failure
 */
static int tnode_icalllangop (langops_t *lops, langop_t *op, va_list ap)
{
	int (*fcn)(langops_t *, ...);
	int r;

	if (op->dotrace) {
		nocc_message ("langoptrace: [%s]", op->name);
	}

	fcn = (int (*)(langops_t *, ...))DA_NTHITEM (lops->opfuncs, (int)op->opno);
	while (fcn == LANGOPTYPE (tnode_callthroughlangops)) {
		if (!lops->next) {
			nocc_internal ("tnode_icalllangop(): called operation [%s] ran out of call-through markers!", op->name);
			return -1;
		}
		lops = lops->next;
		if (((int)op->opno >= DA_CUR (lops->opfuncs)) || !DA_NTHITEM (lops->opfuncs, (int)op->opno)) {
			nocc_warning ("tnode_icalllangop(): no such operation [%s] in langops at 0x%8.8x", op->name, (unsigned int)lops);
			return -1;
		}
		fcn = (int (*)(langops_t *, ...))DA_NTHITEM (lops->opfuncs, (int)op->opno);
	}
	
	switch (op->nparams) {
	case 0:
		r = fcn (lops);
		break;
	case 1:
		{
			void *arg0 = va_arg (ap, void *);

			r = fcn (lops, arg0);
		}
		break;
	case 2:
		{
			void *arg0 = va_arg (ap, void *);
			void *arg1 = va_arg (ap, void *);

			r = fcn (lops, arg0, arg1);
		}
		break;
	case 3:
		{
			void *arg0 = va_arg (ap, void *);
			void *arg1 = va_arg (ap, void *);
			void *arg2 = va_arg (ap, void *);

			r = fcn (lops, arg0, arg1, arg2);
		}
		break;
	case 4:
		{
			void *arg0 = va_arg (ap, void *);
			void *arg1 = va_arg (ap, void *);
			void *arg2 = va_arg (ap, void *);
			void *arg3 = va_arg (ap, void *);

			r = fcn (lops, arg0, arg1, arg2, arg3);
		}
		break;
	case 5:
		{
			void *arg0 = va_arg (ap, void *);
			void *arg1 = va_arg (ap, void *);
			void *arg2 = va_arg (ap, void *);
			void *arg3 = va_arg (ap, void *);
			void *arg4 = va_arg (ap, void *);

			r = fcn (lops, arg0, arg1, arg2, arg3, arg4);
		}
		break;
	case 6:
		{
			void *arg0 = va_arg (ap, void *);
			void *arg1 = va_arg (ap, void *);
			void *arg2 = va_arg (ap, void *);
			void *arg3 = va_arg (ap, void *);
			void *arg4 = va_arg (ap, void *);
			void *arg5 = va_arg (ap, void *);

			r = fcn (lops, arg0, arg1, arg2, arg3, arg4, arg5);
		}
		break;
	case 7:
		{
			void *arg0 = va_arg (ap, void *);
			void *arg1 = va_arg (ap, void *);
			void *arg2 = va_arg (ap, void *);
			void *arg3 = va_arg (ap, void *);
			void *arg4 = va_arg (ap, void *);
			void *arg5 = va_arg (ap, void *);
			void *arg6 = va_arg (ap, void *);

			r = fcn (lops, arg0, arg1, arg2, arg3, arg4, arg5, arg6);
		}
		break;
	default:
		nocc_error ("tnode_icalllangop(): asked for %d params, but not that many supported here!", op->nparams);
		r = -1;
		break;
	}

	return r;
}
/*}}}*/
/*{{{  int tnode_calllangop (langops_t *lops, char *name, int nparams, ...)*/
/*
 *	calls a language operation from the given langops_t structure by name, passing the given parameters
 *	returns function's return value on success (could be any type for langops), <0 on failure
 */
int tnode_calllangop (langops_t *lops, char *name, int nparams, ...)
{
	langop_t *lop = stringhash_lookup (langops, name);
	va_list ap;
	int r;

	if (!lop) {
		nocc_internal ("tnode_calllangop(): no such language operation [%s]", name);
		return -1;
	} else if (lop->nparams != nparams) {
		nocc_error ("tnode_calllangop(): nparams given as %d, expected %d", nparams, lop->nparams);
		return -1;
	}
	if (((int)lop->opno >= DA_CUR (lops->opfuncs)) || !DA_NTHITEM (lops->opfuncs, (int)lop->opno)) {
		nocc_warning ("tnode_calllangop(): no such operation [%s] in langops at 0x%8.8x", lop->name, (unsigned int)lops);
		return -1;
	}

	va_start (ap, nparams);
	r = tnode_icalllangop (lops, lop, ap);
	va_end (ap);

	return r;
}
/*}}}*/
/*{{{  int tnode_haslangop_i (langops_t *lops, int idx)*/
/*
 *	returns non-zero if the specified langops_t structure has an entry for 'idx'
 */
int tnode_haslangop_i (langops_t *lops, int idx)
{
	langop_t *lop;
	
	if (!lops) {
		return 0;
	}
	if ((idx < 0) || (idx >= DA_CUR (alangops))) {
		nocc_error ("tnode_haslangop_i(): no such language operation [index %d]", idx);
		return -1;
	} else {
		lop = DA_NTHITEM (alangops, idx);
	}
	if (!lop) {
		nocc_internal ("tnode_haslangop_i(): no such language operation [index %d]", idx);
		return -1;
	}
	if ((int)lop->opno >= DA_CUR (lops->opfuncs)) {
		return 0;
	}
	if (DA_NTHITEM (lops->opfuncs, (int)lop->opno)) {
		return 1;
	}
	return 0;
}
/*}}}*/
/*{{{  int tnode_calllangop_i (langops_t *lops, int idx, int nparams, ...)*/
/*
 *	calls a language operation from the given langops_t structure by index, passing the given parameters
 *	returns function's return value on success (may be anything for language-ops), <0 on failure
 */
int tnode_calllangop_i (langops_t *lops, int idx, int nparams, ...)
{
	langop_t *lop;
	va_list ap;
	int r;

	if ((idx < 0) || (idx >= DA_CUR (alangops))) {
		nocc_error ("tnode_calllangop_i(): no such compiler operation [index %d]", idx);
		return -1;
	} else {
		lop = DA_NTHITEM (alangops, idx);
	}
	if (!lop) {
		nocc_internal ("tnode_calllangop_i(): no such compiler operation [index %d]", idx);
		return -1;
	} else if (lop->nparams != nparams) {
		nocc_error ("tnode_calllangop(): nparams given as %d, expected %d", nparams, lop->nparams);
		return -1;
	}
	if (((int)lop->opno >= DA_CUR (lops->opfuncs)) || !DA_NTHITEM (lops->opfuncs, (int)lop->opno)) {
		nocc_warning ("tnode_calllangop(): no such operation [%s, index %d] in langops at 0x%8.8x", lop->name, idx, (unsigned int)lops);
		return -1;
	}

	va_start (ap, nparams);
	r = tnode_icalllangop (lops, lop, ap);
	va_end (ap);

	return r;
}
/*}}}*/
/*{{{  int tnode_newlangop (char *name, langops_e opno, int nparams, origin_t *origin)*/
/*
 *	creates a new language operation with the given name;  if 'opno' is valid (!= LOPS_INVALID), setting a preset one
 *	returns index on success, <0 on failure
 */
int tnode_newlangop (char *name, langops_e opno, int nparams, origin_t *origin)
{
	langop_t *lop = stringhash_lookup (langops, name);

	if (lop) {
		nocc_warning ("tnode_newlangop(): already got [%s]", name);
		return (int)lop->opno;
	}
	lop = (langop_t *)smalloc (sizeof (langop_t));
	lop->name = string_dup (name);
	if (opno == COPS_INVALID) {
		/* means select one */
		lop->opno = (langops_e)DA_CUR (alangops);
	} else {
		lop->opno = opno;
	}
	if ((int)lop->opno >= DA_CUR (alangops)) {
		/* need a bit more room */
		int i = DA_CUR (alangops);

		dynarray_setsize (alangops, (int)lop->opno + 1);
		for (; i<DA_CUR (alangops); i++) {
			DA_SETNTHITEM (alangops, i, NULL);
		}
	}
	lop->nparams = nparams;
	lop->dotrace = stringhash_lookup (tracinglangops, name) ? 1 : 0;
	lop->origin = origin;

	stringhash_insert (langops, lop, lop->name);
	DA_SETNTHITEM (alangops, (int)lop->opno, lop);

	return (int)lop->opno;
}
/*}}}*/
/*{{{  langop_t *tnode_findlangop (char *name)*/
/*
 *	finds a language operation by name
 *	returns langop_t pointer on success, NULL on failure
 */
langop_t *tnode_findlangop (char *name)
{
	return stringhash_lookup (langops, name);
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
	ch->flags = CHOOK_NONE;
	ch->chook_copy = NULL;
	ch->chook_free = NULL;
	ch->chook_dumptree = NULL;
	ch->chook_dumpstree = NULL;

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
/*{{{  chook_t *tnode_lookupornewchook (const char *name)*/
/*
 *	creates a new compiler-hook, or returns an existing one
 */
chook_t *tnode_lookupornewchook (const char *name)
{
	chook_t *chook;

	if (!(chook = tnode_lookupchookbyname (name))) {
		chook = tnode_newchook (name);
	}

	return chook;
}
/*}}}*/
/*{{{  int tnode_haschook (tnode_t *t, chook_t *ch)*/
/*
 *	returns non-zero if the specified compiler-hook is present
 */
int tnode_haschook (tnode_t *t, chook_t *ch)
{
	if (!ch || !t) {
		nocc_internal ("tnode_haschook(): null chook or tree!");
		return 0;
	}
	if (ch->id >= DA_CUR (t->chooks)) {
		/* no such hook */
		return 0;
	}
	return 1;
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
/*{{{  void tnode_clearchook (tnode_t *t, chook_t *ch)*/
/*
 *	clears a compiler-hook node for some tree-node
 *	this doesn't attempt to call the free -- just NULLs the entry
 */
void tnode_clearchook (tnode_t *t, chook_t *ch)
{
	if (!ch || !t) {
		nocc_internal ("tnode_clearchook(): null chook or tree!");
	}
	if (ch->id >= DA_CUR (t->chooks)) {
		return;		/* doesn't exist anyway */
	}

	DA_SETNTHITEM (t->chooks, ch->id, NULL);

	return;
}
/*}}}*/
/*{{{  void tnode_dumpchooks (FILE *stream)*/
/*
 *	dumps compiler hooks (debugging)
 */
void tnode_dumpchooks (FILE *stream)
{
	int i;

	for (i=0; i<DA_CUR (acomphooks); i++) {
		chook_t *ch = DA_NTHITEM (acomphooks, i);

		fprintf (stream, "%-2d copy=0x%8.8x free=0x%8.8x dumptree=0x%8.8x %s\n", ch->id, (unsigned int)ch->chook_copy,
				(unsigned int)ch->chook_free, (unsigned int)ch->chook_dumptree, ch->name);
	}
	return;
}
/*}}}*/


/*{{{  int tnode_promotechooks (tnode_t *tsource, tnode_t *tdest)*/
/*
 *	promotes compiler hooks from one node to another (moves them)
 *	returns number of hooks moved
 */
int tnode_promotechooks (tnode_t *tsource, tnode_t *tdest)
{
	int i;
	int moved = 0;

	if (!tsource || !tdest) {
		nocc_internal ("tnode_promotechooks(): null tree!");
	}
	for (i=0; i<DA_CUR (tsource->chooks); i++) {
		chook_t *chdef = DA_NTHITEM (acomphooks, i);

		if ((chdef->flags & CHOOK_AUTOPROMOTE) && DA_NTHITEM (tsource->chooks, i)) {
			/* this one */
			tnode_setchook (tdest, chdef, DA_NTHITEM (tsource->chooks, i));
			DA_SETNTHITEM (tsource->chooks, i, NULL);
			moved++;
		}
	}

	return moved;
}
/*}}}*/
/*{{{  char *tnode_copytextlocationof (tnode_t *t)*/
/*
 *	generates a copy of the location of a node (in its source file)
 *	returns new string on success, NULL on failure
 */
char *tnode_copytextlocationof (tnode_t *t)
{
	char *str;

	if (!t) {
		nocc_warning ("tnode_copytextlocationof(): NULL node!");
		return NULL;
	}
	if (!t->org_file || !t->org_file->fnptr) {
		str = (char *)smalloc (64);
		sprintf (str, "(unknown file):%d", t->org_line);
	} else {
		str = (char *)smalloc (strlen (t->org_file->fnptr) + 16);
		sprintf (str, "%s:%d", t->org_file->fnptr, t->org_line);
	}
	return str;
}
/*}}}*/


/*{{{  int tnode_bytesfor (tnode_t *t, target_t *target)*/
/*
 *	returns the number of "bytes-for" a tree-node
 */
int tnode_bytesfor (tnode_t *t, target_t *target)
{
#if 0
fprintf (stderr, "tnode_bytesfor(): t = [%s]\n", t->tag->name);
#endif
	if (target && ((t->tag == target->tag_NAME) || (t->tag == target->tag_NAMEREF))) {
		/* look at typesize */
		t = tnode_nthsubof (t, 0);
	}
	if (t && t->tag->ndef->lops && tnode_haslangop_i (t->tag->ndef->lops, (int)LOPS_BYTESFOR)) {
		return tnode_calllangop_i (t->tag->ndef->lops, (int)LOPS_BYTESFOR, 2, t, target);
	}
	return -1;		/* don't know */
}
/*}}}*/
/*{{{  int tnode_issigned (tnode_t *t, target_t *target)*/
/*
 *	returns the signedness of a tree-node, similar to "bytes-for"
 *	returns 0 if unsigned, 1 if signed, -1 if unknown/error
 */
int tnode_issigned (tnode_t *t, target_t *target)
{
	if (t && t->tag->ndef->lops && tnode_haslangop_i (t->tag->ndef->lops, (int)LOPS_ISSIGNED)) {
		return tnode_calllangop_i (t->tag->ndef->lops, (int)LOPS_ISSIGNED, 2, t, target);
	}
	return -1;		/* don't know */
}
/*}}}*/


/*{{{  void tnode_message (tnode_t *t, const char *fmt, ...)*/
/*
 *	generates a generic message
 */
void tnode_message (tnode_t *t, const char *fmt, ...)
{
	va_list ap;
	static char msgbuf[512];
	int n;
	lexfile_t *lf = t->org_file;

	va_start (ap, fmt);
	n = sprintf (msgbuf, "%s:%d ", lf ? lf->fnptr : "(unknown)", t->org_line);
	vsnprintf (msgbuf + n, 512 - n, fmt, ap);
	va_end (ap);

	nocc_outerrmsg (msgbuf);
	return;
}
/*}}}*/
/*{{{  void tnode_warning (tnode_t *t, const char *fmt, ...)*/
/*
 *	generates a generic warning message
 */
void tnode_warning (tnode_t *t, const char *fmt, ...)
{
	va_list ap;
	static char warnbuf[512];
	int n;
	lexfile_t *lf = t->org_file;

	va_start (ap, fmt);
	n = sprintf (warnbuf, "%s:%d (warning) ", lf ? lf->fnptr : "(unknown)", t->org_line);
	vsnprintf (warnbuf + n, 512 - n, fmt, ap);
	va_end (ap);

	if (lf) {
		lf->warncount++;
	}

	nocc_outerrmsg (warnbuf);

	return;
}
/*}}}*/
/*{{{  void tnode_error (tnode_t *t, const char *fmt, ...)*/
/*
 *	generates an error message
 */
void tnode_error (tnode_t *t, const char *fmt, ...)
{
	va_list ap;
	static char errbuf[512];
	int n;
	lexfile_t *lf = t->org_file;

	va_start (ap, fmt);
	n = sprintf (errbuf, "%s:%d (error) ", lf ? lf->fnptr : "(unknown)", t->org_line);
	vsnprintf (errbuf + n, 512 - n, fmt, ap);
	va_end (ap);

	if (lf) {
		lf->errcount++;
	}

	nocc_outerrmsg (errbuf);

	return;
}
/*}}}*/


/*{{{  void tnode_dumpsnodetypes (FILE *stream)*/
/*
 *	dumps a list of the registered node types (short form)
 */
void tnode_dumpsnodetypes (FILE *stream)
{
	int i;

	fprintf (stream, "node types:\n");
	for (i=0; i<DA_CUR (anodetypes); i++) {
		tndef_t *tnd = DA_NTHITEM (anodetypes, i);

		fprintf (stream, "    %-32s (%d,%d,%d)\n", tnd->name, tnd->nsub, tnd->nname, tnd->nhooks);
	}
	return;
}
/*}}}*/
/*{{{  void tnode_dumpsnodetags (FILE *stream)*/
/*
 *	dumps a list of the registered node tags (short form)
 */
void tnode_dumpsnodetags (FILE *stream)
{
	int i;

	fprintf (stream, "node tags:\n");
	for (i=0; i<DA_CUR (anodetags); i++) {
		ntdef_t *ntd = DA_NTHITEM (anodetags, i);

		fprintf (stream, "    %s\n", ntd->name);
	}
	return;
}
/*}}}*/


