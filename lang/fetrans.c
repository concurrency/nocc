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


/*{{{  tnode_t *fetrans_maketemp (tnode_t *type, fetrans_t *fe)*/
/*
 *	this creates a new temporary in the front-end transform pass
 *	returns the name-reference to the generated temporary
 */
tnode_t *fetrans_maketemp (tnode_t *type, fetrans_t *fe)
{
	if (!type || !fe || !fe->lang) {
		nocc_internal ("fetrans_maketemp(): called with a bad something");
		return NULL;
	}
	if (!fe->insertpoint) {
		nocc_error ("fetrans_maketemp(): nowhere to insert temporaries here!");
		return NULL;
	}
	if (!fe->lang->maketemp) {
		tnode_error (*(fe->insertpoint), "fetrans_maketemp(): cannot make temporaries in language [%s]", fe->lang->langname);
		return NULL;
	}
	return fe->lang->maketemp (&fe->insertpoint, type);
}
/*}}}*/
/*{{{  tnode_t *fetrans_makeseqassign (tnode_t *lhs, tnode_t *rhs, tnode_t *type, fetrans_t *fe)*/
/*
 *	this creates a new sequential assignment in the front-end transform pass
 *	returns a reference to the created assignment
 */
tnode_t *fetrans_makeseqassign (tnode_t *lhs, tnode_t *rhs, tnode_t *type, fetrans_t *fe)
{
	if (!lhs || !rhs || !type || !fe || !fe->lang) {
		nocc_internal ("fetrans_makeseqassign(): called with a bad something");
		return NULL;
	}
	if (!fe->insertpoint) {
		nocc_error ("fetrans_makeseqassign(): nowhere to insert code here!");
		return NULL;
	}
	if (!fe->lang->makeseqassign) {
		tnode_error (*(fe->insertpoint), "fetrans_makeseqassign(): cannot make sequential assignments in language [%s]", fe->lang->langname);
		return NULL;
	}
	return fe->lang->makeseqassign (&fe->insertpoint, lhs, rhs, type);
}
/*}}}*/
/*{{{  tnode_t *fetrans_makeseqany (fetrans_t *fe)*/
/*
 *	this creates a new sequential node in the front-end transform pass, with the original node as its single content
 *	returns a reference to the created sequential node list (not the sequential node itself necessarily)
 */
tnode_t *fetrans_makeseqany (fetrans_t *fe)
{
	if (!fe || !fe->lang) {
		nocc_internal ("fetrans_makeseqany(): called with a bad something");
		return NULL;
	}
	if (!fe->insertpoint) {
		nocc_error ("fetrans_makeseqany(): nowhere to insert code here!");
		return NULL;
	}
	if (!fe->lang->makeseqany) {
		tnode_error (*(fe->insertpoint), "fetrans_makeseqany(): cannot make sequential node in language [%s]", fe->lang->langname);
		return NULL;
	}
	return fe->lang->makeseqany (&fe->insertpoint);
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

	if (*tptr && (*tptr)->tag->ndef->ops && tnode_hascompop_i ((*tptr)->tag->ndef->ops, (int)COPS_FETRANS)) {
		i = tnode_callcompop_i ((*tptr)->tag->ndef->ops, (int)COPS_FETRANS, 2, tptr, (fetrans_t *)arg);
	}
	return i;
}
/*}}}*/
/*{{{  int fetrans_subtree (tnode_t **tptr, fetrans_t *fe)*/
/*
 *	does a sub-tree walk for front-end transforms
 *	returns 0 on success, non-zero on error
 */
int fetrans_subtree (tnode_t **tptr, fetrans_t *fe)
{
	tnode_modprewalktree (tptr, fetrans_modprewalk, (void *)fe);
	return 0;
}
/*}}}*/
/*{{{  int fetrans_tree (tnode_t **tptr, langparser_t *lang)*/
/*
 *	does front-end tree transforms on the given tree
 *	returns 0 on success, non-zero on error
 */
int fetrans_tree (tnode_t **tptr, langparser_t *lang)
{
	fetrans_t *fe = (fetrans_t *)smalloc (sizeof (fetrans_t));

	fe->insertpoint = NULL;
	fe->lang = lang;
	fe->langpriv = NULL;

	if (!fetranschook) {
		fetranschook = tnode_lookupornewchook ("fetrans");
	}
	if (!fetransdeschook) {
		fetransdeschook = tnode_lookupornewchook ("fetrans:descriptor");
		fetransdeschook->chook_copy = fetrans_deschook_copy;
		fetransdeschook->chook_free = fetrans_deschook_free;
		fetransdeschook->chook_dumptree = fetrans_deschook_dumptree;
	}

	if (lang->fetrans) {
		/* lets language attach extra bits to the fetrans_t structure */
		lang->fetrans (tptr, fe);
	} else {
		tnode_modprewalktree (tptr, fetrans_modprewalk, (void *)fe);
	}

	sfree (fe);

	return 0;
}
/*}}}*/

