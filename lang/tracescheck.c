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


/*}}}*/
/*{{{  forward decls*/
static tchk_traces_t *tchk_newtchktraces (void);
static void tchk_freetchktraces (tchk_traces_t *tct);

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

	/*}}}*/
	/*{{{  traces-check language operation*/

	tnode_newlangop ("tracescheck_check", LOPS_INVALID, 2, INTERNAL_ORIGIN);

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
		/*{{{  INVALID, ATOM, NODEREF*/
	case TCN_INVALID:
	case TCN_ATOM:
	case TCN_NODEREF:
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
			/*{{{  INVALID*/
		case TCN_INVALID:
		case TCN_ATOM:
		case TCN_ATOMREF:
		case TCN_NODEREF:
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
		case TCN_INVALID:
			fprintf (stream, "<tracescheck:node type=\"invalid\" />\n");
			break;
		case TCN_SEQ:
		case TCN_PAR:
		case TCN_DET:
		case TCN_NDET:
			{
				int i;

				fprintf (stream, "<tracescheck:node type=\"");
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
	
	tcc = tchk_newtchknode ();

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

		r = tnode_calllangop (node->tag->ndef->lops, "tracescheck_check", 2, node, tcc);
		if (r) {
			tcc->err++;
		}

		return 0;
	}
	return 1;
}
/*}}}*/
/*{{{  int tracescheck_dosubcheckspec (tnode_t *spec, tchk_traces_t *traces, tchk_check_t *tcc)*/
/*
 *	checks a trace against a trace specification (for nested calls)
 *	returns 0 on success, non-zero on failure
 */
int tracescheck_dosubcheckspec (tnode_t *spec, tchk_traces_t *traces, tchk_check_t *tcc)
{
	tcc->thistrace = traces;
	tcc->thisspec = spec;

	tnode_prewalktree (spec, tracescheck_docheckspec_prewalk, (void *)tcc);
	
	return tcc->err;
}
/*}}}*/
/*{{{  int tracescheck_docheckspec (tnode_t *spec, tchk_traces_t *traces, tchk_state_t *tcstate)*/
/*
 *	checks a trace against a trace specification
 *	returns 0 on success, non-zero on failure
 */
int tracescheck_docheckspec (tnode_t *spec, tchk_traces_t *traces, tchk_state_t *tcstate)
{
	tchk_check_t *tcc = (tchk_check_t *)smalloc (sizeof (tchk_check_t));
	int r = 0;

	tcc->state = tcstate;
	tcc->err = 0;
	tcc->warn = 0;
	tcc->traces = traces;
	tcc->spec = spec;
	tcc->thistrace = traces;
	tcc->thisspec = spec;

	tnode_prewalktree (spec, tracescheck_docheckspec_prewalk, (void *)tcc);

	r = tcc->err;
	sfree (tcc);

	return r;
}
/*}}}*/


