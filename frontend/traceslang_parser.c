/*
 *	traceslang_parser.c -- traces language parser
 *	Copyright (C) 2007-2013 Fred Barnes <frmb@kent.ac.uk>
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
#include <errno.h>

#include "nocc.h"
#include "support.h"
#include "version.h"
#include "fhandle.h"
#include "origin.h"
#include "symbols.h"
#include "keywords.h"
#include "lexer.h"
#include "lexpriv.h"
#include "tnode.h"
#include "parser.h"
#include "fcnlib.h"
#include "dfa.h"
#include "parsepriv.h"
#include "traceslang.h"
#include "library.h"
#include "feunit.h"
#include "names.h"
#include "prescope.h"
#include "scope.h"
#include "prescope.h"
#include "typecheck.h"
#include "extn.h"
#include "langdef.h"

/*}}}*/
/*{{{  forward decls*/
static int traceslang_parser_init (lexfile_t *lf);
static void traceslang_parser_shutdown (lexfile_t *lf);
static tnode_t *traceslang_parser_parse (lexfile_t *lf);
static int traceslang_parser_prescope (tnode_t **tptr, prescope_t *ps);

/*}}}*/
/*{{{  global vars*/

traceslang_pset_t traceslang;

langparser_t traceslang_parser = {
	.langname =		"traceslang",
	.init =			traceslang_parser_init,
	.shutdown =		traceslang_parser_shutdown,
	.parse =		traceslang_parser_parse,
	.descparse =		NULL,
	.prescope =		traceslang_parser_prescope,
	.scope =		NULL,
	.typecheck =		NULL,
	.typeresolve =		NULL,
	.postcheck =		NULL,
	.fetrans =		NULL,
	.getlangdef =		traceslang_getlangdef,
	.maketemp =		NULL,
	.makeseqassign =	NULL,
	.makeseqany =		NULL,
	.tagstruct_hook =	(void *)&traceslang,
	.lexer =		NULL
};


/*}}}*/
/*{{{  private types/vars*/
typedef struct {
	dfanode_t *inode;
	langdef_t *ldef;
} traceslang_parse_t;

static traceslang_parse_t *traceslang_priv = NULL;

static feunit_t *feunit_set[] = {
	&traceslang_expr_feunit,
	NULL
};

/*}}}*/


/*{{{  static traceslang_parse_t *traceslang_newtraceslangparse (void)*/
/*
 *	creates a new traceslang_parse_t structure
 */
static traceslang_parse_t *traceslang_newtraceslangparse (void)
{
	traceslang_parse_t *trlp = (traceslang_parse_t *)smalloc (sizeof (traceslang_parse_t));

	trlp->inode = NULL;
	trlp->ldef = NULL;

	return trlp;
}
/*}}}*/
/*{{{  static void traceslang_freetraceslangparse (traceslang_parse_t *trlp)*/
/*
 *	frees a traceslang_parse_t structure
 */
static void traceslang_freetraceslangparse (traceslang_parse_t *trlp)
{
	if (!trlp) {
		nocc_warning ("traceslang_freetraceslangparse(): NULL pointer!");
		return;
	}
	if (trlp->ldef) {
		langdef_freelangdef (trlp->ldef);
		trlp->ldef = NULL;
	}
	/* leave inode */
	trlp->inode = NULL;
	sfree (trlp);

	return;
}
/*}}}*/


/*{{{  void traceslang_isetindent (fhandle_t *stream, int indent)*/
/*
 *	set-indent for debugging output
 */
void traceslang_isetindent (fhandle_t *stream, int indent)
{
	int i;

	for (i=0; i<indent; i++) {
		fhandle_printf (stream, "    ");
	}
	return;
}
/*}}}*/
/*{{{  langdef_t *traceslang_getlangdef (void)*/
/*
 *	returns the current language definitions
 *	returns NULL on failure
 */
langdef_t *traceslang_getlangdef (void)
{
	if (!traceslang_priv) {
		return NULL;
	}
	return traceslang_priv->ldef;
}
/*}}}*/


/*{{{  static int traceslang_parser_init (lexfile_t *lf)*/
/*
 *	initialises the traces language parser
 *	returns 0 on success, non-zero on failure
 */
static int traceslang_parser_init (lexfile_t *lf)
{
	if (!traceslang_priv) {
		traceslang_priv = traceslang_newtraceslangparse ();

		if (compopts.verbose) {
			nocc_message ("initialising traces parser..");
		}

		memset ((void *)&traceslang, 0, sizeof (traceslang));

		traceslang_priv->ldef = langdef_readdefs ("traceslang.ldef");
		if (!traceslang_priv->ldef) {
			nocc_error ("traceslang_parser_init(): failed to load language definitions!");
			return 1;
		}

		/* initialise! */
		if (feunit_do_init_tokens (0, traceslang_priv->ldef, origin_langparser (&traceslang_parser))) {
			nocc_error ("traceslang_parser_init(): failed to initialise tokens");
			return 1;
		}
		if (feunit_do_init_nodes (feunit_set, 1, traceslang_priv->ldef, origin_langparser (&traceslang_parser))) {
			nocc_error ("traceslang_parser_init(): failed to initialise nodes");
			return 1;
		}
		if (feunit_do_reg_reducers (feunit_set, 0, traceslang_priv->ldef)) {
			nocc_error ("traceslang_parser_init(): failed to register reducers");
			return 1;
		}
		if (feunit_do_init_dfatrans (feunit_set, 1, traceslang_priv->ldef, &traceslang_parser, 1)) {
			nocc_error ("traceslang_parser_init(): failed to initialise DFAs");
			return 1;
		}
		if (feunit_do_post_setup (feunit_set, 1, traceslang_priv->ldef)) {
			nocc_error ("traceslang_parser_init(): failed to post-setup");
			return 1;
		}
		if (langdef_treecheck_setup (traceslang_priv->ldef)) {
			nocc_serious ("traceslang_parser(): failed to initialise tree-checking!");
			/* linger on */
		}

		traceslang_priv->inode = dfa_lookupbyname ("traceslang:expr");
		if (!traceslang_priv->inode) {
			nocc_error ("traceslang_parser_init(): could not find traceslang:expr");
			return 1;
		}
		if (compopts.dumpdfas) {
			dfa_dumpdfas (FHAN_STDERR);
		}
		if (compopts.dumpgrules) {
			parser_dumpgrules (FHAN_STDERR);
		}
	}
	return 0;
}
/*}}}*/
/*{{{  static void traceslang_parser_shutdown (lexfile_t *lf)*/
/*
 *	shuts-down the traces language parser
 */
static void traceslang_parser_shutdown (lexfile_t *lf)
{
	/* make this a no-op */
#if 0
	if (traceslang_priv) {
		traceslang_freetraceslangparse (traceslang_priv);
		traceslang_priv = NULL;
	}
#endif

	return;
}
/*}}}*/


/*{{{  static tnode_t *traceslang_parser_parse (lexfile_t *lf)*/
/*
 *	called to parse a file (containing traces)
 *	returns a tree on success, NULL on failure
 *
 *	note: returns a list of function-definitions at the top-level
 */
static tnode_t *traceslang_parser_parse (lexfile_t *lf)
{
	token_t *tok;
	tnode_t *tree = parser_newlistnode (lf);

	if (compopts.verbose) {
		nocc_message ("traceslang_parser_parse(): starting parse..");
	}

	for (;;) {
		tnode_t *thisone;
		int tnflags;

		tok = lexer_nexttoken (lf);
		while ((tok->type == NEWLINE) || (tok->type == COMMENT)) {
			lexer_freetoken (tok);
			tok = lexer_nexttoken (lf);
		}
		if ((tok->type == END) || (tok->type == NOTOKEN)) {
			/* done */
			lexer_freetoken (tok);
			break;		/* for() */
		}
		lexer_pushback (lf, tok);

		thisone = dfa_walk ("traceslang:expr", 0, lf);
		if (!thisone) {
			break;		/* for() */
		}

		/* add to program */
		parser_addtolist (tree, thisone);
	}

	if (compopts.verbose > 1) {
		nocc_message ("leftover tokens:");
	}

	tok = lexer_nexttoken (lf);
	while (tok) {
		if (compopts.verbose > 1) {
			lexer_dumptoken (FHAN_STDERR, tok);
		}
		if ((tok->type == END) || (tok->type == NOTOKEN)) {
			lexer_freetoken (tok);
			break;
		}
		if ((tok->type != NEWLINE) && (tok->type != COMMENT)) {
			lf->errcount++;				/* errors.. */
		}

		lexer_freetoken (tok);
		tok = lexer_nexttoken (lf);
	}

	return tree;
}
/*}}}*/
/*{{{  static int traceslang_parser_prescope (tnode_t **tptr, prescope_t *ps)*/
/*
 *	called to pre-scope the parse tree
 *	returns 0 on success, non-zero on failure
 */
static int traceslang_parser_prescope (tnode_t **tptr, prescope_t *ps)
{
	nocc_message("scoping %p",*tptr );
	ps->hook = NULL;
	tnode_modprewalktree (tptr, prescope_modprewalktree, (void *)ps);

	return ps->err;
}
/*}}}*/



