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


/*{{{  static int hopp_parser_init (lexfile_t *lf)*/
/*
 *	initialises the hopp parser
 *	returns 0 on success, non-zero on error
 */
static int hopp_parser_init (lexfile_t *lf)
{
	if (compopts.verbose) {
		nocc_message ("initialising haskell occam-pi parser parser..");
	}
	if (occampi_parser.init && occampi_parser.init (lf)) {
		nocc_message ("hopp: failed to initialise occam-pi parser");
		return -1;
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
	/* FIXME! */
	return NULL;
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


