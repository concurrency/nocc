/*
 *	guppy_parser.c -- Guppy parser for nocc
 *	Copyright (C) 2010 Fred Barnes <frmb@kent.ac.uk>
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
#include "guppy.h"
#include "library.h"
#include "feunit.h"
#include "names.h"
#include "scope.h"
#include "prescope.h"
#include "typecheck.h"
#include "extn.h"
#include "mwsync.h"
#include "metadata.h"


/*}}}*/

/*{{{  forward decls*/
static int guppy_parser_init (lexfile_t *lf);
static void guppy_parser_shutdown (lexfile_t *lf);
static tnode_t *guppy_parser_parse (lexfile_t *lf);
static tnode_t *guppy_parser_descparse (lexfile_t *lf);

static tnode_t *guppy_process (lexfile_t *lf);

/*}}}*/
/*{{{  global vars*/

guppy_pset_t gup;		/* attach tags, etc. here */

langparser_t guppy_parser = {
	langname:	"guppy",
	init:		guppy_parser_init,
	shutdown:	guppy_parser_shutdown,
	parse:		guppy_parser_parse,
	descparse:	guppy_parser_descparse,
	prescope:	NULL,
	scope:		NULL,
	typecheck:	NULL,
	typeresolve:	NULL,
	postcheck:	NULL,
	fetrans:	NULL,
	getlangdef:	guppy_getlangdef,
	maketemp:	NULL,
	makeseqassign:	NULL,
	makeseqany:	NULL,
	tagstruct_hook:	(void *)&gup,
	lexer:		NULL
};

/*}}}*/
/*{{{  private types/vars*/
typedef struct {
	dfanode_t *inode;
	langdef_t *langdefs;
} guppy_parse_t;


static guppy_parse_t *guppy_priv = NULL;

static feunit_t *feunit_set[] = {
	&guppy_primproc_feunit,
	&guppy_fcndef_feunit,
	NULL
};

static ntdef_t *testtruetag, *testfalsetag;


/*}}}*/


/*{{{  static guppy_parse_t *guppy_newguppyparse (void)*/
/*
 *	creates a new guppy_parse_t structure
 */
static guppy_parse_t *guppy_newguppyparse (void)
{
	guppy_parse_t *gpse = (guppy_parse_t *)smalloc (sizeof (guppy_parse_t));

	gpse->inode = NULL;
	gpse->langdefs = NULL;

	return gpse;
}
/*}}}*/
/*{{{  static void guppy_freeguppyparse (guppy_parse_t *gpse)*/
/*
 *	frees an guppy_parse_t structure
 */
static void guppy_freeguppyparse (guppy_parse_t *gpse)
{
	if (!gpse) {
		nocc_warning ("guppy_freeguppyparse(): NULL pointer!");
		return;
	}
	if (gpse->langdefs) {
		langdef_freelangdef (gpse->langdefs);
		gpse->langdefs = NULL;
	}
	/* leave inode alone */
	gpse->inode = NULL;
	sfree (gpse);

	return;
}
/*}}}*/


/*{{{  */
/*
 *	set-indent for debugging output
 */
void guppy_isetindent (FILE *stream, int indent)
{
	int i;

	for (i=0; i<indent; i++) {
		fprintf (stream, "    ");
	}
	return;
}
/*}}}*/
/*{{{  langdef_t *guppy_getlangdef (void)*/
/*
 *	returns the language definition for Guppy, or NULL if none
 */
langdef_t *guppy_getlangdef (void)
{
	if (!guppy_priv) {
		return NULL;
	}
	return guppy_priv->langdefs;
}
/*}}}*/


/*{{{  static tnode_t *guppy_includefile (char *fname, lexfile_t *curlf)*/
/*
 *	includes a file
 *	returns a tree or NULL
 */
static tnode_t *guppy_includefile (char *fname, lexfile_t *curlf)
{
	tnode_t *tree;
	lexfile_t *lf;

	lf = lexer_open (fname);
	if (!lf) {
		parser_error (curlf, "failed to open @include'd file %s", fname);
		return NULL;
	}

	lf->toplevel = 0;
	lf->islibrary = curlf->islibrary;
	lf->sepcomp = curlf->sepcomp;

	if (compopts.verbose) {
		nocc_message ("sub-parsing ...");
	}
	tree = parser_parse (lf);
	if (!tree) {
		parser_error (curlf, "failed to parse @include'd file %s", fname);
		lexer_close (lf);
		return NULL;
	}
	lexer_close (lf);

	return tree;
}
/*}}}*/
/*{{{  static int guppy_parser_init (lexfile_t *lf)*/
/*
 *	initialises the Guppy parser
 *	returns 0 on success, non-zero on error
 */
static int guppy_parser_init (lexfile_t *lf)
{
	if (!guppy_priv) {
		guppy_priv = guppy_newguppyparse ();

		if (compopts.verbose) {
			nocc_message ("initialising guppy parser..");
		}

		memset ((void *)&gup, 0, sizeof (gup));

		guppy_priv->langdefs = langdef_readdefs ("guppy.ldef");
		if (!guppy_priv->langdefs) {
			nocc_error ("guppy_parser_init(): failed to load language definitions!");
			return 1;
		}

		/* register some particular tokens (for later comparison) */

		/* register some general reduction functions */

		/* initialise! */
		if (feunit_do_init_tokens (0, guppy_priv->langdefs, origin_langparser (&guppy_parser))) {
			nocc_error ("guppy_parser_init(): failed to initialise tokens");
			return 1;
		}
		if (feunit_do_init_nodes (feunit_set, 1, guppy_priv->langdefs, origin_langparser (&guppy_parser))) {
			nocc_error ("guppy_parser_init(): failed to initialise nodes");
			return 1;
		}
		if (feunit_do_reg_reducers (feunit_set, 0, guppy_priv->langdefs)) {
			nocc_error ("guppy_parser_init(): failed to register reducers");
			return 1;
		}
		if (feunit_do_init_dfatrans (feunit_set, 1, guppy_priv->langdefs, &guppy_parser, 1)) {
			nocc_error ("guppy_parser_init(): failed to initialise DFAs");
			return 1;
		}
		if (feunit_do_post_setup (feunit_set, 1, guppy_priv->langdefs)) {
			nocc_error ("guppy_parser_init(): failed to post-setup");
			return 1;
		}
		if (langdef_treecheck_setup (guppy_priv->langdefs)) {
			nocc_serious ("guppy_parser_init(): failed to initialise tree-checking!");
		}

		guppy_priv->inode = dfa_lookupbyname ("guppy:declorprocstart");
		if (!guppy_priv->inode) {
			nocc_error ("guppy_parser_init(): could not find guppy:declorprocstart!");
			return 1;
		}
		if (compopts.dumpdfas) {
			dfa_dumpdfas (stderr);
		}
		if (compopts.dumpgrules) {
			parser_dumpgrules (stderr);
		}

		/* last, re-init multiway syncs with default end-of-par option */
		mwsync_setresignafterpar (0);

		parser_gettesttags (&testtruetag, &testfalsetag);
	}
	return 0;
}
/*}}}*/
/*{{{  static void guppy_parser_shutdown (lexfile_t *lf)*/
/*
 *	shuts-down the Guppy parser
 */
static void guppy_parser_shutdown (lexfile_t *lf)
{
	return;
}
/*}}}*/


/*{{{  static tnode_t *guppy_parser_parse (lexfile_t *lf)*/
/*
 *	called to parse a file.
 *	returns a tree on success, NULL on failure
 */
static tnode_t *guppy_parser_parse (lexfile_t *lf)
{
	token_t *tok;
	tnode_t *tree = NULL;
	tnode_t **target = &tree;

	if (compopts.verbose) {
		nocc_message ("guppy_parser_parse(): starting parse..");
	}

	/* FIXME: do parse */

	if (compopts.verbose > 1) {
		nocc_message ("leftover tokens:");
	}

	tok = lexer_nexttoken (lf);
	while (tok) {
		if (compopts.verbose > 1) {
			lexer_dumptoken (stderr, tok);
		}
		if ((tok->type == END) || (tok->type == NOTOKEN)) {
			lexer_freetoken (tok);
			break;			/* while() */
		}
		if ((tok->type != NEWLINE) && (tok->type != COMMENT)) {
			lf->errcount++;
		}

		lexer_freetoken (tok);
		tok = lexer_nexttoken (lf);
	}

	/* if building for separate compilation and top-level, drop in library node */
	if (lf->toplevel && lf->sepcomp && !lf->islibrary) {
		tnode_t *libnode = library_newlibnode (lf, NULL);		/* use default name */

		tnode_setnthsub (libnode, 0, tree);
		tree = libnode;
	}

	return tree;
}
/*}}}*/
/*{{{  static tnode_t *guppy_parser_descparse (lexfile_t *lf)*/
/*
 *	called to parse a descriptor line
 *	returns a tree on success (representing the declaration), NULL on failure
 */
static tnode_t *guppy_parser_descparse (lexfile_t *lf)
{
	token_t *tok;
	tnode_t *tree = NULL;
	tnode_t **target = &tree;

	if (compopts.verbose) {
		nocc_message ("guppy_parser_descparse(): parsing descriptor(s)...");
	}

	/* FIXME: parse */

	return tree;
}
/*}}}*/


