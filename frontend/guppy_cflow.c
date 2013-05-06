/*
 *	guppy_cflow.c -- control flow constructs for Guppy
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
/*{{{  private data*/


/*}}}*/

/*{{{  static int guppy_prescope_rnode (compops_t *cops, tnode_t **nodep, prescope_t *ps)*/
/*
 *	does pre-scoping for a 'return' node
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_prescope_rnode (compops_t *cops, tnode_t **nodep, prescope_t *ps)
{
	tnode_t **exprptr = tnode_nthsubaddr (*nodep, 0);

	if (*exprptr) {
		/* make sure it's a list */
		parser_ensurelist (exprptr, *nodep);
	}
	return 1;
}
/*}}}*/
/*{{{  static int guppy_typecheck_rnode (compops_t *cops, tnode_t *node, typecheck_t *tc)*/
/*
 *	does type-checking for a 'return' node
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_typecheck_rnode (compops_t *cops, tnode_t *node, typecheck_t *tc)
{
	guppy_typecheck_t *gtc = (guppy_typecheck_t *)tc->hook;
	tnode_t *rtype;
	tnode_t *expr = tnode_nthsubof (node, 0);
	tnode_t **rtitems, **expitems;
	int i, n_rtitems, n_expitems;
	tnode_t *acttypelist;

	if (!gtc || !gtc->encfcn) {
		typecheck_error (node, tc, "return outside of function body");
		return 0;
	}
	rtype = gtc->encfcnrtype;
	if (expr) {
		typecheck_subtree (expr, tc);
	}

	if (!rtype) {
		if (expr) {
			typecheck_error (node, tc, "return with a value from a result-less function");
		}
		return 0;
	}
	if (!expr) {
		typecheck_error (node, tc, "missing expression in return");
		return 0;
	}

	rtitems = parser_getlistitems (rtype, &n_rtitems);
	expitems = parser_getlistitems (expr, &n_expitems);

	if (n_expitems < n_rtitems) {
		typecheck_error (node, tc, "too few expressions in return");
		return 0;
	} else if (n_expitems > n_rtitems) {
		typecheck_error (node, tc, "too many expressions in return");
		return 0;
	}
	acttypelist = parser_newlistnode (NULL);

	for (i=0; i<n_rtitems; i++) {
		tnode_t *etype, *atype;

		etype = typecheck_gettype (expitems[i], rtitems[i]);
		if (!etype) {
			typecheck_error (node, tc, "failed to determine expression type for return expression %d", i+1);
			return 0;
		}
		atype = typecheck_typeactual (rtitems[i], etype, expitems[i], tc);
		if (!atype) {
			typecheck_error (node, tc, "incompatible types for return expression %d", i+1);
			return 0;
		}
		parser_addtolist (acttypelist, atype);
	}
	tnode_setnthsub (node, 1, acttypelist);

#if 1
fhandle_printf (FHAN_STDERR, "guppy_typecheck_rnode(): result-type:\n");
tnode_dumptree (rtype, 1, FHAN_STDERR);
fhandle_printf (FHAN_STDERR, "expressions:\n");
tnode_dumptree (expr, 1, FHAN_STDERR);
fhandle_printf (FHAN_STDERR, "actual-type-list:\n");
tnode_dumptree (acttypelist, 1, FHAN_STDERR);
#endif

	return 0;
}
/*}}}*/


/*{{{  static int guppy_cflow_init_nodes (void)*/
/*
 *	called to initialise parse-tree nodes for control flow structures (if/while/etc.)
 *	returns 0 on success, non-zero on failure
 */
static int guppy_cflow_init_nodes (void)
{
	tndef_t *tnd;
	int i;
	compops_t *cops;
	langops_t *lops;

	/*{{{  guppy:cflow -- IF, WHILE*/
	i = -1;
	tnd = tnode_newnodetype ("guppy:cflow", &i, 2, 0, 0, TNF_LONGPROC);	/* subnodes: 0 = expr; 1 = body */
	cops = tnode_newcompops ();
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnd->lops = lops;

	i = -1;
	gup.tag_IF = tnode_newnodetag ("IF", &i, tnd, NTF_INDENTED_PROC_LIST);
	i = -1;
	gup.tag_WHILE = tnode_newnodetag ("WHILE", &i, tnd, NTF_INDENTED_PROC_LIST);

	/*}}}*/
	/*{{{  guppy:rnode -- RETURN*/
	i = -1;
	tnd = tnode_newnodetype ("guppy:rnode", &i, 2, 0, 0, TNF_NONE);		/* subnodes: 0 = expr-list, 1 = type */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "prescope", 2, COMPOPTYPE (guppy_prescope_rnode));
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (guppy_typecheck_rnode));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnd->lops = lops;

	i = -1;
	gup.tag_RETURN = tnode_newnodetag ("RETURN", &i, tnd, NTF_NONE);

	/*}}}*/

	return 0;
}
/*}}}*/


/*{{{  guppy_cflow_feunit (feunit_t)*/
feunit_t guppy_cflow_feunit = {
	.init_nodes = guppy_cflow_init_nodes,
	.reg_reducers = NULL,
	.init_dfatrans = NULL,
	.post_setup = NULL,
	.ident = "guppy-cflow"
};
/*}}}*/

