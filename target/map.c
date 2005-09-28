/*
 *	map.c -- memory allocator for NOCC
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
#include "tnode.h"
#include "names.h"
#include "map.h"
#include "target.h"

/*}}}*/


/*{{{  void map_isetindent (FILE *stream, int indent)*/
/*
 *	set-indent for debugging output
 */
void map_isetindent (FILE *stream, int indent)
{
	int i;

	for (i=0; i<indent; i++) {
		fprintf (stream, "    ");
	}
	return;
}
/*}}}*/


/*{{{  static int map_modprewalk_mapnames (tnode_t **tptr, void *arg)*/
/*
 *	private function that walks over nodes mapping names,
 *	returns 0 to stop walk, 1 to continue
 */
static int map_modprewalk_mapnames (tnode_t **tptr, void *arg)
{
	map_t *mdata = (map_t *)arg;
	int i;

	if (!tptr || !*tptr) {
		nocc_warning ("map_modprewalk_mapnames(): null pointer or node!");
		mdata->warn++;
		return 0;
	}

	i = 1;
	if ((*tptr)->tag->ndef->ops && (*tptr)->tag->ndef->ops->namemap) {
		i = (*tptr)->tag->ndef->ops->namemap (tptr, mdata);
	}
	return i;
}
/*}}}*/
/*{{{  static int map_modprewalk_premap (tnode_t **tptr, void *arg)*/
/*
 *	private function that walks over nodes doing pre-mapping.
 *	returns 0 to stop walk, 1 to continue
 */
static int map_modprewalk_premap (tnode_t **tptr, void *arg)
{
	map_t *mdata = (map_t *)arg;
	int i;

	if (!tptr || !*tptr) {
		nocc_warning ("map_modprewalk_premap(): null pointer or node!");
		mdata->warn++;
		return 0;
	}
	i = 1;
	if ((*tptr)->tag->ndef->ops && (*tptr)->tag->ndef->ops->premap) {
		i = (*tptr)->tag->ndef->ops->premap (tptr, mdata);
	}
	return i;
}
/*}}}*/
/*{{{  static int map_modprewalk_bemap (tnode_t **tptr, void *arg)*/
/*
 *	walks over nodes doing back-end mapping
 *	returns 0 to stop walk, 1 to continue
 */
static int map_modprewalk_bemap (tnode_t **tptr, void *arg)
{
	map_t *mdata = (map_t *)arg;
	int i;

	if (!tptr || !*tptr) {
		nocc_warning ("map_modprewalk_bemap(): null pointer or node!");
		mdata->warn++;
	}
	i = 1;
	if ((*tptr)->tag->ndef->ops && (*tptr)->tag->ndef->ops->bemap) {
		i = (*tptr)->tag->ndef->ops->bemap (tptr, mdata);
	}
	return i;
}
/*}}}*/
/*{{{  static void map_namemap_chook_dumptree (tnode_t *node, void *chook, int indent, FILE *stream)*/
/*
 *	displays the contents of a compiler-hook -- actually a back-end name-node
 */
static void map_namemap_chook_dumptree (tnode_t *node, void *chook, int indent, FILE *stream)
{
	if (chook) {
		map_isetindent (stream, indent);
		fprintf (stream, "<chook:map:mapnames addr=\"0x%8.8x\" />\n", (unsigned int)chook);
	}
	return;
}
/*}}}*/
/*{{{  int map_submapnames (tnode_t **tptr, map_t *mdata)*/
/*
 *	transforms the given tree, turning source name-nodes into back-end name-nodes (inner function)
 *	return 0 on success, non-zero on error
 */
int map_submapnames (tnode_t **tptr, map_t *mdata)
{
	tnode_modprewalktree (tptr, map_modprewalk_mapnames, (void *)mdata);

	return 0;		/* this always succeeds.. */
}
/*}}}*/
/*{{{  int map_subpremap (tnode_t **tptr, map_t *mdata)*/
/*
 *	pre-maps the given sub-tree
 *	returns 0 on success, non-zero on error
 */
int map_subpremap (tnode_t **tptr, map_t *mdata)
{
	tnode_modprewalktree (tptr, map_modprewalk_premap, (void *)mdata);

	return 0;		/* this always succeeds.. */
}
/*}}}*/
/*{{{  int map_subbemap (tnode_t **tptr, map_t *mdata)*/
/*
 *	back-end-maps the given sub-tree
 *	returns 0 on success, non-zero on error
 */
int map_subbemap (tnode_t **tptr, map_t *mdata)
{
	tnode_modprewalktree (tptr, map_modprewalk_bemap, (void *)mdata);

	return 0;		/* this always succeeds.. */
}
/*}}}*/
/*{{{  int map_mapnames (tnode_t **tptr, target_t *target)*/
/*
 *	transforms the given tree, turning source name-nodes into back-end name-nodes
 *	returns 0 on success, non-zero on error
 */
int map_mapnames (tnode_t **tptr, target_t *target)
{
	map_t *mdata = (map_t *)smalloc (sizeof (map_t));
	int i;

	mdata->lexlevel = 0;
	mdata->target = target;
	mdata->err = 0;
	mdata->warn = 0;
	mdata->thisblock = NULL;
	mdata->thisprocparams = NULL;
	mdata->thisberesult = NULL;
	mdata->mapchook = tnode_lookupornewchook ("map:mapnames");
	mdata->mapchook->chook_dumptree = map_namemap_chook_dumptree;
	mdata->allocevhook = tnode_lookupornewchook ("alloc:extravars");
	mdata->precodehook = tnode_lookupornewchook ("precode:vars");

	/* do pre-mapping then real mapping */
	tnode_modprewalktree (tptr, map_modprewalk_premap, (void *)mdata);
	tnode_modprewalktree (tptr, map_modprewalk_mapnames, (void *)mdata);
	tnode_modprewalktree (tptr, map_modprewalk_bemap, (void *)mdata);

	i = mdata->err;
	sfree (mdata);

	return i;
}
/*}}}*/
/*{{{  int map_addtoresult (tnode_t **nodep, map_t *mdata)*/
/*
 *	adds the given back-end node reference to a sub-list in a back-end result
 *	returns 0 on success, non-zero on error
 */
int map_addtoresult (tnode_t **nodep, map_t *mdata)
{
	if (mdata->thisberesult) {
		mdata->target->inresult (nodep, mdata);
	}
	return 0;
}
/*}}}*/


/*{{{  int map_init (void)*/
/*
 *	initialises the mapper
 *	return 0 on success, non-zero on error
 */
int map_init (void)
{
	return 0;
}
/*}}}*/
/*{{{  int map_shutdown (void)*/
/*
 *	shuts down the mapper
 *	returns 0 on success, non-zero on error
 */
int map_shutdown (void)
{
	return 0;
}
/*}}}*/



