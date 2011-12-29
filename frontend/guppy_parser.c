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
static int guppy_parser_prescope (tnode_t **tptr, prescope_t *ps);
static int guppy_parser_scope (tnode_t **tptr, scope_t *ss);
static int guppy_parser_typecheck (tnode_t *tptr, typecheck_t *tc);
static int guppy_parser_typeresolve (tnode_t **tptr, typecheck_t *tc);

static tnode_t *guppy_parser_parseproc (lexfile_t *lf);
static tnode_t *guppy_parser_parsedef (lexfile_t *lf);
static tnode_t *guppy_declorproc (lexfile_t *lf);
static tnode_t *guppy_indented_declorproc_list (lexfile_t *lf);

/*}}}*/
/*{{{  global vars*/

guppy_pset_t gup;		/* attach tags, etc. here */

langparser_t guppy_parser = {
	.langname =		"guppy",
	.init =			guppy_parser_init,
	.shutdown =		guppy_parser_shutdown,
	.parse =		guppy_parser_parse,
	.descparse =		guppy_parser_descparse,
	.prescope =		guppy_parser_prescope,
	.scope =		guppy_parser_scope,
	.typecheck =		guppy_parser_typecheck,
	.typeresolve =		guppy_parser_typeresolve,
	.postcheck =		NULL,
	.fetrans =		NULL,
	.getlangdef =		guppy_getlangdef,
	.maketemp =		NULL,
	.makeseqassign =	NULL,
	.makeseqany =		NULL,
	.tagstruct_hook =	(void *)&gup,
	.lexer =		NULL
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
	&guppy_decls_feunit,
	&guppy_types_feunit,
	&guppy_cnode_feunit,
	&guppy_cflow_feunit,
	&guppy_assign_feunit,
	&guppy_io_feunit,
	&guppy_lit_feunit,
	&guppy_oper_feunit,
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
/*{{{  guppy reductions*/
/*{{{  void *guppy_nametoken_to_hook (void *ntok)*/
/*
 *	turns a name token into a hooknode for a tag_NAME
 */
void *guppy_nametoken_to_hook (void *ntok)
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


/*{{{  static int declify_modprewalk (tnode_t **tptr, void *arg)*/
/*
 *	called for each node walked during the 'declify' pass
 *	returns 0 to stop walk, 1 to continue
 */
static int declify_modprewalk (tnode_t **tptr, void *arg)
{
	guppy_declify_t *gdl = (guppy_declify_t *)arg;
	int i = 1;

	if (*tptr && (*tptr)->tag->ndef->ops && tnode_hascompop ((*tptr)->tag->ndef->ops, "declify")) {
		i = tnode_callcompop ((*tptr)->tag->ndef->ops, "declify", 2, tptr, gdl);
	}
	return i;
}
/*}}}*/
/*{{{  static int autoseq_modprewalk (tnode_t **tptr, void *arg)*/
/*
 *	called for each node walked during the 'auto-sequence' pass
 *	returns 0 to stop walk, 1 to continue
 */
static int autoseq_modprewalk (tnode_t **tptr, void *arg)
{
	guppy_autoseq_t *gas = (guppy_autoseq_t *)arg;
	int i = 1;

	if (*tptr && (*tptr)->tag->ndef->ops && tnode_hascompop ((*tptr)->tag->ndef->ops, "autoseq")) {
		i = tnode_callcompop ((*tptr)->tag->ndef->ops, "autoseq", 2, tptr, gas);
	}
	return i;
}
/*}}}*/
/*{{{  static int flattenseq_modprewalk (tnode_t **tptr, void *arg)*/
/*
 *	called for each node walked during the 'flattenseq' pass
 *	returns 0 to stop walk, 1 to continue
 */
static int flattenseq_modprewalk (tnode_t **tptr, void *arg)
{
	int i = 1;

	if (*tptr && (*tptr)->tag->ndef->ops && tnode_hascompop ((*tptr)->tag->ndef->ops, "flattenseq")) {
		i = tnode_callcompop ((*tptr)->tag->ndef->ops, "flattenseq", 1, tptr);
	}
	return i;
}
/*}}}*/


/*{{{  void guppy_isetindent (FILE *stream, int indent)*/
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


/*{{{  int guppy_declify_listtodecllist (tnode_t **listptr, guppy_declify_t *gdl)*/
/*
 *	used during declify to turn lists of declarations and instructions into nested DECLBLOCK nodes
 *	returns 0 on success, non-zero on failure
 */
int guppy_declify_listtodecllist (tnode_t **listptr, guppy_declify_t *gdl)
{
	tnode_t *list = *listptr;
	int nitems = 0;
	tnode_t **items = parser_getlistitems (list, &nitems);
	int i, j;
	tnode_t **nextptr = NULL;

	for (i=0; (i<nitems) && (items[i]->tag == gup.tag_VARDECL); i++);
	for (j=i; (j<nitems) && (items[j]->tag != gup.tag_VARDECL); j++);

	/* note: i is index of first non-decl item, j index of next decl-item or EOL */

#if 0
fprintf (stderr, "guppy_declify_listtodecllist(): i=%d, j=%d, nitems=%d\n", i, j, nitems);
#endif
	if (i > 0) {
		/* at least some declarations -- will trash the whole original list */
		tnode_t *decllist = parser_newlistnode (OrgFileOf (list));
		tnode_t *instlist = parser_newlistnode (OrgFileOf (list));
		tnode_t *vdblock = tnode_createfrom (gup.tag_DECLBLOCK, list, decllist, instlist);
		int k;

		for (k=0; k<i; k++) {
			/* move this one into declaration list */
			parser_addtolist (decllist, items[k]);
		}
		for (; k<nitems; k++) {
			/* move this one into process list below */
			parser_addtolist (instlist, items[k]);
		}

		parser_trashlist (list);
		*listptr = vdblock;
		nextptr = tnode_nthsubaddr (vdblock, 1);
	} else if ((j > 0) && (j < (nitems - 1))) {
		/* item at index j is a declaration, keep original list and fiddle at that point */
		tnode_t *decllist = parser_newlistnode (OrgFileOf (list));
		tnode_t *instlist = parser_newlistnode (OrgFileOf (list));
		tnode_t *vdblock = tnode_createfrom (gup.tag_DECLBLOCK, list, decllist, instlist);

		while ((j < nitems) && (items[j]->tag == gup.tag_VARDECL)) {
			tnode_t *itm = parser_delfromlist (list, j);

			parser_addtolist (decllist, itm);
			nitems--;
		}
		while (j < nitems) {
			/* and if there are any left, put in instlist */
			tnode_t *itm = parser_delfromlist (list, j);

			parser_addtolist (instlist, itm);
			nitems--;
		}

		/* put decl-block in at end */
		parser_addtolist (list, vdblock);
		nextptr = tnode_nthsubaddr (vdblock, 1);
	}

	if (nextptr) {
		/* recurse down into remains of list and do there */
		guppy_declify_listtodecllist (nextptr, gdl);
	}

	return 0;
}
/*}}}*/
/*{{{  int guppy_autoseq_listtoseqlist (tnode_t **listptr, guppy_autoseq_t *gas)*/
/*
 *	used during auto-sequencing to turn lists of instructions into 'seq' nodes
 *	returns 0 on success, non-zero on failure
 */
int guppy_autoseq_listtoseqlist (tnode_t **listptr, guppy_autoseq_t *gas)
{
	tnode_t *lst = *listptr;
	int nitems = 0;
	tnode_t **items = parser_getlistitems (lst, &nitems);

	if (nitems > 0) {
		*listptr = tnode_createfrom (gup.tag_SEQ, lst, NULL, lst);
	}

	return 0;
}
/*}}}*/


/*{{{  int guppy_declify_subtree (tnode_t **tptr, guppy_declify_t *gdl)*/
/*
 *	does declify on a parse-tree (unscoped)
 *	returns 0 on success, non-zero on failure
 */
int guppy_declify_subtree (tnode_t **tptr, guppy_declify_t *gdl)
{
	if (!tptr) {
		nocc_serious ("guppy_declify_subtree(): NULL tree-pointer");
		return 1;
	} else if (!gdl) {
		nocc_serious ("guppy_declify_subtree(): NULL declify structure");
		return 1;
	} else if (!*tptr) {
		return 0;
	} else {
		tnode_modprewalktree (tptr, declify_modprewalk, gdl);
	}

	return gdl->errcount;
}
/*}}}*/
/*{{{  int guppy_autoseq_subtree (tnode_t **tptr, guppy_autoseq_t *gas)*/
/*
 *	does auto-sequencing on a parse-tree (unscoped)
 *	returns 0 on success, non-zero on failure
 */
int guppy_autoseq_subtree (tnode_t **tptr, guppy_autoseq_t *gas)
{
	if (!tptr) {
		nocc_serious ("guppy_autoseq_subtree(): NULL tree-pointer");
		return 1;
	} else if (!gas) {
		nocc_serious ("guppy_autoseq_subtree(): NULL autoseq structure");
		return 1;
	} else if (!*tptr) {
		return 0;
	} else {
		tnode_modprewalktree (tptr, autoseq_modprewalk, gas);
	}

	return gas->errcount;
}
/*}}}*/
/*{{{  int guppy_flattenseq_subtree (tnode_t **tptr)*/
/*
 *	does sequence-node flattening on a subtree (unscoped)
 *	returns 0 on success, non-zero on failure
 */
int guppy_flattenseq_subtree (tnode_t **tptr)
{
	if (!tptr) {
		nocc_serious ("guppy_flattenseq_subtree(): NULL tree-pointer");
		return 1;
	} else if (!*tptr) {
		return 0;
	} else {
		tnode_modprewalktree (tptr, flattenseq_modprewalk, NULL);
	}

	return 0;
}
/*}}}*/


/*{{{  static int declify_cpass (tnode_t **treeptr)*/
/*
 *	called to do the compiler-pass for making declaration blocks
 *	returns 0 on success, non-sero on failure
 */
static int declify_cpass (tnode_t **treeptr)
{
	guppy_declify_t *gdl = (guppy_declify_t *)smalloc (sizeof (guppy_declify_t));
	int err = 0;

	gdl->errcount = 0;
	guppy_declify_subtree (treeptr, gdl);
	err = gdl->errcount;
	sfree (gdl);

	return err;
}
/*}}}*/
/*{{{  static int autoseq_cpass (tnode_t **treeptr)*/
/*
 *	called to do the compiler-pass for auto-sequencing code, applies to particular nodes only
 *	returns 0 on success, non-zero on failure
 */
static int autoseq_cpass (tnode_t **treeptr)
{
	guppy_autoseq_t *gas = (guppy_autoseq_t *)smalloc (sizeof (guppy_autoseq_t));
	int err = 0;

	gas->errcount = 0;
	guppy_autoseq_subtree (treeptr, gas);
	err = gas->errcount;
	sfree (gas);

	return err;
}
/*}}}*/
/*{{{  static int flattenseq_cpass (tnode_t **treeptr)*/
/*
 *	called to do the compiler-pass for flattening sequence nodes
 *	returns 0 on success, non-zero on failure
 */
static int flattenseq_cpass (tnode_t **treeptr)
{
	guppy_flattenseq_subtree (treeptr);
	return 0;
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
		keyword_t *kw;

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
		gup.tok_ATSIGN = lexer_newtoken (SYMBOL, "@");
		gup.tok_STRING = lexer_newtoken (STRING, NULL);
		gup.tok_PUBLIC = lexer_newtoken (KEYWORD, "public");

		/* register some general reduction functions */
		fcnlib_addfcn ("guppy_nametoken_to_hook", (void *)guppy_nametoken_to_hook, 1, 1);

		/* add compiler passes and operations that will be used to pick apart declaration scope and do auto-seq */
		if (nocc_addcompilerpass ("declify", INTERNAL_ORIGIN, "pre-scope", 0, (int (*)(void *))declify_cpass, CPASS_TREEPTR, -1, NULL)) {
			nocc_serious ("guppy_parser_init(): failed to add \"declify\" compiler pass");
			return 1;
		}
		if (nocc_addcompilerpass ("auto-sequence", INTERNAL_ORIGIN, "declify", 0, (int (*)(void *))autoseq_cpass, CPASS_TREEPTR, -1, NULL)) {
			nocc_serious ("guppy_parser_init(): failed to add \"auto-sequence\" compiler pass");
			return 1;
		}
		if (nocc_addcompilerpass ("flattenseq", INTERNAL_ORIGIN, "auto-sequence", 0, (int (*)(void *))flattenseq_cpass, CPASS_TREEPTR, -1, NULL)) {
			nocc_serious ("guppy_parser_init(): failed to add \"flattenseq\" compiler pass");
			return 1;
		}
		if (tnode_newcompop ("declify", COPS_INVALID, 2, INTERNAL_ORIGIN) < 0) {
			nocc_serious ("guppy_parser_init(): failed to add \"declify\" compiler operation");
			return 1;
		}
		if (tnode_newcompop ("autoseq", COPS_INVALID, 2, INTERNAL_ORIGIN) < 0) {
			nocc_serious ("guppy_parser_init(): failed to add \"autoseq\" compiler operation");
			return 1;
		}
		if (tnode_newcompop ("flattenseq", COPS_INVALID, 1, INTERNAL_ORIGIN) < 0) {
			nocc_serious ("guppy_parser_init(): failed to add \"flattenseq\" compiler operation");
			return 1;
		}

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

		guppy_priv->inode = dfa_lookupbyname ("guppy:decl");
		if (!guppy_priv->inode) {
			nocc_error ("guppy_parser_init(): could not find guppy:decl!");
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



/*{{{  static int guppy_skiptoeol (lexfile_t *lf, int skipindent)*/
/*
 *	skips the lexer to the end of a line;  if 'skipindent' is non-zero, will ignore anything indented
 *	to the end of the line.
 *	returns 0 on success (skipped ok), non-zero otherwise
 */
static int guppy_skiptoeol (lexfile_t *lf, int skipindent)
{
	int icount = 0;
	token_t *tok;

	for (;;) {
		tok = lexer_nexttoken (lf);
		if (tok->type == END) {
			/* unexpected */
			lexer_pushback (lf, tok);
			return -1;
		} else if (skipindent && (tok->type == INDENT)) {
			icount++;
		} else if (skipindent && (tok->type == OUTDENT)) {
			icount--;
		} else if ((tok->type == NEWLINE) && (icount <= 0)) {
			/* that's enough */
			lexer_pushback (lf, tok);
			break;
		}
		lexer_freetoken (tok);
	}
	return 0;
}
/*}}}*/


/*{{{  static tnode_t *guppy_parser_parseproc (lexfile_t *lf)*/
/*
 *	parses a single process (in its entirety)
 */
static tnode_t *guppy_parser_parseproc (lexfile_t *lf)
{
	tnode_t *tree = NULL;
	token_t *tok = NULL;
	int emrk = parser_markerror (lf);
	int tnflags;

	tree = dfa_walk ("guppy:procstart", 0, lf);
	if (!tree) {
		/* failed to parse something */
		if (parser_checkerror (lf, emrk)) {
			guppy_skiptoeol (lf, 1);
		}
		return NULL;
	}

	tnflags = tnode_tnflagsof (tree);
	if (tnflags & TNF_LONGPROC) {
		/*{{{  long process (e.g. 'seq', 'par', etc.*/
		int ntflags = tnode_ntflagsof (tree);

		if (ntflags & NTF_INDENTED_PROC_LIST) {
			/*{{{  long process, parse list of indented processes into subnode 1*/
			tnode_t *body = guppy_indented_declorproc_list (lf);

			tnode_setnthsub (tree, 1, body);
			/*}}}*/
		} else {
			tnode_warning (tree, "guppy_parser_parseproc(): unhandled LONGPROC [%s]", tree->tag->name);
		}
		/*}}}*/
	}

	return tree;
}
/*}}}*/
/*{{{  static tnode_t *guppy_declorproc (lexfile_t *lf)*/
/*
 *	parses a single declaration or process
 */
static tnode_t *guppy_declorproc (lexfile_t *lf)
{
	tnode_t *tree = NULL;
	token_t *tok = NULL;

	if (compopts.verbose > 1) {
		nocc_message ("guppy_declorproc(): parsing declaration or process by test at %s:%d", lf->fnptr, lf->lineno);
	}

	/*{{{  skip newlines and comments*/
	tok = lexer_nexttoken (lf);
	while (tok && ((tok->type == NEWLINE) || (tok->type == COMMENT))) {
		lexer_freetoken (tok);
		tok = lexer_nexttoken (lf);
	}
	if (tok) {
		lexer_pushback (lf, tok);
		tok = NULL;
	}

	/*}}}*/

	/* test for a declaration first of all */
	tree = dfa_walk ("guppy:testfordecl", 0, lf);
	if (!tree) {
		parser_error (lf, "expected to find declaration or process, but didn\'t");
	} else if (tree->tag == testtruetag) {
		/* definitely a declaration */
		tnode_free (tree);
		tree = guppy_parser_parsedef (lf);
	} else if (tree->tag == testfalsetag) {
		/* definitely a process */
		tnode_free (tree);
		tree = guppy_parser_parseproc (lf);
	} else {
		nocc_serious ("guppy_declorproc(): guppy_testfordecl DFA returned:");
		tnode_dumptree (tree, 1, stderr);
		tnode_free (tree);
		tree = NULL;
	}

	if (compopts.verbose > 1) {
		nocc_message ("guppy_declorproc(): done parsing declaration or process, got (%s:%s)", tree ? tree->tag->name : "(nil)",
				tree ? tree->tag->ndef->name : "(nil)");
	}

	return tree;
}
/*}}}*/
/*{{{  static tnode_t *guppy_indented_declorproc_list (lexfile_t *lf)*/
/*
 *	parses a list of indented processes or definitions.
 */
static tnode_t *guppy_indented_declorproc_list (lexfile_t *lf)
{
	tnode_t *tree = NULL;
	token_t *tok;

	if (compopts.verbose > 1) {
		nocc_message ("guppy_indented_declorproc_list(): %s:%d: parsing indented declaration or process list", lf->fnptr, lf->lineno);
	}

	tree = parser_newlistnode (lf);

	tok = lexer_nexttoken (lf);
	/*{{{  skip newlines and comments*/
	for (; tok && ((tok->type == NEWLINE) || (tok->type == COMMENT)); tok = lexer_nexttoken (lf)) {
		lexer_freetoken (tok);
	}

	/*}}}*/
	/*{{{  expect indent*/
	if (tok->type != INDENT) {
		parser_error (lf, "expected indent, found:");
		lexer_dumptoken (stderr, tok);
		lexer_pushback (lf, tok);
		tnode_free (tree);
		return NULL;
	}

	/*}}}*/
	lexer_freetoken (tok);

	/* okay, parse declarations and processes */
	for (;;) {
		/*{{{  parse*/
		tnode_t *thisone;

		/* check token for end of indentation */
		tok = lexer_nexttoken (lf);
		/*{{{  skip newlines*/
		for (; tok && ((tok->type == NEWLINE) || (tok->type == COMMENT)); tok = lexer_nexttoken (lf)) {
			lexer_freetoken (tok);
		}

		/*}}}*/
		if (tok->type == OUTDENT) {
			/* got outdent, end-of-list! */
			lexer_freetoken (tok);
			break;		/* for() */
		} else {
			lexer_pushback (lf, tok);
		}

		thisone = guppy_declorproc (lf);

		if (thisone) {
			parser_addtolist (tree, thisone);
		} else {
			/* failed to parse */
			break;		/* for() */
		}
		/*}}}*/
	}

	if (compopts.verbose > 1) {
		nocc_message ("guppy_indented_declorproc_list(): %s:%d: done parsing indented process list (tree at 0x%8.8x)",
				lf->fnptr, lf->lineno, (unsigned int)tree);
	}

#if 0
fprintf (stderr, "guppy_indented_declorproc_list(): returning:\n");
tnode_dumptree (tree, 1, stderr);
#endif
	return tree;
}
/*}}}*/
/*{{{  static tnode_t *guppy_indented_name_list (lexfile_t *lf)*/
/*
 *	parses an indented list of names (and possible assignment-looking things)
 */
static tnode_t *guppy_indented_name_list (lexfile_t *lf)
{
	tnode_t *tree = NULL;
	token_t *tok;

	if (compopts.debugparser) {
		nocc_message ("guppy_indented_name_list(): %s:%d: parsing indented name list", lf->fnptr, lf->lineno);
	}

	tree = parser_newlistnode (lf);

	tok = lexer_nexttoken (lf);
	/*{{{  skip newlines and comments*/
	for (; tok && ((tok->type == NEWLINE) || (tok->type == COMMENT)); tok = lexer_nexttoken (lf)) {
		lexer_freetoken (tok);
	}

	/*}}}*/
	/*{{{  expect indent*/
	if (tok->type != INDENT) {
		parser_error (lf, "expected indent, found:");
		lexer_dumptoken (stderr, tok);
		lexer_pushback (lf, tok);
		tnode_free (tree);
		return NULL;
	}

	/*}}}*/
	lexer_freetoken (tok);

	/* okay, parse names (and possible assignments) */
	for (;;) {
		/*{{{  parse*/
		tnode_t *thisone;

		/* check token for end of indentation */
		tok = lexer_nexttoken (lf);
		/*{{{  skip newlines and comments*/
		for (; tok && ((tok->type == NEWLINE) || (tok->type == COMMENT)); tok = lexer_nexttoken (lf)) {
			lexer_freetoken (tok);
		}

		/*}}}*/
		if (tok->type == OUTDENT) {
			/* got outdent, end-of-list! */
			lexer_freetoken (tok);
			break;		/* for() */
		} else {
			lexer_pushback (lf, tok);
		}

		thisone = dfa_walk ("guppy:nameandassign", 0, lf);

		if (thisone) {
			parser_addtolist (tree, thisone);
		} else {
			/* failed to parse */
			break;		/* for() */
		}
		/*}}}*/
	}

	return tree;
}
/*}}}*/
/*{{{  static tnode_t *guppy_parser_parsedef (lexfile_t *lf)*/
/*
 *	parses a single Guppy definition (e.g. process, type, ...)
 *	returns tree on success, NULL on failure
 */
static tnode_t *guppy_parser_parsedef (lexfile_t *lf)
{
	token_t *tok;
	tnode_t *tree = NULL;
	tnode_t **target = &tree;
	tnode_t *thisone;
	int tnflags, ntflags;
	int publictag = 0;

	if (compopts.debugparser) {
		nocc_message ("guppy_parser_parsedef(): starting parse for single definition..");
	}

	/* eat up comments and newlines */
	tok = lexer_nexttoken (lf);
	while ((tok->type == NEWLINE) || (tok->type == COMMENT)) {
		lexer_freetoken (tok);
		tok = lexer_nexttoken (lf);
	}
	if ((tok->type == END) || (tok->type == NOTOKEN)) {
		/* done */
		lexer_freetoken (tok);
		goto skipout;
	}

	if (lexer_tokmatch (gup.tok_PUBLIC, tok)) {
		lexer_freetoken (tok);
		publictag = 1;
	} else {
		lexer_pushback (lf, tok);
	}

	/* get the definition */
#if 0
fprintf (stderr, "guppy_parser_parsedef(): about to walk for guppy:decl\n");
#endif
	thisone = dfa_walk ("guppy:decl", 0, lf);
#if 0
fprintf (stderr, "guppy_parser_parsedef(): done walking for guppy:decl, got 0x%8.8x, (%s,%s)\n", (unsigned int)thisone,
	thisone ? thisone->tag->name : "(nil)", thisone ? thisone->tag->ndef->name : "(nil)");
#endif
	if (!thisone) {
		*target = NULL;
		goto skipout;
	}
	*target = thisone;
	if (*target) {
		tnflags = tnode_tnflagsof (*target);
		ntflags = tnode_ntflagsof (*target);
	} else {
		tnflags = 0;
		ntflags = 0;
	}

	/* check ntflags for specific structures (e.g. intented process/decl list) */
	if (ntflags & NTF_INDENTED_PROC_LIST) {
		/* parse list of indented processes into subnode 2 */
		thisone = guppy_indented_declorproc_list (lf);
		tnode_setnthsub (*target, 2, thisone);
	} else if (ntflags & NTF_INDENTED_NAME_LIST) {
		/* parse list of indented names into subnode 1 */
		thisone = guppy_indented_name_list (lf);
		tnode_setnthsub (*target, 1, thisone);
	}

	if (publictag) {
		if (tree && (tree->tag == gup.tag_FCNDEF)) {
			library_markpublic (tree);
		}
	} else if (lf->toplevel && lf->sepcomp && tree && (tree->tag == gup.tag_FCNDEF)) {
		library_markpublic (tree);
	}

skipout:

	if (compopts.debugparser) {
		nocc_message ("guppy_parser_parsedef(): done parsing single definition (%p).", tree);
	}

	return tree;
}
/*}}}*/
/*{{{  static int guppy_parser_parseprocdeflist (lexfile_t *lf, tnode_t **target)*/
/*
 *	parses a list of definitions, all indented at the same level
 *	returns 0 on success, non-zero on failure
 */
static int guppy_parser_parsedeflist (lexfile_t *lf, tnode_t **target)
{
	if (compopts.debugparser) {
		nocc_message ("guppy_parser_parsedeflist(): starting parse of process list from [%s]", lf->fnptr);
	}

	for (;;) {
		tnode_t *thisone;
		int gotall = 0;
		int breakfor = 0;
		token_t *tok;

		tok = lexer_nexttoken (lf);
		while ((tok->type == NEWLINE) || (tok->type == COMMENT)) {
			lexer_freetoken (tok);
			tok = lexer_nexttoken (lf);
		}
		if ((tok->type == END) || (tok->type == NOTOKEN)) {
			/* done */
			lexer_freetoken (tok);
			break;				/* for() */
		}
		lexer_pushback (lf, tok);

		thisone = guppy_parser_parsedef (lf);
#if 0
fprintf (stderr, "guppy_parser_parsedeflist(): sausages");
#endif
		if (!thisone) {
			/* nothing left probably */
			break;				/* for() */
		}
		if (!*target) {
			/* make it a list */
			*target = parser_newlistnode (lf);
		} else if (!parser_islistnode (*target)) {
			nocc_internal ("guppy_parser_parsedeflist(): target is not a list! (%s,%s)", (*target)->tag->name, (*target)->tag->ndef->name);
			return -1;
		}
		parser_addtolist (*target, thisone);
	}
	return 0;
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
	int i;

	if (compopts.verbose) {
		nocc_message ("guppy_parser_parse(): starting parse..");
	}

	i = guppy_parser_parsedeflist (lf, &tree);

	if (compopts.debugparser) {
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


/*{{{  static int guppy_parser_prescope (tnode_t **tptr, prescope_t *ps)*/
/*
 *	called to pre-scope the parse tree (or a chunk of it)
 *	returns 0 on success, non-zero on failure
 */
static int guppy_parser_prescope (tnode_t **tptr, prescope_t *ps)
{
	if (!ps->hook) {
		guppy_prescope_t *gps = (guppy_prescope_t *)smalloc (sizeof (guppy_prescope_t));

		gps->last_type = NULL;
		gps->procdepth = 0;
		ps->hook = (void *)gps;
		tnode_modprewalktree (tptr, prescope_modprewalktree, (void *)ps);

		ps->hook = NULL;
		if (gps->last_type) {
			tnode_free (gps->last_type);
			gps->last_type = NULL;
		}
		sfree (gps);
	} else {
		tnode_modprewalktree (tptr, prescope_modprewalktree, (void *)ps);
	}
	return ps->err;
}
/*}}}*/
/*{{{  static int guppy_parser_scope (tnode_t **tptr, scope_t *ss)*/
/*
 *	called to scope declaractions in the parse tree
 *	returns 0 on success, non-zero on failure
 */
static int guppy_parser_scope (tnode_t **tptr, scope_t *ss)
{
	tnode_modprepostwalktree (tptr, scope_modprewalktree, scope_modpostwalktree, (void *)ss);
	return ss->err;
}
/*}}}*/
/*{{{  static int guppy_parser_typecheck (tnode_t *tptr, typecheck_t *tc)*/
/*
 *	called to type-check a tree
 *	returns 0 on success, non-zero on failure
 */
static int guppy_parser_typecheck (tnode_t *tptr, typecheck_t *tc)
{
	tnode_prewalktree (tptr, typecheck_prewalktree, (void *)tc);
	return tc->err;
}
/*}}}*/
/*{{{  static int guppy_parser_typeresolve (tnode_t **tptr, typecheck_t *tc)*/
/*
 *	called to type-resolve a tree
 *	returns 0 on success, non-zero on failure
 */
static int guppy_parser_typeresolve (tnode_t **tptr, typecheck_t *tc)
{
	tnode_modprewalktree (tptr, typeresolve_modprewalktree, (void *)tc);
	return tc->err;
}
/*}}}*/



