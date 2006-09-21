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
} hopp_tag_t;

static hopp_tag_t htagdata[] = {
	{"IntDeclSet", HTAG_INTDECLSET},
	{"OcProc", HTAG_OCPROC},
	{"OcName", HTAG_OCNAME},
	{"OcFormal", HTAG_OCFORMAL},
	{"OcChanOf", HTAG_OCCHANOF},
	{"OcByte", HTAG_OCBYTE},
	{"OcSeq", HTAG_OCSEQ},
	{"OcSkip", HTAG_OCSKIP},
	{"OcMainProcess", HTAG_OCMAINPROCESS},
	{NULL, HTAG_INVALID}
};


typedef struct TAG_hopp_pstack {
	hopp_tag_e id;
	token_t *basetoken;
	int subitems;
	tnode_t *rnode;
} hopp_pstack_t;


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

	for (i=0; htagdata[i].name && (htagdata[i].id != HTAG_INVALID); i++) {
		keywords_add (htagdata[i].name, (int)htagdata[i].id, (void *)&hopp_parser);
	}

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
	token_t *tok;
	tnode_t *tree = NULL;
	DYNARRAY (hopp_pstack_t *, pstk);
	int i;
	
	if (compopts.verbose) {
		nocc_message ("hopp_parser_parse(): starting parse..");
	}

	dynarray_init (pstk);

	for (;;) {
		tok = lexer_nexttoken (lf);

#if 1
		lexer_dumptoken (stderr, tok);
#endif

		if (!tok) {
			break;
		} else if (tok->type == END) {
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
		nocc_error ("hopp_parser_parse(): failed to parse!");
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


