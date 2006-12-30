/*
 *	rcxb_parser.c -- RCX-BASIC parser for nocc
 *	Copyright (C) 2006 Fred Barnes <frmb@kent.ac.uk>
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
#include "symbols.h"
#include "keywords.h"
#include "lexer.h"
#include "lexpriv.h"
#include "tnode.h"
#include "parser.h"
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
	langname:	"rcxbasic",
	init:		rcxb_parser_init,
	shutdown:	rcxb_parser_shutdown,
	parse:		rcxb_parser_parse,
	descparse:	NULL, // mcsp_parser_descparse,
	prescope:	rcxb_parser_prescope,
	scope:		NULL, // mcsp_parser_scope,
	typecheck:	NULL, // mcsp_parser_typecheck,
	postcheck:	NULL,
	fetrans:	NULL,
	getlangdef:	NULL,
	maketemp:	NULL,
	makeseqassign:	NULL,
	tagstruct_hook:	(void *)&rcxb,
	lexer:		NULL
};


/*}}}*/
/*{{{  private types/vars*/
typedef struct {
	dfanode_t *inode;
} rcxb_parse_t;

static rcxb_parse_t *rcxb_priv = NULL;

static feunit_t *feunit_set[] = {
	&rcxb_program_feunit,
	NULL
};

/*}}}*/


/*{{{  void rcxb_isetindent (FILE *stream, int indent)*/
/*
 *	set-indent for debugging output
 */
void rcxb_isetindent (FILE *stream, int indent)
{
	int i;

	for (i=0; i<indent; i++) {
		fprintf (stream, "    ");
	}
	return;
}
/*}}}*/


/*{{{  static int rcxb_tokens_init (void)*/
/*
 *	initialises RCX-BASIC tokens (keywords + symbols)
 *	returns 0 on success, non-zero on failure
 */
static int rcxb_tokens_init (void)
{
	keywords_add ("rem", -1, (void *)&rcxb_parser);

	keywords_add ("set", -1, (void *)&rcxb_parser);
	keywords_add ("motor", -1, (void *)&rcxb_parser);
	keywords_add ("sensor", -1, (void *)&rcxb_parser);
	keywords_add ("power", -1, (void *)&rcxb_parser);
	keywords_add ("direction", -1, (void *)&rcxb_parser);
	keywords_add ("forward", -1, (void *)&rcxb_parser);
	keywords_add ("reverse", -1, (void *)&rcxb_parser);
	keywords_add ("off", -1, (void *)&rcxb_parser);
	keywords_add ("for", -1, (void *)&rcxb_parser);
	keywords_add ("to", -1, (void *)&rcxb_parser);
	keywords_add ("on", -1, (void *)&rcxb_parser);
	keywords_add ("step", -1, (void *)&rcxb_parser);
	keywords_add ("next", -1, (void *)&rcxb_parser);
	keywords_add ("while", -1, (void *)&rcxb_parser);
	keywords_add ("gosub", -1, (void *)&rcxb_parser);
	keywords_add ("goto", -1, (void *)&rcxb_parser);
	keywords_add ("if", -1, (void *)&rcxb_parser);
	keywords_add ("then", -1, (void *)&rcxb_parser);
	keywords_add ("else", -1, (void *)&rcxb_parser);
	keywords_add ("elsif", -1, (void *)&rcxb_parser);
	keywords_add ("endif", -1, (void *)&rcxb_parser);
	keywords_add ("sleep", -1, (void *)&rcxb_parser);
	keywords_add ("sound", -1, (void *)&rcxb_parser);

	return 0;
}
/*}}}*/
/*{{{  static int rcxb_nodes_init (void)*/
/*
 *	initialises the RCX-BASIC nodes
 *	returns 0 on success, non-zero on failure
 */
static int rcxb_nodes_init (void)
{
	int i;

	for (i=0; feunit_set[i]; i++) {
		feunit_t *thisunit = feunit_set[i];

		if (thisunit->init_nodes && thisunit->init_nodes ()) {
			return -1;
		}
	}

	return 0;
}
/*}}}*/
/*{{{  static int rcxb_register_reducers (void)*/
/*
 *	initialises RCX-BASIC reducers
 *	returns 0 on success, non-zero on failure
 */
static int rcxb_register_reducers (void)
{
	int i;

	/*{{{  generic reductions*/
	parser_register_grule ("rcxb:nullreduce", parser_decode_grule ("N+R-"));
	parser_register_grule ("rcxb:nullpush", parser_decode_grule ("0N-"));
	parser_register_grule ("rcxb:nullset", parser_decode_grule ("0R-"));

	/*}}}*/

	for (i=0; feunit_set[i]; i++) {
		feunit_t *thisunit = feunit_set[i];

		if (thisunit->reg_reducers && thisunit->reg_reducers ()) {
			return -1;
		}
	}

	return 0;
}
/*}}}*/
/*{{{  static int rcxb_dfas_init (void)*/
/*
 *	initialises RCX-BASIC DFAs
 *	returns 0 on success, non-zero on failure
 */
static int rcxb_dfas_init (void)
{
	DYNARRAY (dfattbl_t *, transtbls);
	int i, x;

	/*{{{  create DFAs*/
	dfa_clear_deferred ();
	dynarray_init (transtbls);

	for (i=0; feunit_set[i]; i++) {
		feunit_t *thisunit = feunit_set[i];

		if (thisunit->init_dfatrans) {
			dfattbl_t **t_table;
			int t_size = 0;

			t_table = thisunit->init_dfatrans (&t_size);
			if (t_size > 0) {
				int j;

				for (j=0; j<t_size; j++) {
					dynarray_add (transtbls, t_table[j]);
				}
			}
			if (t_table) {
				sfree (t_table);
			}
		}
	}

	dfa_mergetables (DA_PTR (transtbls), DA_CUR (transtbls));

	/*{{{  debug dump of grammars if requested*/
	if (compopts.dumpgrammar) {
		for (i=0; i<DA_CUR (transtbls); i++) {
			dfattbl_t *ttbl = DA_NTHITEM (transtbls, i);

			if (ttbl) {
				dfa_dumpttbl (stderr, ttbl);
			}
		}
	}

	/*}}}*/
	/*{{{  convert into DFA nodes proper*/

	x = 0;
	for (i=0; i<DA_CUR (transtbls); i++) {
		dfattbl_t *ttbl = DA_NTHITEM (transtbls, i);

		/* only convert non-addition nodes */
		if (ttbl && !ttbl->op) {
			x += !dfa_tbltodfa (ttbl);
		}
	}

	if (compopts.dumpgrammar) {
		dfa_dumpdeferred (stderr);
	}

	if (dfa_match_deferred ()) {
		/* failed here, get out */
		return 1;
	}

	/*}}}*/
	/*{{{  free up tables*/
	for (i=0; i<DA_CUR (transtbls); i++) {
		dfattbl_t *ttbl = DA_NTHITEM (transtbls, i);

		if (ttbl) {
			dfa_freettbl (ttbl);
		}
	}
	dynarray_trash (transtbls);

	/*}}}*/

	if (x) {
		return -1;
	}

	/*}}}*/
	return 0;
}
/*}}}*/
/*{{{  static int rcxb_post_setup (void)*/
/*
 *	does post-setup for RCX-BASIC
 *	returns 0 on success, non-zero on failure
 */
static int rcxb_post_setup (void)
{
	int i;

	for (i=0; feunit_set[i]; i++) {
		feunit_t *thisunit = feunit_set[i];

		if (thisunit->post_setup && thisunit->post_setup ()) {
			return -1;
		}
	}

	return 0;
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
		rcxb_priv = (rcxb_parse_t *)smalloc (sizeof (rcxb_parse_t));
		rcxb_priv->inode = NULL;

		memset ((void *)&rcxb, 0, sizeof (rcxb));

		/* initialise! */
		if (rcxb_tokens_init ()) {
			nocc_error ("rcxb_parser_init(): failed to initialise tokens");
			return 1;
		}
		if (rcxb_nodes_init ()) {
			nocc_error ("rcxb_parser_init(): failed to initialise nodes");
			return 1;
		}
		if (rcxb_register_reducers ()) {
			nocc_error ("rcxb_parser_init(): failed to register reducers");
			return 1;
		}
		if (rcxb_dfas_init ()) {
			nocc_error ("rcxb_parser_init(): failed to initialise DFAs");
			return 1;
		}
		if (rcxb_post_setup ()) {
			nocc_error ("rcxb_parser_init(): failed to post-setup");
			return 1;
		}

		rcxb_priv->inode = dfa_lookupbyname ("rcxb:program");
		if (!rcxb_priv->inode) {
			nocc_error ("rcxb_parser_init(): could not find rcxb:program");
			return 1;
		}
		if (compopts.dumpdfas) {
			dfa_dumpdfas (stderr);
		}
		if (compopts.dumpgrules) {
			parser_dumpgrules (stderr);
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
	tnode_t *tree = parser_newlistnode (lf);

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

		thisone = dfa_walk ("rcxb:statement", lf);
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
			lexer_dumptoken (stderr, tok);
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



