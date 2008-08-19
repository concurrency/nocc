/*
 *	mobilitycheck.c -- mobility checker for NOCC
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
#include "mobilitycheck.h"


/*}}}*/
/*{{{  private data*/

static chook_t *tchk_traceschook = NULL;


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
	tchk_traceschook = tracescheck_gettraceschook ();

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
	mchk_state_t *mcstate = (mchk_state_t *)smalloc (sizeof (mchk_state_t));
	int res = 0;

	mcstate->err = 0;
	mcstate->warn = 0;

	tnode_prewalktree (tree, mchk_prewalk_tree, (void *)mcstate);

	res = mcstate->err;

	sfree (mcstate);

	return res;
}
/*}}}*/


