/*
 *	occampi_oper.c -- occam-pi operators
 *	Copyright (C) 2005 Fred Barnes <frmb@kent.ac.uk>
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
#include "feunit.h"
#include "names.h"
#include "scope.h"
#include "prescope.h"
#include "precheck.h"
#include "usagecheck.h"
#include "map.h"
#include "target.h"
#include "transputer.h"
#include "codegen.h"
#include "langops.h"


/*}}}*/


/*{{{  private data*/
static token_t *add_tok = NULL;
static token_t *sub_tok = NULL;
static token_t *div_tok = NULL;
static token_t *rem_tok = NULL;
static token_t *mul_tok = NULL;
static token_t *plus_tok = NULL;
static token_t *minus_tok = NULL;
static token_t *times_tok = NULL;


/*}}}*/


/*{{{  static void occampi_reduce_dop (dfastate_t *dfast, parsepriv_t *pp, void *rarg)*/
/*
 *	reduces a dyadic operator, expects 2 nodes on the node-stack,
 *	and the relevant operator token on the token-stack
 */
static void occampi_reduce_dop (dfastate_t *dfast, parsepriv_t *pp, void *rarg)
{
	token_t *tok = parser_gettok (pp);
	tnode_t *lhs, *rhs;
	ntdef_t *tag;

	if (!tok) {
		parser_error (pp->lf, "occampi_reduce_dop(): no token ?");
		return;
	}
	rhs = dfa_popnode (dfast);
	lhs = dfa_popnode (dfast);
	if (!rhs || !lhs) {
		parser_error (pp->lf, "occampi_reduce_dop(): lhs=0x%8.8x, rhs=0x%8.8x", (unsigned int)lhs, (unsigned int)rhs);
		return;
	}
	if (tok == add_tok) {
		tag = opi.tag_ADD;
	} else if (tok == sub_tok) {
		tag = opi.tag_SUB;
	} else {
		parser_error (pp->lf, "occampi_reduce_dop(): unhandled token [%s]", lexer_stokenstr (tok));
		return;
	}
	*(dfast->ptr) = tnode_create (tag, pp->lf, lhs, rhs, NULL);

	return;
}
/*}}}*/


/*{{{  static int occampi_oper_init_nodes (void)*/
/*
 *	sets up nodes for occam-pi operators (monadic, dyadic)
 *	returns 0 on success, non-zero on error
 */
static int occampi_oper_init_nodes (void)
{
	tndef_t *tnd;
	compops_t *cops;
	int i;

	/*{{{  occampi:dopnode -- MUL, DIV, ADD, SUB; PLUS, MINUS, TIMES*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:dopnode", &i, 3, 0, 0, TNF_NONE);
	cops = tnode_newcompops ();
	tnd->ops = cops;
	i = -1;
	opi.tag_MUL = tnode_newnodetag ("MUL", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_DIV = tnode_newnodetag ("DIV", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_REM = tnode_newnodetag ("REM", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_ADD = tnode_newnodetag ("ADD", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_SUB = tnode_newnodetag ("SUB", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_PLUS = tnode_newnodetag ("PLUS", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_MINUS = tnode_newnodetag ("MINUS", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_TIMES = tnode_newnodetag ("TIMES", &i, tnd, NTF_NONE);
	/*}}}*/

	/*{{{  setup local tokens*/
	add_tok = lexer_newtoken (SYMBOL, "+");
	sub_tok = lexer_newtoken (SYMBOL, "-");
	mul_tok = lexer_newtoken (SYMBOL, "*");
	div_tok = lexer_newtoken (SYMBOL, "/");
	rem_tok = lexer_newtoken (SYMBOL, "\\");
	plus_tok = lexer_newtoken (KEYWORD, "PLUS");
	minus_tok = lexer_newtoken (KEYWORD, "MINUS");
	times_tok = lexer_newtoken (KEYWORD, "TIMES");
	/*}}}*/

	return 0;
}
/*}}}*/
/*{{{  static int occampi_oper_reg_reducers (void)*/
/*
 *	registers reductions for occam-pi operators
 *	returns 0 on success, non-zero on error
 */
static int occampi_oper_reg_reducers (void)
{
	parser_register_reduce ("Roccampi:dopreduce", occampi_reduce_dop, NULL);

	return 0;
}
/*}}}*/
/*{{{  static dfattbl_t **occampi_oper_init_dfatrans (int *ntrans)*/
/*
 *	initialises and returns DFA transition tables for occam-pi operators
 */
static dfattbl_t **occampi_oper_init_dfatrans (int *ntrans)
{
	DYNARRAY (dfattbl_t *, transtbl);

	dynarray_init (transtbl);

	dynarray_add (transtbl, dfa_transtotbl ("occampi:restofexpr ::= [ 0 +@@+ 1 ] [ 0 +@@- 1 ] [ 0 +@@* 1 ] [ 0 +@@/ 1 ] [ 1 occampi:expr 2 ] [ 2 {Roccampi:dopreduce} -* ]"));

	*ntrans = DA_CUR (transtbl);
	return DA_PTR (transtbl);
}
/*}}}*/
/*{{{  static int occampi_oper_post_setup (void)*/
/*
 *	does post-setup for occam-pi operator nodes
 *	returns 0 on success, non-zero on error
 */
static int occampi_oper_post_setup (void)
{
	return 0;
}
/*}}}*/


/*{{{  occampi_oper_feunit (feunit_t)*/
feunit_t occampi_oper_feunit = {
	init_nodes: occampi_oper_init_nodes,
	reg_reducers: occampi_oper_reg_reducers,
	init_dfatrans: occampi_oper_init_dfatrans,
	post_setup: occampi_oper_post_setup
};
/*}}}*/



