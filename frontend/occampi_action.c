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
#include "langops.h"
#include "precheck.h"
#include "usagecheck.h"
#include "fetrans.h"
#include "betrans.h"
#include "map.h"
#include "target.h"
#include "transputer.h"
#include "codegen.h"


/*}}}*/


/*
 *	this file contains front-end routines for handling action-nodes,
 *	e.g. assignment, input and output
 */

/*{{{  static int occampi_typecheck_action (compops_t *cops, tnode_t *node, typecheck_t *tc)*/
/*
 *	called to type-check an action-node
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_typecheck_action (compops_t *cops, tnode_t *node, typecheck_t *tc)
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
	
	/* call type-check on LHS and RHS trees */
	typecheck_subtree (lhs, tc);
	typecheck_subtree (rhs, tc);

	if (node->tag == opi.tag_ASSIGN) {
		/*{{{  assignment*/
#if 0
fprintf (stderr, "occampi_typecheck_action(): lhs = \n");
tnode_dumptree (lhs, 1, stderr);
fprintf (stderr, "occampi_typecheck_action(): rhs = \n");
tnode_dumptree (rhs, 1, stderr);
#endif
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
/*{{{  static tnode_t *occampi_gettype_action (langops_t *lops, tnode_t *node, tnode_t *default_type)*/
/*
 *	called to get the type of an action -- just returns the held type
 */
static tnode_t *occampi_gettype_action (langops_t *lops, tnode_t *node, tnode_t *default_type)
{
	return tnode_nthsubof (node, 2);
}
/*}}}*/
/*{{{  static int occampi_precheck_action (compops_t *cops, tnode_t *node)*/
/*
 *	called to do pre-checks on an action-node
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_precheck_action (compops_t *cops, tnode_t *node)
{
	if (node->tag == opi.tag_INPUT) {
		usagecheck_marknode (tnode_nthsubof (node, 0), USAGE_INPUT, 0);
		usagecheck_marknode (tnode_nthsubof (node, 1), USAGE_WRITE, 0);
	} else if (node->tag == opi.tag_OUTPUT) {
		usagecheck_marknode (tnode_nthsubof (node, 0), USAGE_OUTPUT, 0);
		usagecheck_marknode (tnode_nthsubof (node, 1), USAGE_READ, 0);
	} else if (node->tag == opi.tag_ASSIGN) {
		/* deeper usage-checking may sort these out later on */
		usagecheck_marknode (tnode_nthsubof (node, 0), USAGE_WRITE, 0);
		usagecheck_marknode (tnode_nthsubof (node, 1), USAGE_READ, 0);
	}
	return 1;
}
/*}}}*/
/*{{{  static int occampi_fetrans_action (compops_t *cops, tnode_t **node, fetrans_t *fe)*/
/*
 *	called to do front-end transforms on action nodes
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_fetrans_action (compops_t *cops, tnode_t **node, fetrans_t *fe)
{
	tnode_t *t = *node;
	tnode_t **saved_insertpoint = fe->insertpoint;

	fe->insertpoint = node;				/* before process is a good place to insert temporaries */

	if (t->tag == opi.tag_OUTPUT) {
		/*{{{  if RHS looks complex, add temporary and assignment*/
		if (langops_iscomplex (tnode_nthsubof (t, 1), 1)) {
			tnode_t *temp = fetrans_maketemp (tnode_nthsubof (t, 2), fe);

			/* now assignment.. */
			fetrans_makeseqassign (temp, tnode_nthsubof (t, 1), tnode_nthsubof (t, 2), fe);

			tnode_setnthsub (t, 1, temp);
		}
		/*}}}*/
	}

	fe->insertpoint = saved_insertpoint;

	return 1;
}
/*}}}*/
/*{{{  static int occampi_betrans_action (compops_t *cops, tnode_t **node, betrans_t *be)*/
/*
 *	called to do back-end transforms on action nodes
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_betrans_action (compops_t *cops, tnode_t **node, betrans_t *be)
{
	tnode_t *t = *node;

	if (t->tag == opi.tag_ASSIGN) {
		tnode_t *rhs = tnode_nthsubof (t, 1);
		tnode_t **lhsp = tnode_nthsubaddr (t, 0);
		int nlhs, single = 0;
		int modified = 0;

		if (!parser_islistnode (*lhsp)) {
			/* single result */
			nlhs = single = 1;
		} else {
			lhsp = parser_getlistitems (*lhsp, &nlhs);
		}

		if (rhs->tag == opi.tag_FINSTANCE) {
			/*{{{  special-case: check RHS for parameterised results*/
			tnode_t *fnamenode = tnode_nthsubof (rhs, 0);
			// name_t *fname = tnode_nthnameof (fnamenode, 0);
			tnode_t *ftype = typecheck_gettype (fnamenode, NULL); // NameTypeOf (fname);
			tnode_t **fparams, *aparams;
			int nfparams, i;

			if (!ftype || (ftype->tag != opi.tag_FUNCTIONTYPE)) {
				tnode_error (rhs, "type of function not FUNCTIONTYPE");
				return 0;
			}

			aparams = tnode_nthsubof (rhs, 1);
			fparams = parser_getlistitems (tnode_nthsubof (ftype, 1), &nfparams);

			/* look for those fparams tagged with VALOF */
			for (i=0; i<nfparams; i++) {
				int x;
				ntdef_t *tag = betrans_gettag (fparams[i], &x, be);

				if (tag) {
					/* this one was! */
					if ((x < 0) || (x >= nlhs)) {
						tnode_error (t, "occampi_betrans_action(): RHS function instance has missing result %d on LHS", x);
						return 0;
					}

					/* move it over */
#if 0
fprintf (stderr, "occampi_betrans_action(): FINSTANCE on ASSIGN, fparam %d used to be result %d.  corresponding result is:\n", i, x);
if (x < nlhs) {
	tnode_dumptree (lhsp[x], 1, stderr);
} else {
	fprintf (stderr, "    (out of range!)\n");
}
#endif
					if (single) {
						parser_addtolist (aparams, *lhsp);
						*lhsp = NULL;
						nlhs--;
					} else {
						tnode_t *lhs = parser_delfromlist (*lhsp, x);

						nlhs--;
						parser_addtolist (aparams, lhs);
					}
					modified = 1;
				}
			}

			/* did we get all of them ? */
			if (!nlhs) {
				/* nothing left on LHS, remove assignment */
				if (*lhsp) {
					tnode_free (*lhsp);
				}

				*node = tnode_nthsubof (t, 1);
				tnode_setnthsub (t, 1, NULL);

				tnode_setnthsub (t, 2, NULL); 		/* leave the type alone */

				tnode_free (t);
				modified = 1;
			}
			/*}}}*/
		}

		if (modified) {
			/* if modified, transform tree again */
			betrans_subtree (node, be);
			return 0;
		}
	}
	return 1;
}
/*}}}*/
/*{{{  static int occampi_premap_action (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	does per-mapping for an action -- turns expression nodes into RESULT nodes
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_premap_action (compops_t *cops, tnode_t **node, map_t *map)
{
	/* premap LHS and RHS */
	map_subpremap (tnode_nthsubaddr (*node, 0), map);
	map_subpremap (tnode_nthsubaddr (*node, 1), map);

	return 0;
}
/*}}}*/
/*{{{  static int occampi_namemap_action (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	allocates space necessary for an action
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_namemap_action (compops_t *cops, tnode_t **node, map_t *map)
{
	/* do left/right sides first */
	map_submapnames (tnode_nthsubaddr (*node, 0), map);
	map_submapnames (tnode_nthsubaddr (*node, 1), map);

	if (((*node)->tag == opi.tag_OUTPUT) || ((*node)->tag == opi.tag_INPUT)) {
		tnode_t *bename;

		bename = map->target->newname (*node, NULL, map, 0, map->target->bws.ds_io, 0, 0, 0, 0);
		*node = bename;
	}

	return 0;
}
/*}}}*/
/*{{{  static int occampi_codegen_action (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	generates code for an action
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_codegen_action (compops_t *cops, tnode_t *node, codegen_t *cgen)
{
	tnode_t *lhs = tnode_nthsubof (node, 0);
	tnode_t *rhs = tnode_nthsubof (node, 1);
	tnode_t *type = tnode_nthsubof (node, 2);
	int bytes = tnode_bytesfor (type, cgen->target);

	codegen_callops (cgen, debugline, node);
	/* some special cases for assignment, input and output -- these have codegen_typeaction() set in language-ops */
	if (type && type->tag->ndef->lops && type->tag->ndef->lops->codegen_typeaction) {
		int i;

		i = type->tag->ndef->lops->codegen_typeaction (type->tag->ndef->lops, type, node, cgen);
		if (i >= 0) {
			/* did something */
			return i;
		}	/* else try a normal action handling on it */
	}

	if (node->tag == opi.tag_ASSIGN) {
#if 0
fprintf (stderr, "occampi_codegen_action(): ASSIGN: bytes = %d, cgen->target->intsize = %d\n", bytes, cgen->target->intsize);
fprintf (stderr, "occampi_codegen_action(): ASSIGN: lhs =\n");
tnode_dumptree (lhs, 1, stderr);
fprintf (stderr, "occampi_codegen_action(): ASSIGN: rhs =\n");
tnode_dumptree (rhs, 1, stderr);
fprintf (stderr, "occampi_codegen_action(): ASSIGN: type =\n");
tnode_dumptree (type, 1, stderr);
#endif
		if ((bytes < 0)) {
			/* maybe need alternate code-gen for this! */
			codegen_error (cgen, "occampi_codegen_action(): unknown size for node [%s]", type->tag->name);
		} else if (bytes <= cgen->target->intsize) {
			/* simple load and store */
			codegen_callops (cgen, loadname, rhs, 0);
			codegen_callops (cgen, storename, lhs, 0);
		} else {
			/* load pointers, block move */
			codegen_callops (cgen, loadpointer, rhs, 0);
			codegen_callops (cgen, loadpointer, lhs, 0);
			codegen_callops (cgen, loadconst, bytes);
			codegen_callops (cgen, tsecondary, I_MOVE);
		}
	} else if (node->tag == opi.tag_OUTPUT) {
		/* load a pointer to value, pointer to channel, size */
		codegen_callops (cgen, loadpointer, rhs, 0);
		codegen_callops (cgen, loadpointer, lhs, 0);
		codegen_callops (cgen, loadconst, bytes);
		codegen_callops (cgen, tsecondary, I_OUT);
	} else if (node->tag == opi.tag_INPUT) {
		/* same as output really.. */
		codegen_callops (cgen, loadpointer, rhs, 0);
		codegen_callops (cgen, loadpointer, lhs, 0);
		codegen_callops (cgen, loadconst, bytes);
		codegen_callops (cgen, tsecondary, I_IN);
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
	langops_t *lops;

	/*{{{  occampi:actionnode -- ASSIGN, INPUT, OUTPUT*/
	i = -1;
	opi.node_ACTIONNODE = tnd = tnode_newnodetype ("occampi:actionnode", &i, 3, 0, 0, TNF_NONE);		/* subnodes: left, right, type */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (occampi_typecheck_action));
	tnode_setcompop (cops, "precheck", 1, COMPOPTYPE (occampi_precheck_action));
	tnode_setcompop (cops, "fetrans", 2, COMPOPTYPE (occampi_fetrans_action));
	tnode_setcompop (cops, "betrans", 2, COMPOPTYPE (occampi_betrans_action));
	tnode_setcompop (cops, "premap", 2, COMPOPTYPE (occampi_premap_action));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (occampi_namemap_action));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (occampi_codegen_action));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	lops->gettype = occampi_gettype_action;
	tnd->lops = lops;


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
/*{{{  static int occampi_action_reg_reducers (void)*/
/*
 *	registers reductions for action nodes
 */
static int occampi_action_reg_reducers (void)
{
	parser_register_grule ("opi:assignreduce", parser_decode_grule ("SN1N+N+V0C3R-", opi.tag_ASSIGN));
	parser_register_grule ("opi:outputreduce", parser_decode_grule ("SN1N+N+V0C3R-", opi.tag_OUTPUT));
	parser_register_grule ("opi:inputreduce", parser_decode_grule ("SN1N+N+V0C3R-", opi.tag_INPUT));

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
	dynarray_add (transtbl, dfa_transtotbl ("occampi:namestartname +:= [ 0 @@:= 1 ] [ 1 occampi:expr 2 ] [ 2 {<opi:assignreduce>} -* ]"));
	dynarray_add (transtbl, dfa_transtotbl ("occampi:ctclidecl ::= [ 0 occampi:namelist 1 ] [ 1 @@: 2 ] [ 2 {<opi:declreduce>} -* ]"));
	dynarray_add (transtbl, dfa_transtotbl ("occampi:ctsvrdecl ::= [ 0 occampi:namelist 1 ] [ 1 @@: 2 ] [ 2 {<opi:declreduce>} -* ]"));
	dynarray_add (transtbl, dfa_transtotbl ("occampi:namestartname +:= [ 0 @@! 1 ] [ 1 +Name 2 ] [ 1 -* 4 ] [ 2 -* 4 ] [ 2 +@@, 3 ] [ 2 +@@: 3 ] [ 3 {<parser:rewindtokens>} -* <occampi:ctclidecl> ] " \
				"[ 4 {<parser:rewindtokens>} -* 5 ] [ 5 occampi:exprsemilist 6 ] [ 6 {<opi:outputreduce>} -* ]"));
	dynarray_add (transtbl, dfa_transtotbl ("occampi:namestartname +:= [ 0 @@? 1 ] [ 1 +Name 2 ] [ 1 -* 4 ] [ 2 -* 4 ] [ 2 +@@, 3 ] [ 2 +@@: 3 ] [ 3 {<parser:rewindtokens>} -* <occampi:ctsvrdecl> ] " \
				"[ 4 {<parser:rewindtokens>} -* 5 ] [ 5 occampi:exprsemilist 6 ] [ 6 {<opi:inputreduce>} -* ]"));

	/*
	dynarray_add (transtbl, dfa_transtotbl ("occampi:namestartname +:= [ 0 @@! 1 ] [ 1 occampi:exprsemilist 2 ] [ 2 @@: 3 ] [ 3 {<opi:declreduce>} -* ] " \
				"[ 2 -* 4 ] [ 4 {<opi:outputreduce>} -* ]"));
	dynarray_add (transtbl, dfa_transtotbl ("occampi:namestartname +:= [ 0 @@? 1 ] [ 1 occampi:exprsemilist 2 ] [ 2 @@: 3 ] [ 3 {<opi:declreduce>} -* ] " \
				"[ 2 -* 4 ] [ 4 {<opi:inputreduce>} -* ]"));
	*/

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

