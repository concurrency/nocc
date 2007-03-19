/*
 *	scope.c -- nocc scoper
 *	Copyright (C) 2004 Fred Barnes <frmb@kent.ac.uk>
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
#include "scope.h"


/*{{{  int scope_init (void)*/
/*
 *	initialises the scoper
 *	returns 0 on success, non-zero on failure
 */
int scope_init (void)
{
	return 0;
}
/*}}}*/
/*{{{  int scope_shutdown (void)*/
/*
 *	shuts-down the scoper
 *	returns 0 on success, non-zero on failure
 */
int scope_shutdown (void)
{
	return 0;
}
/*}}}*/


/*{{{  void scope_warning (tnode_t *node, scope_t *ss, const char *fmt, ...)*/
/*
 *	called by pre-scoper bits for warnings
 */
void scope_warning (tnode_t *node, scope_t *ss, const char *fmt, ...)
{
	va_list ap;
	int n;
	char *warnbuf = (char *)smalloc (512);
	lexfile_t *orgfile;

	if (!node) {
		orgfile = NULL;
	} else {
		orgfile = node->org_file;
	}

	va_start (ap, fmt);
	n = sprintf (warnbuf, "%s:%d (warning) ", orgfile ? orgfile->fnptr : "(unknown)", node->org_line);
	vsnprintf (warnbuf + n, 512 - n, fmt, ap);
	va_end (ap);

	if (orgfile) {
		orgfile->warncount++;
	}
	ss->warn++;
	nocc_message (warnbuf);
	sfree (warnbuf);

	return;
}
/*}}}*/
/*{{{  void scope_error (tnode_t *node, scope_t *ss, const char *fmt, ...)*/
/*
 *	called by the pre-scoper bits for errors
 */
void scope_error (tnode_t *node, scope_t *ss, const char *fmt, ...)
{
	va_list ap;
	int n;
	char *warnbuf = (char *)smalloc (512);
	lexfile_t *orgfile;

	if (!node) {
		orgfile = NULL;
	} else {
		orgfile = node->org_file;
	}

	va_start (ap, fmt);
	n = sprintf (warnbuf, "%s:%d (error) ", orgfile ? orgfile->fnptr : "(unknown)", node->org_line);
	vsnprintf (warnbuf + n, 512 - n, fmt, ap);
	va_end (ap);

	if (orgfile) {
		orgfile->errcount++;
	}
	ss->err++;
	nocc_message (warnbuf);
	sfree (warnbuf);

	return;
}
/*}}}*/


/*{{{  int scope_pushdefns (scope_t *ss, namespace_t *ns)*/
/*
 *	pushes a defining namespace
 *	returns 0 on success, non-zero on failure
 */
int scope_pushdefns (scope_t *ss, namespace_t *ns)
{
	if (!ss || !ns) {
		return -1;
	}
	dynarray_add (ss->defns, ns);

	return 0;
}
/*}}}*/
/*{{{  int scope_popdefns (scope_t *ss, namespace_t *ns)*/
/*
 *	pops a defining namespace
 *	returns 0 on success, non-zero on failure
 */
int scope_popdefns (scope_t *ss, namespace_t *ns)
{
	if (!ss || !DA_CUR (ss->defns)) {
		return -1;
	}
	if (!ns) {
		/* remove last regardless */
		dynarray_delitem (ss->defns, DA_CUR (ss->defns) - 1);
	} else if (DA_NTHITEM (ss->defns, DA_CUR (ss->defns) - 1) != ns) {
		/* not us! */
		nocc_warning ("scope_popdefns(): expected namespace [%s] found [%s]", ns->nspace, DA_NTHITEM (ss->defns, DA_CUR (ss->defns) - 1)->nspace);
		return -1;
	} else {
		dynarray_delitem (ss->defns, DA_CUR (ss->defns) - 1);
	}
	return 0;
}
/*}}}*/
/*{{{  int scope_pushusens (scope_t *ss, namespace_t *ns)*/
/*
 *	pushes a using namespace
 *	returns 0 on success, non-zero on failure
 */
int scope_pushusens (scope_t *ss, namespace_t *ns)
{
	if (!ss || !ns) {
		return -1;
	}
	dynarray_add (ss->usens, ns);

	return 0;
}
/*}}}*/
/*{{{  int scope_popusens (scope_t *ss, namespace_t *ns)*/
/*
 *	pops a using namespace
 *	returns 0 on success, non-zero on failure
 */
int scope_popusens (scope_t *ss, namespace_t *ns)
{
	if (!ss || !DA_CUR (ss->usens)) {
		return -1;
	}
	if (!ns) {
		/* remove last regardless */
		dynarray_delitem (ss->usens, DA_CUR (ss->usens) - 1);
	} else if (DA_NTHITEM (ss->usens, DA_CUR (ss->usens) - 1) != ns) {
		/* not us! */
		nocc_warning ("scope_popusens(): expected namespace [%s] found [%s]", ns->nspace, DA_NTHITEM (ss->usens, DA_CUR (ss->usens) - 1)->nspace);
		return -1;
	} else {
		dynarray_delitem (ss->usens, DA_CUR (ss->usens) - 1);
	}
	return 0;
}
/*}}}*/

/*{{{  int scope_modprewalktree (tnode_t **node, void *arg)*/
/*
 *	generic pre-scoping function
 */
int scope_modprewalktree (tnode_t **node, void *arg)
{
	scope_t *sarg = (scope_t *)arg;
	int i = 1;

	if ((*node)->tag->ndef->ops && tnode_hascompop_i ((*node)->tag->ndef->ops, (int)COPS_SCOPEIN)) {
		i = tnode_callcompop_i ((*node)->tag->ndef->ops, (int)COPS_SCOPEIN, 2, node, sarg);
	}

	return i;
}
/*}}}*/
/*{{{  int scope_modpostwalktree (tnode_t **node, void *arg)*/
/*
 *	generic pre-scoping function (post walk)
 */
int scope_modpostwalktree (tnode_t **node, void *arg)
{
	scope_t *sarg = (scope_t *)arg;
	int i = 1;

	if ((*node)->tag->ndef->ops && tnode_hascompop_i ((*node)->tag->ndef->ops, (int)COPS_SCOPEOUT)) {
		i = tnode_callcompop_i ((*node)->tag->ndef->ops, (int)COPS_SCOPEOUT, 2, node, sarg);
	}

	return i;
}
/*}}}*/
/*{{{  int scope_tree (tnode_t *t, langparser_t *lang)*/
/*
 *	scopes declarations within a tree
 *	return 0 on success, non-zero on failure
 */
int scope_tree (tnode_t *t, langparser_t *lang)
{
	scope_t *ss = (scope_t *)smalloc (sizeof (scope_t));
	int r;

	ss->err = 0;
	ss->warn = 0;
	ss->scoped = 0;
	if (!lang->scope) {
		nocc_error ("scope_tree(): don\'t know how to scope this language!");
		sfree (ss);
		return 1;
	}
	ss->lang = lang;
	ss->langpriv = NULL;
	dynarray_init (ss->defns);
	dynarray_init (ss->usens);

	r = lang->scope (&t, ss);

	if (compopts.verbose || ss->err || ss->warn) {
		nocc_message ("scope_tree(): completed! %d names scoped, %d error(s), %d warning(s)", ss->scoped, ss->err, ss->warn);
	}

	if (ss->err) {
		r = ss->err;
	}

	dynarray_trash (ss->defns);
	dynarray_trash (ss->usens);
	sfree (ss);

	if (compopts.dumpnames) {
		/* bit hackish perhaps (moved out from main) */
		name_dumpnames (stderr);
	}

	return r;
}
/*}}}*/
/*{{{  int scope_subtree (tnode_t **tptr, scope_t *sarg)*/
/*
 *	does sub-tree scoping within a tree
 *	return 0 on success, non-zero on failure
 */
int scope_subtree (tnode_t **tptr, scope_t *sarg)
{
	int r;

	if (!sarg || !sarg->lang) {
		nocc_error ("scope_subtree(): null scope-state or language");
		return 1;
	}
	if (!sarg->lang->scope) {
		nocc_error ("scope_subtree(): don\'t know how to scope this language!");
		return 1;
	}

	r = sarg->lang->scope (tptr, sarg);
	
	return r;
}
/*}}}*/

