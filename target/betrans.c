/*
 *	betrans.c -- back-end tree transforms
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
#include "tnode.h"
#include "codegen.h"
#include "target.h"
#include "betrans.h"


/*}}}*/
/*{{{  private things*/
static chook_t *betranstaghook = NULL;
static chook_t *betransnodehook = NULL;

/*}}}*/


/*{{{  static void betrans_isetindent (int indent, FILE *stream)*/
/*
 *	sets indentation for output
 */
static void betrans_isetindent (int indent, FILE *stream)
{
	int i;

	for (i=0; i<indent; i++) {
		fprintf (stream, "    ");
	}
	return;
}
/*}}}*/


/*{{{  betrans compiler-hook routines*/
/*{{{  static void *betrans_taghook_copy (void *hook)*/
/*
 *	copies a tag hook
 */
static void *betrans_taghook_copy (void *hook)
{
	betranstag_t *tag = (betranstag_t *)hook;
	betranstag_t *ntag = NULL;

	if (tag) {
		ntag = (betranstag_t *)smalloc (sizeof (tag));
		ntag->flag = tag->flag;
		ntag->val = tag->val;
	}

	return (void *)ntag;
}
/*}}}*/
/*{{{  static void betrans_taghook_free (void *hook)*/
/*
 *	frees a tag hook
 */
static void betrans_taghook_free (void *hook)
{
	betranstag_t *tag = (betranstag_t *)hook;

	if (tag) {
		sfree (tag);
	}
	return;
}
/*}}}*/
/*{{{  static void betrans_taghook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	dumps a tag hook (debugging)
 */
static void betrans_taghook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	betranstag_t *tag = (betranstag_t *)hook;

	betrans_isetindent (indent, stream);
	fprintf (stream, "<betrans:tag flag=\"%s\" val=\"%d\" />\n", (tag && tag->flag) ? tag->flag->name : "", tag ? tag->val : 0);

	return;
}
/*}}}*/

/*{{{  static void *betrans_nodehook_copy (void *hook)*/
/*
 *	copies a node hook
 */
static void *betrans_nodehook_copy (void *hook)
{
	tnode_t *node = (tnode_t *)hook;

	if (node) {
		return (void *)tnode_copytree (node);
	}
	return NULL;
}
/*}}}*/
/*{{{  static void betrans_nodehook_free (void *hook)*/
/*
 *	frees a node hook
 */
static void betrans_nodehook_free (void *hook)
{
	tnode_t *node = (tnode_t *)hook;

	if (node) {
		tnode_warning (node, "betrans_nodehook_free(): not freeing this hook!");
	}
	return;
}
/*}}}*/
/*{{{  static void betrans_nodehook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	dumps a node-hook (debugging)
 */
static void betrans_nodehook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	tnode_t *hnode = (tnode_t *)hook;

	betrans_isetindent (indent, stream);
	fprintf (stream, "<betrans:node addr=\"0x%8.8x\">\n", (unsigned int)hnode);

	tnode_dumptree (hnode, indent+1, stream);

	betrans_isetindent (indent, stream);
	fprintf (stream, "</betrans:node>\n");

	return;
}
/*}}}*/
/*}}}*/


/*{{{  int betrans_init (void)*/
/*
 *	initialises back-end tree transforms
 *	returns 0 on success, non-zero on error
 */
int betrans_init (void)
{
	if (!betranstaghook) {
		betranstaghook = tnode_newchook ("betrans:tag");

		betranstaghook->chook_copy = betrans_taghook_copy;
		betranstaghook->chook_free = betrans_taghook_free;
		betranstaghook->chook_dumptree = betrans_taghook_dumptree;
	}
	if (!betransnodehook) {
		betransnodehook = tnode_newchook ("betrans:node");

		betransnodehook->chook_copy = betrans_nodehook_copy;
		betransnodehook->chook_free = betrans_nodehook_free;
		betransnodehook->chook_dumptree = betrans_nodehook_dumptree;
	}

	return 0;
}
/*}}}*/
/*{{{  int betrans_shutdown (void)*/
/*
 *	shuts-down back-end tree transforms
 *	returns 0 on success, non-zero on error
 */
int betrans_shutdown (void)
{
	return 0;
}
/*}}}*/


/*{{{  betranstag_t *betrans_newtag (ntdef_t *tag, int val)*/
/*
 *	creates a new betranstag_t compiler-hook node
 */
betranstag_t *betrans_newtag (ntdef_t *tag, int val)
{
	betranstag_t *btag = (betranstag_t *)smalloc (sizeof (betranstag_t));

	btag->flag = tag;
	btag->val = val;

	return btag;
}
/*}}}*/
/*{{{  void betrans_tagnode (tnode_t *t, ntdef_t *tag, int val, betrans_t *be)*/
/*
 *	creates a new betranstag_t compiler-hook and attaches it to the given node
 */
void betrans_tagnode (tnode_t *t, ntdef_t *tag, int val, betrans_t *be)
{
	betranstag_t *etag;

	etag = (betranstag_t *)tnode_getchook (t, be ? be->betranstaghook : betranstaghook);
	if (etag) {
		tnode_warning (t, "betrans_tagnode(): already tagged with [%s]", etag->flag ? etag->flag->name : "");
	} else {
		etag = betrans_newtag (tag, val);

		tnode_setchook (t, be ? be->betranstaghook : betranstaghook, etag);
	}
	return;
}
/*}}}*/
/*{{{  ntdef_t *betrans_gettag (tnode_t *t, int *valp, betrans_t *be)*/
/*
 *	returns the tag attached to a betrans:tag hook in the given node
 *	returns NULL if not here (or if null)
 */
ntdef_t *betrans_gettag (tnode_t *t, int *valp, betrans_t *be)
{
	betranstag_t *etag;

	etag = (betranstag_t *)tnode_getchook (t, be ? be->betranstaghook : betranstaghook);
	if (!etag) {
		return NULL;
	}
	
	if (valp) {
		*valp = etag->val;
	}

	return etag->flag;
}
/*}}}*/


/*{{{  static int betrans_modprewalk_tree (tnode_t **tptr, void *arg)*/
/*
 *	walks over a tree calling back-end transforms
 *	returns 0 to stop walk, 1 to continue
 */
static int betrans_modprewalk_tree (tnode_t **tptr, void *arg)
{
	int i = 1;

#if 0
fprintf (stderr, "betrans_modprewalk_tree(): on [%s]\n", *tptr ? (*tptr)->tag->name : "?");
if (*tptr) {
	fprintf (stderr, "                         : nodetype = [%s], ops = [0x%8.8x], betrans = [0x%8.8x]\n", (*tptr)->tag->ndef->name, (unsigned int)((*tptr)->tag->ndef->ops),
			((*tptr)->tag->ndef->ops) ? (unsigned int)((*tptr)->tag->ndef->ops->betrans) : 0);
}
#endif
	if (*tptr && (*tptr)->tag->ndef->ops && (*tptr)->tag->ndef->ops->betrans) {
		i = (*tptr)->tag->ndef->ops->betrans (tptr, (betrans_t *)arg);
	}
	return i;
}
/*}}}*/
/*{{{  int betrans_subtree (tnode_t **tptr, betrans_t *be)*/
/*
 *	does back-end tree transformations on the given sub-tree
 *	returns 0 on success, non-zero on error
 */
int betrans_subtree (tnode_t **tptr, betrans_t *be)
{
	tnode_modprewalktree (tptr, betrans_modprewalk_tree, (void *)be);

	return 0;
}
/*}}}*/
/*{{{  int betrans_tree (tnode_t **tptr, target_t *target)*/
/*
 *	does back-end tree transforms on the given tree
 *	returns 0 on success, non-zero on error
 */
int betrans_tree (tnode_t **tptr, target_t *target)
{
	betrans_t *be = (betrans_t *)smalloc (sizeof (betrans_t));

	be->insertpoint = NULL;
	be->target = target;
	be->betranstaghook = tnode_lookupornewchook ("betrans:tag");
	be->betransnodehook = tnode_lookupornewchook ("betrans:node");

	tnode_modprewalktree (tptr, betrans_modprewalk_tree, (void *)be);

	sfree (be);

	return 0;
}
/*}}}*/

