/*
 *	typecheck.c -- nocc's type-checker
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
#include "typecheck.h"


/*{{{  void typecheck_init (void)*/
/*
 *	initialises type-checker
 */
void typecheck_init (void)
{
	return;
}
/*}}}*/
/*{{{  void typecheck_shutdown (void)*/
/*
 *	shuts-down type-checker
 */
void typecheck_shutdown (void)
{
	return;
}
/*}}}*/


/*{{{  void typecheck_warning (tnode_t *node, typecheck_t *tc, const char *fmt, ...)*/
/*
 *	called by pre-scoper bits for warnings
 */
void typecheck_warning (tnode_t *node, typecheck_t *tc, const char *fmt, ...)
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
	tc->warn++;
	nocc_message (warnbuf);
	sfree (warnbuf);

	return;
}
/*}}}*/
/*{{{  void typecheck_error (tnode_t *node, typecheck_t *tc, const char *fmt, ...)*/
/*
 *	called by the pre-scoper bits for errors
 */
void typecheck_error (tnode_t *node, typecheck_t *tc, const char *fmt, ...)
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
	tc->err++;
	nocc_message (warnbuf);
	sfree (warnbuf);

	return;
}
/*}}}*/


/*{{{  int typecheck_prewalktree (tnode_t *node, void *arg)*/
/*
 *	generic type-check
 */
int typecheck_prewalktree (tnode_t *node, void *arg)
{
	int i = 1;

	if (node->tag->ndef->ops && node->tag->ndef->ops->typecheck) {
		i = node->tag->ndef->ops->typecheck (node, (typecheck_t *)arg);
	}
	return i;
}
/*}}}*/
/*{{{  tnode_t *typecheck_gettype (tnode_t *node, tnode_t *default_type)*/
/*
 *	returns the type of a node
 */
tnode_t *typecheck_gettype (tnode_t *node, tnode_t *default_type)
{
	tnode_t *type;

	if (!node->tag->ndef->ops || !node->tag->ndef->ops->gettype) {
		nocc_internal ("typecheck_gettype(): don't know how to get type of [%s]!", node->tag->ndef->name);
		return NULL;
	}
	type = node->tag->ndef->ops->gettype (node, default_type);

	return type;
}
/*}}}*/
/*{{{  tnode_t *typecheck_typeactual (tnode_t *formaltype, tnode_t *actualtype, tnode_t *node, typecheck_t *tc)*/
/*
 *	tests to see whether one type is compatible with another (in a formal/actual sense)
 *	returns the actual type used for the operation on success, NULL on failure
 */
tnode_t *typecheck_typeactual (tnode_t *formaltype, tnode_t *actualtype, tnode_t *node, typecheck_t *tc)
{
	tnode_t *usedtype;

	if (!formaltype->tag->ndef->ops || !formaltype->tag->ndef->ops->typeactual) {
		/* don't have a check on the formal-type, blind comparison */
		if (formaltype->tag == actualtype->tag) {
			return actualtype;		/* types compatible..  FIXME! */
		}
		return NULL;
	}
	usedtype = formaltype->tag->ndef->ops->typeactual (formaltype, actualtype, node, tc);

	return usedtype;
}
/*}}}*/
/*{{{  int typecheck_tree (tnode_t *t, langparser_t *lang)*/
/*
 *	performs the top-level type-check pass
 */
int typecheck_tree (tnode_t *t, langparser_t *lang)
{
	typecheck_t *tc = (typecheck_t *)smalloc (sizeof (typecheck_t));
	int i;

	tc->err = 0;
	tc->warn = 0;
	if (!lang->typecheck) {
		nocc_error ("typecheck_tree(): don\'t know how to pre-scope this language!");
		sfree (tc);
		return 1;
	}
	i = lang->typecheck (t, tc);

	nocc_message ("typecheck_tree(): type-checked.  %d error(s), %d warning(s)", tc->err, tc->warn);

	if (tc->err) {
		i = tc->err;
	}
	sfree (tc);
	return i;
}
/*}}}*/

