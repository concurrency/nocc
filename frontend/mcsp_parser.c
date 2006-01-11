/*
 *	mcsp_parser.c -- MCSP parser for nocc
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
#include "mcsp.h"
#include "library.h"
#include "feunit.h"
#include "names.h"
#include "scope.h"
#include "prescope.h"
#include "typecheck.h"
#include "fetrans.h"
#include "extn.h"

/*}}}*/
/*{{{  forward decls*/
static int mcsp_parser_init (lexfile_t *lf);
static void mcsp_parser_shutdown (lexfile_t *lf);
static tnode_t *mcsp_parser_parse (lexfile_t *lf);
static tnode_t *mcsp_parser_descparse (lexfile_t *lf);
static int mcsp_parser_prescope (tnode_t **tptr, prescope_t *ps);
static int mcsp_parser_scope (tnode_t **tptr, scope_t *ss);
static int mcsp_parser_typecheck (tnode_t *tptr, typecheck_t *tc);
static int mcsp_parser_fetrans (tnode_t **tptr, fetrans_t *fe);


/*}}}*/
/*{{{  global vars*/

mcsp_pset_t mcsp;

langparser_t mcsp_parser = {
	langname:	"mcsp",
	init:		mcsp_parser_init,
	shutdown:	mcsp_parser_shutdown,
	parse:		mcsp_parser_parse,
	descparse:	mcsp_parser_descparse,
	prescope:	mcsp_parser_prescope,
	scope:		mcsp_parser_scope,
	typecheck:	mcsp_parser_typecheck,
	fetrans:	mcsp_parser_fetrans,
	maketemp:	NULL,
	makeseqassign:	NULL,
	tagstruct_hook:	(void *)&mcsp,
	lexer:		NULL
};


/*}}}*/
/*{{{  private types/vars*/
typedef struct {
	dfanode_t *inode;
} mcsp_parse_t;

static mcsp_parse_t *mcsp_priv = NULL;

static feunit_t *feunit_set[] = {
	&mcsp_process_feunit,
	NULL
};

/*}}}*/


/*{{{  static int mcsp_tokens_init (void)*/
/*
 *	initialises extra tokens needed by MCSP (symbols and keywords)
 *	returns 0 on success, non-zero on failure
 */
static int mcsp_tokens_init (void)
{
	symbols_add ("|||", 3, (void *)&mcsp_parser);
	symbols_add ("|~|", 3, (void *)&mcsp_parser);
	symbols_add ("::=", 3, (void *)&mcsp_parser);
	symbols_add (".", 1, (void *)&mcsp_parser);

	keywords_add ("DIV", -1, (void *)&mcsp_parser);
	keywords_add ("CHAOS", -1, (void *)&mcsp_parser);

	return 0;
}
/*}}}*/
/*{{{  static int mcsp_nodes_init (void)*/
/*
 *	initialises MCSP nodes
 *	returns 0 on success, non-zero on failure
 */
static int mcsp_nodes_init (void)
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
/*{{{  static int mcsp_register_reducers (void)*/
/*
 *	registers MCSP reducers
 *	returns 0 on success, non-zero on failure
 */
static int mcsp_register_reducers (void)
{
	int i;

	/*{{{  generic reductions*/
	parser_register_grule ("mcsp:nullreduce", parser_decode_grule ("N+R-"));
	parser_register_grule ("mcsp:nullpush", parser_decode_grule ("0N-"));
	parser_register_grule ("mcsp:nullset", parser_decode_grule ("0R-"));

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
/*{{{  static int mcsp_dfas_init (void)*/
/*
 *	initialises MCSP DFAs
 *	returns 0 on success, non-zero on failure
 */
static int mcsp_dfas_init (void)
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
/*{{{  static int mcsp_post_setup (void)*/
/*
 *	does post-setup for MCSP nodes
 *	returns 0 on success, non-zero on failure
 */
static int mcsp_post_setup (void)
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
/*{{{  void mcsp_isetindent (FILE *stream, int indent)*/
/*
 *	set-indent for debugging output
 */
void mcsp_isetindent (FILE *stream, int indent)
{
	int i;

	for (i=0; i<indent; i++) {
		fprintf (stream, "    ");
	}
	return;
}
/*}}}*/


/*{{{  static int mcsp_parser_init (lexfile_t *lf)*/
/*
 *	initialises the MCSP parser
 *	returns 0 on success, non-zero on failure
 */
static int mcsp_parser_init (lexfile_t *lf)
{
	if (compopts.verbose) {
		nocc_message ("initialising MCSP parser..");
	}
	if (!mcsp_priv) {
		mcsp_priv = (mcsp_parse_t *)smalloc (sizeof (mcsp_parse_t));
		mcsp_priv->inode = NULL;

		memset ((void *)&mcsp, 0, sizeof (mcsp));

		/* initialise! */
		if (mcsp_tokens_init ()) {
			nocc_error ("mcsp_parser_init(): failed to initialise tokens");
			return 1;
		}
		if (mcsp_nodes_init ()) {
			nocc_error ("mcsp_parser_init(): failed to initialise nodes");
			return 1;
		}
		if (mcsp_register_reducers ()) {
			nocc_error ("mcsp_parser_init(): failed to register reducers");
			return 1;
		}
		if (mcsp_dfas_init ()) {
			nocc_error ("mcsp_parser_init(): failed to initialise DFAs");
			return 1;
		}
		if (mcsp_post_setup ()) {
			nocc_error ("mcsp_parser_init(): failed to post-setup");
			return 1;
		}

		mcsp_priv->inode = dfa_lookupbyname ("mcsp:procdecl");
		if (!mcsp_priv->inode) {
			nocc_error ("mcsp_parser_init(): could not find mcsp:procdecl");
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
/*{{{  static void mcsp_parser_shutdown (lexfile_t *lf)*/
/*
 *	shuts-down the MCSP parser
 */
static void mcsp_parser_shutdown (lexfile_t *lf)
{
	return;
}
/*}}}*/


/*{{{  static tnode_t *mcsp_parser_parse (lexfile_t *lf)*/
/*
 *	called to parse a file (or chunk of MCSP)
 *	returns a tree on success, NULL on failure
 */
static tnode_t *mcsp_parser_parse (lexfile_t *lf)
{
	token_t *tok;
	tnode_t *tree = NULL;
	tnode_t **target = &tree;

	if (compopts.verbose) {
		nocc_message ("mcsp_parser_parse(): starting parse..");
	}

	for (;;) {
		tnode_t *thisone;
		int tnflags;
		int breakfor = 0;

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

		thisone = dfa_walk ("mcsp:procdecl", lf);
		if (!thisone) {
			*target = NULL;
			break;		/* for() */
		}
		*target = thisone;
		while (*target) {
			/* sink through nodes */
			tnflags = tnode_tnflagsof (*target);
			if (tnflags & TNF_TRANSPARENT) {
				target = tnode_nthsubaddr (*target, 0);
			} else if (tnflags & TNF_SHORTDECL) {
				target = tnode_nthsubaddr (*target, 2);
			} else if (tnflags & TNF_LONGDECL) {
				target = tnode_nthsubaddr (*target, 3);
			} else {
				/* assume done */
				breakfor = 1;
				break;		/* while() */
			}
		}
		if (breakfor) {
			break;		/* for() */
		}
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
/*{{{  static tnode_t *mcsp_parser_descparse (lexfile_t *lf)*/
/*
 *	parses an MCSP descriptor -- actually some specification/process
 *	returns tree on success, NULL on failure
 */
static tnode_t *mcsp_parser_descparse (lexfile_t *lf)
{
	token_t *tok;
	tnode_t *tree = NULL;
	tnode_t **target = &tree;

	if (compopts.verbose) {
		nocc_message ("mcsp_parser_descparse(): parsing descriptor (specification)..");
	}

	for (;;) {
		tnode_t *thisone;
		int breakfor = 0;
		int tnflags;

		tok = lexer_nexttoken (lf);
		while (tok->type == NEWLINE) {
			lexer_freetoken (tok);
			tok = lexer_nexttoken (lf);
		}
		if ((tok->type == END) || (tok->type == NOTOKEN)) {
			/* done */
			lexer_freetoken (tok);
			break;		/* for() */
		}
		lexer_pushback (lf, tok);

		/* walk as a descriptor-line */
		thisone = dfa_walk ("mcsp:process", lf);
		if (!thisone) {
			*target = NULL;
			break;		/* for() */
		}
#if 0
fprintf (stderr, "mcsp_parser_descparse(): thisone->tag->name = [%s], thisone->tag->ndef->name = [%s]\n", thisone->tag->name, thisone->tag->ndef->name);
#endif
		*target = thisone;
		while (*target) {
			/* sink through things */
			tnflags = tnode_tnflagsof (*target);
			if (tnflags & TNF_TRANSPARENT) {
				target = tnode_nthsubaddr (*target, 0);
			} else if (tnflags & TNF_SHORTDECL) {
				target = tnode_nthsubaddr (*target, 2);
			} else if (tnflags & TNF_LONGDECL) {
				target = tnode_nthsubaddr (*target, 3);
			} else {
				/* assume we're done! */
				breakfor = 1;
				break;		/* while() */
			}
		}
		if (breakfor) {
			break;		/* for() */
		}

		/* next token should be newline or end */
		tok = lexer_nexttoken (lf);
		if ((tok->type != NEWLINE) && (tok->type != END)) {
			parser_error (lf, "in descriptor, expected newline or end, found [%s]", lexer_stokenstr (tok));
			if (tree) {
				tnode_free (tree);
			}
			lexer_freetoken (tok);
			tree = NULL;
			break;		/* for() */
		}
		lexer_pushback (lf, tok);
		/* and go round */
	}

#if 0
fprintf (stderr, "mcsp_parser_descparse(): got tree:\n");
tnode_dumptree (tree, 1, stderr);
#endif

	return tree;
}
/*}}}*/
/*{{{  static int mcsp_parser_prescope (tnode_t **tptr, prescope_t *ps)*/
/*
 *	called to pre-scope the parse tree (whole MCSP only!)
 *	returns 0 on success, non-zero on failure
 */
static int mcsp_parser_prescope (tnode_t **tptr, prescope_t *ps)
{
	tnode_modprewalktree (tptr, prescope_modprewalktree, (void *)ps);

	return ps->err;
}
/*}}}*/
/*{{{  static int mcsp_parser_scope (tnode_t **tptr, scope_t *ss)*/
/*
 *	called to scope declarations in the parse tree
 *	returns 0 on success, non-zero on failure
 */
static int mcsp_parser_scope (tnode_t **tptr, scope_t *ss)
{
	mcsp_scope_t *mss = (mcsp_scope_t *)smalloc (sizeof (mcsp_scope_t));

	mss->uvinsertlist = NULL;
	mss->uvscopemark = NULL;
	mss->inamescope = 0;

	ss->langpriv = (void *)mss;

	tnode_modprepostwalktree (tptr, scope_modprewalktree, scope_modpostwalktree, (void *)ss);

	sfree (mss);
	return ss->err;
}
/*}}}*/
/*{{{  static int mcsp_parser_typecheck (tnode_t *tptr, typecheck_t *tc)*/
/*
 *	called to type-check a tree
 *	returns 0 on success, non-zero on failure
 */
static int mcsp_parser_typecheck (tnode_t *tptr, typecheck_t *tc)
{
	tnode_prewalktree (tptr, typecheck_prewalktree, (void *)tc);
	return tc->err;
}
/*}}}*/
/*{{{  static int mcsp_parser_fetrans (tnode_t **tptr, fetrans_t *fe)*/
/*
 *	called to do front-end transforms on a tree
 *	returns 0 on success, non-zero on failure
 */
static int mcsp_parser_fetrans (tnode_t **tptr, fetrans_t *fe)
{
	mcsp_fetrans_t *mfe = (mcsp_fetrans_t *)smalloc (sizeof (mcsp_fetrans_t));
	int err;

	fe->langpriv = (void *)mfe;
	mfe->errcount = 0;

	for (mfe->parse=0; mfe->parse < 2; mfe->parse++) {
		fetrans_subtree (tptr, fe);
	}

	err = mfe->errcount;
	sfree (mfe);

	if (!err) {
		/*{{{  need to do a little work for interfacing with the world*/
		/*}}}*/
	}

	return err;
}
/*}}}*/


