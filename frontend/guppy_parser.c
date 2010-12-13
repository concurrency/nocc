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

static tnode_t *guppy_process (lexfile_t *lf);
static tnode_t *guppy_indented_process (lexfile_t *lf);
static tnode_t *guppy_indented_process_list (lexfile_t *lf, char *leaddfa);

/*}}}*/
/*{{{  global vars*/

guppy_pset_t gup;		/* attach tags, etc. here */

langparser_t guppy_parser = {
	langname:	"guppy",
	init:		guppy_parser_init,
	shutdown:	guppy_parser_shutdown,
	parse:		guppy_parser_parse,
	descparse:	guppy_parser_descparse,
	prescope:	guppy_parser_prescope,
	scope:		guppy_parser_scope,
	typecheck:	guppy_parser_typecheck,
	typeresolve:	guppy_parser_typeresolve,
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
	&guppy_decls_feunit,
	&guppy_types_feunit,
	&guppy_cnode_feunit,
	&guppy_cflow_feunit,
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
/*{{{  int guppy_autoseq_listtoseqlist (tnode_t **listptr, guppy_autoseq_t *gas)*/
/*
 *	used during auto-sequencing to fixup variable declarations in-list
 *	returns 0 on success, non-zero on failure
 */
int guppy_autoseq_listtoseqlist (tnode_t **listptr, guppy_autoseq_t *gas)
{
	tnode_t *lst = *listptr;
	int nitems = 0;
	tnode_t **items = parser_getlistitems (lst, &nitems);
	int i;

	*listptr = tnode_createfrom (gup.tag_SEQ, lst, NULL, lst);
	for (i=0; i<nitems; i++) {
		if ((items[i]->tag == gup.tag_VARDECL) && (tnode_nthsubof (items[i], 2) == NULL)) {
			break;		/* for() */
		}
	}
	if (i < nitems) {
		/* peel off everything in the list after the declaration and make in body of vardecl, then post-process those */
		tnode_t *newlst = parser_newlistnode (NULL);
		int j;

		for (j=i+1; j<nitems; j++) {
			parser_addtolist (newlst, items[j]);
			items[j] = NULL;
		}

		parser_cleanuplist (lst);
		guppy_autoseq_listtoseqlist (&newlst, gas);
		tnode_setnthsub (items[i], 2, newlst);

	}

	return 0;
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

		/* XXX: these bits are now in the statically generated stuff -- can't modify table generated by gperf */
#if 0
		/* add bits to certain keywords */
		kw = keywords_lookup ("int", 3, LANGTAG_GUPPY);
		if (!kw) {
			nocc_error ("guppy_parser_init(): failed to find \"int\" keyword!\n");
			return 1;
		}
		kw->langtag |= LANGTAG_STYPE;

		kw = keywords_lookup ("real", 4, LANGTAG_GUPPY);
		if (!kw) {
			nocc_error ("guppy_parser_init(): failed to find \"real\" keyword!\n");
			return 1;
		}
		kw->langtag |= LANGTAG_STYPE;
#endif

		/* register some general reduction functions */
		fcnlib_addfcn ("guppy_nametoken_to_hook", (void *)guppy_nametoken_to_hook, 1, 1);

		/* add compiler pass and compiler operation that will be used to pick apart declaration scope and do auto-seq */
		if (nocc_addcompilerpass ("auto-sequence", INTERNAL_ORIGIN, "pre-scope", 1, (int (*)(void *))autoseq_cpass, CPASS_TREEPTR, -1, NULL)) {
			nocc_serious ("guppy_parser_init(): failed to add \"auto-sequence\" compiler pass");
			return 1;
		}
		if (tnode_newcompop ("autoseq", COPS_INVALID, 2, INTERNAL_ORIGIN) < 0) {
			nocc_serious ("guppy_parser_init(): failed to add \"autoseq\" compiler operation");
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
/*{{{  static tnode_t *guppy_declorprocstart (lexfile_t *lf, int *gotall, char *thedfa)*/
/*
 *	parses a declaration/process for single-liner's, or start of a declaration/process
 *	for multi-liners.  Sets "gotall" non-zero if the tree returned is largely whole.
 */
static tnode_t *guppy_declorprocstart (lexfile_t *lf, int *gotall, char *thedfa)
{
	token_t *tok;
	tnode_t *tree = NULL;
	tnode_t **ltarget = &tree;

	/*
	 * for starts of declarations, parsing things like:
	 *     define name (paramlist)
	 */
	
	*gotall = 0;
restartpoint:

	/* skip newlines/comments */
	tok = lexer_nexttoken (lf);

	if (compopts.debugparser) {
		nocc_message ("guppy_declorprocstart(): first token is [%s]", lexer_stokenstr (tok));
	}

	while (tok && ((tok->type == NEWLINE) || (tok->type == COMMENT))) {
		lexer_freetoken (tok);
		tok = lexer_nexttoken (lf);
	}

	if (!tok || (tok->type == END)) {
		if (tok) {
			lexer_freetoken (tok);
		}
		return tree;
	}

	/* if we're parsing a particular ruleset, may need to parse intervening declarations first */
	lexer_pushback (lf, tok);
	if (thedfa) {
		tnode_t *dtest = dfa_walk ("guppy:testfordecl", 0, lf);

		if (dtest->tag == testtruetag) {
			/* right, return this: spot the declaration and handle in guppy_decl() */
			return dtest;
		} else if (dtest->tag == testfalsetag) {
			/* free and continue */
			tnode_free (dtest);
		} else {
			nocc_serious ("guppy_declorprocstart(): guppy:testfordecl DFA returned:");
			tnode_dumptree (dtest, 1, stderr);
			tnode_free (dtest);
		}
	}
	tok = lexer_nexttoken (lf);

	if (lexer_tokmatch (gup.tok_ATSIGN, tok)) {
		/*{{{  probably a pre-processor action of some kind*/
		/*}}}*/
	} else {
		lexer_pushback (lf, tok);
		tree = dfa_walk (thedfa ? thedfa : "guppy:declorprocstart", 0, lf);

		if (lf->toplevel && lf->sepcomp && tree && (tree->tag == gup.tag_FCNDEF)) {
			library_markpublic (tree);
		}
	}

	return tree;
}
/*}}}*/
/*{{{  static tnode_t *guppy_declorproc (lexfile_t *lf, int *gotall, char *thedfa)*/
/*
 *	parses a whole declaration or process, then returns it
 */
static tnode_t *guppy_declorproc (lexfile_t *lf, int *gotall, char *thedfa)
{
	tnode_t *tree = NULL;
	int tnflags;
	int emrk = parser_markerror (lf);
	int earlyret = 0;
	tnode_t **treetarget = &tree;

restartpoint:
	if (compopts.debugparser) {
		nocc_message ("guppy_declorproc(): %s:%d: parsing declaration or process start", lf->fnptr, lf->lineno);
	}

	*treetarget = guppy_declorprocstart (lf, gotall, thedfa);


	if (compopts.debugparser) {
		nocc_message ("guppy_declorproc(): %s:%d: finished parsing declaration or process start, *treetarget [0x%8.8x]",
				lf->fnptr, lf->lineno, (unsigned int)(*treetarget));
		if (*treetarget) {
			nocc_message ("guppy_declorproc(): %s:%d: that *treetarget is (%s,%s)", lf->fnptr, lf->lineno,
					(*treetarget)->tag->ndef->name, (*treetarget)->tag->name);
		}
	}

	if (thedfa && *treetarget && ((*treetarget)->tag == testtruetag)) {
		/*{{{  okay, declaration for sure (unparsed)*/
		int lgotall = 0;
		tnode_t *decl;

		decl = guppy_declorproc (lf, &lgotall, NULL);

#if 0
fprintf (stderr, "guppy_declorproc(): specific DFA [%s] found DECL, parsed it and got:\n", thedfa);
tnode_dumptree (decl, 1, stderr);
#endif
		/* trash test-true flag and put in new declaration */
		tnode_free (*treetarget);
		*treetarget = decl;

		/* sink through */
		while (*treetarget) {
			tnflags = tnode_tnflagsof (*treetarget);

			if (tnflags & TNF_LONGDECL) {
				treetarget = tnode_nthsubaddr (*treetarget, 3);
			} else if (tnflags & TNF_SHORTDECL) {
				treetarget = tnode_nthsubaddr (*treetarget, 2);
			} else if (tnflags & TNF_TRANSPARENT) {
				treetarget = tnode_nthsubaddr (*treetarget, 0);
			} else {
				/* shouldn't get this: means it probably wasn't a declaration.. */
				parser_error (lf, "expected declaration, found [%s]", (*treetarget)->tag->name);
				return tree;
			}
		}

		/* and restart */
		goto restartpoint;
		/*}}}*/
	}

	if (parser_checkerror (lf, emrk)) {
		guppy_skiptoeol (lf, 1);
	}

	if (!tree) {
		return NULL;
	} else if (earlyret) {
		return tree;
	}
#if 0
fprintf (stderr, "guppy_declorproc(): got this tree from declorprocstart:\n");
tnode_dumptree (tree, 1, stderr);
#endif

	/* decide how to deal with it */
	if (!*gotall) {
		tnflags = tnode_tnflagsof (*treetarget);
		if (tnflags & TNF_LONGDECL) {
			/*{{{  long declaration (e.g. PROC, CHAN TYPE, etc.)*/
			int ntflags = tnode_ntflagsof (*treetarget);

			if (ntflags & NTF_INDENTED_PROC) {
				/* parse body into subnode 2 */
				tnode_t *body;
				token_t *tok;

				body = guppy_indented_process (lf);
				tnode_setnthsub (*treetarget, 2, body);
			} else if (ntflags & NTF_INDENTED_PROC_LIST) {
				/* parse body into subnode 2 */
				tnode_t *body;
				token_t *tok;

				body = guppy_indented_process_list (lf, NULL);
				tnode_setnthsub (*treetarget, 2, body);
			}
			/*}}}*/
		} else if (tnflags & TNF_LONGPROC) {
			/*{{{  long process (e.g. SEQ, CLAIM, FORKING, etc.)*/
			int ntflags = tnode_ntflagsof (*treetarget);

			if (ntflags & NTF_INDENTED_PROC_LIST) {
				/*{{{  parse a list of processes into subnode 1*/
				tnode_t *body;

				body = guppy_indented_process_list (lf, NULL);
				tnode_setnthsub (*treetarget, 1, body);
				/*}}}*/
#if 0
			} else if (ntflags & NTF_INDENTED_CONDPROC_LIST) {
				/*{{{  parse a list of indented conditions + processes into subnode 1*/
				tnode_t *body;

				body = guppy_indented_process_list (lf, "guppy:ifcond");
				tnode_setnthsub (*treetarget, 1, body);
				/*}}}*/
			} else if (ntflags & NTF_INDENTED_PROC) {
				/*{{{  parse indented process into subnode 1*/
				tnode_t *body;

				body = guppy_indented_process (lf);
				tnode_setnthsub (*treetarget, 1, body);
				/*}}}*/
			} else if (ntflags & NTF_INDENTED_GUARDPROC_LIST) {
				/*{{{  parses a list of indented guards + processes into subnode 1*/
				tnode_t *body;

				body = guppy_indented_process_list (lf, "guppy:altguard");
				tnode_setnthsub (*treetarget, 1, body);
				/*}}}*/
			} else if (ntflags & NTF_INDENTED_CASEINPUT_LIST) {
				/*{{{  parses a list of indented case-inputs + processes into subnode 1*/
				tnode_t *body;

				body = guppy_indented_process_list (lf, "guppy:caseinputcase");
				tnode_setnthsub (*treetarget, 1, body);
				/*}}}*/
			} else if (ntflags & NTF_INDENTED_PLACEDON_LIST) {
				/*{{{  parses a list of indented placed-on statements + processes into subnode 1*/
				tnode_t *body;

				body = guppy_indented_process_list (lf, "guppy:placedon");
				tnode_setnthsub (*treetarget, 1, body);
				/*}}}*/
#endif
			} else {
				tnode_warning (*treetarget, "guppy_declorproc(): unhandled LONGPROC [%s]", (*treetarget)->tag->name);
			}
			/*}}}*/
		} else if (tnflags & TNF_TRANSPARENT) {
			/*{{{  transparent node (e.g. library usage)*/
			/* FIXME: nothing to do here..? */
			/*}}}*/
		}
	}

	return tree;
}
/*}}}*/
/*{{{  static tnode_t *guppy_declstart (lexfile_t *lf, int *gotall, char *thedfa)*/
/*
 *	parses a declaration for single-liner's, or start of a declaration for multi-line.
 *	Sets "gotall" non-zero if the tree returned is largely whole
 */
static tnode_t *guppy_declstart (lexfile_t *lf, int *gotall, char *thedfa)
{
	token_t *tok;
	tnode_t *tree = NULL;
	tnode_t **ltarget = &tree;

	/*
	 * for starts of declarations, parsing things like:
	 *     define name (paramlist)
	 */
	
	*gotall = 0;
restartpoint:

	/* skip newlines/comments */
	tok = lexer_nexttoken (lf);

	if (compopts.debugparser) {
		nocc_message ("duppy_declstart(): first token is [%s]", lexer_stokenstr (tok));
	}

	while (tok && ((tok->type == NEWLINE) || (tok->type == COMMENT))) {
		lexer_freetoken (tok);
		tok = lexer_nexttoken (lf);
	}

	if (!tok || (tok->type == END)) {
		if (tok) {
			lexer_freetoken (tok);
		}
		return tree;
	}

	/* if we're parsing a particular ruleset, may need to parse intervening declarations first */
	lexer_pushback (lf, tok);
	if (thedfa) {
		tnode_t *dtest = dfa_walk ("guppy:testfordecl", 0, lf);

		if (dtest->tag == testtruetag) {
			/* right, return this: spot the declaration and handle in guppy_decl() */
			return dtest;
		} else if (dtest->tag == testfalsetag) {
			/* free and continue */
			tnode_free (dtest);
		} else {
			nocc_serious ("guppy_declstart(): guppy:testfordecl DFA returned:");
			tnode_dumptree (dtest, 1, stderr);
			tnode_free (dtest);
		}
	}
	tok = lexer_nexttoken (lf);

	if (lexer_tokmatch (gup.tok_ATSIGN, tok)) {
		/*{{{  probably a pre-processor action of some kind*/
		/*}}}*/
	} else {
		lexer_pushback (lf, tok);
		tree = dfa_walk (thedfa ? thedfa : "guppy:declstart", 0, lf);

		if (lf->toplevel && lf->sepcomp && tree && (tree->tag == gup.tag_FCNDEF)) {
			library_markpublic (tree);
		}
	}

	return tree;
}
/*}}}*/
/*{{{  static tnode_t *guppy_decl (lexfile_t *lf, int *gotall, char *thedfa)*/
/*
 *	this parses a guppy declaration and returns it
 */
static tnode_t *guppy_decl (lexfile_t *lf, int *gotall, char *thedfa)
{
	tnode_t *tree = NULL;
	int tnflags;
	int emrk = parser_markerror (lf);
	int earlyret = 0;
	tnode_t **treetarget = &tree;

restartpoint:
	if (compopts.debugparser) {
		nocc_message ("guppy_decl(): %s:%d: parsing declaration", lf->fnptr, lf->lineno);
	}

	*treetarget = guppy_declstart (lf, gotall, thedfa);

	if (compopts.debugparser) {
		nocc_message ("guppy_decl(): %s:%d: finished parsing declaration start, *treetarget [0x%8.8x]",
				lf->fnptr, lf->lineno, (unsigned int)(*treetarget));
		if (*treetarget) {
			nocc_message ("guppy_decl(): %s:%d: that *treetarget is (%s,%s)", lf->fnptr, lf->lineno,
					(*treetarget)->tag->ndef->name, (*treetarget)->tag->name);
		}
	}

	if (thedfa && *treetarget && ((*treetarget)->tag == testtruetag)) {
		/*{{{  okay, declaration for sure (unparsed)*/
		int lgotall = 0;
		tnode_t *decl;

		decl = guppy_decl (lf, &lgotall, NULL);

		tnode_free (*treetarget);
		*treetarget = decl;

		/* sink through */
		while (*treetarget) {
			tnflags = tnode_tnflagsof (*treetarget);

			if (tnflags & TNF_LONGDECL) {
				treetarget = tnode_nthsubaddr (*treetarget, 3);
			} else if (tnflags & TNF_SHORTDECL) {
				treetarget = tnode_nthsubaddr (*treetarget, 2);
			} else if (tnflags & TNF_TRANSPARENT) {
				treetarget = tnode_nthsubaddr (*treetarget, 0);
			} else {
				/* shouldn't get this: means probably wasn't a declaration */
				parser_error (lf, "expected declaration, found [%s]", (*treetarget)->tag->name);
				return tree;
			}
		}

		/* and restart */
		goto restartpoint;
		/*}}}*/
	}

	if (parser_checkerror (lf, emrk)) {
		guppy_skiptoeol (lf, 1);
	}

	if (!tree) {
		return NULL;
	} else if (earlyret) {
		return tree;
	}

	/* decide how to deal with it */
	if (!*gotall) {
		tnflags = tnode_tnflagsof (*treetarget);

		if (tnflags & TNF_LONGDECL) {
			/*{{{  long declaration*/
			int ntflags = tnode_ntflagsof (*treetarget);

			if (ntflags & NTF_INDENTED_PROC) {
				/* parse body into subnode 2 */
				tnode_t *body;
				token_t *tok;

				body = guppy_indented_process (lf);
				tnode_setnthsub (*treetarget, 2, body);
			} else if (ntflags & NTF_INDENTED_PROC_LIST) {
				/* parse body into subnode 2 */
				tnode_t *body;
				token_t *tok;

				body = guppy_indented_process_list (lf, NULL);
				tnode_setnthsub (*treetarget, 2, body);
			}
			/*}}}*/
		} else if (tnflags & TNF_TRANSPARENT) {
			/*{{{  transparent node (e.g. library usage)*/
			/* FIXME: nothing to do here..? */
			/*}}}*/
		}
	}

	return tree;
}
/*}}}*/
/*{{{  static tnode_t *guppy_indented_process_list (lexfile_t *lf, char *leaddfa)*/
/*
 *	parses a list of indented processes.  if "leaddfa" is non-null, parses that
 *	indented before the process (that may have leading declarations too)
 */
static tnode_t *guppy_indented_process_list (lexfile_t *lf, char *leaddfa)
{
	tnode_t *tree = NULL;
	tnode_t *stored = NULL;
	tnode_t **target = &stored;
	token_t *tok;

	if (compopts.debugparser) {
		nocc_message ("guppy_indented_process_list(): %s:%d: parsing indented declaration (%s)", lf->fnptr, lf->lineno, leaddfa ?: "--");
	}

	tree = parser_newlistnode (lf);

	tok = lexer_nexttoken (lf);
	/*{{{  skip newlines*/
	for (; tok && (tok->type == NEWLINE); tok = lexer_nexttoken (lf)) {
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

	/* okay, parse declarations and process */
	for (;;) {
		/*{{{  parse 'leaddfa'*/
		tnode_t *thisone;
		int tnflags;
		int breakfor = 0;
		int gotall = 0;

		thisone = guppy_declorproc (lf, &gotall, leaddfa);
		if (!thisone) {
			*target = NULL;
			break;		/* for() */
		}
#if 0
fprintf (stderr, "guppy_indented_process_list(): parsing DFA [%s], got:\n", leaddfa ?: "--");
tnode_dumptree (thisone, 1, stderr);
#endif
		*target = thisone;
		while (*target) {
			/*{{{  sink through trees*/
			tnflags = tnode_tnflagsof (*target);
			if (!leaddfa) {
				/* no lead DFA, if this was a declaration, retarget */
				if (tnflags & TNF_LONGDECL) {
					target = tnode_nthsubaddr (*target, 3);
				} else if (tnflags & TNF_SHORTDECL) {
					target = tnode_nthsubaddr (*target, 2);
				} else if (tnflags & TNF_TRANSPARENT) {
					target = tnode_nthsubaddr (*target, 0);
				} else {
					/* process with some leading declarations probably -- add to the list */
					parser_addtolist (tree, stored);
					stored = NULL;
					target = &stored;
				}
			} else
#if 0
			if (tnflags & TNF_LONGDECL) {
				target = tnode_nthsubaddr (*target, 3);
			} else if (tnflags & TNF_SHORTDECL) {
				target = tnode_nthsubaddr (*target, 2);
			} else if (tnflags & TNF_TRANSPARENT) {
				target = tnode_nthsubaddr (*target, 0);
			} else
#endif
#if 0
			if (tnflags & TNF_LONGPROC) {
#if 0
fprintf (stderr, "guppy_indented_process_list(): got LONGPROC [%s]\n", (*target)->tag->name);
#endif
			} else
#endif
			{
				/* process with some leading declarations probably -- add to the list */
				parser_addtolist (tree, stored);
				stored = NULL;
				target = &stored;
			}

			/* peek at the next token -- if outdent, get out */
			tok = lexer_nexttoken (lf);
			for (; tok && ((tok->type == NEWLINE) || (tok->type == COMMENT)); tok = lexer_nexttoken (lf)) {
				lexer_freetoken (tok);
			}
			if (tok && (tok->type == OUTDENT)) {
				int lineno = tok->lineno;

				lexer_pushback (lf, tok);
				/*
				 *	slightly ugly check for outdented comments
				 *	(not strictly valid, but we'll allow with a warning)
				 */
#if 0
				if (check_outdented_comment (lf)) {
					parser_warning_line (lf, lineno, "outdented comment");
				} else
#endif
				{
					breakfor = 1;
					break;		/* while() */
				}
			} else {
				lexer_pushback (lf, tok);
			}
			/*}}}*/
		}
		if (breakfor) {
			break;		/* for() */
		}
		/*}}}*/
	}

	if (stored) {
		parser_addtolist (tree, stored);
		stored = NULL;
	}

	tok = lexer_nexttoken (lf);
	/*{{{  skip newlines*/
	for (; tok && ((tok->type == NEWLINE) || (tok->type == COMMENT)); tok = lexer_nexttoken (lf)) {
		lexer_freetoken (tok);
	}

	/*}}}*/
	/*{{{  expect outdent*/
	if (tok->type != OUTDENT) {
		parser_error (lf, "expected outdent, found:");
		lexer_dumptoken (stderr, tok);
		lexer_pushback (lf, tok);
		if (tree) {
			tnode_free (tree);
		}
		tree = NULL;
		return NULL;
	}

	/*}}}*/
	lexer_freetoken (tok);

	if (compopts.debugparser) {
		nocc_message ("guppy_indented_process_list(): %s:%d: done parsing indented process list (tree at 0x%8.8x)",
				lf->fnptr, lf->lineno, (unsigned int)tree);
	}

#if 0
fprintf (stderr, "guppy_indented_process_list(): returning:\n");
tnode_dumptree (tree, 1, stderr);
#endif
	return tree;
}
/*}}}*/
/*{{{  static tnode_t *guppy_indented_process (lexfile_t *lf)*/
/*
 *	parses an indented process
 *	returns the tree on success, NULL on failure
 */
static tnode_t *guppy_indented_process (lexfile_t *lf)
{
	tnode_t *tree = NULL;
	tnode_t **target = &tree;
	token_t *tok;

	tok = lexer_nexttoken (lf);
	/* skip newlines */
	for (; tok && (tok->type == NEWLINE); tok = lexer_nexttoken (lf)) {
		lexer_freetoken (tok);
	}
	/* expect indent */
	if (tok->type != INDENT) {
		parser_error (lf, "expected indent, found:");
		lexer_dumptoken (stderr, tok);
		lexer_pushback (lf, tok);
		return NULL;
	}
	lexer_freetoken (tok);

	/* okay, parse declarations and process */
	for (;;) {
		tnode_t *thisone;
		int tnflags;
		int breakfor = 0;
		int gotall = 0;

		thisone = guppy_declorproc (lf, &gotall, NULL);
		if (!thisone) {
			*target = NULL;
			break;			/* for() */
		}
		*target = thisone;
		while (*target) {
			/* sink through anything included, etc. */
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
				break;		/* while() */
			}
		}
		if (breakfor) {
			break;			/* for() */
		}
	}

	tok = lexer_nexttoken (lf);

	/* skip newlines */
	for (; tok && ((tok->type == NEWLINE) || (tok->type == COMMENT)); tok = lexer_nexttoken (lf)) {
		lexer_freetoken (tok);
	}

	/* expect outdent */
	if (tok->type != OUTDENT) {
		parser_error (lf, "expected outdent, found:");
		lexer_dumptoken (stderr, tok);
		lexer_pushback (lf, tok);
		if (tree) {
			tnode_free (tree);
		}
		tree = NULL;
		return NULL;
	}

	lexer_freetoken (tok);

	return tree;
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

	for (;;) {
		tnode_t *thisone;
		int tnflags;
		int gotall = 0;
		int breakfor = 0;

		thisone = guppy_decl (lf, &gotall, NULL);
		if (!thisone) {
			*target = NULL;
			break;			/* for() */
		}
		*target = thisone;
		while (*target) {
			/* sink through stuff (see this sometimes when @include'ing, etc. */
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



