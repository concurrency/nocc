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
#include "typecheck.h"
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


/*{{{  static void map_mapstate_freewalk (mapstate_t *ms, char *key, void *arg)*/
/*
 *	walks through items in the "mstate" field of a map_t structure freeing them
 */
static void map_mapstate_freewalk (mapstate_t *ms, char *key, void *arg)
{
	if (!ms) {
		return;
	}
	if (ms->id) {
		sfree (ms->id);
		ms->id = NULL;
	}
	sfree (ms);
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
	if ((*tptr)->tag->ndef->ops && tnode_hascompop_i ((*tptr)->tag->ndef->ops, (int)COPS_NAMEMAP)) {
#if 0
fprintf (stderr, "map_modprewalk_mapnames(): calling on node [%s:%s]\n", (*tptr)->tag->ndef->name, (*tptr)->tag->name);
#endif
		i = tnode_callcompop_i ((*tptr)->tag->ndef->ops, (int)COPS_NAMEMAP, 2, tptr, mdata);
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
	if ((*tptr)->tag->ndef->ops && tnode_hascompop_i ((*tptr)->tag->ndef->ops, (int)COPS_PREMAP)) {
		i = tnode_callcompop_i ((*tptr)->tag->ndef->ops, (int)COPS_PREMAP, 2, tptr, mdata);
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
	if ((*tptr)->tag->ndef->ops && tnode_hascompop_i ((*tptr)->tag->ndef->ops, (int)COPS_BEMAP)) {
		i = tnode_callcompop_i ((*tptr)->tag->ndef->ops, (int)COPS_BEMAP, 2, tptr, mdata);
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
	if (mdata->target && mdata->target->be_do_namemap) {
		mdata->target->be_do_namemap (tptr, mdata);
	} else {
		tnode_modprewalktree (tptr, map_modprewalk_mapnames, (void *)mdata);
	}

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
	if (mdata->target && mdata->target->be_do_premap) {
		mdata->target->be_do_premap (tptr, mdata);
	} else {
		tnode_modprewalktree (tptr, map_modprewalk_premap, (void *)mdata);
	}

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
	if (mdata->target && mdata->target->be_do_bemap) {
		mdata->target->be_do_bemap (tptr, mdata);
	} else {
		tnode_modprewalktree (tptr, map_modprewalk_bemap, (void *)mdata);
	}

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
	mdata->hook = NULL;
	dynarray_init (mdata->thisblock);
	dynarray_init (mdata->thisprocparams);
	mdata->thisberesult = NULL;
	mdata->mapchook = tnode_lookupornewchook ("map:mapnames");
	mdata->mapchook->chook_dumptree = map_namemap_chook_dumptree;
	mdata->allocevhook = tnode_lookupornewchook ("alloc:extravars");
	mdata->precodehook = tnode_lookupornewchook ("precode:vars");
	mdata->inparamlist = 0;
	stringhash_init (mdata->mstate, MAP_MAPSTATE_BITSIZE);

	/* do pre-mapping then real mapping */
	if (target->be_do_premap) {
		target->be_do_premap (tptr, mdata);
	} else {
		tnode_modprewalktree (tptr, map_modprewalk_premap, (void *)mdata);
	}

	if (target->be_do_namemap) {
		target->be_do_namemap (tptr, mdata);
	} else {
		tnode_modprewalktree (tptr, map_modprewalk_mapnames, (void *)mdata);
	}

	if (target->be_do_bemap) {
		target->be_do_bemap (tptr, mdata);
	} else {
		tnode_modprewalktree (tptr, map_modprewalk_bemap, (void *)mdata);
	}

	i = mdata->err;

	stringhash_walk (mdata->mstate, map_mapstate_freewalk, NULL);
	stringhash_trash (mdata->mstate);
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


/*{{{  int map_pushlexlevel (map_t *map, tnode_t *thisblock, tnode_t **thisprocparams)*/
/*
 *	pushes the mapping lex-level down and adds the given block and params-ptr
 *	returns new lex-level
 */
int map_pushlexlevel (map_t *map, tnode_t *thisblock, tnode_t **thisprocparams)
{
	int i;

	map->lexlevel++;
	while (DA_CUR (map->thisblock) <= (map->lexlevel + 1)) {
		/* adds NULLs to current level */
		dynarray_add (map->thisblock, NULL);
		dynarray_add (map->thisprocparams, NULL);
	}
	DA_SETNTHITEM (map->thisblock, map->lexlevel, thisblock);
	DA_SETNTHITEM (map->thisprocparams, map->lexlevel, thisprocparams);

	return map->lexlevel;
}
/*}}}*/
/*{{{  int map_poplexlevel (map_t *map)*/
/*
 *	pops the mapping lex-level (up)
 *	returns the new lex-level
 */
int map_poplexlevel (map_t *map)
{
	if (map->lexlevel <= 0) {
		nocc_internal ("map_poplexlevel(): at bottom!");
	}
	if (map->lexlevel >= DA_CUR (map->thisblock)) {
		nocc_internal ("map_poplexlevel(): at level %d, but only got %d items!", map->lexlevel, DA_CUR (map->thisblock));
	}
	DA_SETNTHITEM (map->thisblock, map->lexlevel, NULL);
	DA_SETNTHITEM (map->thisprocparams, map->lexlevel, NULL);
	map->lexlevel--;

	return map->lexlevel;
}
/*}}}*/
/*{{{  tnode_t *map_thisblock_cll (map_t *mdata)*/
/*
 *	returns the 'thisblock' pointer for the current lex-level
 */
tnode_t *map_thisblock_cll (map_t *mdata)
{
	if (mdata->lexlevel < 0) {
		nocc_internal ("map_thisblock_cll: at bottom!");
	}
	if (mdata->lexlevel >= DA_CUR (mdata->thisblock)) {
		nocc_internal ("map_thisblock_cll(): at level %d, but only got %d items!", mdata->lexlevel, DA_CUR (mdata->thisblock));
	}
	return DA_NTHITEM (mdata->thisblock, mdata->lexlevel);
}
/*}}}*/
/*{{{  tnode_t **map_thisprocparams_cll (map_t *mdata)*/
/*
 *	returns the 'thisprocparams' pointer for the current lex-level
 */
tnode_t **map_thisprocparams_cll (map_t *mdata)
{
	if (mdata->lexlevel < 0) {
		nocc_internal ("map_thisprocparams_cll: at bottom!");
	}
	if (mdata->lexlevel >= DA_CUR (mdata->thisprocparams)) {
		nocc_internal ("map_thisprocparams_cll(): at level %d, but only got %d items!", mdata->lexlevel, DA_CUR (mdata->thisblock));
	}
	return DA_NTHITEM (mdata->thisprocparams, mdata->lexlevel);
}
/*}}}*/
/*{{{  tnode_t *map_thisblock_ll (map_t *mdata, int lexlevel)*/
/*
 *	returns the 'thisblock' pointer for a particular lex-level
 */
tnode_t *map_thisblock_ll (map_t *mdata, int lexlevel)
{
	if ((lexlevel < 0) || (lexlevel > mdata->lexlevel)) {
		nocc_internal ("map_thisblock_ll: lexlevel out of range!");
	}
	if (mdata->lexlevel >= DA_CUR (mdata->thisblock)) {
		nocc_internal ("map_thisblock_ll(): at level %d, but only got %d items!", mdata->lexlevel, DA_CUR (mdata->thisblock));
	}
	return DA_NTHITEM (mdata->thisblock, lexlevel);
}
/*}}}*/
/*{{{  tnode_t **map_thisprocparams_ll (map_t *mdata, int lexlevel)*/
/*
 *	returns the 'thisprocparams' pointer for a particular lex-level
 */
tnode_t **map_thisprocparams_ll (map_t *mdata, int lexlevel)
{
	if ((lexlevel < 0) || (lexlevel > mdata->lexlevel)) {
		nocc_internal ("map_thisprocparams_ll: lexlevel out of range!");
	}
	if (mdata->lexlevel >= DA_CUR (mdata->thisprocparams)) {
		nocc_internal ("map_thisprocparams_cll(): at level %d, but only got %d items!", mdata->lexlevel, DA_CUR (mdata->thisblock));
	}
	return DA_NTHITEM (mdata->thisprocparams, lexlevel);
}
/*}}}*/

/*{{{  int map_hasstate (map_t *mdata, const char *id)*/
/*
 *	tests to see whether a mapping structure has some particular state in it
 *	returns truth value
 */
int map_hasstate (map_t *mdata, const char *id)
{
	mapstate_t *ms = stringhash_lookup (mdata->mstate, id);

	if (ms) {
		return 1;
	}
	return 0;
}
/*}}}*/
/*{{{  void *map_getstate (map_t *mdata, const char *id)*/
/*
 *	retrieves some state attached to a mapping structure
 *	returns value on success, NULL on failure (may be legitimate value)
 */
void *map_getstate (map_t *mdata, const char *id)
{
	mapstate_t *ms = stringhash_lookup (mdata->mstate, id);

	if (!ms) {
		return NULL;
	}
	return ms->ptr;
}
/*}}}*/
/*{{{  int map_setstate (map_t *mdata, const char *id, void *data)*/
/*
 *	sets some state in a mapping structure
 *	returns 1 if some existing state was replaced, 0 otherwise
 */
int map_setstate (map_t *mdata, const char *id, void *data)
{
	mapstate_t *ms = stringhash_lookup (mdata->mstate, id);
	int r = 0;

	if (!ms) {
		r = 1;
		ms = (mapstate_t *)smalloc (sizeof (mapstate_t));
		ms->id = string_dup (id);
		stringhash_insert (mdata->mstate, ms, ms->id);
	}
	ms->ptr = data;
	return r;
}
/*}}}*/
/*{{{  void map_clearstate (map_t *mdata, const char *id)*/
/*
 *	clears some state from a mapping structure
 */
void map_clearstate (map_t *mdata, const char *id)
{
	mapstate_t *ms = stringhash_lookup (mdata->mstate, id);

	if (ms) {
		stringhash_remove (mdata->mstate, ms, ms->id);
		sfree (ms->id);
		sfree (ms);
	}
	return;
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



