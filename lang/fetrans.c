/*
 *	fetrans.c -- front-end tree transforms
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
#include "dfa.h"
#include "names.h"
#include "fetrans.h"
#include "langops.h"


/*}}}*/
/*{{{  private things*/
static chook_t *fetranschook = NULL;
static chook_t *fetransdeschook = NULL;

/*}}}*/


/*{{{  static void fetrans_isetindent (int indent, FILE *stream)*/
/*
 *	sets indentation for output
 */
static void fetrans_isetindent (int indent, FILE *stream)
{
	int i;

	for (i=0; i<indent; i++) {
		fprintf (stream, "    ");
	}
	return;
}
/*}}}*/


/*{{{  descriptor compiler-hook routines*/
/*{{{  static void *fetrans_deschook_copy (void *hook)*/
/*
 *	copies a descriptor hook
 */
static void *fetrans_deschook_copy (void *hook)
{
	void *nhook;

	if (!hook) {
		nhook = NULL;
	} else {
		nhook = (void *)string_dup ((char *)hook);
	}
	return nhook;
}
/*}}}*/
/*{{{  static void fetrans_deschook_free (void *hook)*/
/*
 *	frees a descriptor hook
 */
static void fetrans_deschook_free (void *hook)
{
	if (hook) {
		sfree (hook);
	}
	return;
}
/*}}}*/
/*{{{  static void fetrans_deschook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	dumps a descriptor hook (debugging)
 */
static void fetrans_deschook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	fetrans_isetindent (indent, stream);
	fprintf (stream, "<fetrans:descriptor value=\"%s\" />\n", hook ? (char *)hook : "");
}
/*}}}*/
/*}}}*/


/*{{{  int fetrans_init (void)*/
/*
 *	initialises front-end tree transforms
 *	returns 0 on success, non-zero on error
 */
int fetrans_init (void)
{
	return 0;
}
/*}}}*/
/*{{{  int fetrans_shutdown (void)*/
/*
 *	shuts-down front-end tree transforms
 *	returns 0 on success, non-zero on error
 */
int fetrans_shutdown (void)
{
	return 0;
}
/*}}}*/


/*{{{  static int fetrans_modprewalk (tnode_t **tptr, void *arg)*/
/*
 *	does the front-end tree transform walk
 *	returns 0 to stop walk, 1 to continue
 */
static int fetrans_modprewalk (tnode_t **tptr, void *arg)
{
	int i = 1;

	if (*tptr && (*tptr)->tag->ndef->ops && (*tptr)->tag->ndef->ops->fetrans) {
		i = (*tptr)->tag->ndef->ops->fetrans (tptr);
	}
	return i;
}
/*}}}*/
/*{{{  int fetrans_tree (tnode_t **tptr, langparser_t *lang)*/
/*
 *	does front-end tree transforms on the given tree
 *	returns 0 on success, non-zero on error
 */
int fetrans_tree (tnode_t **tptr, langparser_t *lang)
{
	if (!fetranschook) {
		fetranschook = tnode_newchook ("fetrans");
	}
	if (!fetransdeschook) {
		fetransdeschook = tnode_newchook ("fetrans:descriptor");
		fetransdeschook->chook_copy = fetrans_deschook_copy;
		fetransdeschook->chook_free = fetrans_deschook_free;
		fetransdeschook->chook_dumptree = fetrans_deschook_dumptree;
	}

	tnode_modprewalktree (tptr, fetrans_modprewalk, (void *)lang);

	return 0;
}
/*}}}*/

