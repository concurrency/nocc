/*
 *	tracescheck.c -- traces checker for NOCC
 *	Copyright (C) 2007 Fred Barnes <frmb@kent.ac.uk>
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


/*}}}*/
/*{{{  private data*/

static int tchk_acounter;
static chook_t *tchk_noderefchook = NULL;


/*}}}*/


/*{{{  static void tchk_isetindent (FILE *stream, int indent)*/
/*
 *	set-indent for debugging output
 */
static void tchk_isetindent (FILE *stream, int indent)
{
	int i;

	for (i=0; i<indent; i++) {
		fprintf (stream, "    ");
	}
	return;
}
/*}}}*/
/*{{{  static void *tchk_noderefchook_copy (void *hook)*/
/*
 *	duplicates a "metadata" compiler hook
 */
static void *tchk_noderefchook_copy (void *hook)
{
	return hook;
}
/*}}}*/
/*{{{  static void tchk_noderefchook_free (void *hook)*/
/*
 *	frees a "metadata" compiler hook
 */
static void tchk_noderefchook_free (void *hook)
{
	/* nothing! */
	return;
}
/*}}}*/
/*{{{  static void tchk_noderefchook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	dumps a tchknode_t reference compiler hook (debugging)
 */
static void tchk_noderefchook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	tchknode_t *tcn = (tchknode_t *)hook;

	tchk_isetindent (stream, indent);
	if (!hook) {
		fprintf (stream, "<chook id=\"traceschecknoderef\" value=\"\" />\n");
	} else {
		fprintf (stream, "<chook id=\"traceschecknoderef\">\n");
		tracescheck_dumpnode (tcn, indent + 1, stream);
		tchk_isetindent (stream, indent);
		fprintf (stream, "</chook>\n");
	}
	return;
}
/*}}}*/


/*{{{  int tracescheck_init (void)*/
/*
 *	initialises the traces checker
 *	returns 0 on success, non-zero on failure
 */
int tracescheck_init (void)
{
	if (metadata_addreservedname ("traces")) {
		nocc_error ("tracescheck_init(): failed to register reserved metadata name \"traces\"");
		return -1;
	}

	tchk_acounter = 1;

	nocc_addxmlnamespace ("tracescheck", "http://www.cs.kent.ac.uk/projects/ofa/nocc/NAMESPACES/tracescheck");

	/*{{{  traces compiler-hooks*/
	tchk_noderefchook = tnode_lookupornewchook ("traceschecknoderef");
	tchk_noderefchook->chook_copy = tchk_noderefchook_copy;
	tchk_noderefchook->chook_free = tchk_noderefchook_free;
	tchk_noderefchook->chook_dumptree = tchk_noderefchook_dumptree;

	/*}}}*/

	return 0;
}
/*}}}*/
/*{{{  int tracescheck_shutdown (void)*/
/*
 *	shuts-down the traces checker
 *	returns 0 on success, non-zero on failure
 */
int tracescheck_shutdown (void)
{
	return 0;
}
/*}}}*/


/*{{{  static tchknode_t *tchk_newtchknode (void)*/
/*
 *	creates a blank tchknode_t structure
 */
static tchknode_t *tchk_newtchknode (void)
{
	tchknode_t *tcn = (tchknode_t *)smalloc (sizeof (tchknode_t));

	tcn->type = TCN_INVALID;

	return tcn;
}
/*}}}*/
/*{{{  static void tchk_freetchknode (tchknode_t *tcn)*/
/*
 *	frees a tchknode_t structure (deep)
 */
static void tchk_freetchknode (tchknode_t *tcn)
{
	if (!tcn) {
		nocc_internal ("tchk_freetchknode(): NULL node!");
		return;
	}
	switch (tcn->type) {
	case TCN_INVALID:
		break;
	case TCN_ATOMREF:
		tcn->u.tcnaref.aref = NULL;
		break;
	case TCN_NODEREF:
		tcn->u.tcnnref.nref = NULL;
		break;
	case TCN_SEQ:
	case TCN_PAR:
	case TCN_DET:
	case TCN_NDET:
		{
			int i;

			for (i=0; i<DA_CUR (tcn->u.tcnlist.items); i++) {
				tchknode_t *item = DA_NTHITEM (tcn->u.tcnlist.items, i);

				if (item) {
					tchk_freetchknode (item);
				}
			}
			dynarray_trash (tcn->u.tcnlist.items);
		}
		break;
	case TCN_ATOM:
		if (tcn->u.tcnatom.id) {
			sfree (tcn->u.tcnatom.id);
			tcn->u.tcnatom.id = NULL;
		}
		break;
	case TCN_INPUT:
	case TCN_OUTPUT:
		/* always a link into the tree */
		tcn->u.tcnio.varptr = NULL;
		break;
	case TCN_FIXPOINT:
		if (tcn->u.tcnfix.id) {
			tchk_freetchknode (tcn->u.tcnfix.id);
			tcn->u.tcnfix.id = NULL;
		}
		if (tcn->u.tcnfix.proc) {
			tchk_freetchknode (tcn->u.tcnfix.proc);
			tcn->u.tcnfix.proc = NULL;
		}
		break;
	}

	sfree (tcn);
	return;
}
/*}}}*/


/*{{{  static tchk_state_t *tchk_newtchkstate (void)*/
/*
 *	creates a new tchk_state_t structure (blank)
 */
static tchk_state_t *tchk_newtchkstate (void)
{
	tchk_state_t *tcstate = (tchk_state_t *)smalloc (sizeof (tchk_state_t));

	tcstate->prevstate = NULL;
	tcstate->inparams = 0;

	dynarray_init (tcstate->ivars);
	dynarray_init (tcstate->traces);
	dynarray_init (tcstate->bucket);

	tcstate->err = 0;
	tcstate->warn = 0;

	return tcstate;
}
/*}}}*/
/*{{{  static void tchk_freetchkstate (tchk_state_t *tcstate)*/
/*
 *	frees a tchk_state_t structure (deep)
 */
static void tchk_freetchkstate (tchk_state_t *tcstate)
{
	int i;

	if (!tcstate) {
		nocc_internal ("tchk_freetchkstate(): NULL state!");
		return;
	}

	if (tcstate->prevstate) {
		nocc_internal ("tchk_freetchkstate(): non-NULL previous state!");
		return;
	}

	for (i=0; i<DA_CUR (tcstate->ivars); i++) {
		tchknode_t *tcn = DA_NTHITEM (tcstate->ivars, i);

		if (tcn) {
			tchk_freetchknode (tcn);
		}
	}
	dynarray_trash (tcstate->ivars);

	for (i=0; i<DA_CUR (tcstate->traces); i++) {
		tchknode_t *tcn = DA_NTHITEM (tcstate->traces, i);

		if (tcn) {
			tchk_freetchknode (tcn);
		}
	}
	dynarray_trash (tcstate->traces);

	for (i=0; i<DA_CUR (tcstate->bucket); i++) {
		tchknode_t *tcn = DA_NTHITEM (tcstate->bucket, i);

		if (tcn) {
			tchk_freetchknode (tcn);
		}
	}
	dynarray_trash (tcstate->bucket);

	sfree (tcstate);
	return;
}
/*}}}*/


/*{{{  static int tchk_prewalk_tree (tnode_t *node, void *data)*/
/*
 *	called to do traces-checks on a given node (prewalk order)
 *	return 0 to stop walk, 1 to continue
 */
static int tchk_prewalk_tree (tnode_t *node, void *data)
{
	tchk_state_t *tcstate = (tchk_state_t *)data;
	int res = 1;

	if (!node) {
		nocc_internal ("tchk_prewalk_tree(): NULL tree!");
		return 0;
	}
	if (!tcstate) {
		nocc_internal ("tchk_prewalk_tree(): NULL state!");
		return 0;
	}

	if (node->tag->ndef->ops && tnode_hascompop_i (node->tag->ndef->ops, (int)COPS_TRACESCHECK)) {
		res = tnode_callcompop_i (node->tag->ndef->ops, (int)COPS_TRACESCHECK, 2, node, tcstate);
	}

	return res;
}
/*}}}*/
/*{{{  static int tchk_cleanrefchooks_prewalk (tnode_t *tptr, void *arg)*/
/*
 *	called in a pre-walk to clean up traceschecknoderef compiler hooks
 *	returns 0 to stop walk, 1 to continue
 */
static int tchk_cleanrefchooks_prewalk (tnode_t *tptr, void *arg)
{
	tchk_state_t *tcstate = (tchk_state_t *)arg;

	if (tnode_haschook (tptr, tchk_noderefchook)) {
		tchknode_t *tcn = (tchknode_t *)tnode_getchook (tptr, tchk_noderefchook);

		if (tcn) {
			tchk_freetchknode (tcn);
			tnode_clearchook (tptr, tchk_noderefchook);
		}
	}
	return 1;
}
/*}}}*/


/*{{{  int tracescheck_subtree (tnode_t *tree, tchk_state_t *tcstate)*/
/*
 *	does a traces check on a sub-tree
 *	returns 0 on success, non-zero on failure
 */
int tracescheck_subtree (tnode_t *tree, tchk_state_t *tcstate)
{
	tnode_prewalktree (tree, tchk_prewalk_tree, (void *)tcstate);
	return 0;
}
/*}}}*/
/*{{{  int tracescheck_tree (tnode_t *tree, langparser_t *lang)*/
/*
 *	does a traces check on a tree
 *	returns 0 on success, non-zero on failure
 */
int tracescheck_tree (tnode_t *tree, langparser_t *lang)
{
	tchk_state_t *tcstate = tchk_newtchkstate ();
	int res = 0;

	tnode_prewalktree (tree, tchk_prewalk_tree, (void *)tcstate);

	res = tcstate->err;

	tchk_freetchkstate (tcstate);

	return res;
}
/*}}}*/

/*{{{  void tracescheck_dumpstate (tchk_state_t *tcstate, int indent, FILE *stream)*/
/*
 *	dumps a traces check state (debugging)
 */
void tracescheck_dumpstate (tchk_state_t *tcstate, int indent, FILE *stream)
{
	int i;

	tchk_isetindent (stream, indent);
	fprintf (stream, "<tracescheck:state inparams=\"%d\" err=\"%d\" warn=\"%d\">\n", tcstate->inparams, tcstate->err, tcstate->warn);

	tchk_isetindent (stream, indent + 1);
	fprintf (stream, "<tracescheck:ivars>\n");
	for (i=0; i<DA_CUR (tcstate->ivars); i++) {
		tchknode_t *tcn = DA_NTHITEM (tcstate->ivars, i);

		tracescheck_dumpnode (tcn, indent + 2, stream);
	}
	tchk_isetindent (stream, indent + 1);
	fprintf (stream, "</tracescheck:ivars>\n");

	tchk_isetindent (stream, indent + 1);
	fprintf (stream, "<tracescheck:traces>\n");
	for (i=0; i<DA_CUR (tcstate->traces); i++) {
		tchknode_t *tcn = DA_NTHITEM (tcstate->traces, i);

		tracescheck_dumpnode (tcn, indent + 2, stream);
	}
	tchk_isetindent (stream, indent + 1);
	fprintf (stream, "</tracescheck:traces>\n");

	tchk_isetindent (stream, indent + 1);
	fprintf (stream, "<tracescheck:bucket>\n");
	for (i=0; i<DA_CUR (tcstate->bucket); i++) {
		tchknode_t *tcn = DA_NTHITEM (tcstate->bucket, i);

		tracescheck_dumpnode (tcn, indent + 2, stream);
	}
	tchk_isetindent (stream, indent + 1);
	fprintf (stream, "</tracescheck:bucket>\n");

	tchk_isetindent (stream, indent);
	fprintf (stream, "</tracescheck:state>\n");
	return;
}
/*}}}*/
/*{{{  void tracescheck_dumpnode (tchknode_t *tcn, int indent, FILE *stream)*/
/*
 *	dumps a traces check node (debugging)
 */
void tracescheck_dumpnode (tchknode_t *tcn, int indent, FILE *stream)
{
	tchk_isetindent (stream, indent);
	if (!tcn) {
		fprintf (stream, "<nullnode />\n");
	} else {
		switch (tcn->type) {
		case TCN_INVALID:
			fprintf (stream, "<tracescheck:node type=\"invalid\" />\n");
			break;
		case TCN_SEQ:
		case TCN_PAR:
		case TCN_DET:
		case TCN_NDET:
			fprintf (stream, "<tracescheck:node type=\"");
			switch (tcn->type) {
			case TCN_SEQ:	fprintf (stream, "seq\">\n");	break;
			case TCN_PAR:	fprintf (stream, "par\">\n");	break;
			case TCN_DET:	fprintf (stream, "det\">\n");	break;
			case TCN_NDET:	fprintf (stream, "ndet\">\n");	break;
			default:	break;
			}

			tchk_isetindent (stream, indent);
			fprintf (stream, "</tracescheck:node>\n");
			break;
		case TCN_INPUT:
		case TCN_OUTPUT:
			fprintf (stream, "<tracescheck:node type=\"%s\">\n", ((tcn->type == TCN_INPUT) ? "input" : "output"));
			tracescheck_dumpnode (tcn->u.tcnio.varptr, indent + 1, stream);
			tchk_isetindent (stream, indent);
			fprintf (stream, "</tracescheck:node>\n");
			break;
		case TCN_FIXPOINT:
			fprintf (stream, "<tracescheck:node type=\"fixpoint\">\n");
			tracescheck_dumpnode (tcn->u.tcnfix.id, indent + 1, stream);
			tracescheck_dumpnode (tcn->u.tcnfix.proc, indent + 1, stream);
			tchk_isetindent (stream, indent);
			fprintf (stream, "</tracescheck:node>\n");
			break;
		case TCN_ATOM:
			fprintf (stream, "<tracescheck:node type=\"atom\" id=\"%s\" />\n", tcn->u.tcnatom.id);
			break;
		case TCN_ATOMREF:
			{
				tchknode_t *aref = tcn->u.tcnaref.aref;

				if (aref && (aref->type == TCN_ATOM)) {
					fprintf (stream, "<tracescheck:node type=\"atomref\" id=\"%s\" />\n", tcn->u.tcnaref.aref->u.tcnatom.id);
				} else {
					fprintf (stream, "<tracescheck:node type=\"atomref\">\n");
					tracescheck_dumpnode (aref, indent + 1, stream);
					tchk_isetindent (stream, indent);
					fprintf (stream, "</tracescheck:node>\n");
				}
			}
			break;
		case TCN_NODEREF:
			{
				tnode_t *node = tcn->u.tcnnref.nref;

				fprintf (stream, "<tracescheck:node type=\"noderef\" addr=\"0x%8.8x\" nodetag=\"%s\" nodetype=\"%s\" />\n",
					(unsigned int)node, node->tag->name, node->tag->ndef->name);
			}
			break;
		}
	}
	return;
}
/*}}}*/

/*{{{  tchk_state_t *tracescheck_pushstate (tchk_state_t *tcstate)*/
/*
 *	pushes the traces-check state (needed for handling nested PROCs and similar)
 *	returns a new state
 */
tchk_state_t *tracescheck_pushstate (tchk_state_t *tcstate)
{
	tchk_state_t *newstate;
	
	if (!tcstate) {
		nocc_internal ("tracescheck_pushstate(): NULL state!");
		return NULL;
	}

	newstate = tchk_newtchkstate ();
	newstate->prevstate = tcstate;

	return newstate;
}
/*}}}*/
/*{{{  tchk_state_t *tracescheck_popstate (tchk_state_t *tcstate)*/
/*
 *	pops the traces-check state (needed for handling nested PROCs and similar)
 *	returns the previous state
 */
tchk_state_t *tracescheck_popstate (tchk_state_t *tcstate)
{
	tchk_state_t *oldstate;

	if (!tcstate) {
		nocc_internal ("tracescheck_popstate(): NULL state!");
		return NULL;
	}

	oldstate = tcstate->prevstate;
	if (!oldstate) {
		nocc_internal ("tracescheck_popstate(): NULL previous state!");
		return NULL;
	}

	tcstate->prevstate = NULL;
	tchk_freetchkstate (tcstate);

	return oldstate;
}
/*}}}*/

/*{{{  tchknode_t *tracescheck_dupref (tchknode_t *tcn)*/
/*
 *	duplicates a tchknode_t reference-type
 *	returns new node on success, NULL on failure
 */
tchknode_t *tracescheck_dupref (tchknode_t *tcn)
{
	tchknode_t *newtcn = NULL;

	switch (tcn->type) {
	case TCN_ATOMREF:
		newtcn = tracescheck_createnode (tcn->type, tcn->u.tcnaref.aref);
		break;
	case TCN_NODEREF:
		newtcn = tracescheck_createnode (tcn->type, tcn->u.tcnnref.nref);
		break;
	default:
		nocc_internal ("tracescheck_dupref(): cannot duplicate non-reference type %d", (int)tcn->type);
		break;
	}
	return newtcn;
}
/*}}}*/
/*{{{  tchknode_t *tracescheck_createatom (void)*/
/*
 *	creates a new traces-check atom (with a unique identifier)
 */
tchknode_t *tracescheck_createatom (void)
{
	tchknode_t *tcn = tchk_newtchknode ();

	tcn->type = TCN_ATOM;
	tcn->u.tcnatom.id = (char *)smalloc (32);
	sprintf (tcn->u.tcnatom.id, "A%4.4X", tchk_acounter);
	tchk_acounter++;

	return tcn;
}
/*}}}*/
/*{{{  tchknode_t *tracescheck_createnode (tchknodetype_e type, ...)*/
/*
 *	creates a new tchknode_t, populated
 *	returns node on success, NULL on failure
 */
tchknode_t *tracescheck_createnode (tchknodetype_e type, ...)
{
	tchknode_t *tcn = tchk_newtchknode ();
	va_list ap;

	va_start (ap, type);
	switch (type) {
	case TCN_INVALID:
		break;
	case TCN_SEQ:
	case TCN_PAR:
	case TCN_DET:
	case TCN_NDET:
		/* NULL-terminated list */
		{
			tchknode_t *arg = va_arg (ap, tchknode_t *);

			while (arg) {
				dynarray_add (tcn->u.tcnlist.items, arg);
				arg = va_arg (ap, tchknode_t *);
			}
		}
		break;
	case TCN_INPUT:
	case TCN_OUTPUT:
		tcn->u.tcnio.varptr = va_arg (ap, tchknode_t *);
		break;
	case TCN_FIXPOINT:
		tcn->u.tcnfix.id = va_arg (ap, tchknode_t *);
		tcn->u.tcnfix.proc = va_arg (ap, tchknode_t *);
		if (tcn->u.tcnfix.id && (tcn->u.tcnfix.id->type != TCN_ATOM)) {
			nocc_warning ("tracescheck_createnode(): TCN_FIXPOINT id is not of type TCN_ATOM (got %d)", (int)tcn->u.tcnfix.id->type);
		}
		break;
	case TCN_ATOM:
		tcn->u.tcnatom.id = string_dup (va_arg (ap, char *));
		break;
	case TCN_ATOMREF:
		tcn->u.tcnaref.aref = va_arg (ap, tchknode_t *);
		if (tcn->u.tcnaref.aref && (tcn->u.tcnaref.aref->type != TCN_ATOM)) {
			nocc_warning ("tracescheck_createnode(): TCN_ATOMREF reference is not of type TCN_ATOM (got %d)", (int)tcn->u.tcnaref.aref->type);
		}
		break;
	case TCN_NODEREF:
		tcn->u.tcnnref.nref = va_arg (ap, tnode_t *);
		break;
	}
	tcn->type = type;
	va_end (ap);

	return tcn;
}
/*}}}*/

/*{{{  int tracescheck_addivar (tchk_state_t *tcstate, tchknode_t *tcn)*/
/*
 *	adds a traces-check variable to the list of interesting/interface variables
 *	return 0 on success, non-zero on failure
 */
int tracescheck_addivar (tchk_state_t *tcstate, tchknode_t *tcn)
{
	if (!tcstate || !tcn) {
		nocc_internal ("tracescheck_addivar(): NULL state or node!");
		return -1;
	}
	dynarray_add (tcstate->ivars, tcn);
	return 0;
}
/*}}}*/
/*{{{  int tracescheck_addtobucket (tchk_state_t *tcstate, tchknode_t *tcn)*/
/*
 *	adds a traces-check something to the bucket
 *	returns 0 on success, non-zero on failure
 */
int tracescheck_addtobucket (tchk_state_t *tcstate, tchknode_t *tcn)
{
	if (!tcstate || !tcn) {
		nocc_internal ("tracescheck_addtobucket(): NULL state or node!");
		return -1;
	}
	dynarray_add (tcstate->bucket, tcn);
	return 0;
}
/*}}}*/
/*{{{  int tracescheck_cleanrefchooks (tchk_state_t *tcstate, tnode_t *tptr)*/
/*
 *	goes through a tree structure removing traceschecknoderef compiler hooks (typically used with parameter lists)
 *	returns 0 on success, non-zero on failure
 */
int tracescheck_cleanrefchooks (tchk_state_t *tcstate, tnode_t *tptr)
{
	tnode_prewalktree (tptr, tchk_cleanrefchooks_prewalk, (void *)tcstate);
	return 0;
}
/*}}}*/

/*{{{  chook_t *tracescheck_getnoderefchook (void)*/
/*
 *	returns the traceschecknoderef compiler hook
 */
chook_t *tracescheck_getnoderefchook (void)
{
	return tchk_noderefchook;
}
/*}}}*/

