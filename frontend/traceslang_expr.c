/*
 *	traceslang_expr.c -- traces language expression handling
 *	Copyright (C) 2007 Fred Barnes <frmb@kent.ac.uk>
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
#include "origin.h"
#include "symbols.h"
#include "keywords.h"
#include "lexer.h"
#include "lexpriv.h"
#include "tnode.h"
#include "parser.h"
#include "fcnlib.h"
#include "langops.h"
#include "treeops.h"
#include "dfa.h"
#include "parsepriv.h"
#include "traceslang.h"
#include "traceslang_fe.h"
#include "feunit.h"
#include "names.h"
#include "scope.h"
#include "prescope.h"
#include "typecheck.h"
#include "usagecheck.h"
#include "map.h"
#include "codegen.h"
#include "target.h"
#include "transputer.h"
#include "tracescheck.h"

/*}}}*/

/*{{{  private types*/

typedef struct TAG_traceslang_lithook {
	char *data;
	int len;
} traceslang_lithook_t;

typedef struct TAG_dopmap {
	tokentype_t ttype;
	const char *lookup;
	token_t *tok;
	ntdef_t **tagp;
} dopmap_t;


/*}}}*/
/*{{{  private data*/
static dopmap_t dopmap[] = {
	{SYMBOL, "->", NULL, &(traceslang.tag_SEQ)},
	{SYMBOL, ";", NULL, &(traceslang.tag_SEQ)},
	{SYMBOL, "[]", NULL, &(traceslang.tag_DET)},
	{SYMBOL, "|~|", NULL, &(traceslang.tag_NDET)},
	{SYMBOL, "||", NULL, &(traceslang.tag_PAR)},
	{NOTOKEN, NULL, NULL, NULL}
};

/*}}}*/


/*{{{  static void *traceslang_nametoken_to_hook (void *ntok)*/
/*
 *	turns a name token into a hooknode for a tag_NAME
 */
static void *traceslang_nametoken_to_hook (void *ntok)
{
	token_t *tok = (token_t *)ntok;
	char *rawname;

	rawname = tok->u.name;
	tok->u.name = NULL;

	lexer_freetoken (tok);

	return (void *)rawname;
}
/*}}}*/
/*{{{  static void *traceslang_stringtoken_to_node (void *ntok)*/
/*
 *	turns a string token in to a LITSTR node
 */
static void *traceslang_stringtoken_to_node (void *ntok)
{
	token_t *tok = (token_t *)ntok;
	tnode_t *node = NULL;
	traceslang_lithook_t *litdata = (traceslang_lithook_t *)smalloc (sizeof (traceslang_lithook_t));

	if (tok->type != STRING) {
		lexer_error (tok->origin, "expected string, found [%s]", lexer_stokenstr (tok));
		sfree (litdata);
		lexer_freetoken (tok);
		return NULL;
	} 
	litdata->data = string_ndup (tok->u.str.ptr, tok->u.str.len);
	litdata->len = tok->u.str.len;

	node = tnode_create (traceslang.tag_LITSTR, tok->origin, (void *)litdata);
	lexer_freetoken (tok);

	return (void *)node;
}
/*}}}*/
/*{{{  static void *traceslang_integertoken_to_node (void *ntok)*/
/*
 *	turns an integer token into a LITINT node
 */
static void *traceslang_integertoken_to_node (void *ntok)
{
	token_t *tok = (token_t *)ntok;
	tnode_t *node = NULL;
	traceslang_lithook_t *litdata = (traceslang_lithook_t *)smalloc (sizeof (traceslang_lithook_t));

	if (tok->type != INTEGER) {
		lexer_error (tok->origin, "expected integer, found [%s]", lexer_stokenstr (tok));
		sfree (litdata);
		lexer_freetoken (tok);
		return NULL;
	} 
	litdata->data = mem_ndup (&(tok->u.ival), sizeof (int));
	litdata->len = 4;

	node = tnode_create (traceslang.tag_LITINT, tok->origin, (void *)litdata);
	lexer_freetoken (tok);

	return (void *)node;
}
/*}}}*/


/*{{{  static void traceslang_rawnamenode_hook_free (void *hook)*/
/*
 *	frees a rawnamenode hook (name-bytes)
 */
static void traceslang_rawnamenode_hook_free (void *hook)
{
	if (hook) {
		sfree (hook);
	}
	return;
}
/*}}}*/
/*{{{  static void *traceslang_rawnamenode_hook_copy (void *hook)*/
/*
 *	copies a rawnamenode hook (name-bytes)
 */
static void *traceslang_rawnamenode_hook_copy (void *hook)
{
	char *rawname = (char *)hook;

	if (rawname) {
		return string_dup (rawname);
	}
	return NULL;
}
/*}}}*/
/*{{{  static void traceslang_rawnamenode_hook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	dump-tree for rawnamenode hook (name-bytes)
 */
static void traceslang_rawnamenode_hook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	traceslang_isetindent (stream, indent);
	fprintf (stream, "<traceslangrawnamenode value=\"%s\" />\n", hook ? (char *)hook : "(null)");
	return;
}
/*}}}*/

/*{{{  static void traceslang_litnode_hook_free (void *hook)*/
/*
 *	frees a litnode hook
 */
static void traceslang_litnode_hook_free (void *hook)
{
	traceslang_lithook_t *ld = (traceslang_lithook_t *)hook;

	if (ld) {
		if (ld->data) {
			sfree (ld->data);
		}
		sfree (ld);
	}

	return;
}
/*}}}*/
/*{{{  static void *traceslang_litnode_hook_copy (void *hook)*/
/*
 *	copies a litnode hook (name-bytes)
 */
static void *traceslang_litnode_hook_copy (void *hook)
{
	traceslang_lithook_t *lit = (traceslang_lithook_t *)hook;

	if (lit) {
		traceslang_lithook_t *newlit = (traceslang_lithook_t *)smalloc (sizeof (traceslang_lithook_t));

		newlit->data = lit->data ? mem_ndup (lit->data, lit->len) : NULL;
		newlit->len = lit->len;

		return (void *)newlit;
	}
	return NULL;
}
/*}}}*/
/*{{{  static void traceslang_litnode_hook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	dump-tree for litnode hook (name-bytes)
 */
static void traceslang_litnode_hook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	traceslang_lithook_t *lit = (traceslang_lithook_t *)hook;

	traceslang_isetindent (stream, indent);
	if (node->tag == traceslang.tag_LITSTR) {
		fprintf (stream, "<traceslanglitnode size=\"%d\" value=\"%s\" />\n", lit ? lit->len : 0, (lit && lit->data) ? lit->data : "(null)");
	} else {
		char *sdata = mkhexbuf ((unsigned char *)lit->data, lit->len);

		fprintf (stream, "<traceslanglitnode size=\"%d\" value=\"%s\" />\n", lit ? lit->len : 0, sdata);
		sfree (sdata);
	}

	return;
}
/*}}}*/


/*{{{  static int traceslang_scopein_rawname (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	scopes in a traceslang name
 *	returns 0 to stop walk, 1 to continue
 */
static int traceslang_scopein_rawname (compops_t *cops, tnode_t **node, scope_t *ss)
{
	tnode_t *name = *node;
	char *rawname;
	name_t *sname = NULL;

	if (name->tag != traceslang.tag_NAME) {
		scope_error (name, ss, "name not raw-name!");
		return 0;
	}
	rawname = (char *)tnode_nthhookof (name, 0);
	sname = name_lookupss (rawname, ss);
	if (sname) {
		/* resolved */
		tnode_t *rnode = NameNodeOf (sname);

#if 0
fprintf (stderr, "traceslang_scopein_rawname(): found name, node tag: %s\n", rnode->tag->name);
#endif
		if ((rnode->tag == traceslang.tag_NPARAM) || (rnode->tag == traceslang.tag_NFIX)) {
			*node = rnode;
			tnode_free (name);
		} else if (traceslang_isregisteredtracetype (rnode->tag)) {
			*node = rnode;
			tnode_free (name);
		} else {
			scope_error (name, ss, "name [%s] is not a trace parameter or name", rawname);
			return 0;
		}
	} else {
		scope_error (name, ss, "unresolved name \"%s\"", rawname);
	}

	return 1;
}
/*}}}*/

/*{{{  static int traceslang_scopeout_setnode (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	does scope-out on a set node (used to flatten out nodes)
 *	returns 0 to stop walk, 1 to continue
 */
static int traceslang_scopeout_setnode (compops_t *cops, tnode_t **node, scope_t *ss)
{
	tnode_t *nd = *node;
	ntdef_t *tag = nd->tag;
	tnode_t *list = tnode_nthsubof (nd, 0);
	tnode_t **items;
	int n, i;

	items = parser_getlistitems (list, &n);
	for (i=0; i<n; i++) {
		int saved_i = i;
		tnode_t *item = items[i];

		if (item && (item->tag == tag)) {
			/* this is the same, pull in items */
			int m, j;
			tnode_t **subitems = parser_getlistitems (tnode_nthsubof (item, 0), &m);

			for (j=0; j<m; j++) {
				if (subitems[j]) {
					parser_insertinlist (list, subitems[j], i+1);
					i++;
					subitems[j] = NULL;
				}
			}

			/* now trash it */
			tnode_free (item);
			parser_delfromlist (list, saved_i);
			i--;
		}
	}

	return 1;
}
/*}}}*/

/*{{{  static int traceslang_scopein_ionode (compops_t *cops, tnode_t **nodep, scope_t *ss)*/
/*
 *	scopes-in a traces-lang io-node
 *	returns 0 to stop walk, 1 to continue
 */
static int traceslang_scopein_ionode (compops_t *cops, tnode_t **nodep, scope_t *ss)
{
	if (((*nodep)->tag == traceslang.tag_INPUT) || ((*nodep)->tag == traceslang.tag_OUTPUT)) {
		/*{{{  input or output node*/
		tnode_t **tagsp = tnode_nthsubaddr (*nodep, 1);

		/* scope LHS (should be channel or somesuch) */
		scope_subtree (tnode_nthsubaddr (*nodep, 0), ss);

#if 0
fprintf (stderr, "traceslang_scopein_ionode(): here! *tagsp = 0x%8.8x\n", (unsigned int)(*tagsp));
#endif
		if (*tagsp) {
			/* got a tag-pointer, see if the LHS has tags */
			tnode_t *lhs = tnode_nthsubof (*nodep, 0);
			tnode_t *lhstags = langops_gettags (lhs);
			void *namemark = name_markscope ();

#if 0
fprintf (stderr, "traceslang_scopein_ionode(): here2! lhs =\n");
tnode_dumptree (lhs, 1, stderr);
fprintf (stderr, "traceslang_scopein_ionode(): here2! lhstags =\n");
tnode_dumptree (lhstags, 1, stderr);
#endif

			if (!lhstags) {
				/* ignore for now, will fail in typecheck */
			} else {
				int nitems, i;
				tnode_t **tagnamelist = parser_getlistitems (lhstags, &nitems);

#if 0
fprintf (stderr, "traceslang_scopein_ionode(): got tag pointer and LHS tags!\n");
#endif
				for (i=0; i<nitems; i++) {
					name_t *tagname = langops_nameof (tagnamelist[i]);

					if (tagname) {
						name_scopename (tagname);
					}
				}
			}

			scope_subtree (tnode_nthsubaddr (*nodep, 1), ss);

			name_markdescope (namemark);
		}

		return 0;
		/*}}}*/
	}
	return 1;
}
/*}}}*/
/*{{{  static int traceslang_typecheck_ionode (compops_t *cops, tnode_t *node, typecheck_t *tc)*/
/*
 *	does type-checking on a traces io-node
 *	returns 0 to stop walk, 1 to continue
 */
static int traceslang_typecheck_ionode (compops_t *cops, tnode_t *node, typecheck_t *tc)
{
	tnode_t *arg = tnode_nthsubof (node, 0);
	tnode_t *type;
	char *name = NULL;

	typecheck_subtree (arg, tc);
	type = typecheck_gettype (arg, NULL);

	if (!type) {
		langops_getname (arg, &name);
		typecheck_error (node, tc, "name [%s] has no type (used in input/output)", name ?: "(unknown)");
	} else if (type->tag != traceslang.tag_EVENT) {
		langops_getname (arg, &name);
		typecheck_error (node, tc, "input or output on non-event [%s]", name ?: "(unknown)");
	}
#if 0
fprintf (stderr, "traceslang_typecheck_ionode(): got type:\n");
tnode_dumptree (type, 1, stderr);
#endif

	return 0;
}
/*}}}*/

/*{{{  static int traceslang_prescope_instancenode (compops_t *cops, tnode_t **tptr, prescope_t *ps)*/
/*
 *	does pre-scoping on a traces instance node, makes sure the parameters are a list
 *	returns 0 to stop walk, 1 to continue
 */
static int traceslang_prescope_instancenode (compops_t *cops, tnode_t **tptr, prescope_t *ps)
{
	tnode_t *params = tnode_nthsubof (*tptr, 1);

	if (!parser_islistnode (params)) {
		params = parser_buildlistnode (NULL, params, NULL);
		tnode_setnthsub (*tptr, 1, params);
	}
	return 1;
}
/*}}}*/
/*{{{  static int traceslang_typecheck_instancenode (compops_t *cops, tnode_t *node, typecheck_t *tc)*/
/*
 *	does type-checking on a traces instance node
 *	returns 0 to stop walk, 1 to continue
 */
static int traceslang_typecheck_instancenode (compops_t *cops, tnode_t *node, typecheck_t *tc)
{
	tnode_t *iname = tnode_nthsubof (node, 0);

#if 0
fprintf (stderr, "traceslang_typecheck_instancenode(): type-checking instance, name is:\n");
tnode_dumptree (iname, 1, stderr);
#endif
	/* the name (whatever it is) should support traceslang_getparams() and traceslang_getbody() */
	if (!tnode_haslangop (iname->tag->ndef->lops, "traceslang_getparams") || !tnode_haslangop (iname->tag->ndef->lops, "traceslang_getbody")) {
		char *name = NULL;

		langops_getname (iname, &name);
		typecheck_error (node, tc, "%s is not a valid trace name for instance", name ?: "(unknown)");
		if (name) {
			sfree (name);
		}

		return 0;
	}

	/* we'll do the substitution and check parameter types later */

	return 1;
}
/*}}}*/
/*{{{  static int traceslang_typeresolve_instancenode (compops_t *cops, tnode_t **tptr, typecheck_t *tc)*/
/*
 *	does type-resolution on a traces instance node -- effectively substitutes the trace, literally
 *	returns 0 to stop walk, 1 to continue
 */
static int traceslang_typeresolve_instancenode (compops_t *cops, tnode_t **tptr, typecheck_t *tc)
{
	tnode_t **inamep = tnode_nthsubaddr (*tptr, 0);
	tnode_t **iargsp = tnode_nthsubaddr (*tptr, 1);
	tnode_t *xparams, *xbody;
	char *irname = NULL;

	xparams = (tnode_t *)tnode_calllangop ((*inamep)->tag->ndef->lops, "traceslang_getparams", 1, *inamep);
	xbody = (tnode_t *)tnode_calllangop ((*inamep)->tag->ndef->lops, "traceslang_getbody", 1, *inamep);

	/* do type-resolution on the subtrees first */

#if 0
fprintf (stderr, "traceslang_typeresolve_instancenode(): got parameters of named trace-type:\n");
tnode_dumptree (xparams, 1, stderr);
fprintf (stderr, "traceslang_typeresolve_instancenode(): got body of named trace-type:\n");
tnode_dumptree (xbody, 1, stderr);
fprintf (stderr, "traceslang_typeresolve_instancenode(): local parameters are:\n");
tnode_dumptree (*iargsp, 1, stderr);
#endif

	if (!xbody) {
		langops_getname (*inamep, &irname);
		typecheck_error (*tptr, tc, "trace-type [%s] is blank!", irname ?: "(unknown)");
	} else if ((!xparams && *iargsp) || (xparams && !*iargsp)) {
		/* parameter inbalance */
		langops_getname (*inamep, &irname);
		typecheck_error (*tptr, tc, "instance of trace-type [%s] has the wrong number parameters", irname ?: "(unknown)");
	} else if (xparams && *iargsp) {
		/* check parameters */
		int nfparams, naparams, i;
		tnode_t **fparams = parser_getlistitems (xparams, &nfparams);
		tnode_t **aparams = parser_getlistitems (*iargsp, &naparams);

		if (nfparams != naparams) {
			langops_getname (*inamep, &irname);
			typecheck_error (*tptr, tc, "expected %d parameters for instance of trace-type [%s], but found %d",
					nfparams, irname ?: "(unknown)", naparams);
		} else {
			for (i=0; i<naparams; i++) {
				tnode_t *aparam = aparams[i];
				tnode_t *atype;

				if ((aparam->tag == traceslang.tag_INPUT) || (aparam->tag == traceslang.tag_OUTPUT)) {
					tnode_t *tmp = tnode_nthsubof (aparam, 0);

					/* remove input or output, XXX: we should probably check this, but that information has gone to the host language.. */
					tnode_setnthsub (aparam, 0, NULL);
					tnode_free (aparam);
					aparam = tmp;
					aparams[i] = tmp;
				}

				atype = typecheck_gettype (aparam, NULL);
				if (!atype || (atype->tag != traceslang.tag_EVENT)) {
					langops_getname (*inamep, &irname);
					typecheck_error (*tptr, tc, "parameter %d of trace-type instance [%s] is not an event", i+1, irname ?: "(unknown)");
				}
				/* else we'll assume it's good! */
#if 0
fprintf (stderr, "traceslang_typeresolve_instancenode(): formal parameter %d:\n", i);
tnode_dumptree (fparams[i], 1, stderr);
fprintf (stderr, "traceslang_typeresolve_instancenode(): actual parameter %d:\n", i);
tnode_dumptree (aparam, 1, stderr);
#endif
			}
		}

		/* if we don't have a type-check error, copy and substitute the trace into the instance */
		if (!typecheck_haserror (tc)) {
			tnode_t *copy = traceslang_structurecopy (xbody);

			copy = treeops_substitute (copy, fparams, aparams, nfparams);

			/* okay, if that ends in a Skip (successful termination), need to chop that off,
			 * otherwise wrap in a fixpoint (repeat indefinitely)
			 */

			copy = traceslang_listtondet (copy);
			traceslang_noskiporloop (&copy);

			*tptr = copy;
#if 0
fprintf (stderr, "traceslang_typeresolve_instancenode(): did substitution on instancenode, got:\n");
tnode_dumptree (*tptr, 1, stderr);
#endif
		}
	}

	if (irname) {
		sfree (irname);
	}

	/* don't walk resulting children -- will have already been done */
	return 0;
}
/*}}}*/

/*{{{  static int traceslang_scopein_fixpointnode (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	scopes-in a traceslang fixpoint node
 *	returns 0 to stop walk, 1 to continue
 */
static int traceslang_scopein_fixpointnode (compops_t *cops, tnode_t **node, scope_t *ss)
{
	tnode_t *name = tnode_nthsubof (*node, 0);
	tnode_t *type, *newname;
	char *rawname;
	void *nsmark;
	name_t *sname;

	nsmark = name_markscope ();

	if (name->tag != traceslang.tag_NAME) {
		scope_error (name, ss, "name not raw-name!");
		return 0;
	}
	rawname = (char *)tnode_nthhookof (name, 0);

	type = tnode_createfrom (traceslang.tag_FIXPOINTTYPE, *node);
	sname = name_addscopename (rawname, *node, type, NULL);
	newname = tnode_createfrom (traceslang.tag_NFIX, name, sname);
	SetNameNode (sname, newname);

	/* replace old name */
	tnode_free (name);
	tnode_setnthsub (*node, 0, newname);
	ss->scoped++;

	/* scope in the body */
	scope_subtree (tnode_nthsubaddr (*node, 1), ss);
	name_markdescope (nsmark);

	return 0;
}
/*}}}*/

/*{{{  static int traceslang_tracescheck_litnode (langops_t *lops, tnode_t *node, tchk_check_t *tcc)*/
/*
 *	does traces checks on a traceslang literal node
 *	returns 0 on success, non-zero on failure
 */
static int traceslang_tracescheck_litnode (langops_t *lops, tnode_t *node, tchk_check_t *tcc)
{
	tracescheck_checkwarning (node, tcc, "traceslang_tracescheck_litnode(): here!");
	return 0;
}
/*}}}*/
/*{{{  static int traceslang_tracescheck_setnode (langops_t *lops, tnode_t *node, tchk_check_t *tcc)*/
/*
 *	does traces checks on a traceslang set node
 *	returns 0 on success, non-zero on failure
 */
static int traceslang_tracescheck_setnode (langops_t *lops, tnode_t *node, tchk_check_t *tcc)
{
	if (node->tag == traceslang.tag_SEQ) {
		/*{{{  check a sequential trace specification, list of items*/
		int nitems, i;
		tnode_t **items = parser_getlistitems (tnode_nthsubof (node, 0), &nitems);
		tchknode_t *trc = NULL;

#if 0
fprintf (stderr, "traceslang_tracescheck_setnode(): SEQ: checking specification node:\n");
tnode_dumptree (node, 1, stderr);
fprintf (stderr, "traceslang_tracescheck_setnode(): SEQ: against actual trace:\n");
tracescheck_dumpnode (tcc->thistrace, 1, stderr);
fprintf (stderr, "traceslang_tracescheck_setnode(): SEQ: testing walk on trace..\n");
tracescheck_testwalk (tcc->thistrace);
#endif

		// trc = tracescheck_stepwalk (tcc->thiswalk);
		trc = tcc->thistrace;
#if 0
fprintf (stderr, "traceslang_tracescheck_setnode(): SEQ: got node from actual trace:\n");
tracescheck_dumpnode (trc, 1, stderr);
#endif
		if (trc->type == TCN_SEQ) {
			/* got a sequence of something in the trace, start walking it */
			trc = tracescheck_stepwalk (tcc->thiswalk);
		}
		for (i=0; i<nitems; i++) {
			tracescheck_dosubcheckspec (items[i], trc, tcc);
			trc = tracescheck_stepwalk (tcc->thiswalk);
		}
		/*}}}*/
	} else if (node->tag == traceslang.tag_PAR) {
	} else if (node->tag == traceslang.tag_DET) {
	} else if (node->tag == traceslang.tag_NDET) {
		tracescheck_checkerror (node, tcc, "missing check for trace specification type [%s]", node->tag->name);
		return 1;
	}
	return 0;
}
/*}}}*/
/*{{{  static int traceslang_tracescheck_ionode (langops_t *lops, tnode_t *node, tchk_check_t *tcc)*/
/*
 *	does traces checks on a traceslang I/O node
 *	returns 0 on success, non-zero on failure
 */
static int traceslang_tracescheck_ionode (langops_t *lops, tnode_t *node, tchk_check_t *tcc)
{
	tchknode_t *trc = NULL;
	tnode_t *item = tnode_nthsubof (node, 0);
#if 0
fprintf (stderr, "traceslang_tracescheck_ionode(): checking specification node:\n");
tnode_dumptree (node, 1, stderr);
fprintf (stderr, "traceslang_tracescheck_ionode(): against actual trace:\n");
tracescheck_dumpnode (tcc->thistrace, 1, stderr);
#endif

	if (node->tag == traceslang.tag_INPUT) {
		if (!tcc->thistrace || (tcc->thistrace->type != TCN_INPUT)) {
			tracescheck_checkerror ((tcc->thistrace ? tcc->thistrace->orgnode : node), tcc, "input in specification not matched");
			return 1;
		}
	} else if (node->tag == traceslang.tag_OUTPUT) {
		if (!tcc->thistrace || (tcc->thistrace->type != TCN_OUTPUT)) {
			tracescheck_checkerror ((tcc->thistrace ? tcc->thistrace->orgnode : node), tcc, "output in specification not matched");
			return 1;
		}
	}

	trc = tracescheck_stepwalk (tcc->thiswalk);

#if 0
fprintf (stderr, "traceslang_tracescheck_ionode(): checking inner specification node:\n");
tnode_dumptree (item, 1, stderr);
fprintf (stderr, "traceslang_tracescheck_ionode(): against actual trace item:\n");
tracescheck_dumpnode (trc, 1, stderr);
#endif
	/* whatever's in here should (TCN_INPUT/OUTPUT) should be a noderef and match our subnode */
	if (trc->type != TCN_NODEREF) {
		tracescheck_checkerror (node, tcc, "input/output item type %d not a node!", (int)trc->type);
		return 1;
	} else if (trc->u.tcnnref.nref != item) {
		tracescheck_checkerror (node, tcc, "input/output in specification not matched");
		return 1;
	}

	return 0;
}
/*}}}*/
/*{{{  static int traceslang_tracescheck_leafnode (langops_t *lops, tnode_t *node, tchk_check_t *tcc)*/
/*
 *	does traces checks on a traceslang leaf-node
 *	returns 0 on success, non-zero on failure
 */
static int traceslang_tracescheck_leafnode (langops_t *lops, tnode_t *node, tchk_check_t *tcc)
{
#if 0
fprintf (stderr, "traceslang_tracescheck_leafnode(): checking specification node:\n");
tnode_dumptree (node, 1, stderr);
fprintf (stderr, "traceslang_tracescheck_leafnode(): against actual trace:\n");
tracescheck_dumpnode (tcc->thistrace, 1, stderr);
#endif

	if (node->tag == traceslang.tag_SKIP) {
		/* must be termination of the traces */
		if (tcc->thistrace) {
			tracescheck_checkerror (tcc->thistrace->orgnode, tcc, "termination in specification not matched");
			return 1;
		}
	}

	// tracescheck_checkwarning (node, tcc, "traceslang_tracescheck_leafnode(): here!");
	return 0;
}
/*}}}*/
/*{{{  static int traceslang_tracescheck_fixpointnode (langops_t *lops, tnode_t *node, tchk_check_t *tcc)*/
/*
 *	does traces checks on a traceslang fixpoint-node
 *	returns 0 on success, non-zero on failure
 */
static int traceslang_tracescheck_fixpointnode (langops_t *lops, tnode_t *node, tchk_check_t *tcc)
{
	tracescheck_checkwarning (node, tcc, "traceslang_tracescheck_fixpointnode(): here!");
	return 0;
}
/*}}}*/
/*{{{  static int traceslang_tracescheck_namenode (langops_t *lops, tnode_t *node, tchk_check_t *tcc)*/
/*
 *	does traces checks on a traceslang name-node
 *	return 0 on success, non-zero on failure
 */
static int traceslang_tracescheck_namenode (langops_t *lops, tnode_t *node, tchk_check_t *tcc)
{
#if 0
fprintf (stderr, "traceslang_tracescheck_namenode(): checking specification node:\n");
tnode_dumptree (node, 1, stderr);
fprintf (stderr, "traceslang_tracescheck_namenode(): against actual trace:\n");
tracescheck_dumpnode (tcc->thistrace, 1, stderr);
#endif
	tracescheck_checkerror (node, tcc, "lone namenode in trace specification!");
	return 1;
}
/*}}}*/

/*{{{  static tnode_t *traceslang_gettype_namenode (langops_t *lops, tnode_t *node, tnode_t *default_type)*/
/*
 *	returns the type of a traceslang name
 */
static tnode_t *traceslang_gettype_namenode (langops_t *lops, tnode_t *node, tnode_t *default_type)
{
	name_t *name = tnode_nthnameof (node, 0);

	if (!name) {
		nocc_fatal ("traceslang_gettype_namenode(): NULL name!");
		return NULL;
	}
	if (name->type) {
		return name->type;
	}
	nocc_fatal ("traceslang_gettype_namenode(): name has NULL type!");
	return NULL;
}
/*}}}*/

/*{{{  static int traceslang_totrace_setnode (langops_t *lops, tnode_t *node, tchk_bucket_t *bucket)*/
/*
 *	converts a traceslang setnode into a traces-check node
 *	returns 0 to stop walk, 1 to continue
 */
static int traceslang_totrace_setnode (langops_t *lops, tnode_t *node, tchk_bucket_t *bucket)
{
	tnode_t *ilist = tnode_nthsubof (node, 0);
	tchknode_t *tcn;

#if 0
fprintf (stderr, "traceslang_totrace_setnode(): here!\n");
#endif
	if (!ilist || !parser_islistnode (ilist) || (parser_countlist (ilist) <= 0)) {
		tcn = tracescheck_createnode (TCN_SKIP, node);
	} else {
		int nitems, i;
		tnode_t **items = parser_getlistitems (ilist, &nitems);

		tcn = tracescheck_createnode (TCN_SEQ, node, NULL);
		for (i=0; i<nitems; i++) {
			tchknode_t *titem = tracescheck_totrace (items[i]);

			if (titem) {
				tracescheck_addtolistnode (tcn, titem);
			}
		}
	}
	dynarray_add (bucket->items, tcn);

	return 0;
}
/*}}}*/
/*{{{  static int traceslang_totrace_ionode (langops_t *lops, tnode_t *node, tchk_bucket_t *bucket)*/
/*
 *	converts a traceslang ionode into a traces-check node
 *	returns 0 to stop walk, 1 to continue
 */
static int traceslang_totrace_ionode (langops_t *lops, tnode_t *node, tchk_bucket_t *bucket)
{
	tnode_t *item = tnode_nthsubof (node, 0);
	tchknode_t *tcn;

	tcn = tracescheck_createnode ((node->tag == traceslang.tag_INPUT) ? TCN_INPUT : TCN_OUTPUT, node,
			tracescheck_createnode (TCN_NODEREF, item, item));
	dynarray_add (bucket->items, tcn);

#if 0
fprintf (stderr, "traceslang_totrace_ionode(): here!\n");
#endif
	return 0;
}
/*}}}*/

/*{{{  static void traceslang_reduce_dop (dfastate_t *dfast, parsepriv_t *pp, void *rarg)*/
/*
 *	reduces a dyadic operator (in the parser), expects 2 nodes on the node-stack,
 *	and the relevant operator token on the token-stack
 */
static void traceslang_reduce_dop (dfastate_t *dfast, parsepriv_t *pp, void *rarg)
{
	token_t *tok = parser_gettok (pp);
	tnode_t *lhs, *rhs;
	ntdef_t *tag = NULL;
	int i;

	if (!tok) {
		parser_error (pp->lf, "traceslang_reduce_dop(): no token ?");
		return;
	}
	rhs = dfa_popnode (dfast);
	lhs = dfa_popnode (dfast);
	if (!rhs || !lhs) {
		parser_error (pp->lf, "traceslan_reduce_dop(): lhs=0x%8.8x, rhs=0x%8.8x", (unsigned int)lhs, (unsigned int)rhs);
		return;
	}

	for (i=0; dopmap[i].lookup; i++) {
		if (lexer_tokmatch (dopmap[i].tok, tok)) {
			tag = *(dopmap[i].tagp);
			break;
		}
	}
	if (!tag) {
		parser_error (pp->lf, "traceslang_reduce_dop(): unhandled token [%s]", lexer_stokenstr (tok));
		return;
	}

	*(dfast->ptr) = tnode_create (tag, pp->lf, parser_buildlistnode (pp->lf, lhs, rhs, NULL));
	return;
}
/*}}}*/


/*{{{  static int traceslang_expr_init_nodes (void)*/
/*
 *	initialises nodes for traces language
 *	returns 0 on success, non-zero on failure
 */
static int traceslang_expr_init_nodes (void)
{
	tndef_t *tnd;
	int i;
	compops_t *cops;
	langops_t *lops;

	/*{{{  register reduction functions*/
	fcnlib_addfcn ("traceslang_nametoken_to_hook", (void *)traceslang_nametoken_to_hook, 1, 1);
	fcnlib_addfcn ("traceslang_stringtoken_to_node", (void *)traceslang_stringtoken_to_node, 1, 1);
	fcnlib_addfcn ("traceslang_integertoken_to_node", (void *)traceslang_integertoken_to_node, 1, 1);

	fcnlib_addfcn ("traceslang_reduce_dop", (void *)traceslang_reduce_dop, 0, 3);

	/*}}}*/
	/*{{{  create some new language operations to extract params/body from language-specific trace types*/
	if (tnode_newlangop ("traceslang_getparams", LOPS_INVALID, 1, INTERNAL_ORIGIN) < 0) {
		nocc_internal ("traceslang_expr_init_nodes(): failed to create \"traceslang_getparams\" lang-op");
		return -1;
	}
	if (tnode_newlangop ("traceslang_getbody", LOPS_INVALID, 1, INTERNAL_ORIGIN) < 0) {
		nocc_internal ("traceslang_expr_init_nodes(): failed to create \"traceslang_getbody\" lang-op");
		return -1;
	}

	/*}}}*/
	/*{{{  traceslang:rawnamenode -- TRACESLANGNAME*/
	i = -1;
	tnd = tnode_newnodetype ("traceslang:rawnamenode", &i, 0, 0, 1, TNF_NONE);		/* hooks: 0 = raw-name */
	tnd->hook_free = traceslang_rawnamenode_hook_free;
	tnd->hook_copy = traceslang_rawnamenode_hook_copy;
	tnd->hook_dumptree = traceslang_rawnamenode_hook_dumptree;
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (traceslang_scopein_rawname));
	tnd->ops = cops;

	i = -1;
	traceslang.tag_NAME = tnode_newnodetag ("TRACESLANGNAME", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  traceslang:litnode -- TRACESLANGLITSTR, TRACESLANGLITINT*/
	i = -1;
	tnd = tnode_newnodetype ("traceslang:litnode", &i, 0, 0, 1, TNF_NONE);			/* hooks: 0 = traceslang_lithook_t */
	tnd->hook_free = traceslang_litnode_hook_free;
	tnd->hook_copy = traceslang_litnode_hook_copy;
	tnd->hook_dumptree = traceslang_litnode_hook_dumptree;
	cops = tnode_newcompops ();
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "tracescheck_check", 2, LANGOPTYPE (traceslang_tracescheck_litnode));
	tnd->lops = lops;

	i = -1;
	traceslang.tag_LITSTR = tnode_newnodetag ("TRACESLANGLITSTR", &i, tnd, NTF_TRACESLANGSTRUCTURAL);
	i = -1;
	traceslang.tag_LITINT = tnode_newnodetag ("TRACESLANGLITINT", &i, tnd, NTF_TRACESLANGSTRUCTURAL);

	/*}}}*/
	/*{{{  traceslang:setnode -- TRACESLANGSEQ, TRACESLANGPAR, TRACESLANGDET, TRACESLANGNDET*/
	i = -1;
	tnd = tnode_newnodetype ("traceslang:setnode", &i, 1, 0, 0, TNF_NONE);			/* subnodes: 0 = list-of-items */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "scopeout", 2, COMPOPTYPE (traceslang_scopeout_setnode));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "tracescheck_check", 2, LANGOPTYPE (traceslang_tracescheck_setnode));
	tnode_setlangop (lops, "tracescheck_totrace", 2, LANGOPTYPE (traceslang_totrace_setnode));
	tnd->lops = lops;

	i = -1;
	traceslang.tag_SEQ = tnode_newnodetag ("TRACESLANGSEQ", &i, tnd, NTF_TRACESLANGSTRUCTURAL);
	i = -1;
	traceslang.tag_PAR = tnode_newnodetag ("TRACESLANGPAR", &i, tnd, NTF_TRACESLANGSTRUCTURAL);
	i = -1;
	traceslang.tag_DET = tnode_newnodetag ("TRACESLANGDET", &i, tnd, NTF_TRACESLANGSTRUCTURAL);
	i = -1;
	traceslang.tag_NDET = tnode_newnodetag ("TRACESLANGNDET", &i, tnd, NTF_TRACESLANGSTRUCTURAL);

	/*}}}*/
	/*{{{  traceslang:ionode -- TRACESLANGINPUT, TRACESLANGOUTPUT, TRACESLANGSYNC*/
	i = -1;
	tnd = tnode_newnodetype ("traceslang:ionode", &i, 2, 0, 0, TNF_NONE);			/* subnodes: 0 = item, 1 = tag (if any) */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (traceslang_scopein_ionode));
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (traceslang_typecheck_ionode));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "tracescheck_check", 2, LANGOPTYPE (traceslang_tracescheck_ionode));
	tnode_setlangop (lops, "tracescheck_totrace", 2, LANGOPTYPE (traceslang_totrace_ionode));
	tnd->lops = lops;

	i = -1;
	traceslang.tag_INPUT = tnode_newnodetag ("TRACESLANGINPUT", &i, tnd, NTF_TRACESLANGSTRUCTURAL);
	i = -1;
	traceslang.tag_OUTPUT = tnode_newnodetag ("TRACESLANGOUTPUT", &i, tnd, NTF_TRACESLANGSTRUCTURAL);
	i = -1;
	traceslang.tag_SYNC = tnode_newnodetag ("TRACESLANGSYNC", &i, tnd, NTF_TRACESLANGSTRUCTURAL);

	/*}}}*/
	/*{{{  traceslang:leafnode -- TRACESLANGEVENT, TRACESLANGFIXPOINTTYPE, TRACESLANGSKIP, TRACESLANGSTOP, TRACESLANGCHAOS, TRACESLANGDIV*/
	i = -1;
	tnd = tnode_newnodetype ("traceslang:leafnode", &i, 0, 0, 0, TNF_NONE);
	cops = tnode_newcompops ();
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "tracescheck_check", 2, LANGOPTYPE (traceslang_tracescheck_leafnode));
	tnd->lops = lops;

	i = -1;
	traceslang.tag_EVENT = tnode_newnodetag ("TRACESLANGEVENT", &i, tnd, NTF_TRACESLANGSTRUCTURAL);
	i = -1;
	traceslang.tag_FIXPOINTTYPE = tnode_newnodetag ("TRACESLANGFIXPOINTTYPE", &i, tnd, NTF_TRACESLANGSTRUCTURAL);
	i = -1;
	traceslang.tag_SKIP = tnode_newnodetag ("TRACESLANGSKIP", &i, tnd, NTF_TRACESLANGSTRUCTURAL);
	i = -1;
	traceslang.tag_STOP = tnode_newnodetag ("TRACESLANGSTOP", &i, tnd, NTF_TRACESLANGSTRUCTURAL);
	i = -1;
	traceslang.tag_CHAOS = tnode_newnodetag ("TRACESLANGCHAOS", &i, tnd, NTF_TRACESLANGSTRUCTURAL);
	i = -1;
	traceslang.tag_DIV = tnode_newnodetag ("TRACESLANGDIV", &i, tnd, NTF_TRACESLANGSTRUCTURAL);

	/*}}}*/
	/*{{{  traceslang:instancenode -- TRACESLANGINSTANCE*/
	i = -1;
	tnd = tnode_newnodetype ("traceslang:instancenode", &i, 2, 0, 0, TNF_NONE);		/* subnodes: 0 = name, 1 = params */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "prescope", 2, COMPOPTYPE (traceslang_prescope_instancenode));
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (traceslang_typecheck_instancenode));
	tnode_setcompop (cops, "typeresolve", 2, COMPOPTYPE (traceslang_typeresolve_instancenode));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnd->lops = lops;

	i = -1;
	traceslang.tag_INSTANCE = tnode_newnodetag ("TRACESLANGINSTANCE", &i, tnd, NTF_TRACESLANGSTRUCTURAL);

	/*}}}*/
	/*{{{  traceslang:namenode -- TRACESLANGNPARAM, TRACESLANGNFIX, TRACESLANGNTAG*/
	i = -1;
	tnd = tnode_newnodetype ("traceslang:namenode", &i, 0, 1, 0, TNF_NONE);			/* subnames: 0 = name */
	cops = tnode_newcompops ();
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "gettype", 2, LANGOPTYPE (traceslang_gettype_namenode));
	tnode_setlangop (lops, "tracescheck_check", 2, LANGOPTYPE (traceslang_tracescheck_namenode));
	tnd->lops = lops;

	i = -1;
	traceslang.tag_NPARAM = tnode_newnodetag ("TRACESLANGNPARAM", &i, tnd, NTF_TRACESLANGCOPYALIAS);
	i = -1;
	traceslang.tag_NFIX = tnode_newnodetag ("TRACESLANGNFIX", &i, tnd, NTF_TRACESLANGCOPYALIAS);
	i = -1;
	traceslang.tag_NTAG = tnode_newnodetag ("TRACESLANGTAG", &i, tnd, NTF_TRACESLANGCOPYALIAS);

	/*}}}*/
	/*{{{  traceslang:fixpointnode -- TRACESLANGFIXPOINT*/
	i = -1;
	tnd = tnode_newnodetype ("traceslang:fixpointnode", &i, 2, 0, 0, TNF_NONE);		/* subnodes: 0 = name, 1 = body */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (traceslang_scopein_fixpointnode));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "tracescheck_check", 2, LANGOPTYPE (traceslang_tracescheck_fixpointnode));
	tnd->lops = lops;

	i = -1;
	traceslang.tag_FIXPOINT = tnode_newnodetag ("TRACESLANGFIXPOINT", &i, tnd, NTF_TRACESLANGSTRUCTURAL);

	/*}}}*/

	/*{{{  setup local tokens*/
	for (i=0; dopmap[i].lookup; i++) {
		dopmap[i].tok = lexer_newtoken (dopmap[i].ttype, dopmap[i].lookup);
	}

	/*}}}*/

	return 0;
}
/*}}}*/


/*{{{  traceslang_expr_feinit (feunit_t)*/
feunit_t traceslang_expr_feunit = {
	init_nodes: traceslang_expr_init_nodes,
	reg_reducers: NULL,
	init_dfatrans: NULL,
	post_setup: NULL,
	ident: "traceslang-expr"
};

/*}}}*/


