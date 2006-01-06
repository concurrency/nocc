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
#include "extn.h"

/*}}}*/
/*{{{  forward decls*/
static int mcsp_parser_init (lexfile_t *lf);
static void mcsp_parser_shutdown (lexfile_t *lf);
static tnode_t *mcsp_parser_parse (lexfile_t *lf);


/*}}}*/
/*{{{  global vars*/

mcsp_pset_t mcsp;

langparser_t mcsp_parser = {
	langname:	"mcsp",
	init:		mcsp_parser_init,
	shutdown:	mcsp_parser_shutdown,
	parse:		mcsp_parser_parse,
	descparse:	NULL,
	prescope:	NULL,
	scope:		NULL,
	typecheck:	NULL,
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

		mcsp_priv->inode = dfa_lookupbyname ("mcsp:process");
		if (!mcsp_priv->inode) {
			nocc_error ("mcsp_parser_init(): could not find mcsp:process");
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

		thisone = dfa_walk ("mcsp:process", lf);
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


