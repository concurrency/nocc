/*
 *	guppy_udo.c -- user-defined operators in guppy
 *	Copyright (C) 2013-2016 Fred Barnes <frmb@kent.ac.uk>
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

/* Note: this deals mostly with the translation between operators and function names
 */


/*{{{  includes*/
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/types.h>

#include "nocc.h"
#include "support.h"
#include "version.h"
#include "origin.h"
#include "fhandle.h"
#include "symbols.h"
#include "keywords.h"
#include "lexer.h"
#include "lexpriv.h"
#include "tnode.h"
#include "parser.h"
#include "guppy.h"
#include "names.h"

/*}}}*/
/*{{{  private types*/

typedef struct TAG_udo_entry {
	char *opstr;			/* operator string ("+") */
	tnode_t *params;		/* parameter types */
	tnode_t *results;		/* result types */
	char *fcnname;			/* function name */
	char *fixname;			/* fixed function name (once types are fully known) */
} udo_entry_t;

/*}}}*/
/*{{{  private data*/

STATICDYNARRAY (udo_entry_t *, uentries);

static chook_t *guppy_udo_tempnamechook = NULL;


/*}}}*/


/*{{{  static udo_entry_t *guppy_udo_newentry (void)*/
/*
 *	creates a new udo_entry_t structure
 */
static udo_entry_t *guppy_udo_newentry (void)
{
	udo_entry_t *uent = (udo_entry_t *)smalloc (sizeof (udo_entry_t));

	uent->opstr = NULL;
	uent->params = NULL;
	uent->results = NULL;
	uent->fcnname = NULL;
	uent->fixname = NULL;

	return uent;
}
/*}}}*/
/*{{{  static void guppy_udo_freeentry (udo_entry_t *uent)*/
/*
 *	frees a udo_entry_t structure
 */
static void guppy_udo_freeentry (udo_entry_t *uent)
{
	if (!uent) {
		nocc_serious ("guppy_udo_freeentry(): NULL pointer!");
		return;
	}
	if (uent->opstr) {
		sfree (uent->opstr);
		uent->opstr = NULL;
	}
	if (uent->fcnname) {
		sfree (uent->fcnname);
		uent->fcnname = NULL;
	}
	if (uent->fixname) {
		sfree (uent->fixname);
		uent->fixname = NULL;
	}
	sfree (uent);
	return;
}
/*}}}*/

/*{{{  static void *guppy_udo_tempnamechook_copy (void *hook)*/
/*
 *	duplicates a guppy:udo:tempname compiler hook
 */
static void *guppy_udo_tempnamechook_copy (void *hook)
{
	if (!hook) {
		return NULL;
	}
	return string_dup ((char *)hook);
}
/*}}}*/
/*{{{  static void guppy_udo_tempnamechook_free (void *hook)*/
/*
 *	frees a guppy:udo:tempname compiler hook
 */
static void guppy_udo_tempnamechook_free (void *hook)
{
	if (!hook) {
		return;
	}
	sfree (hook);
	return;
}
/*}}}*/
/*{{{  static void guppy_udo_tempnamechook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)*/
/*
 *	dumps a guppy:udo:tempname compiler hook (debugging)
 */
static void guppy_udo_tempnamechook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)
{
	guppy_isetindent (stream, indent);
	fhandle_printf (stream, "<chook:guppy:udo:tempname name=\"%s\" />\n", hook ? (char *)hook : "");
	return;
}
/*}}}*/

/*{{{  char *guppy_udo_maketempfcnname (tnode_t *node)*/
/*
 *	creates (or reports existing) temporary name for a UDO -- used in prescope/scope
 */
char *guppy_udo_maketempfcnname (tnode_t *node)
{
	char *tstr;

#if 0
fhandle_printf (FHAN_STDERR, "guppy_udo_maketempfcnname(): node=[%s 0x%8.8x], guppy_udo_tempnamechook=0x%8.8x\n",
		node->tag->name, (unsigned int)node, (unsigned int)guppy_udo_tempnamechook);
#endif
	if (tnode_haschook (node, guppy_udo_tempnamechook)) {
		return (char *)tnode_getchook (node, guppy_udo_tempnamechook);
	}
	tstr = string_fmt ("udotmp%8.8x", (unsigned int)node);
	tnode_setchook (node, guppy_udo_tempnamechook, tstr);

	return tstr;
}
/*}}}*/
/*{{{  char *guppy_udo_newfunction (const char *fstr, tnode_t *results, tnode_t *params)*/
/*
 *	creates a new UDO function, happens during scope-in
 *	returns the raw name to use
 */
char *guppy_udo_newfunction (const char *fstr, tnode_t *results, tnode_t *params)
{
	char *tstr;
	udo_entry_t *uent;

	uent = guppy_udo_newentry ();
	uent->opstr = string_dup (fstr);
	uent->params = params;
	uent->results = results;
	uent->fcnname = string_fmt ("udo$%8.8x", (unsigned int)uent);

	dynarray_add (uentries, uent);

	return uent->fcnname;
}
/*}}}*/

/*{{{  int guppy_udo_init (void)*/
/*
 *	initialises Guppy UDO handling
 *	returns 0 on success, non-zero on failure
 */
int guppy_udo_init (void)
{
	dynarray_init (uentries);

	guppy_udo_tempnamechook = tnode_lookupornewchook ("guppy:udo:tempname");
	guppy_udo_tempnamechook->chook_copy = guppy_udo_tempnamechook_copy;
	guppy_udo_tempnamechook->chook_free = guppy_udo_tempnamechook_free;
	guppy_udo_tempnamechook->chook_dumptree = guppy_udo_tempnamechook_dumptree;

	return 0;
}
/*}}}*/
/*{{{  int guppy_udo_shutdown (void)*/
/*
 *	shuts-down Guppy UDO handling
 *	returns 0 on success, non-zero on failure
 */
int guppy_udo_shutdown (void)
{
	return 0;
}
/*}}}*/

