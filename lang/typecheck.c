/*
 *	typecheck.c -- nocc's type-checker
 *	Copyright (C) 2005-2007 Fred Barnes <frmb@kent.ac.uk>
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
 *	called by type-checker for warnings
 */
void typecheck_warning (tnode_t *node, typecheck_t *tc, const char *fmt, ...)
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
	tc->warn++;
	nocc_message ("%s", warnbuf);
	sfree (warnbuf);

	return;
}
/*}}}*/
/*{{{  void typecheck_error (tnode_t *node, typecheck_t *tc, const char *fmt, ...)*/
/*
 *	called by the type-checker for errors
 */
void typecheck_error (tnode_t *node, typecheck_t *tc, const char *fmt, ...)
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
	tc->err++;
	nocc_message ("%s", warnbuf);
	sfree (warnbuf);

	return;
}
/*}}}*/


/*{{{  static typecheck_t *typecheck_newtypecheck (void)*/
/*
 *	creates a new typecheck_t structure
 */
static typecheck_t *typecheck_newtypecheck (void)
{
	typecheck_t *tc = (typecheck_t *)smalloc (sizeof (typecheck_t));

	tc->err = 0;
	tc->warn = 0;
	tc->hook = NULL;
	tc->lang = NULL;

	tc->this_ftype = NULL;
	tc->this_aparam = NULL;

	tc->this_protocol = NULL;

	return tc;
}
/*}}}*/
/*{{{  static void typecheck_freetypecheck (typecheck_t *tc)*/
/*
 *	frees a typecheck_t structure
 */
static void typecheck_freetypecheck (typecheck_t *tc)
{
	if (!tc) {
		nocc_error ("typecheck_freetypecheck(): NULL pointer!");
		return;
	}
	sfree (tc);
	return;
}
/*}}}*/


/*{{{  int typecheck_haserror (typecheck_t *tc)*/
/*
 *	returns the number of errors collected by the type-check so far
 */
int typecheck_haserror (typecheck_t *tc)
{
	return tc->err;
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

	if (parser_islistnode (node)) {
		/*{{{  special case: type of a list is the list of types of its elements*/
		int nitems;
		tnode_t **items = parser_getlistitems (node, &nitems);
		tnode_t *typelist = parser_newlistnode (NULL);

#if 0
fprintf (stderr, "typecheck_gettype(): on list.  node =\n");
tnode_dumptree (node, 1, stderr);
fprintf (stderr, "typecheck_gettype(): on list.  default_type =\n");
tnode_dumptree (default_type, 1, stderr);
#endif
		if (default_type && parser_islistnode (default_type)) {
			/* default type is a list too, pull into elements */
			int ndtypes, i;
			tnode_t **deftypes = parser_getlistitems (default_type, &ndtypes);

			if (nitems != ndtypes) {
				/* wrongness */
				parser_trashlist (typelist);
				typelist = NULL;
			} else {
				for (i=0; i<nitems; i++) {
					tnode_t *rtype = typecheck_gettype (items[i], deftypes[i]);

					parser_addtolist (typelist, rtype);
				}
			}
		} else {
			int i;

			for (i=0; i<nitems; i++) {
				tnode_t *rtype = typecheck_gettype (items[i], NULL);

				parser_addtolist (typelist, rtype);
			}
		}

#if 0
fprintf (stderr, "typecheck_gettype(): on list.  resulting typelist =\n");
tnode_dumptree (typelist, 1, stderr);
#endif
		return typelist;
		/*}}}*/
	}

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
	/* tnode_t *usedtype = NULL;*/
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
			nocc_message ("typecheck_typereduce(): reducing (%s,%s)", type->tag->ndef->name, type->tag->name);
			/*}}}*/
		}
		return (tnode_t *)tnode_calllangop_i (type->tag->ndef->lops, (int)LOPS_TYPEREDUCE, 1, type);
	}
	return NULL;
}
/*}}}*/
/*{{{  int typecheck_cantypecast (tnode_t *node, tnode_t *srctype)*/
/*
 *	determines whether one type can be cast into another.  'node' should be the
 *	target type, 'srctype' is the source type
 *	returns 0 if not, 1 otherwise
 */
int typecheck_cantypecast (tnode_t *node, tnode_t *srctype)
{
	if (!node || !srctype) {
		return 0;
	}
	if (node->tag->ndef->lops && tnode_haslangop_i (node->tag->ndef->lops, (int)LOPS_CANTYPECAST)) {
		if (compopts.tracetypecheck) {
			/*{{{  report attempted type-cast*/
			nocc_message ("typecheck_cantypecast(): checking whether (%s,%s) can be cast to (%s,%s)", node->tag->ndef->name, node->tag->name,
					srctype->tag->ndef->name, srctype->tag->name);
			/*}}}*/
		}
		return tnode_calllangop_i (node->tag->ndef->lops, (int)LOPS_CANTYPECAST, 2, node, srctype);
	}
	tnode_warning (node, "typecheck_cantypecast(): don\'t know how to check type-casts to (%s,%s)", node->tag->ndef->name, node->tag->name);
	return 0;
}
/*}}}*/
/*{{{  int typecheck_istype (tnode_t *node)*/
/*
 *	returns non-zero if the specified node is a type
 */
int typecheck_istype (tnode_t *node)
{
	if (!node) {
		return 0;
	}
	if (node->tag->ndef->lops && tnode_haslangop_i (node->tag->ndef->lops, (int)LOPS_ISTYPE)) {
		if (compopts.tracetypecheck) {
			/*{{{  report attempted is-type-check*/
			nocc_message ("typecheck_istype(): checking whether (%s,%s) is a type", node->tag->ndef->name, node->tag->name);
			/*}}}*/
		}
		return tnode_calllangop_i (node->tag->ndef->lops, (int)LOPS_ISTYPE, 1, node);
	}
	return 0;
}
/*}}}*/
/*{{{  typecat_e typecheck_typetype (tnode_t *node)*/
/*
 *	returns the type-category for a type node
 */
typecat_e typecheck_typetype (tnode_t *node)
{
	if (!node) {
		return TYPE_NOTTYPE;
	}
	if (node->tag->ndef->lops && tnode_haslangop_i (node->tag->ndef->lops, (int)LOPS_TYPETYPE)) {
		return (typecat_e)tnode_calllangop_i (node->tag->ndef->lops, (int)LOPS_TYPETYPE, 1, node);
	}
	nocc_message ("typecheck_typetype(): called for non-supporting node! (%s,%s)", node->tag->ndef->name, node->tag->name);
	return TYPE_NOTTYPE;

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
	typecheck_t *tc = typecheck_newtypecheck ();
	int i;

	if (!lang->typecheck) {
		nocc_error ("typecheck_tree(): don\'t know how to type-check this language!");
		typecheck_freetypecheck (tc);
		return 1;
	}
	tc->lang = lang;
	i = lang->typecheck (t, tc);

	if (compopts.verbose || tc->err || tc->warn) {
		nocc_message ("typecheck_tree(): type-checked.  %d error(s), %d warning(s)", tc->err, tc->warn);
	}

	if (tc->err) {
		i = tc->err;
	}
	typecheck_freetypecheck (tc);

	return i;
}
/*}}}*/


/*{{{  int typeresolve_modprewalktree (tnode_t **nodep, void *arg)*/
/*
 *	generic type-check
 */
int typeresolve_modprewalktree (tnode_t **nodep, void *arg)
{
	int i = 1;

	if (*nodep && (*nodep)->tag->ndef->ops && tnode_hascompop_i ((*nodep)->tag->ndef->ops, (int)COPS_TYPERESOLVE)) {
		i = tnode_callcompop_i ((*nodep)->tag->ndef->ops, (int)COPS_TYPERESOLVE, 2, nodep, (typecheck_t *)arg);
	}
	return i;
}
/*}}}*/
/*{{{  int typeresolve_subtree (tnode_t **tptr, typecheck_t *tc)*/
/*
 *	performs a sub type-reduce
 *	returns 0 on success, non-zero on failure
 */
int typeresolve_subtree (tnode_t **tptr, typecheck_t *tc)
{
	int saved_err = tc->err;
	int saved_warn = tc->warn;
	int i;

	tc->err = 0;
	tc->warn = 0;
	i = tc->lang->typeresolve (tptr, tc);

	if (tc->err) {
		i = tc->err;
	}

	tc->err += saved_err;
	tc->warn += saved_warn;

	return i;
}
/*}}}*/
/*{{{  int typeresolve_tree (tnode_t **tptr, langparser_t *lang)*/
/*
 *	performs the top-level type-resolve pass
 *	returns 0 on success, non-zero on failure
 */
int typeresolve_tree (tnode_t **tptr, langparser_t *lang)
{
	typecheck_t *tc = (typecheck_t *)smalloc (sizeof (typecheck_t));
	int i;

	tc->err = 0;
	tc->warn = 0;
	if (!lang->typeresolve) {
		/* only certain languages may need this (occam-pi) */
		sfree (tc);
		return 0;
	}
	tc->lang = lang;
	i = lang->typeresolve (tptr, tc);

	if (compopts.verbose || tc->err || tc->warn) {
		nocc_message ("typeresolve_tree(): type-resolved.  %d error(s), %d warning(s)", tc->err, tc->warn);
	}

	if (tc->err) {
		i = tc->err;
	}
	sfree (tc);
	return i;
}
/*}}}*/


