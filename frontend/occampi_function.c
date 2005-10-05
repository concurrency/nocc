/*
 *	occampi_function.c -- occam-pi FUNCTIONs
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


/*{{{  static int occampi_function_init_nodes (void)*/
/*
 *	sets up nodes for occam-pi functionators (monadic, dyadic)
 *	returns 0 on success, non-zero on error
 */
static int occampi_function_init_nodes (void)
{
	tndef_t *tnd;
	compops_t *cops;
	int i;

	/*{{{  occampi:finstancenode -- FINSTANCE*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:finstancenode", &i, 3, 0, 0, TNF_NONE);
	cops = tnode_newcompops ();
	tnd->ops = cops;
	i = -1;
	opi.tag_FINSTANCE = tnode_newnodetag ("FINSTANCE", &i, tnd, NTF_NONE);
	/*}}}*/

	return 0;
}
/*}}}*/
/*{{{  static int occampi_function_reg_reducers (void)*/
/*
 *	registers reductions for occam-pi functionators
 *	returns 0 on success, non-zero on error
 */
static int occampi_function_reg_reducers (void)
{

	return 0;
}
/*}}}*/
/*{{{  static dfattbl_t **occampi_function_init_dfatrans (int *ntrans)*/
/*
 *	initialises and returns DFA transition tables for occam-pi functionators
 */
static dfattbl_t **occampi_function_init_dfatrans (int *ntrans)
{
	DYNARRAY (dfattbl_t *, transtbl);

	dynarray_init (transtbl);

	*ntrans = DA_CUR (transtbl);
	return DA_PTR (transtbl);
}
/*}}}*/
/*{{{  static int occampi_function_post_setup (void)*/
/*
 *	does post-setup for occam-pi functionator nodes
 *	returns 0 on success, non-zero on error
 */
static int occampi_function_post_setup (void)
{
	return 0;
}
/*}}}*/


/*{{{  occampi_function_feunit (feunit_t)*/
feunit_t occampi_function_feunit = {
	init_nodes: occampi_function_init_nodes,
	reg_reducers: occampi_function_reg_reducers,
	init_dfatrans: occampi_function_init_dfatrans,
	post_setup: occampi_function_post_setup
};
/*}}}*/


