/*
 *	occampi_typeop.c -- occam-pi type operators
 *	Copyright (C) 2007 Fred Barnes <frmb@kent.ac.uk>
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
#include "symbols.h"
#include "keywords.h"
#include "lexer.h"
#include "lexpriv.h"
#include "tnode.h"
#include "parser.h"
#include "fcnlib.h"
#include "langdef.h"
#include "dfa.h"
#include "dfaerror.h"
#include "parsepriv.h"
#include "occampi.h"
#include "feunit.h"
#include "names.h"
#include "scope.h"
#include "prescope.h"
#include "typecheck.h"
#include "constprop.h"
#include "betrans.h"
#include "langops.h"
#include "target.h"
#include "map.h"
#include "transputer.h"
#include "codegen.h"


/*}}}*/


/*{{{  static int occampi_typeop_typecheck (tnode_t *tptr, typecheck_t *tc)*/
/*
 *	does type-checking on a type operator node
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_typeop_typecheck (compops_t *cops, tnode_t *tptr, typecheck_t *tc)
{
	if (!typecheck_istype (tnode_nthsubof (tptr, 0))) {
		typecheck_error (tptr, tc, "operand is not a type");
	}
	return 1;
}
/*}}}*/
/*{{{  static int occampi_typeop_constprop (compops_t *cops, tnode_t **tptr)*/
/*
 *	does constant-propagation on a type operator node
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_typeop_constprop (compops_t *cops, tnode_t **tptr)
{
	return 1;
}
/*}}}*/
/*{{{  static int occampi_typeop_namemap (compops_t *cops, tnode_t **tptr, map_t *map)*/
/*
 *	does name-mapping on a type operator node
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_typeop_namemap (compops_t *cops, tnode_t **tptr, map_t *map)
{
	return 1;
}
/*}}}*/

/*{{{  static tnode_t *occampi_typeop_gettype (langops_t *lops, tnode_t *node, tnode_t *defaulttype)*/
/*
 *	gets the type of a type-operator node
 *	returns type on success, NULL on failure
 */
static tnode_t *occampi_typeop_gettype (langops_t *lops, tnode_t *node, tnode_t *defaulttype)
{
	return tnode_nthsubof (node, 1);
}
/*}}}*/


/*{{{  static int occampi_typeop_init_nodes (void)*/
/*
 *	initialises type-operator nodes for occam-pi
 *	returns 0 on success, non-zero on failure
 */
static int occampi_typeop_init_nodes (void)
{
	int i;
	tndef_t *tnd;
	compops_t *cops;
	langops_t *lops;

	/*{{{  occampi:typeopnode -- MOSTPOS, MOSTNEG*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:typeopnode", &i, 2, 0, 0, TNF_NONE);					/* subnodes: 0 = operand, 1 = type */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (occampi_typeop_typecheck));
	tnode_setcompop (cops, "constprop", 1, COMPOPTYPE (occampi_typeop_constprop));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (occampi_typeop_namemap));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "gettype", 2, LANGOPTYPE (occampi_typeop_gettype));
	tnd->lops = lops;

	i = -1;
	opi.tag_MOSTPOS = tnode_newnodetag ("MOSTPOS", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_MOSTNEG = tnode_newnodetag ("MOSTNEG", &i, tnd, NTF_NONE);

	/*}}}*/

	return 0;
}
/*}}}*/


/*{{{  occampi_typeop_feunit (feunit_t struct)*/
feunit_t occampi_typeop_feunit = {
	init_nodes: occampi_typeop_init_nodes,
	reg_reducers: NULL,
	init_dfatrans: NULL,
	post_setup: NULL,
	ident: "occampi-typeop"
};
/*}}}*/

