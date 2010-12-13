/*
 *	guppy_cnode.c -- constructor nodes for Guppy
 *	Copyright (C) 2010 Fred Barnes <frmb@kent.ac.uk>
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


/*{{{  static int guppy_autoseq_cnode (compops_t *cops, tnode_t **node, guppy_autoseq_t *gas)*/
/*
 *	does auto-sequencing processing on constructor nodes (no-op)
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_autoseq_cnode (compops_t *cops, tnode_t **node, guppy_autoseq_t *gas)
{
	return 1;
}
/*}}}*/
/*{{{  static int guppy_prescope_cnode (compops_t *cops, tnode_t **node, guppy_prescope_t *ps)*/
/*
 *	does pre-scoping on a constructor node
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_prescope_cnode (compops_t *cops, tnode_t **node, guppy_prescope_t *ps)
{
	tnode_t *cnode = *node;
	tnode_t **listptr = tnode_nthsubaddr (cnode, 1);

	if (parser_countlist (*listptr) == 1) {
		/* single item SEQ or PAR, remove it */
		int nitems = 0;
		tnode_t **items = parser_getlistitems (*listptr, &nitems);

		*node = items[0];
		items[0] = NULL;

		tnode_free (cnode);

		/* do pre-scope on the result */
		prescope_subtree (node, ps);
		return 0;
	}
	return 1;		/* do items */
}
/*}}}*/


/*{{{  static int guppy_cnode_init_nodes (void)*/
/*
 *	called to initialise parse-tree nodes for constructors
 *	returns 0 on success, non-zero on failure
 */
static int guppy_cnode_init_nodes (void)
{
	tndef_t *tnd;
	int i;
	compops_t *cops;
	langops_t *lops;

	/*{{{  guppy:cnode -- SEQ, PAR*/
	i = -1;
	tnd = tnode_newnodetype ("guppy:cnode", &i, 2, 0, 0, TNF_LONGPROC);		/* subnodes: 0 = expr/operand/parspaceref; 1 = body */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "autoseq", 2, COMPOPTYPE (guppy_autoseq_cnode));
	tnode_setcompop (cops, "prescope", 2, COMPOPTYPE (guppy_prescope_cnode));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnd->lops = lops;

	i = -1;
	gup.tag_SEQ = tnode_newnodetag ("SEQ", &i, tnd, NTF_INDENTED_PROC_LIST);
	i = -1;
	gup.tag_PAR = tnode_newnodetag ("PAR", &i, tnd, NTF_INDENTED_PROC_LIST);

	/*}}}*/

	return 0;
}
/*}}}*/


/*{{{  guppy_cnode_feunit (feunit_t)*/
feunit_t guppy_cnode_feunit = {
	init_nodes: guppy_cnode_init_nodes,
	reg_reducers: NULL,
	init_dfatrans: NULL,
	post_setup: NULL,
	ident: "guppy-cnode"
};
/*}}}*/

