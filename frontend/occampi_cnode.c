/*
 *	occampi_cnode.c -- occam-pi constructor processes for NOCC  (SEQ, PAR, etc.)
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
#include "usagecheck.h"
#include "map.h"
#include "codegen.h"
#include "target.h"
#include "transputer.h"


/*}}}*/


/*{{{  static void occampi_reduce_cnode (dfastate_t *dfast, parsepriv_t *pp, void *rarg)*/
/*
 *	reduces a constructor node (e.g. SEQ, PAR, FORKING, CLAIM, ..)
 */
static void occampi_reduce_cnode (dfastate_t *dfast, parsepriv_t *pp, void *rarg)
{
	token_t *tok;
	ntdef_t *tag;

	tok = parser_gettok (pp);
	tag = tnode_lookupnodetag (tok->u.kw->name);
	*(dfast->ptr) = tnode_create (tag, tok->origin, NULL, NULL);
	lexer_freetoken (tok);

	return;
}
/*}}}*/
/*{{{  static void occampi_reduce_replcnode (dfastate_t *dfast, parsepriv_t *pp, void *rarg)*/
/*
 *	reduces a replicated constructor node (e.g. REPLSEQ, REPLPAR)
 */
static void occampi_reduce_replcnode (dfastate_t *dfast, parsepriv_t *pp, void *rarg)
{
	token_t *tok;
	ntdef_t *tag;
	tnode_t *rname, *rstart, *rlength;

	tok = parser_gettok (pp);
	tag = tnode_lookupnodetag (tok->u.kw->name);
	if (tag == opi.tag_SEQ) {
		tag = opi.tag_REPLSEQ;
	} else if (tag == opi.tag_PAR) {
		tag = opi.tag_REPLPAR;
	} else {
		parser_error (pp->lf, "occampi_reduce_replcnode(): unknown tag to replicate [%s]", tag->name);
		return;
	}
	rlength = dfa_popnode (dfast);
	rstart = dfa_popnode (dfast);
	rname = dfa_popnode (dfast);
	*(dfast->ptr) = tnode_create (tag, tok->origin, NULL, NULL, rname, rstart, rlength);
	lexer_freetoken (tok);

	return;
}
/*}}}*/


/*{{{  static int occampi_cnode_dousagecheck (tnode_t *node, uchk_state_t *ucstate)*/
/*
 *	does usage-checking for a CNODE
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_cnode_dousagecheck (tnode_t *node, uchk_state_t *ucstate)
{
	if (node->tag == opi.tag_PAR) {
		/*{{{  usage-check PAR bodies*/
		tnode_t *body = tnode_nthsubof (node, 1);
		tnode_t **bodies;
		int nbodies, i;
		tnode_t *parnode = node;

		if (!parser_islistnode (body)) {
			nocc_internal ("occampi_cnode_dousagecheck(): body of PAR not list");
			return 0;
		}

		usagecheck_begin_branches (node, ucstate);

		bodies = parser_getlistitems (body, &nbodies);
		for (i=0; i<nbodies; i++) {
			/*{{{  do usage-checks on subnode*/
			usagecheck_branch (bodies[i], ucstate);
			/*}}}*/
		}

		usagecheck_end_branches (node, ucstate);
		if (!usagecheck_no_overlaps (node, ucstate)) {
			usagecheck_mergeall (node, ucstate);
		} /* else don't merge from something that failed (XXX: maybe..) */
		/*}}}*/

		return 0;
	}
	return 1;
}
/*}}}*/
/*{{{  static int occampi_namemap_cnode (tnode_t **node, map_t *map)*/
/*
 *	does name mapping for constructor nodes (SEQ, PAR)
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_namemap_cnode (tnode_t **node, map_t *map)
{
	if ((*node)->tag == opi.tag_SEQ) {
		/* SEQ nodes don't need any special processing */
		return 1;
	} else if ((*node)->tag == opi.tag_PAR) {
		/*{{{  map PAR bodies*/
		tnode_t *body = tnode_nthsubof (*node, 1);
		tnode_t **bodies;
		int nbodies, i;
		tnode_t *parnode = *node;

		if (!parser_islistnode (body)) {
			nocc_internal ("occampi_namemap_cnode(): body of PAR not list");
			return 1;
		}

		bodies = parser_getlistitems (body, &nbodies);
		for (i=0; i<nbodies; i++) {
			/*{{{  turn body into back-end block*/
			tnode_t *blk, *parbodyspace;
			tnode_t *saved_blk = map->thisblock;
			tnode_t **saved_params = map->thisprocparams;

			blk = map->target->newblock (bodies[i], map, NULL, map->lexlevel + 1);
			map->thisblock = blk;
			map->thisprocparams = NULL;
			map->lexlevel++;

			/* map body */
			map_submapnames (&(bodies[i]), map);
			parbodyspace = map->target->newname (tnode_create (opi.tag_PARSPACE, NULL), bodies[i], map, 0, 16, 0, 0, 0, 0);	/* FIXME! */
			*(map->target->be_blockbodyaddr (blk)) = parbodyspace;

			map->lexlevel--;
			map->thisblock = saved_blk;
			map->thisprocparams = saved_params;

			/* make block node the individual PAR process */
			bodies[i] = blk;
			/*}}}*/
		}

		if (nbodies > 1) {
			tnode_t *bename, *bodyref, *blist;
			tnode_t *fename = tnode_create (opi.tag_PARSPACE, NULL);

			blist = parser_newlistnode (NULL);
			for (i=0; i<nbodies; i++) {
				parser_addtolist (blist, bodies[i]);
			}
			bodyref = map->target->newblockref (blist, *node, map);
			bename = map->target->newname (fename, bodyref, map, 12, 0, 0, 0, 0, 0);                    /* FIXME! */
			tnode_setchook (fename, map->mapchook, (void *)bename);

			*node = bename;

			tnode_setnthsub (parnode, 0, map->target->newnameref (bename, map));
		}

		/*}}}*/
	} else {
		nocc_internal ("occampi_namemap_cnode(): don\'t know how to handle tag [%s]", (*node)->tag->name);
	}
	return 0;
}
/*}}}*/
/*{{{  static int occampi_codegen_cnode (tnode_t *node, codegen_t *cgen)*/
/*
 *	does code-generation for certain conditional-nodes
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_codegen_cnode (tnode_t *node, codegen_t *cgen)
{
	if (node->tag == opi.tag_SEQ) {
		/* SEQ nodes don't need any special processing */
		return 1;
	} else if (node->tag == opi.tag_PAR) {
		/*{{{  generate code for PAR*/
		tnode_t *body = tnode_nthsubof (node, 1);
		tnode_t **bodies;
		int nbodies, i;
		int joinlab = codegen_new_label (cgen);
		int pp_wsoffs = 0;
		tnode_t *parspaceref = tnode_nthsubof (node, 0);

		bodies = parser_getlistitems (body, &nbodies);
		/*{{{  PAR setup*/
		codegen_callops (cgen, comment, "BEGIN PAR SETUP");
		for (i=0; i<nbodies; i++) {
			int ws_size, vs_size, ms_size;
			int ws_offset, adjust, elab;
			tnode_t *statics = tnode_nthsubof (bodies[i], 1);

			codegen_check_beblock (bodies[i], cgen, 1);
			cgen->target->be_getblocksize (bodies[i], &ws_size, &ws_offset, &vs_size, &ms_size, &adjust, &elab);

			/*{{{  setup statics in workspace of PAR process*/
			if (statics && parser_islistnode (statics)) {
				int nitems, p, wsoff;
				tnode_t **items = parser_getlistitems (statics, &nitems);

				for (p=nitems - 1, wsoff = pp_wsoffs-4; p>=0; p--, wsoff -= 4) {
					codegen_callops (cgen, loadparam, items[p], PARAM_REF);
					codegen_callops (cgen, storelocal, wsoff);
				}
			} else if (statics) {
				codegen_callops (cgen, loadparam, statics, PARAM_REF);
				codegen_callops (cgen, storelocal, pp_wsoffs-4);
			}
			/*}}}*/

			pp_wsoffs -= ws_size;
		}
		/*{{{  setup local PAR workspace*/
		codegen_callops (cgen, loadconst, nbodies + 1);		/* par-count */
		codegen_callops (cgen, storename, parspaceref, 4);
		codegen_callops (cgen, loadconst, 0);			/* priority */
		codegen_callops (cgen, storename, parspaceref, 8);
		codegen_callops (cgen, loadlabaddr, joinlab);		/* join-lab */
		codegen_callops (cgen, storename, parspaceref, 0);
		/*}}}*/
		pp_wsoffs = 0;
		for (i=0; i<nbodies; i++) {
			int ws_size, vs_size, ms_size;
			int ws_offset, adjust, elab;

			codegen_check_beblock (bodies[i], cgen, 1);
			cgen->target->be_getblocksize (bodies[i], &ws_size, &ws_offset, &vs_size, &ms_size, &adjust, &elab);

			/*{{{  start PAR process*/
			codegen_callops (cgen, loadlabaddr, elab);
			codegen_callops (cgen, loadlocalpointer, pp_wsoffs - adjust);
			codegen_callops (cgen, tsecondary, I_STARTP);
			/*}}}*/

			pp_wsoffs -= ws_size;
		}
		codegen_callops (cgen, comment, "END PAR SETUP");
		/*}}}*/
		/*{{{  end process doing PAR*/
		codegen_callops (cgen, loadpointer, parspaceref, 0);
		codegen_callops (cgen, tsecondary, I_ENDP);
		/*}}}*/
		pp_wsoffs = 0;
		for (i=0; i<nbodies; i++) {
			/*{{{  PAR body*/
			int ws_size, vs_size, ms_size;
			int ws_offset, adjust, elab;

			cgen->target->be_getblocksize (bodies[i], &ws_size, &ws_offset, &vs_size, &ms_size, &adjust, &elab);
			codegen_callops (cgen, comment, "PAR = %d,%d,%d,%d,%d", ws_size, ws_offset, vs_size, ms_size, adjust);

			codegen_subcodegen (bodies[i], cgen);

			codegen_callops (cgen, loadpointer, parspaceref, pp_wsoffs + adjust);
			codegen_callops (cgen, tsecondary, I_ENDP);
			/*}}}*/

			pp_wsoffs += ws_size;
		}
		/*}}}*/
		/*{{{  PAR cleanup*/
		codegen_callops (cgen, setlabel, joinlab);
		/*}}}*/
	} else {
		nocc_internal ("occampi_codegen_cnode(): don\'t know how to handle tag [%s]", node->tag->name);
	}
	return 0;
}
/*}}}*/


/*{{{  static int occampi_replcnode_dousagecheck (tnode_t *node, uchk_state_t *ucstate)*/
/*
 *	does usage-checking for a replicated constructor-node (REPLSEQ, REPLPAR)
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_replcnode_dousagecheck (tnode_t *node, uchk_state_t *ucstate)
{
	return 1;
}
/*}}}*/
/*{{{  static int occampi_scopein_replcnode (tnode_t **node, scope_t *ss)*/
/*
 *	scopes in a replicated constructor node
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_scopein_replcnode (tnode_t **node, scope_t *ss)
{
	return 1;
}
/*}}}*/
/*{{{  static int occampi_scopeout_replcnode (tnode_t **node, scope_t *ss)*/
/*
 *	scopes out a replicated constructor node
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_scopeout_replcnode (tnode_t **node, scope_t *ss)
{
	return 1;
}
/*}}}*/
/*{{{  static int occampi_namemap_replcnode (tnode_t **node, map_t *map)*/
/*
 *	does name-mapping for replicated constructor nodes (REPLSEQ, REPLPAR)
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_namemap_replcnode (tnode_t **node, map_t *map)
{
	return 1;
}
/*}}}*/
/*{{{  static int occampi_codegen_replcnode (tnode_t *node, codegen_t *cgen)*/
/*
 *	does code-generation for replicated constructor nodes (REPLSEQ, REPLPAR)
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_codegen_replcnode (tnode_t *node, codegen_t *cgen)
{
	return 1;
}
/*}}}*/


/*{{{  static int occampi_typecheck_cexpnode (tnode_t *node, typecheck_t *tc)*/
/*
 *	does type-checking for a cexpnode
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_typecheck_cexpnode (tnode_t *node, typecheck_t *tc)
{
	tnode_t *littype;

	typecheck_subtree (tnode_nthsubof (node, 0), tc);

	littype = typecheck_gettype (tnode_nthsubof (node, 0), NULL);
	if (!littype || (littype->tag != opi.tag_BOOL)) {
		typecheck_error (node, tc, "boolean expression expected");
	}

	return 1;
}
/*}}}*/
/*{{{  static int occampi_namemap_cexpnode (tnode_t **node, map_t *map)*/
/*
 *	does name-mapping for a cexpnode
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_namemap_cexpnode (tnode_t **node, map_t *map)
{
	if ((*node)->tag == opi.tag_SHORTIF) {
		/* no special processing for short IFs */
	} else if ((*node)->tag == opi.tag_WHILE) {
		/* no special processing for WHILEs either */
	} else {
		nocc_internal ("occampi_namemap_cexpnode(): don\'t know how to handle tag [%s]", (*node)->tag->name);
	}
	return 1;
}
/*}}}*/
/*{{{  static int occampi_codegen_cexpnode (tnode_t *node, codegen_t *cgen)*/
/*
 *	does code-gen for a cexpnode
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_codegen_cexpnode (tnode_t *node, codegen_t *cgen)
{
	if (node->tag == opi.tag_SHORTIF) {
		/*{{{  generate code for a short IF*/
		int joinlab = codegen_new_label (cgen);
		tnode_t *cond = tnode_nthsubof (node, 0);
		tnode_t *body = tnode_nthsubof (node, 1);

		codegen_callops (cgen, loadname, cond, 0);
		codegen_callops (cgen, branch, I_CJ, joinlab);
		codegen_subcodegen (body, cgen);
		codegen_callops (cgen, setlabel, joinlab);
		/*}}}*/
	} else if (node->tag == opi.tag_WHILE) {
		int looplab = codegen_new_label (cgen);
		int exitlab = codegen_new_label (cgen);
		tnode_t *cond = tnode_nthsubof (node, 0);
		tnode_t *body = tnode_nthsubof (node, 1);

		codegen_callops (cgen, setlabel, looplab);
		codegen_callops (cgen, loadname, cond, 0);
		codegen_callops (cgen, branch, I_CJ, exitlab);

		codegen_subcodegen (body, cgen);
		codegen_callops (cgen, branch, I_J, looplab);
		codegen_callops (cgen, setlabel, exitlab);
	} else {
		nocc_internal ("occampi_codegen_cexpnode(): don\'t know how to handle tag [%s]", node->tag->name);
	}
	return 0;
}
/*}}}*/


/*{{{  static int occampi_cnode_init_nodes (void)*/
/*
 *	initialises constructor-process nodes for occam-pi
 *	return 0 on success, non-zero on failure
 */
static int occampi_cnode_init_nodes (void)
{
	tndef_t *tnd;
	int i;
	compops_t *cops;
	langops_t *lops;

	/*{{{  occampi:cnode -- SEQ, PAR*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:cnode", &i, 2, 0, 0, TNF_LONGPROC);		/* subnodes: 0 = expr/operand/parspaceref; 1 = body */
	cops = tnode_newcompops ();
	cops->namemap = occampi_namemap_cnode;
	cops->codegen = occampi_codegen_cnode;
	tnd->ops = cops;
	lops = tnode_newlangops ();
	lops->do_usagecheck = occampi_cnode_dousagecheck;
	tnd->lops = lops;

	i = -1;
	opi.tag_SEQ = tnode_newnodetag ("SEQ", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_PAR = tnode_newnodetag ("PAR", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  occampi:replcnode -- REPLSEQ, REPLPAR*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:replcnode", &i, 5, 0, 0, TNF_LONGPROC);		/* subnodes: 0 = expr/operand/parspaceref; 1 = body; 2 = name; 3 = start; 4 = length */
	cops = tnode_newcompops ();
	cops->scopein = occampi_scopein_replcnode;
	cops->scopeout = occampi_scopeout_replcnode;
	cops->namemap = occampi_namemap_replcnode;
	cops->codegen = occampi_codegen_replcnode;
	tnd->ops = cops;
	lops = tnode_newlangops ();
	lops->do_usagecheck = occampi_replcnode_dousagecheck;
	tnd->lops = lops;

	i = -1;
	opi.tag_REPLSEQ = tnode_newnodetag ("REPLSEQ", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_REPLPAR = tnode_newnodetag ("REPLPAR", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  occampi:cexpnode -- SHORTIF, WHILE*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:cexpnode", &i, 2, 0, 0, TNF_LONGPROC);	/* subnodes: 0 = expr; 1 = body */
	cops = tnode_newcompops ();
	cops->typecheck = occampi_typecheck_cexpnode;
	cops->namemap = occampi_namemap_cexpnode;
	cops->codegen = occampi_codegen_cexpnode;
	tnd->ops = cops;

	i = -1;
	opi.tag_WHILE = tnode_newnodetag ("WHILE", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_SHORTIF = tnode_newnodetag ("SHORTIF", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  occampi:leafnode -- PARSPACE*/
	tnd = tnode_lookupnodetype ("occampi:leafnode");

	i = -1;
	opi.tag_PARSPACE = tnode_newnodetag ("PARSPACE", &i, tnd, NTF_NONE);

	/*}}}*/

	return 0;
}
/*}}}*/
/*{{{  static int occampi_cnode_reg_reducers (void)*/
/*
 *	registers reducers for constructor-process nodes
 *	return 0 on success, non-zero on failure
 */
static int occampi_cnode_reg_reducers (void)
{
	parser_register_reduce ("Roccampi:cnode", occampi_reduce_cnode, NULL);
	parser_register_reduce ("Roccampi:replcnode", occampi_reduce_replcnode, NULL);

	parser_register_grule ("opi:shortif", parser_decode_grule ("T+@tSN0N+0C2R-", opi.tag_SHORTIF));
	parser_register_grule ("opi:while", parser_decode_grule ("SN0N+0C2R-", opi.tag_WHILE));
	parser_register_grule ("opi:ifstart", parser_decode_grule ("ST0T+@t000C3R-", opi.tag_IF));

	return 0;
}
/*}}}*/
/*{{{  static dfattbl_t **occampi_cnode_init_dfatrans (int *ntrans)*/
/*
 *	creates and returns DFA transition tables for constructor-process nodes
 */
static dfattbl_t **occampi_cnode_init_dfatrans (int *ntrans)
{
	DYNARRAY (dfattbl_t *, transtbl);

	dynarray_init (transtbl);
	dynarray_add (transtbl, dfa_transtotbl ("occampi:replcproc ::= [ 0 occampi:name 1 ] [ 1 @@= 2 ] [ 2 occampi:expr 3 ] [ 3 @FOR 4 ] [ 4 occampi:expr 5 ] [ 5 {Roccampi:replcnode} -* ]"));
	dynarray_add (transtbl, dfa_transtotbl ("occampi:cproc ::= [ 0 +@SEQ 1 ] [ 0 +@PAR 1 ] [ 1 -Newline 2 ] [ 1 -Name <occampi:replcproc> ] [ 2 {Roccampi:cnode} -* ]"));
	dynarray_add (transtbl, dfa_transtotbl ("occampi:cproc +:= [ 0 +@IF 1 ] [ 1 -Newline 2 ] [ 1 %occampi:expr 3 ] [ 2 {<opi:ifstart>} -* ] " \
				"[ 3 occampi:expr 4 ] [ 4 -Newline 5 ] [ 5 {<opi:shortif>} -* ]"));
	dynarray_add (transtbl, dfa_transtotbl ("occampi:cproc +:= [ 0 @WHILE 1 ] [ 1 occampi:expr 2 ] [ 2 {<opi:while>} -* ]"));

	*ntrans = DA_CUR (transtbl);
	return DA_PTR (transtbl);
}
/*}}}*/


/*{{{  occampi_cnode_feunit (feunit_t)*/
feunit_t occampi_cnode_feunit = {
	init_nodes: occampi_cnode_init_nodes,
	reg_reducers: occampi_cnode_reg_reducers,
	init_dfatrans: occampi_cnode_init_dfatrans,
	post_setup: NULL
};
/*}}}*/

