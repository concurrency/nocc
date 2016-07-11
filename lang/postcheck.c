/*
 *	postcheck.c -- post-checker for NOCC
 *	Copyright (C) 2006-2016 Fred Barnes <frmb@kent.ac.uk>
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
#include <stdint.h>
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
#include "postcheck.h"


/*}}}*/


/*{{{  static int postcheck_modprewalk_tree (tnode_t **node, void *data)*/
/*
 *	does pre-checking on nodes
 *	returns 0 to stop walk, 1 to continue
 */
static int postcheck_modprewalk_tree (tnode_t **node, void *data)
{
	int result = 1;

	if (!node || !*node) {
		nocc_internal ("postcheck_modprewalk_tree(): NULL node!");
		return 0;
	}
	if ((*node)->tag->ndef->ops && tnode_hascompop_i ((*node)->tag->ndef->ops, (int)COPS_POSTCHECK)) {
		result = tnode_callcompop_i ((*node)->tag->ndef->ops, (int)COPS_POSTCHECK, 2, node, (postcheck_t *)data);
	}

	return result;
}
/*}}}*/


/*{{{  int postcheck_init (void)*/
/*
 *	initialises pre-checker
 *	returns 0 on success, non-zero on error
 */
int postcheck_init (void)
{
	return 0;
}
/*}}}*/
/*{{{  int postcheck_shutdown (void)*/
/*
 *	shuts-down pre-checker
 *	returns 0 on success, non-zero on error
 */
int postcheck_shutdown (void)
{
	return 0;
}
/*}}}*/


/*{{{  int postcheck_subtree (tnode_t **treep, postcheck_t *pc)*/
/*
 *	called to do nested processing on a tree
 *	returns 0 on success, non-zero on error
 */
int postcheck_subtree (tnode_t **treep, postcheck_t *pc)
{
	tnode_modprewalktree (treep, postcheck_modprewalk_tree, (void *)pc);

	return 0;
}
/*}}}*/
/*{{{  int postcheck_tree (tnode_t *tree)*/
/*
 *	does pre-checking on the parse tree
 *	returns 0 on success, non-zero on error
 */
int postcheck_tree (tnode_t **treep, langparser_t *lang)
{
	postcheck_t *pc = (postcheck_t *)smalloc (sizeof (postcheck_t));

	pc->lang = lang;
	pc->langpriv = NULL;

	if (lang->postcheck) {
		lang->postcheck (treep, pc);
	} else {
		tnode_modprewalktree (treep, postcheck_modprewalk_tree, (void *)pc);
	}

	sfree (pc);

	return 0;
}
/*}}}*/

