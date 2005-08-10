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
#include "map.h"
#include "codegen.h"
#include "target.h"


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


/*{{{  static int occampi_namemap_cnode (tnode_t **node, map_t *map)*/
/*
 *	does name mapping for certain conditional-nodes
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

		if (!parser_islistnode (body)) {
			nocc_internal ("occampi_namemap_cnode(): body of PAR not list");
			return 1;
		}

		bodies = parser_getlistitems (body, &nbodies);
		for (i=0; i<nbodies; i++) {
			/*{{{  turn body into back-end block*/
			tnode_t *blk;
			tnode_t *saved_blk = map->thisblock;
			tnode_t **saved_params = map->thisprocparams;

			blk = map->target->newblock (bodies[i], map, NULL, map->lexlevel + 1);
			map->thisblock = blk;
			map->thisprocparams = NULL;
			map->lexlevel++;

			/* map body */
			map_submapnames (&(bodies[i]), map);
			*(map->target->be_blockbodyaddr (blk)) = bodies[i];

			map->lexlevel--;
			map->thisblock = saved_blk;
			map->thisprocparams = saved_params;

			/* make block node the individual PAR process */
			bodies[i] = blk;
			/*}}}*/
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

		/* FIXME: PAR seutp */
		bodies = parser_getlistitems (body, &nbodies);
		for (i=0; i<nbodies; i++) {
			int ws_size, vs_size, ms_size;
			int ws_offset, adjust;

			cgen->target->be_getblocksize (bodies[i], &ws_size, &ws_offset, &vs_size, &ms_size, &adjust);

			codegen_callops (cgen, comment, "PAR = %d,%d,%d,%d,%d", ws_size, ws_offset, vs_size, ms_size, adjust);

			codegen_subcodegen (bodies[i], cgen);

			/* FIXME: PAR end */
		}
		/* FIXME: PAR cleanup */
		/*}}}*/
	} else {
		nocc_internal ("occampi_codegen_cnode(): don\'t know how to handle tag [%s]", node->tag->name);
	}
	return 0;
}
/*}}}*/


/*{{{  static int occampi_cnode_init_nodes (void)*/
/*
 *	initialises literal-nodes for occam-pi
 *	return 0 on success, non-zero on failure
 */
static int occampi_cnode_init_nodes (void)
{
	tndef_t *tnd;
	int i;
	compops_t *cops;

	/*{{{  occampi:cnode -- SEQ, PAR*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:cnode", &i, 2, 0, 0, TNF_LONGPROC);
	cops = tnode_newcompops ();
	cops->namemap = occampi_namemap_cnode;
	cops->codegen = occampi_codegen_cnode;
	tnd->ops = cops;
	i = -1;
	opi.tag_SEQ = tnode_newnodetag ("SEQ", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_PAR = tnode_newnodetag ("PAR", &i, tnd, NTF_NONE);
	/*}}}*/

	return 0;
}
/*}}}*/
/*{{{  static int occampi_cnode_reg_reducers (void)*/
/*
 *	registers reducers for literal nodes
 */
static int occampi_cnode_reg_reducers (void)
{
	parser_register_reduce ("Roccampi:cnode", occampi_reduce_cnode, NULL);

	return 0;
}
/*}}}*/
/*{{{  static dfattbl_t **occampi_cnode_init_dfatrans (int *ntrans)*/
/*
 *	creates and returns DFA transition tables for literal nodes
 */
static dfattbl_t **occampi_cnode_init_dfatrans (int *ntrans)
{
	DYNARRAY (dfattbl_t *, transtbl);

	dynarray_init (transtbl);
	dynarray_add (transtbl, dfa_bnftotbl ("occampi:cproc ::= ( +@SEQ | +@PAR ) -Newline {Roccampi:cnode}"));
	dynarray_add (transtbl, dfa_bnftotbl ("occampi:cproc +:= +@IF -Newline {Roccampi:cnode}"));

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

