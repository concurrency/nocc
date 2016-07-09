/*
 *	rcxb_parser.c -- RCX-BASIC parser for nocc
 *	Copyright (C) 2006-2016 Fred Barnes <frmb@kent.ac.uk>
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
#include <stdint.h>
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
#include "rcxb.h"
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
static int rcxb_parser_init (lexfile_t *lf);
static void rcxb_parser_shutdown (lexfile_t *lf);
static tnode_t *rcxb_parser_parse (lexfile_t *lf);
static int rcxb_parser_prescope (tnode_t **tptr, prescope_t *ps);

/*}}}*/
/*{{{  global vars*/

rcxb_pset_t rcxb;

langparser_t rcxb_parser = {
	.langname =		"rcxbasic",
	.init =			rcxb_parser_init,
	.shutdown =		rcxb_parser_shutdown,
	.parse =		rcxb_parser_parse,
	.descparse =		NULL, // rcxb_parser_descparse,
	.prescope =		rcxb_parser_prescope,
	.scope =		NULL, // rcxb_parser_scope,
	.typecheck =		NULL, // rcxb_parser_typecheck,
	.typeresolve =		NULL,
	.postcheck =		NULL,
	.fetrans =		NULL,
	.getlangdef =		rcxb_getlangdef,
	.getlanglibs =		NULL,
	.maketemp =		NULL,
	.makeseqassign =	NULL,
	.makeseqany =		NULL,
	.tagstruct_hook =	(void *)&rcxb,
	.lexer =		NULL
};


/*}}}*/
/*{{{  private types/vars*/
typedef struct {
	dfanode_t *inode;
	langdef_t *ldef;
} rcxb_parse_t;

static rcxb_parse_t *rcxb_priv = NULL;

static feunit_t *feunit_set[] = {
	&rcxb_program_feunit,
	NULL
};

/*}}}*/


/*{{{  static rcxb_parse_t *rcxb_newrcxbparse (void)*/
/*
 *	creates a new rcxb_parse_t structure
 */
static rcxb_parse_t *rcxb_newrcxbparse (void)
{
	rcxb_parse_t *rcxp = (rcxb_parse_t *)smalloc (sizeof (rcxb_parse_t));

	rcxp->inode = NULL;
	rcxp->ldef = NULL;

	return rcxp;
}
/*}}}*/
/*{{{  static void rcxb_freercxbparse (rcxb_parse_t *rcxp)*/
/*
 *	frees a rcxb_parse_t structure
 */
static void rcxb_freercxbparse (rcxb_parse_t *rcxp)
{
	if (!rcxp) {
		nocc_warning ("rcxb_freercxbparse(): NULL pointer!");
		return;
	}
	if (rcxp->ldef) {
		langdef_freelangdef (rcxp->ldef);
		rcxp->ldef = NULL;
	}
	/* leave inode */
	rcxp->inode = NULL;
	sfree (rcxp);

	return;
}
/*}}}*/


/*{{{  void rcxb_isetindent (fhandle_t *stream, int indent)*/
/*
 *	set-indent for debugging output
 */
void rcxb_isetindent (fhandle_t *stream, int indent)
{
	int i;

	for (i=0; i<indent; i++) {
		fhandle_printf (stream, "    ");
	}
	return;
}
/*}}}*/
/*{{{  langdef_t *rcxb_getlangdef (void)*/
/*
 *	returns the current language definitions
 *	returns NULL on failure
 */
langdef_t *rcxb_getlangdef (void)
{
	if (!rcxb_priv) {
		return NULL;
	}
	return rcxb_priv->ldef;
}
/*}}}*/


/*{{{  static int rcxb_parser_init (lexfile_t *lf)*/
/*
 *	initialises the RCX-BASIC parser
 *	returns 0 on success, non-zero on failure
 */
static int rcxb_parser_init (lexfile_t *lf)
{
	if (compopts.verbose) {
		nocc_message ("initialising RCX-BASIC parser..");
	}
	if (!rcxb_priv) {
		rcxb_priv = rcxb_newrcxbparse ();

		memset ((void *)&rcxb, 0, sizeof (rcxb));

		rcxb_priv->ldef = langdef_readdefs ("rcxb.ldef");
		if (!rcxb_priv->ldef) {
			nocc_error ("rcxb_parser_init(): failed to load language definitions!");
			return 1;
		}

		/* initialise! */
		if (feunit_do_init_tokens (0, rcxb_priv->ldef, origin_langparser (&rcxb_parser))) {
			nocc_error ("rcxb_parser_init(): failed to initialise tokens");
			return 1;
		}
		if (feunit_do_init_nodes (feunit_set, 1, rcxb_priv->ldef, origin_langparser (&rcxb_parser))) {
			nocc_error ("rcxb_parser_init(): failed to initialise nodes");
			return 1;
		}
		if (feunit_do_reg_reducers (feunit_set, 0, rcxb_priv->ldef)) {
			nocc_error ("rcxb_parser_init(): failed to register reducers");
			return 1;
		}
		if (feunit_do_init_dfatrans (feunit_set, 1, rcxb_priv->ldef, &rcxb_parser, 1)) {
			nocc_error ("rcxb_parser_init(): failed to initialise DFAs");
			return 1;
		}
		if (feunit_do_post_setup (feunit_set, 1, rcxb_priv->ldef)) {
			nocc_error ("rcxb_parser_init(): failed to post-setup");
			return 1;
		}
		if (langdef_treecheck_setup (rcxb_priv->ldef)) {
			nocc_serious ("rcxb_parser(): failed to initialise tree-checking!");
			/* linger on */
		}

		rcxb_priv->inode = dfa_lookupbyname ("rcxb:program");
		if (!rcxb_priv->inode) {
			nocc_error ("rcxb_parser_init(): could not find rcxb:program");
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
/*{{{  static void rcxb_parser_shutdown (lexfile_t *lf)*/
/*
 *	shuts-down the RCX-BASIC parser
 */
static void rcxb_parser_shutdown (lexfile_t *lf)
{
	if (rcxb_priv) {
		rcxb_freercxbparse (rcxb_priv);
		rcxb_priv = NULL;
	}

	return;
}
/*}}}*/


/*{{{  static tnode_t *rcxb_parser_parse (lexfile_t *lf)*/
/*
 *	called to parse a file (containing RCX-BASIC)
 *	returns a tree on success, NULL on failure
 *
 *	note: for this BASIC-ish language, the tree is just a list of
 *	things at the top-level
 */
static tnode_t *rcxb_parser_parse (lexfile_t *lf)
{
	token_t *tok;
	tnode_t *tree = parser_newlistnode (SLOCN (lf));

	if (compopts.verbose) {
		nocc_message ("rcxb_parser_parse(): starting parse..");
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

		thisone = dfa_walk ("rcxb:statement", 0, lf);
		if (!thisone) {
			break;		/* for() */
		}

		/* add to program */
		parser_addtolist (tree, thisone);
	}

	if (compopts.verbose) {
		nocc_message ("leftover tokens:");
	}

	tok = lexer_nexttoken (lf);
	while (tok) {
		if (compopts.verbose) {
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
/*{{{  static int rcxb_parser_prescope (tnode_t **tptr, prescope_t *ps)*/
/*
 *	called to pre-scope the parse tree
 *	returns 0 on success, non-zero on failure
 */
static int rcxb_parser_prescope (tnode_t **tptr, prescope_t *ps)
{
	ps->hook = NULL;
	tnode_modprewalktree (tptr, prescope_modprewalktree, (void *)ps);

	return ps->err;
}
/*}}}*/



