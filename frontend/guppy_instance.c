/*
 *	guppy_instance.c -- procedure and function instances for Guppy
 *	Copyright (C) 2013 Fred Barnes <frmb@kent.ac.uk>
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
#include "cccsp.h"


/*}}}*/
/*{{{  private types*/

/*}}}*/


/*{{{  static int guppy_scopein_instance (compops_t *cops, tnode_t **nodep, scope_t *ss)*/
/*
 *	called to scope-in a function instance
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_scopein_instance (compops_t *cops, tnode_t **nodep, scope_t *ss)
{
	return 1;
}
/*}}}*/
/*{{{  static int guppy_typecheck_instance (compops_t *cops, tnode_t *node, typecheck_t *tc)*/
/*
 *	called to do type-checking on a function instance
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_typecheck_instance (compops_t *cops, tnode_t *node, typecheck_t *tc)
{
	tnode_t *fparamlist = typecheck_gettype (tnode_nthsubof (node, 0), NULL);
	tnode_t *aparamlist = tnode_nthsubof (node, 1);
	tnode_t **fp_items, **ap_items;
	int fp_nitems, ap_nitems;
	int i;

#if 1
fhandle_printf (FHAN_STDERR, "guppy_typecheck_instance(): instance of:\n");
tnode_dumptree (node, 1, FHAN_STDERR);
#endif

	return 0;
}
/*}}}*/
/*{{{  static int guppy_namemap_instance (compops_t *cops, tnode_t **nodep, map_t *map)*/
/*
 *	does name-mapping for an instance node
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_namemap_instance (compops_t *cops, tnode_t **nodep, map_t *map)
{
	tnode_t **nameptr = tnode_nthsubaddr (*nodep, 0);
	tnode_t **paramsptr = tnode_nthsubaddr (*nodep, 1);

	/* map parameters and name */
	map_submapnames (paramsptr, map);
	map_submapnames (nameptr, map);

	return 0;
}
/*}}}*/
/*{{{  static int guppy_codegen_instance (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	does code-generation for an instance node
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_codegen_instance (compops_t *cops, tnode_t *node, codegen_t *cgen)
{
	if (node->tag == gup.tag_INSTANCE) {
		name_t *pname = tnode_nthnameof (tnode_nthsubof (node, 0), 0);

		codegen_callops (cgen, c_proccall, NameNameOf (pname), tnode_nthsubof (node, 1), 0);
	} else if (node->tag == gup.tag_APICALL) {
		tnode_t *callnum = tnode_nthsubof (node, 0);

		if (!constprop_isconst (callnum)) {
			/* unexpected! */
			nocc_internal ("guppy_codegen_instance(): APICALL but name was [%s]", callnum->tag->name);
			return 0;
		}
		codegen_callops (cgen, c_proccall, NULL, tnode_nthsubof (node, 1), constprop_intvalof (callnum));
	} else {
		nocc_internal ("guppy_codegen_instance(): unhandled [%s:%s]", node->tag->ndef->name, node->tag->name);
		return 0;
	}
	return 0;
}
/*}}}*/


/*{{{  static int guppy_instance_init_nodes (void)*/
/*
 *	sets up function instance nodes for Guppy
 *	returns 0 on success, non-zero on error
 */
static int guppy_instance_init_nodes (void)
{
	tndef_t *tnd;
	compops_t *cops;
	langops_t *lops;
	int i;

	/*{{{  guppy:instance -- INSTANCE, APICALL*/
	i = -1;
	tnd = tnode_newnodetype ("guppy:instance", &i, 2, 0, 0, TNF_NONE);		/* subnodes: name, aparams */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (guppy_scopein_instance));
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (guppy_typecheck_instance));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (guppy_namemap_instance));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (guppy_codegen_instance));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnd->lops = lops;

	i = -1;
	gup.tag_INSTANCE = tnode_newnodetag ("INSTANCE", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_APICALL = tnode_newnodetag ("APICALL", &i, tnd, NTF_NONE);		/* only after "namemap" */

	/*}}}*/

	return 0;
}
/*}}}*/
/*{{{  static int guppy_instance_post_setup (void)*/
/*
 *	does post-setup for function instance nodes
 *	returns 0 on success, non-zero on error
 */
static int guppy_instance_post_setup (void)
{
	return 0;
}
/*}}}*/


/*{{{  fuppy_instance_feunit (feunit_t)*/
feunit_t guppy_instance_feunit = {
	.init_nodes = guppy_instance_init_nodes,
	.reg_reducers = NULL,
	.init_dfatrans = NULL,
	.post_setup = guppy_instance_post_setup,
	.ident = "guppy-instance",
};

/*}}}*/

