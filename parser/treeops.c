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


