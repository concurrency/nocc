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


/*{{{  void scope_init (void)*/
/*
 *	initialises the scoper
 */
void scope_init (void)
{
	return;
}
/*}}}*/
/*{{{  void scope_shutdown (void)*/
/*
 *	shuts-down the scoper
 */
void scope_shutdown (void)
{
	return;
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


/*{{{  int scope_modprewalktree (tnode_t **node, void *arg)*/
/*
 *	generic pre-scoping function
 */
int scope_modprewalktree (tnode_t **node, void *arg)
{
	scope_t *sarg = (scope_t *)arg;
	int i = 1;

	if ((*node)->tag->ndef->ops && (*node)->tag->ndef->ops->scopein) {
		i = (*node)->tag->ndef->ops->scopein (node, sarg);
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

	if ((*node)->tag->ndef->ops && (*node)->tag->ndef->ops->scopeout) {
		i = (*node)->tag->ndef->ops->scopeout (node, sarg);
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
	r = lang->scope (&t, ss);

	nocc_message ("scope_tree(): completed! %d names scoped, %d error(s), %d warning(s)", ss->scoped, ss->err, ss->warn);

	if (ss->err) {
		r = ss->err;
	}

	sfree (ss);
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

