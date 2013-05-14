/*
 *	prescope.c -- nocc pre-scope'r
 *	Copyright (C) 2005-2013 Fred Barnes <frmb@kent.ac.uk>
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
#include "prescope.h"


/*{{{  int prescope_init (void)*/
/*
 *	initialises pre-scope
 *	returns 0 on success, non-zero on failure
 */
int prescope_init (void)
{
	return 0;
}
/*}}}*/
/*{{{  int prescope_shutdown (void)*/
/*
 *	shuts-down pre-scope
 *	returns 0 on success, non-zero on failure
 */
int prescope_shutdown (void)
{
	return 0;
}
/*}}}*/


/*{{{  void prescope_warning (tnode_t *node, prescope_t *ps, const char *fmt, ...)*/
/*
 *	called by pre-scoper bits for warnings
 */
void prescope_warning (tnode_t *node, prescope_t *ps, const char *fmt, ...)
{
	va_list ap;
	int n;
	char *warnbuf = (char *)smalloc (512);
	srclocn_t *src;

	src = node ? node->org : NULL;

	va_start (ap, fmt);
	n = sprintf (warnbuf, "%s:%d (warning) ", src ? src->org_file->fnptr : "(unknown)", src ? src->org_line : 0);
	vsnprintf (warnbuf + n, 512 - n, fmt, ap);
	va_end (ap);

	if (src) {
		src->org_file->warncount++;
	}
	ps->warn++;
	nocc_message ("%s", warnbuf);
	sfree (warnbuf);

	return;
}
/*}}}*/
/*{{{  void prescope_error (tnode_t *node, prescope_t *ps, const char *fmt, ...)*/
/*
 *	called by the pre-scoper bits for errors
 */
void prescope_error (tnode_t *node, prescope_t *ps, const char *fmt, ...)
{
	va_list ap;
	int n;
	char *warnbuf = (char *)smalloc (512);
	srclocn_t *src;

	src = node ? node->org : NULL;

	va_start (ap, fmt);
	n = sprintf (warnbuf, "%s:%d (error) ", src ? src->org_file->fnptr : "(unknown)", src ? src->org_line : 0);
	vsnprintf (warnbuf + n, 512 - n, fmt, ap);
	va_end (ap);

	if (src) {
		src->org_file->errcount++;
	}
	ps->err++;
	nocc_message ("%s", warnbuf);
	sfree (warnbuf);

	return;
}
/*}}}*/


/*{{{  int prescope_modprewalktree (tnode_t **node, void *arg)*/
/*
 *	generic pre-scoping function
 */
int prescope_modprewalktree (tnode_t **node, void *arg)
{
	int i = 1;

	if ((*node)->tag->ndef->ops && tnode_hascompop_i ((*node)->tag->ndef->ops, (int)COPS_PRESCOPE)) {
		i = tnode_callcompop_i ((*node)->tag->ndef->ops, (int)COPS_PRESCOPE, 2, node, (prescope_t *)arg);
	}
	return i;
}
/*}}}*/
/*{{{  int prescope_subtree (tnode_t **t, prescope_t *ps)*/
/*
 *	performs pre-scoping on a sub-tree
 *	returns 0 on success, non-zero on failure
 */
int prescope_subtree (tnode_t **t, prescope_t *ps)
{
	int i;
	void *oldhook = ps->hook;

	i = ps->lang->prescope (t, ps);
	ps->hook = oldhook;

	return i;
}
/*}}}*/
/*{{{  int prescope_tree (tnode_t **t, langparser_t *lang)*/
/*
 *	performs the top-level pre-scope pass
 *	returns 0 on success, non-zero on failure
 */
int prescope_tree (tnode_t **t, langparser_t *lang)
{
	prescope_t *ps = (prescope_t *)smalloc (sizeof (prescope_t));
	int i;

	ps->err = 0;
	ps->warn = 0;
	if (!lang->prescope) {
		nocc_error ("prescope_tree(): don\'t know how to pre-scope this language!");
		sfree (ps);
		return 1;
	}
	ps->lang = lang;
	i = lang->prescope (t, ps);

	if (compopts.verbose || ps->err || ps->warn) {
		nocc_message ("prescope_tree(): pre-scoped.  %d error(s), %d warning(s)", ps->err, ps->warn);
	}

	if (ps->err) {
		i = ps->err;
	}
	sfree (ps);
	return i;
}
/*}}}*/

