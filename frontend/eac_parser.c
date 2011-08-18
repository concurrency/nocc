/*
 *	eac_parser.c -- EAC parser for NOCC
 *	Copyright (C) 2011 Fred Barnes <frmb@kent.ac.uk>
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
#include <errno.h>

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
#include "langdef.h"
#include "dfa.h"
#include "dfaerror.h"
#include "parsepriv.h"
#include "eac.h"
#include "library.h"
#include "feunit.h"
#include "names.h"
#include "scope.h"
#include "prescope.h"
#include "typecheck.h"
#include "extn.h"
#include "metadata.h"
#include "interact.h"


/*}}}*/


/*{{{  forward decls.*/
static int eac_parser_init (lexfile_t *lf);
static void eac_parser_shutdown (lexfile_t *lf);
static tnode_t *eac_parser_parse (lexfile_t *lf);
static tnode_t *eac_parser_descparse (lexfile_t *lf);
static int eac_parser_prescope (tnode_t **tptr, prescope_t *ps);
static int eac_parser_scope (tnode_t **tptr, scope_t *ss);
static int eac_parser_typecheck (tnode_t *tptr, typecheck_t *tc);
static int eac_parser_typeresolve (tnode_t **tptr, typecheck_t *tc);

static tnode_t *eac_process (lexfile_t *lf);

/*}}}*/
/*{{{  global vars*/

eac_pset_t eac;			/* attach tags, etc. here */

langparser_t eac_parser = {
	.langname =		"eac",
	.init =			eac_parser_init,
	.shutdown =		eac_parser_shutdown,
	.parse =		eac_parser_parse,
	.descparse =		eac_parser_descparse,
	.prescope =		eac_parser_prescope,
	.scope =		eac_parser_scope,
	.typecheck =		eac_parser_typecheck,
	.typeresolve =		eac_parser_typeresolve,
	.postcheck =		NULL,
	.fetrans =		NULL,
	.getlangdef =		eac_getlangdef,
	.maketemp =		NULL,
	.makeseqassign =	NULL,
	.makeseqany =		NULL,
	.tagstruct_hook =	(void *)&eac,
	.lexer =		NULL
};

/*}}}*/
/*{{{  private types/vars*/
typedef struct {
	dfanode_t *inode;
	langdef_t *langdefs;
} eac_parse_t;

static eac_parse_t *eac_priv = NULL;

static feunit_t *feunit_set[] = {
	&eac_code_feunit,
	NULL
};

static ntdef_t *testtruetag, *testfalsetag;

/*}}}*/


/*{{{  static eac_parse_t *eac_neweacparse (void)*/
/*
 *	creates a new eac_parse_t structure
 */
static eac_parse_t *eac_neweacparse (void)
{
	eac_parse_t *epse = (eac_parse_t *)smalloc (sizeof (eac_parse_t));

	epse->inode = NULL;
	epse->langdefs = NULL;

	return epse;
}
/*}}}*/
/*{{{  static void eac_freeeacparse (eac_parse_t *epse)*/
/*
 *	frees an eac_parse_t structure
 */
static void eac_freeeacparse (eac_parse_t *epse)
{
	if (!epse) {
		nocc_warning ("eac_freeeacparse(): NULL pointer!");
		return;
	}
	if (epse->langdefs) {
		langdef_freelangdef (epse->langdefs);
		epse->langdefs = NULL;
	}
	/* leave inode alone */
	epse->inode = NULL;
	sfree (epse);

	return;
}
/*}}}*/
/*{{{  EAC reductions*/
/*{{{  void *eac_nametoken_to_hook (void *ntok)*/
/*
 *	turns a name token into a hooknode for a tag_NAME
 */
void *eac_nametoken_to_hook (void *ntok)
{
	token_t *tok = (token_t *)ntok;
	char *rawname;

	rawname = tok->u.name;
	tok->u.name = NULL;

	lexer_freetoken (tok);

	return (void *)rawname;
}
/*}}}*/

/*}}}*/


/*{{{  void eac_isetindent (FILE *stream, int indent)*/
/*
 *	set-indent for debugging output
 */
void eac_isetindent (FILE *stream, int indent)
{
	int i;

	for (i=0; i<indent; i++) {
		fprintf (stream, "    ");
	}
	return;
}
/*}}}*/
/*{{{  langdef_t *eac_getlangdef (void)*/
/*
 *	returns the language definition for EAC, or NULL if none
 */
langdef_t *eac_getlangdef (void)
{
	if (!eac_priv) {
		return NULL;
	}
	return eac_priv->langdefs;
}
/*}}}*/


/*{{{  int eac_callback_line (char *line)*/
/*
 *	callback in interactive mode for handling lines of text
 *	returns IHR_ constant
 */
int eac_callback_line (char *line)
{
	nocc_message ("eac: callback line [%s]", line);
	return IHR_UNHANDLED;
}
/*}}}*/


/*{{{  static int eac_parser_init (lexfile_t *lf)*/
/*
 *	initialises the EAC parser
 *	returns 0 on success, non-zero on error
 */
static int eac_parser_init (lexfile_t *lf)
{
	if (!eac_priv) {
		keyword_t *kw;

		eac_priv = eac_neweacparse ();

		if (compopts.verbose) {
			nocc_message ("initialising EAC parser..");
		}

		memset ((void *)&eac, 0, sizeof (eac));

		eac_priv->langdefs = langdef_readdefs ("eac.ldef");
		if (!eac_priv->langdefs) {
			nocc_error ("eac_parser_init(): failed to load language definitions!");
			return 1;
		}

		/* register some particular tokens (for later comparison) */
		eac.tok_ATSIGN = lexer_newtoken (SYMBOL, "@");

		/* register some general reduction functions */
		fcnlib_addfcn ("eac_nametoken_to_hook", (void *)eac_nametoken_to_hook, 1, 1);

		/* initialise! */
		if (feunit_do_init_tokens (0, eac_priv->langdefs, origin_langparser (&eac_parser))) {
			nocc_error ("eac_parser_init(): failed to initialise tokens");
			return 1;
		}
		if (feunit_do_init_nodes (feunit_set, 1, eac_priv->langdefs, origin_langparser (&eac_parser))) {
			nocc_error ("eac_parser_init(): failed to initialise nodes");
			return 1;
		}
		if (feunit_do_reg_reducers (feunit_set, 0, eac_priv->langdefs)) {
			nocc_error ("eac_parser_init(): failed to register reducers");
			return 1;
		}
		if (feunit_do_init_dfatrans (feunit_set, 1, eac_priv->langdefs, &eac_parser, 1)) {
			nocc_error ("eac_parser_init(): failed to initialise DFAs");
			return 1;
		}
		if (feunit_do_post_setup (feunit_set, 1, eac_priv->langdefs)) {
			nocc_error ("eac_parser_init(): failed to post-setup");
			return 1;
		}
		if (langdef_treecheck_setup (eac_priv->langdefs)) {
			nocc_serious ("eac_parser_init(): failed to initialise tree-checking!");
		}

		eac_priv->inode = dfa_lookupbyname ("eac:process");
		if (!eac_priv->inode) {
			nocc_error ("eac_parser_init(): could not find eac:process!");
			return 1;
		}
		if (compopts.dumpdfas) {
			dfa_dumpdfas (stderr);
		}
		if (compopts.dumpgrules) {
			parser_dumpgrules (stderr);
		}

		parser_gettesttags (&testtruetag, &testfalsetag);
	}
	return 0;
}
/*}}}*/
/*{{{  static void eac_parser_shutdown (lexfile_t *lf)*/
/*
 *	shuts-down the EAC parser
 */
static void eac_parser_shutdown (lexfile_t *lf)
{
	if (compopts.verbose) {
		nocc_message ("eac parser shutting down");
	}
	return;
}
/*}}}*/


/*{{{  static tnode_t *eac_parser_parseprocessdef (lexfile_t *lf)*/
/*
 *	parses a single EAC process definition, as typically read from a file
 *	returns a tree on success, NULL on failure
 */
static tnode_t *eac_parser_parseprocdef (lexfile_t *lf)
{
	token_t *tok;
	tnode_t *tree = NULL;
	tnode_t **target = &tree;

	if (compopts.verbose > 1) {
		nocc_message ("eac_parser_parseprocdef(): starting parse for single process definition..");
	}

	for (;;) {
		tnode_t *thisone;
		int tnflags;
		int breakfor = 0;

		/* eat up newlines, stop if we get to the end */
		tok = lexer_nexttoken (lf);
		while ((tok->type == NEWLINE) || (tok->type == COMMENT)) {
			lexer_freetoken (tok);
			tok = lexer_nexttoken (lf);
		}
		if ((tok->type == END) || (tok->type == NOTOKEN)) {
			/* done */
			lexer_freetoken (tok);
			break;			/* for() */
		}
		lexer_pushback (lf, tok);

		/* get the process */
		thisone = dfa_walk ("eac:process", 0, lf);
		if (!thisone) {
			*target = NULL;
			break;			/* for() */
		}
		*target = thisone;
		while (*target) {
			/* sink through (see sometimes when @include'ing, etc. */
			tnflags = tnode_tnflagsof (*target);
			if (tnflags & TNF_LONGDECL) {
				target = tnode_nthsubaddr (*target, 3);
			} else if (tnflags & TNF_SHORTDECL) {
				target = tnode_nthsubaddr (*target, 2);
			} else if (tnflags & TNF_TRANSPARENT) {
				target = tnode_nthsubaddr (*target, 0);
			} else {
				/* assume we're done! */
				breakfor = 1;
				break;			/* while() */
			}
		}
		if (breakfor) {
			break;
		}
	}

	if (compopts.verbose > 1) {
		nocc_message ("eac_parser_parseprocdef(): done parsing single process definition (%p).", tree);
	}

	return tree;
}
/*}}}*/
/*{{{  static int eac_parser_parseprocdeflist (lexfile_t *lf, tnode_t **target)*/
/*
 *	called to parse EAC process definitions into a list
 *	returns 0 on success, non-zero on failure
 */
static int eac_parser_parseprocdeflist (lexfile_t *lf, tnode_t **target)
{
	if (compopts.verbose > 1) {
		nocc_message ("eac_parser_parseprocdeflist(): starting parse of process list from [%s]", lf->fnptr);
	}

	for (;;) {
		tnode_t *thisone;
		int tnflags;
		int gotall = 0;
		token_t *tok;

		tok = lexer_nexttoken (lf);
		while ((tok->type == NEWLINE) || (tok->type == COMMENT)) {
			lexer_freetoken (tok);
			tok = lexer_nexttoken (lf);
		}
		if ((tok->type == END) || (tok->type == NOTOKEN)) {
			/* done */
			lexer_freetoken (tok);
			break;			/* for() */
		}
		lexer_pushback (lf, tok);

		thisone = eac_parser_parseprocdef (lf);
		if (!thisone) {
			/* nothing left probably */
			break;			/* for() */
		}
		if (!*target) {
			/* make it a list node */
			*target = parser_newlistnode (lf);
		} else if (!parser_islistnode (*target)) {
			nocc_internal ("eac_parser_parseprocdeflist(): target is not a list! (%s,%s)", (*target)->tag->name, (*target)->tag->ndef->name);
			return -1;
		}
		parser_addtolist (*target, thisone);
	}
	return 0;
}
/*}}}*/
/*{{{  static tnode_t *eac_parser_parse (lexfile_t *lf)*/
/*
 *	called to parse a file
 *	returns a tree on success, NULL on failure
 */
static tnode_t *eac_parser_parse (lexfile_t *lf)
{
	int i;
	tnode_t *tree = NULL;
	token_t *tok;

	if (compopts.verbose) {
		nocc_message ("eac_parser_parse(): starting parse..");
	}

	i = eac_parser_parseprocdeflist (lf, &tree);

	if (compopts.verbose > 1) {
		nocc_message ("leftover tokens:");
	}

	tok = lexer_nexttoken (lf);
	while (tok) {
		if (compopts.verbose > 1) {
			lexer_dumptoken (stderr, tok);
		}
		if ((tok->type = END) || (tok->type == NOTOKEN)) {
			lexer_freetoken (tok);
			break;			/* while() */
		}
		if ((tok->type != NEWLINE) && (tok->type != COMMENT)) {
			lf->errcount++;
		}

		lexer_freetoken (tok);
		tok = lexer_nexttoken (lf);
	}

	return tree;
}
/*}}}*/
/*{{{  static tnode_t *eac_parser_descparse (lexfile_t *lf)*/
/*
 *	called to parse a descriptor line
 *	returns a tree on success, NULL on failure
 */
static tnode_t *eac_parser_descparse (lexfile_t *lf)
{
	/* FIXME...! */
	return NULL;
}
/*}}}*/


/*{{{  static int eac_parser_prescope (tnode_t **tptr, prescope_t *ps)*/
/*
 *	called to pre-scope the parse tree (of part of it)
 *	returns 0 on success, non-zero on failure
 */
static int eac_parser_prescope (tnode_t **tptr, prescope_t *ps)
{
	tnode_modprewalktree (tptr, prescope_modprewalktree, (void *)ps);
	return ps->err;
}
/*}}}*/
/*{{{  static int eac_parser_scope (tnode_t **tptr, scope_t *ss)*/
/*
 *	called to scope declaractions in the parse tree
 *	returns 0 on success, non-zero on failure
 */
static int eac_parser_scope (tnode_t **tptr, scope_t *ss)
{
	tnode_modprepostwalktree (tptr, scope_modprewalktree, scope_modpostwalktree, (void *)ss);
	return ss->err;
}
/*}}}*/
/*{{{  static int eac_parser_typecheck (tnode_t *tptr, typecheck_t *tc)*/
/*
 *	called to type-check a tree
 *	returns 0 on success, non-zero on failure
 */
static int eac_parser_typecheck (tnode_t *tptr, typecheck_t *tc)
{
	tnode_prewalktree (tptr, typecheck_prewalktree, (void *)tc);
	return tc->err;
}
/*}}}*/
/*{{{  static int eac_parser_typeresolve (tnode_t **tptr, typecheck_t *tc)*/
/*
 *	called to type-resolve a tree
 *	returns 0 on success, non-zero on failure
 */
static int eac_parser_typeresolve (tnode_t **tptr, typecheck_t *tc)
{
	tnode_modprewalktree (tptr, typeresolve_modprewalktree, (void *)tc);
	return tc->err;
}
/*}}}*/



