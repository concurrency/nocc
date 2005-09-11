/*
 *	names.c -- name stuff
 *	Copyright (C) 2004-2005 Fred Barnes <frmb@kent.ac.uk>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

#include "nocc.h"
#include "support.h"
#include "version.h"
#include "symbols.h"
#include "keywords.h"
#include "lexer.h"
#include "lexpriv.h"
#include "tnode.h"
#include "names.h"


/*{{{  private stuff*/
STATICSTRINGHASH (namelist_t *, names, 7);
STATICDYNARRAY (name_t *, namestack);


/*}}}*/


/*{{{  void name_init (void)*/
/*
 *	initialises the name handling bits
 */
void name_init (void)
{
	stringhash_init (names);
	dynarray_init (namestack);

	return;
}
/*}}}*/
/*{{{  void name_shutdown (void)*/
/*
 *	shuts down name handling bits
 */
void name_shutdown (void)
{
	return;
}
/*}}}*/


/*{{{  name_t *name_lookup (char *str)*/
/*
 *	looks up a name -- returns it at the current scoping level (or last if unset)
 */
name_t *name_lookup (char *str)
{
	namelist_t *nl;
	name_t *name;

	nl = stringhash_lookup (names, str);
#if 0
fprintf (stderr, "name_lookup(): str=[%s], nl=0x%8.8x, nl->curscope = %d, DA_CUR (nl->scopes) = %d\n", str, (unsigned int)nl, (unsigned int)nl->curscope, DA_CUR (nl->scopes));
#endif
	if (!nl) {
		name = NULL;
	} else if ((nl->curscope < 0) && !DA_CUR (nl->scopes)) {
		name = NULL;
	} else if (nl->curscope < 0) {
		name = DA_NTHITEM (nl->scopes, DA_CUR (nl->scopes) - 1);
	} else {
		name = DA_NTHITEM (nl->scopes, nl->curscope);
	}
	return name;
}
/*}}}*/
/*{{{  name_t *name_addname (char *str, tnode_t *decl, tnode_t *type, tnode_t *namenode)*/
/*
 *	adds a name -- and returns it, after putting it in scope
 */
name_t *name_addscopename (char *str, tnode_t *decl, tnode_t *type, tnode_t *namenode)
{
	name_t *name;
	namelist_t *nl;

	name = (name_t *)smalloc (sizeof (name_t));
	name->decl = decl;
	name->type = type;
	name->namenode = namenode;
	name->refc = 0;
#if 0
fprintf (stderr, "name_addscopename(): adding name [%s] type:\n", str);
tnode_dumptree (type, 1, stderr);
#endif

	nl = stringhash_lookup (names, str);
	if (!nl) {
		nl = (namelist_t *)smalloc (sizeof (namelist_t));
		nl->name = string_dup (str);
		dynarray_init (nl->scopes);
		nl->curscope = -1;
		stringhash_insert (names, nl, nl->name);
	}
	dynarray_add (nl->scopes, name);
	nl->curscope = DA_CUR (nl->scopes) - 1;
	name->me = nl;
	dynarray_add (namestack, name);
	
	return name;
}
/*}}}*/
/*{{{  void name_scopename (name_t *name)*/
/*
 *	scopes in a name
 */
void name_scopename (name_t *name)
{
	namelist_t *nl = name->me;
	int i;

	for (i=0; i<DA_CUR (nl->scopes); i++) {
		if (DA_NTHITEM (nl->scopes, i) == name) {
			nocc_internal ("name_scopename(): name [%s] already in scope", nl->name);
		}
	}
	dynarray_add (nl->scopes, name);
	nl->curscope = DA_CUR (nl->scopes) - 1;
	dynarray_add (namestack, name);

	return;
}
/*}}}*/
/*{{{  void name_descopename (name_t *name)*/
/*
 *	descopes a name
 */
void name_descopename (name_t *name)
{
	namelist_t *nl = name->me;
	int i;
	int found = 0;

	for (i=0; i<DA_CUR (nl->scopes); i++) {
		if (DA_NTHITEM (nl->scopes, i) == name) {
			dynarray_delitem (nl->scopes, i);
			found = 1;
			break;		/* for() */
		}
	}
	if (!found) {
		nocc_internal ("name_descopename(): name [%s] not in scope", nl->name);
	}
#if 0
fprintf (stderr, "name_descopename(): removing name [%s]\n", nl->name);
#endif
	/* find and remove from the namestack */
	for (i=DA_CUR (namestack) - 1; i >= 0; i--) {
		if (DA_NTHITEM (namestack, i) == name) {
			if (i != (DA_CUR (namestack) - 1)) {
				nocc_warning ("name_descopename(): name [%s] not top of scope", nl->name);
			}
			dynarray_delitem (namestack, i);
			for (; i < DA_CUR (namestack); ) {
				dynarray_delitem (namestack, i);
			}

			break;		/* for() */
		}
	}
		
	if (nl->curscope >= DA_CUR (nl->scopes)) {
		nl->curscope = DA_CUR (nl->scopes) - 1;
	}
	return;
}
/*}}}*/
/*{{{  void name_delname (name_t *name)*/
/*
 *	deletes a name
 */
void name_delname (name_t *name)
{
	namelist_t *nl = name->me;

	dynarray_rmitem (nl->scopes, name);
	if (nl->curscope >= DA_CUR (nl->scopes)) {
		nl->curscope = DA_CUR (nl->scopes) - 1;
	}
	sfree (name);

	return;
}
/*}}}*/


/*{{{  void *name_markscope (void)*/
/*
 *	this "marks" the name-stack, such that it can be restored with
 *	name_markdescope() below
 */
void *name_markscope (void)
{
	if (!DA_CUR (namestack)) {
		return NULL;
	}
	return DA_NTHITEM (namestack, DA_CUR (namestack) - 1);
}
/*}}}*/
/*{{{  void name_markdescope (void *mark)*/
/*
 *	this descopes names above some mark
 */
void name_markdescope (void *mark)
{
	int i;

	if (!mark) {
		/* means we're descoping everything! */
		for (i=0; i<DA_CUR (namestack);) {
			name_descopename (DA_NTHITEM (namestack, DA_CUR (namestack) - 1));
		}
	} else {
		for (i=DA_CUR (namestack) - 1; i >= 0; i--) {
			if (DA_NTHITEM (namestack, i) == mark) {
				break;		/* for() */
			}
		}
		if (i < 0) {
			nocc_internal ("name_markdescope(): mark not found!");
			return;
		}
		for (i++; i<DA_CUR (namestack);) {
			name_descopename (DA_NTHITEM (namestack, DA_CUR (namestack) - 1));
		}
	}
	return;
}
/*}}}*/


/*{{{  void name_dumpname (name_t *name, int indent, FILE *stream)*/
/*
 *	dumps a single name (global call)
 */
void name_dumpname (name_t *name, int indent, FILE *stream)
{
	int i;

	for (i=0; i<indent; i++) {
		fprintf (stream, "    ");
	}
	fprintf (stream, "<name name=\"%s\" />\n", name->me->name);

	return;
}
/*}}}*/
/*{{{  static void name_walkdumpname (namelist_t *nl, char *key, void *ptr)*/
/*
 *	dumps a single name
 */
static void name_walkdumpname (namelist_t *nl, char *key, void *ptr)
{
	FILE *stream = ptr ? (FILE *)ptr : stderr;
	int i;

	fprintf (stream, "name [%s] curscope = %d\n", key, nl->curscope);
	for (i=0; i<DA_CUR (nl->scopes); i++) {
		name_t *name = DA_NTHITEM (nl->scopes, i);

		fprintf (stream, "\t%d\trefc = %-3d  decl = 0x%8.8x:\n", i, name->refc, (unsigned int)(name->decl));
	}
	return;
}
/*}}}*/
/*{{{  void name_dumpnames (FILE *stream)*/
/*
 *	dumps the names
 */
void name_dumpnames (FILE *stream)
{
	stringhash_walk (names, name_walkdumpname, (void *)stream);
	return;
}
/*}}}*/


