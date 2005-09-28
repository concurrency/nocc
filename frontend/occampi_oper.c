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
#include "typecheck.h"
#include "usagecheck.h"
#include "map.h"
#include "target.h"
#include "transputer.h"
#include "codegen.h"
#include "langops.h"


/*}}}*/


/*{{{  private types*/
typedef struct TAG_dopmap {
	tokentype_t ttype;
	const char *lookup;
	token_t *tok;
	ntdef_t **tagp;
	transinstr_e instr;
} dopmap_t;

typedef struct TAG_mopmap {
	tokentype_t ttype;
	const char *lookup;
	token_t *tok;
	ntdef_t **tagp;
	transinstr_e instr;
} mopmap_t;


/*}}}*/
/*{{{  private data*/
static dopmap_t dopmap[] = {
	{SYMBOL, "+", NULL, &(opi.tag_ADD), I_ADD},
	{SYMBOL, "-", NULL, &(opi.tag_SUB), I_SUB},
	{SYMBOL, "*", NULL, &(opi.tag_MUL), I_MUL},
	{SYMBOL, "/", NULL, &(opi.tag_DIV), I_DIV},
	{SYMBOL, "\\", NULL, &(opi.tag_REM), I_REM},
	{KEYWORD, "PLUS", NULL, &(opi.tag_PLUS), I_SUM},
	{KEYWORD, "MINUS", NULL, &(opi.tag_MINUS), I_DIFF},
	{KEYWORD, "TIMES", NULL, &(opi.tag_TIMES), I_PROD},
	{NOTOKEN, NULL, NULL, NULL}
};

static mopmap_t mopmap[] = {
	{SYMBOL, "-", NULL, &(opi.tag_UMINUS), I_NEG},
	{SYMBOL, "~", NULL, &(opi.tag_BITNOT), I_NOT},
	{NOTOKEN, NULL, NULL, NULL}
};


/*}}}*/


/*{{{  static tnode_t *occampi_gettype_dop (tnode_t *node, tnode_t *defaulttype)*/
/*
 *	returns the type associated with a DOPNODE, also sets the type in the node
 */
static tnode_t *occampi_gettype_dop (tnode_t *node, tnode_t *defaulttype)
{
	tnode_t *lefttype, *righttype;

	lefttype = typecheck_gettype (tnode_nthsubof (node, 0), defaulttype);
	righttype = typecheck_gettype (tnode_nthsubof (node, 1), defaulttype);

	if (lefttype == defaulttype) {
		tnode_setnthsub (node, 2, righttype);
	} else if (righttype == defaulttype) {
		tnode_setnthsub (node, 2, lefttype);
	} else {
		tnode_setnthsub (node, 2, defaulttype);
	}
	/* FIXME! -- needs more.. */

	return defaulttype;
}
/*}}}*/
/*{{{  static int occampi_premap_dop (tnode_t **node, map_t *map)*/
/*
 *	maps out a DOPNODE, turning into a back-end RESULT
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_premap_dop (tnode_t **node, map_t *map)
{
	/* pre-map left and right */
	map_subpremap (tnode_nthsubaddr (*node, 0), map);
	map_subpremap (tnode_nthsubaddr (*node, 1), map);

	*node = map->target->newresult (*node, map);

	return 0;
}
/*}}}*/
/*{{{  static int occampi_namemap_dop (tnode_t **node, map_t *map)*/
/*
 *	name-maps a DOPNODE, adding child nodes to any enclosing result
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_namemap_dop (tnode_t **node, map_t *map)
{
	/* name-map left and right */
	map_submapnames (tnode_nthsubaddr (*node, 0), map);
	map_submapnames (tnode_nthsubaddr (*node, 1), map);

	/* set in result */
	map_addtoresult (tnode_nthsubaddr (*node, 0), map);
	map_addtoresult (tnode_nthsubaddr (*node, 1), map);

	return 0;
}
/*}}}*/
/*{{{  static int occampi_codegen_dop (tnode_t *node, codegen_t *cgen)*/
/*
 *	called to do code-generation for a DOPNODE -- operands are already on the stack
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_codegen_dop (tnode_t *node, codegen_t *cgen)
{
	int i;

	for (i=0; dopmap[i].lookup; i++) {
		if (node->tag == *(dopmap[i].tagp)) {
			codegen_callops (cgen, tsecondary, dopmap[i].instr);
			return 0;
		}
	}

	codegen_error (cgen, "occampi_codgen_dop(): don\'t know how to generate code for [%s] [%s]", node->tag->ndef->name, node->tag->name);

	return 0;
}
/*}}}*/


/*{{{  static tnode_t *occampi_gettype_mop (tnode_t *node, tnode_t *defaulttype)*/
/*
 *	gets type of a MOPNODE
 */
static tnode_t *occampi_gettype_mop (tnode_t *node, tnode_t *defaulttype)
{
	tnode_t *optype;

	optype = typecheck_gettype (tnode_nthsubof (node, 0), defaulttype);
	tnode_setnthsub (node, 1, optype);
	/* FIXME! -- needs more.. */

	return optype;
}
/*}}}*/
/*{{{  static int occampi_premap_mop (tnode_t **node, map_t *map)*/
/*
 *	does pre-mapping for a MOPNODE -- inserts back-end RESULT nodes
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_premap_mop (tnode_t **node, map_t *map)
{
	/* pre-map operand */
	map_subpremap (tnode_nthsubaddr (*node, 0), map);

	*node = map->target->newresult (*node, map);

	return 0;
}
/*}}}*/
/*{{{  static int occampi_namemap_mop (tnode_t **node, map_t *map)*/
/*
 *	does name-mapping for a MOPNODE
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_namemap_mop (tnode_t **node, map_t *map)
{
	/* name-map operand */
	map_submapnames (tnode_nthsubaddr (*node, 0), map);

	/* set in result */
	map_addtoresult (tnode_nthsubaddr (*node, 0), map);

	return 0;
}
/*}}}*/
/*{{{  static int occampi_codegen_mop (tnode_t *node, codegen_t *cgen)*/
/*
 *	called to do code-generation for a MOPNODE -- operand already on the stack
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_codegen_mop (tnode_t *node, codegen_t *cgen)
{
	int i;

	for (i=0; mopmap[i].lookup; i++) {
		if (node->tag == *(mopmap[i].tagp)) {
			codegen_callops (cgen, tsecondary, mopmap[i].instr);
			return 0;
		}
	}

	codegen_error (cgen, "occampi_codgen_mop(): don\'t know how to generate code for [%s] [%s]", node->tag->ndef->name, node->tag->name);

	return 0;
}
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
	ntdef_t *tag = NULL;
	int i;

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
	for (i=0; dopmap[i].lookup; i++) {
		if (lexer_tokmatch (dopmap[i].tok, tok)) {
			tag = *(dopmap[i].tagp);
			break;
		}
	}
	if (!tag) {
		parser_error (pp->lf, "occampi_reduce_dop(): unhandled token [%s]", lexer_stokenstr (tok));
		return;
	}
	*(dfast->ptr) = tnode_create (tag, pp->lf, lhs, rhs, NULL);

	return;
}
/*}}}*/
/*{{{  static void occampi_reduce_mop (dfastate_t *dfast, parsepriv_t *pp, void *rarg)*/
/*
 *	reduces a monadic operator, expects operand on the node-stack, operator on
 *	the token-stack.
 */
static void occampi_reduce_mop (dfastate_t *dfast, parsepriv_t *pp, void *rarg)
{
	token_t *tok = parser_gettok (pp);
	tnode_t *operand;
	ntdef_t *tag = NULL;
	int i;

	if (!tok) {
		parser_error (pp->lf, "occampi_reduce_mop(): no token ?");
		return;
	}
	operand = dfa_popnode (dfast);
	if (!operand) {
		parser_error (pp->lf, "occampi_reduce_mop(): operand=0x%8.8x", (unsigned int)operand);
		return;
	}
	for (i=0; mopmap[i].lookup; i++) {
		if (lexer_tokmatch (mopmap[i].tok, tok)) {
			tag = *(mopmap[i].tagp);
			break;
		}
	}
	if (!tag) {
		parser_error (pp->lf, "occampi_reduce_mop(): unhandled token [%s]", lexer_stokenstr (tok));
		return;
	}
	*(dfast->ptr) = tnode_create (tag, pp->lf, operand, NULL);

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

	/*{{{  occampi:dopnode -- MUL, DIV, ADD, SUB, REM; PLUS, MINUS, TIMES*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:dopnode", &i, 3, 0, 0, TNF_NONE);
	cops = tnode_newcompops ();
	cops->gettype = occampi_gettype_dop;
	cops->premap = occampi_premap_dop;
	cops->namemap = occampi_namemap_dop;
	cops->codegen = occampi_codegen_dop;
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
	/*{{{  occampi:mopnode -- UMINUS, BITNOT*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:mopnode", &i, 2, 0, 0, TNF_NONE);
	cops = tnode_newcompops ();
	cops->gettype = occampi_gettype_mop;
	cops->premap = occampi_premap_mop;
	cops->namemap = occampi_namemap_mop;
	cops->codegen = occampi_codegen_mop;
	tnd->ops = cops;
	i = -1;
	opi.tag_UMINUS = tnode_newnodetag ("UMINUS", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_BITNOT = tnode_newnodetag ("BITNOT", &i, tnd, NTF_NONE);
	/*}}}*/

	/*{{{  setup local tokens*/
	for (i=0; dopmap[i].lookup; i++) {
		dopmap[i].tok = lexer_newtoken (dopmap[i].ttype, dopmap[i].lookup);
	}
	for (i=0; mopmap[i].lookup; i++) {
		mopmap[i].tok = lexer_newtoken (mopmap[i].ttype, mopmap[i].lookup);
	}

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
	parser_register_reduce ("Roccampi:mopreduce", occampi_reduce_mop, NULL);

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

	dynarray_add (transtbl, dfa_transtotbl ("occampi:restofexpr +:= [ 0 +@@+ 1 ] [ 0 +@@- 1 ] [ 0 +@@* 1 ] [ 0 +@@/ 1 ] [ 0 +@@\\ 1 ] [ 0 +@PLUS 1 ] [ 0 +@MINUS 1 ] [ 0 +@TIMES 1 ] " \
				"[ 1 occampi:expr 2 ] [ 2 {Roccampi:dopreduce} -* ]"));
	dynarray_add (transtbl, dfa_transtotbl ("occampi:expr +:= [ 0 +@@- 1 ] [ 0 +@@~ 1 ] [ 1 occampi:expr 2 ] [ 2 {Roccampi:mopreduce} -* ]"));

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


