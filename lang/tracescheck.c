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
#include "origin.h"
#include "symbols.h"
#include "keywords.h"
#include "opts.h"
#include "tnode.h"
#include "crypto.h"
#include "treeops.h"
#include "xml.h"
#include "lexer.h"
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
static chook_t *tchk_tracesrefchook = NULL;
static chook_t *tchk_traceschook = NULL;
static chook_t *tchk_tracesimplchook = NULL;
static chook_t *tchk_tracesbvarschook = NULL;


/*}}}*/
/*{{{  forward decls*/
static tchk_traces_t *tchk_newtchktraces (void);
static void tchk_freetchktraces (tchk_traces_t *tct);

/*}}}*/
/*{{{  private types*/

typedef struct TAG_prunetraces {
	tnode_t *vlist;
	int changed;
} prunetraces_t;

typedef struct TAG_firstevent {
	tchknode_t *item;
} firstevent_t;


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
 *	duplicates a tchknode-ref compiler hook
 */
static void *tchk_noderefchook_copy (void *hook)
{
	return hook;
}
/*}}}*/
/*{{{  static void tchk_noderefchook_free (void *hook)*/
/*
 *	frees a tchknode-ref compiler hook
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
/*{{{  static void *tchk_tracesrefchook_copy (void *hook)*/
/*
 *	duplicates a traces-ref compiler hook
 */
static void *tchk_tracesrefchook_copy (void *hook)
{
	return hook;
}
/*}}}*/
/*{{{  static void tchk_tracesrefchook_free (void *hook)*/
/*
 *	frees a traces-ref compiler hook
 */
static void tchk_tracesrefchook_free (void *hook)
{
	/* nothing! */
	return;
}
/*}}}*/
/*{{{  static void tchk_tracesrefchook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 * 	dumps a traces-ref compiler hook (debugging)
 */
static void tchk_tracesrefchook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	tchk_traces_t *tct = (tchk_traces_t *)hook;

	tchk_isetindent (stream, indent);
	if (!hook) {
		fprintf (stream, "<chook id=\"traceschecktracesref\" value=\"\" />\n");
	} else {
		fprintf (stream, "<chook id=\"traceschecktracesref\">\n");
		tracescheck_dumptraces (tct, indent + 1, stream);
		tchk_isetindent (stream, indent);
		fprintf (stream, "</chook>\n");
	}
	return;
}
/*}}}*/
/*{{{  static void *tchk_traceschook_copy (void *hook)*/
/*
 *	duplicates a traces compiler hook
 */
static void *tchk_traceschook_copy (void *hook)
{
	tchk_traces_t *tct = (tchk_traces_t *)hook;

	if (tct) {
		tchk_traces_t *tcopy = tchk_newtchktraces ();
		int i;

		for (i=0; i<DA_CUR (tct->items); i++) {
			tchknode_t *item = DA_NTHITEM (tct->items, i);

			if (item) {
				tchknode_t *icopy = tracescheck_copynode (item);

				dynarray_add (tcopy->items, icopy);
			}
		}

		return (void *)tcopy;
	}
	return NULL;
}
/*}}}*/
/*{{{  static void tchk_traceschook_free (void *hook)*/
/*
 *	frees a traces compiler hook
 */
static void tchk_traceschook_free (void *hook)
{
	tchk_traces_t *tct = (tchk_traces_t *)hook;

	if (tct) {
		tchk_freetchktraces (tct);
	}
	return;
}
/*}}}*/
/*{{{  static void tchk_traceschook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	dumps a traces compiler hook (debugging)
 */
static void tchk_traceschook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	tchk_traces_t *tct = (tchk_traces_t *)hook;

	tchk_isetindent (stream, indent);
	if (!hook) {
		fprintf (stream, "<chook id=\"traceschecktraces\" value=\"\" />\n");
	} else {
		fprintf (stream, "<chook id=\"traceschecktraces\">\n");
		tracescheck_dumptraces (tct, indent + 1, stream);
		tchk_isetindent (stream, indent);
		fprintf (stream, "</chook>\n");
	}
	return;
}
/*}}}*/
/*{{{  static void *tchk_tracesimplchook_copy (void *hook)*/
/*
 *	duplicates a tracescheckimpl compiler hook
 */
static void *tchk_tracesimplchook_copy (void *hook)
{
	tnode_t *copy = NULL;
	tnode_t *tr = (tnode_t *)hook;

	if (tr) {
		copy = tnode_copytree (tr);
	}

	return (void *)copy;
}
/*}}}*/
/*{{{  static void tchk_tracesimplchook_free (void *hook)*/
/*
 *	frees a tracescheckimpl compiler hook
 */
static void tchk_tracesimplchook_free (void *hook)
{
	tnode_t *tr = (tnode_t *)hook;

	if (tr) {
		tnode_free (tr);
	}
	return;
}
/*}}}*/
/*{{{  static void tchk_tracesimplchook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	dumps a tracescheckimpl compiler hook (debugging)
 */
static void tchk_tracesimplchook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	tnode_t *tr = (tnode_t *)hook;

	tchk_isetindent (stream, indent);
	if (!hook) {
		fprintf (stream, "<chook id=\"tracescheckimpl\" value=\"\" />\n");
	} else {
		fprintf (stream, "<chook id=\"tracescheckimpl\">\n");
		tnode_dumptree (tr, indent + 1, stream);
		tchk_isetindent (stream, indent);
		fprintf (stream, "</chook>\n");
	}
	return;
}
/*}}}*/
/*{{{  static void *tchk_tracesbvarschook_copy (void *hook)*/
/*
 *	duplicates a tracesbvars compiler hook
 */
static void *tchk_tracesbvarschook_copy (void *hook)
{
	tnode_t *list = (tnode_t *)hook;
	tnode_t *newlist = parser_newlistnode (NULL);
	int nitems, i;
	tnode_t **items = parser_getlistitems (list, &nitems);

	newlist->org_file = list->org_file;
	newlist->org_line = list->org_line;

	/* makes aliases */
	for (i=0; i<nitems; i++) {
		parser_addtolist (newlist, items[i]);
	}

	return (void *)newlist;
}
/*}}}*/
/*{{{  static void tchk_tracesbvarschook_free (void *hook)*/
/*
 *	frees a tracesbvars compiler hook
 */
static void tchk_tracesbvarschook_free (void *hook)
{
	tnode_t *list = (tnode_t *)hook;
	int nitems, i;
	tnode_t **items = parser_getlistitems (list, &nitems);

	for (i=0; i<nitems; i++) {
		/* drop references */
		items[i] = NULL;
	}

	tnode_free (list);
	return;
}
/*}}}*/
/*{{{  static void tchk_tracesbvarschook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	dumps a tracesbvars compiler hook (debugging)
 */
static void tchk_tracesbvarschook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	tnode_t *list = (tnode_t *)hook;

	tchk_isetindent (stream, indent);
	if (!list) {
		fprintf (stream, "<chook id=\"tracesbvars\" value=\"\" />\n");
	} else {
		fprintf (stream, "<chook id=\"tracesbvars\">\n");
		tnode_dumptree (list, indent + 1, stream);
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

	tchk_tracesrefchook = tnode_lookupornewchook ("traceschecktracesref");
	tchk_tracesrefchook->chook_copy = tchk_tracesrefchook_copy;
	tchk_tracesrefchook->chook_free = tchk_tracesrefchook_free;
	tchk_tracesrefchook->chook_dumptree = tchk_tracesrefchook_dumptree;

	tchk_traceschook = tnode_lookupornewchook ("traceschecktraces");
	tchk_traceschook->chook_copy = tchk_traceschook_copy;
	tchk_traceschook->chook_free = tchk_traceschook_free;
	tchk_traceschook->chook_dumptree = tchk_traceschook_dumptree;

	tchk_tracesimplchook = tnode_lookupornewchook ("tracescheckimpl");
	tchk_tracesimplchook->chook_copy = tchk_tracesimplchook_copy;
	tchk_tracesimplchook->chook_free = tchk_tracesimplchook_free;
	tchk_tracesimplchook->chook_dumptree = tchk_tracesimplchook_dumptree;

	tchk_tracesbvarschook = tnode_lookupornewchook ("tracesbvars");
	tchk_tracesbvarschook->chook_copy = tchk_tracesbvarschook_copy;
	tchk_tracesbvarschook->chook_free = tchk_tracesbvarschook_free;
	tchk_tracesbvarschook->chook_dumptree = tchk_tracesbvarschook_dumptree;

	/*}}}*/
	/*{{{  traces-check language operations*/

	tnode_newlangop ("tracescheck_check", LOPS_INVALID, 2, INTERNAL_ORIGIN);
	tnode_newlangop ("tracescheck_totrace", LOPS_INVALID, 2, INTERNAL_ORIGIN);

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


/*{{{  void tracescheck_warning (tnode_t *node, tchk_state_t *tc, const char *fmt, ...)*/
/*
 *	called by pre-scoper bits for warnings
 */
void tracescheck_warning (tnode_t *node, tchk_state_t *tc, const char *fmt, ...)
{
	va_list ap;
	int n;
	char *warnbuf = (char *)smalloc (512);
	lexfile_t *orgfile;

	if (!node) {
		orgfile = NULL;
	} else {
		orgfile = node->org_file;
	}

	va_start (ap, fmt);
	n = sprintf (warnbuf, "%s:%d (warning) ", orgfile ? orgfile->fnptr : "(unknown)", node->org_line);
	vsnprintf (warnbuf + n, 512 - n, fmt, ap);
	va_end (ap);

	if (orgfile) {
		orgfile->warncount++;
	}
	tc->warn++;
	nocc_message (warnbuf);
	sfree (warnbuf);

	return;
}
/*}}}*/
/*{{{  void tracescheck_error (tnode_t *node, tchk_state_t *tc, const char *fmt, ...)*/
/*
 *	called by the pre-scoper bits for errors
 */
void tracescheck_error (tnode_t *node, tchk_state_t *tc, const char *fmt, ...)
{
	va_list ap;
	int n;
	char *warnbuf = (char *)smalloc (512);
	lexfile_t *orgfile;

	if (!node) {
		orgfile = NULL;
	} else {
		orgfile = node->org_file;
	}

	va_start (ap, fmt);
	n = sprintf (warnbuf, "%s:%d (error) ", orgfile ? orgfile->fnptr : "(unknown)", node->org_line);
	vsnprintf (warnbuf + n, 512 - n, fmt, ap);
	va_end (ap);

	if (orgfile) {
		orgfile->errcount++;
	}
	tc->err++;
	nocc_message (warnbuf);
	sfree (warnbuf);

	return;
}
/*}}}*/

/*{{{  void tracescheck_checkwarning (tnode_t *node, tchk_check_t *tcc, const char *fmt, ...)*/
/*
 *	called by pre-scoper bits for warnings
 */
void tracescheck_checkwarning (tnode_t *node, tchk_check_t *tcc, const char *fmt, ...)
{
	va_list ap;
	int n;
	char *warnbuf = (char *)smalloc (512);
	lexfile_t *orgfile;

	if (!node) {
		orgfile = NULL;
	} else {
		orgfile = node->org_file;
	}

	va_start (ap, fmt);
	n = sprintf (warnbuf, "%s:%d (warning) ", orgfile ? orgfile->fnptr : "(unknown)", node->org_line);
	vsnprintf (warnbuf + n, 512 - n, fmt, ap);
	va_end (ap);

	if (orgfile) {
		orgfile->warncount++;
	}
	tcc->warn++;
	nocc_message (warnbuf);
	sfree (warnbuf);

	return;
}
/*}}}*/
/*{{{  void tracescheck_checkerror (tnode_t *node, tchk_check_t *tcc, const char *fmt, ...)*/
/*
 *	called by the pre-scoper bits for errors
 */
void tracescheck_checkerror (tnode_t *node, tchk_check_t *tcc, const char *fmt, ...)
{
	va_list ap;
	int n;
	char *warnbuf = (char *)smalloc (512);
	lexfile_t *orgfile;

	if (!node) {
		orgfile = NULL;
	} else {
		orgfile = node->org_file;
	}

	va_start (ap, fmt);
	n = sprintf (warnbuf, "%s:%d (error) ", orgfile ? orgfile->fnptr : "(unknown)", node->org_line);
	vsnprintf (warnbuf + n, 512 - n, fmt, ap);
	va_end (ap);

	if (orgfile) {
		orgfile->errcount++;
	}
	tcc->err++;
	nocc_message (warnbuf);
	sfree (warnbuf);

	return;
}
/*}}}*/


/*{{{  static tchknode_t *tchk_newtchknode (tnode_t *orgnode)*/
/*
 *	creates a blank tchknode_t structure
 */
static tchknode_t *tchk_newtchknode (tnode_t *orgnode)
{
	tchknode_t *tcn = (tchknode_t *)smalloc (sizeof (tchknode_t));

	tcn->type = TCN_INVALID;
	tcn->orgnode = orgnode;
	tcn->mark = 0;

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
	case TCN_SKIP:
	case TCN_STOP:
	case TCN_DIV:
	case TCN_CHAOS:
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
/*{{{  static tchk_bucket_t *tchk_newtchkbucket (void)*/
/*
 *	creates a new tchk_bucket_t structure (blank)
 */
static tchk_bucket_t *tchk_newtchkbucket (void)
{
	tchk_bucket_t *tcb = (tchk_bucket_t *)smalloc (sizeof (tchk_bucket_t));

	tcb->prevbucket = NULL;
	dynarray_init (tcb->items);

	return tcb;
}
/*}}}*/
/*{{{  static void tchk_freetchkbucket (tchk_bucket_t *tcb)*/
/*
 *	frees a tchk_bucket_t structure (deep)
 */
static void tchk_freetchkbucket (tchk_bucket_t *tcb)
{
	int i;

	if (!tcb) {
		nocc_internal ("tchk_freetchkbucket(): NULL bucket!");
		return;
	}
	if (tcb->prevbucket) {
		nocc_serious ("tchk_freetchkbucket(): have a previous bucket, freeing that too..");
		tchk_freetchkbucket (tcb->prevbucket);
		tcb->prevbucket = NULL;
	}

	for (i=0; i<DA_CUR (tcb->items); i++) {
		tchknode_t *tcn = DA_NTHITEM (tcb->items, i);

		if (tcn) {
			tchk_freetchknode (tcn);
		}
	}
	dynarray_trash (tcb->items);

	sfree (tcb);
	return;
}
/*}}}*/
/*{{{  static tchk_traces_t *tchk_newtchktraces (void)*/
/*
 *	creates a new tchk_traces_t structure (blank)
 */
static tchk_traces_t *tchk_newtchktraces (void)
{
	tchk_traces_t *tct = (tchk_traces_t *)smalloc (sizeof (tchk_traces_t));

	dynarray_init (tct->items);

	return tct;
}
/*}}}*/
/*{{{  static void tchk_freetchktraces (tchk_traces_t *tct)*/
/*
 *	frees a tchk_traces_t structure (deep)
 */
static void tchk_freetchktraces (tchk_traces_t *tct)
{
	int i;

	if (!tct) {
		nocc_internal ("tchk_freetchktraces(): NULL traces!");
		return;
	}

	for (i=0; i<DA_CUR (tct->items); i++) {
		tchknode_t *tcn = DA_NTHITEM (tct->items, i);

		if (tcn) {
			tchk_freetchknode (tcn);
		}
	}
	dynarray_trash (tct->items);

	sfree (tct);
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
	tcstate->traces = tchk_newtchktraces ();
	tcstate->bucket = tchk_newtchkbucket ();

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

	if (tcstate->traces) {
		tchk_freetchktraces (tcstate->traces);
		tcstate->traces = NULL;
	}
	if (tcstate->bucket) {
		tchk_freetchkbucket (tcstate->bucket);
		tcstate->bucket = NULL;
	}

	sfree (tcstate);
	return;
}
/*}}}*/
/*{{{  static tchk_tracewalk_t *tchk_newtchktracewalk (void)*/
/*
 *	creates a new, blank, tchk_tracewalk_t structure
 */
static tchk_tracewalk_t *tchk_newtchktracewalk (void)
{
	tchk_tracewalk_t *ttw = (tchk_tracewalk_t *)smalloc (sizeof (tchk_tracewalk_t));

	ttw->thisnode = NULL;
	dynarray_init (ttw->stack);
	dynarray_init (ttw->data);
	ttw->depth = 0;
	ttw->end = 0;

	return ttw;
}
/*}}}*/
/*{{{  static void tchk_freetchktracewalk (tchk_tracewalk_t *ttw)*/
/*
 *	frees a tchk_tracewalk_t structure
 */
static void tchk_freetchktracewalk (tchk_tracewalk_t *ttw)
{
	if (!ttw) {
		nocc_internal ("tchk_freetchktracewalk(): NULL trace-walk!");
		return;
	}
	dynarray_trash (ttw->stack);
	dynarray_trash (ttw->data);
	ttw->depth = 0;
	ttw->thisnode = NULL;
	ttw->end = 1;

	sfree (ttw);
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
	// tchk_state_t *tcstate = (tchk_state_t *)arg;

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
/*{{{  static int tchk_substatomrefsinnode (tchknode_t *node, tchknode_t *aold, tchknode_t *anew)*/
/*
 *	substitutes references to one TCN_ATOM for another, needed after copying these
 *	returns 0 on success, non-zero on failure
 */
static int tchk_substatomrefsinnode (tchknode_t *node, tchknode_t *aold, tchknode_t *anew)
{
	int r = 0;

	if (!node || !aold || !anew) {
		nocc_serious ("tchk_substatomrefsinnode(): NULL node or atoms");
		return -1;
	}

	switch (node->type) {
		/*{{{  INVALID,ATOM,NODEREF,SKIP,STOP,DIV,CHAOS*/
	case TCN_INVALID:
	case TCN_ATOM:
	case TCN_NODEREF:
	case TCN_SKIP:
	case TCN_STOP:
	case TCN_DIV:
	case TCN_CHAOS:
		break;
		/*}}}*/
		/*{{{  SEQ,PAR,DET,NDET*/
	case TCN_SEQ:
	case TCN_PAR:
	case TCN_DET:
	case TCN_NDET:
		{
			int i;

			for (i=0; i<DA_CUR (node->u.tcnlist.items); i++) {
				if (tchk_substatomrefsinnode (DA_NTHITEM (node->u.tcnlist.items, i), aold, anew)) {
					r++;
				}
			}
		}
		break;
		/*}}}*/
		/*{{{  INPUT,OUTPUT*/
	case TCN_INPUT:
	case TCN_OUTPUT:
		if (tchk_substatomrefsinnode (node->u.tcnio.varptr, aold, anew)) {
			r++;
		}
		break;
		/*}}}*/
		/*{{{  FIXPOINT*/
	case TCN_FIXPOINT:
		if (tchk_substatomrefsinnode (node->u.tcnfix.proc, aold, anew)) {
			r++;
		}
		break;
		/*}}}*/
		/*{{{  ATOMREF*/
	case TCN_ATOMREF:
		if (node->u.tcnaref.aref == aold) {
			node->u.tcnaref.aref = anew;
		}
		break;
		/*}}}*/
	}

	return r;
}
/*}}}*/
/*{{{  static int tchk_simplifynodeprewalk (tchknode_t **tcnptr, void *arg)*/
/*
 *	called to simplify a node (in a modprewalk)
 *	returns 0 to stop walk, 1 to continue
 */
static int tchk_simplifynodeprewalk (tchknode_t **tcnptr, void *arg)
{
	tchknode_t *tcn;

	if (!tcnptr || !*tcnptr) {
		nocc_serious ("tchk_simplifynodeprewalk(): NULL pointer or node!");
		return 0;
	}
	tcn = *tcnptr;
	switch (tcn->type) {
	default:
		break;
	case TCN_SEQ:
	case TCN_PAR:
	case TCN_DET:
	case TCN_NDET:
		if (DA_CUR (tcn->u.tcnlist.items) == 1) {
			tchknode_t *item = DA_NTHITEM (tcn->u.tcnlist.items, 0);

			*tcnptr = item;
			dynarray_trash (tcn->u.tcnlist.items);
			tchk_freetchknode (tcn);
		}
		break;
	}
	return 1;
}
/*}}}*/
/*{{{  static int tchk_prunetracesmodprewalk (tchknode_t **nodep, void *arg)*/
/*
 *	does a mod pre-walk to prune traces, essentially the CSP rules for hiding
 *	NOTE: this works in reverse -- i.e. we have a list of things we want to keep,
 *	      rather than a list of those we want to throw away.
 *	returns 0 to stop walk, 1 to continue
 */
static int tchk_prunetracesmodprewalk (tchknode_t **nodep, void *arg)
{
	prunetraces_t *ptrace = (prunetraces_t *)arg;
	tchknode_t *n = *nodep;
	int i, changed;

	switch (n->type) {
		/*{{{  INVALID,ATOM,ATOMREF,SKIP,STOP,DIV,CHAOS*/
	case TCN_INVALID:
	case TCN_ATOM:
	case TCN_ATOMREF:
	case TCN_SKIP:
	case TCN_STOP:
	case TCN_DIV:
	case TCN_CHAOS:
		return 0;
		/*}}}*/
		/*{{{  FIXPOINT -- walk down, but check for fixpoints..!*/
	case TCN_FIXPOINT:
		changed = 0;
		{
			tchknode_t **iptr = &(n->u.tcnfix.proc);
			int saved_changed = ptrace->changed;

			ptrace->changed = 0;
			tracescheck_modprewalk (iptr, tchk_prunetracesmodprewalk, (void *)ptrace);

			changed += ptrace->changed;
			ptrace->changed = saved_changed;
		}
		if (changed) {
			/* inspect what's left for obvious cases */
			if (!n->u.tcnfix.proc) {
				/* process reduced to absolutely nothing -- we're toast too then */
				*nodep = NULL;
				tchk_freetchknode (n);
				ptrace->changed++;
			} else {
				tchknode_t *ptcn = n->u.tcnfix.proc;

				switch (ptcn->type) {
				default:
					break;
				case TCN_SEQ:
					/* if we've got (Skip ; <fixpoint>), or just (<fixpoint>) reduce to divergence */
					if (DA_CUR (ptcn->u.tcnlist.items) == 1) {
						tchknode_t *item = DA_NTHITEM (ptcn->u.tcnlist.items, 0);

						if ((item->type == TCN_ATOMREF) && (item->u.tcnaref.aref == n->u.tcnfix.id)) {
							/* yes, this is us */
							*nodep = tracescheck_createnode (TCN_DIV, n->orgnode);
							tchk_freetchknode (n);
							ptrace->changed++;
						}
					} else if (DA_CUR (ptcn->u.tcnlist.items) == 2) {
						tchknode_t *item1 = DA_NTHITEM (ptcn->u.tcnlist.items, 0);
						tchknode_t *item2 = DA_NTHITEM (ptcn->u.tcnlist.items, 1);

						if ((item1->type == TCN_SKIP) && (item2->type == TCN_ATOMREF) && (item2->u.tcnaref.aref == n->u.tcnfix.id)) {
							/* yes, this is us */
							*nodep = tracescheck_createnode (TCN_DIV, n->orgnode);
							tchk_freetchknode (n);
							ptrace->changed++;
						}
					}
					break;
				case TCN_INVALID:
				case TCN_SKIP:
				case TCN_STOP:
				case TCN_DIV:
				case TCN_CHAOS:
				case TCN_NODEREF:
					/* singleton things, which cannot reference this fixpoint, reduce entirely */
					n->u.tcnfix.proc = NULL;
					*nodep = ptcn;
					tchk_freetchknode (n);
					ptrace->changed++;
					break;
				}
			}
		}
		break;
		/*}}}*/
		/*{{{  DET -- maybe becomes nondeterministic*/
	case TCN_DET:
		/* FIXME! */
		break;
		/*}}}*/
		/*{{{  SEQ, PAR -- simply walk down subnodes*/
	case TCN_SEQ:
	case TCN_PAR:
		changed = 0;
		for (i=0; i<DA_CUR (n->u.tcnlist.items); i++) {
			tchknode_t **iptr = DA_NTHITEMADDR (n->u.tcnlist.items, i);
			int saved_changed = ptrace->changed;

			ptrace->changed = 0;
			tracescheck_modprewalk (iptr, tchk_prunetracesmodprewalk, (void *)ptrace);

			changed += ptrace->changed;
			ptrace->changed = saved_changed;
		}
		if (changed) {
			/* might have some NULL items in the list */
			for (i=0; i<DA_CUR (n->u.tcnlist.items); i++) {
				if (!DA_NTHITEM (n->u.tcnlist.items, i)) {
					dynarray_delitem (n->u.tcnlist.items, i);
					i--;
				}
			}
			if (!DA_CUR (n->u.tcnlist.items)) {
				/* we're dead too */
				*nodep = NULL;
				tchk_freetchknode (n);
			} else if (DA_CUR (n->u.tcnlist.items) == 1) {
				/* singleton, reduce */
				tchknode_t *sub = DA_NTHITEM (n->u.tcnlist.items, 0);

				DA_SETNTHITEM (n->u.tcnlist.items, 0, NULL);
				*nodep = sub;
				tchk_freetchknode (n);
			}
			ptrace->changed++;
		}
		return 0;
		/*}}}*/
		/*{{{  NODEREF -- maybe something we're looking for*/
	case TCN_NODEREF:
		{
			int nvitems;
			tnode_t **vitems = parser_getlistitems (ptrace->vlist, &nvitems);

			for (i=0; (i<nvitems) && (n->u.tcnnref.nref != vitems[i]); i++);
			if (i == nvitems) {
				/* this noderef wasn't in the list -- remove it */
				*nodep = NULL;
				tchk_freetchknode (n);
				ptrace->changed++;
			}
		}
		return 0;
		/*}}}*/
		/*{{{  INPUT, OUTPUT -- simply walk down subitems*/
	case TCN_INPUT:
	case TCN_OUTPUT:
		changed = 0;
		{
			tchknode_t **iptr = &(n->u.tcnio.varptr);
			int saved_changed = ptrace->changed;

			ptrace->changed = 0;
			tracescheck_modprewalk (iptr, tchk_prunetracesmodprewalk, (void *)ptrace);

			changed += ptrace->changed;
			ptrace->changed = saved_changed;
		}
		if (changed) {
			if (!n->u.tcnio.varptr) {
				/* we're toast */
				*nodep = NULL;
				tchk_freetchknode (n);
			}
			ptrace->changed++;
		}
		return 0;
		/*}}}*/
	}
	return 1;
}
/*}}}*/
/*{{{  static int tchk_firsteventprewalk (tchknode_t *node, void *arg)*/
/*
 *	does a pre-walk to find the first event in a trace
 *	returns 0 to stop walk, 1 to continue
 */
static int tchk_firsteventprewalk (tchknode_t *node, void *arg)
{
	firstevent_t *fev = (firstevent_t *)arg;

	if (fev->item) {
		return 0;
	}
	switch (node->type) {
		/*{{{  NODEREF -- event*/
	case TCN_NODEREF:
		fev->item = node;
		return 0;
		/*}}}*/
	default:
		break;
	}
	return 1;
}
/*}}}*/

/*{{{  static int tchk_walkpopstack (tchk_tracewalk_t *ttw)*/
/*
 *	called to pop the tchk_traceswalk_t stack
 *	returns 0 on success, non-zero on failure
 */
static int tchk_walkpopstack (tchk_tracewalk_t *ttw)
{
	if (!ttw->depth) {
		nocc_serious ("tchk_walkpopstack(): ttw->depth is zero on stack pop attempt!");
		return -1;
	}
	ttw->depth--;
	dynarray_delitem (ttw->stack, ttw->depth);
	dynarray_delitem (ttw->data, ttw->depth);
	if (!ttw->depth) {
		ttw->end = 1;
	}
	return 0;
}
/*}}}*/
/*{{{  static int tchk_walkpushstack (tchk_tracewalk_t *ttw, tchknode_t *tcn, int data)*/
/*
 *	called to push something onto the tchk_traceswalk_t stack
 *	returns 0 on success, non-zero on failure
 */
static int tchk_walkpushstack (tchk_tracewalk_t *ttw, tchknode_t *tcn, int data)
{
	if (ttw->end) {
		nocc_serious ("tchk_walkpushstack(): at end of walk!");
		return -1;
	}

	dynarray_add (ttw->stack, tcn);
	dynarray_add (ttw->data, data);
	ttw->depth++;
	ttw->thisnode = tcn;

	return 0;
}
/*}}}*/
/*{{{  static int tchk_walknextstep (tchk_tracewalk_t *ttw)*/
/*
 *	called to move the state in a traceswalk to the next item
 *	returns 0 on success, non-zero on failure
 */
static int tchk_walknextstep (tchk_tracewalk_t *ttw)
{
	int atnext = 0;

	while (!atnext) {
		tchk_walkpopstack (ttw);
		if (!ttw->end) {
			tchknode_t *nextnode = DA_NTHITEM (ttw->stack, ttw->depth - 1);
			int *nextdatap = DA_NTHITEMADDR (ttw->data, ttw->depth - 1);

			switch (nextnode->type) {
			case TCN_SEQ:
			case TCN_PAR:
			case TCN_DET:
			case TCN_NDET:
				if (*nextdatap < DA_CUR (nextnode->u.tcnlist.items)) {
					/* got some left here */
					atnext = 1;
				}
				break;
			case TCN_INPUT:
			case TCN_OUTPUT:
				/* nothing left here */
				break;
			default:
				nocc_serious ("tchk_walknextstep(): unexpected node type %d in walk back!", (int)nextnode->type);
				atnext = 1;
				break;
			}
		} else {
			atnext = 1;
		}
	}
	return 0;
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

/*{{{  int tracescheck_modprewalk (tchknode_t **tcnptr, int (*func)(tchknode_t **, void *), void *arg)*/
/*
 *	does a walk over nodes in traces
 *	returns 0 on success, non-zero on failure
 */
int tracescheck_modprewalk (tchknode_t **tcnptr, int (*func)(tchknode_t **, void *), void *arg)
{
	int i, r = 0;

	if (!func) {
		return -1;
	}
	if (!tcnptr || !*tcnptr) {
		return 0;
	}
	i = func (tcnptr, arg);
	if (i) {
		switch ((*tcnptr)->type) {
			/*{{{  INVALID,ATOM,ATOMREF,NODEREF,SKIP,STOP,DIV,CHAOS*/
		case TCN_INVALID:
		case TCN_ATOM:
		case TCN_ATOMREF:
		case TCN_NODEREF:
		case TCN_SKIP:
		case TCN_STOP:
		case TCN_DIV:
		case TCN_CHAOS:
			break;
			/*}}}*/
			/*{{{  SEQ,PAR,DET,NDET*/
		case TCN_SEQ:
		case TCN_PAR:
		case TCN_DET:
		case TCN_NDET:
			for (i=0; i<DA_CUR ((*tcnptr)->u.tcnlist.items); i++) {
				tchknode_t **iptr = DA_NTHITEMADDR ((*tcnptr)->u.tcnlist.items, i);

				if (tracescheck_modprewalk (iptr, func, arg)) {
					r++;
				}
			}
			break;
			/*}}}*/
			/*{{{  FIXPOINT*/
		case TCN_FIXPOINT:
			if (tracescheck_modprewalk (&((*tcnptr)->u.tcnfix.id), func, arg)) {
				r++;
			}
			if (tracescheck_modprewalk (&((*tcnptr)->u.tcnfix.proc), func, arg)) {
				r++;
			}
			break;
			/*}}}*/
			/*{{{  INPUT,OUTPUT*/
		case TCN_INPUT:
		case TCN_OUTPUT:
			if (tracescheck_modprewalk (&((*tcnptr)->u.tcnio.varptr), func, arg)) {
				r++;
			}
			break;
			/*}}}*/
		}
	}
	return r;
}
/*}}}*/
/*{{{  int tracescheck_prewalk (tchknode_t *tcn, int (*func)(tchknode_t *, void *), void *arg)*/
/*
 *	does a walk over nodes in traces
 *	returns 0 on success, non-zero on failure
 */
int tracescheck_prewalk (tchknode_t *tcn, int (*func)(tchknode_t *, void *), void *arg)
{
	int i, r = 0;

	if (!func) {
		return -1;
	}
	if (!tcn || !tcn) {
		return 0;
	}
	i = func (tcn, arg);
	if (i) {
		switch (tcn->type) {
			/*{{{  INVALID,ATOM,ATOMREF,NODEREF,SKIP,STOP,DIV,CHAOS*/
		case TCN_INVALID:
		case TCN_ATOM:
		case TCN_ATOMREF:
		case TCN_NODEREF:
		case TCN_SKIP:
		case TCN_STOP:
		case TCN_DIV:
		case TCN_CHAOS:
			break;
			/*}}}*/
			/*{{{  SEQ,PAR,DET,NDET*/
		case TCN_SEQ:
		case TCN_PAR:
		case TCN_DET:
		case TCN_NDET:
			for (i=0; i<DA_CUR (tcn->u.tcnlist.items); i++) {
				tchknode_t *item = DA_NTHITEM (tcn->u.tcnlist.items, i);

				if (tracescheck_prewalk (item, func, arg)) {
					r++;
				}
			}
			break;
			/*}}}*/
			/*{{{  FIXPOINT*/
		case TCN_FIXPOINT:
			if (tracescheck_prewalk (tcn->u.tcnfix.id, func, arg)) {
				r++;
			}
			if (tracescheck_prewalk (tcn->u.tcnfix.proc, func, arg)) {
				r++;
			}
			break;
			/*}}}*/
			/*{{{  INPUT,OUTPUT*/
		case TCN_INPUT:
		case TCN_OUTPUT:
			if (tracescheck_prewalk (tcn->u.tcnio.varptr, func, arg)) {
				r++;
			}
			break;
			/*}}}*/
		}
	}
	return r;
}
/*}}}*/

/*{{{  void tracescheck_dumpbucket (tchk_bucket_t *tcb, int indent, FILE *stream)*/
/*
 *	dumps a traces check bucket (debugging)
 */
void tracescheck_dumpbucket (tchk_bucket_t *tcb, int indent, FILE *stream)
{
	int i;

	tchk_isetindent (stream, indent);
	fprintf (stream, "<tracescheck:bucket>\n");
	for (i=0; i<DA_CUR (tcb->items); i++) {
		tchknode_t *tcn = DA_NTHITEM (tcb->items, i);

		tracescheck_dumpnode (tcn, indent + 1, stream);
	}
	tchk_isetindent (stream, indent);
	fprintf (stream, "</tracescheck:bucket>\n");
	return;
}
/*}}}*/
/*{{{  void tracescheck_dumptraces (tchk_traces_t *tct, int indent, FILE *stream)*/
/*
 *	dumps a traces-check trace set (debugging)
 */
void tracescheck_dumptraces (tchk_traces_t *tct, int indent, FILE *stream)
{
	int i;

	tchk_isetindent (stream, indent);
	fprintf (stream, "<tracescheck:traces>\n");
	for (i=0; i<DA_CUR (tct->items); i++) {
		tchknode_t *tcn = DA_NTHITEM (tct->items, i);

		tracescheck_dumpnode (tcn, indent + 1, stream);
	}
	tchk_isetindent (stream, indent);
	fprintf (stream, "</tracescheck:traces>\n");
	return;
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

	tracescheck_dumptraces (tcstate->traces, indent + 1, stream);
	tracescheck_dumpbucket (tcstate->bucket, indent + 1, stream);

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
			/*{{{  INVALID*/
		case TCN_INVALID:
			fprintf (stream, "<tracescheck:node orgnode=\"0x%8.8x\" type=\"invalid\" />\n", (unsigned int)tcn->orgnode);
			break;
			/*}}}*/
			/*{{{  SEQ,PAR,DET,NDET*/
		case TCN_SEQ:
		case TCN_PAR:
		case TCN_DET:
		case TCN_NDET:
			{
				int i;

				fprintf (stream, "<tracescheck:node orgnode=\"0x%8.8x\" type=\"", (unsigned int)tcn->orgnode);
				switch (tcn->type) {
				case TCN_SEQ:	fprintf (stream, "seq\">\n");	break;
				case TCN_PAR:	fprintf (stream, "par\">\n");	break;
				case TCN_DET:	fprintf (stream, "det\">\n");	break;
				case TCN_NDET:	fprintf (stream, "ndet\">\n");	break;
				default:	break;
				}

				for (i=0; i<DA_CUR (tcn->u.tcnlist.items); i++) {
					tchknode_t *item = DA_NTHITEM (tcn->u.tcnlist.items, i);

					tracescheck_dumpnode (item, indent + 1, stream);
				}

				tchk_isetindent (stream, indent);
				fprintf (stream, "</tracescheck:node>\n");
			}
			break;
			/*}}}*/
			/*{{{  INPUT,OUTPUT*/
		case TCN_INPUT:
		case TCN_OUTPUT:
			fprintf (stream, "<tracescheck:node orgnode=\"0x%8.8x\" type=\"%s\">\n",
					(unsigned int)tcn->orgnode, ((tcn->type == TCN_INPUT) ? "input" : "output"));
			tracescheck_dumpnode (tcn->u.tcnio.varptr, indent + 1, stream);
			tchk_isetindent (stream, indent);
			fprintf (stream, "</tracescheck:node>\n");
			break;
			/*}}}*/
			/*{{{  FIXPOINT*/
		case TCN_FIXPOINT:
			fprintf (stream, "<tracescheck:node orgnode=\"0x%8.8x\" type=\"fixpoint\">\n",
					(unsigned int)tcn->orgnode);
			tracescheck_dumpnode (tcn->u.tcnfix.id, indent + 1, stream);
			tracescheck_dumpnode (tcn->u.tcnfix.proc, indent + 1, stream);
			tchk_isetindent (stream, indent);
			fprintf (stream, "</tracescheck:node>\n");
			break;
			/*}}}*/
			/*{{{  ATOM*/
		case TCN_ATOM:
			fprintf (stream, "<tracescheck:node orgnode=\"0x%8.8x\" type=\"atom\" id=\"%s\" />\n",
					(unsigned int)tcn->orgnode, tcn->u.tcnatom.id);
			break;
			/*}}}*/
			/*{{{  ATOMREF*/
		case TCN_ATOMREF:
			{
				tchknode_t *aref = tcn->u.tcnaref.aref;

				if (aref && (aref->type == TCN_ATOM)) {
					fprintf (stream, "<tracescheck:node orgnode=\"0x%8.8x\" type=\"atomref\" id=\"%s\" />\n",
							(unsigned int)tcn->orgnode, tcn->u.tcnaref.aref->u.tcnatom.id);
				} else {
					fprintf (stream, "<tracescheck:node orgnode=\"0x%8.8x\" type=\"atomref\">\n",
							(unsigned int)tcn->orgnode);
					tracescheck_dumpnode (aref, indent + 1, stream);
					tchk_isetindent (stream, indent);
					fprintf (stream, "</tracescheck:node>\n");
				}
			}
			break;
			/*}}}*/
			/*{{{  NODEREF*/
		case TCN_NODEREF:
			{
				tnode_t *node = tcn->u.tcnnref.nref;

				fprintf (stream, "<tracescheck:node orgnode=\"0x%8.8x\" type=\"noderef\" addr=\"0x%8.8x\" nodetag=\"%s\" nodetype=\"%s\" />\n",
						(unsigned int)tcn->orgnode, (unsigned int)node, node->tag->name, node->tag->ndef->name);
			}
			break;
			/*}}}*/
			/*{{{  SKIP*/
		case TCN_SKIP:
			fprintf (stream, "<tracescheck:node orgnode=\"0x%8.8x\" type=\"skip\" />\n", (unsigned int)tcn->orgnode);
			break;
			/*}}}*/
			/*{{{  STOP*/
		case TCN_STOP:
			fprintf (stream, "<tracescheck:node orgnode=\"0x%8.8x\" type=\"stop\" />\n", (unsigned int)tcn->orgnode);
			break;
			/*}}}*/
			/*{{{  DIV*/
		case TCN_DIV:
			fprintf (stream, "<tracescheck:node orgnode=\"0x%8.8x\" type=\"div\" />\n", (unsigned int)tcn->orgnode);
			break;
			/*}}}*/
			/*{{{  CHAOS*/
		case TCN_CHAOS:
			fprintf (stream, "<tracescheck:node orgnode=\"0x%8.8x\" type=\"chaos\" />\n", (unsigned int)tcn->orgnode);
			break;
			/*}}}*/
		}
	}
	return;
}
/*}}}*/

/*{{{  static int tracescheck_addtostring (char **sptr, int *cur, int *max, const char *str)*/
/*
 *	adds something to a string -- used when building traces-check structures
 *	returns 0 on success, non-zero on failure
 */
static int tracescheck_addtostring (char **sptr, int *cur, int *max, const char *str)
{
	int slen = strlen (str);

	if ((*cur + slen) >= *max) {
		/* need more */
		int nmax = (*max) + (((*cur + slen) - *max) > 1024) ?: 1024;

		*sptr = (char *)srealloc (*sptr, *max, nmax);
		*max = nmax;
	}
	strcpy ((*sptr) + *cur, str);
	*cur = (*cur) + slen;

	return 0;
}
/*}}}*/
/*{{{  static int tracescheck_subformat (tchknode_t *tcn, char **sptr, int *cur, int *max)*/
/*
 *	called to format a set of traces (into a form that we can parse again)
 *	returns 0 on success, non-zero on failure
 */
static int tracescheck_subformat (tchknode_t *tcn, char **sptr, int *cur, int *max)
{
	int v = 0;

	if (!tcn) {
		nocc_warning ("tracescheck_subformat(): NULL node!");
		return 1;
	}
	switch (tcn->type) {
		/*{{{  INVALID*/
	case TCN_INVALID:
		nocc_warning ("tracescheck_subformat(): invalid node!");
		return 0;
		/*}}}*/
		/*{{{  SEQ,PAR,DET,NDET*/
	case TCN_SEQ:
	case TCN_PAR:
	case TCN_DET:
	case TCN_NDET:
		{
			int i;

			tracescheck_addtostring (sptr, cur, max, "(");
			for (i=0; i<DA_CUR (tcn->u.tcnlist.items); i++) {
				tchknode_t *item = DA_NTHITEM (tcn->u.tcnlist.items, i);

				if (i > 0) {
					switch (tcn->type) {
					case TCN_SEQ:	tracescheck_addtostring (sptr, cur, max, " -> "); break;
					case TCN_PAR:	tracescheck_addtostring (sptr, cur, max, " || "); break;
					case TCN_DET:	tracescheck_addtostring (sptr, cur, max, " [] "); break;
					case TCN_NDET:	tracescheck_addtostring (sptr, cur, max, " |~| "); break;
					default: break;
					}
				}
				v += tracescheck_subformat (item, sptr, cur, max);
			}
			tracescheck_addtostring (sptr, cur, max, ")");
		}
		break;
		/*}}}*/
		/*{{{  INPUT,OUTPUT*/
	case TCN_INPUT:
	case TCN_OUTPUT:
		v += tracescheck_subformat (tcn->u.tcnio.varptr, sptr, cur, max);
		tracescheck_addtostring (sptr, cur, max, (tcn->type == TCN_INPUT) ? "?" : "!");
		break;
		/*}}}*/
		/*{{{  FIXPOINT*/
	case TCN_FIXPOINT:
		tracescheck_addtostring (sptr, cur, max, "@");
		v += tracescheck_subformat (tcn->u.tcnfix.id, sptr, cur, max);
		tracescheck_addtostring (sptr, cur, max, ",");
		v += tracescheck_subformat (tcn->u.tcnfix.proc, sptr, cur, max);
		break;
		/*}}}*/
		/*{{{  ATOM*/
	case TCN_ATOM:
		{
			/* include atom node address (keep unique) */
			char *tstr = string_fmt ("%s.%8.8x", tcn->u.tcnatom.id, (unsigned int)tcn);

			tracescheck_addtostring (sptr, cur, max, tstr);
			sfree (tstr);
		}
		break;
		/*}}}*/
		/*{{{  ATOMREF*/
	case TCN_ATOMREF:
		{
			/* include atom node address (keep unique) */
			char *tstr = string_fmt ("%s.%8.8x", tcn->u.tcnaref.aref->u.tcnatom.id, (unsigned int)tcn->u.tcnaref.aref);

			tracescheck_addtostring (sptr, cur, max, tstr);
			sfree (tstr);
		}
		break;
		/*}}}*/
		/*{{{  NODEREF*/
	case TCN_NODEREF:
		{
			char *xname = NULL;
			tnode_t *node = tcn->u.tcnnref.nref;

			langops_getname (node, &xname);
			if (xname) {
				tracescheck_addtostring (sptr, cur, max, xname);
				sfree (xname);
			}
		}
		break;
		/*}}}*/
		/*{{{  SKIP*/
	case TCN_SKIP:
		tracescheck_addtostring (sptr, cur, max, "Skip");
		break;
		/*}}}*/
		/*{{{  STOP*/
	case TCN_STOP:
		tracescheck_addtostring (sptr, cur, max, "Stop");
		break;
		/*}}}*/
		/*{{{  DIV*/
	case TCN_DIV:
		tracescheck_addtostring (sptr, cur, max, "Div");
		break;
		/*}}}*/
		/*{{{  CHAOS*/
	case TCN_CHAOS:
		tracescheck_addtostring (sptr, cur, max, "Chaos");
		break;
		/*}}}*/
	}
	return v;
}
/*}}}*/
/*{{{  int tracescheck_formattraces (tchknode_t *tcn, char **sptr)*/
/*
 *	called to format a set of traces (into a form that we can parse again)
 *	returns 0 on success, non-zero on failure
 */
int tracescheck_formattraces (tchknode_t *tcn, char **sptr)
{
	int cur = 0;
	int max = 4096;
	int v;

	if (*sptr) {
		sfree (*sptr);
		*sptr = NULL;
	}
	*sptr = (char *)smalloc (max);

	v = tracescheck_subformat (tcn, sptr, &cur, &max);
	if (v) {
		/* trash anything we did have */
		if (*sptr) {
			sfree (*sptr);
			*sptr = NULL;
		}
	}

	return v;
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

/*{{{  tchk_bucket_t *tracescheck_newbucket (void)*/
/*
 *	creates a new traces-check bucket
 */
tchk_bucket_t *tracescheck_newbucket (void)
{
	return tchk_newtchkbucket ();
}
/*}}}*/
/*{{{  int tracescheck_pushbucket (tchk_state_t *tcstate)*/
/*
 *	pushes a new bucket onto the traces-check bucket stack -- used when processing fine-grained detail
 *	returns 0 on success, non-zero on failure
 */
int tracescheck_pushbucket (tchk_state_t *tcstate)
{
	tchk_bucket_t *nb;

	if (!tcstate || !tcstate->bucket) {
		nocc_internal ("tracescheck_pushbucket(): NULL state or bucket!");
		return -1;
	}
	nb = tchk_newtchkbucket ();
	nb->prevbucket = tcstate->bucket;
	tcstate->bucket = nb;

	return 0;
}
/*}}}*/
/*{{{  int tracescheck_popbucket (tchk_state_t *tcstate)*/
/*
 *	pops a bucket from the traces-check bucket stack and discards it
 *	returns 0 on success, non-zero on failure
 */
int tracescheck_popbucket (tchk_state_t *tcstate)
{
	tchk_bucket_t *thisb;

	if (!tcstate || !tcstate->bucket || !tcstate->bucket->prevbucket) {
		nocc_internal ("tracescheck_popbucket(): NULL state, bucket or previous bucket!");
		return -1;
	}
	thisb = tcstate->bucket;
	tcstate->bucket = thisb->prevbucket;
	thisb->prevbucket = NULL;

	tchk_freetchkbucket (thisb);
	return 0;
}
/*}}}*/
/*{{{  tchk_bucket_t *tracescheck_pullbucket (tchk_state_t *tcstate)*/
/*
 *	pops a bucket from the traces-check bucket stack and return it
 *	returns bucket on success, NULL on failure
 */
tchk_bucket_t *tracescheck_pullbucket (tchk_state_t *tcstate)
{
	tchk_bucket_t *thisb;

	if (!tcstate || !tcstate->bucket || !tcstate->bucket->prevbucket) {
		nocc_internal ("tracescheck_pullbucket(): NULL state, bucket or previous bucket!");
		return NULL;
	}
	thisb = tcstate->bucket;
	tcstate->bucket = thisb->prevbucket;
	thisb->prevbucket = NULL;

	return thisb;
}
/*}}}*/
/*{{{  int tracescheck_freebucket (tchk_bucket_t *tcb)*/
/*
 *	called to free a previously pulled traces-check bucket
 *	returns 0 on success, non-zero on failure
 */
int tracescheck_freebucket (tchk_bucket_t *tcb)
{
	if (!tcb) {
		nocc_serious ("tracescheck_freebucket(): NULL bucket!");
		return -1;
	}

	tchk_freetchkbucket (tcb);
	return 0;
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
		newtcn = tracescheck_createnode (tcn->type, tcn->orgnode, tcn->u.tcnaref.aref);
		break;
	case TCN_NODEREF:
		newtcn = tracescheck_createnode (tcn->type, tcn->orgnode, tcn->u.tcnnref.nref);
		break;
	default:
		nocc_internal ("tracescheck_dupref(): cannot duplicate non-reference type %d", (int)tcn->type);
		break;
	}
	return newtcn;
}
/*}}}*/
/*{{{  tchknode_t *tracescheck_copynode (tchknode_t *tcn)*/
/*
 *	duplicates (deep) a traces-check node
 *	returns new node on success, NULL on failure
 */
tchknode_t *tracescheck_copynode (tchknode_t *tcn)
{
	tchknode_t *tcc;

	if (!tcn) {
		nocc_serious ("tracescheck_copynode(): NULL node!");
		return NULL;
	}
	
	tcc = tchk_newtchknode (tcn->orgnode);

	tcc->type = tcn->type;
	switch (tcn->type) {
		/*{{{  INVALID*/
	case TCN_INVALID:
		break;
		/*}}}*/
		/*{{{  SEQ,PAR,DET,NDET*/
	case TCN_SEQ:
	case TCN_PAR:
	case TCN_DET:
	case TCN_NDET:
		{
			int i;

			dynarray_init (tcc->u.tcnlist.items);
			for (i=0; i<DA_CUR (tcn->u.tcnlist.items); i++) {
				tchknode_t *item = DA_NTHITEM (tcn->u.tcnlist.items, i);

				if (item) {
					tchknode_t *icopy = tracescheck_copynode (item);

					dynarray_add (tcc->u.tcnlist.items, icopy);
				}
			}
		}
		break;
		/*}}}*/
		/*{{{  INPUT,OUTPUT*/
	case TCN_INPUT:
	case TCN_OUTPUT:
		tcc->u.tcnio.varptr = tracescheck_copynode (tcn->u.tcnio.varptr);
		break;
		/*}}}*/
		/*{{{  FIXPOINT*/
	case TCN_FIXPOINT:
		if (!tcn->u.tcnfix.id || (tcn->u.tcnfix.id->type != TCN_ATOM)) {
			nocc_serious ("tracescheck_copynode(): TCN_FIXPOINT id is NULL or not of type TCN_ATOM");
			tcc->type = TCN_INVALID;
		} else {
			tcc->u.tcnfix.id = tracescheck_copynode (tcn->u.tcnfix.id);
			tcc->u.tcnfix.proc = NULL;

			if (tcn->u.tcnfix.proc) {
				tcc->u.tcnfix.proc = tracescheck_copynode (tcn->u.tcnfix.proc);

				/* now go through and rename references */
				tchk_substatomrefsinnode (tcc->u.tcnfix.proc, tcn->u.tcnfix.id, tcc->u.tcnfix.id);
			}
		}
		break;
		/*}}}*/
		/*{{{  ATOM*/
	case TCN_ATOM:
		/* create a completely fresh atom */
		tcc->type = TCN_INVALID;
		tchk_freetchknode (tcc);
		tcc = tracescheck_createatom ();
		break;
		/*}}}*/
		/*{{{  ATOMREF*/
	case TCN_ATOMREF:
		/* preserve the original reference */
		tcc->u.tcnaref.aref = tcn->u.tcnaref.aref;
		break;
		/*}}}*/
		/*{{{  NODEREF*/
	case TCN_NODEREF:
		/* preserve the original reference */
		tcc->u.tcnnref.nref = tcn->u.tcnnref.nref;
		break;
		/*}}}*/
		/*{{{  SKIP,STOP,DIV,CHAOS*/
	case TCN_SKIP:
	case TCN_STOP:
	case TCN_DIV:
	case TCN_CHAOS:
		break;
		/*}}}*/
	}

	return tcc;
}
/*}}}*/
/*{{{  tchknode_t *tracescheck_createatom (void)*/
/*
 *	creates a new traces-check atom (with a unique identifier)
 */
tchknode_t *tracescheck_createatom (void)
{
	tchknode_t *tcn = tchk_newtchknode (NULL);

	tcn->type = TCN_ATOM;
	tcn->u.tcnatom.id = (char *)smalloc (32);
	sprintf (tcn->u.tcnatom.id, "A%4.4X", tchk_acounter);
	tchk_acounter++;

	return tcn;
}
/*}}}*/
/*{{{  tchknode_t *tracescheck_createnode (tchknodetype_e type, tnode_t *orgnode, ...)*/
/*
 *	creates a new tchknode_t, populated
 *	returns node on success, NULL on failure
 */
tchknode_t *tracescheck_createnode (tchknodetype_e type, tnode_t *orgnode, ...)
{
	tchknode_t *tcn = tchk_newtchknode (orgnode);
	va_list ap;

	va_start (ap, orgnode);
	switch (type) {
		/*{{{  INVALID*/
	case TCN_INVALID:
		break;
		/*}}}*/
		/*{{{  SEQ,PAR,DET,NDET*/
	case TCN_SEQ:
	case TCN_PAR:
	case TCN_DET:
	case TCN_NDET:
		/* NULL-terminated list */
		{
			tchknode_t *arg = va_arg (ap, tchknode_t *);

			dynarray_init (tcn->u.tcnlist.items);
			while (arg) {
				dynarray_add (tcn->u.tcnlist.items, arg);
				arg = va_arg (ap, tchknode_t *);
			}
		}
		break;
		/*}}}*/
		/*{{{  INPUT,OUTPUT*/
	case TCN_INPUT:
	case TCN_OUTPUT:
		tcn->u.tcnio.varptr = va_arg (ap, tchknode_t *);
		break;
		/*}}}*/
		/*{{{  FIXPOINT*/
	case TCN_FIXPOINT:
		tcn->u.tcnfix.id = va_arg (ap, tchknode_t *);
		tcn->u.tcnfix.proc = va_arg (ap, tchknode_t *);
		if (tcn->u.tcnfix.id && (tcn->u.tcnfix.id->type != TCN_ATOM)) {
			nocc_warning ("tracescheck_createnode(): TCN_FIXPOINT id is not of type TCN_ATOM (got %d)", (int)tcn->u.tcnfix.id->type);
		}
		break;
		/*}}}*/
		/*{{{  ATOM*/
	case TCN_ATOM:
		tcn->u.tcnatom.id = string_dup (va_arg (ap, char *));
		break;
		/*}}}*/
		/*{{{  ATOMREF*/
	case TCN_ATOMREF:
		tcn->u.tcnaref.aref = va_arg (ap, tchknode_t *);
		if (tcn->u.tcnaref.aref && (tcn->u.tcnaref.aref->type != TCN_ATOM)) {
			nocc_warning ("tracescheck_createnode(): TCN_ATOMREF reference is not of type TCN_ATOM (got %d)", (int)tcn->u.tcnaref.aref->type);
		}
		break;
		/*}}}*/
		/*{{{  NODEREF*/
	case TCN_NODEREF:
		tcn->u.tcnnref.nref = va_arg (ap, tnode_t *);
		break;
		/*}}}*/
		/*{{{  SKIP,STOP,DIV,CHAOS*/
	case TCN_SKIP:
	case TCN_STOP:
	case TCN_DIV:
	case TCN_CHAOS:
		break;
		/*}}}*/
	}
	tcn->type = type;
	va_end (ap);

	return tcn;
}
/*}}}*/
/*{{{  int tracescheck_simplifynode (tchknode_t **tcnptr)*/
/*
 *	simplifies a single node -- that is, removing single-item SEQs/PARs, etc.
 *	returns 0 on success, non-zero on failure
 */
int tracescheck_simplifynode (tchknode_t **tcnptr)
{
	int i;

	i = tracescheck_modprewalk (tcnptr, tchk_simplifynodeprewalk, NULL);

	return i;
}
/*}}}*/
/*{{{  tchknode_t *tracescheck_firstevent (tchknode_t *tcn)*/
/*
 *	returns the first event in a trace (noderef)
 *	returns NULL if none
 */
tchknode_t *tracescheck_firstevent (tchknode_t *tcn)
{
	firstevent_t *fev = (firstevent_t *)smalloc (sizeof (firstevent_t));
	tchknode_t *tci = NULL;

	fev->item = NULL;

	tracescheck_prewalk (tcn, tchk_firsteventprewalk, (void *)fev);

	tci = fev->item;
	sfree (fev);

	return tci;
}
/*}}}*/

/*{{{  int tracescheck_addtolistnode (tchknode_t *tcn, tchknode_t *item)*/
/*
 *	adds a traces-check node to a list (SEQ/PAR/DET/NDET)
 *	returns 0 on success, non-zero on failure
 */
int tracescheck_addtolistnode (tchknode_t *tcn, tchknode_t *item)
{
	if (!tcn || !item) {
		nocc_internal ("tracescheck_addtolistnode(): NULL node or item!");
		return -1;
	}
	switch (tcn->type) {
	case TCN_SEQ:
	case TCN_PAR:
	case TCN_DET:
	case TCN_NDET:
		dynarray_add (tcn->u.tcnlist.items, item);
		break;
	default:
		nocc_serious ("tracescheck_addtolistnode(): not a list node! (type %d)", (int)tcn->type);
		break;
	}

	return 0;
}
/*}}}*/
/*{{{  int tracescheck_buckettotraces (tchk_state_t *tcstate)*/
/*
 *	moves items from the traces-check bucket into real traces
 *	returns 0 on success, non-zero on failure
 */
int tracescheck_buckettotraces (tchk_state_t *tcstate)
{
	int i;

	if (!tcstate || !tcstate->bucket || !tcstate->traces) {
		nocc_internal ("tracescheck_buckettotraces(): NULL state, bucket or traces!");
		return -1;
	}
	for (i=0; i<DA_CUR (tcstate->bucket->items); i++) {
		tchknode_t *tcn = DA_NTHITEM (tcstate->bucket->items, i);

		dynarray_add (tcstate->traces->items, tcn);
	}
	dynarray_trash (tcstate->bucket->items);

	return 0;
}
/*}}}*/
/*{{{  tchk_traces_t *tracescheck_pulltraces (tchk_state_t *tcstate)*/
/*
 *	removes a completed set of traces from a PROC
 *	returns the traces on success, NULL on failure
 */
tchk_traces_t *tracescheck_pulltraces (tchk_state_t *tcstate)
{
	tchk_traces_t *tct;

	if (!tcstate || !tcstate->traces) {
		nocc_internal ("tracescheck_pulltraces(): NULL state or traces!");
		return NULL;
	}
	tct = tcstate->traces;
	tcstate->traces = NULL;

	return tct;
}
/*}}}*/
/*{{{  int tracescheck_freetraces (tchk_traces_t *tct)*/
/*
 *	used to release a set of traces (tchk_traces_t)
 *	returns 0 on success, non-zero on failure
 */
int tracescheck_freetraces (tchk_traces_t *tct)
{
	if (!tct) {
		nocc_internal ("tracescheck_freetraces(): NULL traces!");
		return -1;
	}
	tchk_freetchktraces (tct);
	return 0;
}
/*}}}*/
/*{{{  int tracescheck_simplifytraces (tchk_traces_t *tct)*/
/*
 *	simplifies traces -- that is, removing single-item SEQs/PARs, etc.
 *	returns 0 on success, non-zero on failure
 */
int tracescheck_simplifytraces (tchk_traces_t *tct)
{
	int i, r = 0;

	if (!tct) {
		nocc_serious ("tracescheck_simplifytraces(): NULL traces!");
		return -1;
	}

	for (i=0; i<DA_CUR (tct->items); i++) {
		tchknode_t **tcnptr = DA_NTHITEMADDR (tct->items, i);

		if (tracescheck_simplifynode (tcnptr)) {
			r++;
		}
	}

	return 0;
}
/*}}}*/
/*{{{  tchk_traces_t *tracescheck_copytraces (tchk_traces_t *tct)*/
/*
 *	duplicates a set of traces
 *	returns new set on success, NULL on failure
 */
tchk_traces_t *tracescheck_copytraces (tchk_traces_t *tct)
{
	tchk_traces_t *newtr = tchk_newtchktraces ();
	int i;

	for (i=0; i<DA_CUR (tct->items); i++) {
		tchknode_t *tnode = DA_NTHITEM (tct->items, i);
		tchknode_t *tcopy = tracescheck_copynode (tnode);

		dynarray_add (newtr->items, tcopy);
	}
	return newtr;
}
/*}}}*/
/*{{{  int tracescheck_prunetraces (tchk_traces_t *tct, tnode_t *vlist)*/
/*
 *	prunes a set of traces -- restricts it to those involving the given nodes (in a list)
 *	returns 0 on success, non-zero on failure
 */
int tracescheck_prunetraces (tchk_traces_t *tct, tnode_t *vlist)
{
	int i;

	for (i=0; i<DA_CUR (tct->items); i++) {
		prunetraces_t *ptrace = (prunetraces_t *)smalloc (sizeof (prunetraces_t));

		ptrace->vlist = vlist;
		ptrace->changed = 0;

		tracescheck_modprewalk (DA_NTHITEMADDR (tct->items, i), tchk_prunetracesmodprewalk, (void *)ptrace);

		sfree (ptrace);
	}

	return 0;
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
	if (!tcstate->bucket) {
		nocc_internal ("tracescheck_addtobucket(): NULL bucket in state!");
		return -1;
	}
	dynarray_add (tcstate->bucket->items, tcn);
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

/*{{{  tchk_tracewalk_t *tracescheck_startwalk (tchknode_t *start)*/
/*
 *	starts a traces walk -- designed to walk through traces
 *	returns a tracewalk stucture
 */
tchk_tracewalk_t *tracescheck_startwalk (tchknode_t *start)
{
	tchk_tracewalk_t *ttw = tchk_newtchktracewalk ();

	ttw->end = 0;
	ttw->depth = 0;
	tchk_walkpushstack (ttw, start, -1);

	return ttw;
}
/*}}}*/
/*{{{  tchknode_t *tracescheck_stepwalk (tchk_tracewalk_t *ttw)*/
/*
 *	steps through a traces walk -- returns the node touched next
 *	may legitimately return NULL as a node, use "->end" to check for end
 */
tchknode_t *tracescheck_stepwalk (tchk_tracewalk_t *ttw)
{
	tchknode_t *result = NULL;
	tchknode_t *thisnode;
	int *thisdatap;
	int atnext = 0;

	if (ttw->end) {
		return NULL;
	}

	thisnode = DA_NTHITEM (ttw->stack, ttw->depth - 1);
	thisdatap = DA_NTHITEMADDR (ttw->data, ttw->depth - 1);

	result = thisnode;

	/* setup for the next node */

	while (!atnext) {
		thisnode = DA_NTHITEM (ttw->stack, ttw->depth - 1);
		thisdatap = DA_NTHITEMADDR (ttw->data, ttw->depth - 1);

		switch (thisnode->type) {
		case TCN_SEQ:
		case TCN_PAR:
		case TCN_DET:
		case TCN_NDET:
			(*thisdatap)++;
			if (*thisdatap == DA_CUR (thisnode->u.tcnlist.items)) {
				/* end of subwalk */
				tchk_walknextstep (ttw);
			} else {
				tchk_walkpushstack (ttw, DA_NTHITEM (thisnode->u.tcnlist.items, *thisdatap), -1);
				atnext = 1;
			}
			break;
		case TCN_INPUT:
		case TCN_OUTPUT:
			(*thisdatap)++;
			if (*thisdatap) {
				tchk_walknextstep (ttw);
				/* end of subwalk */
			} else {
				tchk_walkpushstack (ttw, thisnode->u.tcnio.varptr, -1);
				atnext = 1;
			}
			break;
		case TCN_FIXPOINT:
			(*thisdatap)++;
			if (*thisdatap) {
				tchk_walknextstep (ttw);
				/* end of subwalk */
			} else {
				tchk_walkpushstack (ttw, thisnode->u.tcnfix.proc, -1);
				atnext = 1;
			}
			break;
		case TCN_NODEREF:
		case TCN_ATOM:
		case TCN_ATOMREF:
		case TCN_SKIP:
		case TCN_STOP:
		case TCN_DIV:
		case TCN_CHAOS:
		case TCN_INVALID:
			/* singleton, end of subwalk */
			tchk_walknextstep (ttw);
			break;
		}
		if (!ttw->depth) {
			atnext = 1;
		}
	}

	return result;
}
/*}}}*/
/*{{{  int tracescheck_endwalk (tchk_tracewalk_t *ttw)*/
/*
 *	called to end a walk inside a trace
 *	returns 0 if the walk was complete, non-zero otherwise
 */
int tracescheck_endwalk (tchk_tracewalk_t *ttw)
{
	int r = ttw->end ? 0 : 1;

	tchk_freetchktracewalk (ttw);
	return r;
}
/*}}}*/
/*{{{  void tracescheck_testwalk (tchknode_t *tcn)*/
/*
 *	tests at trace walk (debugging)
 */
void tracescheck_testwalk (tchknode_t *tcn)
{
	tchk_tracewalk_t *ttw = tracescheck_startwalk (tcn);

	while (!ttw->end) {
		tchknode_t *node = tracescheck_stepwalk (ttw);

		fprintf (stderr, "tracescheck_testwalk(): visiting 0x%8.8x (type %d)\n", (unsigned int)node, node ? (int)node->type : -1);
	}

	tracescheck_endwalk (ttw);
	return;
}
/*}}}*/

/*{{{  static int tchk_totrace_prewalk (tnode_t *tptr, void *arg)*/
/*
 *	transforms a tnode_t tree-structu into a tracescheck tchknode_t structure (prewalk)
 *	returns 0 to stop walk, 1 to continue
 */
static int tchk_totrace_prewalk (tnode_t *tptr, void *arg)
{
	tchk_bucket_t *tb = (tchk_bucket_t *)arg;
	int v = 1;

	if (!tb) {
		nocc_error ("tchk_totrace_prewalk(): NULL bucket!");
		return 0;
	} else if (!tptr) {
		nocc_error ("tchk_totrace_prewalk(): NULL tree!");
		return 0;
	}

	if (tnode_haslangop (tptr->tag->ndef->lops, "tracescheck_totrace")) {
		v = tnode_calllangop (tptr->tag->ndef->lops, "tracescheck_totrace", 2, tptr, tb);
	}
	return v;
}
/*}}}*/
/*{{{  tchknode_t *tracescheck_totrace (tnode_t *tptr)*/
/*
 *	transforms a tnode_t tree-structure into a tracescheck tchknode_t structure
 *	(used for importing previously generated traces from external sources)
 *	returns trace structure on success, NULL on failure
 */
tchknode_t *tracescheck_totrace (tnode_t *tptr)
{
	tchk_bucket_t *tb = tracescheck_newbucket ();

	tnode_prewalktree (tptr, tchk_totrace_prewalk, (void *)tb);

	if (DA_CUR (tb->items) == 1) {
		/* this one */
		tchknode_t *tcn = DA_NTHITEM (tb->items, 0);

		DA_SETNTHITEM (tb->items, 0, NULL);
		tracescheck_freebucket (tb);

		return tcn;
	} else if (!DA_CUR (tb->items)) {
		tnode_error (tptr, "failed to get traces from trace tree (%s,%s)", tptr->tag->name, tptr->tag->ndef->name);
	} else {
		tnode_error (tptr, "too many traces (%d) from trace tree (%s,%s)", DA_CUR (tb->items), tptr->tag->name, tptr->tag->ndef->name);
	}
	tracescheck_freebucket (tb);
	return NULL;
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
/*{{{  chook_t *tracescheck_gettracesrefchook (void)*/
/*
 *	returns the traceschecktracesref compiler hook
 */
chook_t *tracescheck_gettracesrefchook (void)
{
	return tchk_tracesrefchook;
}
/*}}}*/
/*{{{  chook_t *tracescheck_gettraceschook (void)*/
/*
 *	returns the traceschecktraces compiler hook
 */
chook_t *tracescheck_gettraceschook (void)
{
	return tchk_traceschook;
}
/*}}}*/
/*{{{  chook_t *tracescheck_getimplchook (void)*/
/*
 *	returns the tracescheckimpl compiler hook
 */
chook_t *tracescheck_getimplchook (void)
{
	return tchk_tracesimplchook;
}
/*}}}*/
/*{{{  chook_t *tracescheck_getbvarschook (void)*/
/*
 *	returns the tracesbvars compiler hook (used to identify bound variables after a re-write)
 */
chook_t *tracescheck_getbvarschook (void)
{
	return tchk_tracesbvarschook;
}
/*}}}*/

/*{{{  static int tracescheck_docheckspec_prewalk (tnode_t *node, void *arg)*/
/*
 *	called in tree prewalk order to do verification checks on a trace against a specification
 *	returns 0 to stop walk, 1 to continue
 */
static int tracescheck_docheckspec_prewalk (tnode_t *node, void *arg)
{
	tchk_check_t *tcc = (tchk_check_t *)arg;

	if (tnode_haslangop (node->tag->ndef->lops, "tracescheck_check")) {
		int r;

		if (compopts.tracetracescheck) {
			fprintf (stderr, "tracescheck_docheckspec_prewalk(): calling \"tracescheck_check\" op on (%s,%s)\n",
					node->tag->name, node->tag->ndef->name);
		}
		r = tnode_calllangop (node->tag->ndef->lops, "tracescheck_check", 2, node, tcc);
		if (r) {
			tcc->err++;
		}

		return 0;
	}
	return 1;
}
/*}}}*/
/*{{{  int tracescheck_dosubcheckspec (tnode_t *spec, tchknode_t *trace, tchk_check_t *tcc)*/
/*
 *	checks a trace against a trace specification (for nested calls)
 *	returns 0 on success, non-zero on failure
 */
int tracescheck_dosubcheckspec (tnode_t *spec, tchknode_t *trace, tchk_check_t *tcc)
{
	tcc->thistrace = trace;
	tcc->thisspec = spec;

	tnode_prewalktree (spec, tracescheck_docheckspec_prewalk, (void *)tcc);
	
	return tcc->err;
}
/*}}}*/
/*{{{  int tracescheck_docheckspec (tnode_t *spec, tchk_traces_t *traces, tchk_state_t *tcstate, tnode_t *locn)*/
/*
 *	checks a trace against a trace specification
 *	returns 0 on success, non-zero on failure
 */
int tracescheck_docheckspec (tnode_t *spec, tchk_traces_t *traces, tchk_state_t *tcstate, tnode_t *locn)
{
	tchk_check_t *tcc = (tchk_check_t *)smalloc (sizeof (tchk_check_t));
	int r = 0;
	int i;

	tcc->state = tcstate;
	tcc->err = 0;
	tcc->warn = 0;
	tcc->traces = traces;
	tcc->spec = spec;

	if (compopts.tracetracescheck) {
		fprintf (stderr, "tracescheck_docheckspec(): checking specification against traces..\n");
	}

#if 0
fprintf (stderr, "tracescheck_docheckspec(): specification is:\n");
tnode_dumptree (spec, 1, stderr);
fprintf (stderr, "tracescheck_docheckspec(): traces were:\n");
tracescheck_dumptraces (traces, 1, stderr);
#endif

	if (!DA_CUR (traces->items)) {
		/* means there were no traces! */
		tracescheck_checkerror (locn, tcc, "no traces to match against specification!");
	}

	/* if more than one trace in "traces", all must match */
	for (i=0; i<DA_CUR (traces->items); i++) {
		tcc->thisspec = spec;
		tcc->thiswalk = tracescheck_startwalk (DA_NTHITEM (traces->items, i));
		tcc->thistrace = tracescheck_stepwalk (tcc->thiswalk);				/* get first item in the trace */

		if (tcc->thiswalk->end) {
			/* end of walk */
		} else {
			tnode_prewalktree (spec, tracescheck_docheckspec_prewalk, (void *)tcc);
		}
		tracescheck_endwalk (tcc->thiswalk);
		tcc->thiswalk = NULL;
	}

	r = tcc->err;
	sfree (tcc);

	return r;
}
/*}}}*/


