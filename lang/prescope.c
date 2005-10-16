/*
 *	prescope.c -- nocc pre-scope'r
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


/*{{{  void prescope_init (void)*/
/*
 *	initialises pre-scope
 */
void prescope_init (void)
{
	return;
}
/*}}}*/
/*{{{  void prescope_shutdown (void)*/
/*
 *	shuts-down pre-scope
 */
void prescope_shutdown (void)
{
	return;
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
	ps->warn++;
	nocc_message (warnbuf);
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
	ps->err++;
	nocc_message (warnbuf);
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

	if ((*node)->tag->ndef->ops && (*node)->tag->ndef->ops->prescope) {
		i = (*node)->tag->ndef->ops->prescope (node, (prescope_t *)arg);
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

	nocc_message ("prescope_tree(): pre-scoped.  %d error(s), %d warning(s)", ps->err, ps->warn);

	if (ps->err) {
		i = ps->err;
	}
	sfree (ps);
	return i;
}
/*}}}*/

