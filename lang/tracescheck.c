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
	tchk_state_t *tcstate = (tchk_state_t *)smalloc (sizeof (tchk_state_t));
	int res = 0;

	tcstate->err = 0;
	tcstate->warn = 0;

	tnode_prewalktree (tree, tchk_prewalk_tree, (void *)tcstate);

	res = tcstate->err;
	sfree (tcstate);

	return res;
}
/*}}}*/

