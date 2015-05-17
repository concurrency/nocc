/*
 *	guppy_oper.c -- operators for guppy
 *	Copyright (C) 2011-2015 Fred Barnes <frmb@kent.ac.uk>
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

#include "nocc.h"
#include "support.h"
#include "version.h"
#include "origin.h"
#include "fhandle.h"
#include "symbols.h"
#include "keywords.h"
#include "lexer.h"
#include "lexpriv.h"
#include "tnode.h"
#include "parser.h"
#include "dfa.h"
#include "dfaerror.h"
#include "parsepriv.h"
#include "guppy.h"
#include "feunit.h"
#include "fcnlib.h"
#include "names.h"
#include "scope.h"
#include "prescope.h"
#include "library.h"
#include "typecheck.h"
#include "constprop.h"
#include "precheck.h"
#include "usagecheck.h"
#include "map.h"
#include "target.h"
#include "transputer.h"
#include "codegen.h"
#include "langops.h"
#include "fetrans.h"
#include "betrans.h"
#include "metadata.h"
#include "tracescheck.h"
#include "mobilitycheck.h"
#include "cccsp.h"


/*}}}*/
/*{{{  private things*/

static tnode_t *guppy_oper_inttypenode = NULL;
static tnode_t *guppy_oper_booltypenode = NULL;

STATICPOINTERHASH (ntdef_t *, compdopmap, 4);

/*}}}*/


/*{{{  static int guppy_typecheck_dopnode (compops_t *cops, tnode_t *node, typecheck_t *tc)*/
/*
 *	does type-checking on a dyadic operator
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_typecheck_dopnode (compops_t *cops, tnode_t *node, typecheck_t *tc)
{
	tnode_t *op0, *op1;
	tnode_t *op0type, *op1type;
	tnode_t *atype;
	int pref = 0;

	op0 = tnode_nthsubof (node, 0);
	op1 = tnode_nthsubof (node, 1);

	/* walk operators */
	typecheck_subtree (op0, tc);
	typecheck_subtree (op1, tc);

	/* first, blindly attempt to get default types */
	op0type = typecheck_gettype (op0, NULL);
	op1type = typecheck_gettype (op1, NULL);

	if (!op0type && !op1type) {
		typecheck_error (node, tc, "failed to determine either type for operator");
		return 0;
	}
	if (!op0type) {
		op0type = typecheck_gettype (op0, op1type);
		pref = 1;
	} else if (!op1type) {
		op1type = typecheck_gettype (op1, op0type);
		pref = 0;
	}
	if (!op0type || !op1type) {
		typecheck_error (node, tc, "failed to determine types for operator");
		return 0;
	}
	/* reduce any singleton lists that we encounter */
	if (op0type && parser_islistnode (op0type) && (parser_countlist (op0type) == 1)) {
		op0type = parser_getfromlist (op0type, 0);
	}
	if (op1type && parser_islistnode (op1type) && (parser_countlist (op1type) == 1)) {
		op1type = parser_getfromlist (op1type, 0);
	}
	atype = typecheck_typeactual (pref ? op0type : op1type, pref ? op1type : op0type, node, tc);
	if (!atype) {
		typecheck_error (node, tc, "incompatible types for operator");
		return 0;
	}

	tnode_setnthsub (node, 2, atype);

	/* some slightly special handling for strings */
	if (atype->tag == gup.tag_STRING) {
		if (node->tag != gup.tag_ADD) {
			typecheck_error (node, tc, "cannot do [%s] on a string", node->tag->name);
			return 0;
		}
	}

	return 0;
}
/*}}}*/
/*{{{  static int guppy_fetrans1_dopnode (compops_t *cops, tnode_t **nodep, guppy_fetrans1_t *fe1)*/
/*
 *	does front-end-1 transforms for dyadic operators (handles strings specially)
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_fetrans1_dopnode (compops_t *cops, tnode_t **nodep, guppy_fetrans1_t *fe1)
{
	tnode_t *type = tnode_nthsubof (*nodep, 2);

	/* do fetrans1 on subtrees -- dismantle anything complex there */
	guppy_fetrans1_subtree (tnode_nthsubaddr (*nodep, 0), fe1);
	guppy_fetrans1_subtree (tnode_nthsubaddr (*nodep, 1), fe1);

	if ((type->tag == gup.tag_STRING) && ((*nodep)->tag == gup.tag_ADD)) {
		/* turn into special string operator and temporary */
		tnode_t *tname, *concat, *seq, *seqlist;
		tnode_t **newp;

		tname = guppy_fetrans1_maketemp (gup.tag_NDECL, *nodep, type, NULL, fe1);
		concat = tnode_createfrom (gup.tag_STRCONCAT, *nodep, NULL, tname, tnode_nthsubof (*nodep, 0), tnode_nthsubof (*nodep, 1));
		seqlist = parser_newlistnode (SLOCI);
		parser_addtolist (seqlist, concat);
		newp = parser_addtolist (seqlist, *fe1->inspoint);
		seq = tnode_createfrom (gup.tag_SEQ, *nodep, NULL, seqlist);

		*nodep = tname;
		*fe1->inspoint = seq;
		fe1->inspoint = newp;
	}
	return 0;
}
/*}}}*/
/*{{{  static int guppy_namemap_dopnode (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	does name-mapping for a dyadic operator node
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_namemap_dopnode (compops_t *cops, tnode_t **node, map_t *map)
{
	map_submapnames (tnode_nthsubaddr (*node, 0), map);
	map_submapnames (tnode_nthsubaddr (*node, 1), map);
	return 0;
}
/*}}}*/
/*{{{  static int guppy_lpreallocate_dopnode (compops_t *cops, tnode_t *node, cccsp_preallocate_t *cpa)*/
/*
 *	does pre-allocation for a dyadic operator
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_lpreallocate_dopnode (compops_t *cops, tnode_t *node, cccsp_preallocate_t *cpa)
{
	cpa->collect += 2;		/* arbitrary, but assume we need something */
	return 1;
}
/*}}}*/
/*{{{  static int guppy_codegen_dopnode (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	does code-generation for a dyadic operator node
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_codegen_dopnode (compops_t *cops, tnode_t *node, codegen_t *cgen)
{
	codegen_write_fmt (cgen, "(");
	codegen_subcodegen (tnode_nthsubof (node, 0), cgen);
	if (node->tag == gup.tag_ADD) {
		codegen_write_fmt (cgen, "+");
	} else if (node->tag == gup.tag_SUB) {
		codegen_write_fmt (cgen, "-");
	} else if (node->tag == gup.tag_DIV) {
		codegen_write_fmt (cgen, "/");
	} else if (node->tag == gup.tag_MUL) {
		codegen_write_fmt (cgen, "*");
	} else if (node->tag == gup.tag_REM) {
		codegen_write_fmt (cgen, "%%");
	} else if (node->tag == gup.tag_ASHR) {
		/* FIXME: needs something better! */
		codegen_write_fmt (cgen, ">>");
	} else if (node->tag == gup.tag_SHR) {
		/* FIXME: needs something better! */
		codegen_write_fmt (cgen, ">>");
	} else if (node->tag == gup.tag_SHL) {
		codegen_write_fmt (cgen, "<<");
	} else if (node->tag == gup.tag_PLUS) {
		codegen_write_fmt (cgen, "+");
	} else if (node->tag == gup.tag_MINUS) {
		codegen_write_fmt (cgen, "-");
	} else if (node->tag == gup.tag_TIMES) {
		codegen_write_fmt (cgen, "*");
	} else {
		nocc_internal ("guppy_codegen_dopnode(): unhandled operator!");
	}
	codegen_subcodegen (tnode_nthsubof (node, 1), cgen);
	codegen_write_fmt (cgen, ")");
	return 0;
}
/*}}}*/
/*{{{  static tnode_t *guppy_gettype_dopnode (langops_t *lops, tnode_t *node, tnode_t *default_type)*/
/*
 *	gets the type of a dyadic operator
 */
static tnode_t *guppy_gettype_dopnode (langops_t *lops, tnode_t *node, tnode_t *default_type)
{
	return tnode_nthsubof (node, 2);
}
/*}}}*/

/*{{{  static int guppy_postscope_compdopnode (compops_t *cops, tnode_t **nodep)*/
/*
 *	does post-scope for compound dop node (undoes into full assignment)
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_postscope_compdopnode (compops_t *cops, tnode_t **nodep)
{
	tnode_t *lhs = tnode_nthsubof (*nodep, 0);
	tnode_t *rhs = tnode_nthsubof (*nodep, 1);
	ntdef_t *xop, *tag = (*nodep)->tag;

#if 0
fhandle_printf (FHAN_STDERR, "guppy_postscope_compdopnode(): FIXME! :).  node is:\n");
tnode_dumptree (*nodep, 1, FHAN_STDERR);
#endif
	xop = pointerhash_lookup (compdopmap, (void *)tag);
	if (xop) {
		tnode_t *lhscopy = tnode_copytree (lhs);
		tnode_t *newass, *newop;

		newop = tnode_createfrom (xop, *nodep, lhscopy, rhs, NULL);
		newass = tnode_createfrom (gup.tag_ASSIGN, *nodep, lhs, newop, NULL);

		*nodep = newass;
	} else {
		tnode_error (*nodep, "unhandled compound operator (%s)", tag->name);
		nocc_internal ("abandoning");
	}
	return 0;
}
/*}}}*/

/*{{{  static int guppy_typecheck_booldopnode (compops_t *cops, tnode_t *node, typecheck_t *tc)*/
/*
 *	does type-checking for a boolean dop-node
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_typecheck_booldopnode (compops_t *cops, tnode_t *node, typecheck_t *tc)
{
	tnode_t *op0, *op1;
	tnode_t *op0type, *op1type;
	tnode_t *atype;
	int pref = 0;

	op0 = tnode_nthsubof (node, 0);
	op1 = tnode_nthsubof (node, 1);

	/* walk operators */
	typecheck_subtree (op0, tc);
	typecheck_subtree (op1, tc);

	/* attempt to get default types, should be boolean */
	op0type = typecheck_gettype (op0, guppy_oper_booltypenode);
	op1type = typecheck_gettype (op1, guppy_oper_booltypenode);

	if (!op0type && !op1type) {
		typecheck_error (node, tc, "failed to determine either type for boolean operator");
		return 0;
	}
	if (!op0type) {
		op0type = typecheck_gettype (op0, op1type);
		pref = 1;
	} else if (!op1type) {
		op1type = typecheck_gettype (op1, op0type);
		pref = 0;
	}
	if (!op0type || !op1type) {
		typecheck_error (node, tc, "failed to determine types for boolean operator");
		return 0;
	}
	atype = typecheck_typeactual (pref ? op0type : op1type, pref ? op1type : op0type, node, tc);
	if (!atype) {
		typecheck_error (node, tc, "incompatible types for boolean operator");
		return 0;
	}

	tnode_setnthsub (node, 2, atype);

	return 0;
}
/*}}}*/
/*{{{  static int guppy_namemap_booldopnode (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	does name-mapping for a boolean operator node
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_namemap_booldopnode (compops_t *cops, tnode_t **node, map_t *map)
{
	map_submapnames (tnode_nthsubaddr (*node, 0), map);
	map_submapnames (tnode_nthsubaddr (*node, 1), map);
	return 0;
}
/*}}}*/
/*{{{  static int guppy_lpreallocate_booldopnode (compops_t *cops, tnode_t *node, cccsp_preallocate_t *cpa)*/
/*
 *	does pre-allocation for a boolean dyadic operator
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_lpreallocate_booldopnode (compops_t *cops, tnode_t *node, cccsp_preallocate_t *cpa)
{
	cpa->collect += 2;		/* arbitrary, but assume we need something */
	return 1;
}
/*}}}*/
/*{{{  static int guppy_codegen_booldopnode (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	does code-generation for a boolean operator node
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_codegen_booldopnode (compops_t *cops, tnode_t *node, codegen_t *cgen)
{
	codegen_write_fmt (cgen, "(");
	codegen_subcodegen (tnode_nthsubof (node, 0), cgen);
	if (node->tag == gup.tag_AND) {
		codegen_write_fmt (cgen, "&&");
	} else if (node->tag == gup.tag_OR) {
		codegen_write_fmt (cgen, "||");
	} else if (node->tag == gup.tag_XOR) {
		codegen_write_fmt (cgen, "^");
	} else {
		nocc_internal ("guppy_codegen_booldopnode(): unhandled operator!");
	}
	codegen_subcodegen (tnode_nthsubof (node, 1), cgen);
	codegen_write_fmt (cgen, ")");
	return 0;
}
/*}}}*/
/*{{{  static tnode_t *guppy_gettype_booldopnode (langops_t *lops, tnode_t *node, tnode_t *default_type)*/
/*
 *	gets the type of a boolean operator
 */
static tnode_t *guppy_gettype_booldopnode (langops_t *lops, tnode_t *node, tnode_t *default_type)
{
	return tnode_nthsubof (node, 2);
}
/*}}}*/

/*{{{  static int guppy_typecheck_reldopnode (compops_t *cops, tnode_t *node, typecheck_t *tc)*/
/*
 *	does type-checking on a relational operator
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_typecheck_reldopnode (compops_t *cops, tnode_t *node, typecheck_t *tc)
{
	tnode_t *op0, *op1;
	tnode_t *op0type, *op1type;
	tnode_t *atype;
	int pref = 0;

	op0 = tnode_nthsubof (node, 0);
	op1 = tnode_nthsubof (node, 1);

	/* walk operators */
	typecheck_subtree (op0, tc);
	typecheck_subtree (op1, tc);

	/* first, blindly attempt to get default types */
	op0type = typecheck_gettype (op0, NULL);
	op1type = typecheck_gettype (op1, NULL);

	if (!op0type && !op1type) {
		typecheck_error (node, tc, "failed to determine either type for relational operator");
		return 0;
	}
	if (!op0type) {
		op0type = typecheck_gettype (op0, op1type);
		pref = 1;
	} else if (!op1type) {
		op1type = typecheck_gettype (op1, op0type);
		pref = 0;
	}
	if (!op0type || !op1type) {
		typecheck_error (node, tc, "failed to determine types for relational operator");
		return 0;
	}
	atype = typecheck_typeactual (pref ? op0type : op1type, pref ? op1type : op0type, node, tc);
	if (!atype) {
		typecheck_error (node, tc, "incompatible types for relational operator");
		return 0;
	}

	atype = tnode_copytree (guppy_oper_booltypenode);
	tnode_setnthsub (node, 2, atype);

	return 0;
}
/*}}}*/
/*{{{  static int guppy_namemap_reldopnode (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	does name-mapping for a relational operator node
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_namemap_reldopnode (compops_t *cops, tnode_t **node, map_t *map)
{
	map_submapnames (tnode_nthsubaddr (*node, 0), map);
	map_submapnames (tnode_nthsubaddr (*node, 1), map);
	return 0;
}
/*}}}*/
/*{{{  static int guppy_lpreallocate_reldopnode (compops_t *cops, tnode_t *node, cccsp_preallocate_t *cpa)*/
/*
 *	does pre-allocation for a relational operator
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_lpreallocate_reldopnode (compops_t *cops, tnode_t *node, cccsp_preallocate_t *cpa)
{
	cpa->collect += 2;		/* arbitrary, but assume we need something */
	return 1;
}
/*}}}*/
/*{{{  static int guppy_codegen_reldopnode (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	does code-generation for a relational operator node
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_codegen_reldopnode (compops_t *cops, tnode_t *node, codegen_t *cgen)
{
	codegen_write_fmt (cgen, "(");
	codegen_subcodegen (tnode_nthsubof (node, 0), cgen);
	if (node->tag == gup.tag_LT) {
		codegen_write_fmt (cgen, "<");
	} else if (node->tag == gup.tag_GT) {
		codegen_write_fmt (cgen, ">");
	} else if (node->tag == gup.tag_LE) {
		codegen_write_fmt (cgen, "<=");
	} else if (node->tag == gup.tag_GE) {
		codegen_write_fmt (cgen, ">=");
	} else if (node->tag == gup.tag_EQ) {
		codegen_write_fmt (cgen, "==");
	} else if (node->tag == gup.tag_NE) {
		codegen_write_fmt (cgen, "!=");
	} else {
		nocc_internal ("guppy_codegen_reldopnode(): unhandled operator!");
	}
	codegen_subcodegen (tnode_nthsubof (node, 1), cgen);
	codegen_write_fmt (cgen, ")");
	return 0;
}
/*}}}*/
/*{{{  static tnode_t *guppy_gettype_reldopnode (langops_t *lops, tnode_t *node, tnode_t *default_type)*/
/*
 *	gets the type of a relational operator
 */
static tnode_t *guppy_gettype_reldopnode (langops_t *lops, tnode_t *node, tnode_t *default_type)
{
	return tnode_nthsubof (node, 2);
}
/*}}}*/

/*{{{  static int guppy_typecheck_mopnode (compops_t *cops, tnode_t *node, typecheck_t *tc)*/
/*
 *	does type-checking for a monadic operator (neg, bitnot)
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_typecheck_mopnode (compops_t *cops, tnode_t *node, typecheck_t *tc)
{
	tnode_t *op, *optype, *atype;
	
	op = tnode_nthsubof (node, 0);
	typecheck_subtree (op, tc);

	optype = typecheck_gettype (op, guppy_oper_inttypenode);

	if (!optype) {
		typecheck_error (node, tc, "failed to determine type for operand");
		return 0;
	}
	atype = typecheck_typeactual (optype, optype, node, tc);
	if (!atype) {
		typecheck_error (node, tc, "impossible type for monadic operator");
		return 0;
	}

	tnode_setnthsub (node, 1, atype);
	return 0;
}
/*}}}*/
/*{{{  static int guppy_lpreallocate_mopnode (compops_t *cops, tnode_t *node, cccsp_preallocate_t *cpa)*/
/*
 *	does pre-allocation for a monadic operator
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_lpreallocate_mopnode (compops_t *cops, tnode_t *node, cccsp_preallocate_t *cpa)
{
	cpa->collect += 2;		/* arbitrary, but assume we need something */
	return 1;
}
/*}}}*/
/*{{{  static int guppy_codegen_mopnode (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	does code-generation for a monadic operator (neg, bitnot)
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_codegen_mopnode (compops_t *cops, tnode_t *node, codegen_t *cgen)
{
	if (node->tag == gup.tag_NEG) {
		codegen_write_fmt (cgen, "-");
	} else if (node->tag == gup.tag_BITNOT) {
		codegen_write_fmt (cgen, "~");
	}
	codegen_write_fmt (cgen, "(");
	codegen_subcodegen (tnode_nthsubof (node, 0), cgen);
	codegen_write_fmt (cgen, ")");
	return 0;
}
/*}}}*/
/*{{{  static tnode_t *guppy_gettype_mopnode (langops_t *lops, tnode_t *node, tnode_t *default_type)*/
/*
 *	gets the type of a monadic operator
 */
static tnode_t *guppy_gettype_mopnode (langops_t *lops, tnode_t *node, tnode_t *default_type)
{
	return tnode_nthsubof (node, 1);
}
/*}}}*/

/*{{{  static int guppy_lpreallocate_boolmopnode (compops_t *cops, tnode_t *node, cccsp_preallocate_t *cpa)*/
/*
 *	does pre-allocation for a boolean monadic operator
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_lpreallocate_boolmopnode (compops_t *cops, tnode_t *node, cccsp_preallocate_t *cpa)
{
	cpa->collect += 2;		/* arbitrary, but assume we need something */
	return 1;
}
/*}}}*/

/*{{{  static int guppy_typecheck_chanopnode (compops_t *cops, tnode_t *node, typecheck_t *tc)*/
/*
 *	does type-checking for a channel-operator
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_typecheck_chanopnode (compops_t *cops, tnode_t *node, typecheck_t *tc)
{
	tnode_t *op = tnode_nthsubof (node, 0);
	tnode_t **typep = tnode_nthsubaddr (node, 1);
	tnode_t *optype;
	int m_in, m_out;

	if (*typep) {
		/* already done! */
		return 0;
	}
	typecheck_subtree (op, tc);
	optype = typecheck_gettype (op, NULL);

	if (!optype) {
		typecheck_error (node, tc, "cannot determine type of operator [%s] in [%s]", op->tag->name, node->tag->name);
		return 0;
	} else if (optype->tag != gup.tag_CHAN) {
		typecheck_error (node, tc, "invalid type for [%s], expected channel, got [%s]", node->tag->name, optype->tag->name);
		return 0;
	}

	if (guppy_chantype_getinout (optype, &m_in, &m_out)) {
		typecheck_error (node, tc, "failed to get channel-directions from operand [%s]", op->tag->name);
		return 0;
	}
	if ((m_out && (node->tag == gup.tag_MARKEDIN)) || (m_in && (node->tag == gup.tag_MARKEDOUT))) {
		typecheck_error (node, tc, "incompatible direction specified on operand [%s]", op->tag->name);
		return 0;
	}
	if (!m_in && !m_out) {
		/* neither is specified, probably means we're talking about both-ends, so duplicate type first */
		optype = tnode_copytree (optype);
		if (node->tag == gup.tag_MARKEDIN) {
			m_in = 1;
		} else if (node->tag == gup.tag_MARKEDOUT) {
			m_out = 1;
		} else {
			nocc_internal ("guppy_typecheck_chanopnode(): impossible thing!");
			return 0;
		}
		guppy_chantype_setinout (optype, m_in, m_out);
	}

	*typep = optype;
#if 0
fhandle_printf (FHAN_STDERR, "guppy_typecheck_chanopnode(): checking type on channel-op [%s], got:\n", node->tag->name);
tnode_dumptree (optype, 1, FHAN_STDERR);
#endif
	return 0;
}
/*}}}*/
/*{{{  static int guppy_typeresolve_chanopnode (compops_t *cops, tnode_t **nodep, typecheck_t *tc)*/
/*
 *	does type-resolution for a channel-operator (trivially, replaces with operand)
 *	return 0 to stop walk, 1 to continue
 */
static int guppy_typeresolve_chanopnode (compops_t *cops, tnode_t **nodep, typecheck_t *tc)
{
	tnode_t *type = tnode_nthsubof (*nodep, 1);

	if (!type) {
		typecheck_error (*nodep, tc, "unchecked channel-operator [%s]", (*nodep)->tag->name);
		return 0;
	}
	*nodep = tnode_nthsubof (*nodep, 0);
	typeresolve_subtree (nodep, tc);

	return 0;
}
/*}}}*/
/*{{{  static tnode_t *guppy_gettype_chanopnode (langops_t *lops, tnode_t *node, tnode_t *default_type)*/
/*
 *	gets the type of a channel-operator node (trivial)
 */
static tnode_t *guppy_gettype_chanopnode (langops_t *lops, tnode_t *node, tnode_t *default_type)
{
	return tnode_nthsubof (node, 1);
}
/*}}}*/
/*{{{  static int guppy_getname_chanopnode (langops_t *lops, tnode_t *node, char **strp)*/
/*
 *	gets the name of a channel-operator node
 *	returns 0 on success, < 0 on error
 */
static int guppy_getname_chanopnode (langops_t *lops, tnode_t *node, char **strp)
{
	char *lstr;

	langops_getname (tnode_nthsubof (node, 0), strp);
	if (*strp) {
		if (node->tag == gup.tag_MARKEDIN) {
			lstr = string_fmt ("%s?", *strp);
		} else if (node->tag == gup.tag_MARKEDOUT) {
			lstr = string_fmt ("%s!", *strp);
		} else {
			nocc_internal ("guppy_getname_chanopnode(): unhandled [%s]", node->tag->name);
			return -1;
		}
		sfree (*strp);
		*strp = lstr;
	}
	return 0;
}
/*}}}*/
/*{{{  static int guppy_isconst_chanopnode (langops_t *lops, tnode_t *node)*/
/*
 *	determines whether the specified thing is a constant
 */
static int guppy_isconst_chanopnode (langops_t *lops, tnode_t *node)
{
	return 0;
}
/*}}}*/
/*{{{  static int guppy_isvar_chanopnode (langops_t *lops, tnode_t *node)*/
/*
 *	determines whether the specified thing is a variable
 */
static int guppy_isvar_chanopnode (langops_t *lops, tnode_t *node)
{
	return langops_isvar (tnode_nthsubof (node, 0));
}
/*}}}*/
/*{{{  static int guppy_isaddressable_chanopnode (langops_t *lops, tnode_t *node)*/
/*
 *	determines whether the specified thing is addressable
 */
static int guppy_isaddressable_chanopnode (langops_t *lops, tnode_t *node)
{
	return langops_isaddressable (tnode_nthsubof (node, 0));
}
/*}}}*/

/*{{{  static int guppy_typecheck_typeopnode (compops_t *cops, tnode_t *node, typecheck_t *tc)*/
/*
 *	does type-checking on a type-operator node (size/bytesin)
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_typecheck_typeopnode (compops_t *cops, tnode_t *node, typecheck_t *tc)
{
	tnode_t *argtype;

	typecheck_subtree (tnode_nthsubof (node, 0), tc);
	argtype = typecheck_gettype (tnode_nthsubof (node, 0), NULL);
	if (!argtype) {
		typecheck_error (node, tc, "failed to get argument type for [%s]", node->tag->name);
		return 0;
	}
	if (node->tag == gup.tag_SIZE) {
		/* must be an array or (special case) a string */
		if ((argtype->tag != gup.tag_ARRAY) && (argtype->tag != gup.tag_STRING)) {
			typecheck_error (node, tc, "invalid type for 'size', expected array or string, got [%s]", argtype->tag->name);
			return 0;
		}
	}	/* else allow anything really! */

	tnode_setnthsub (node, 1, argtype);

	return 1;
}
/*}}}*/
/*{{{  static int guppy_constprop_typeopnode (compops_t *cops, tnode_t **nodep)*/
/*
 *	does constant propagation on a type-operator node (size/bytesin)
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_constprop_typeopnode (compops_t *cops, tnode_t **nodep)
{
	tnode_t *type = tnode_nthsubof (*nodep, 1);

#if 0
fhandle_printf (FHAN_STDERR, "guppy_constprop_typeopnode(): *nodep =\n");
tnode_dumptree (*nodep, 1, FHAN_STDERR);
#endif
	constprop_tree (tnode_nthsubaddr (*nodep, 0));		/* any propagation within argument */

	/* type must be array or string */
	if ((type->tag == gup.tag_STRING) || (type->tag == gup.tag_ARRAY)) {
		int cnst;

		if ((*nodep)->tag == gup.tag_SIZE) {
			/* ask how many elements */
			cnst = tnode_knownsizeof (type);
		} else if ((*nodep)->tag == gup.tag_BYTESIN) {
			/* how many bytes */
			cnst = tnode_bytesfor (type, NULL);
		}

		if (cnst >= 0) {
			tnode_t *stype = guppy_newprimtype (gup.tag_INT, *nodep, 0);

			*nodep = constprop_newconst (CONST_INT, *nodep, stype, cnst);
		}
	} else {
		nocc_internal ("guppy_constprop_typeopnode(): type not string or array!  got [%s:%s]", type->tag->ndef->name, type->tag->name);
	}

	return 0;
}
/*}}}*/
/*{{{  static tnode_t *guppy_gettype_typeopnode (langops_t *lops, tnode_t *node, tnode_t *default_type)*/
/*
 *	gets type of a type-operator node (size/bytesin)
 */
static tnode_t *guppy_gettype_typeopnode (langops_t *lops, tnode_t *node, tnode_t *default_type)
{
	return NULL;
}
/*}}}*/

/*{{{  static int guppy_scopein_arrayopnode (compops_t *cops, tnode_t **nodep, scope_t *ss)*/
/*
 *	does scope-in on an array-operator node (arraysub, possibly recordsub later)
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_scopein_arrayopnode (compops_t *cops, tnode_t **nodep, scope_t *ss)
{
	tnode_t **lhsp = tnode_nthsubaddr (*nodep, 0);
	tnode_t **rhsp = tnode_nthsubaddr (*nodep, 1);
	guppy_scope_t *gss = (guppy_scope_t *)ss->langpriv;

	scope_subtree (lhsp, ss);
	/* Updated: doesn't matter if the RHS scope picks up something that isn't a field in recordsub, since typecheck will put it right
	 * once the LHS type is known.
	 */
	if ((*rhsp)->tag == gup.tag_NAME) {
		gss->resolve_nametype_first = gup.tag_NFIELD;
		scope_subtree (rhsp, ss);
		gss->resolve_nametype_first = NULL;
	} else {
		scope_subtree (rhsp, ss);
	}

	return 0;
}
/*}}}*/
/*{{{  static int guppy_typecheck_arrayopnode (compops_t *cops, tnode_t *node, typecheck_t *tc)*/
/*
 *	does type-checking on an array-operator node (arraysub or recordsub)
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_typecheck_arrayopnode (compops_t *cops, tnode_t *node, typecheck_t *tc)
{
	if (node->tag == gup.tag_ARRAYSUB) {
		tnode_t *operand = tnode_nthsubof (node, 0);
		tnode_t *mytype, *optype;

		optype = typecheck_gettype (operand, NULL);
		if (optype && (optype->tag == gup.tag_NTYPEDECL)) {
			/* RHS should be a field, but find it based on the name matching here */
			tnode_t *fname = tnode_nthsubof (node, 1);

			if (fname->tag->ndef != gup.node_NAMENODE) {
				typecheck_error (node, tc, "field-name is not a name, found [%s]", fname->tag->name);
			} else {
				tnode_t *flist = NameTypeOf (tnode_nthnameof (optype, 0));
				tnode_t **ftitems;
				int nftitems, i;
				char *fcharname = NameNameOf (tnode_nthnameof (fname, 0));

				ftitems = parser_getlistitems (flist, &nftitems);
				for (i=0; i<nftitems; i++) {
					/* ftitems[i] should be a FIELDDECL */
					tnode_t *fldname = tnode_nthsubof (ftitems[i], 0);

					if (fldname->tag != gup.tag_NFIELD) {
						typecheck_error (fldname, tc, "type field-name is not a field, found [%s]", fldname->tag->name);
						break;				/* for() */
					} else {
						char *fldcharname = NameNameOf (tnode_nthnameof (fldname, 0));

						if (!strcmp (fldcharname, fcharname)) {
							/* this one! */
							fname = fldname;
							tnode_setnthsub (node, 1, fname);
							break;			/* for() */
						}
					}
				}
				if (i == nftitems) {
					typecheck_error (node, tc, "failed to find field [%s] in type", fcharname);
				}
			}

			/* fname should have been fiddled */
			mytype = typecheck_gettype (fname, NULL);

			tnode_changetag (node, gup.tag_RECORDSUB);

		} else {
			/* must be an array.. */
			tnode_t *rtype, *idxtype;

			idxtype = typecheck_gettype (tnode_nthsubof (node, 1), guppy_oper_inttypenode);

			rtype = typecheck_typeactual (guppy_oper_inttypenode, idxtype, node, tc);
#if 0
fhandle_printf (FHAN_STDERR, "guppy_typecheck_arrayopnode(): got optype =\n");
tnode_dumptree (optype, 1, FHAN_STDERR);
fhandle_printf (FHAN_STDERR, "guppy_typecheck_arrayopnode(): got rtype (from idxtype) =\n");
tnode_dumptree (idxtype, 1, FHAN_STDERR);
#endif

			if (optype->tag == gup.tag_STRING) {
				/* special case: strings can be treated as an array of characters */
				mytype = guppy_newprimtype (gup.tag_CHAR, node, 0);
			} else if (optype->tag == gup.tag_ARRAY) {
				/* array type, this becomes subtype */
				mytype = tnode_copytree (tnode_nthsubof (optype, 1));
			} else {
				typecheck_error (node, tc, "array-sub used on non-array, got type [%s] from [%s]", rtype->tag->name, idxtype->tag->name);
			}
		}

		tnode_setnthsub (node, 2, mytype);
	}	/* else ignore, must have been processed already */

	return 0;
}
/*}}}*/
/*{{{  static int guppy_namemap_arrayopnode (compops_t *cops, tnode_t **nodep, map_t *map)*/
/*
 *	does name-mapping for an array operator (arraysub, recordsub)
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_namemap_arrayopnode (compops_t *cops, tnode_t **nodep, map_t *map)
{
	cccsp_mapdata_t *cmd = (cccsp_mapdata_t *)map->hook;
	int tindir = cmd->target_indir;
	tnode_t *idxnode;
	tnode_t *type = tnode_nthsubof (*nodep, 2);

	/* for both array and record bases, want a pointer */
	cmd->target_indir = 1;
	map_submapnames (tnode_nthsubaddr (*nodep, 0), map);
	cmd->target_indir = 0;
	/* but for the index/field, just the plain thing */
	map_submapnames (tnode_nthsubaddr (*nodep, 1), map);

	cmd->target_indir = tindir;
	if ((*nodep)->tag == gup.tag_ARRAYSUB) {
		idxnode = cccsp_create_arraysub (OrgOf (*nodep), map->target, tnode_nthsubof (*nodep, 0), tnode_nthsubof (*nodep, 1), tindir, type);
	} else if ((*nodep)->tag == gup.tag_RECORDSUB) {
		idxnode = cccsp_create_recordsub (OrgOf (*nodep), map->target, tnode_nthsubof (*nodep, 0), tnode_nthsubof (*nodep, 1), tindir, type);
	} else {
		nocc_internal ("guppy_namemap_arrayopnode(): unhandled [%s]", (*nodep)->tag->name);
	}

	*nodep = idxnode;

	return 0;
}
/*}}}*/
/*{{{  static tnode_t *guppy_gettype_arrayopnode (langops_t *lops, tnode_t *node, tnode_t *default_type)*/
/*
 *	gets type of an array-operator node (arraysub, recordsub)
 */
static tnode_t *guppy_gettype_arrayopnode (langops_t *lops, tnode_t *node, tnode_t *default_type)
{
	return tnode_nthsubof (node, 2);
}
/*}}}*/
/*{{{  static int guppy_isvar_arrayopnode (langops_t *lops, tnode_t *node)*/
/*
 *	determines whether the specified array-operator is a variable (non-constant)
 */
static int guppy_isvar_arrayopnode (langops_t *lops, tnode_t *node)
{
	return langops_isvar (tnode_nthsubof (node, 0));
}
/*}}}*/

/*{{{  static int guppy_fetrans15_stropnode (compops_t *cops, tnode_t **nodep, guppy_fetrans15_t *fe15)*/
/*
 *	does fetrans1.5 on a string operator node (do nothing, don't look inside)
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_fetrans15_stropnode (compops_t *cops, tnode_t **nodep, guppy_fetrans15_t *fe15)
{
	return 0;
}
/*}}}*/
/*{{{  static int guppy_namemap_stropnode (compops_t *cops, tnode_t **nodep, map_t *map)*/
/*
 *	does name-mapping for a string operator (strassign, strconcat)
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_namemap_stropnode (compops_t *cops, tnode_t **nodep, map_t *map)
{
	cccsp_mapdata_t *cmd = (cccsp_mapdata_t *)map->hook;
	tnode_t *action, *aparms;
	cccsp_apicall_e apicall;

	tnode_setnthsub (*nodep, 0, cmd->process_id);
	cmd->target_indir = 0;
	map_submapnames (tnode_nthsubaddr (*nodep, 0), map);
	/* if assign, make target double-indirect */
	if ((*nodep)->tag == gup.tag_STRASSIGN) {
		cmd->target_indir = 2;
		apicall = STR_ASSIGN;
	} else if ((*nodep)->tag == gup.tag_STRCONCAT) {
		/* else, target is kept intact */
		cmd->target_indir = 1;
		apicall = STR_CONCAT;
	} else {
		nocc_internal ("guppy_namemap_stropnode(): unhandled [%s]", (*nodep)->tag->name);
		return 0;
	}

	map_submapnames (tnode_nthsubaddr (*nodep, 1), map);
	cmd->target_indir = 1;
	map_submapnames (tnode_nthsubaddr (*nodep, 2), map);
	if ((*nodep)->tag == gup.tag_STRCONCAT) {
		map_submapnames (tnode_nthsubaddr (*nodep, 3), map);
	}
	cmd->target_indir = 0;

	aparms = parser_newlistnode (SLOCI);
	parser_addtolist (aparms, tnode_nthsubof (*nodep, 0));
	parser_addtolist (aparms, tnode_nthsubof (*nodep, 1));
	parser_addtolist (aparms, tnode_nthsubof (*nodep, 2));
	if ((*nodep)->tag == gup.tag_STRCONCAT) {
		parser_addtolist (aparms, tnode_nthsubof (*nodep, 3));
	}
	action = tnode_create (gup.tag_APICALL, OrgOf (*nodep), cccsp_create_apicallname (apicall), aparms);

	*nodep = action;

	return 0;
}
/*}}}*/

/*{{{  static int guppy_oper_init_nodes (void)*/
/*
 *	called to initialise nodes/etc. for guppy operators
 *	returns 0 on success, non-zero on failure
 */
static int guppy_oper_init_nodes (void)
{
	tndef_t *tnd;
	compops_t *cops;
	langops_t *lops;
	int i;

	pointerhash_sinit (compdopmap);

	/*{{{  guppy:dopnode -- ADD, SUB, MUL, DIV, REM, ASHR, SHR, SHL, BITXOR, BITAND, BITOR, PLUS, MINUS, TIMES*/
	i = -1;
	tnd = tnode_newnodetype ("guppy:dopnode", &i, 3, 0, 0, TNF_NONE);			/* subnodes: left, right, type */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (guppy_typecheck_dopnode));
	tnode_setcompop (cops, "fetrans1", 2, COMPOPTYPE (guppy_fetrans1_dopnode));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (guppy_namemap_dopnode));
	tnode_setcompop (cops, "lpreallocate", 2, COMPOPTYPE (guppy_lpreallocate_dopnode));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (guppy_codegen_dopnode));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "gettype", 2, LANGOPTYPE (guppy_gettype_dopnode));
	tnd->lops = lops;

	i = -1;
	gup.tag_ADD = tnode_newnodetag ("ADD", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_SUB = tnode_newnodetag ("SUB", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_MUL = tnode_newnodetag ("MUL", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_DIV = tnode_newnodetag ("DIV", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_REM = tnode_newnodetag ("REM", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_ASHR = tnode_newnodetag ("ASHR", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_SHR = tnode_newnodetag ("SHR", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_SHL = tnode_newnodetag ("SHL", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_BITXOR = tnode_newnodetag ("BITXOR", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_BITAND = tnode_newnodetag ("BITAND", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_BITOR = tnode_newnodetag ("BITOR", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_PLUS = tnode_newnodetag ("PLUS", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_MINUS = tnode_newnodetag ("MINUS", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_TIMES = tnode_newnodetag ("TIMES", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  guppy:compdopnode -- ADDIN, SUBIN, DIVIN, REMIN, ASHRIN, SHRIN, SHLIN, BITXORIN, BITANDIN, PLUSIN, MINUSIN, TIMESIN, XORIN, ANDIN, ORIN*/
	/* Note: no type needed, disappears before typecheck */
	i = -1;
	tnd = tnode_newnodetype ("guppy:compdopnode", &i, 2, 0, 0, TNF_NONE);			/* subnodes: left, right */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "postscope", 1, COMPOPTYPE (guppy_postscope_compdopnode));
	tnd->ops = cops;

	i = -1;
	gup.tag_ADDIN = tnode_newnodetag ("ADDIN", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_SUBIN = tnode_newnodetag ("SUBIN", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  guppy:booldopnode -- XOR, AND, OR*/
	i = -1;
	tnd = tnode_newnodetype ("guppy:booldopnode", &i, 3, 0, 0, TNF_NONE);			/* subnodes: left, right, type */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (guppy_typecheck_booldopnode));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (guppy_namemap_booldopnode));
	tnode_setcompop (cops, "lpreallocate", 2, COMPOPTYPE (guppy_lpreallocate_booldopnode));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (guppy_codegen_booldopnode));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "gettype", 2, LANGOPTYPE (guppy_gettype_booldopnode));
	tnd->lops = lops;

	i = -1;
	gup.tag_XOR = tnode_newnodetag ("XOR", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_AND = tnode_newnodetag ("AND", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_OR = tnode_newnodetag ("OR", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  guppy:reldopnode -- LT, GT, LE, GE, EQ, NE*/
	i = -1;
	tnd = tnode_newnodetype ("guppy:reldopnode", &i, 3, 0, 0, TNF_NONE);			/* subnodes: left, right, type */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (guppy_typecheck_reldopnode));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (guppy_namemap_reldopnode));
	tnode_setcompop (cops, "lpreallocate", 2, COMPOPTYPE (guppy_lpreallocate_reldopnode));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (guppy_codegen_reldopnode));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "gettype", 2, LANGOPTYPE (guppy_gettype_reldopnode));
	tnd->lops = lops;

	i = -1;
	gup.tag_LT = tnode_newnodetag ("LT", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_GT = tnode_newnodetag ("GT", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_LE = tnode_newnodetag ("LE", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_GE = tnode_newnodetag ("GE", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_EQ = tnode_newnodetag ("EQ", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_NE = tnode_newnodetag ("NE", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  guppy:mopnode -- BITNOT, NEG*/
	i = -1;
	tnd = tnode_newnodetype ("guppy:mopnode", &i, 2, 0, 0, TNF_NONE);			/* subnodes: op, type */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (guppy_typecheck_mopnode));
	tnode_setcompop (cops, "lpreallocate", 2, COMPOPTYPE (guppy_lpreallocate_mopnode));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (guppy_codegen_mopnode));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "gettype", 2, LANGOPTYPE (guppy_gettype_mopnode));
	tnd->lops = lops;

	i = -1;
	gup.tag_BITNOT = tnode_newnodetag ("BITNOT", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_NEG = tnode_newnodetag ("NEG", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  guppy:boolmopnode -- NOT*/
	i = -1;
	tnd = tnode_newnodetype ("guppy:boolmopnode", &i, 2, 0, 0, TNF_NONE);			/* subnodes: op, type */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "lpreallocate", 2, COMPOPTYPE (guppy_lpreallocate_boolmopnode));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnd->lops = lops;

	i = -1;
	gup.tag_NOT = tnode_newnodetag ("NOT", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  guppy:chanopnode -- MARKEDIN, MARKEDOUT*/
	i = -1;
	tnd = tnode_newnodetype ("guppy:chanopnode", &i, 2, 0, 0, TNF_NONE);			/* subnodes: op, type */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (guppy_typecheck_chanopnode));
	tnode_setcompop (cops, "typeresolve", 2, COMPOPTYPE (guppy_typeresolve_chanopnode));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "gettype", 2, LANGOPTYPE (guppy_gettype_chanopnode));
	tnode_setlangop (lops, "getname", 2, LANGOPTYPE (guppy_getname_chanopnode));
	tnode_setlangop (lops, "isconst", 1, LANGOPTYPE (guppy_isconst_chanopnode));
	tnode_setlangop (lops, "isvar", 1, LANGOPTYPE (guppy_isvar_chanopnode));
	tnode_setlangop (lops, "isaddressable", 1, LANGOPTYPE (guppy_isaddressable_chanopnode));
	tnd->lops = lops;

	i = -1;
	gup.tag_MARKEDIN = tnode_newnodetag ("MARKEDIN", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_MARKEDOUT = tnode_newnodetag ("MARKEDOUT", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  guppy:typeopnode -- SIZE, BYTESIN*/
	/* RHS can either be an expression, or it can be a type */

	i = -1;
	tnd = tnode_newnodetype ("guppy:typeopnode", &i, 2, 0, 0, TNF_NONE);			/* subnodes: operand, type */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (guppy_typecheck_typeopnode));
	tnode_setcompop (cops, "constprop", 1, COMPOPTYPE (guppy_constprop_typeopnode));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "gettype", 2, LANGOPTYPE (guppy_gettype_typeopnode));
	tnd->lops = lops;

	i = -1;
	gup.tag_SIZE = tnode_newnodetag ("SIZE", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_BYTESIN = tnode_newnodetag ("BYTESIN", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  guppy:arrayopnode -- ARRAYSUB, RECORDSUB*/
	i = -1;
	tnd = tnode_newnodetype ("guppy:arrayopnode", &i, 3, 0, 0, TNF_NONE);			/* subnodes: operand, index, type */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (guppy_scopein_arrayopnode));
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (guppy_typecheck_arrayopnode));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (guppy_namemap_arrayopnode));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "gettype", 2, LANGOPTYPE (guppy_gettype_arrayopnode));
	tnode_setlangop (lops, "isvar", 1, LANGOPTYPE (guppy_isvar_arrayopnode));
	tnd->lops = lops;

	i = -1;
	gup.tag_ARRAYSUB = tnode_newnodetag ("ARRAYSUB", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_RECORDSUB = tnode_newnodetag ("RECORDSUB", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  guppy:stropnode -- STRASSIGN, STRCONCAT*/
	i = -1;
	tnd = tnode_newnodetype ("guppy:stropnode", &i, 4, 0, 0, TNF_NONE);			/* subnodes: wptr, target, src1, src2 */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "fetrans15", 2, COMPOPTYPE (guppy_fetrans15_stropnode));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (guppy_namemap_stropnode));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnd->lops = lops;

	i = -1;
	gup.tag_STRASSIGN = tnode_newnodetag ("STRASSIGN", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_STRCONCAT = tnode_newnodetag ("STRCONCAT", &i, tnd, NTF_NONE);

	/*}}}*/

	/* bung relevant ones into compdopmap */
	pointerhash_insert (compdopmap, gup.tag_ADD, (void *)gup.tag_ADDIN);
	pointerhash_insert (compdopmap, gup.tag_SUB, (void *)gup.tag_SUBIN);
	pointerhash_insert (compdopmap, gup.tag_MUL, (void *)gup.tag_MULIN);
	pointerhash_insert (compdopmap, gup.tag_DIV, (void *)gup.tag_DIVIN);
	pointerhash_insert (compdopmap, gup.tag_REM, (void *)gup.tag_REMIN);
	pointerhash_insert (compdopmap, gup.tag_ASHR, (void *)gup.tag_ASHRIN);
	pointerhash_insert (compdopmap, gup.tag_SHR, (void *)gup.tag_SHRIN);
	pointerhash_insert (compdopmap, gup.tag_SHL, (void *)gup.tag_SHLIN);
	pointerhash_insert (compdopmap, gup.tag_BITXOR, (void *)gup.tag_BITXORIN);
	pointerhash_insert (compdopmap, gup.tag_BITAND, (void *)gup.tag_BITANDIN);
	pointerhash_insert (compdopmap, gup.tag_BITOR, (void *)gup.tag_BITORIN);
	pointerhash_insert (compdopmap, gup.tag_PLUS, (void *)gup.tag_PLUSIN);
	pointerhash_insert (compdopmap, gup.tag_MINUS, (void *)gup.tag_MINUSIN);
	pointerhash_insert (compdopmap, gup.tag_TIMES, (void *)gup.tag_TIMESIN);
	pointerhash_insert (compdopmap, gup.tag_XOR, (void *)gup.tag_XORIN);
	pointerhash_insert (compdopmap, gup.tag_AND, (void *)gup.tag_ANDIN);
	pointerhash_insert (compdopmap, gup.tag_OR, (void *)gup.tag_ORIN);
	pointerhash_insert (compdopmap, gup.tag_PLUS, (void *)gup.tag_PLUSIN);

	return 0;
}
/*}}}*/
/*{{{  static int guppy_oper_post_setup (void)*/
/*
 *	called to do any post-setup
 *	returns 0 on success, non-zero on failure
 */
static int guppy_oper_post_setup (void)
{
	guppy_oper_inttypenode = guppy_newprimtype (gup.tag_INT, NULL, 0);
	guppy_oper_booltypenode = guppy_newprimtype (gup.tag_BOOL, NULL, 0);
	return 0;
}
/*}}}*/



/*{{{  guppy_oper_feunit (feunit_t)*/
feunit_t guppy_oper_feunit = {
	.init_nodes = guppy_oper_init_nodes,
	.reg_reducers = NULL,
	.init_dfatrans = NULL,
	.post_setup = guppy_oper_post_setup,
	.ident = "guppy-oper"
};

/*}}}*/

