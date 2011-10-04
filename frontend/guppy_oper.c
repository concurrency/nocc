/*
 *	guppy_oper.c -- operators for guppy
 *	Copyright (C) 2011 Fred Barnes <frmb@kent.ac.uk>
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

	/*{{{  guppy:dopnode -- ADD, SUB, MUL, DIV, REM, BITXOR, BITAND, BITOR*/
	i = -1;
	tnd = tnode_newnodetype ("guppy:dopnode", &i, 3, 0, 0, TNF_NONE);			/* subnodes: left, right, type */
	cops = tnode_newcompops ();
	tnd->ops = cops;

	i = -1;
	gup.tag_ADD = tnode_newnodetag ("ADD", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_SUB = tnode_newnodetag ("SUB", &i, tnd, NTF_NONE);

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

