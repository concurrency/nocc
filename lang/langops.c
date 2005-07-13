/*
 *	langops.c -- langage-level operations for nocc
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
#include "names.h"
#include "langops.h"
#include "langops.h"

/*}}}*/


/*{{{  static int langops_getdescriptor_walk (tnode_t *node, void *ptr)*/
/*
 *	tree-walk routine for getting descriptors
 *	returns 0 to stop walk, 1 to continue
 */
static int langops_getdescriptor_walk (tnode_t *node, void *ptr)
{
	int r;

	if (!node) {
		return 0;
	}
	if (node->tag->ndef->lops && node->tag->ndef->lops->getdescriptor) {
		r = node->tag->ndef->lops->getdescriptor (node, (char **)ptr);
	} else {
		r = 1;
	}
	return r;
}
/*}}}*/
/*{{{  void langops_getdescriptor (tnode_t *node, char **str)*/
/*
 *	walks a section of the tree to get descriptor-line information
 */
void langops_getdescriptor (tnode_t *node, char **str)
{
	tnode_prewalktree (node, langops_getdescriptor_walk, (void *)str);
	return;
}
/*}}}*/


/*{{{  int langops_init (void)*/
/*
 *	initialises tree-operations
 *	returns 0 on success, non-zero on failure
 */
int langops_init (void)
{
	return 0;
}
/*}}}*/
/*{{{  int langops_shutdown (void)*/
/*
 *	shuts-down tree-operations
 *	returns 0 on success, non-zero on failure
 */
int langops_shutdown (void)
{
	return 0;
}
/*}}}*/


