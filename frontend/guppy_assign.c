/*
 *	guppy_assign.c -- assignment for Guppy
 *	Copyright (C) 2010-2013 Fred Barnes <frmb@kent.ac.uk>
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
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (guppy_namemap_assign));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (guppy_codegen_assign));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnd->lops = lops;

	i = -1;
	gup.tag_ASSIGN = tnode_newnodetag ("ASSIGN", &i, tnd, NTF_NONE);
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

