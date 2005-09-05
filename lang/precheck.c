/*
 *	precheck.c -- pre-checker for NOCC
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
#include "symbols.h"
#include "keywords.h"
#include "lexer.h"
#include "lexpriv.h"
#include "tnode.h"
#include "parser.h"
#include "parsepriv.h"
#include "dfa.h"
#include "names.h"
#include "precheck.h"


/*}}}*/


/*{{{  static int precheck_prewalk_tree (tnode_t *node, void *data)*/
/*
 *	does pre-checking on nodes
 *	returns 0 to stop walk, 1 to continue
 */
static int precheck_prewalk_tree (tnode_t *node, void *data)
{
	int result = 1;

	if (!node) {
		nocc_internal ("precheck_prewalk_tree(): NULL node!");
		return 0;
	}
	if (node->tag->ndef->ops && node->tag->ndef->ops->precheck) {
		result = node->tag->ndef->ops->precheck (node);
	}

	return result;
}
/*}}}*/


/*{{{  int precheck_init (void)*/
/*
 *	initialises pre-checker
 *	returns 0 on success, non-zero on error
 */
int precheck_init (void)
{
	return 0;
}
/*}}}*/
/*{{{  int precheck_shutdown (void)*/
/*
 *	shuts-down pre-checker
 *	returns 0 on success, non-zero on error
 */
int precheck_shutdown (void)
{
	return 0;
}
/*}}}*/


/*{{{  int precheck_subtree (tnode_t *tree)*/
/*
 *	called to do nested processing on a tree
 *	returns 0 on success, non-zero on error
 */
int precheck_subtree (tnode_t *tree)
{
	tnode_prewalktree (tree, precheck_prewalk_tree, NULL);

	return 0;
}
/*}}}*/
/*{{{  int precheck_tree (tnode_t *tree)*/
/*
 *	does pre-checking on the parse tree
 *	returns 0 on success, non-zero on error
 */
int precheck_tree (tnode_t *tree)
{
	tnode_prewalktree (tree, precheck_prewalk_tree, NULL);

	return 0;
}
/*}}}*/

