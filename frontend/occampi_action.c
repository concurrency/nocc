/*
 *	occampi_action.c -- occam-pi action handling for NOCC
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
#include "typecheck.h"
#include "map.h"
#include "target.h"
#include "transputer.h"
#include "codegen.h"


/*}}}*/


/*
 *	this file contains front-end routines for handling action-nodes,
 *	e.g. assignment, input and output
 */

/*{{{  static int occampi_typecheck_action (tnode_t *node, typecheck_t *tc)*/
/*
 *	called to type-check an action-node
 */
static int occampi_typecheck_action (tnode_t *node, typecheck_t *tc)
{
	/* FIXME: to handle multiple-assignment more transparently ? */
	tnode_t *lhs = tnode_nthsubof (node, 0);
	tnode_t *rhs = tnode_nthsubof (node, 1);
	tnode_t *acttype = tnode_nthsubof (node, 2);
	tnode_t *lhstype, *rhstype;

#if 0
fprintf (stderr, "occampi_typecheck_action(): here!\n");
#endif
	if (acttype) {
		nocc_warning ("occampi_typecheck_action(): strange.  already type-checked this action..");
		return 0;		/* don't walk sub-nodes */
	}

	if (node->tag == opi.tag_ASSIGN) {
		/*{{{  assignment*/
		lhstype = typecheck_gettype (lhs, NULL);
		rhstype = typecheck_gettype (rhs, lhstype);
		/*}}}*/
	} else {
		/*{{{  other action -- communication*/
		tnode_t *prot;

		lhstype = typecheck_gettype (lhs, NULL);

		/* expecting a channel */
		if (lhstype->tag != opi.tag_CHAN) {
			typecheck_error (node, tc, "LHS is not a channel");
			return 0;
		}

		/* get the type of the channel (channel protocol) */
		prot = typecheck_gettype (lhstype, NULL);

		rhstype = typecheck_gettype (rhs, prot);
		/*}}}*/
	}

#if 0
fprintf (stderr, "occampi_typecheck_action(): lhstype = \n");
tnode_dumptree (lhstype, 1, stderr);
fprintf (stderr, "occampi_typecheck_action(): rhstype = \n");
tnode_dumptree (rhstype, 1, stderr);
#endif

	/* got two valid types, check that the RHS type is good for the LHS */
	acttype = typecheck_typeactual (lhstype, rhstype, node, tc);
	if (!acttype) {
		typecheck_error (node, tc, "incompatible types");
		return 0;
	} else {
		tnode_setnthsub (node, 2, acttype);
	}

	return 0;	/* don't walk sub-nodes */
}
/*}}}*/
/*{{{  static int occampi_namemap_action (tnode_t **node, map_t *map)*/
/*
 *	allocates space necessary for an action
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_namemap_action (tnode_t **node, map_t *map)
{
	/* do left/right sides first */
	map_submapnames (tnode_nthsubaddr (*node, 0), map);
	map_submapnames (tnode_nthsubaddr (*node, 1), map);

	if (((*node)->tag == opi.tag_OUTPUT) || ((*node)->tag == opi.tag_INPUT)) {
		tnode_t *bename;

		bename = map->target->newname (*node, NULL, map, 0, 16, 0, 0, 0, 0);                    /* FIXME! */

		*node = bename;
	}

	return 0;
}
/*}}}*/
/*{{{  static int occampi_codegen_action (tnode_t *node, codegen_t *cgen)*/
/*
 *	generates code for an action
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_codegen_action (tnode_t *node, codegen_t *cgen)
{
	tnode_t *lhs = tnode_nthsubof (node, 0);
	tnode_t *rhs = tnode_nthsubof (node, 1);
	tnode_t *type = tnode_nthsubof (node, 2);
	int bytes = tnode_bytesfor (type);

	if (node->tag == opi.tag_ASSIGN) {
		if (bytes <= cgen->target->intsize) {
			/* simple load and store */
			codegen_callops (cgen, loadname, rhs);
			codegen_callops (cgen, storename, lhs);
		} else {
			/* load pointers, block move */
			/* codegen_callops (cgen, loadpointer, rhs);
			codegen_callops (cgen, loadpointer, lhs); */
			codegen_callops (cgen, comment, "FIXME!");
		}
	} else if (node->tag == opi.tag_OUTPUT) {
		/* load a pointer to value, pointer to channel, size */
		codegen_callops (cgen, loadpointer, rhs);
		codegen_callops (cgen, loadpointer, lhs);
		codegen_callops (cgen, loadconst, bytes);
		codegen_callops (cgen, tsecondary, I_OUT);
	} else {
		codegen_callops (cgen, comment, "FIXME!");
	}

	return 0;
}
/*}}}*/


/*{{{  static int occampi_action_init_nodes (void)*/
/*
 *	initialises nodes for occam-pi actions
 */
static int occampi_action_init_nodes (void)
{
	tndef_t *tnd;
	int i;
	compops_t *cops;

	/*{{{  occampi:actionnode -- ASSIGN, INPUT, OUTPUT*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:actionnode", &i, 3, 0, 0, TNF_NONE);
	cops = tnode_newcompops ();
	cops->typecheck = occampi_typecheck_action;
	cops->namemap = occampi_namemap_action;
	cops->codegen = occampi_codegen_action;
	tnd->ops = cops;

	i = -1;
	opi.tag_ASSIGN = tnode_newnodetag ("ASSIGN", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_INPUT = tnode_newnodetag ("INPUT", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_OUTPUT = tnode_newnodetag ("OUTPUT", &i, tnd, NTF_NONE);
	/*}}}*/

	return 0;
}
/*}}}*/
/*{{{  */
/*
 *	registers reductions for action nodes
 */
static int occampi_action_reg_reducers (void)
{
	/* FIXME: ... */
	return 0;
}
/*}}}*/
/*{{{  static dfattbl_t **occampi_action_init_dfatrans (int *ntrans)*/
/*
 *	creates and returns DFA transition tables for action nodes
 */
static dfattbl_t **occampi_action_init_dfatrans (int *ntrans)
{
	DYNARRAY (dfattbl_t *, transtbl);

	dynarray_init (transtbl);
	/* FIXME: ... */

	*ntrans = DA_CUR (transtbl);
	return DA_PTR (transtbl);
}
/*}}}*/


/*{{{  occampi_action_feunit (feunit_t)*/
feunit_t occampi_action_feunit = {
	init_nodes: occampi_action_init_nodes,
	reg_reducers: occampi_action_reg_reducers,
	init_dfatrans: occampi_action_init_dfatrans,
	post_setup: NULL
};
/*}}}*/

