/*
 *	guppy_cnode.c -- constructor nodes for Guppy
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


/*{{{  static int guppy_prescope_cnode (compops_t *cops, tnode_t **node, guppy_prescope_t *ps)*/
/*
 *	does pre-scoping on a constructor node
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_prescope_cnode (compops_t *cops, tnode_t **node, guppy_prescope_t *ps)
{
	return 1;
}
/*}}}*/
/*{{{  static int guppy_declify_cnode (compops_t *cops, tnode_t **nodeptr, guppy_declify_t *gdl)*/
/*
 *	does declify on constructor nodes
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_declify_cnode (compops_t *cops, tnode_t **nodeptr, guppy_declify_t *gdl)
{
	tnode_t *node = *nodeptr;
	tnode_t **bptr = tnode_nthsubaddr (node, 1);

	if (parser_islistnode (*bptr)) {
		if (node->tag == gup.tag_PAR) {
			/* need to be slightly careful: so declarations and processes don't get auto-seq'd */
			guppy_declify_listtodecllist_single (bptr, gdl);
		} else {
			guppy_declify_listtodecllist (bptr, gdl);
		}
	}
	return 0;
}
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
/*{{{  static int guppy_flattenseq_cnode (compops_t *cops, tnode_t **nodeptr)*/
/*
 *	does flattening on a constructor node
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_flattenseq_cnode (compops_t *cops, tnode_t **nodeptr)
{
	tnode_t *node = *nodeptr;
	tnode_t **listptr = tnode_nthsubaddr (node, 1);

	if (!parser_islistnode (*listptr)) {
		/* not a problem in practice, just means we were probably here before! */
		// nocc_serious ("guppy_flattenseq_cnode(): node body is not a list..");
		// return 0;
	} else if (parser_countlist (*listptr) == 1) {
		/* single item, remove it and replace */
		tnode_t *item = parser_delfromlist (*listptr, 0);

		tnode_free (node);
		*nodeptr = item;
	} else if (parser_countlist (*listptr) == 0) {
		/* no items, replace with 'skip' */
		tnode_t *item = tnode_createfrom (gup.tag_SKIP, node);

		tnode_free (node);
		*nodeptr = item;
	}
	return 1;
}
/*}}}*/


/*{{{  static int guppy_scopein_replcnode (compops_t *cops, tnode_t **nodep, scope_t *ss)*/
/*
 *	does scope-in for a replicated constructor node.
 *	returns 0 to stop walk, 1 to continue.
 */
static int guppy_scopein_replcnode (compops_t *cops, tnode_t **nodep, scope_t *ss)
{
	tnode_t **bodyp = tnode_nthsubaddr (*nodep, 1);
	tnode_t **rnamep = tnode_nthsubaddr (*nodep, 2);
	tnode_t *rname = *rnamep;
	tnode_t **rstartp = tnode_nthsubaddr (*nodep, 3);
	tnode_t **rcountp = tnode_nthsubaddr (*nodep, 4);
	void *nsmark;

	if (*rstartp) {
		scope_subtree (rstartp, ss);
	}
	if (*rcountp) {
		scope_subtree (rcountp, ss);
	}

	nsmark = name_markscope ();

	if (rname) {
		char *rawname;
		name_t *repname;
		tnode_t *type, *newname;

		/* only if we have a name for this */
		if (rname->tag != gup.tag_NAME) {
			scope_error (*nodep, ss, "replicator name not raw-name, found [%s:%s]", rname->tag->ndef->name, rname->tag->name);
			return 0;
		}
		rawname = (char *)tnode_nthhookof (rname, 0);

		type = guppy_newprimtype (gup.tag_INT, rname, 0);
		repname = name_addscopename (rawname, *nodep, type, NULL);
		newname = tnode_createfrom (gup.tag_NREPL, rname, repname);
		SetNameNode (repname, newname);

		*rnamep = newname;
		tnode_free (rname);
		rname = NULL;

		ss->scoped++;
	}

	/* scope-in body */
	if (*bodyp) {
		scope_subtree (bodyp, ss);
	}

	name_markdescope (nsmark);

	return 0;
}
/*}}}*/
/*{{{  static int guppy_scopeout_replcnode (compops_t *cops, tnode_t **nodep, scope_t *ss)*/
/*
 *	does scope-out for a replicated constructor node (no-op).
 *	return value meaningless (post-walk).
 */
static int guppy_scopeout_replcnode (compops_t *cops, tnode_t **nodep, scope_t *ss)
{
	return 1;
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
	tnode_setcompop (cops, "prescope", 2, COMPOPTYPE (guppy_prescope_cnode));
	tnode_setcompop (cops, "declify", 2, COMPOPTYPE (guppy_declify_cnode));
	tnode_setcompop (cops, "autoseq", 2, COMPOPTYPE (guppy_autoseq_cnode));
	tnode_setcompop (cops, "flattenseq", 1, COMPOPTYPE (guppy_flattenseq_cnode));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnd->lops = lops;

	i = -1;
	gup.tag_SEQ = tnode_newnodetag ("SEQ", &i, tnd, NTF_INDENTED_PROC_LIST);
	i = -1;
	gup.tag_PAR = tnode_newnodetag ("PAR", &i, tnd, NTF_INDENTED_PROC_LIST);

	/*}}}*/
	/*{{{  guppy:replcnode -- REPLSEQ, REPLPAR*/
	i = -1;
	tnd = tnode_newnodetype ("guppy:replcnode", &i, 5, 0, 0, TNF_LONGPROC);		/* subnodes: 0 = expr/operand/parspaceref; 1 = body; 2 = repl-name; 3 = start-expr; 4 = count-expr */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (guppy_scopein_replcnode));
	tnode_setcompop (cops, "scopeout", 2, COMPOPTYPE (guppy_scopeout_replcnode));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnd->lops = lops;

	i = -1;
	gup.tag_REPLSEQ = tnode_newnodetag ("REPLSEQ", &i, tnd, NTF_INDENTED_PROC_LIST);
	i = -1;
	gup.tag_REPLPAR = tnode_newnodetag ("REPLPAR", &i, tnd, NTF_INDENTED_PROC_LIST);
	/*}}}*/

	return 0;
}
/*}}}*/


/*{{{  guppy_cnode_feunit (feunit_t)*/
feunit_t guppy_cnode_feunit = {
	.init_nodes = guppy_cnode_init_nodes,
	.reg_reducers = NULL,
	.init_dfatrans = NULL,
	.post_setup = NULL,
	.ident = "guppy-cnode"
};
/*}}}*/

