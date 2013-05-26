/*
 *	guppy_oper.c -- operators for guppy
 *	Copyright (C) 2011-2013 Fred Barnes <frmb@kent.ac.uk>
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


/*}}}*/
/*{{{  private things*/

static tnode_t *guppy_oper_inttypenode = NULL;
static tnode_t *guppy_oper_booltypenode = NULL;

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
	atype = typecheck_typeactual (pref ? op0type : op1type, pref ? op1type : op0type, node, tc);
	if (!atype) {
		typecheck_error (node, tc, "incompatible types for operator");
		return 0;
	}

	tnode_setnthsub (node, 2, atype);

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

/*{{{  static int guppy_typecheck_arrayopnode (compops_t *cops, tnode_t *node, typecheck_t *tc)*/
/*
 *	does type-checking on an array-operator node (arraysub)
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_typecheck_arrayopnode (compops_t *cops, tnode_t *node, typecheck_t *tc)
{
	tnode_t *operand = tnode_nthsubof (node, 0);
	tnode_t *optype, *idxtype;
	tnode_t *rtype, *mytype;

	optype = typecheck_gettype (operand, NULL);
	idxtype = typecheck_gettype (tnode_nthsubof (node, 1), guppy_oper_inttypenode);

	rtype = typecheck_typeactual (guppy_oper_inttypenode, idxtype, node, tc);

#if 0
fprintf (stderr, "guppy_typecheck_arrayopnode(): got optype =\n");
tnode_dumptree (optype, 1, FHAN_STDERR);
fprintf (stderr, "guppy_typecheck_arrayopnode(): got rtype (from idxtype) =\n");
tnode_dumptree (idxtype, 1, FHAN_STDERR);
#endif

	if (optype->tag == gup.tag_STRING) {
		/* special case: strings can be treated as an array of characters */
		mytype = guppy_newprimtype (gup.tag_CHAR, node, 0);
	} else if (optype->tag == gup.tag_ARRAY) {
		/* array type, this becomes subtype */
		mytype = tnode_copytree (tnode_nthsubof (optype, 0));
	} else {
		typecheck_error (node, tc, "array-sub used on non-array, got type [%s] from [%s]", rtype->tag->name, idxtype->tag->name);
	}

	tnode_setnthsub (node, 2, mytype);

	return 0;
}
/*}}}*/
/*{{{  static int guppy_codegen_arrayopnode (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	does code-generation for an array operator (array-sub)
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_codegen_arrayopnode (compops_t *cops, tnode_t *node, codegen_t *cgen)
{
	codegen_subcodegen (tnode_nthsubof (node, 0), cgen);
	codegen_write_fmt (cgen, "[");
	codegen_subcodegen (tnode_nthsubof (node, 1), cgen);
	codegen_write_fmt (cgen, "]");

	return 0;
}
/*}}}*/
/*{{{  static tnode_t *guppy_gettype_arrayopnode (langops_t *lops, tnode_t *node, tnode_t *default_type)*/
/*
 *	gets type of an array-operator node (arraysub)
 */
static tnode_t *guppy_gettype_arrayopnode (langops_t *lops, tnode_t *node, tnode_t *default_type)
{
	return tnode_nthsubof (node, 2);
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

	/*{{{  guppy:dopnode -- ADD, SUB, MUL, DIV, REM, XOR, AND, OR, BITXOR, BITAND, BITOR*/
	i = -1;
	tnd = tnode_newnodetype ("guppy:dopnode", &i, 3, 0, 0, TNF_NONE);			/* subnodes: left, right, type */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (guppy_typecheck_dopnode));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (guppy_namemap_dopnode));
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
	gup.tag_BITXOR = tnode_newnodetag ("BITXOR", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_BITAND = tnode_newnodetag ("BITAND", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_BITOR = tnode_newnodetag ("BITOR", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  guppy:booldopnode -- XOR, AND, OR*/
	i = -1;
	tnd = tnode_newnodetype ("guppy:booldopnode", &i, 3, 0, 0, TNF_NONE);			/* subnodes: left, right, type */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (guppy_typecheck_booldopnode));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (guppy_namemap_booldopnode));
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
	tnd->ops = cops;
	lops = tnode_newlangops ();
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
	tnd->ops = cops;
	lops = tnode_newlangops ();
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
	/*{{{  guppy:arrayopnode -- ARRAYSUB*/
	i = -1;
	tnd = tnode_newnodetype ("guppy:arrayopnode", &i, 3, 0, 0, TNF_NONE);			/* subnodes: operand, index, type */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (guppy_typecheck_arrayopnode));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (guppy_codegen_arrayopnode));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "gettype", 2, LANGOPTYPE (guppy_gettype_arrayopnode));
	tnd->lops = lops;

	i = -1;
	gup.tag_ARRAYSUB = tnode_newnodetag ("ARRAYSUB", &i, tnd, NTF_NONE);

	/*}}}*/

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

