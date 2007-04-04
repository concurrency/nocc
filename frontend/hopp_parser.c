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
#include "origin.h"
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
	postcheck:	NULL,
	fetrans:	NULL,
	getlangdef:	NULL,
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
	HTAG_DECL = 401,
	HTAG_PROC = 402,
	HTAG_NAME = 403,
	HTAG_FORMAL = 404,
	HTAG_CHAN = 405,
	HTAG_BYTE = 406,
	HTAG_SEQ = 407,
	HTAG_SKIP = 408,
	HTAG_MAIN = 409,
	HTAG_STOP = 410,
	HTAG_INT = 411,
	HTAG_WHILE = 412,
	HTAG_TRUE = 413,
	HTAG_FALSE = 414,
	HTAG_VARS = 415,
	HTAG_VAL = 416,
	HTAG_IS = 417,
} hopp_tag_e;

typedef struct TAG_hopp_tag {
	char *name;
	hopp_tag_e id;
	keyword_t *kw;
} hopp_tag_t;

static hopp_tag_t htagdata[] = {
	{"decl", HTAG_DECL, NULL},
	{"proc", HTAG_PROC, NULL},
	{"name", HTAG_NAME, NULL},
	{"formal", HTAG_FORMAL, NULL},
	{"chan", HTAG_CHAN, NULL},
	{"byte", HTAG_BYTE, NULL},
	{"seq", HTAG_SEQ, NULL},
	{"skip", HTAG_SKIP, NULL},
	{"main", HTAG_MAIN, NULL},
	{"stop", HTAG_STOP, NULL},
	{"int", HTAG_INT, NULL},
	{"while", HTAG_WHILE, NULL},
	{"true", HTAG_TRUE, NULL},
	{"false", HTAG_FALSE, NULL},
	{"vars", HTAG_VARS, NULL},
	{"val", HTAG_VAL, NULL},
	{"is", HTAG_IS, NULL},
	{NULL, HTAG_INVALID, NULL}
};


static symbol_t *sym_lbracket = NULL;
static symbol_t *sym_rbracket = NULL;
static symbol_t *sym_box = NULL;
static symbol_t *sym_lparen = NULL;
static symbol_t *sym_rparen = NULL;
static symbol_t *sym_comma = NULL;
static symbol_t *sym_colon = NULL;

/*}}}*/



/*{{{  static tnode_t *hopp_parse_toplevel (lexfile_t *lf)*/
/*
 *	called to parse a whole chunk of input (tag and argument(s))
 *	does this through recursive drop-down
 *	returns tree on success, NULL on failure
 */
static tnode_t *hopp_parse_toplevel (lexfile_t *lf)
{
	token_t *tok = lexer_nexttoken (lf);
	tnode_t *tree = NULL;

	if (!tok) {
		return NULL;
	} else if (tok->type == END) {
		lexer_freetoken (tok);
		return NULL;
	}

	switch (tok->type) {
		/*{{{  SYMBOL -- probably meta-level stuff*/
	case SYMBOL:
		if (tok->u.sym == sym_lparen) {
			/*{{{  this is a closed chunk or list*/
			lexer_freetoken (tok);

			tree = hopp_parse_toplevel (lf);
			tok = lexer_nexttoken (lf);
			if (!tok || (tok->type == END)) {
				parser_error (lf, "unexpected end");
			} else if ((tok->type == SYMBOL) && (tok->u.sym == sym_rparen)) {
				/* good, got a closed chunk */
			} else {
				tnode_t *tmp;

				/* assume a list .. */
				lexer_pushback (lf, tok);
				tok = NULL;
				tmp = tree;
				tree = parser_newlistnode (lf);
				parser_addtolist (tree, tmp);

				for (;;) {
					tmp = hopp_parse_toplevel (lf);

					if (!tmp) {
						parser_error (lf, "null item while parsing for list");
						break;		/* for() */
					}
					parser_addtolist (tree, tmp);
					tok = lexer_nexttoken (lf);
					if (tok && (tok->type == SYMBOL) && (tok->u.sym == sym_rparen)) {
						/* end of list */
						break;		/* for() */
					}
					lexer_pushback (lf, tok);
					tok = NULL;
				}
			}
			/*}}}*/
		} else if (tok->u.sym == sym_rparen) {
			/*{{{  unexpected*/
			parser_error (lf, "unexpected ')'");
			/*}}}*/
		} else if (tok->u.sym == sym_colon) {
			/*{{{  declaration of something*/
			tnode_t *ibody;

			tree = hopp_parse_toplevel (lf);		/* actual declaration */
			ibody = hopp_parse_toplevel (lf);

			if (!tree) {
				parser_error (lf, "DECL: bad declaration");
				tree = ibody;
			} else {
				if (tree->tag == opi.tag_PROCDECL) {
					tnode_setnthsub (tree, 3, ibody);
				} else if (tree->tag == opi.tag_VARDECL) {
					tnode_setnthsub (tree, 2, ibody);
				}
			}
			/*}}}*/
		}
		break;
		/*}}}*/
		/*{{{  KEYWORD -- probably a directive*/
	case KEYWORD:
		switch ((hopp_tag_e)tok->u.kw->tagval) {
			/*{{{  PROC -- process definition*/
		case HTAG_PROC:
			{
				tnode_t *name, *params, *body;

				tree = tnode_create (opi.tag_PROCDECL, lf, NULL, NULL, NULL, NULL);		/* name, params, process-body, in-scope-body */
				name = hopp_parse_toplevel (lf);
				params = hopp_parse_toplevel (lf);
				body = hopp_parse_toplevel (lf);

				if (!name || !body) {
					parser_error (lf, "PROC: bad name or body");
				}
				tnode_setnthsub (tree, 0, name);
				tnode_setnthsub (tree, 1, params);
				tnode_setnthsub (tree, 2, body);
			}
			break;
			/*}}}*/
			/*{{{  VARS -- variable declaration*/
		case HTAG_VARS:
			{
				tnode_t *name, *type;

				tree = tnode_create (opi.tag_VARDECL, lf, NULL, NULL, NULL);			/* name, type, in-scope-body */
				type = hopp_parse_toplevel (lf);
				name = hopp_parse_toplevel (lf);

				if (!name || !type) {
					parser_error (lf, "VARS: bad name or type");
				}
				tnode_setnthsub (tree, 0, name);
				tnode_setnthsub (tree, 1, type);
			}
			break;
			/*}}}*/
			/*{{{  NAME -- quoted name*/
		case HTAG_NAME:
			tree = tnode_create (opi.tag_NAME, lf, NULL);
			lexer_freetoken (tok);
			tok = lexer_nexttoken (lf);

			if (!tok) {
				parser_error (lf, "NAME: missing name ?");
			} else if (tok->type == KEYWORD) {
				/* assume ok for now -- extract keyword and make it a name */
				tnode_setnthhook (tree, 0, occampi_keywordtoken_to_namehook ((void *)tok));
				tok = NULL;
			} else if (tok->type != NAME) {
				parser_error (lf, "NAME: following token not NAME, was:");
				lexer_dumptoken (stderr, tok);
			} else {
				tnode_setnthhook (tree, 0, occampi_nametoken_to_hook ((void *)tok));
				tok = NULL;
			}
			break;
			/*}}}*/
			/*{{{  FORMAL -- formal parameter*/
		case HTAG_FORMAL:
			{
				tnode_t *name, *type;

				tree = tnode_create (opi.tag_FPARAM, lf, NULL, NULL);					/* name, type */
				type = hopp_parse_toplevel (lf);
				name = hopp_parse_toplevel (lf);

				if (!name || !type) {
					parser_error (lf, "FORMAL: bad name or type");
				}
				tnode_setnthsub (tree, 0, name);
				tnode_setnthsub (tree, 1, type);
			}
			break;
			/*}}}*/
			/*{{{  CHAN -- CHAN type (not channel-type!)*/
		case HTAG_CHAN:
			{
				tnode_t *protocol;

				tree = tnode_create (opi.tag_CHAN, lf, NULL);						/* protocol */
				protocol = hopp_parse_toplevel (lf);

				if (!protocol) {
					parser_error (lf, "CHAN: bad protocol");
				}
				tnode_setnthsub (tree, 0, protocol);
			}
			break;
			/*}}}*/
			/*{{{  BYTE*/
		case HTAG_BYTE:
			tree = tnode_create (opi.tag_BYTE, lf);
			break;
			/*}}}*/
			/*{{{  INT*/
		case HTAG_INT:
			tree = tnode_create (opi.tag_INT, lf);
			break;
			/*}}}*/
			/*{{{  SKIP*/
		case HTAG_SKIP:
			tree = tnode_create (opi.tag_SKIP, lf);
			break;
			/*}}}*/
			/*{{{  STOP*/
		case HTAG_STOP:
			tree = tnode_create (opi.tag_STOP, lf);
			break;
			/*}}}*/
			/*{{{  TRUE*/
		case HTAG_TRUE:
			tree = occampi_makelitbool (lf, 1);
			break;
			/*}}}*/
			/*{{{  FALSE*/
		case HTAG_FALSE:
			tree = occampi_makelitbool (lf, 0);
			break;
			/*}}}*/
			/*{{{  SEQ*/
		case HTAG_SEQ:
			{
				tnode_t *body;

				tree = tnode_create (opi.tag_SEQ, lf, NULL, NULL);				/* par-specials, body */
				body = hopp_parse_toplevel (lf);

				if (!body) {
					parser_error (lf, "SEQ: bad body");
				} else if (!parser_islistnode (body)) {
					body = parser_buildlistnode (lf, body, NULL);
				}
				tnode_setnthsub (tree, 1, body);
			}
			break;
			/*}}}*/
			/*{{{  MAIN -- top-level identifier*/
		case HTAG_MAIN:
			tree = NULL;
			break;
			/*}}}*/
			/*{{{  WHILE*/
		case HTAG_WHILE:
			{
				tnode_t *cond, *body;

				tree = tnode_create (opi.tag_WHILE, lf, NULL, NULL);					/* expr, body */
				cond = hopp_parse_toplevel (lf);
				body = hopp_parse_toplevel (lf);

				if (!cond || !body) {
					parser_error (lf, "WHILE: bad expression or body");
				}
				tnode_setnthsub (tree, 0, cond);
				tnode_setnthsub (tree, 1, body);
			}
			break;
			/*}}}*/
			/*{{{  default -- warning*/
		default:
			parser_warning (lf, "unhandled keyword token:");
			lexer_dumptoken (stderr, tok);
			break;
			/*}}}*/
		}
		break;
		/*}}}*/
		/*{{{  default -- warning and ignore*/
	default:
		parser_warning (lf, "unhandled token:");
		lexer_dumptoken (stderr, tok);
		break;
		/*}}}*/
	}

	if (tok) {
		lexer_freetoken (tok);
	}

	return tree;
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
		keyword_t *kw = keywords_add (htagdata[i].name, (int)htagdata[i].id, LANGTAG_HOPP, INTERNAL_ORIGIN);

		htagdata[i].kw = kw;
	}

	/* symbols */
	sym_lbracket = symbols_lookup ("[", 1, LANGTAG_HOPP);
	sym_rbracket = symbols_lookup ("]", 1, LANGTAG_HOPP);
	sym_box = symbols_lookup ("[]", 2, LANGTAG_HOPP);
	sym_lparen = symbols_lookup ("(", 1, LANGTAG_HOPP);
	sym_rparen = symbols_lookup (")", 1, LANGTAG_HOPP);
	sym_comma = symbols_lookup (",", 1, LANGTAG_HOPP);
	sym_colon = symbols_lookup (":", 1, LANGTAG_HOPP);

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
	int i, r;
	
	if (compopts.verbose) {
		nocc_message ("hopp_parser_parse(): starting parse..");
	}

	tree = hopp_parse_toplevel (lf);
	if (!tree) {
		parser_error (lf, "hopp_parser_parse(): got nothing back at top-level..");
	}

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


