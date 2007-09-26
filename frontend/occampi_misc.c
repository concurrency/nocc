/*
 *	occampi_misc.c - miscellaneous things for occam-pi
 *	Copyright (C) 2006 Fred Barnes <frmb@kent.ac.uk>
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
#include "origin.h"
#include "parser.h"
#include "fcnlib.h"
#include "langdef.h"
#include "dfa.h"
#include "parsepriv.h"
#include "occampi.h"
#include "feunit.h"
#include "names.h"
#include "scope.h"
#include "prescope.h"
#include "typecheck.h"
#include "betrans.h"
#include "langops.h"
#include "target.h"
#include "map.h"
#include "transputer.h"
#include "codegen.h"


/*}}}*/
/*{{{  private stuff*/

static chook_t *miscstring_chook = NULL;
static chook_t *miscmetadata_chook = NULL;
static chook_t *miscmetadatalist_chook = NULL;


/*}}}*/


/*{{{  static opi_metadata_t *new_miscmetadata (char *name, char *data)*/
/*
 *	creates a popualted metadata structure
 */
static opi_metadata_t *new_miscmetadata (char *name, char *data)
{
	opi_metadata_t *omd = (opi_metadata_t *)smalloc (sizeof (opi_metadata_t));

	omd->name = string_dup (name);
	omd->data = string_dup (data);

	return omd;
}
/*}}}*/
/*{{{  static void free_miscmetadata (opi_metadata_t *md)*/
/*
 *	destroys a metadata structure
 */
static void free_miscmetadata (opi_metadata_t *md)
{
	if (!md) {
		nocc_internal ("free_miscmetadata(): NULL hook!");
	}

	if (md->name) {
		sfree (md->name);
	}
	if (md->data) {
		sfree (md->data);
	}
	sfree (md);
}
/*}}}*/
/*{{{  static opi_metadatalist_t *new_miscmetadatalist (void)*/
/*
 *	creates a blank metadatalist structure
 */
static opi_metadatalist_t *new_miscmetadatalist (void)
{
	opi_metadatalist_t *mdl = (opi_metadatalist_t *)smalloc (sizeof (opi_metadatalist_t));

	dynarray_init (mdl->items);

	return mdl;
}
/*}}}*/
/*{{{  static void free_miscmetadatalist (opi_metadatalist_t *mdl)*/
/*
 *	destroys a metadatalist structure (deep)
 */
static void free_miscmetadatalist (opi_metadatalist_t *mdl)
{
	int i;

	if (!mdl) {
		nocc_internal ("free_miscmetadatalist(): NULL hook!");
	}
	for (i=0; i<DA_CUR (mdl->items); i++) {
		opi_metadata_t *mdi = DA_NTHITEM (mdl->items, i);

		if (mdi) {
			free_miscmetadata (mdi);
		}
	}
	dynarray_trash (mdl->items);
	sfree (mdl);
}
/*}}}*/


/*{{{  static void *miscstringhook_copy (void *hook)*/
/*
 *	duplicates a "misc:string" compiler hook
 */
static void *miscstringhook_copy (void *hook)
{
	if (hook) {
		return (void *)string_dup ((char *)hook);
	}
	return NULL;
}
/*}}}*/
/*{{{  static void miscstringhook_free (void *hook)*/
/*
 *	frees a "misc:string" compiler hook
 */
static void miscstringhook_free (void *hook)
{
	if (hook) {
		sfree (hook);
	}
	return;
}
/*}}}*/
/*{{{  static void miscstringhook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	dumps a "misc:string" compiler hook (debugging)
 */
static void miscstringhook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	occampi_isetindent (stream, indent);
	fprintf (stream, "<chook id=\"misc:string\" value=\"%s\" />\n", hook ? (char *)hook : "");
	return;
}
/*}}}*/


/*{{{  static void *miscmetadatahook_copy (void *hook)*/
/*
 *	duplicates a "misc:metadata" compiler hook
 */
static void *miscmetadatahook_copy (void *hook)
{
	if (hook) {
		opi_metadata_t *md = (opi_metadata_t *)hook;

		return (void *)new_miscmetadata (md->name, md->data);
	}
	return NULL;
}
/*}}}*/
/*{{{  static void miscmetadatahook_free (void *hook)*/
/*
 *	frees a "misc:metadata" compiler hook
 */
static void miscmetadatahook_free (void *hook)
{
	if (hook) {
		free_miscmetadata ((opi_metadata_t *)hook);
	}
	return;
}
/*}}}*/
/*{{{  static void miscmetadatahook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	dumps a "misc:metadata" compiler hook (debugging)
 */
static void miscmetadatahook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	opi_metadata_t *omd = (opi_metadata_t *)hook;

	occampi_isetindent (stream, indent);
	if (!hook) {
		fprintf (stream, "<chook id=\"misc:metadata\" value=\"\" />\n");
	} else {
		fprintf (stream, "<chook id=\"misc:metadata\" name=\"%s\" data=\"%s\" />\n", omd->name, omd->data);
	}
}
/*}}}*/


/*{{{  static void *miscmetadatalisthook_copy (void *hook)*/
/*
 *	duplicates a "misc:metadatalist" compiler hook
 */
static void *miscmetadatalisthook_copy (void *hook)
{
	if (hook) {
		int i;
		opi_metadatalist_t *mdl = (opi_metadatalist_t *)hook;
		opi_metadatalist_t *newl = new_miscmetadatalist ();

		for (i=0; i<DA_CUR (mdl->items); i++) {
			opi_metadata_t *mdi = DA_NTHITEM (mdl->items, i);

			if (mdi) {
				void *copy = miscmetadatahook_copy ((void *)mdi);

				dynarray_add (newl->items, copy);
			}
		}

		return (void *)newl;
	}
	return NULL;
}
/*}}}*/
/*{{{  static void miscmetadatalisthook_free (void *hook)*/
/*
 *	frees a "misc:metadatalist" compiler hook
 */
static void miscmetadatalisthook_free (void *hook)
{
	if (hook) {
		free_miscmetadatalist ((opi_metadatalist_t *)hook);
	}
	return;
}
/*}}}*/
/*{{{  static void miscmetadatalisthook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	dumps a "misc:metadatalist" compiler hook (debugging)
 */
static void miscmetadatalisthook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	opi_metadatalist_t *mdl = (opi_metadatalist_t *)hook;

	occampi_isetindent (stream, indent);
	if (!hook) {
		fprintf (stream, "<chook id=\"misc:metadatalist\" value=\"\" />\n");
	} else {
		int i;

		fprintf (stream, "<chook id=\"misc:metadatalist\" size=\"%d\">\n", DA_CUR (mdl->items));
		for (i=0; i<DA_CUR (mdl->items); i++) {
			miscmetadatahook_dumptree (node, (void *)DA_NTHITEM (mdl->items, i), indent + 1, stream);
		}
		occampi_isetindent (stream, indent);
		fprintf (stream, "</chook>\n");
	}
}
/*}}}*/


/*{{{  static int occampi_misc_prescope (compops_t *cops, tnode_t **tptr, prescope_t *ps)*/
/*
 *	does pre-scoping on a misc node
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_misc_prescope (compops_t *cops, tnode_t **tptr, prescope_t *ps)
{
	return 1;
}
/*}}}*/
/*{{{  static int occampi_misc_precode (compops_t *cops, tnode_t **tptr, codegen_t *cgen)*/
/*
 *	does pre-codegen on a misc node
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_misc_precode (compops_t *cops, tnode_t **tptr, codegen_t *cgen)
{
	return 1;
}
/*}}}*/
/*{{{  static int occampi_misc_namemap (compops_t *cops, tnode_t **tptr, map_t *map)*/
/*
 *	does name-mapping on a misc node
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_misc_namemap (compops_t *cops, tnode_t **tptr, map_t *map)
{
	return 1;
}
/*}}}*/
/*{{{  static int occampi_misc_codegen (compops_t *cops, tnode_t *tptr, codegen_t *cgen)*/
/*
 *	does code-generation for a misc node
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_misc_codegen (compops_t *cops, tnode_t *tptr, codegen_t *cgen)
{
	if (tptr->tag == opi.tag_MISCCOMMENT) {
		/*{{{  MISCCOMMENT -- drop TCOFF object*/
		char *comment = (char *)tnode_getchook (tptr, miscstring_chook);

		if (comment) {
			codegen_callops (cgen, tcoff, 20, comment, strlen (comment));
		}
		/*}}}*/
	}
	return 1;
}
/*}}}*/
/*{{{  static int occampi_misc_miscnodetrans (compops_t *cops, tnode_t **tptr, occampi_miscnodetrans_t *mnt)*/
/*
 *	called during the miscnodetrans pass (post-walk) to scoop up METADATA nodes (dropped on the PROCs)
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_misc_miscnodetrans (compops_t *cops, tnode_t **tptr, occampi_miscnodetrans_t *mnt)
{
	if ((*tptr)->tag == opi.tag_METADATA) {
		/* detatch */
		tnode_t *node = *tptr;
		tnode_t **bodyp = tnode_nthsubaddr (node, 0);

		*tptr = *bodyp;		/* replace with body */
		*bodyp = NULL;

		if (!mnt->md_node) {
			mnt->md_iptr = bodyp;
		} else {
			*bodyp = mnt->md_node;
		}
		mnt->md_node = node;
	}
	return 1;
}
/*}}}*/


/*{{{  static occampi_miscnodetrans_t *miscnode_newmiscnodetrans (void)*/
/*
 *	creates a new occampi_miscnodetrans_t structure
 */
static occampi_miscnodetrans_t *miscnode_newmiscnodetrans (void)
{
	occampi_miscnodetrans_t *mnt = (occampi_miscnodetrans_t *)smalloc (sizeof (occampi_miscnodetrans_t));

	mnt->md_node = NULL;
	mnt->md_iptr = NULL;
	mnt->error = 0;

	return mnt;
}
/*}}}*/
/*{{{  static void miscnode_freemiscnodetrans (occampi_miscnodetrans_t *mnt)*/
/*
 *	frees an occampi_miscnodetrans_t structure
 */
static void miscnode_freemiscnodetrans (occampi_miscnodetrans_t *mnt)
{
	if (!mnt) {
		nocc_warning ("miscnode_freemiscnodetrans(): erm, NULL pointer here!");
		return;
	}
	if (mnt->md_node) {
		nocc_warning ("miscnode_freemiscnodetrans(): still got a node hooked in here.. (0x%8.8x)", (unsigned int)mnt->md_node);
	}
	sfree (mnt);
	return;
}
/*}}}*/


/*{{{  static int miscnode_modprewalk (tnode_t **tptr, void *arg)*/
/*
 *	called during (pre) tree walk to handle certain types of data in MISC nodes
 *	returns 0 to stop walk, 1 to continue
 */
static int miscnode_modprewalk (tnode_t **tptr, void *arg)
{
	return 1;
}
/*}}}*/
/*{{{  static int miscnode_modprewalk (tnode_t **tptr, void *arg)*/
/*
 *	called during (post) tree walk to handle certain types of data in MISC nodes
 *	returns 0 to stop walk, 1 to continue
 */
static int miscnode_modpostwalk (tnode_t **tptr, void *arg)
{
	int i = 1;

	if (*tptr && (*tptr)->tag->ndef->ops && tnode_hascompop ((*tptr)->tag->ndef->ops, "miscnodetrans")) {
		i = tnode_callcompop ((*tptr)->tag->ndef->ops, "miscnodetrans", 2, tptr, (occampi_miscnodetrans_t *)arg);
	}
	return i;
}
/*}}}*/
/*{{{  int miscnode_transsubtree (tnode_t **tptr, occampi_miscnodetrans_t *mnt)*/
/*
 *	does misc-node transforms on a sub-tree
 *	returns 0 on success, non-zero on failure
 */
int miscnode_transsubtree (tnode_t **tptr, occampi_miscnodetrans_t *mnt)
{
	tnode_modprepostwalktree (tptr, miscnode_modprewalk, miscnode_modpostwalk, (void *)mnt);
	return mnt->error;
}
/*}}}*/
/*{{{  static int miscnodetrans_cpass (tnode_t *tree)*/
/*
 *	called for a compiler pass that pulls misc-data upwards in the tree
 *	returns 0 on success, non-zero on failure
 */
static int miscnodetrans_cpass (tnode_t *tree)
{
	occampi_miscnodetrans_t *mnt = miscnode_newmiscnodetrans ();
	int err = 0;

	miscnode_transsubtree (&tree, mnt);
	err = mnt->error;

	miscnode_freemiscnodetrans (mnt);
	return err;
}
/*}}}*/


/*{{{  static int occampi_misc_init_nodes (void)*/
/*
 *	initialises misc nodes for occam-pi
 *	return 0 on success, non-zero on error
 */
static int occampi_misc_init_nodes (void)
{
	int i;
	tndef_t *tnd;
	compops_t *cops;
	langops_t *lops;

	/*{{{  miscnodetrans -- new compiler pass and compiler operation*/
	if (nocc_addcompilerpass ("miscnodetrans", origin_langparser (&occampi_parser), "fetrans", 1, (int (*)(void *))miscnodetrans_cpass, CPASS_TREE, -1, NULL)) {
		nocc_internal ("occampi_misc_init_nodes(): failed to add miscnodetrans compiler pass");
		return -1;
	}

	tnode_newcompop ("miscnodetrans", COPS_INVALID, 2, NULL);

	/*}}}*/
	/*{{{  misc:string compiler-hook*/
	miscstring_chook = tnode_lookupornewchook ("misc:string");

	miscstring_chook->chook_copy = miscstringhook_copy;
	miscstring_chook->chook_free = miscstringhook_free;
	miscstring_chook->chook_dumptree = miscstringhook_dumptree;

	/*}}}*/
	/*{{{  misc:metadata compiler-hook*/
	miscmetadata_chook = tnode_lookupornewchook ("misc:metadata");

	miscmetadata_chook->chook_copy = miscmetadatahook_copy;
	miscmetadata_chook->chook_free = miscmetadatahook_free;
	miscmetadata_chook->chook_dumptree = miscmetadatahook_dumptree;

	/*}}}*/
	/*{{{  misc:metadatalist compiler-hook*/
	miscmetadatalist_chook = tnode_lookupornewchook ("misc:metadatalist");

	miscmetadatalist_chook->chook_copy = miscmetadatalisthook_copy;
	miscmetadatalist_chook->chook_free = miscmetadatalisthook_free;
	miscmetadatalist_chook->chook_dumptree = miscmetadatalisthook_dumptree;

	/*}}}*/
	/*{{{  occampi:miscnode -- MISCCOMMENT*/
	i = -1;
	tnd = opi.node_MISCNODE = tnode_newnodetype ("occampi:miscnode", &i, 1, 0, 0, TNF_TRANSPARENT);			/* subnodes: 0 = body */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "prescope", 2, COMPOPTYPE (occampi_misc_prescope));
	tnode_setcompop (cops, "precode", 2, COMPOPTYPE (occampi_misc_precode));
	tnode_setcompop (cops, "miscnodetrans", 2, COMPOPTYPE (occampi_misc_miscnodetrans));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (occampi_misc_namemap));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (occampi_misc_codegen));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnd->lops = lops;

	i = -1;
	opi.tag_MISCCOMMENT = tnode_newnodetag ("MISCCOMMENT", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_METADATA = tnode_newnodetag ("METADATA", &i, tnd, NTF_NONE);

	/*}}}*/

	return 0;
}
/*}}}*/
/*{{{  static int occampi_misc_post_setup (void)*/
/*
 *	does post-setup for misc nodes
 *	returns 0 on success, non-zero on failure
 */
static int occampi_misc_post_setup (void)
{
	fcnlib_addfcn ("new_miscmetadata", (void *)new_miscmetadata, 1, 2);
	fcnlib_addfcn ("new_miscmetadatalist", (void *)new_miscmetadatalist, 1, 0);
	return 0;
}
/*}}}*/


/*{{{  occampi_misc_feunit (feunit_t struct)*/
feunit_t occampi_misc_feunit = {
	init_nodes: occampi_misc_init_nodes,
	reg_reducers: NULL,
	init_dfatrans: NULL,
	post_setup: occampi_misc_post_setup,
	ident: "occampi-misc"
};
/*}}}*/

