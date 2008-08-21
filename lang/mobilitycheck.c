/*
 *	mobilitycheck.c -- mobility checker for NOCC
 *	Copyright (C) 2007-2008 Fred Barnes <frmb@kent.ac.uk>
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


/*{{{  includes*/
#ifdef have_config_h
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/types.h>
#include <errno.h>

#include "nocc.h"
#include "support.h"
#include "version.h"
#include "symbols.h"
#include "keywords.h"
#include "opts.h"
#include "tnode.h"
#include "crypto.h"
#include "treeops.h"
#include "xml.h"
#include "parser.h"
#include "parsepriv.h"
#include "dfa.h"
#include "names.h"
#include "langops.h"
#include "metadata.h"
#include "tracescheck.h"
#include "mobilitycheck.h"


/*}}}*/
/*{{{  private data*/
static chook_t *mchk_traceschook = NULL;

/*}}}*/


/*{{{  static void mchk_isetindent (FILE *stream, int indent)*/
/*
 *	sets indent for debug output
 */
static void mchk_isetindent (FILE *stream, int indent)
{
	int i;

	for (i=0; i<indent; i++) {
		fprintf (stream, "    ");
	}
	return;
}
/*}}}*/
/*{{{  static mchknode_t *mchk_newmchknode (void)*/
/*
 *	creates a new (blank) mchknode_t structure
 */
static mchknode_t *mchk_newmchknode (void)
{
	mchknode_t *mcn = (mchknode_t *)smalloc (sizeof (mchknode_t));

	mcn->type = MCN_INVALID;
	mcn->orgnode = NULL;

	return mcn;
}
/*}}}*/
/*{{{  static mchknode_t *mchk_newmchknodet (mchknodetype_e type, tnode_t *orgnode)*/
/*
 *	creates a new mchknode_t structure with the specified type and origin
 */
static mchknode_t *mchk_newmchknodet (mchknodetype_e type, tnode_t *orgnode)
{
	mchknode_t *mcn = mchk_newmchknode ();

	mcn->type = type;
	mcn->orgnode = orgnode;

	switch (mcn->type) {
	case MCN_INVALID:
		break;
	case MCN_INPUT:
	case MCN_OUTPUT:
		mcn->u.mcnio.chanptr = NULL;
		mcn->u.mcnio.varptr = NULL;
		break;
	case MCN_PARAM:
	case MCN_VAR:
		mcn->u.mcnpv.id = NULL;
		break;
	case MCN_PARAMREF:
	case MCN_VARREF:
		mcn->u.mcnref.ref = NULL;
		break;
	case MCN_SEQ:
		dynarray_init (mcn->u.mcnlist.items);
		break;
	}

	return mcn;
}
/*}}}*/
/*{{{  static void mchk_freemchknode (mchknode_t *mcn)*/
/*
 *	frees an mchknode_t structure
 */
static void mchk_freemchknode (mchknode_t *mcn)
{
	int i;

	if (!mcn) {
		nocc_internal ("mchk_freemchknode(): NULL pointer!");
		return;
	}

	switch (mcn->type) {
	case MCN_INVALID:
		break;
	case MCN_INPUT:
	case MCN_OUTPUT:
		if (mcn->u.mcnio.chanptr) {
			mchk_freemchknode (mcn->u.mcnio.chanptr);
			mcn->u.mcnio.chanptr = NULL;
		}
		if (mcn->u.mcnio.varptr) {
			mchk_freemchknode (mcn->u.mcnio.varptr);
			mcn->u.mcnio.varptr = NULL;
		}
		break;
	case MCN_PARAM:
	case MCN_VAR:
		if (mcn->u.mcnpv.id) {
			sfree (mcn->u.mcnpv.id);
			mcn->u.mcnpv.id = NULL;
		}
		break;
	case MCN_PARAMREF:
	case MCN_VARREF:
		mcn->u.mcnref.ref = NULL;
		break;
	case MCN_SEQ:
		for (i=0; i<DA_CUR (mcn->u.mcnlist.items); i++) {
			mchk_freemchknode (DA_NTHITEM (mcn->u.mcnlist.items, i));
		}
		dynarray_trash (mcn->u.mcnlist.items);
		break;
	}
	mcn->type = MCN_INVALID;

	sfree (mcn);
	return;
}
/*}}}*/
/*{{{  static mchk_traces_t *mchk_newmchktraces (void)*/
/*
 *	creates a new mchk_traces_t structure
 */
static mchk_traces_t *mchk_newmchktraces (void)
{
	mchk_traces_t *mct = (mchk_traces_t *)smalloc (sizeof (mchk_traces_t));

	dynarray_init (mct->items);
	dynarray_init (mct->params);
	dynarray_init (mct->vars);

	return mct;
}
/*}}}*/
/*{{{  static void mchk_freemchktraces (mchk_traces_t *mct)*/
/*
 *	trashes a mchk_traces_t structure
 */
static void mchk_freemchktraces (mchk_traces_t *mct)
{
	int i;

	if (!mct) {
		nocc_internal ("mchk_freemchktraces(): NULL pointer!");
		return;
	}

	for (i=0; i<DA_CUR (mct->items); i++) {
		mchk_freemchknode (DA_NTHITEM (mct->items, i));
	}
	dynarray_trash (mct->items);

	for (i=0; i<DA_CUR (mct->params); i++) {
		mchk_freemchknode (DA_NTHITEM (mct->params, i));
	}
	dynarray_trash (mct->params);

	for (i=0; i<DA_CUR (mct->vars); i++) {
		mchk_freemchknode (DA_NTHITEM (mct->vars, i));
	}
	dynarray_trash (mct->vars);

	sfree (mct);
	return;
}
/*}}}*/
/*{{{  static mchk_bucket_t *mchk_newmchkbucket (void)*/
/*
 *	creates a new mchk_bucket_t structure
 */
static mchk_bucket_t *mchk_newmchkbucket (void)
{
	mchk_bucket_t *mcb = (mchk_bucket_t *)smalloc (sizeof (mchk_bucket_t));

	mcb->prevbucket = NULL;
	dynarray_init (mcb->items);

	return mcb;
}
/*}}}*/
/*{{{  static void mchk_freemchkbucket (mchk_bucket_t *mcb)*/
/*
 *	frees an mchk_bucket_t structure
 */
static void mchk_freemchkbucket (mchk_bucket_t *mcb)
{
	int i;

	if (!mcb) {
		nocc_internal ("mchk_freemchkbucket(): NULL pointer!\n");
		return;
	}
	for (i=0; i<DA_CUR (mcb->items); i++) {
		mchk_freemchknode (DA_NTHITEM (mcb->items, i));
	}
	dynarray_trash (mcb->items);

	sfree (mcb);
	return;
}
/*}}}*/
/*{{{  static mchk_state_t *mchk_newmchkstate (void)*/
/*
 *	creates a new mchk_state_t structure
 */
static mchk_state_t *mchk_newmchkstate (void)
{
	mchk_state_t *mcs = (mchk_state_t *)smalloc (sizeof (mchk_state_t));

	mcs->prevstate = NULL;
	mcs->inparams = 0;

	dynarray_init (mcs->ichans);
	dynarray_init (mcs->ivars);
	mcs->bucket = mchk_newmchkbucket ();

	mcs->err = 0;
	mcs->warn = 0;

	return mcs;
}
/*}}}*/
/*{{{  static void mchk_freemchkstate (mchk_state_t *mcs)*/
/*
 *	frees an mchk_state_t structure
 */
static void mchk_freemchkstate (mchk_state_t *mcs)
{
	int i;

	if (!mcs) {
		nocc_internal ("mchk_freemchkstate(): NULL pointer!");
		return;
	}

	for (i=0; i<DA_CUR (mcs->ichans); i++) {
		mchk_freemchknode (DA_NTHITEM (mcs->ichans, i));
	}
	dynarray_trash (mcs->ichans);

	for (i=0; i<DA_CUR (mcs->ivars); i++) {
		mchk_freemchknode (DA_NTHITEM (mcs->ivars, i));
	}
	dynarray_trash (mcs->ivars);

	if (mcs->bucket) {
		mchk_freemchkbucket (mcs->bucket);
		mcs->bucket = NULL;
	}

	sfree (mcs);
	return;
}
/*}}}*/

/*{{{  static void *mchk_traceschook_copy (void *hook)*/
/*
 *	duplicates a traces compiler hook
 */
static void *mchk_traceschook_copy (void *hook)
{
	mchk_traces_t *mct = (mchk_traces_t *)hook;

	if (mct) {
		mchk_traces_t *tcopy = mchk_newmchktraces ();
		int i;

		for (i=0; i<DA_CUR (mct->items); i++) {
			mchknode_t *item = DA_NTHITEM (mct->items, i);

			if (item) {
				mchknode_t *icopy = mobilitycheck_copynode (item);

				dynarray_add (tcopy->items, icopy);
			}
		}

		return (void *)tcopy;
	}
	return NULL;
}
/*}}}*/
/*{{{  static void mchk_traceschook_free (void *hook)*/
/*
 *	frees a traces compiler hook
 */
static void mchk_traceschook_free (void *hook)
{
	mchk_traces_t *mct = (mchk_traces_t *)hook;

	if (mct) {
		mchk_freemchktraces (mct);
	}
	return;
}
/*}}}*/
/*{{{  static void mchk_traceschook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	dumps a traces compiler hook (debugging)
 */
static void mchk_traceschook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	mchk_traces_t *mct = (mchk_traces_t *)hook;

	mchk_isetindent (stream, indent);
	if (!hook) {
		fprintf (stream, "<chook id=\"mobilitychecktraces\" value=\"\" />\n");
	} else {
		fprintf (stream, "<chook id=\"mobilitychecktraces\">\n");
		mobilitycheck_dumptraces (mct, indent + 1, stream);
		mchk_isetindent (stream, indent);
		fprintf (stream, "</chook>\n");
	}
	return;
}
/*}}}*/


/*{{{  int mobilitycheck_init (void)*/
/*
 *	initialises the mobility checker
 *	returns 0 on success, non-zero on failure
 */
int mobilitycheck_init (void)
{
	if (metadata_addreservedname ("mobility")) {
		nocc_error ("mobilitycheck_init(): failed to register reserved metadata name \"mobility\"");
		return -1;
	}

	mchk_traceschook = tnode_lookupornewchook ("mobilitychecktraces");
	mchk_traceschook->chook_copy = mchk_traceschook_copy;
	mchk_traceschook->chook_free = mchk_traceschook_free;
	mchk_traceschook->chook_dumptree = mchk_traceschook_dumptree;

	return 0;
}
/*}}}*/
/*{{{  int mobilitycheck_shutdown (void)*/
/*
 *	shuts-down the mobility checker
 *	returns 0 on success, non-zero on failure
 */
int mobilitycheck_shutdown (void)
{
	return 0;
}
/*}}}*/


/*{{{  static int mchk_prewalk_tree (tnode_t *node, void *data)*/
/*
 *	called to do mobility checks on a given node (prewalk order)
 *	returns 0 to stop walk, 1 to continue
 */
static int mchk_prewalk_tree (tnode_t *node, void *data)
{
	mchk_state_t *mcstate = (mchk_state_t *)data;
	int res = 1;

	if (!node) {
		nocc_internal ("mchk_prewalk_tree(): NULL node!");
		return 0;
	}
	if (!mcstate) {
		nocc_internal ("mchk_prewalk_tree(): NULL state!");
		return 0;
	}

	if (node->tag->ndef->ops && tnode_hascompop_i (node->tag->ndef->ops, (int)COPS_MOBILITYCHECK)) {
		res = tnode_callcompop_i (node->tag->ndef->ops, (int)COPS_MOBILITYCHECK, 2, node, mcstate);
	}

	return res;
}
/*}}}*/


/*{{{  int mobilitycheck_subtree (tnode_t *tree, mchk_state_t *mcstate)*/
/*
 *	does a mobility check on a sub-tree
 *	returns 0 on success, non-zero on failure
 */
int mobilitycheck_subtree (tnode_t *tree, mchk_state_t *mcstate)
{
	tnode_prewalktree (tree, mchk_prewalk_tree, (void *)mcstate);
	return 0;
}
/*}}}*/
/*{{{  int mobilitycheck_tree (tnode_t *tree, langparser_t *lang)*/
/*
 *	does a mobility check on a tree
 *	returns 0 on success, non-zero on failure
 */
int mobilitycheck_tree (tnode_t *tree, langparser_t *lang)
{
	mchk_state_t *mcstate = mchk_newmchkstate ();
	int res = 0;

	tnode_prewalktree (tree, mchk_prewalk_tree, (void *)mcstate);

	res = mcstate->err;

	mchk_freemchkstate (mcstate);

	return res;
}
/*}}}*/


/*{{{  mchknode_t *mobilitycheck_copynode (mchknode_t *mcn)*/
/*
 *	copies a mobility-check node
 */
mchknode_t *mobilitycheck_copynode (mchknode_t *mcn)
{
	mchknode_t *newmcn;
	int i;

	if (!mcn) {
		nocc_serious ("mobilitycheck_copynode(): NULL node!");
		return NULL;
	}

	newmcn = mchk_newmchknodet (mcn->type, mcn->orgnode);
	switch (mcn->type) {
	case MCN_INVALID:
		break;
	case MCN_INPUT:
	case MCN_OUTPUT:
		if (mcn->u.mcnio.chanptr) {
			newmcn->u.mcnio.chanptr = mobilitycheck_copynode (mcn->u.mcnio.chanptr);
		}
		if (mcn->u.mcnio.varptr) {
			newmcn->u.mcnio.varptr = mobilitycheck_copynode (mcn->u.mcnio.varptr);
		}
		break;
	case MCN_PARAM:
	case MCN_VAR:
		if (mcn->u.mcnpv.id) {
			newmcn->u.mcnpv.id = string_dup (mcn->u.mcnpv.id);
		}
		break;
	case MCN_PARAMREF:
	case MCN_VARREF:
		newmcn->u.mcnref.ref = mcn->u.mcnref.ref;
		break;
	case MCN_SEQ:
		for (i=0; i<DA_CUR (mcn->u.mcnlist.items); i++) {
			dynarray_add (newmcn->u.mcnlist.items, mobilitycheck_copynode (DA_NTHITEM (mcn->u.mcnlist.items, i)));
		}
		break;
	}

	return newmcn;
}
/*}}}*/


/*{{{  void mobilitycheck_dumpbucket (mchk_bucket_t *mcb, int indent, FILE *stream)*/
/*
 *	dumps a mobiltiy check bucket (debugging)
 */
void mobilitycheck_dumpbucket (mchk_bucket_t *mcb, int indent, FILE *stream)
{
	int i;

	mchk_isetindent (stream, indent);
	fprintf (stream, "<mobilitycheck:bucket prevbucket=\"0x%8.8x\" nitems=\"%d\">\n", (unsigned int)mcb->prevbucket, DA_CUR (mcb->items));
	for (i=0; i<DA_CUR (mcb->items); i++) {
		mobilitycheck_dumpnode (DA_NTHITEM (mcb->items, i), indent + 1, stream);
	}
	mchk_isetindent (stream, indent);
	fprintf (stream, "</mobilitycheck:bucket>\n");
	return;
}
/*}}}*/
/*{{{  void mobilitycheck_dumptraces (mchk_traces_t *mct, int indent, FILE *stream)*/
/*
 *	dumps a set of mobility traces (debugging)
 */
void mobilitycheck_dumptraces (mchk_traces_t *mct, int indent, FILE *stream)
{
	int i;

	mchk_isetindent (stream, indent);
	fprintf (stream, "<mobilitycheck:traces nitems=\"%d\" nchans=\"%d\" nvars=\"%d\">\n", DA_CUR (mct->items), DA_CUR (mct->params), DA_CUR (mct->vars));
	for (i=0; i<DA_CUR (mct->items); i++) {
		mobilitycheck_dumpnode (DA_NTHITEM (mct->items, i), indent + 1, stream);
	}
	for (i=0; i<DA_CUR (mct->params); i++) {
		mobilitycheck_dumpnode (DA_NTHITEM (mct->params, i), indent + 1, stream);
	}
	for (i=0; i<DA_CUR (mct->vars); i++) {
		mobilitycheck_dumpnode (DA_NTHITEM (mct->vars, i), indent + 1, stream);
	}
	mchk_isetindent (stream, indent);
	fprintf (stream, "</mobilitycheck:traces>\n");
	return;
}
/*}}}*/
/*{{{  void mobilitycheck_dumpstate (mchk_state_t *mcstate, int indent, FILE *stream)*/
/*
 *	dumps a mobility state (debugging)
 */
void mobilitycheck_dumpstate (mchk_state_t *mcstate, int indent, FILE *stream)
{
	int i;

	mchk_isetindent (stream, indent);
	fprintf (stream, "<mobilitycheck:state prevstate=\"0x%8.8x\" inparams=\"%d\" nichans=\"%d\" nivars=\"%d\" err=\"%d\" warn=\"%d\">\n",
			(unsigned int)mcstate->prevstate, mcstate->inparams, DA_CUR (mcstate->ichans), DA_CUR (mcstate->ivars), mcstate->err, mcstate->warn);
	for (i=0; i<DA_CUR (mcstate->ichans); i++) {
		mobilitycheck_dumpnode (DA_NTHITEM (mcstate->ichans, i), indent + 1, stream);
	}
	for (i=0; i<DA_CUR (mcstate->ivars); i++) {
		mobilitycheck_dumpnode (DA_NTHITEM (mcstate->ivars, i), indent + 1, stream);
	}
	mobilitycheck_dumpbucket (mcstate->bucket, indent + 1, stream);
	mchk_isetindent (stream, indent);
	fprintf (stream, "</mobilitycheck:state>\n");
}
/*}}}*/
/*{{{  void mobilitycheck_dumpnode (mchknode_t *mcn, int indent, FILE *stream)*/
/*
 *	dumps a mobility-analysis node (debugging)
 */
void mobilitycheck_dumpnode (mchknode_t *mcn, int indent, FILE *stream)
{
	mchk_isetindent (stream, indent);
	if (!mcn) {
		fprintf (stream, "<mobilitycheck:node value=\"null\" />\n");
	} else {
		char *mctnames[] = {"INVALID", "INPUT", "OUTPUT", "PARAM", "PARAMREF", "VAR", "VARREF", "SEQ"};
		int dotrail = 0;
		int i;

		fprintf (stream, "<mobilitycheck:node type=\"%d\" typename=\"%s\" orgnode=\"0x%8.8x\"", (int)mcn->type,
				((mcn->type >= MCN_INVALID) && ((int)mcn->type <= MCN_SEQ)) ? mctnames[(int)mcn->type] : "?", (unsigned int)mcn->orgnode);
		switch (mcn->type) {
		case MCN_INVALID:
			break;
		case MCN_INPUT:
		case MCN_OUTPUT:
			fprintf (stream, ">\n");
			mobilitycheck_dumpnode (mcn->u.mcnio.chanptr, indent + 1, stream);
			mobilitycheck_dumpnode (mcn->u.mcnio.varptr, indent + 1, stream);
			dotrail = 1;
			break;
		case MCN_PARAM:
		case MCN_VAR:
			fprintf (stream, " id=\"%s\"", mcn->u.mcnpv.id);
			break;
		case MCN_PARAMREF:
		case MCN_VARREF:
			fprintf (stream, ">\n");
			mobilitycheck_dumpnode (mcn->u.mcnref.ref, indent + 1, stream);
			dotrail = 1;
			break;
		case MCN_SEQ:
			fprintf (stream, " nitems=\"%d\">\n", DA_CUR (mcn->u.mcnlist.items));
			for (i=0; i<DA_CUR (mcn->u.mcnlist.items); i++) {
				mobilitycheck_dumpnode (DA_NTHITEM (mcn->u.mcnlist.items, i), indent + 1, stream);
			}
			dotrail = 1;
			break;
		}

		if (dotrail) {
			mchk_isetindent (stream, indent);
			fprintf (stream, "</mobilitycheck:node>\n");
		} else {
			fprintf (stream, " />\n");
		}
	}
	return;
}
/*}}}*/


/*{{{  mchk_state_t *mobilitycheck_pushstate (mchk_state_t *mcstate)*/
/*
 *	pushes mobility-check state stack, returns new state
 */
mchk_state_t *mobilitycheck_pushstate (mchk_state_t *mcstate)
{
	mchk_state_t *newstate;

	if (!mcstate) {
		nocc_internal ("mobilitycheck_pushstate(): NULL state!");
		return NULL;
	}

	newstate = mchk_newmchkstate ();
	newstate->prevstate = mcstate;

	return newstate;
}
/*}}}*/
/*{{{  mchk_state_t *mobilitycheck_popstate (mchk_state_t *mcstate)*/
/*
 *	pops mobility-check state stack, returns old state
 */
mchk_state_t *mobilitycheck_popstate (mchk_state_t *mcstate)
{
	mchk_state_t *oldstate;

	if (!mcstate) {
		nocc_internal ("mobilitycheck_popstate(): NULL state!");
		return NULL;
	}

	oldstate = mcstate->prevstate;
	if (!oldstate) {
		nocc_internal ("mobilitycheck_popstate(): NULL previous state!");
		return NULL;
	}

	mcstate->prevstate = NULL;
	mchk_freemchkstate (mcstate);

	return oldstate;
}
/*}}}*/


/*{{{  chook_t *mobilitycheck_gettraceschook (void)*/
/*
 *	returns compiler hook for attaching mobility traces (usually to procedure definitions)
 */
chook_t *mobilitycheck_gettraceschook (void)
{
	return mchk_traceschook;
}
/*}}}*/

