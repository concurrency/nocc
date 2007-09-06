/*
 *	treeops.c -- tree operations for nocc
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
#include "treeops.h"

/*}}}*/
/*{{{  comment -- regular expressions*/
/*
 *	these only match on node-tags or node-types, e.g.:
 *
 *	"occampi:decl (*, INT, *)" will match integer declaration nodes
 */
/*}}}*/
/*{{{  local things*/
typedef struct TAG_topsmap {
	POINTERHASH (tnode_t **, tagmap, 6);
	POINTERHASH (tnode_t **, typemap, 4);

} topsmap_t;


typedef struct TAG_topsmatch {
	void *tagmatch, *typematch;		/* tag and/or type match */
	struct TAG_topsmatch **substruct;	/* sub-structure match, "x ( y z )" */
	struct TAG_topsmatch *neststruct;	/* nested structure match, "x > z" */
} topsmatch_t;

/*}}}*/


/*{{{  static topsmap_t *tops_newmap (void)*/
/*
 *	creates a new topsmap_t structure
 */
static topsmap_t *tops_newmap (void)
{
	topsmap_t *tmap = (topsmap_t *)smalloc (sizeof (tmap));

	pointerhash_init (tmap->tagmap, 6);
	pointerhash_init (tmap->typemap, 4);

	return tmap;
}
/*}}}*/
/*{{{  static void tops_freemap (topsmap_t *tmap)*/
/*
 *	destroys a topsmap_t structure
 */
static void tops_freemap (topsmap_t *tmap)
{
	pointerhash_trash (tmap->tagmap);
	pointerhash_trash (tmap->typemap);
	return;
}
/*}}}*/
/*{{{  static topsmatch_t *tops_newmatch (void)*/
/*
 *	creates a new topsmatch_t structure
 */
static topsmatch_t *tops_newmatch (void)
{
	topsmatch_t *tmatch = (topsmatch_t *)smalloc (sizeof (topsmatch_t));

	tmatch->tagmatch = NULL;
	tmatch->typematch = NULL;
	tmatch->substruct = NULL;
	tmatch->neststruct = NULL;

	return tmatch;
}
/*}}}*/
/*{{{  static void tops_freematch (topsmatch_t *tmatch)*/
/*
 *	frees a topsmatch_t structure (recursively)
 */
static void tops_freematch (topsmatch_t *tmatch)
{
	if (tmatch->neststruct) {
		tops_freematch (tmatch->neststruct);
	}
	if (tmatch->substruct) {
		int i;

		for (i=0; tmatch->substruct[i]; i++) {
			tops_freematch (tmatch->substruct[i]);
		}
		sfree (tmatch->substruct);
	}
	sfree (tmatch);

	return;
}
/*}}}*/
/*{{{  static int tops_modprewalktree (tnode_t **tptr, void *arg)*/
/*
 *	tree traversal function, adds treenodes to given map
 *	returns 1 to continue walk, 0 to stop
 */
static int tops_modprewalktree (tnode_t **tptr, void *arg)
{
	topsmap_t *tmap = (topsmap_t *)arg;

	pointerhash_insert (tmap->tagmap, tptr, (void *)((*tptr)->tag));
	pointerhash_insert (tmap->typemap, tptr, (void *)((*tptr)->tag->ndef));

	return 1;
}
/*}}}*/
/*{{{  static topsmap_t *tops_makemap (tnode_t **tptr)*/
/*
 *	traverses the given tree and produces a tree-ops map for it
 */
static topsmap_t *tops_makemap (tnode_t **tptr)
{
	topsmap_t *tmap = tops_newmap ();

	tnode_modprewalktree (tptr, tops_modprewalktree, (void *)tmap);

	return tmap;
}
/*}}}*/


/*{{{  tnode_t *treeops_findintree (tnode_t *tree, ntdef_t *tag)*/
/*
 *	finds a sub-tree within a tree with the given node tag
 */
tnode_t *treeops_findintree (tnode_t *tree, ntdef_t *tag)
{
	int i, nnodes;
	tnode_t **subnodes;

	if (!tree) {
		return NULL;
	}
	if (tree->tag == tag) {
		return tree;
	}
	if (parser_islistnode (tree)) {
		subnodes = parser_getlistitems (tree, &nnodes);
	} else {
		subnodes = tnode_subnodesof (tree, &nnodes);
	}
#if 0
fprintf (stderr, "searching [%s] for [%s], %d subnodes\n", tree->tag->name, tag->name, nnodes);
#endif
	for (i=0; i<nnodes; i++) {
		tnode_t *result = treeops_findintree (subnodes[i], tag);

		if (result) {
			return result;
		}
	}
	return NULL;
}
/*}}}*/
/*{{{  tnode_t **treeops_findintreeptr (tnode_t **tree, ntdef_t *tag)*/
/*
 *	finds a sub-tree within a tree with the given node tag
 *	returns a pointer to where that node is, rather than the node itself
 */
tnode_t **treeops_findintreeptr (tnode_t **tree, ntdef_t *tag)
{
	int i, nnodes;
	tnode_t **subnodes;

	if (!tree || !*tree) {
		return NULL;
	}
	if ((*tree)->tag == tag) {
		return tree;
	}
	if (parser_islistnode (*tree)) {
		subnodes = parser_getlistitems (*tree, &nnodes);
	} else {
		subnodes = tnode_subnodesof (*tree, &nnodes);
	}

	for (i=0; i<nnodes; i++) {
		tnode_t **result = treeops_findintreeptr (subnodes + i, tag);

		if (result) {
			return result;
		}
	}
	return NULL;
}
/*}}}*/
/*{{{  static tnode_t *tops_findtwointree (tnode_t *parent, tnode_t *tree, ntdef_t *tag1, ntdef_t *tag2)*/
/*
 *	local helper for finding parent/child trees with given tags
 */
static tnode_t *tops_findtwointree (tnode_t *parent, tnode_t *tree, ntdef_t *tag1, ntdef_t *tag2)
{
	int i, nnodes;
	tnode_t **subnodes;

	if (!parent || !tree) {
		return NULL;
	}
	if ((parent->tag == tag1) && (tree->tag == tag2)) {
		return parent;
	}

	if (parser_islistnode (tree)) {
		subnodes = parser_getlistitems (tree, &nnodes);
	} else {
		subnodes = tnode_subnodesof (tree, &nnodes);
	}

	for (i=0; i<nnodes; i++) {
		tnode_t *result = tops_findtwointree (tree, subnodes[i], tag1, tag2);

		if (result) {
			return result;
		}
	}
	return NULL;
}
/*}}}*/
/*{{{  tnode_t *treeops_findtwointree (tnode_t *tree, ntdef_t *tag1, ntdef_t *tag2)*/
/*
 *	finds a sub-tree within a tree with the given node tag, and that has a child with the
 *	second node tag.
 */
tnode_t *treeops_findtwointree (tnode_t *tree, ntdef_t *tag1, ntdef_t *tag2)
{
	int i, nnodes;
	tnode_t **subnodes;

	if (parser_islistnode (tree)) {
		subnodes = parser_getlistitems (tree, &nnodes);
	} else {
		subnodes = tnode_subnodesof (tree, &nnodes);
	}
#if 0
fprintf (stderr, "searching [%s] for [%s]->[%s], %d subnodes\n", tree->tag->name, tag1->name, tag2->name, nnodes);
#endif
	for (i=0; i<nnodes; i++) {
		tnode_t *result = tops_findtwointree (tree, subnodes[i], tag1, tag2);

		if (result) {
			return result;
		}
	}
	return NULL;
}
/*}}}*/
/*{{{  tnode_t *treeops_findprocess (tnode_t *tree)*/
/*
 *	walks through declarations until the next process is found
 */
tnode_t *treeops_findprocess (tnode_t *tree)
{
	for (; tree;) {
		int flags = tnode_tnflagsof (tree);

		if (flags & TNF_LONGDECL) {
			tree = tnode_nthsubof (tree, 3);
		} else if (flags & TNF_SHORTDECL) {
			tree = tnode_nthsubof (tree, 2);
		} else if (flags & TNF_TRANSPARENT) {
			tree = tnode_nthsubof (tree, 0);
		} else {
			/* assume process! */
			return tree;
		}
	}
	return NULL;
}
/*}}}*/


/*{{{  tnode_t *treeops_transform (tnode_t *tree, ...)*/
/*
 *	used for re-writing bits of tree
 *	
 */
tnode_t *treeops_transform (tnode_t *tree, ...)
{
	tnode_t *result = NULL;
	va_list ap;

	va_start (ap, tree);
	/* FIXME ... */
	va_end (ap);

	return result;
}
/*}}}*/


/*{{{  int treeops_init (void)*/
/*
 *	initialises tree-operations
 *	returns 0 on success, non-zero on failure
 */
int treeops_init (void)
{
	return 0;
}
/*}}}*/
/*{{{  int treeops_shutdown (void)*/
/*
 *	shuts-down tree-operations
 *	returns 0 on success, non-zero on failure
 */
int treeops_shutdown (void)
{
	return 0;
}
/*}}}*/


