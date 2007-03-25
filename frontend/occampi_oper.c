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
#include "fcnlib.h"
#include "dfa.h"
#include "parsepriv.h"
#include "occampi.h"
#include "feunit.h"
#include "names.h"
#include "scope.h"
#include "prescope.h"
#include "precheck.h"
#include "typecheck.h"
#include "constprop.h"
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

typedef struct TAG_relmap {
	tokentype_t ttype;
	const char *lookup;
	token_t *tok;
	ntdef_t **tagp;
	void (*cgfunc)(codegen_t *, int);
	int cgarg;
} relmap_t;


/*}}}*/
/*{{{  forward decls*/
static void occampi_oper_genrelop (codegen_t *cgen, int arg);
static void occampi_oper_geninvrelop (codegen_t *cgen, int arg);


/*}}}*/
/*{{{  private data*/
static dopmap_t dopmap[] = {
	{SYMBOL, "+", NULL, &(opi.tag_ADD), I_ADD},
	{SYMBOL, "-", NULL, &(opi.tag_SUB), I_SUB},
	{SYMBOL, "*", NULL, &(opi.tag_MUL), I_MUL},
	{SYMBOL, "/\\", NULL, &(opi.tag_BITAND), I_AND},
	{SYMBOL, "><", NULL, &(opi.tag_BITXOR), I_XOR},
	{SYMBOL, "/", NULL, &(opi.tag_DIV), I_DIV},
	{SYMBOL, "<<", NULL, &(opi.tag_LSHIFT), I_SHL},
	{SYMBOL, ">>", NULL, &(opi.tag_RSHIFT), I_SHR},
	{SYMBOL, "\\/", NULL, &(opi.tag_BITOR), I_OR},
	{SYMBOL, "\\", NULL, &(opi.tag_REM), I_REM},
	{KEYWORD, "PLUS", NULL, &(opi.tag_PLUS), I_SUM},
	{KEYWORD, "MINUS", NULL, &(opi.tag_MINUS), I_DIFF},
	{KEYWORD, "TIMES", NULL, &(opi.tag_TIMES), I_PROD},
	{NOTOKEN, NULL, NULL, NULL, I_INVALID}
};

static relmap_t relmap[] = {
	{SYMBOL, "=", NULL, &(opi.tag_RELEQ), occampi_oper_genrelop, I_EQ},
	{SYMBOL, "<>", NULL, &(opi.tag_RELNEQ), occampi_oper_geninvrelop, I_EQ},
	{SYMBOL, "<", NULL, &(opi.tag_RELLT), occampi_oper_genrelop, I_LT},
	{SYMBOL, ">=", NULL, &(opi.tag_RELGEQ), occampi_oper_geninvrelop, I_LT},
	{SYMBOL, ">", NULL, &(opi.tag_RELGT), occampi_oper_genrelop, I_GT},
	{SYMBOL, "<=", NULL, &(opi.tag_RELLEQ), occampi_oper_geninvrelop, I_GT},
	{NOTOKEN, NULL, NULL, NULL, NULL, I_INVALID}
};

static mopmap_t mopmap[] = {
	{SYMBOL, "-", NULL, &(opi.tag_UMINUS), I_NEG},
	{SYMBOL, "~", NULL, &(opi.tag_BITNOT), I_NOT},
	{NOTOKEN, NULL, NULL, NULL, I_INVALID}
};


/*}}}*/


/*{{{  static int occampi_typecheck_dop (compops_t *cops, tnode_t *tptr, typecheck_t *tc)*/
/*
 *	does type-checking on a dyadic operator
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_typecheck_dop (compops_t *cops, tnode_t *tptr, typecheck_t *tc)
{
	/* FIXME! */
	return 1;
}
/*}}}*/
/*{{{  static int occampi_constprop_dop (compops_t *cops, tnode_t **tptr)*/
/*
 *	does constant propagation for a DOPNODE
 *	returns 0 to stop walk, 1 to continue (post-walk)
 */
static int occampi_constprop_dop (compops_t *cops, tnode_t **tptr)
{
	tnode_t *left, *right;

	left = tnode_nthsubof (*tptr, 0);
	right = tnode_nthsubof (*tptr, 1);

	if (constprop_isconst (left) && constprop_isconst (right) && constprop_sametype (left, right)) {
		tnode_t *newconst = *tptr;

		/* turn this node into a constant */
		switch (constprop_consttype (left)) {
		case CONST_INVALID:
			constprop_error (*tptr, "occampi_constprop_dop(): CONST_INVALID!");
			break;
		case CONST_BYTE:
			{
				unsigned char b1, b2;
				unsigned int bres = 0;

				langops_constvalof (left, &b1);
				langops_constvalof (right, &b2);

				if ((*tptr)->tag == opi.tag_MUL) {
					bres = (unsigned int)b1 * (unsigned int)b2;
				} else if ((*tptr)->tag == opi.tag_DIV) {
					if (!b2) {
						constprop_error (*tptr, "division by zero");
						bres = 0;
					} else {
						bres = (unsigned int)b1 / (unsigned int)b2;
					}
				} else if ((*tptr)->tag == opi.tag_ADD) {
					bres = (unsigned int)b1 + (unsigned int)b2;
				} else if ((*tptr)->tag == opi.tag_SUB) {
					bres = (unsigned int)b1 - (unsigned int)b2;
				} else if ((*tptr)->tag == opi.tag_REM) {
					if (!b2) {
						constprop_error (*tptr, "remainder of division by zero");
						bres = 0;
					} else {
						bres = (unsigned int)b1 % (unsigned int)b2;
					}
				} else if ((*tptr)->tag == opi.tag_PLUS) {
					bres = (unsigned int)b1 + (unsigned int)b2;
					bres &= 0xff;
				} else if ((*tptr)->tag == opi.tag_MINUS) {
					bres = (unsigned int)b1 - (unsigned int)b2;
					bres &= 0xff;
				} else if ((*tptr)->tag == opi.tag_TIMES) {
					bres = (unsigned int)b1 * (unsigned int)b2;
					bres &= 0xff;
				}
				if (bres > 0xff) {
					constprop_error (*tptr, "BYTE constant overflow");
					bres &= 0xff;
				}
				b1 = (unsigned char)bres;
				newconst = constprop_newconst (CONST_BYTE, *tptr, tnode_nthsubof (*tptr, 2), b1);
			}
			break;
		case CONST_INT:
			{
				int i1, i2;
				long long ires = 0LL;

				langops_constvalof (left, &i1);
				langops_constvalof (right, &i2);

				if ((*tptr)->tag == opi.tag_MUL) {
					ires = (long long)i1 * (long long)i2;
				} else if ((*tptr)->tag == opi.tag_DIV) {
					if (!i2) {
						constprop_error (*tptr, "division by zero");
						ires = 0;
					} else {
						ires = (long long)i1 / (long long)i2;
					}
				} else if ((*tptr)->tag == opi.tag_ADD) {
					ires = (long long)i1 + (long long)i2;
				} else if ((*tptr)->tag == opi.tag_SUB) {
					ires = (long long)i1 - (long long)i2;
				} else if ((*tptr)->tag == opi.tag_REM) {
					if (!i2) {
						constprop_error (*tptr, "remainder of division by zero");
						ires = 0;
					} else {
						ires = (long long)i1 % (long long)i2;
					}
				} else if ((*tptr)->tag == opi.tag_PLUS) {
					ires = (long long)i1 + (long long)i2;
					ires &= 0xffffffffLL;
				} else if ((*tptr)->tag == opi.tag_MINUS) {
					ires = (long long)i1 - (long long)i2;
					ires &= 0xffffffffLL;
				} else if ((*tptr)->tag == opi.tag_TIMES) {
					ires = (long long)i1 * (long long)i2;
					ires &= 0xffffffffLL;
				}
				if (ires > 0xffffffffLL) {
					constprop_error (*tptr, "INT constant overflow");
					ires &= 0xffffffffLL;
				}
				i1 = (int)ires;
				newconst = constprop_newconst (CONST_INT, *tptr, tnode_nthsubof (*tptr, 2), i1);
			}
			break;
		case CONST_DOUBLE:
		case CONST_ULL:
			constprop_warning (*tptr, "occampi_constprop_dop(): unsupported constant type.. (yet)");
			break;
		}

		*tptr = newconst;
	}

	return 1;
}
/*}}}*/
/*{{{  static int occampi_premap_dop (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	maps out a DOPNODE, turning into a back-end RESULT
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_premap_dop (compops_t *cops, tnode_t **node, map_t *map)
{
	/* pre-map left and right */
	map_subpremap (tnode_nthsubaddr (*node, 0), map);
	map_subpremap (tnode_nthsubaddr (*node, 1), map);

	*node = map->target->newresult (*node, map);

	return 0;
}
/*}}}*/
/*{{{  static int occampi_namemap_dop (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	name-maps a DOPNODE, adding child nodes to any enclosing result
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_namemap_dop (compops_t *cops, tnode_t **node, map_t *map)
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
/*{{{  static int occampi_codegen_dop (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	called to do code-generation for a DOPNODE -- operands are already on the stack
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_codegen_dop (compops_t *cops, tnode_t *node, codegen_t *cgen)
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

/*{{{  static tnode_t *occampi_gettype_dop (langops_t *lops, tnode_t *node, tnode_t *defaulttype)*/
/*
 *	returns the type associated with a DOPNODE, also sets the type in the node
 */
static tnode_t *occampi_gettype_dop (langops_t *lops, tnode_t *node, tnode_t *defaulttype)
{
	tnode_t *lefttype, *righttype;

	lefttype = typecheck_gettype (tnode_nthsubof (node, 0), defaulttype);
	righttype = typecheck_gettype (tnode_nthsubof (node, 1), defaulttype);

	if (lefttype == defaulttype) {
		tnode_setnthsub (node, 2, righttype);
	} else if (righttype == defaulttype) {
		tnode_setnthsub (node, 2, lefttype);
	} else {
		tnode_setnthsub (node, 2, tnode_copytree (defaulttype));
	}
	/* FIXME! -- needs more.. */

	return tnode_nthsubof (node, 2);
}
/*}}}*/
/*{{{  static int occampi_iscomplex_dop (langops_t *lops, tnode_t *node, int deep)*/
/*
 *	returns non-zero if the dyadic operation is complex (i.e. warrants separate evaluation)
 */
static int occampi_iscomplex_dop (langops_t *lops, tnode_t *node, int deep)
{
	int i = 0;

	if ((node->tag == opi.tag_REM) || (node->tag == opi.tag_DIV)) {
		return 1;		/* yes, complex generally */
	}

	if (deep) {
		i = langops_iscomplex (tnode_nthsubof (node, 0), deep);
		if (!i) {
			i = langops_iscomplex (tnode_nthsubof (node, 1), deep);
		}
	}
	return i;
}
/*}}}*/


/*{{{  static int occampi_typecheck_rel (compops_t *cops, tnode_t *tptr, typecheck_t *tc)*/
/*
 *	does type-checking on a relational operator
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_typecheck_rel (compops_t *cops, tnode_t *tptr, typecheck_t *tc)
{
	/* FIXME! */
	return 1;
}
/*}}}*/
/*{{{  static int occampi_premap_rel (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	maps out a DOPNODE, turning into a back-end RESULT
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_premap_rel (compops_t *cops, tnode_t **node, map_t *map)
{
	/* pre-map left and right */
	map_subpremap (tnode_nthsubaddr (*node, 0), map);
	map_subpremap (tnode_nthsubaddr (*node, 1), map);

	*node = map->target->newresult (*node, map);

	return 0;
}
/*}}}*/
/*{{{  static int occampi_namemap_rel (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	name-maps a DOPNODE, adding child nodes to any enclosing result
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_namemap_rel (compops_t *cops, tnode_t **node, map_t *map)
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
/*{{{  static int occampi_codegen_rel (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	called to do code-generation for a RELNODE -- operands are already on the stack
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_codegen_rel (compops_t *cops, tnode_t *node, codegen_t *cgen)
{
	int i;

	for (i=0; relmap[i].lookup; i++) {
		if (node->tag == *(relmap[i].tagp)) {
			relmap[i].cgfunc (cgen, relmap[i].cgarg);
			return 0;
		}
	}

	codegen_error (cgen, "occampi_codgen_rel(): don\'t know how to generate code for [%s] [%s]", node->tag->ndef->name, node->tag->name);

	return 0;
}
/*}}}*/

/*{{{  static tnode_t *occampi_gettype_rel (langops_t *lops, tnode_t *node, tnode_t *defaulttype)*/
/*
 *	returns the type associated with a DOPNODE, also sets the type in the node
 */
static tnode_t *occampi_gettype_rel (langops_t *lops, tnode_t *node, tnode_t *defaulttype)
{
	tnode_t *lefttype, *righttype;

	lefttype = typecheck_gettype (tnode_nthsubof (node, 0), defaulttype);
	righttype = typecheck_gettype (tnode_nthsubof (node, 1), defaulttype);

	/* FIXME! -- needs more.. */
	if (!tnode_nthsubof (node, 2)) {
		/* not got a type yet -- always BOOL */
		tnode_setnthsub (node, 2, tnode_create (opi.tag_BOOL, NULL));
	}

	return tnode_nthsubof (node, 2);
}
/*}}}*/
/*{{{  static int occampi_iscomplex_rel (langops_t *lops, tnode_t *node, int deep)*/
/*
 *	returns non-zero if the relational operation is complex
 */
static int occampi_iscomplex_rel (langops_t *lops, tnode_t *node, int deep)
{
	int i = 0;

	if (deep) {
		i = langops_iscomplex (tnode_nthsubof (node, 0), deep);
		if (!i) {
			i = langops_iscomplex (tnode_nthsubof (node, 1), deep);
		}
	}

	return i;
}
/*}}}*/
/*{{{  static void occampi_oper_genrelop (codegen_t *cgen, int arg)*/
/*
 *	generates code for a relational operator
 */
static void occampi_oper_genrelop (codegen_t *cgen, int arg)
{
	transinstr_e tins = (transinstr_e)arg;

	switch (tins) {
	case I_EQ:
		codegen_callops (cgen, tsecondary, I_DIFF);
		codegen_callops (cgen, tsecondary, I_BOOLINVERT);
		break;
	case I_GT:
		codegen_callops (cgen, tsecondary, I_GT);
		break;
	case I_LT:
		codegen_callops (cgen, tsecondary, I_LT);
		break;
	default:
		codegen_callops (cgen, comment, "occampi_oper_genrelop(): fixme!");
		break;
	}
	return;
}
/*}}}*/
/*{{{  static void occampi_oper_geninvrelop (codegen_t *cgen, int arg)*/
/*
 *	generates code for a relational operator and inverts the result
 */
static void occampi_oper_geninvrelop (codegen_t *cgen, int arg)
{
	transinstr_e tins = (transinstr_e)arg;

	switch (tins) {
	case I_EQ:
		codegen_callops (cgen, tsecondary, I_DIFF);
		break;
	case I_GT:
		codegen_callops (cgen, tsecondary, I_GT);
		codegen_callops (cgen, tsecondary, I_BOOLINVERT);
		break;
	case I_LT:
		codegen_callops (cgen, tsecondary, I_LT);
		codegen_callops (cgen, tsecondary, I_BOOLINVERT);
		break;
	default:
		codegen_callops (cgen, comment, "occampi_oper_geninvrelop(): fixme!");
		break;
	}
	return;
}
/*}}}*/


/*{{{  static int occampi_typecheck_mop (compops_t *cops, tnode_t *tptr, typecheck_t *tc)*/
/*
 *	does type-checking on a monadic operator
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_typecheck_mop (compops_t *cops, tnode_t *tptr, typecheck_t *tc)
{
	/* FIXME! */
	return 1;
}
/*}}}*/
/*{{{  static int occampi_premap_mop (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	does pre-mapping for a MOPNODE -- inserts back-end RESULT nodes
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_premap_mop (compops_t *cops, tnode_t **node, map_t *map)
{
	/* pre-map operand */
	map_subpremap (tnode_nthsubaddr (*node, 0), map);

	*node = map->target->newresult (*node, map);

	return 0;
}
/*}}}*/
/*{{{  static int occampi_namemap_mop (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	does name-mapping for a MOPNODE
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_namemap_mop (compops_t *cops, tnode_t **node, map_t *map)
{
	/* name-map operand */
	map_submapnames (tnode_nthsubaddr (*node, 0), map);

	/* set in result */
	map_addtoresult (tnode_nthsubaddr (*node, 0), map);

	return 0;
}
/*}}}*/
/*{{{  static int occampi_codegen_mop (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	called to do code-generation for a MOPNODE -- operand already on the stack
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_codegen_mop (compops_t *cops, tnode_t *node, codegen_t *cgen)
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
/*{{{  static tnode_t *occampi_gettype_mop (langops_t *lops, tnode_t *node, tnode_t *defaulttype)*/
/*
 *	gets type of a MOPNODE
 */
static tnode_t *occampi_gettype_mop (langops_t *lops, tnode_t *node, tnode_t *defaulttype)
{
	tnode_t *optype;

	optype = typecheck_gettype (tnode_nthsubof (node, 0), defaulttype);
	tnode_setnthsub (node, 1, optype);
	/* FIXME! -- needs more.. */

	return optype;
}
/*}}}*/
/*{{{  static int occampi_iscomplex_mop (langops_t *lops, tnode_t *node, int deep)*/
/*
 *	returns non-zero if the monadic operator is complex
 */
static int occampi_iscomplex_mop (langops_t *lops, tnode_t *node, int deep)
{
	int i = 0;

	if (deep) {
		i = langops_iscomplex (tnode_nthsubof (node, 0), deep);
	}

	return i;
}
/*}}}*/


/*{{{  static int occampi_typecheck_typecast (compops_t *cops, tnode_t *node, typecheck_t *tc)*/
/*
 *	does type-checking on a type-cast
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_typecheck_typecast (compops_t *cops, tnode_t *node, typecheck_t *tc)
{
	/* FIXME! */
	return 1;
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
#if 1
fprintf (stderr, "occampi_reduce_dop: unhandled symbol:\n");
lexer_dumptoken (stderr, tok);
#endif
		parser_error (pp->lf, "occampi_reduce_dop(): unhandled token [%s]", lexer_stokenstr (tok));
		return;
	}
	*(dfast->ptr) = tnode_create (tag, pp->lf, lhs, rhs, NULL);

	return;
}
/*}}}*/
/*{{{  static void occampi_reduce_rel (dfastate_t *dfast, parsepriv_t *pp, void *rarg)*/
/*
 *	reduces a relational operator, expects 2 nodes on the node-stack,
 *	and the relevant operator token on the token-stack
 */
static void occampi_reduce_rel (dfastate_t *dfast, parsepriv_t *pp, void *rarg)
{
	token_t *tok = parser_gettok (pp);
	tnode_t *lhs, *rhs;
	ntdef_t *tag = NULL;
	int i;

	if (!tok) {
		parser_error (pp->lf, "occampi_reduce_rel(): no token ?");
		return;
	}
	rhs = dfa_popnode (dfast);
	lhs = dfa_popnode (dfast);
	if (!rhs || !lhs) {
		parser_error (pp->lf, "occampi_reduce_rel(): lhs=0x%8.8x, rhs=0x%8.8x", (unsigned int)lhs, (unsigned int)rhs);
		return;
	}
	for (i=0; relmap[i].lookup; i++) {
		if (lexer_tokmatch (relmap[i].tok, tok)) {
			tag = *(relmap[i].tagp);
			break;
		}
	}
#if 0
if (!tag) {
lexer_dumptoken (stderr, tok);
for (i=0; relmap[i].lookup; i++) {
	fprintf (stderr, "--> tagname [%s], token: ", *(relmap[i].tagp) ? (*(relmap[i].tagp))->name : "(null)");
	lexer_dumptoken (stderr, relmap[i].tok);
}
}
#endif
	if (!tag) {
		parser_error (pp->lf, "occampi_reduce_rel(): unhandled token [%s]", lexer_stokenstr (tok));
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
/*{{{  static void occampi_reduce_typecast (dfastate_t *dfast, parsepriv_t *pp, void *rarg)*/
/*
 *	reduces a type-cast, expects operand and type on the node-stack
 */
static void occampi_reduce_typecast (dfastate_t *dfast, parsepriv_t *pp, void *rarg)
{
	tnode_t *operand, *rtype;
	int i;

	rtype = dfa_popnode (dfast);
	operand = dfa_popnode (dfast);
	if (!operand || !rtype) {
		parser_error (pp->lf, "occampi_reduce_typecast(): operand=0x%8.8x rtype=0x%8.8x", (unsigned int)operand, (unsigned int)rtype);
		return;
	}
	*(dfast->ptr) = tnode_create (opi.tag_TYPECAST, pp->lf, operand, rtype);

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
	langops_t *lops;
	int i;

	/*{{{  register reduction functions*/
	fcnlib_addfcn ("occampi_reduce_dop", occampi_reduce_dop, 0, 3);
	fcnlib_addfcn ("occampi_reduce_rel", occampi_reduce_rel, 0, 3);
	fcnlib_addfcn ("occampi_reduce_mop", occampi_reduce_mop, 0, 3);
	fcnlib_addfcn ("occampi_reduce_typecast", occampi_reduce_typecast, 0, 3);

	/*}}}*/
	/*{{{  occampi:dopnode -- MUL, DIV, ADD, SUB, REM; PLUS, MINUS, TIMES*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:dopnode", &i, 3, 0, 0, TNF_NONE);			/* subnodes: 0 = left; 1 = right; 2 = type */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (occampi_typecheck_dop));
	tnode_setcompop (cops, "constprop", 1, COMPOPTYPE (occampi_constprop_dop));
	tnode_setcompop (cops, "premap", 2, COMPOPTYPE (occampi_premap_dop));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (occampi_namemap_dop));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (occampi_codegen_dop));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "gettype", 2, LANGOPTYPE (occampi_gettype_dop));
	tnode_setlangop (lops, "iscomplex", 2, LANGOPTYPE (occampi_iscomplex_dop));
	tnd->lops = lops;

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
	i = -1;
	opi.tag_LSHIFT = tnode_newnodetag ("LSHIFT", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_RSHIFT = tnode_newnodetag ("RSHIFT", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_BITOR = tnode_newnodetag ("BITOR", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_BITAND = tnode_newnodetag ("BITAND", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_BITXOR = tnode_newnodetag ("BITXOR", &i, tnd, NTF_NONE);
	/*}}}*/
	/*{{{  occampi:relnode -- RELEQ, RELNEQ, RELLT, RELGT, RELLEQ, RELGEQ, RELAND, RELOR, RELXOR*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:relnode", &i, 3, 0, 0, TNF_NONE);			/* subnodes: 0 = left; 1 = right; 2 = type */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (occampi_typecheck_rel));
	tnode_setcompop (cops, "premap", 2, COMPOPTYPE (occampi_premap_rel));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (occampi_namemap_rel));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (occampi_codegen_rel));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "gettype", 2, LANGOPTYPE (occampi_gettype_rel));
	tnode_setlangop (lops, "iscomplex", 2, LANGOPTYPE (occampi_iscomplex_rel));
	tnd->lops = lops;

	i = -1;
	opi.tag_RELEQ = tnode_newnodetag ("RELEQ", &i, tnd, NTF_BOOLOP);
	i = -1;
	opi.tag_RELNEQ = tnode_newnodetag ("RELNEQ", &i, tnd, NTF_BOOLOP);
	i = -1;
	opi.tag_RELGT = tnode_newnodetag ("RELGT", &i, tnd, NTF_BOOLOP);
	i = -1;
	opi.tag_RELGEQ = tnode_newnodetag ("RELGEQ", &i, tnd, NTF_BOOLOP);
	i = -1;
	opi.tag_RELLT = tnode_newnodetag ("RELLT", &i, tnd, NTF_BOOLOP);
	i = -1;
	opi.tag_RELLEQ = tnode_newnodetag ("RELLEQ", &i, tnd, NTF_BOOLOP);
	/*}}}*/
	/*{{{  occampi:mopnode -- UMINUS, BITNOT*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:mopnode", &i, 2, 0, 0, TNF_NONE);			/* subnodes: 0 = operand; 1 = type */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (occampi_typecheck_mop));
	tnode_setcompop (cops, "premap", 2, COMPOPTYPE (occampi_premap_mop));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (occampi_namemap_mop));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (occampi_codegen_mop));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "gettype", 2, LANGOPTYPE (occampi_gettype_mop));
	tnode_setlangop (lops, "iscomplex", 2, LANGOPTYPE (occampi_iscomplex_mop));
	tnd->lops = lops;

	i = -1;
	opi.tag_UMINUS = tnode_newnodetag ("UMINUS", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_BITNOT = tnode_newnodetag ("BITNOT", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  occampi:typecastnode -- TYPECAST*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:typecastnode", &i, 2, 0, 0, TNF_NONE);		/* subnodes: 0 = operand; 1 = type */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (occampi_typecheck_typecast));
	// tnode_setcompop (cops, "premap", 2, COMPOPTYPE (occampi_premap_typecast));
	// tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (occampi_namemap_typecast));
	// tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (occampi_codegen_typecast));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	// tnode_setlangop (lops, "gettype", 2, LANGOPTYPE (occampi_gettype_mop));
	// tnode_setlangop (lops, "iscomplex", 2, LANGOPTYPE (occampi_iscomplex_mop));
	tnd->lops = lops;

	i = -1;
	opi.tag_TYPECAST = tnode_newnodetag ("TYPECAST", &i, tnd, NTF_NONE);
	/*}}}*/

	/*{{{  setup local tokens*/
	for (i=0; dopmap[i].lookup; i++) {
		dopmap[i].tok = lexer_newtoken (dopmap[i].ttype, dopmap[i].lookup);
	}
	for (i=0; relmap[i].lookup; i++) {
		relmap[i].tok = lexer_newtoken (relmap[i].ttype, relmap[i].lookup);
	}
	for (i=0; mopmap[i].lookup; i++) {
		mopmap[i].tok = lexer_newtoken (mopmap[i].ttype, mopmap[i].lookup);
	}

	/*}}}*/

	return 0;
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
	reg_reducers: NULL,
	init_dfatrans: NULL,
	post_setup: occampi_oper_post_setup,
	ident: "occampi-oper"
};
/*}}}*/



