/*
 *	occampi_parser.c -- occam-pi parser for nocc
 *	Copyright (C) 2005-2016 Fred Barnes <frmb@kent.ac.uk>
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
#include "langdef.h"
#include "dfa.h"
#include "dfaerror.h"
#include "parsepriv.h"
#include "occampi.h"
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
static int occampi_parser_init (lexfile_t *lf);
static void occampi_parser_shutdown (lexfile_t *lf);
static tnode_t *occampi_parser_parse (lexfile_t *lf);
static tnode_t *occampi_parser_descparse (lexfile_t *lf);
static int occampi_parser_prescope (tnode_t **tptr, prescope_t *ps);
static int occampi_parser_scope (tnode_t **tptr, scope_t *ss);
static int occampi_parser_typecheck (tnode_t *tptr, typecheck_t *tc);
static int occampi_parser_typeresolve (tnode_t **tptr, typecheck_t *tc);
static tnode_t *occampi_parser_maketemp (tnode_t ***insertpointp, tnode_t *type);
static tnode_t *occampi_parser_makeseqassign (tnode_t ***insertpointp, tnode_t *lhs, tnode_t *rhs, tnode_t *type);
static tnode_t *occampi_parser_makeseqany (tnode_t ***insertpointp);

static tnode_t *occampi_indented_process (lexfile_t *lf);
static tnode_t *occampi_indented_process_trailing (lexfile_t *lf, char *extradfa, tnode_t **extrares);
static tnode_t *occampi_indented_process_list (lexfile_t *lf, char *leaddfa);
static tnode_t *occampi_process (lexfile_t *lf);


/*}}}*/
/*{{{  global vars*/

occampi_pset_t opi;		/* attach tags, etc. here */

langparser_t occampi_parser = {
	.langname =		"occam-pi",
	.init =			occampi_parser_init,
	.shutdown =		occampi_parser_shutdown,
	.parse =		occampi_parser_parse,
	.descparse =		occampi_parser_descparse,
	.prescope =		occampi_parser_prescope,
	.scope =		occampi_parser_scope,
	.typecheck =		occampi_parser_typecheck,
	.typeresolve =		occampi_parser_typeresolve,
	.postcheck =		NULL,
	.fetrans =		NULL,
	.getlangdef =		occampi_getlangdef,
	.getlanglibs =		NULL,
	.maketemp =		occampi_parser_maketemp,
	.makeseqassign =	occampi_parser_makeseqassign,
	.makeseqany =		occampi_parser_makeseqany,
	.tagstruct_hook =	(void *)&opi,
	.lexer =		NULL
};


/*}}}*/
/*{{{  private types/vars*/
typedef struct {
	dfanode_t *inode;
	langdef_t *langdefs;
} occampi_parse_t;


static occampi_parse_t *occampi_priv = NULL;

static feunit_t *feunit_set[] = {
	&occampi_primproc_feunit,
	&occampi_cnode_feunit,
	&occampi_snode_feunit,
	&occampi_decl_feunit,
	&occampi_procdecl_feunit,
	&occampi_dtype_feunit,
	&occampi_vector_feunit,
	&occampi_protocol_feunit,
	&occampi_action_feunit,
	&occampi_lit_feunit,
	&occampi_type_feunit,
	&occampi_typeop_feunit,
	&occampi_instance_feunit,
	&occampi_oper_feunit,
	&occampi_function_feunit,
	&occampi_mobiles_feunit,
	&occampi_initial_feunit,
	&occampi_asm_feunit,
	&occampi_traces_feunit,
	&mwsync_feunit,
	&occampi_mwsync_feunit,
	&occampi_misc_feunit,
	&occampi_arrayconstructor_feunit,
	&occampi_ptype_feunit,
	&occampi_timer_feunit,
	&occampi_exceptions_feunit,
	&occampi_placedpar_feunit,
	NULL
};

static ntdef_t *testtruetag, *testfalsetag;


/*}}}*/


/*{{{  static occampi_parse_t *occampi_newoccampiparse (void)*/
/*
 *	creates a new occampi_parse_t structure
 */
static occampi_parse_t *occampi_newoccampiparse (void)
{
	occampi_parse_t *opse = (occampi_parse_t *)smalloc (sizeof (occampi_parse_t));

	opse->inode = NULL;
	opse->langdefs = NULL;

	return opse;
}
/*}}}*/
/*{{{  static void occampi_freeoccampiparse (occampi_parse_t *opse)*/
/*
 *	frees an occampi_parse_t structure
 */
static void occampi_freeoccampiparse (occampi_parse_t *opse)
{
	if (!opse) {
		nocc_warning ("occampi_freeoccampiparse(): NULL pointer!");
		return;
	}
	if (opse->langdefs) {
		langdef_freelangdef (opse->langdefs);
		opse->langdefs = NULL;
	}
	/* leave inode alone */
	opse->inode = NULL;
	sfree (opse);

	return;
}
/*}}}*/
/*{{{  occampi reductions*/
/*{{{  void *occampi_nametoken_to_hook (void *ntok)*/
/*
 *	turns a name token into a hooknode for a tag_NAME
 */
void *occampi_nametoken_to_hook (void *ntok)
{
	token_t *tok = (token_t *)ntok;
	char *rawname;

	rawname = tok->u.name;
	tok->u.name = NULL;

	lexer_freetoken (tok);

	return (void *)rawname;
}
/*}}}*/
/*{{{  void *occampi_stringtoken_to_namehook (void *ntok)*/
/*
 *	turns a string token into a hooknode for a tag_NAME
 */
void *occampi_stringtoken_to_namehook (void *ntok)
{
	token_t *tok = (token_t *)ntok;
	char *rawname;

	rawname = string_ndup (tok->u.str.ptr, tok->u.str.len);
	lexer_freetoken (tok);

	return (void *)rawname;
}
/*}}}*/
/*{{{  void *occampi_keywordtoken_to_namehook (void *ntok)*/
/*
 *	turns a keyword token in to a hooknode for a tag_NAME
 */
void *occampi_keywordtoken_to_namehook (void *ntok)
{
	token_t *tok = (token_t *)ntok;
	char *rawname;

	rawname = string_dup (tok->u.kw->name);
	lexer_freetoken (tok);

	return (void *)rawname;
}
/*}}}*/
/*{{{  void *occampi_integertoken_to_hook (void *itok)*/
/*
 *	turns an integer token into a hooknode for tag_LITINT
 */
void *occampi_integertoken_to_hook (void *itok)
{
	token_t *tok = (token_t *)itok;
	occampi_litdata_t *ldata = (occampi_litdata_t *)smalloc (sizeof (occampi_litdata_t));

	ldata->bytes = sizeof (tok->u.ival);
	ldata->data = mem_ndup (&(tok->u.ival), ldata->bytes);

	lexer_freetoken (tok);

	return (void *)ldata;
}
/*}}}*/
/*{{{  void *occampi_realtoken_to_hook (void *itok)*/
/*
 *	turns an real token into a hooknode for tag_LITREAL
 */
void *occampi_realtoken_to_hook (void *itok)
{
	token_t *tok = (token_t *)itok;
	occampi_litdata_t *ldata = (occampi_litdata_t *)smalloc (sizeof (occampi_litdata_t));

	ldata->bytes = sizeof (tok->u.dval);
	ldata->data = mem_ndup (&(tok->u.dval), ldata->bytes);

	lexer_freetoken (tok);

	return (void *)ldata;
}
/*}}}*/
/*{{{  void *occampi_stringtoken_to_hook (void *itok)*/
/*
 *	turns a string token into a hooknode for a tag_LITARRAY
 */
void *occampi_stringtoken_to_hook (void *itok)
{
	token_t *tok = (token_t *)itok;
	occampi_litdata_t *ldata = (occampi_litdata_t *)smalloc (sizeof (occampi_litdata_t));

	ldata->bytes = tok->u.str.len;
	ldata->data = mem_ndup (tok->u.str.ptr, ldata->bytes);

	lexer_freetoken (tok);

	return (void *)ldata;
}
/*}}}*/

/*{{{  static void occampi_inlistreduce (dfastate_t *dfast, parsepriv_t *pp, void *rarg)*/
/*
 *	this reduces into a list
 */
static void occampi_inlistreduce (dfastate_t *dfast, parsepriv_t *pp, void *rarg)
{
	tnode_t *node;

	if (!parser_islistnode (dfast->local)) {
		/* make into a listnode, set it */
		node = dfast->local;

		dfast->local = parser_newlistnode (SLOCN (pp->lf));
		parser_addtolist (dfast->local, node);
	}
	dfast->ptr = parser_addtolist (dfast->local, NULL);

	return;
}
/*}}}*/
/*{{{  static void occampi_debug_gstack (void **items, int icnt)*/
/*
 *	used for debugging generic reductions
 */
static void occampi_debug_gstack (void **items, int icnt)
{
	int i;

	fhandle_printf (FHAN_STDERR, "occampi_debug_gstack(): icnt=%d, items:  ", icnt);
	for (i=0; i<icnt; i++) {
		fhandle_printf (FHAN_STDERR, "%p  ", items[i]);
	}
	fhandle_printf (FHAN_STDERR, "\n");
	return;
}
/*}}}*/


/*}}}*/

	/*{{{  COMMENT: very manual DFA construction (example)*/
#if 0
	decl = dfa_newnode ();
	tmp = dfa_newnode ();
	tmp2 = dfa_newnode ();
	dfa_matchpush (decl, "occampi:primtype", tmp);
	dfa_matchpush (tmp, "occampi:name", tmp2);
	tmp = tmp2;
	tmp2 = dfa_newnode_init (occampi_reduce_decl);
	dfa_addmatch (tmp, lexer_newtoken (SYMBOL, ":"), tmp2, 0);
	dfa_defaultreturn (tmp2);			/* makes this a returning state */
	dfa_setname (decl, "occampi:decl");

	dfa_dumpdfas (stderr);
#endif
	/*}}}*/


/*{{{  void occampi_isetindent (fhandle_t *stream, int indent)*/
/*
 *	set-indent for debugging output
 */
void occampi_isetindent (fhandle_t *stream, int indent)
{
	int i;

	for (i=0; i<indent; i++) {
		fhandle_printf (stream, "    ");
	}
	return;
}
/*}}}*/
/*{{{  langdef_t *occampi_getlangdef (void)*/
/*
 *	returns the language definition for occam-pi, or NULL if none
 */
langdef_t *occampi_getlangdef (void)
{
	if (!occampi_priv) {
		return NULL;
	}
	return occampi_priv->langdefs;
}
/*}}}*/


/*{{{  static tnode_t *occampi_includefile (char *fname, lexfile_t *curlf)*/
/*
 *	includes a file
 *	returns a tree or NULL
 */
static tnode_t *occampi_includefile (char *fname, lexfile_t *curlf)
{
	tnode_t *tree;
	lexfile_t *lf;

	lf = lexer_open (fname);
	if (!lf) {
		parser_error (SLOCN (curlf), "failed to open #INCLUDE'd file %s", fname);
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
		parser_error (SLOCN (curlf), "failed to parse #INCLUDE'd file %s", fname);
		lexer_close (lf);
		return NULL;
	}
	lexer_close (lf);

	return tree;
}
/*}}}*/
/*{{{  static void occampi_parser_init (lexfile_t *lf)*/
/*
 *	initialises the occam-pi parser
 *	returns 0 on success, non-zero on error
 */
static int occampi_parser_init (lexfile_t *lf)
{
	if (!occampi_priv) {
		occampi_priv = occampi_newoccampiparse ();

		if (compopts.verbose) {
			nocc_message ("initialising occam-pi parser..");
		}

		/* wipe out the "opi" structure */
		memset ((void *)&opi, 0, sizeof (opi));

		occampi_priv->langdefs = langdef_readdefs ("occampi.ldef");
		if (!occampi_priv->langdefs) {
			nocc_error ("occampi_parser_init(): failed to load language definitions!");
			return 1;
		}

		/* register some particular tokens (for comparison later) */
		opi.tok_COLON = lexer_newtoken (SYMBOL, ":");
		opi.tok_HASH = lexer_newtoken (SYMBOL, "#");
		opi.tok_STRING = lexer_newtoken (STRING, NULL);
		opi.tok_PUBLIC = lexer_newtoken (KEYWORD, "PUBLIC");
		opi.tok_TRACES = lexer_newtoken (KEYWORD, "TRACES");
	
		/* register some general reduction functions */
		fcnlib_addfcn ("occampi_inlistreduce", (void *)occampi_inlistreduce, 0, 3);
		fcnlib_addfcn ("occampi_nametoken_to_hook", (void *)occampi_nametoken_to_hook, 1, 1);
		fcnlib_addfcn ("occampi_integertoken_to_hook", (void *)occampi_integertoken_to_hook, 1, 1);
		fcnlib_addfcn ("occampi_realtoken_to_hook", (void *)occampi_realtoken_to_hook, 1, 1);
		fcnlib_addfcn ("occampi_stringtoken_to_hook", (void *)occampi_stringtoken_to_hook, 1, 1);

		/* initialise! */
		if (feunit_do_init_tokens (0, occampi_priv->langdefs, origin_langparser (&occampi_parser))) {
			nocc_error ("occampi_parser_init(): failed to initialise tokens");
			return 1;
		}
		if (feunit_do_init_nodes (feunit_set, 1, occampi_priv->langdefs, origin_langparser (&occampi_parser))) {
			nocc_error ("occampi_parser_init(): failed to initialise nodes");
			return 1;
		}
		if (feunit_do_reg_reducers (feunit_set, 0, occampi_priv->langdefs)) {
			nocc_error ("occampi_parser_init(): failed to register reducers");
			return 1;
		}
		if (feunit_do_init_dfatrans (feunit_set, 1, occampi_priv->langdefs, &occampi_parser, 1)) {
			nocc_error ("occampi_parser_init(): failed to initialise DFAs");
			return 1;
		}
		if (feunit_do_post_setup (feunit_set, 1, occampi_priv->langdefs)) {
			nocc_error ("occampi_parser_init(): failed to post-setup");
			return 1;
		}
		if (langdef_treecheck_setup (occampi_priv->langdefs)) {
			nocc_serious ("occampi_parser_init(): failed to initialise tree-checking!");
		}

		occampi_priv->inode = dfa_lookupbyname ("occampi:declorprocstart");
		if (!occampi_priv->inode) {
			nocc_error ("occampi_parser_init(): could not find occampi:declorprocstart!");
			return 1;
		}
		if (compopts.dumpdfas) {
			dfa_dumpdfas (FHAN_STDERR);
		}
		if (compopts.dumpgrules) {
			parser_dumpgrules (FHAN_STDERR);
		}

		/* last, re-init multiway syncs with default end-of-par option */
		mwsync_setresignafterpar (0);

		parser_gettesttags (&testtruetag, &testfalsetag);
	}
	return 0;
}
/*}}}*/
/*{{{  static void occampi_parser_shutdown (lexfile_t *lf)*/
/*
 *	shuts-down the occam-pi parser
 */
static void occampi_parser_shutdown (lexfile_t *lf)
{
	/* actually, don't do this -- keep initialised */
#if 0
	if (occampi_priv) {
		occampi_freeoccampiparse (occampi_priv);
		occampi_priv = NULL;
	}
#endif

	return;
}
/*}}}*/


/*{{{  static int occampi_skiptoeol (lexfile_t *lf, int skipindent)*/
/*
 *	skips the lexer to the end of a line;  if 'skipindent' is non-zero, will ignore anything indented
 *	to the end of the line.
 *	returns 0 on success (skipped ok), non-zero otherwise
 */
static int occampi_skiptoeol (lexfile_t *lf, int skipindent)
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
/*{{{  static int occampi_tracesparse (lexfile_t *lf, tnode_t *tree)*/
/*
 *	parses a traces specification and attaches to the given tree
 *	returns 0 on success, non-zero on failure
 */
static int occampi_tracesparse (lexfile_t *lf, tnode_t *tree)
{
	/*
	 * this parses:
	 *	TRACES
	 *	  <0 or more String trace specifications>
	 *	:
	 */
	tnode_t *tracenode = NULL;
	char *sbuf;
	token_t *tok;
	lexfile_t *newlex;
#if 0
fprintf (stderr, "occampi_tracesparse(): here!\n");
#endif

	tok = lexer_nexttoken (lf);
	if (!lexer_tokmatch (opi.tok_TRACES, tok)) {
		parser_error (SLOCN (lf), "expected TRACES");
		lexer_pushback (lf, tok);
		return -1;
	}
	lexer_freetoken (tok);

	/* eat up comments and newlines */
	for (tok = lexer_nexttoken (lf); tok && ((tok->type == COMMENT) || (tok->type == NEWLINE)); lexer_freetoken (tok), tok = lexer_nexttoken (lf));

	/* expecting indentation */
	if (!tok || (tok->type != INDENT)) {
		parser_error (SLOCN (lf), "expected indent");
		goto out_error;
	}
	lexer_freetoken (tok);
	tok = lexer_nexttoken (lf);

	if (!tok || (tok->type != STRING)) {
		parser_error (SLOCN (lf), "expected string");
		goto out_error;
	}

	/* feed it to the MCSP parser */
	sbuf = string_ndup (tok->u.str.ptr, tok->u.str.len);
	lexer_freetoken (tok);
	newlex = lexer_openbuf (NULL, "mcsp", sbuf);
	if (!newlex) {
		nocc_error ("occampi_tracesparse(): failed to open traces string");
		sfree (sbuf);
		goto out_error;
	}

	tracenode = parser_descparse (newlex);
	lexer_close (newlex);
	sfree (sbuf);

	if (!tracenode) {
		nocc_error ("occampi_tracesparse(): failed to parse descriptor");
		goto out_error;
	}
#if 0
fprintf (stderr, "occampi_tracesparse(): got tree!:\n");
tnode_dumptree (tracenode, 1, stderr);
#endif

	tracenode = tnode_create (opi.tag_TRACES, SLOCN (lf), tracenode);
	/* attach to tree given */
	tnode_setchook (tree, tnode_lookupchookbyname ("occampi:trace"), (void *)tracenode);

	/* eat up comments and newlines */
	for (tok = lexer_nexttoken (lf); tok && ((tok->type == COMMENT) || (tok->type == NEWLINE)); lexer_freetoken (tok), tok = lexer_nexttoken (lf));

	/* expecting outdent */
	if (!tok || (tok->type != OUTDENT)) {
		parser_error (SLOCN (lf), "expected outdent");
		goto out_error;
	}
	lexer_freetoken (tok);
	tok = lexer_nexttoken (lf);

	if (!tok || !lexer_tokmatch (opi.tok_COLON, tok)) {
		parser_error (SLOCN (lf), "expected :");
		goto out_error;
	}
	lexer_freetoken (tok);

	/* all done.. (hope) */

	return 0;
out_error:
	/* scan to newline */
	if (!tok) {
		tok = lexer_nexttoken (lf);
	}
	while (tok && (tok->type != NEWLINE)) {
		lexer_freetoken (tok);
		tok = lexer_nexttoken (lf);
	}
	if (tok) {
		lexer_pushback (lf, tok);
	}
	return -1;
}
/*}}}*/
#if 0
/*{{{  static int check_outdented_comment (lexfile_t *lf)*/
/*
 *	checks for an outdented comment, that is "{outdent*} [COMMENT|NEWLINE]* {indent*}"
 *	returns 1 if found and skipped, 0 otherwise
 *	(transparently skips outdented whitespace)
 */
static int check_outdented_comment (lexfile_t *lf)
{
	DYNARRAY (token_t *, ltokens);
	token_t *tok;
	int balance = 0;
	int cleanup = 0;
	int i;
	int hadcomment = 0;

	if (compopts.debugparser) {
		nocc_message ("check_outdented_comment(): checking..");
	}

	dynarray_init (ltokens);
	for (;;) {
		tok = lexer_nexttoken (lf);
		dynarray_add (ltokens, tok);

		if (tok->type == OUTDENT) {
			balance--;
		} else if (tok->type == INDENT) {
			balance++;
			if (!balance) {
				/* back to where we were */
				cleanup = 1;
				break;		/* for() */
			}
		} else if (tok->type == NEWLINE) {
			/* ignore */
		} else if (tok->type == COMMENT) {
			/* mostly ignore */
			hadcomment = 1;
		} else {
			/* something else */
			break;		/* for() */
		}
	}

	if (cleanup) {
		/* trash all the tokens we found */
		for (i=0; i<DA_CUR (ltokens); i++) {
			lexer_freetoken (DA_NTHITEM (ltokens, i));
		}
	} else {
		/* push all the tokens back into the lexer */
		for (i=DA_CUR (ltokens) - 1; i>=0; i--) {
			lexer_pushback (lf, DA_NTHITEM (ltokens, i));
		}
	}
	dynarray_trash (ltokens);

	return cleanup && hadcomment;
}
/*}}}*/
/*{{{  static int check_indented_comment (lexfile_t *lf)*/
/*
 *	check for an indented comment, that is "{indent*} [COMMENT|NEWLINE]* {outdent*}"
 *	returns 1 if found and skipped, 0 otherwise
 *	(transparently skips indented whitespace)
 */
static int check_indented_comment (lexfile_t *lf)
{
	DYNARRAY (token_t *, ltokens);
	token_t *tok;
	int balance = 0;
	int cleanup = 0;
	int hadcomment = 0;
	int i;

	if (compopts.debugparser) {
		nocc_message ("check_indented_comment(): checking..");
	}

	dynarray_init (ltokens);
	for (;;) {
		tok = lexer_nexttoken (lf);
		dynarray_add (ltokens, tok);

		if (tok->type == INDENT) {
			balance++;
		} else if (tok->type == OUTDENT) {
			balance--;
			if (!balance) {
				/* back to where we were */
				cleanup = 1;
				break;		/* for() */
			}
		} else if (tok->type == NEWLINE) {
			/* ignore */
		} else if (tok->type == COMMENT) {
			/* mostly ignore */
			hadcomment = 1;
		} else {
			/* something else */
			break;		/* for() */
		}
	}

	if (cleanup) {
		/* trash all the tokens we found */
		for (i=0; i<DA_CUR (ltokens); i++) {
			lexer_freetoken (DA_NTHITEM (ltokens, i));
		}
	} else {
		/* push all the tokens back into the lexer */
		for (i=DA_CUR (ltokens) - 1; i>=0; i--) {
			lexer_pushback (lf, DA_NTHITEM (ltokens, i));
		}
	}
	dynarray_trash (ltokens);

	return cleanup && hadcomment;
}
/*}}}*/
#endif
/*{{{  static tnode_t *occampi_parsemetadata (lexfile_t *lf)*/
/*
 *	parses a "#META" / "#PRAGMA META" directive, from the next token
 *	returns tree object on success, NULL on failure
 */
static tnode_t *occampi_parsemetadata (lexfile_t *lf)
{
	tnode_t *tree = NULL;
	token_t *tok;
	char *m_name = NULL;
	char *m_data = NULL;
	chook_t *mdhook = tnode_lookupornewchook ("metadata");
	void *(*newmdhook)(char *, char *) = (void *(*)(char *, char *))metadata_createmetadata;

	if (!mdhook || !newmdhook) {
		nocc_internal ("occampi_parsemetadata(): something missing..");
		return NULL;
	}


	tok = lexer_nexttoken (lf);
	if (tok && lexer_tokmatch (opi.tok_STRING, tok)) {
		m_name = string_ndup (tok->u.str.ptr, tok->u.str.len);
	} else {
		if (tok) {
			lexer_pushback (lf, tok);
		}
		return NULL;
	}
	lexer_freetoken (tok);

	tok = lexer_nexttoken (lf);
	if (tok && lexer_tokmatch (opi.tok_STRING, tok)) {
		m_data = string_ndup (tok->u.str.ptr, tok->u.str.len);
	} else {
		if (tok) {
			lexer_pushback (lf, tok);
		}
		sfree (m_name);
		return NULL;
	}
	lexer_freetoken (tok);

	tree = tnode_create (opi.tag_METADATA, SLOCN (lf), NULL);
	tnode_setchook (tree, mdhook, newmdhook (m_name, m_data));

	sfree (m_name);
	sfree (m_data);

#if 0
nocc_message ("occampi_parsemetadata(): got metadata!");
#endif

	return tree;
}
/*}}}*/
/*{{{  static int occampi_creepfordecl (lexfile_t *lf)*/
/*
 *	"creeps" on the input to determine whether or not something is a declaration
 *	returns non-zero if declaration, 0 otherwise
 */
static int occampi_creepfordecl (lexfile_t *lf)
{
	return 0;
}
/*}}}*/
/*{{{  static tnode_t *occampi_declorprocstart (lexfile_t *lf, int *gotall, char *thedfa)*/
/*
 *	parses a declaration/process for single-liner's, or start of a declaration/process
 *	for multi-liners.  Sets "gotall" non-zero if the tree returned is largely whole
 */
static tnode_t *occampi_declorprocstart (lexfile_t *lf, int *gotall, char *thedfa)
{
	token_t *tok;
	tnode_t *tree = NULL;
	tnode_t **ltarget = &tree;

	/*
	 * for starts of declarations/processes, parsing things like:
	 *     PROC name (paramlist)
	 *     INT FUNCTION name (paramlist)
	 *     DATA TYPE
	 *     CHAN TYPE
	 *     VALOF
	 *     SEQ, PAR, IF, ALT
	 *     WHILE
	 *     CASE
	 *     in ? CASE
	 *     etc.
	 */

	*gotall = 0;
restartpoint:

	/* skip newlines/comments */
	tok = lexer_nexttoken (lf);

	if (compopts.debugparser) {
		nocc_message ("occampi_declorprocstart(): first token is [%s]", lexer_stokenstr (tok));
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

#if 0
	/*{{{  handle indented/outdented comments*/
	/*
	 * NOTE: need to handle the case of an outdented comment in
	 * the flow of everything else -- not nice!
	 * -- this is not strictly valid occam-pi, but we'll allow it for programmer convenience (with a warning)
	 */
	while ((tok->type == OUTDENT) || (tok->type == INDENT)) {
		int dostopi = 0;

		if (tok->type == OUTDENT) {
			int lineno = tok->lineno;

			lexer_pushback (lf, tok);
			if (check_outdented_comment (lf)) {
				parser_warning_line (lf, lineno, "outdented comment");
			}
			tok = lexer_nexttoken (lf);
		}

		/* and also for an indented comment */
		if (tok->type == INDENT) {
			int lineno = tok->lineno;

			lexer_pushback (lf, tok);
			if (check_indented_comment (lf)) {
				/* if we swallowed it up, fine :) */
			} else {
				/* else don't check indent again */
				dostopi = 1;
			}
			tok = lexer_nexttoken (lf);
		}

		if ((tok->type == INDENT) && dostopi) {
			break;
		}
	}
	/*}}}*/
#endif

	/* if we're parsing a particular ruleset, may need to parse intervening declarations first */
	lexer_pushback (lf, tok);
	if (thedfa) {
		tnode_t *dtest = dfa_walk ("occampi:testfordecl", 0, lf);

		if (dtest->tag == testtruetag) {
#if 0
fprintf (stderr, "occampi_declorprocstart(): specific dfa [%s] found declaration!\n", thedfa);
#endif
			/* right, return this: spot the declaration and handle in occampi_declorproc() */
			return dtest;
		} else if (dtest->tag == testfalsetag) {
#if 0
fprintf (stderr, "occampi_declorprocstart(): specific dfa [%s] found non declaration!\n", thedfa);
#endif
			/* free and continue */
			tnode_free (dtest);
		} else {
			nocc_serious ("occampi_declorprocstart(): occampi:testfordecl DFA returned:");
			tnode_dumptree (dtest, 1, FHAN_STDERR);
			tnode_free (dtest);
		}
	}
	tok = lexer_nexttoken (lf);

	if (lexer_tokmatch (opi.tok_HASH, tok)) {
		/*{{{  probably a pre-processor action of some kind*/
		token_t *nexttok = lexer_nexttoken (lf);

#if 0
nocc_message ("occampi_declorprocstart(): got a hash token from the lexer.");
#endif
		if (nexttok && lexer_tokmatchlitstr (nexttok, "INCLUDE")) {
			/*{{{  #INCLUDE*/
			lexer_freetoken (tok);
			lexer_freetoken (nexttok);

			nexttok = lexer_nexttoken (lf);
			if (nexttok && lexer_tokmatch (opi.tok_STRING, nexttok)) {
				/*{{{  include another file*/
#if 0
fprintf (stderr, "occampi_declorprocstart(): think i should be including another file about here.. :)  [%s]\n", nexttok->u.str.ptr);
#endif
				tree = occampi_includefile (nexttok->u.str.ptr, lf);
				lexer_freetoken (nexttok);
				*gotall = 1;
				/*}}}*/
			} else {
				parser_error (SLOCN (lf), "while processing #INCLUDE, expected string found ");
				lexer_dumptoken (FHAN_STDERR, nexttok);
				lexer_freetoken (nexttok);
				return tree;
			}
			/*}}}*/
		} else if (nexttok && lexer_tokmatchlitstr (nexttok, "OPTION")) {
			/*{{{  #OPTION*/
			lexer_freetoken (tok);
			lexer_freetoken (nexttok);

			nexttok = lexer_nexttoken (lf);
			if (nexttok && lexer_tokmatch (opi.tok_STRING, nexttok)) {
				/*{{{  option for the compiler*/
				char *scopy = string_ndup (nexttok->u.str.ptr, nexttok->u.str.len);

				lexer_freetoken (nexttok);
				if (nocc_dooption_arg (scopy, (void *)lf) < 0) {
					/* failed while processing option */
					parser_error (SLOCN (lf), "failed while processing #OPTION directive");
					sfree (scopy);
					return tree;
				}

				sfree (scopy);
				/*}}}*/
			} else {
				parser_error (SLOCN (lf), "while processing #OPTION, expected string found ");
				lexer_dumptoken (FHAN_STDERR, nexttok);
				lexer_freetoken (nexttok);
				return tree;
			}
			/*}}}*/
		} else if (nexttok && lexer_tokmatchlitstr (nexttok, "LIBRARY")) {
			/*{{{  #LIBRARY*/
			lexer_freetoken (tok);
			lexer_freetoken (nexttok);

			nexttok = lexer_nexttoken (lf);
			if (nexttok && lexer_tokmatch (opi.tok_STRING, nexttok)) {
				/*{{{  the contents of this file will become part of a library*/
				char *sname = string_ndup (nexttok->u.str.ptr, nexttok->u.str.len);

				lexer_freetoken (nexttok);
				tree = library_newlibnode (lf, sname);
				if (!tree) {
					parser_error (SLOCN (lf), "failed while processing #LIBRARY directive");
					sfree (sname);
					return tree;
				}
				sfree (sname);

				for (nexttok = lexer_nexttoken (lf); nexttok && ((nexttok->type == NEWLINE) || (nexttok->type == COMMENT)); lexer_freetoken (nexttok), nexttok = lexer_nexttoken (lf));
				if (nexttok && (nexttok->type == INDENT)) {
					/*{{{  indented under library: INCLUDES, USES, NATIVELIB*/
					lexer_freetoken (nexttok);
					nexttok = lexer_nexttoken (lf);

					for (; nexttok && (nexttok->type != OUTDENT);) {
#if 0
						if (nexttok->type == OUTDENT) {
							int lineno = nexttok->lineno;

							lexer_pushback (lf, nexttok);
							nexttok = NULL;
							if (check_outdented_comment (lf)) {
								parser_warning_line (lf, lineno, "outdented comment");
								nexttok = lexer_nexttoken (lf);
							} else {
								nexttok = lexer_nexttoken (lf);			/* hopefully the one we just pushed back! */
								break;		/* for() */
							}
						}
#endif
						if (lexer_tokmatchlitstr (nexttok, "INCLUDES")) {
							/*{{{  auto-including something*/
							char *iname;

							lexer_freetoken (nexttok);
							nexttok = lexer_nexttoken (lf);

							if (!nexttok || (nexttok->type != STRING)) {
								parser_error (SLOCN (lf), "failed while processing #LIBRARY INCLUDES directive");
								lexer_pushback (lf, nexttok);
								return tree;
							}
							iname = string_ndup (nexttok->u.str.ptr, nexttok->u.str.len);
							if (library_addincludes (tree, iname)) {
								lexer_pushback (lf, nexttok);
								sfree (iname);
								return tree;
							}
							sfree (iname);

							/*}}}*/
						} else if (lexer_tokmatchlitstr (nexttok, "USES")) {
							/*{{{  auto-using something*/
							char *lname;

							lexer_freetoken (nexttok);
							nexttok = lexer_nexttoken (lf);

							if (!nexttok || (nexttok->type != STRING)) {
								parser_error (SLOCN (lf), "failed while processing #LIBRARY USES directive");
								lexer_pushback (lf, nexttok);
								return tree;
							}
							lname = string_ndup (nexttok->u.str.ptr, nexttok->u.str.len);
							if (library_adduses (tree, lname)) {
								lexer_pushback (lf, nexttok);
								sfree (lname);
								return tree;
							}
							sfree (lname);

							/*}}}*/
						} else if (lexer_tokmatchlitstr (nexttok, "NATIVELIB")) {
							/*{{{  native-library specified*/
							char *lname;

							lexer_freetoken (nexttok);
							nexttok = lexer_nexttoken (lf);

							if (!nexttok || (nexttok->type != STRING)) {
								parser_error (SLOCN (lf), "failed while processing #LIBRARY NATIVELIB directive");
								lexer_pushback (lf, nexttok);
								return tree;
							}
							lname = string_ndup (nexttok->u.str.ptr, nexttok->u.str.len);
							if (library_setnativelib (tree, lname)) {
								lexer_pushback (lf, nexttok);
								sfree (lname);
								return tree;
							}
							sfree (lname);

							/*}}}*/
						} else if (lexer_tokmatchlitstr (nexttok, "NAMESPACE")) {
							/*{{{  namespace specified*/
							char *nsname;

							lexer_freetoken (nexttok);
							nexttok = lexer_nexttoken (lf);

							if (!nexttok || (nexttok->type != STRING)) {
								parser_error (SLOCN (lf), "failed while processing #LIBRARY NAMESPACE directive");
								lexer_pushback (lf, nexttok);
								return tree;
							}
							nsname = string_ndup (nexttok->u.str.ptr, nexttok->u.str.len);
							if (library_setnamespace (tree, nsname)) {
								lexer_pushback (lf, nexttok);
								sfree (nsname);
								return tree;
							}
							sfree (nsname);

							/*}}}*/
						} else {
							/*{{{  unknown #LIBRARY directive*/
							parser_error (SLOCN (lf), "unknown #LIBRARY directive");
							lexer_pushback (lf, nexttok);
							return tree;
							/*}}}*/
						}
						lexer_freetoken (nexttok);
						nexttok = lexer_nexttoken (lf);

						/* skip newlines and comments */
						for (; nexttok && ((nexttok->type == NEWLINE) || (nexttok->type == COMMENT)); lexer_freetoken (nexttok), nexttok = lexer_nexttoken (lf));
					}

					if (!nexttok || (nexttok->type != OUTDENT)) {
						parser_error (SLOCN (lf), "failed while processing #LIBRARY directive");
						return tree;
					}
					lexer_freetoken (nexttok);
					/*}}}*/
				} else if (nexttok) {
					/*{{{  push it back!*/
					lexer_pushback (lf, nexttok);
					/*}}}*/
				}

				*gotall = 1;
				/*}}}*/
			} else {
				parser_error (SLOCN (lf), "while processing #LIBRARY, expected string found ");
				lexer_dumptoken (FHAN_STDERR, nexttok);
				lexer_freetoken (nexttok);
				return tree;
			}
			/*}}}*/
		} else if (nexttok && lexer_tokmatchlitstr (nexttok, "USE")) {
			/*{{{  #USE*/
			lexer_freetoken (tok);
			lexer_freetoken (nexttok);

			nexttok = lexer_nexttoken (lf);
			if (nexttok && lexer_tokmatch (opi.tok_STRING, nexttok)) {
				/*{{{  using an external library (or separately compiled file)*/
				char *libname = string_ndup (nexttok->u.str.ptr, nexttok->u.str.len);

				lexer_freetoken (nexttok);
				tree = library_newusenode (lf, libname);
				if (!tree) {
					parser_error (SLOCN (lf), "failed while processing #USE directive");
					sfree (libname);
					return tree;
				}
				sfree (libname);

				/* maybe followed up with "AS <litstring>" for changing namespaces */
				nexttok = lexer_nexttoken (lf);
				if (nexttok && lexer_tokmatchlitstr (nexttok, "AS")) {
					lexer_freetoken (nexttok);
					nexttok = lexer_nexttoken (lf);
					if (nexttok && lexer_tokmatch (opi.tok_STRING, nexttok)) {
						/*{{{  using library AS something else*/
						char *usename = string_ndup (nexttok->u.str.ptr, nexttok->u.str.len);

						lexer_freetoken (nexttok);
						library_setusenamespace (tree, usename);

						sfree (usename);
						/*}}}*/
					} else {
						parser_error (SLOCN (lf), "while processing #USE AS, expected string found ");
						lexer_dumptoken (FHAN_STDERR, nexttok);
						lexer_freetoken (nexttok);
						return tree;
					}
				} else {
					/* something else, discontinue */
					lexer_pushback (lf, nexttok);
				}

				*gotall = 1;
				/*}}}*/
			} else {
				parser_error (SLOCN (lf), "while processing #USE, expected string found ");
				lexer_dumptoken (FHAN_STDERR, nexttok);
				lexer_freetoken (nexttok);
				return tree;
			}
			/*}}}*/
		} else if (nexttok && lexer_tokmatchlitstr (nexttok, "META")) {
			/*{{{  #META*/
			lexer_freetoken (tok);
			lexer_freetoken (nexttok);

			tree = occampi_parsemetadata (lf);
			if (!tree) {
				parser_error (SLOCN (lf), "failed while processing #META directive");
				return tree;
			}
			*gotall = 1;

			/*}}}*/
		} else if (nexttok && lexer_tokmatchlitstr (nexttok, "PRAGMA")) {
			/*{{{  #PRAGMA*/
			lexer_freetoken (tok);
			lexer_freetoken (nexttok);

			nexttok = lexer_nexttoken (lf);
			if (nexttok && lexer_tokmatchlitstr (nexttok, "EXTERNAL")) {
				/*{{{  #PRAGMA EXTERNAL*/
				lexer_freetoken (nexttok);

				nexttok = lexer_nexttoken (lf);
				if (nexttok && lexer_tokmatch (opi.tok_STRING, nexttok)) {
					char *extdef = string_ndup (nexttok->u.str.ptr, nexttok->u.str.len);

					lexer_freetoken (nexttok);
					/*
					 *  NOTE: #PRAGMA EXTERNAL decls go through the library mechanism
					 */
					tree = library_externaldecl (lf, extdef);
					if (!tree) {
						parser_error (SLOCN (lf), "failed while processing #PRAGMA EXTERNAL directive");
						sfree (extdef);
						return tree;
					}
					sfree (extdef);
					*gotall = 1;
				} else {
					parser_error (SLOCN (lf), "malformed #PRAGMA EXTERNAL directive, expected string found ");
					lexer_dumptoken (FHAN_STDERR, nexttok);
					lexer_freetoken (nexttok);
					return tree;
				}
				/*}}}*/
			} else if (nexttok && lexer_tokmatchlitstr (nexttok, "COMMENT")) {
				/*{{{  #PRAGMA COMMENT*/
				lexer_freetoken (nexttok);

				nexttok = lexer_nexttoken (lf);
				if (nexttok && lexer_tokmatch (opi.tok_STRING, nexttok)) {
					chook_t *mchook = tnode_lookupornewchook ("misc:string");

					tree = tnode_create (opi.tag_MISCCOMMENT, SLOCN (lf), NULL);
					tnode_setchook (tree, mchook, string_ndup (nexttok->u.str.ptr, nexttok->u.str.len));

					lexer_freetoken (nexttok);
					*gotall = 1;
				} else {
					parser_error (SLOCN (lf), "malformed #PRAGMA COMMENT directive, expected string found ");
					lexer_dumptoken (FHAN_STDERR, nexttok);
					lexer_freetoken (nexttok);
					return tree;
				}
				/*}}}*/
			} else if (nexttok && lexer_tokmatchlitstr (nexttok, "META")) {
				/*{{{  #PRAGMA META*/
				lexer_freetoken (nexttok);

				tree = occampi_parsemetadata (lf);
				if (!tree) {
					parser_error (SLOCN (lf), "failed while processing #PRAGMA META directive");
					return tree;
				}
				*gotall = 1;

				/*}}}*/
			} else {
				parser_error (SLOCN (lf), "while processing #PRAGMA, expected string found ");
				lexer_dumptoken (FHAN_STDERR, nexttok);
				lexer_freetoken (nexttok);
				return tree;
			}
			/*}}}*/
		} else if (nexttok && lexer_tokmatchlitstr (nexttok, "COMMENT")) {
			/*{{{  #COMMENT*/
			lexer_freetoken (tok);
			lexer_freetoken (nexttok);

			nexttok = lexer_nexttoken (lf);
			if (nexttok && lexer_tokmatch (opi.tok_STRING, nexttok)) {
				chook_t *mchook = tnode_lookupornewchook ("misc:string");

				tree = tnode_create (opi.tag_MISCCOMMENT, SLOCN (lf), NULL);
				tnode_setchook (tree, mchook, string_ndup (nexttok->u.str.ptr, nexttok->u.str.len));

				lexer_freetoken (nexttok);
				*gotall = 1;
			} else {
				parser_error (SLOCN (lf), "malformed #COMMENT directive, expected string found ");
				lexer_dumptoken (FHAN_STDERR, nexttok);
				lexer_freetoken (nexttok);
				return tree;
			}

			/*}}}*/
		} else if (nexttok) {
			parser_error (SLOCN (lf), "unrecognised compiler directive #%s", lexer_stokenstr (nexttok));
			lexer_freetoken (tok);
			lexer_freetoken (nexttok);
		} else {
			parser_error (SLOCN (lf), "malformed compiler directive");
			lexer_freetoken (tok);
			lexer_freetoken (nexttok);
		}
		if (!tree) {
			/* didn't get anything here, go round */
			goto restartpoint;
		}
		/*}}}*/
	} else if (lexer_tokmatch (opi.tok_PUBLIC, tok)) {
		lexer_freetoken (tok);
		tree = dfa_walk (thedfa ? thedfa : "occampi:declorprocstart", 0, lf);

		if (tree) {
			library_markpublic (tree);
		}
	} else {
		lexer_pushback (lf, tok);
		tree = dfa_walk (thedfa ? thedfa : "occampi:declorprocstart", 0, lf);

		if (lf->toplevel && lf->sepcomp && tree && ((tree->tag == opi.tag_PROCDECL) || (tree->tag == opi.tag_FUNCDECL))) {
			library_markpublic (tree);
		}
	}

	return tree;
}
/*}}}*/
/*{{{  static int occampi_procend (lexfile_t *lf)*/
/*
 *	parses a single colon (marking the end of some declaration)
 *	returns 0 on success, -1 on error
 */
static int occampi_procend (lexfile_t *lf)
{
	token_t *tok;

	tok = lexer_nexttoken (lf);
	while (tok && ((tok->type == NEWLINE) || (tok->type == COMMENT))) {
		lexer_freetoken (tok);
		tok = lexer_nexttoken (lf);
	}
	if (!tok) {
		parser_error (SLOCN (lf), "end!");
		return -1;
	}
	if (lexer_tokmatch (opi.tok_COLON, tok)) {
		lexer_freetoken (tok);
	} else if (lexer_tokmatch (opi.tok_TRACES, tok)) {
		/* hang onto this, push back into the lexer */
		lexer_pushback (lf, tok);
	} else {
		parser_error (SLOCN (lf), "expected : found");
		lexer_dumptoken (FHAN_STDERR, tok);
		lexer_pushback (lf, tok);
		return -1;
	}
	return 0;
}
/*}}}*/
/*{{{  static tnode_t *occampi_declorproc (lexfile_t *lf, int *gotall, char *thedfa)*/
/*
 *	this parses a whole declaration or process, then returns it
 */
static tnode_t *occampi_declorproc (lexfile_t *lf, int *gotall, char *thedfa)
{
	tnode_t *tree = NULL;
	int tnflags;
	int emrk = parser_markerror (lf);
	int earlyret = 0;
	tnode_t **treetarget = &tree;

restartpoint:
	if (compopts.debugparser) {
		nocc_message ("occampi_declorproc(): %s:%d: parsing declaration or process start", lf->fnptr, lf->lineno);
	}

	*treetarget = occampi_declorprocstart (lf, gotall, thedfa);

	if (compopts.debugparser) {
		nocc_message ("occampi_declorproc(): %s:%d: finished parsing declaration or process start, *treetarget [%p]",
				lf->fnptr, lf->lineno, *treetarget);
		if (*treetarget) {
			nocc_message ("occampi_declorproc(): %s:%d: that *treetarget is (%s,%s)", lf->fnptr, lf->lineno,
					(*treetarget)->tag->ndef->name, (*treetarget)->tag->name);
		}
	}

	if (thedfa && *treetarget && ((*treetarget)->tag == testtruetag)) {
		/*{{{  okay, declaration for sure (unparsed)*/
		int lgotall = 0;
		tnode_t *decl;

		decl = occampi_declorproc (lf, &lgotall, NULL);

#if 0
fprintf (stderr, "occampi_declorproc(): specific DFA [%s] found DECL, parsed it and got:\n", thedfa);
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
				parser_error (SLOCN (lf), "expected declaration, found [%s]", (*treetarget)->tag->name);
				return tree;
			}
		}

		/* and restart */
		goto restartpoint;
		/*}}}*/
	}

	if (*treetarget && ((*treetarget)->tag == opi.tag_INPUTGUARD)) {
		tnode_t *gexpr = tnode_nthsubof (*treetarget, 0);

		if (gexpr->tag == opi.tag_CASEINPUT) {
			/* rightho, expecting indented list of CASEs at this point */
			tnode_t *body = occampi_indented_process_list (lf, "occampi:caseinputcase");

			tnode_setnthsub (gexpr, 1, body);
#if 0
fprintf (stderr, "occampi_declorproc(): declorprocstart gave back an INPUTGUARD / CASEINPUT:\n");
tnode_dumptree (tree, 1, stderr);
#endif
			earlyret = 1;
		}
	}

	if (parser_checkerror (lf, emrk)) {
		occampi_skiptoeol (lf, 1);
	}

	if (!tree) {
		return NULL;
	} else if (earlyret) {
		return tree;
	}
#if 0
fprintf (stderr, "occampi_declorproc(): got this tree from declorprocstart:\n");
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

				body = occampi_indented_process (lf);
				tnode_setnthsub (*treetarget, 2, body);

				/* check trailing colon */
				if (occampi_procend (lf) < 0) {
					/* failed, but error already reported */
				}

				/* might have some TRACEs left */
				tok = lexer_nexttoken (lf);
				if (lexer_tokmatch (opi.tok_TRACES, tok)) {
					lexer_pushback (lf, tok);
					occampi_tracesparse (lf, *treetarget);
				} else {
					/* nope, pop it back */
					lexer_pushback (lf, tok);
				}
			}
			/*}}}*/
		} else if (tnflags & TNF_LONGPROC) {
			/*{{{  long process (e.g. SEQ, CLAIM, FORKING, etc.)*/
			int ntflags = tnode_ntflagsof (*treetarget);

			if (ntflags & NTF_INDENTED_PROC_LIST) {
				/*{{{  parse a list of processes into subnode 1*/
				tnode_t *body;

				body = occampi_indented_process_list (lf, NULL);
				tnode_setnthsub (*treetarget, 1, body);
				/*}}}*/
			} else if (ntflags & NTF_INDENTED_CONDPROC_LIST) {
				/*{{{  parse a list of indented conditions + processes into subnode 1*/
				tnode_t *body;

				body = occampi_indented_process_list (lf, "occampi:ifcond");
				tnode_setnthsub (*treetarget, 1, body);
				/*}}}*/
			} else if (ntflags & NTF_INDENTED_PROC) {
				/*{{{  parse indented process into subnode 1*/
				tnode_t *body;

				body = occampi_indented_process (lf);
				tnode_setnthsub (*treetarget, 1, body);
				/*}}}*/
			} else if (ntflags & NTF_INDENTED_GUARDPROC_LIST) {
				/*{{{  parses a list of indented guards + processes into subnode 1*/
				tnode_t *body;

				body = occampi_indented_process_list (lf, "occampi:altguard");
				tnode_setnthsub (*treetarget, 1, body);
				/*}}}*/
			} else if (ntflags & NTF_INDENTED_CASEINPUT_LIST) {
				/*{{{  parses a list of indented case-inputs + processes into subnode 1*/
				tnode_t *body;

				body = occampi_indented_process_list (lf, "occampi:caseinputcase");
				tnode_setnthsub (*treetarget, 1, body);
				/*}}}*/
			} else if (ntflags & NTF_INDENTED_PLACEDON_LIST) {
				/*{{{  parses a list of indented placed-on statements + processes into subnode 1*/
				tnode_t *body;

				body = occampi_indented_process_list (lf, "occampi:placedon");
				tnode_setnthsub (*treetarget, 1, body);
				/*}}}*/
			} else if ((*treetarget)->tag == opi.tag_VALOF) {
				/*{{{  parse indented process into subnode 1, extra into subnode 0*/
				tnode_t *body;
				tnode_t *extra = NULL;

				body = occampi_indented_process_trailing (lf, "occampi:valofresult", &extra);
				tnode_setnthsub (*treetarget, 1, body);
				tnode_setnthsub (*treetarget, 0, extra);
				/*}}}*/
			} else if ((*treetarget)->tag == opi.tag_TRY) {
				/*{{{  parse a fairly complex TRY block*/
				tnode_t *body;
				tnode_t *catches;
				tnode_t *finally;
				token_t *tok;

				tok = lexer_nexttoken (lf);
				/* skip newlines */
				for (; tok && (tok->type == NEWLINE); tok = lexer_nexttoken (lf)) {
					lexer_freetoken (tok);
				}
				/* expect indent */
				if (tok->type != INDENT) {
					parser_error (SLOCN (lf), "expected indent, found:");
					lexer_dumptoken (FHAN_STDERR, tok);
					lexer_pushback (lf, tok);
					return tree;
				}
				lexer_freetoken (tok);

				body = occampi_process (lf);
				
				/* skip newlines and comments */
				tok = lexer_nexttoken (lf);
				for (; tok && ((tok->type == NEWLINE) || (tok->type == COMMENT)); tok = lexer_nexttoken (lf)) {
					lexer_freetoken (tok);
				}

				/* might have a CATCH */
				if (tok && (tok->type == OUTDENT)) {
					lexer_pushback (lf, tok);
					catches = NULL;
				} else {
					if (tok) {
						lexer_pushback (lf, tok);
					}
					catches = occampi_process (lf);
				}

				/* skip newlines and comments */
				tok = lexer_nexttoken (lf);
				for (; tok && ((tok->type == NEWLINE) || (tok->type == COMMENT)); tok = lexer_nexttoken (lf)) {
					lexer_freetoken (tok);
				}

				/* might have a FINALLY */
				if (tok && (tok->type == OUTDENT)) {
					lexer_pushback (lf, tok);
  					finally = NULL;
				} else {
					if (tok) {
						lexer_pushback (lf, tok);
					}
					finally = occampi_process (lf);
				}

				tnode_setnthsub (*treetarget, 1, body);
				tnode_setnthsub (*treetarget, 2, catches);
				tnode_setnthsub (*treetarget, 3, finally);

				/* skip newlines */
				tok = lexer_nexttoken (lf);
				for (; tok && ((tok->type == NEWLINE) || (tok->type == COMMENT)); tok = lexer_nexttoken (lf)) {
					lexer_freetoken (tok);
				}
				/* expect outdent */
				if (tok->type != OUTDENT) {
					parser_error (SLOCN (lf), "expected outdent, found:");
					lexer_dumptoken (FHAN_STDERR, tok);
					lexer_pushback (lf, tok);
					if (*treetarget) {
						tnode_free (*treetarget);
					}
					*treetarget = NULL;
					return tree;
				}
				lexer_freetoken (tok);

				/*}}}*/
			} else if ((*treetarget)->tag == opi.tag_CATCH) {
				/*{{{  parse an indented list of CATCH expressions into subnode 0*/
				tnode_t *body;

				body = occampi_indented_process_list (lf, "occampi:catchexpr");
				tnode_setnthsub (*treetarget, 0, body);
				/*}}}*/
			} else {
				tnode_warning (*treetarget, "occampi_declorproc(): unhandled LONGPROC [%s]", (*treetarget)->tag->name);
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
/*{{{  static tnode_t *occampi_indented_process_trailing (lexfile_t *lf, char *extradfa, tnode_t **extrares)*/
/*
 *	parses an indented process with an extra bit at the end before
 *	the outdent.
 */
static tnode_t *occampi_indented_process_trailing (lexfile_t *lf, char *extradfa, tnode_t **extrares)
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
		parser_error (SLOCN (lf), "expected indent, found:");
		lexer_dumptoken (FHAN_STDERR, tok);
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

		thisone = occampi_declorproc (lf, &gotall, NULL);
		if (!thisone) {
			*target = NULL;
			break;		/* for() */
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
			break;		/* for() */
		}
	}

	tok = lexer_nexttoken (lf);
	/* skip newlines */
	for (; tok && (tok->type == NEWLINE); tok = lexer_nexttoken (lf)) {
		lexer_freetoken (tok);
	}
	/* now we try and parse the extra bit */
	{
		int gotall = 0;

		lexer_pushback (lf, tok);
		*extrares = occampi_declorproc (lf, &gotall, extradfa);
	}

	tok = lexer_nexttoken (lf);
	/* skip newlines and comments */
	for (; tok && ((tok->type == NEWLINE) || (tok->type == COMMENT)); tok = lexer_nexttoken (lf)) {
		lexer_freetoken (tok);
	}
	/* expect outdent */
	if (tok->type != OUTDENT) {
		parser_error (SLOCN (lf), "expected outdent, found:");
		lexer_dumptoken (FHAN_STDERR, tok);
		lexer_pushback (lf, tok);
		if (tree) {
			tnode_free (tree);
		}
		if (*extrares) {
			tnode_free (*extrares);
		}
		tree = NULL;
		*extrares = NULL;

		return NULL;
	}
	lexer_freetoken (tok);

	return tree;
}
/*}}}*/
/*{{{  static tnode_t *occampi_indented_process (lexfile_t *lf)*/
/*
 *	parses an indented process
 *	returns the tree on success, NULL on failure
 */
static tnode_t *occampi_indented_process (lexfile_t *lf)
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
		parser_error (SLOCN (lf), "expected indent, found:");
		lexer_dumptoken (FHAN_STDERR, tok);
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

		thisone = occampi_declorproc (lf, &gotall, NULL);
		if (!thisone) {
			*target = NULL;
			break;		/* for() */
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
			break;		/* for() */
		}
	}

	tok = lexer_nexttoken (lf);
	/* skip newlines */
	for (; tok && ((tok->type == NEWLINE) || (tok->type == COMMENT)); tok = lexer_nexttoken (lf)) {
		lexer_freetoken (tok);
	}
	/* expect outdent */
	if (tok->type != OUTDENT) {
		parser_error (SLOCN (lf), "expected outdent, found:");
		lexer_dumptoken (FHAN_STDERR, tok);
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
/*{{{  static tnode_t *occampi_indented_process_list (lexfile_t *lf, char *leaddfa)*/
/*
 *	parses a list of indented processes.  if "leaddfa" is non-null, parses that
 *	indented before the process (that may have leading declarations too)
 */
static tnode_t *occampi_indented_process_list (lexfile_t *lf, char *leaddfa)
{
	tnode_t *tree = NULL;
	tnode_t *stored = NULL;
	tnode_t **target = &stored;
	token_t *tok;

	if (compopts.debugparser) {
		nocc_message ("occampi_indented_process_list(): %s:%d: parsing indented declaration (%s)", lf->fnptr, lf->lineno, leaddfa ?: "--");
	}

	tree = parser_newlistnode (SLOCN (lf));

	tok = lexer_nexttoken (lf);
	/*{{{  skip newlines*/
	for (; tok && (tok->type == NEWLINE); tok = lexer_nexttoken (lf)) {
		lexer_freetoken (tok);
	}

	/*}}}*/
	/*{{{  expect indent*/
	if (tok->type != INDENT) {
		parser_error (SLOCN (lf), "expected indent, found:");
		lexer_dumptoken (FHAN_STDERR, tok);
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

		thisone = occampi_declorproc (lf, &gotall, leaddfa);
		if (!thisone) {
			*target = NULL;
			break;		/* for() */
		}
#if 0
fprintf (stderr, "occampi_indented_process_list(): parsing DFA [%s], got:\n", leaddfa ?: "--");
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
fprintf (stderr, "occampi_indented_process_list(): got LONGPROC [%s]\n", (*target)->tag->name);
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
		parser_error (SLOCN (lf), "expected outdent, found:");
		lexer_dumptoken (FHAN_STDERR, tok);
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
		nocc_message ("occampi_indented_process_list(): %s:%d: done parsing indented process list (tree at 0x%p)",
				lf->fnptr, lf->lineno, tree);
	}

#if 0
fprintf (stderr, "occampi_indented_process_list(): returning:\n");
tnode_dumptree (tree, 1, stderr);
#endif
	return tree;
}
/*}}}*/
/*{{{  static tnode_t *occampi_process (lexfile_t *lf)*/
/*
 *	parses a plain process, potentially surrounded by newlines and comments
 *	returns the tree on success, NULL on failure
 */
static tnode_t *occampi_process (lexfile_t *lf)
{
	tnode_t *tree = NULL;
	tnode_t **target = &tree;
	token_t *tok;

	tok = lexer_nexttoken (lf);
	/*{{{  skip newlines and comments*/
	for (; tok && ((tok->type == NEWLINE) || (tok->type == COMMENT)); tok = lexer_nexttoken (lf)) {
		lexer_freetoken (tok);
	}

	/*}}}*/
	lexer_pushback (lf, tok);

	/* okay, parse declarations and process */
	for (;;) {
		/*{{{  parse default*/
		tnode_t *thisone;
		int tnflags;
		int breakfor = 0;
		int gotall = 0;

		thisone = occampi_declorproc (lf, &gotall, NULL);
		if (!thisone) {
			*target = NULL;
			break;		/* for() */
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
			break;		/* for() */
		}
		/*}}}*/
	}

	tok = lexer_nexttoken (lf);
	/*{{{  skip newlines*/
	for (; tok && ((tok->type == NEWLINE) || (tok->type == COMMENT)); tok = lexer_nexttoken (lf)) {
		lexer_freetoken (tok);
	}

	/*}}}*/
	lexer_pushback (lf, tok);

	return tree;
}
/*}}}*/


/*{{{  static tnode_t *occampi_parser_parse (lexfile_t *lf)*/
/*
 *	called to parse a file.
 *	returns a tree on success, NULL on failure
 */
static tnode_t *occampi_parser_parse (lexfile_t *lf)
{
	token_t *tok;
	tnode_t *tree = NULL;
	tnode_t **target = &tree;

	if (compopts.verbose) {
		nocc_message ("occampi_parser_parse(): starting parse..");
	}

	for (;;) {
		tnode_t *thisone;
		int tnflags;
		int gotall = 0;
		int breakfor = 0;

		thisone = occampi_declorproc (lf, &gotall, NULL);
		if (!thisone) {
			*target = NULL;
			break;		/* for() */
		}
		*target = thisone;
		while (*target) {
			/* sink through stuff (see this sometimes when INCLUDE'ing,etc. */
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
			break;
		}
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

	/* if building for separate compilation and top-level, drop in library node */
	if (lf->toplevel && lf->sepcomp && !lf->islibrary) {
		tnode_t *libnode = library_newlibnode (lf, NULL);		/* use default name */

		tnode_setnthsub (libnode, 0, tree);
		tree = libnode;
	}

	return tree;
}
/*}}}*/
/*{{{  static tnode_t *occampi_parser_descparse (lexfile_t *lf)*/
/*
 *	called to parse a descriptor line
 *	returns a tree on success (representing the declaration), NULL on failure
 */
static tnode_t *occampi_parser_descparse (lexfile_t *lf)
{
	token_t *tok;
	tnode_t *tree = NULL;
	tnode_t **target = &tree;

	if (compopts.verbose) {
		nocc_message ("occampi_parser_descparse(): parsing descriptor(s)..");
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
		thisone = dfa_walk ("occampi:descriptorline", 0, lf);
		if (!thisone) {
			*target = NULL;
			break;		/* for() */
		}
		*target = thisone;
		while (*target) {
			/* sink through things */
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
			break;		/* for() */
		}

		/* next token should be newline or end */
		tok = lexer_nexttoken (lf);
		if ((tok->type != NEWLINE) && (tok->type != END)) {
			parser_error (SLOCN (lf), "in descriptor, expected newline or end, found [%s]", lexer_stokenstr (tok));
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

	return tree;
}
/*}}}*/
/*{{{  static int occampi_parser_prescope (tnode_t **tptr, prescope_t *ps)*/
/*
 *	called to pre-scope the parse tree (or a chunk of it)
 *	returns 0 on success, non-zero on failure
 */
static int occampi_parser_prescope (tnode_t **tptr, prescope_t *ps)
{

	if (!ps->hook) {
		occampi_prescope_t *ops = (occampi_prescope_t *)smalloc (sizeof (occampi_prescope_t));

		ops->last_type = NULL;
		ops->procdepth = 0;
		ps->hook = (void *)ops;
		tnode_modprewalktree (tptr, prescope_modprewalktree, (void *)ps);

		ps->hook = NULL;
		if (ops->last_type) {
			tnode_free (ops->last_type);
			ops->last_type = NULL;
		}
		sfree (ops);
	} else {
		tnode_modprewalktree (tptr, prescope_modprewalktree, (void *)ps);
	}
	return ps->err;
}
/*}}}*/
/*{{{  static int occampi_parser_scope (tnode_t **tptr)*/
/*
 *	called to scope declarations in the parse tree
 *	returns 0 on success, non-zero on failure
 */
static int occampi_parser_scope (tnode_t **tptr, scope_t *ss)
{
	tnode_modprepostwalktree (tptr, scope_modprewalktree, scope_modpostwalktree, (void *)ss);
	return ss->err;
}
/*}}}*/
/*{{{  static int occampi_parser_typecheck (tnode_t *tptr, typecheck_t *tc)*/
/*
 *	called to type-check a tree
 *	returns 0 on success, non-zero on failure
 */
static int occampi_parser_typecheck (tnode_t *tptr, typecheck_t *tc)
{
	tnode_prewalktree (tptr, typecheck_prewalktree, (void *)tc);
	return tc->err;
}
/*}}}*/
/*{{{  static int occampi_parser_typeresolve (tnode_t **tptr, typecheck_t *tc)*/
/*
 *	called to type-resolve a tree
 *	returns 0 on success, non-zero on failure
 */
static int occampi_parser_typeresolve (tnode_t **tptr, typecheck_t *tc)
{
	tnode_modprewalktree (tptr, typeresolve_modprewalktree, (void *)tc);
	return tc->err;
}
/*}}}*/
/*{{{  static tnode_t *occampi_parser_maketemp (tnode_t ***insertpointp, tnode_t *type)*/
/*
 *	called from elsewhere to create a temporary.  This creates a DECL/NDECL pair
 *	returns the NDECL part (reference)
 */
static tnode_t *occampi_parser_maketemp (tnode_t ***insertpointp, tnode_t *type)
{
	tnode_t *namenode = NULL;
	name_t *name;

	name = name_addtempname (NULL, type, opi.tag_NDECL, &namenode);
	**insertpointp = tnode_createfrom (opi.tag_VARDECL, **insertpointp, namenode, type, **insertpointp);
	SetNameDecl (name, **insertpointp);

	*insertpointp = tnode_nthsubaddr (**insertpointp, 2);

	return namenode;
}
/*}}}*/
/*{{{  static tnode_t *occampi_parser_makeseqassign (tnode_t ***insertpointp, tnode_t *lhs, tnode_t *rhs, tnode_t *type)*/
/*
 *	called from elsewhere to create a sequential assignment.
 *	returns the ASSIGN part
 */
static tnode_t *occampi_parser_makeseqassign (tnode_t ***insertpointp, tnode_t *lhs, tnode_t *rhs, tnode_t *type)
{
	tnode_t *assnode = tnode_createfrom (opi.tag_ASSIGN, **insertpointp, lhs, rhs, type);
	tnode_t *listnode = parser_buildlistnode ((**insertpointp)->org, assnode, NULL);
	tnode_t *seqnode = tnode_createfrom (opi.tag_SEQ, **insertpointp, NULL, listnode);
	tnode_t *savedproc = **insertpointp;

	**insertpointp = seqnode;
	*insertpointp = parser_addtolist (listnode, savedproc);

	return assnode;
}
/*}}}*/
/*{{{  static tnode_t *occampi_parser_makeseqany (tnode_t ***insertpointp)*/
/*
 *	called from elsewhere to insert a sequential node
 *	returns the list associated with the sequence of actions
 */
static tnode_t *occampi_parser_makeseqany (tnode_t ***insertpointp)
{
	tnode_t *listnode = parser_buildlistnode ((**insertpointp)->org, NULL);
	tnode_t *seqnode = tnode_createfrom (opi.tag_SEQ, **insertpointp, NULL, listnode);
	tnode_t *savedproc = **insertpointp;

	**insertpointp = seqnode;
	*insertpointp = parser_addtolist (listnode, savedproc);

	return listnode;
}
/*}}}*/


