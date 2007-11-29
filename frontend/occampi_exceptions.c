/*
 *	occampi_exceptions.c -- EXCEPTION mechanism for occam-pi
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
#include "symbols.h"
#include "keywords.h"
#include "lexer.h"
#include "lexpriv.h"
#include "tnode.h"
#include "parser.h"
#include "treeops.h"
#include "dfa.h"
#include "parsepriv.h"
#include "occampi.h"
#include "feunit.h"
#include "names.h"
#include "fcnlib.h"
#include "scope.h"
#include "prescope.h"
#include "library.h"
#include "typecheck.h"
#include "precheck.h"
#include "usagecheck.h"
#include "tracescheck.h"
#include "map.h"
#include "target.h"
#include "transputer.h"
#include "codegen.h"
#include "langops.h"
#include "fetrans.h"


/*}}}*/


/*{{{  static int occampi_exceptions_init_nodes (void)*/
/*
 *	initialises exception handling nodes
 *	returns 0 on success, non-zero on failure
 */
static int occampi_exceptions_init_nodes (void)
{
	tndef_t *tnd;
	int i;
	compops_t *cops;
	langops_t *lops;

	/*{{{  occampi:exceptiontypedecl -- EXCEPTIONTYPEDECL*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:exceptiontypedecl", &i, 3, 0, 0, TNF_SHORTDECL);	/* subnodes: 0 = name, 1 = type, 2 = in-scope-body */
	cops = tnode_newcompops ();
	tnd->ops = cops;

	i = -1;
	opi.tag_EXCEPTIONTYPEDECL = tnode_newnodetag ("EXCEPTIONTYPEDECL", &i, tnd, NTF_NONE);

	/*}}}*/

	return 0;
}
/*}}}*/
/*{{{  static int occampi_exceptions_post_setup (void)*/
/*
 *	does post-setup for exceptions nodes
 *	returns 0 on success, non-zero on failure
 */
static int occampi_exceptions_post_setup (void)
{
	return 0;
}
/*}}}*/


/*{{{  occampi_exceptions_feunit (feunit_t)*/
feunit_t occampi_exceptions_feunit = {
	init_nodes: occampi_exceptions_init_nodes,
	reg_reducers: NULL,
	init_dfatrans: NULL,
	post_setup: occampi_exceptions_post_setup,
	ident: "occampi-exceptions"
};
/*}}}*/


