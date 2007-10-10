/*
 *	metadata.c -- separated meta-data handling for NOCC
 *	Copyright (C) 2007 Fred Barnes <frmb@kent.ac.uk>
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
 *	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
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
#include <errno.h>

#include "nocc.h"
#include "support.h"
#include "version.h"
#include "symbols.h"
#include "keywords.h"
#include "opts.h"
#include "tnode.h"
#include "crypto.h"
#include "treeops.h"
#include "xml.h"
#include "metadata.h"


/*}}}*/

/*{{{  private types*/

typedef struct TAG_md_reserved {
	char *name;
} md_reserved_t;

/*}}}*/
/*{{{  private data*/

static chook_t *metadata_chook = NULL;
static chook_t *metadatalist_chook = NULL;

#define MDRES_HASHBITSIZE (3)

STATICSTRINGHASH (md_reserved_t *, mdres, MDRES_HASHBITSIZE);

/*}}}*/


/*{{{  static void metadata_isetindent (FILE *stream, int indent)*/
/*
 *	set-indent for debugging output
 */
static void metadata_isetindent (FILE *stream, int indent)
{
	int i;

	for (i=0; i<indent; i++) {
		fprintf (stream, "    ");
	}
	return;
}
/*}}}*/


/*{{{  static md_reserved_t *metadata_newreserved (void)*/
/*
 *	creates a new md_reserved_t structure
 */
static md_reserved_t *metadata_newreserved (void)
{
	md_reserved_t *mdr = (md_reserved_t *)smalloc (sizeof (md_reserved_t));

	mdr->name = NULL;

	return mdr;
}
/*}}}*/
/*{{{  static void metadata_freereserved (md_reserved_t *mdr)*/
/*
 *	frees a md_reserved_t structure
 */
static void metadata_freereserved (md_reserved_t *mdr)
{
	if (!mdr) {
		nocc_internal ("metadata_freereserved(): null pointer!");
		return;
	}
	if (mdr->name) {
		sfree (mdr->name);
		mdr->name = NULL;
	}
	sfree (mdr);
	return;
}
/*}}}*/


/*{{{  int metadata_addreservedname (const char *name)*/
/*
 *	adds a reserved metadata name
 *	returns 0 on success, non-zero on failure
 */
int metadata_addreservedname (const char *name)
{
	md_reserved_t *mdr = stringhash_lookup (mdres, name);

	if (mdr) {
		nocc_warning ("metadata_addreservedname(): name \"%s\" already reserved", name);
		return -1;
	}
	mdr = metadata_newreserved ();
	mdr->name = string_dup (name);

	stringhash_insert (mdres, mdr, mdr->name);

	return 0;
}
/*}}}*/
/*{{{  int metadata_isreservedname (const char *name)*/
/*
 *	tests to see if a given metadata name is reserved
 *	returns truth value
 */
int metadata_isreservedname (const char *name)
{
	md_reserved_t *mdr = stringhash_lookup (mdres, name);

	if (mdr) {
		return 1;
	}
	return 0;
}
/*}}}*/
/*{{{  int metadata_fixreserved (metadata_t *md)*/
/*
 *	if the given metadata's name is reserved, fixes it up
 *	returns 0 if nothing changed, 1 if it did
 */
int metadata_fixreserved (metadata_t *md)
{
	if (metadata_isreservedname (md->name)) {
		char *newname = (char *)smalloc (strlen (md->name) + 4);

		sprintf (newname, "u:%s", md->name);
		sfree (md->name);
		md->name = newname;

		return 1;
	}
	return 0;
}
/*}}}*/


/*{{{  metadata_t *metadata_createmetadata (char *name, char *data)*/
/*
 *	creates a popualted metadata structure
 */
metadata_t *metadata_createmetadata (char *name, char *data)
{
	metadata_t *omd = (metadata_t *)smalloc (sizeof (metadata_t));

	omd->name = string_dup (name);
	omd->data = string_dup (data);

	return omd;
}
/*}}}*/
/*{{{  metadata_t *metadata_newmetadata (void)*/
/*
 *	creates a blank metadata structure
 */
metadata_t *metadata_newmetadata (void)
{
	metadata_t *omd = (metadata_t *)smalloc (sizeof (metadata_t));

	omd->name = NULL;
	omd->data = NULL;

	return omd;
}
/*}}}*/
/*{{{  metadata_t *metadata_copymetadata (metadata_t *md)*/
/*
 *	creates a blank metadata structure
 */
metadata_t *metadata_copymetadata (metadata_t *md)
{
	metadata_t *omd = (metadata_t *)smalloc (sizeof (metadata_t));

	omd->name = md->name ? string_dup (md->name) : NULL;
	omd->data = md->data ? string_dup (md->data) : NULL;

	return omd;
}
/*}}}*/
/*{{{  void metadata_freemetadata (metadata_t *md)*/
/*
 *	destroys a metadata structure
 */
void metadata_freemetadata (metadata_t *md)
{
	if (!md) {
		nocc_internal ("metadata_freemetadata(): NULL hook!");
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
/*{{{  metadatalist_t *metadata_newmetadatalist (void)*/
/*
 *	creates a blank metadatalist structure
 */
metadatalist_t *metadata_newmetadatalist (void)
{
	metadatalist_t *mdl = (metadatalist_t *)smalloc (sizeof (metadatalist_t));

	dynarray_init (mdl->items);

	return mdl;
}
/*}}}*/
/*{{{  void metadata_freemetadatalist (metadatalist_t *mdl)*/
/*
 *	destroys a metadatalist structure (deep)
 */
void metadata_freemetadatalist (metadatalist_t *mdl)
{
	int i;

	if (!mdl) {
		nocc_internal ("metadata_freemetadatalist(): NULL hook!");
	}
	for (i=0; i<DA_CUR (mdl->items); i++) {
		metadata_t *mdi = DA_NTHITEM (mdl->items, i);

		if (mdi) {
			metadata_freemetadata (mdi);
		}
	}
	dynarray_trash (mdl->items);
	sfree (mdl);
}
/*}}}*/


/*{{{  static void *metadatahook_copy (void *hook)*/
/*
 *	duplicates a "metadata" compiler hook
 */
static void *metadatahook_copy (void *hook)
{
	if (hook) {
		metadata_t *md = (metadata_t *)hook;

		return (void *)metadata_createmetadata (md->name, md->data);
	}
	return NULL;
}
/*}}}*/
/*{{{  static void metadatahook_free (void *hook)*/
/*
 *	frees a "metadata" compiler hook
 */
static void metadatahook_free (void *hook)
{
	if (hook) {
		metadata_freemetadata((metadata_t *)hook);
	}
	return;
}
/*}}}*/
/*{{{  static void metadatahook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	dumps a "metadata" compiler hook (debugging)
 */
static void metadatahook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	metadata_t *omd = (metadata_t *)hook;

	metadata_isetindent (stream, indent);
	if (!hook) {
		fprintf (stream, "<chook id=\"metadata\" value=\"\" />\n");
	} else {
		fprintf (stream, "<chook id=\"metadata\" name=\"%s\" data=\"%s\" />\n", omd->name, omd->data);
	}
}
/*}}}*/


/*{{{  static void *metadatalisthook_copy (void *hook)*/
/*
 *	duplicates a "metadatalist" compiler hook
 */
static void *metadatalisthook_copy (void *hook)
{
	if (hook) {
		int i;
		metadatalist_t *mdl = (metadatalist_t *)hook;
		metadatalist_t *newl = metadata_newmetadatalist ();

		for (i=0; i<DA_CUR (mdl->items); i++) {
			metadata_t *mdi = DA_NTHITEM (mdl->items, i);

			if (mdi) {
				void *copy = metadatahook_copy ((void *)mdi);

				dynarray_add (newl->items, copy);
			}
		}

		return (void *)newl;
	}
	return NULL;
}
/*}}}*/
/*{{{  static void metadatalisthook_free (void *hook)*/
/*
 *	frees a "metadatalist" compiler hook
 */
static void metadatalisthook_free (void *hook)
{
	if (hook) {
		metadata_freemetadatalist ((metadatalist_t *)hook);
	}
	return;
}
/*}}}*/
/*{{{  static void metadatalisthook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	dumps a "metadatalist" compiler hook (debugging)
 */
static void metadatalisthook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	metadatalist_t *mdl = (metadatalist_t *)hook;

	metadata_isetindent (stream, indent);
	if (!hook) {
		fprintf (stream, "<chook id=\"metadatalist\" value=\"\" />\n");
	} else {
		int i;

		fprintf (stream, "<chook id=\"metadatalist\" size=\"%d\">\n", DA_CUR (mdl->items));
		for (i=0; i<DA_CUR (mdl->items); i++) {
			metadatahook_dumptree (node, (void *)DA_NTHITEM (mdl->items, i), indent + 1, stream);
		}
		metadata_isetindent (stream, indent);
		fprintf (stream, "</chook>\n");
	}
}
/*}}}*/


/*{{{  int metadata_init (void)*/
/*
 *	initialises meta-data handling
 *	returns 0 on success, non-zero on failure
 */
int metadata_init (void)
{
	/*{{{  local initialisation*/
	stringhash_init (mdres);

	/*}}}*/
	/*{{{  metadata compiler-hook*/
	metadata_chook = tnode_lookupornewchook ("metadata");

	metadata_chook->chook_copy = metadatahook_copy;
	metadata_chook->chook_free = metadatahook_free;
	metadata_chook->chook_dumptree = metadatahook_dumptree;

	/*}}}*/
	/*{{{  metadatalist compiler-hook*/
	metadatalist_chook = tnode_lookupornewchook ("metadatalist");

	metadatalist_chook->chook_copy = metadatalisthook_copy;
	metadatalist_chook->chook_free = metadatalisthook_free;
	metadatalist_chook->chook_dumptree = metadatalisthook_dumptree;

	/*}}}*/
	return 0;
}
/*}}}*/
/*{{{  int metadata_shutdown (void)*/
/*
 *	shuts-down meta-data handling
 *	return 0 on success, non-zero on failure
 */
int metadata_shutdown (void)
{
	return 0;
}
/*}}}*/




