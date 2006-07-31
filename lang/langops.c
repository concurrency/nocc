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
		r = node->tag->ndef->lops->getdescriptor (node->tag->ndef->lops, node, (char **)ptr);
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
/*{{{  static int langops_getname_walk (tnode_t *node, void *ptr)*/
/*
 *	tree-walk routine for getting names
 *	returns 0 to stop walk, 1 to continue
 */
static int langops_getname_walk (tnode_t *node, void *ptr)
{
	int r = 1;

	if (!node) {
		return 0;
	}
	if (node->tag->ndef->lops && node->tag->ndef->lops->getname) {
		r = node->tag->ndef->lops->getname (node->tag->ndef->lops, node, (char **)ptr);
		if (r < 0) {
			r = 1;		/* try again down the tree */
		}
	}

	return r;
}
/*}}}*/
/*{{{  void langops_getname (tnode_t *node, char **str)*/
/*
 *	walks a section of the tree to extract a name
 */
void langops_getname (tnode_t *node, char **str)
{
	tnode_prewalktree (node, langops_getname_walk, (void *)str);
	return;
}
/*}}}*/
/*{{{  int langops_isconst (tnode_t *node)*/
/*
 *	determines whether or not the given node is a known constant
 */
int langops_isconst (tnode_t *node)
{
	int r = 0;

	if (node && node->tag->ndef->lops && node->tag->ndef->lops->isconst) {
		r = node->tag->ndef->lops->isconst (node->tag->ndef->lops, node);
	}
	return r;
}
/*}}}*/
/*{{{  int langops_constvalof (tnode_t *node, void *ptr)*/
/*
 *	extracts the constant value of a given node.  if "ptr" is non-null,
 *	value is assigned there (to the appropriate width).  Anything integer
 *	or less is returned as well.
 */
int langops_constvalof (tnode_t *node, void *ptr)
{
	int r = 0;

	if (node && node->tag->ndef->lops && node->tag->ndef->lops->constvalof) {
		r = node->tag->ndef->lops->constvalof (node->tag->ndef->lops, node, ptr);
	} else {
		tnode_warning (node, "extracting non-existant constant value!");
	}

	return r;
}
/*}}}*/
/*{{{  int langops_valbyref (tnode_t *node)*/
/*
 *	returns non-zero if VALs of this node (type/constant) are treated as references
 */
int langops_valbyref (tnode_t *node)
{
	int r = 0;

	if (node && node->tag->ndef->lops && node->tag->ndef->lops->valbyref) {
		r = node->tag->ndef->lops->valbyref (node->tag->ndef->lops, node);
	}

	return r;
}
/*}}}*/
/*{{{  static int langops_iscomplex_prewalk (tnode_t *t, void *arg)*/
/*
 *	helper for langops_iscomplex()
 *	returns 0 to stop walk, 1 to continue
 */
static int langops_iscomplex_prewalk (tnode_t *t, void *arg)
{
	int *r = (int *)arg;

	if (t && t->tag->ndef->lops && t->tag->ndef->lops->iscomplex) {
		*r = t->tag->ndef->lops->iscomplex (t->tag->ndef->lops, t, 1);
		return 0;
	}
	return 1;
}
/*}}}*/
/*{{{  int langops_iscomplex (tnode_t *node, int deep)*/
/*
 *	returns non-zero if a node is complex (used largely by fetrans for
 *	simplifying expressions).
 */
int langops_iscomplex (tnode_t *node, int deep)
{
	int r = -1;

	if (node && node->tag->ndef->lops && node->tag->ndef->lops->iscomplex) {
		r = node->tag->ndef->lops->iscomplex (node->tag->ndef->lops, node, deep);
	} else if (node && deep) {
		r = 0;
		tnode_prewalktree (node, langops_iscomplex_prewalk, &r);
	}
	return r;
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


