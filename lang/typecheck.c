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

	if (node->tag->ndef->ops && tnode_hascompop_i (node->tag->ndef->ops, (int)COPS_TYPECHECK)) {
		i = tnode_callcompop_i (node->tag->ndef->ops, (int)COPS_TYPECHECK, 2, node, (typecheck_t *)arg);
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

	if (!node->tag->ndef->lops || !tnode_haslangop_i (node->tag->ndef->lops, (int)LOPS_GETTYPE)) {
		nocc_internal ("typecheck_gettype(): don't know how to get type of [%s]", node->tag->ndef->name);
		return NULL;
	}
	type = (tnode_t *)tnode_calllangop_i (node->tag->ndef->lops, (int)LOPS_GETTYPE, 2, node, default_type);

	return type;
}
/*}}}*/
/*{{{  tnode_t *typecheck_getsubtype (tnode_t *node, tnode_t *default_type)*/
/*
 *	returns the sub-type of a type node
 */
tnode_t *typecheck_getsubtype (tnode_t *node, tnode_t *default_type)
{
	tnode_t *type;

	if (!node->tag->ndef->lops || !tnode_haslangop_i (node->tag->ndef->lops, (int)LOPS_GETSUBTYPE)) {
		nocc_internal ("typecheck_getsubtype(): don't know how to get sub-type of [%s]", node->tag->ndef->name);
		return NULL;
	}
	type = (tnode_t *)tnode_calllangop_i (node->tag->ndef->lops, (int)LOPS_GETSUBTYPE, 2, node, default_type);

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
	tnode_t *usedtype = NULL;

	if (compopts.tracetypecheck) {
		/*{{{  report type-check*/
		nocc_message ("typecheck_typeactual(): checking [%s (%s)] applied to [%s (%s)] with [%s (%s)]", actualtype->tag->name, actualtype->tag->ndef->name,
				formaltype->tag->name, formaltype->tag->ndef->name, node ? node->tag->name : "(null)", node ? node->tag->ndef->name : "(null)");
		/*}}}*/
	}

	if (formaltype->tag->ndef->lops && tnode_haslangop_i (formaltype->tag->ndef->lops, (int)LOPS_TYPEACTUAL)) {
		usedtype = (tnode_t *)tnode_calllangop_i (formaltype->tag->ndef->lops, (int)LOPS_TYPEACTUAL, 4, formaltype, actualtype, node, tc);
	} else {
		if (typecheck_fixedtypeactual (formaltype, actualtype, node, tc, 0)) {
			/* assume OK and use the actual-type */
			usedtype = actualtype;
		}
	}

	if (!usedtype) {
		/* if the actual-type can be reduced, test that instead */
		tnode_t *rtype = typecheck_typereduce (actualtype);

		if (rtype) {
			usedtype = typecheck_typeactual (formaltype, rtype, node, tc);
		}
	}

	return usedtype;
}
/*}}}*/
/*{{{  tnode_t *typecheck_fixedtypeactual (tnode_t *formaltype, tnode_t *actualtype, tnode_t *node, typecheck_t *tc, const int deep)*/
/*
 *	does type compatibility tests on plain type-trees (doesn't use typeactual operator)
 *	if "deep" is non-zero, all descendants are checked with this, otherwise the usual typeactual
 *	is used.
 *	returns the actual type used for the operation on success, NULL on failure
 */
tnode_t *typecheck_fixedtypeactual (tnode_t *formaltype, tnode_t *actualtype, tnode_t *node, typecheck_t *tc, const int deep)
{
	tnode_t *usedtype = NULL;
	tnode_t **f_subnodes;
	tnode_t **a_subnodes;
	int f_nodes, a_nodes;
	int i;

	if (compopts.tracetypecheck) {
		/*{{{  report type-check*/
		nocc_message ("typecheck_fixedtypeactual(): checking [%s (%s)] applied to [%s (%s)] with [%s (%s)]", actualtype->tag->name, actualtype->tag->ndef->name,
				formaltype->tag->name, formaltype->tag->ndef->name, node ? node->tag->name : "(null)", node ? node->tag->ndef->name : "(null)");
		/*}}}*/
	}

	/* blind comparison */
	if (formaltype->tag != actualtype->tag) {
		return NULL;
	}

	f_subnodes = tnode_subnodesof (formaltype, &f_nodes);
	a_subnodes = tnode_subnodesof (actualtype, &a_nodes);

	for (i=0; (i<f_nodes) && (i<a_nodes); i++) {
		tnode_t *match;

		if (deep) {
			match = typecheck_fixedtypeactual (f_subnodes[i], a_subnodes[i], node, tc, deep);
		} else {
			match = typecheck_typeactual (f_subnodes[i], a_subnodes[i], node, tc);
		}
		if (!match) {
			return NULL;		/* failed */
		}
	}

	/* assume OK and return the actual type */
	return actualtype;
}
/*}}}*/
/*{{{  tnode_t *typecheck_typereduce (tnode_t *type)*/
/*
 *	returns a reduced type, or NULL.  This is currently used for de-mobilising MOBILE types, since
 *	the underlying type can be referred to without fundamentally changing the type.
 */
tnode_t *typecheck_typereduce (tnode_t *type)
{
	if (!type) {
		return NULL;
	}
	if (type->tag->ndef->lops && tnode_haslangop_i (type->tag->ndef->lops, (int)LOPS_TYPEREDUCE)) {
		if (compopts.tracetypecheck) {
			/*{{{  report attempted type reduction*/
			nocc_message ("typecheck_typereduce(): reducing [%s (%s)]", type->tag->name, type->tag->ndef->name);
			/*}}}*/
		}
		return (tnode_t *)tnode_calllangop_i (type->tag->ndef->lops, (int)LOPS_TYPEREDUCE, 1, type);
	}
	return NULL;
}
/*}}}*/
/*{{{  int typecheck_subtree (tnode_t *t, typecheck_t *tc)*/
/*
 *	performs a sub type-check
 *	returns 0 on success, non-zero on failure
 */
int typecheck_subtree (tnode_t *t, typecheck_t *tc)
{
	int saved_err = tc->err;
	int saved_warn = tc->warn;
	int i;

	tc->err = 0;
	tc->warn = 0;
	i = tc->lang->typecheck (t, tc);

	if (tc->err) {
		i = tc->err;
	}

	tc->err += saved_err;
	tc->warn += saved_warn;

	return i;
}
/*}}}*/
/*{{{  int typecheck_tree (tnode_t *t, langparser_t *lang)*/
/*
 *	performs the top-level type-check pass
 *	returns 0 on success, non-zero on failure
 */
int typecheck_tree (tnode_t *t, langparser_t *lang)
{
	typecheck_t *tc = (typecheck_t *)smalloc (sizeof (typecheck_t));
	int i;

	tc->err = 0;
	tc->warn = 0;
	if (!lang->typecheck) {
		nocc_error ("typecheck_tree(): don\'t know how to type-check this language!");
		sfree (tc);
		return 1;
	}
	tc->lang = lang;
	i = lang->typecheck (t, tc);

	nocc_message ("typecheck_tree(): type-checked.  %d error(s), %d warning(s)", tc->err, tc->warn);

	if (tc->err) {
		i = tc->err;
	}
	sfree (tc);
	return i;
}
/*}}}*/

