/*
 *	avrasm_parser.c -- AVR assembler parser for nocc
 *	Copyright (C) 2012 Fred Barnes <frmb@kent.ac.uk>
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
#include "dfa.h"
#include "parsepriv.h"
#include "avrasm.h"
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
static int avrasm_parser_init (lexfile_t *lf);
static void avrasm_parser_shutdown (lexfile_t *lf);
static tnode_t *avrasm_parser_parse (lexfile_t *lf);
static int avrasm_parser_prescope (tnode_t **tptr, prescope_t *ps);
static int avrasm_parser_scope (tnode_t **tptr, scope_t *ss);

/*}}}*/
/*{{{  global vars*/

avrasm_pset_t avrasm;

langparser_t avrasm_parser = {
	.langname =		"avrasm",
	.init =			avrasm_parser_init,
	.shutdown =		avrasm_parser_shutdown,
	.parse =		avrasm_parser_parse,
	.descparse =		NULL, // avrasm_parser_descparse,
	.prescope =		avrasm_parser_prescope,
	.scope =		avrasm_parser_scope,
	.typecheck =		NULL, // avrasm_parser_typecheck,
	.typeresolve =		NULL,
	.postcheck =		NULL,
	.fetrans =		NULL,
	.getlangdef =		avrasm_getlangdef,
	.maketemp =		NULL,
	.makeseqassign =	NULL,
	.makeseqany =		NULL,
	.tagstruct_hook =	(void *)&avrasm,
	.lexer =		NULL
};


/*}}}*/
/*{{{  private types/vars*/
typedef struct {
	dfanode_t *inode;
	langdef_t *ldef;
} avrasm_parse_t;

static avrasm_parse_t *avrasm_priv = NULL;

static feunit_t *feunit_set[] = {
	&avrasm_program_feunit,
	NULL
};

/*}}}*/


/*{{{  static avrasm_parse_t *avrasm_newavrasmparse (void)*/
/*
 *	creates a new avrasm_parse_t structure
 */
static avrasm_parse_t *avrasm_newavrasmparse (void)
{
	avrasm_parse_t *avrp = (avrasm_parse_t *)smalloc (sizeof (avrasm_parse_t));

	avrp->inode = NULL;
	avrp->ldef = NULL;

	return avrp;
}
/*}}}*/
/*{{{  static void avrasm_freeavrasmparse (avrasm_parse_t *avrp)*/
/*
 *	frees an avrasm_parse_t structure
 */
static void avrasm_freeavrasmparse (avrasm_parse_t *avrp)
{
	if (!avrp) {
		nocc_warning ("avrasm_freeavrasmparse(): NULL pointer!");
		return;
	}
	if (avrp->ldef) {
		langdef_freelangdef (avrp->ldef);
		avrp->ldef = NULL;
	}
	/* leave inode */
	avrp->inode = NULL;
	sfree (avrp);

	return;
}
/*}}}*/


/*{{{  void avrasm_isetindent (FILE *stream, int indent)*/
/*
 *	set-indent for debugging output
 */
void avrasm_isetindent (FILE *stream, int indent)
{
	int i;

	for (i=0; i<indent; i++) {
		fprintf (stream, "\t");
	}
	return;
}
/*}}}*/
/*{{{  langdef_t *avrasm_getlangdef (void)*/
/*
 *	returns the current language definitions, NULL on failure.
 */
langdef_t *avrasm_getlangdef (void)
{
	if (!avrasm_priv) {
		return NULL;
	}
	return avrasm_priv->ldef;
}
/*}}}*/


/*{{{  static int avrasm_parser_init (lexfile_t *lf)*/
/*
 *	initialises the AVR assembler parser
 *	returns 0 on success, non-zero on failure
 */
static int avrasm_parser_init (lexfile_t *lf)
{
	if (compopts.verbose) {
		nocc_message ("initialising AVR assembler parser..");
	}
	if (!avrasm_priv) {
		avrasm_priv = avrasm_newavrasmparse ();

		memset ((void *)&avrasm, 0, sizeof (avrasm));

		avrasm_priv->ldef = langdef_readdefs ("avrasm.ldef");
		if (!avrasm_priv->ldef) {
			nocc_error ("avrasm_parser_init(): failed to load language definitions!");
			return 1;
		}

		/* initialise */
		if (feunit_do_init_tokens (0, avrasm_priv->ldef, origin_langparser (&avrasm_parser))) {
			nocc_error ("avrasm_parser_init(): failed to initialise tokens");
			return 1;
		}
		if (feunit_do_init_nodes (feunit_set, 1, avrasm_priv->ldef, origin_langparser (&avrasm_parser))) {
			nocc_error ("avrasm_parser_init(): failed to initialise nodes");
			return 1;
		}
		if (feunit_do_reg_reducers (feunit_set, 0, avrasm_priv->ldef)) {
			nocc_error ("avrasm_parser_init(): failed to register reducers");
			return 1;
		}
		if (feunit_do_init_dfatrans (feunit_set, 1, avrasm_priv->ldef, &avrasm_parser, 1)) {
			nocc_error ("avrasm_parser_init(): failed to initialise DFAs");
			return 1;
		}
		if (feunit_do_post_setup (feunit_set, 1, avrasm_priv->ldef)) {
			nocc_error ("avrasm_parser_init(): failed to post-setup");
			return 1;
		}
		if (langdef_treecheck_setup (avrasm_priv->ldef)) {
			nocc_serious ("avrasm_parser(): failed to initialise tree-checking!");
			/* linger on */
		}

		avrasm_priv->inode = dfa_lookupbyname ("avrasm:program");
		if (!avrasm_priv->inode) {
			nocc_error ("avrasm_parser_init(): could not find avrasm:program");
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
/*{{{  static void avrasm_parser_shutdown (lexfile_t *lf)*/
/*
 *	shuts-down the AVR assembler parser
 */
static void avrasm_parser_shutdown (lexfile_t *lf)
{
	if (avrasm_priv) {
		avrasm_freeavrasmparse (avrasm_priv);
		avrasm_priv = NULL;
	}
	return;
}
/*}}}*/


/*{{{  static tnode_t *avrasm_parser_parse (lexfile_t *lf)*/
/*
 *	called to parse a file (containing AVR assembler)
 *	returns a tree on success, NULL on failure
 *
 *	note: for assembler source, tree is just a list to start with
 */
static tnode_t *avrasm_parser_parse (lexfile_t *lf)
{
	token_t *tok;
	tnode_t *tree = parser_newlistnode (lf);

	if (compopts.verbose) {
		nocc_message ("avrasm_parser_parse(): starting parse..");
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

		thisone = dfa_walk ("avrasm:codeline", 0, lf);
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
			lf->errcount++;				/* got errors.. */
		}

		lexer_freetoken (tok);
		tok = lexer_nexttoken (lf);
	}

	return tree;
}
/*}}}*/
/*{{{  static int avrasm_parser_prescope (tnode_t **tptr, prescope_t *ps)*/
/*
 *	called to pre-scope the parse tree
 *	returns 0 on success, non-zero on failure
 */
static int avrasm_parser_prescope (tnode_t **tptr, prescope_t *ps)
{
	ps->hook = NULL;

	if (!*tptr) {
		return -1;
	}
	tnode_modprewalktree (tptr, prescope_modprewalktree, (void *)ps);

	return ps->err;
}
/*}}}*/
/*{{{  static int avrasm_parser_scope (tnode_t **tptr, scope_t *ss)*/
/*
 *	called to scope the parse tree
 *	returns 0 on success, non-zero on failure
 */
static int avrasm_parser_scope (tnode_t **tptr, scope_t *ss)
{
	tnode_t *tree = *tptr;
	tnode_t **items;
	int nitems, i;
	void *nsmark;

	if (!parser_islistnode (tree)) {
		nocc_internal ("avrasm_parser_scope(): top-level tree is not a list! (serious).  Got [%s:%s]\n",
				tree->tag->ndef->name, tree->tag->name);
		return -1;
	}
	items = parser_getlistitems (tree, &nitems);

	nsmark = name_markscope ();
	for (i=0; i<nitems; i++) {
		tnode_t *node = items[i];

		/* first, scope in label names */
		if (node->tag == avrasm.tag_GLABELDEF) {
			tnode_t *lname_node = tnode_nthsubof (node, 0);

			if (lname_node->tag != avrasm.tag_NAME) {
				scope_error (lname_node, ss, "label name not raw-name");
			} else {
				char *rawname = tnode_nthhookof (lname_node, 0);
				name_t *labname;
				tnode_t *namenode;

				labname = name_addscopenamess (rawname, lname_node, NULL, NULL, ss);
				namenode = tnode_createfrom (avrasm.tag_GLABEL, lname_node, labname);
				SetNameNode (labname, namenode);

				tnode_free (lname_node);
				tnode_setnthsub (node, 0, namenode);

				ss->scoped++;
			}
		}
	}

	tnode_modprepostwalktree (tptr, scope_modprewalktree, scope_modpostwalktree, (void *)ss);

	name_markdescope (nsmark);

	return 0;
}
/*}}}*/


