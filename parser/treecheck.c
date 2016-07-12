/*
 *	treecheck.c -- tree checking routines for NOCC
 *	Copyright (C) 2007-2016 Fred Barnes <frmb@kent.ac.uk>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/*{{{  includes*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/types.h>
#include <errno.h>

#include "nocc.h"
#include "support.h"
#include "version.h"
#include "symbols.h"
#include "keywords.h"
#include "lexer.h"
#include "tnode.h"
#include "treecheck.h"
#include "parser.h"
#include "typecheck.h"
#include "fcnlib.h"
#include "extn.h"
#include "dfa.h"
#include "dfaerror.h"
#include "langdef.h"
#include "parsepriv.h"
#include "lexpriv.h"
#include "names.h"
#include "target.h"
#include "opts.h"


/*}}}*/
/*{{{  private types*/

typedef struct {
	char *passname;		/* compiler pass we're currently doing */
	int passenabled;	/* zero if the pass is disabled */
	int prepost;		/* zero if pre-pass, non-zero if post-pass */

	int warncount;		/* warning-count */
	int errcount;		/* error-count */
} tchk_treewalk_t;


/*}}}*/
/*{{{  private data*/

static int tchk_dumpsyntaxflag = 0;

STATICDYNARRAY (treecheckdef_t *, treechecks);


/*}}}*/


/*{{{  static treecheckdef_t *tchk_newdef (void)*/
/*
 *	creates a new treecheckdef_t structure
 */
static treecheckdef_t *tchk_newdef (void)
{
	treecheckdef_t *tcdef = (treecheckdef_t *)smalloc (sizeof (treecheckdef_t));

	dynarray_init (tcdef->descs);
	tcdef->invbefore = NULL;
	tcdef->invafter = NULL;
	tcdef->cvalid = 1;			/* assume valid to start with */

	tcdef->tndef = NULL;

	return tcdef;
}
/*}}}*/
/*{{{  static void tchk_freedef (treecheckdef_t *tcdef)*/
/*
 *	frees a treecheckdef_t structure
 */
static void tchk_freedef (treecheckdef_t *tcdef)
{
	int i;

	if (!tcdef) {
		nocc_warning ("tchk_freedef(): NULL pointer!");
		return;
	}
	for (i=0; i<DA_CUR (tcdef->descs); i++) {
		char *desc = DA_NTHITEM (tcdef->descs, i);

		if (desc) {
			sfree (desc);
		}
	}
	dynarray_trash (tcdef->descs);

	if (tcdef->invbefore) {
		sfree (tcdef->invbefore);
		tcdef->invbefore = NULL;
	}
	if (tcdef->invafter) {
		sfree (tcdef->invafter);
		tcdef->invafter = NULL;
	}
	tcdef->cvalid = 0;
	tcdef->tndef = NULL;

	sfree (tcdef);
	return;
}
/*}}}*/
/*{{{  static tchk_treewalk_t *tchk_newtreewalk (void)*/
/*
 *	creates a new tchk_treewalk_t structure
 */
static tchk_treewalk_t *tchk_newtreewalk (void)
{
	tchk_treewalk_t *tw = (tchk_treewalk_t *)smalloc (sizeof (tchk_treewalk_t));

	tw->passname = NULL;
	tw->passenabled = 0;
	tw->prepost = 0;
	tw->warncount = 0;
	tw->errcount = 0;

	return tw;
}
/*}}}*/
/*{{{  static void tchk_freetreewalk (tchk_treewalk_t *tw)*/
/*
 *	frees a tchk_treewalk_t structure
 */
static void tchk_freetreewalk (tchk_treewalk_t *tw)
{
	if (!tw) {
		nocc_error ("tchk_freetreewalk(): NULL pointer!");
		return;
	}
	/* this doesn't hold any data of its own per-se */
	sfree (tw);
	return;
}
/*}}}*/


/*{{{  static int tchk_opthandler (cmd_option_t *opt, char ***argwalk, int *argleft)*/
/*
 *	called to handle certain command-line arguments
 *	returns 0 on success, non-zero on failure
 */
static int tchk_opthandler (cmd_option_t *opt, char ***argwalk, int *argleft)
{
	int optv = (int)((uint64_t)opt->arg);

	switch (optv) {
		/*{{{  1 -- setting 'dump syntax' flag*/
	case 1:
		if (compopts.verbose) {
			nocc_message ("will dump abstract syntax tree after compiler run");
		}

		tchk_dumpsyntaxflag = 1;

		break;
		/*}}}*/
	default:
		nocc_error ("tchk_opthandler(): unknown option [%s]", **argwalk);
		break;
	}
	return 0;
}
/*}}}*/


/*{{{  static int tchk_pretreewalk (tnode_t *node, void *arg)*/
/*
 *	called to do checking on an individual node
 *	returns 0 to stop walk, 1 to continue
 */
static int tchk_pretreewalk (tnode_t *node, void *arg)
{
	tchk_treewalk_t *tw = (tchk_treewalk_t *)arg;
	tndef_t *tnd = node->tag->ndef;
	treecheckdef_t *tcdef = tnd->tchkdef;

	if (!tcdef) {
		/* no checks for this node, ignore */
		return 1;
	}

	if (!tcdef->cvalid) {
		tnode_warning (node, "** treecheck %s %s: node-type %s invalid here", tw->prepost ? "after" : "before", tw->passname, tnd->name);
		tw->errcount++;
	}

	/* other checks ? */

	return 1;
}
/*}}}*/


/*{{{  int treecheck_init (void)*/
/*
 *	initialises the tree-checking routines
 *	returns 0 on success, non-zero on failure
 */
int treecheck_init (void)
{
	dynarray_init (treechecks);

	opts_add ("treecheck-astdump", '\0', tchk_opthandler, (void *)1, "1dump abstract syntax tree from tree-check");

	return 0;
}
/*}}}*/
/*{{{  int treecheck_finalise (void)*/
/*
 *	finalises the tree-checking routines, dumps abstract syntax if requested
 *	returns 0 on success, non-zero on failure
 */
int treecheck_finalise (void)
{
	if (tchk_dumpsyntaxflag) {
		int i;

		nocc_message ("abstract syntax tree as held by tree-check:");
		for (i=0; i<DA_CUR (treechecks); i++) {
			treecheckdef_t *tcdef = DA_NTHITEM (treechecks, i);
			int j;

			if (!tcdef->tndef) {
				continue;
			}
			fprintf (stderr, "    %s (", tcdef->tndef->name);
			for (j=0; j<tcdef->tndef->nsub; j++) {
				fprintf (stderr, "%s%s", j ? "," : "", tcdef->descs[j]);
			}
			fprintf (stderr, ")\n");
		}
	}
	return 0;
}
/*}}}*/
/*{{{  int treecheck_shutdown (void)*/
/*
 *	shuts-down the tree-checking routines
 *	returns 0 on success, non-zero on failure
 */
int treecheck_shutdown (void)
{
	int i;

	for (i=0; i<DA_CUR (treechecks); i++) {
		treecheckdef_t *tcdef = DA_NTHITEM (treechecks, i);

		if (!treecheck_destroycheck (tcdef)) {
			i--;
		}
	}
	dynarray_trash (treechecks);

	return 0;
}
/*}}}*/


/*{{{  treecheckdef_t *treecheck_createcheck (char *nodename, int nsub, int nname, int nhook, char **descs, char *invbefore, char *invafter)*/
/*
 *	creates a new treecheckdef_t structure, populated with the given data and linked to the relevant tndef_t node-type
 *	returns check on success, NULL on failure
 */
treecheckdef_t *treecheck_createcheck (char *nodename, int nsub, int nname, int nhook, char **descs, char *invbefore, char *invafter)
{
	treecheckdef_t *tcdef = NULL;
	tndef_t *tnd = tnode_lookupnodetype (nodename);
	int i;

	if (!tnd) {
		nocc_error ("treecheck_createcheck(): node type [%s] not found", nodename);
		return NULL;
	}

	if (tnd->tchkdef) {
		nocc_error ("treecheck_createcheck(): node type [%s] already has check!", nodename);
		return NULL;
	}

	/* first check is to see whether the counts match up with what we know! -- hard fail */
	if (tnd->nsub != nsub) {
		nocc_error ("treecheck_createcheck(): node type [%s] has %d subnodes, but checking for %d", nodename, tnd->nsub, nsub);
		return NULL;
	}
	if (tnd->nname != nname) {
		nocc_error ("treecheck_createcheck(): node type [%s] has %d name-nodes, but checking for %d", nodename, tnd->nname, nname);
		return NULL;
	}
	if (tnd->nhooks != nhook) {
		nocc_error ("treecheck_createcheck(): node type [%s] has %d name-nodes, but checking for %d", nodename, tnd->nhooks, nhook);
		return NULL;
	}

	tcdef = tchk_newdef ();

	for (i=0; i<(nsub + nname + nhook); i++) {
		dynarray_add (tcdef->descs, string_dup (descs[i] ?: ""));
	}
	tcdef->invbefore = invbefore ? string_dup (invbefore) : NULL;
	tcdef->invafter = invafter ? string_dup (invafter) : NULL;

	if (tcdef->invbefore) {
		tcdef->cvalid = 0;	/* invalid before a particular pass, so invalid initially */
	} else {
		tcdef->cvalid = 1;	/* otherwise must be initially valid */
	}

	/* link onto node-type definition */
	tcdef->tndef = tnd;
	tnd->tchkdef = tcdef;

	dynarray_add (treechecks, tcdef);

	return tcdef;
}
/*}}}*/
/*{{{  int treecheck_destroycheck (treecheckdef_t *tcdef)*/
/*
 *	destroys a treedefcheck_t structure, first unlinking it from the associated node-type
 *	returns 0 on success, non-zero on failure
 */
int treecheck_destroycheck (treecheckdef_t *tcdef)
{
	tndef_t *tnd;
	
	if (!tcdef) {
		nocc_error ("treecheck_destroycheck(): NULL pointer!");
		return -1;
	}
	tnd = tcdef->tndef;
	if (!tnd) {
		nocc_error ("treecheck_destroycheck(): check not linked to a node-type!");
		return -1;
	}
	if (tnd->tchkdef != tcdef) {
		nocc_error ("treecheck_destroycheck(): linkage confusion, check at %p, but %p linked to [%s]", tcdef, tnd->tchkdef, tnd->name);
		return -1;
	}

	/* unlink and free */
	tnd->tchkdef = NULL;
	tcdef->tndef = NULL;

	dynarray_rmitem (treechecks, tcdef);

	tchk_freedef (tcdef);
	return 0;
}
/*}}}*/


/*{{{  int treecheck_prepass (tnode_t *tree, const char *pname, const int penabled)*/
/*
 *	called to do pre-pass checks on a parse-tree
 *	returns 0 on success, non-zero on failure
 */
int treecheck_prepass (tnode_t *tree, const char *pname, const int penabled)
{
	tchk_treewalk_t *tw = tchk_newtreewalk ();
	int rval = 0;
	int i;

	tw->passname = (char *)pname;
	tw->passenabled = penabled;
	tw->prepost = 0;

	tnode_prewalktree (tree, tchk_pretreewalk, (void *)tw);

	if (tw->errcount) {
		rval = -1;
	}
	tchk_freetreewalk (tw);

	/* anything invalid before this pass is now valid */
	for (i=0; i<DA_CUR (treechecks); i++) {
		treecheckdef_t *tcdef = DA_NTHITEM (treechecks, i);

		if (tcdef->invbefore && !strcmp (pname, tcdef->invbefore)) {
			tcdef->cvalid = 1;
		}
	}

	return rval;
}
/*}}}*/
/*{{{  int treecheck_postpass (tnode_t *tree, const char *pname, const int penabled)*/
/*
 *	called to do post-pass checks on a parse-tree
 *	returns 0 on success, non-zero on failure
 */
int treecheck_postpass (tnode_t *tree, const char *pname, const int penabled)
{
	tchk_treewalk_t *tw = tchk_newtreewalk ();
	int rval = 0;
	int i;

	/* anything invalid after this pass is now invalid */
	for (i=0; i<DA_CUR (treechecks); i++) {
		treecheckdef_t *tcdef = DA_NTHITEM (treechecks, i);

		if (tcdef->invafter && !strcmp (pname, tcdef->invafter)) {
			tcdef->cvalid = 0;
		}
	}

	tw->passname = (char *)pname;
	tw->passenabled = penabled;
	tw->prepost = 1;

	tnode_prewalktree (tree, tchk_pretreewalk, (void *)tw);

	if (tw->errcount) {
		rval = -1;
	}
	tchk_freetreewalk (tw);

	return rval;
}
/*}}}*/



