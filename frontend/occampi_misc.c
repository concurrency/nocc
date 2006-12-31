/*
 *	occampi_misc.c - miscellaneous things for occam-pi
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
#include "langdef.h"
#include "dfa.h"
#include "parsepriv.h"
#include "occampi.h"
#include "feunit.h"
#include "names.h"
#include "scope.h"
#include "prescope.h"
#include "typecheck.h"
#include "betrans.h"
#include "langops.h"
#include "target.h"
#include "map.h"
#include "transputer.h"
#include "codegen.h"


/*}}}*/


/*{{{  static int occampi_misc_prescope (compops_t *cops, tnode_t **tptr, prescope_t *ps)*/
/*
 *	does pre-scoping on a misc node
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_misc_prescope (compops_t *cops, tnode_t **tptr, prescope_t *ps)
{
	return 1;
}
/*}}}*/
/*{{{  static int occampi_misc_precode (compops_t *cops, tnode_t **tptr, codegen_t *cgen)*/
/*
 *	does pre-codegen on a misc node
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_misc_precode (compops_t *cops, tnode_t **tptr, codegen_t *cgen)
{
	return 1;
}
/*}}}*/
/*{{{  static int occampi_misc_namemap (compops_t *cops, tnode_t **tptr, map_t *map)*/
/*
 *	does name-mapping on a misc node
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_misc_namemap (compops_t *cops, tnode_t **tptr, map_t *map)
{
	return 1;
}
/*}}}*/
/*{{{  static int occampi_misc_codegen (compops_t *cops, tnode_t *tptr, codegen_t *cgen)*/
/*
 *	does code-generation for a misc node
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_misc_codegen (compops_t *cops, tnode_t *tptr, codegen_t *cgen)
{
	return 1;
}
/*}}}*/



/*{{{  static int occampi_misc_init_nodes (void)*/
/*
 *	initialises misc nodes for occam-pi
 *	return 0 on success, non-zero on error
 */
static int occampi_misc_init_nodes (void)
{
	int i;
	tndef_t *tnd;
	compops_t *cops;
	langops_t *lops;

	/*{{{  occampi:miscnode -- MISCCOMMENT*/
	i = -1;
	tnd = opi.node_MISCNODE = tnode_newnodetype ("occampi:miscnode", &i, 1, 0, 0, TNF_TRANSPARENT);			/* subnodes: 0 = body */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "prescope", 2, COMPOPTYPE (occampi_misc_prescope));
	tnode_setcompop (cops, "precode", 2, COMPOPTYPE (occampi_misc_precode));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (occampi_misc_namemap));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (occampi_misc_codegen));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnd->lops = lops;

	i = -1;
	opi.tag_MISCCOMMENT = tnode_newnodetag ("MISCCOMMENT", &i, tnd, NTF_NONE);

	/*}}}*/

	return 0;
}
/*}}}*/
/*{{{  static int occampi_misc_post_setup (void)*/
/*
 *	does post-setup for misc nodes
 *	returns 0 on success, non-zero on failure
 */
static int occampi_misc_post_setup (void)
{
	return 0;
}
/*}}}*/


/*{{{  occampi_misc_feunit (feunit_t struct)*/
feunit_t occampi_misc_feunit = {
	init_nodes: occampi_misc_init_nodes,
	reg_reducers: NULL,
	init_dfatrans: NULL,
	post_setup: occampi_misc_post_setup,
	ident: "occampi-misc"
};
/*}}}*/

