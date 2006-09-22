/*
 *	hopp_parser.c -- haskell occam-pi parser parser for nocc
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
#include "symbols.h"
#include "keywords.h"
#include "lexer.h"
#include "lexpriv.h"
#include "tnode.h"
#include "parser.h"
#include "dfa.h"
#include "parsepriv.h"
#include "occampi.h"
#include "library.h"
#include "feunit.h"
#include "names.h"
#include "scope.h"
#include "prescope.h"
#include "typecheck.h"
#include "extn.h"
#include "hopp.h"

/*}}}*/

/*{{{  forward decls*/
static int hopp_parser_init (lexfile_t *lf);
static void hopp_parser_shutdown (lexfile_t *lf);
static tnode_t *hopp_parser_parse (lexfile_t *lf);
static tnode_t *hopp_parser_descparse (lexfile_t *lf);
static int hopp_parser_prescope (tnode_t **tptr, prescope_t *ps);
static int hopp_parser_scope (tnode_t **tptr, scope_t *ss);
static int hopp_parser_typecheck (tnode_t *tptr, typecheck_t *tc);
static tnode_t *hopp_parser_maketemp (tnode_t ***insertpointp, tnode_t *type);
static tnode_t *hopp_parser_makeseqassign (tnode_t ***insertpointp, tnode_t *lhs, tnode_t *rhs, tnode_t *type);


/*}}}*/
/*{{{  global vars*/

langparser_t hopp_parser = {
	langname:	"occam-pi",
	init:		hopp_parser_init,
	shutdown:	hopp_parser_shutdown,
	parse:		hopp_parser_parse,
	descparse:	hopp_parser_descparse,
	prescope:	hopp_parser_prescope,
	scope:		hopp_parser_scope,
	typecheck:	hopp_parser_typecheck,
	maketemp:	hopp_parser_maketemp,
	makeseqassign:	hopp_parser_makeseqassign,
	tagstruct_hook:	NULL,
	lexer:		NULL
};


/*}}}*/
/*{{{  private types/data*/

/* FIXME: this should probably be generated from Adam's "Tree.hs" which defines the various nodes generated */


typedef enum ENUM_hopp_tag {
	HTAG_LIST = 399,
	HTAG_INVALID = 400,
	HTAG_INTDECLSET = 401,
	HTAG_OCPROC = 402,
	HTAG_OCNAME = 403,
	HTAG_OCFORMAL = 404,
	HTAG_OCCHANOF = 405,
	HTAG_OCBYTE = 406,
	HTAG_OCSEQ = 407,
	HTAG_OCSKIP = 408,
	HTAG_OCMAINPROCESS = 409
} hopp_tag_e;

typedef struct TAG_hopp_tag {
	char *name;
	hopp_tag_e id;
	keyword_t *kw;
} hopp_tag_t;

static hopp_tag_t htagdata[] = {
	{"IntDeclSet", HTAG_INTDECLSET, NULL},
	{"OcProc", HTAG_OCPROC, NULL},
	{"OcName", HTAG_OCNAME, NULL},
	{"OcFormal", HTAG_OCFORMAL, NULL},
	{"OcChanOf", HTAG_OCCHANOF, NULL},
	{"OcByte", HTAG_OCBYTE, NULL},
	{"OcSeq", HTAG_OCSEQ, NULL},
	{"OcSkip", HTAG_OCSKIP, NULL},
	{"OcMainProcess", HTAG_OCMAINPROCESS, NULL},
	{NULL, HTAG_INVALID, NULL}
};


typedef struct TAG_hopp_pstack {
	hopp_tag_e id;
	token_t *basetoken;
	int subitems;
	tnode_t *rnode;
	int inlist;
	int bracketed;
} hopp_pstack_t;


static symbol_t *sym_lbracket = NULL;
static symbol_t *sym_rbracket = NULL;
static symbol_t *sym_box = NULL;
static symbol_t *sym_lparen = NULL;
static symbol_t *sym_rparen = NULL;

/*}}}*/


/*{{{  static hopp_pstack_t *hopp_newpstack (void)*/
/*
 *	creates a new hopp_pstack_t structure
 */
static hopp_pstack_t *hopp_newpstack (void)
{
	hopp_pstack_t *hps = (hopp_pstack_t *)smalloc (sizeof (hopp_pstack_t));

	hps->id = HTAG_INVALID;
	hps->basetoken = NULL;
	hps->subitems = 0;
	hps->rnode = NULL;
	hps->inlist = 0;
	hps->bracketed = 0;

	return hps;
}
/*}}}*/
/*{{{  static void hopp_freepstack (hopp_pstack_t *hps)*/
/*
 *	frees a hopp_pstack_t structure
 */
static void hopp_freepstack (hopp_pstack_t *hps)
{
	if (!hps) {
		nocc_warning ("hopp_freepstack(): NULL pointer!");
		return;
	}
	if (hps->basetoken) {
		lexer_freetoken (hps->basetoken);
		hps->basetoken = NULL;
	}
	if (hps->rnode) {
		tnode_free (hps->rnode);
		hps->rnode = NULL;
	}
	sfree (hps);
	return;
}
/*}}}*/


/*{{{  static int hopp_pstackstart (hopp_pstack_t *hps)*/
/*
 *	called when a keyword token is encountered
 *	returns 1 if the stack node should be added to the parser stack, 0 otherwise
 */
static int hopp_pstackstart (hopp_pstack_t *hps)
{
	switch ((hopp_tag_e)hps->basetoken->u.kw->tagval) {
	case HTAG_INVALID:
		return 0;
	case HTAG_INTDECLSET:
		hps->subitems = 1;
		return 1;
	case HTAG_OCPROC:
		hps->subitems = 3;
		return 1;
	}
	return 0;
}
/*}}}*/


/*{{{  static int hopp_parser_init (lexfile_t *lf)*/
/*
 *	initialises the hopp parser
 *	returns 0 on success, non-zero on error
 */
static int hopp_parser_init (lexfile_t *lf)
{
	int i;

	if (compopts.verbose) {
		nocc_message ("initialising haskell occam-pi parser parser..");
	}
	if (occampi_parser.init && occampi_parser.init (lf)) {
		nocc_message ("hopp: failed to initialise occam-pi parser");
		return -1;
	}

	/* keywords */
	for (i=0; htagdata[i].name && (htagdata[i].id != HTAG_INVALID); i++) {
		keyword_t *kw = keywords_add (htagdata[i].name, (int)htagdata[i].id, (void *)&hopp_parser);

		htagdata[i].kw = kw;
	}

	/* symbols */
	sym_lbracket = symbols_lookup ("[", 1);
	sym_rbracket = symbols_lookup ("]", 1);
	sym_box = symbols_lookup ("[]", 2);
	sym_lparen = symbols_lookup ("(", 1);
	sym_rparen = symbols_lookup (")", 1);

	return 0;
}
/*}}}*/
/*{{{  static void hopp_parser_shutdown (lexfile_t *lf)*/
/*
 *	shuts-down the hopp parser
 */
static void hopp_parser_shutdown (lexfile_t *lf)
{
	if (occampi_parser.shutdown) {
		occampi_parser.shutdown (lf);
	}
	return;
}
/*}}}*/
/*{{{  static tnode_t *hopp_parser_parse (lexfile_t *lf)*/
/*
 *	called to parse a file
 *	returns tree on success, NULL on failure
 */
static tnode_t *hopp_parser_parse (lexfile_t *lf)
{
	tnode_t *tree = NULL;
	DYNARRAY (hopp_pstack_t *, pstk);
	int i, r;
	
	if (compopts.verbose) {
		nocc_message ("hopp_parser_parse(): starting parse..");
	}

	dynarray_init (pstk);

	for (;;) {
		hopp_pstack_t *thispstk = NULL;
		token_t *tok = NULL;
		int dofree = 0;

		tok = lexer_nexttoken (lf);
		if (!tok) {
			break;
		}
#if 1
		lexer_dumptoken (stderr, tok);
#endif

		switch (tok->type) {
			/*{{{  SYMBOL -- meta-data for the haskell tree probably*/
		case SYMBOL:
			if (tok->u.sym == sym_lbracket) {
				/* starting list */
				thispstk = hopp_newpstack ();
				thispstk->id = HTAG_LIST;
				thispstk->rnode = parser_newlistnode (lf);

				dynarray_add (pstk, thispstk);
			} else if (tok->u.sym == sym_lparen) {
				/* starting list */
				thispstk = hopp_newpstack ();
				thispstk->id = HTAG_INVALID;
				thispstk->bracketed = 1;

				dynarray_add (pstk, thispstk);
			}
			break;
			/*}}}*/
			/*{{{  KEYWORD -- probably a tree token*/
		case KEYWORD:
			if (DA_CUR (pstk) >= 1) {
				thispstk = DA_NTHITEM (pstk, DA_CUR (pstk) - 1);
				if (thispstk->id == HTAG_INVALID) {
					/* use this */
					thispstk->basetoken = tok;
					tok = NULL;
				} else {
					/* don't use this one */
					thispstk = NULL;
				}
			}
			if (!thispstk) {
				thispstk = hopp_newpstack ();
				thispstk->basetoken = tok;
				tok = NULL;
				dofree = 1;
			}

			r = hopp_pstackstart (thispstk);
			if (r == 1) {
				/* add to stack */
				dynarray_add (pstk, thispstk);
			} else if (!r && dofree) {
				hopp_freepstack (thispstk);
			}

			break;
			/*}}}*/
			/*{{{  default -- ignore*/
		default:
			break;
			/*}}}*/
		}

		if (tok && (tok->type == END)) {
			lexer_freetoken (tok);
			break;
		}
		if (tok) {
			lexer_freetoken (tok);
		}
	}

	if (DA_CUR (pstk) == 1) {
		hopp_pstack_t *tos = DA_NTHITEM (pstk, 0);

		if (!tos->rnode) {
			nocc_error ("hopp_parser_parse(): nothing at top-level!");
		}
		tree = tos->rnode;
		tos->rnode = NULL;
	} else if (DA_CUR (pstk) > 1) {
		nocc_error ("hopp_parser_parse(): failed to parse! (%d things left on pstack)", DA_CUR (pstk));
	} else if (!DA_CUR (pstk)) {
		nocc_error ("hopp_parser_parse(): nothing left after parse!");
	}

	/* trash partial stack */
	for (i=0; i<DA_CUR (pstk); i++) {
		hopp_pstack_t *si = DA_NTHITEM (pstk, i);

		hopp_freepstack (si);
	}
	dynarray_trash (pstk);
		
	return tree;
}
/*}}}*/
/*{{{  static tnode_t *hopp_parser_descparse (lexfile_t *lf)*/
/*
 *	called to parse a descriptor line
 *	returns a tree on success (representing the top-level declaration), NULL on failure
 */
static tnode_t *hopp_parser_descparse (lexfile_t *lf)
{
	tnode_t *t = NULL;

	if (occampi_parser.descparse) {
		t = occampi_parser.descparse (lf);
	}
	return t;
}
/*}}}*/
/*{{{  static int hopp_parser_prescope (tnode_t **tptr, prescope_t *ps)*/
/*
 *	does pre-scoping on the parse tree (or chunk)
 *	returns 0 on success, non-zero on failure
 */
static int hopp_parser_prescope (tnode_t **tptr, prescope_t *ps)
{
	int r = 0;

	if (occampi_parser.prescope) {
		r = occampi_parser.prescope (tptr, ps);
	}
	return r;
}
/*}}}*/
/*{{{  static int hopp_parser_scope (tnode_t **tptr, scope_t *ss)*/
/*
 *	called to scope declarations in the parse tree
 *	returns 0 on success, non-zero on failure
 */
static int hopp_parser_scope (tnode_t **tptr, scope_t *ss)
{
	int r = 0;

	if (occampi_parser.scope) {
		r = occampi_parser.scope (tptr, ss);
	}
	return r;
}
/*}}}*/
/*{{{  static int hopp_parser_typecheck (tnode_t *tptr, typecheck_t *tc)*/
/*
 *	called to type-check a tree
 *	returns 0 on success, non-zero on failure
 */
static int hopp_parser_typecheck (tnode_t *tptr, typecheck_t *tc)
{
	int r = 0;

	if (occampi_parser.typecheck) {
		r = occampi_parser.typecheck (tptr, tc);
	}
	return r;
}
/*}}}*/
/*{{{  static tnode_t *hopp_parser_maketemp (tnode_t ***insertpointp, tnode_t *type)*/
/*
 *	called to create a temporary
 *	returns NDECL part (reference) on success, NULL on failure
 */
static tnode_t *hopp_parser_maketemp (tnode_t ***insertpointp, tnode_t *type)
{
	tnode_t *t = NULL;

	if (occampi_parser.maketemp) {
		t = occampi_parser.maketemp (insertpointp, type);
	}
	return t;
}
/*}}}*/
/*{{{  static tnode_t *hopp_parser_makeseqassign (tnode_t ***insertpointp, tnode_t *lhs, tnode_t *rhs, tnode_t *type)*/
/*
 *	called to create a sequential assignment
 *	returns the ASSIGN part, NULL on failure
 */
static tnode_t *hopp_parser_makeseqassign (tnode_t ***insertpointp, tnode_t *lhs, tnode_t *rhs, tnode_t *type)
{
	tnode_t *t = NULL;

	if (occampi_parser.makeseqassign) {
		t = occampi_parser.makeseqassign (insertpointp, lhs, rhs, type);
	}
	return t;
}
/*}}}*/


