/*
 *	guppy_assign.c -- assignment for Guppy
 *	Copyright (C) 2010-2016 Fred Barnes <frmb@kent.ac.uk>
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
#include <stdint.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/types.h>

#include "nocc.h"
#include "support.h"
#include "version.h"
#include "fhandle.h"
#include "symbols.h"
#include "keywords.h"
#include "lexer.h"
#include "lexpriv.h"
#include "tnode.h"
#include "parser.h"
#include "fcnlib.h"
#include "dfa.h"
#include "parsepriv.h"
#include "guppy.h"
#include "feunit.h"
#include "names.h"
#include "scope.h"
#include "prescope.h"
#include "typecheck.h"
#include "constprop.h"
#include "tracescheck.h"
#include "langops.h"
#include "usagecheck.h"
#include "fetrans.h"
#include "betrans.h"
#include "map.h"
#include "codegen.h"
#include "target.h"
#include "transputer.h"


/*}}}*/


/*{{{  static int guppy_prescope_assign (compops_t *cops, tnode_t **nodep, prescope_t *ps)*/
/*
 *	does pre-scoping for an assignment
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_prescope_assign (compops_t *cops, tnode_t **nodep, prescope_t *ps)
{
	return 1;
}
/*}}}*/
/*{{{  static int guppy_typecheck_assign (compops_t *cops, tnode_t *node, typecheck_t *tc)*/
/*
 *	does type-checking for an assignment
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_typecheck_assign (compops_t *cops, tnode_t *node, typecheck_t *tc)
{
	tnode_t *lhs = tnode_nthsubof (node, 0);
	tnode_t *rhs = tnode_nthsubof (node, 1);
	tnode_t **typep = tnode_nthsubaddr (node, 2);
	tnode_t *lhstype, *rhstype, *acttype;

	typecheck_subtree (lhs, tc);
	typecheck_subtree (rhs, tc);

	if (*typep) {
		nocc_serious ("guppy_typecheck_assign(): already got type!");
		return 0;
	}

	lhstype = typecheck_gettype (lhs, NULL);
	rhstype = typecheck_gettype (rhs, lhstype);
#if 0
fhandle_printf (FHAN_STDERR, "guppy_typecheck_assign(): lhstype =\n");
tnode_dumptree (lhstype, 1, FHAN_STDERR);
fhandle_printf (FHAN_STDERR, "rhstype =\n");
tnode_dumptree (rhstype, 1, FHAN_STDERR);
#endif
	if (!rhstype) {
		typecheck_error (node, tc, "invalid type on right of assignment");
		return 0;
	} else if (parser_islistnode (rhstype) && (parser_countlist (rhstype) == 1)) {
		/* singleton list -- possibly a function call or similar */
		rhstype = parser_getfromlist (rhstype, 0);
	}

	acttype = typecheck_typeactual (lhstype, rhstype, node, tc);
#if 0
fhandle_printf (FHAN_STDERR, "acttype =\n");
tnode_dumptree (acttype, 1, FHAN_STDERR);
#endif
	if (!acttype) {
		typecheck_error (node, tc, "incompatible types in assignment");
		return 0;
	}
	*typep = acttype;

	return 0;
}
/*}}}*/
/*{{{  static int guppy_fetrans1_assign (compops_t *cops, tnode_t **nodep, guppy_fetrans1_t *fe1)*/
/*
 *	does fetrans1 for an assignment (unpicks RHS)
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_fetrans1_assign (compops_t *cops, tnode_t **nodep, guppy_fetrans1_t *fe1)
{
	if ((*nodep)->tag == gup.tag_ASSIGN) {
		fe1->inspoint = nodep;
		guppy_fetrans1_subtree (tnode_nthsubaddr (*nodep, 0), fe1);
		guppy_fetrans1_subtree (tnode_nthsubaddr (*nodep, 1), fe1);
		fe1->inspoint = NULL;
		fe1->decllist = NULL;
	}
	return 0;
}
/*}}}*/
/*{{{  static int guppy_fetrans15_assign (compops_t *cops, tnode_t **nodep, guppy_fetrans15_t *fe15)*/
/*
 *	does fetrans1.5 for an assignment (do nothing, don't look inside)
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_fetrans15_assign (compops_t *cops, tnode_t **nodep, guppy_fetrans15_t *fe15)
{
	return 0;
}
/*}}}*/
/*{{{  static int guppy_fetrans2_assign (compops_t *cops, tnode_t **nodep, guppy_fetrans2_t *fe2)*/
/*
 *	does fetrans2 for an assignment (if SASSIGN and RHS instance, pushes result parameters in)
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_fetrans2_assign (compops_t *cops, tnode_t **nodep, guppy_fetrans2_t *fe2)
{
	if ((*nodep)->tag == gup.tag_SASSIGN) {
		tnode_t *rhs = tnode_nthsubof (*nodep, 1);

		if (rhs->tag == gup.tag_INSTANCE) {
			tnode_t *itype = typecheck_gettype (tnode_nthsubof (rhs, 0), NULL);

			if (itype && (itype->tag == gup.tag_FCNTYPE)) {
				/* have results, push into parameter list */
				tnode_t *plist = tnode_nthsubof (rhs, 1);
				tnode_t *lhslist = tnode_nthsubof (*nodep, 0);
				tnode_t **litems;
				int i, nlitems;

#if 0
fhandle_printf (FHAN_STDERR, "guppy_fetrans2_assign(): SASSIGN instance of function, plist is:\n");
tnode_dumptree (plist, 1, FHAN_STDERR);
fhandle_printf (FHAN_STDERR, "guppy_fetrans2_assign(): whole thing is:\n");
tnode_dumptree (*nodep, 1, FHAN_STDERR);
#endif
				litems = parser_getlistitems (lhslist, &nlitems);
				for (i=0; i<nlitems; i++) {
					parser_insertinlist (plist, litems[i], i);
					litems[i] = NULL;
				}

				/* make instance the whole thing */
				*nodep = rhs;
			}
		}
		/* won't do subtrees */
		return 0;
	}
	return 1;
}
/*}}}*/
/*{{{  static int guppy_fetrans3_assign (compops_t *cops, tnode_t **nodep, guppy_fetrans3_t *fe3)*/
/*
 *	does fetrans3 for an assignment (if string, map into special)
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_fetrans3_assign (compops_t *cops, tnode_t **nodep, guppy_fetrans3_t *fe3)
{
	tnode_t *type = tnode_nthsubof (*nodep, 2);

	if ((type->tag == gup.tag_STRING) && ((*nodep)->tag == gup.tag_ASSIGN)) {
		/* transform */
		tnode_t *action = tnode_create (gup.tag_STRASSIGN, OrgOf (*nodep), NULL, tnode_nthsubof (*nodep, 0), tnode_nthsubof (*nodep, 1), NULL);

		*nodep = action;
	}
	return 1;
}
/*}}}*/
/*{{{  static int guppy_namemap_assign (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	does name-mapping for an assignment
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_namemap_assign (compops_t *cops, tnode_t **node, map_t *map)
{
	map_submapnames (tnode_nthsubaddr (*node, 0), map);
	map_submapnames (tnode_nthsubaddr (*node, 1), map);
	return 0;
}
/*}}}*/
/*{{{  static int guppy_codegen_assign (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	does code-generation for an assignment
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_codegen_assign (compops_t *cops, tnode_t *node, codegen_t *cgen)
{
	codegen_ssetindent (cgen);
	codegen_subcodegen (tnode_nthsubof (node, 0), cgen);
	codegen_write_fmt (cgen, " = ");
	codegen_subcodegen (tnode_nthsubof (node, 1), cgen);
	codegen_write_fmt (cgen, ";\n");

	return 0;
}
/*}}}*/


/*{{{  static int guppy_assign_init_nodes (void)*/
/*
 *	called to initialise parse-tree nodes for assignment
 *	returns 0 on success, non-zero on failure
 */
static int guppy_assign_init_nodes (void)
{
	tndef_t *tnd;
	int i;
	compops_t *cops;
	langops_t *lops;

	/*{{{  guppy:assign -- ASSIGN, IS*/
	i = -1;
	tnd = tnode_newnodetype ("guppy:assign", &i, 3, 0, 0, TNF_NONE);		/* subnodes: 0 = LHS, 1 = RHS, 2 = type */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "prescope", 2, COMPOPTYPE (guppy_prescope_assign));
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (guppy_typecheck_assign));
	tnode_setcompop (cops, "fetrans1", 2, COMPOPTYPE (guppy_fetrans1_assign));
	tnode_setcompop (cops, "fetrans15", 2, COMPOPTYPE (guppy_fetrans15_assign));
	tnode_setcompop (cops, "fetrans2", 2, COMPOPTYPE (guppy_fetrans2_assign));
	tnode_setcompop (cops, "fetrans3", 2, COMPOPTYPE (guppy_fetrans3_assign));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (guppy_namemap_assign));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (guppy_codegen_assign));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnd->lops = lops;

	i = -1;
	gup.tag_ASSIGN = tnode_newnodetag ("ASSIGN", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_SASSIGN = tnode_newnodetag ("SASSIGN", &i, tnd, NTF_NONE);		/* simplified assignment (added in fetrans1/2) */
	i = -1;
	gup.tag_IS = tnode_newnodetag ("IS", &i, tnd, NTF_NONE);

	/*}}}*/

	return 0;
}
/*}}}*/


/*{{{  guppy_assign_feunit (feunit_t)*/
feunit_t guppy_assign_feunit = {
	.init_nodes = guppy_assign_init_nodes,
	.reg_reducers = NULL,
	.init_dfatrans = NULL,
	.post_setup = NULL,
	.ident = "guppy-assign"
};
/*}}}*/

