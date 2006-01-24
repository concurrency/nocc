/*
 *	occampi_primproc.c -- occam-pi primitive processes for NOCC
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
#include "transputer.h"


/*}}}*/


/*{{{  static void occampi_reduce_primproc (dfastate_t *dfast, parsepriv_t *pp, void *rarg)*/
/*
 *	reduces a primitive process
 */
static void occampi_reduce_primproc (dfastate_t *dfast, parsepriv_t *pp, void *rarg)
{
	token_t *tok;
	ntdef_t *tag;

	tok = parser_gettok (pp);
	tag = tnode_lookupnodetag (tok->u.kw->name);
	*(dfast->ptr) = tnode_create (tag, tok->origin);
	lexer_freetoken (tok);

	return;
}
/*}}}*/


/*{{{  static int occampi_namemap_leafnode (tnode_t **nodep, map_t *mapdata)*/
/*
 *	called to do name-mapping on a primitive process
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_namemap_leafnode (tnode_t **nodep, map_t *mapdata)
{
	if ((*nodep)->tag == opi.tag_STOP) {
		tnode_t *bename;

		bename = mapdata->target->newname (*nodep, NULL, mapdata, 0, mapdata->target->bws.ds_min, 0, 0, 0, 0);
		*nodep = bename;
	}
	return 0;
}
/*}}}*/
/*{{{  static int occampi_codegen_leafnode (tnode_t *node, codegen_t *cgen)*/
/*
 *	called to do code-generation for a primitive process
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_codegen_leafnode (tnode_t *node, codegen_t *cgen)
{
	if (node->tag == opi.tag_STOP) {
		codegen_callops (cgen, debugline, node);
		codegen_callops (cgen, tsecondary, I_SETERR);
	}
	return 0;
}
/*}}}*/


/*{{{  static int occampi_primproc_init_nodes (void)*/
/*
 *	initialises literal-nodes for occam-pi
 *	return 0 on success, non-zero on failure
 */
static int occampi_primproc_init_nodes (void)
{
	tndef_t *tnd;
	compops_t *cops;
	int i;

	/*{{{  occampi:leafnode -- SKIP, STOP*/
	i = -1;
	tnd = opi.node_LEAFNODE = tnode_newnodetype ("occampi:leafnode", &i, 0, 0, 0, TNF_NONE);
	cops = tnode_newcompops ();
	cops->namemap = occampi_namemap_leafnode;
	cops->codegen = occampi_codegen_leafnode;
	tnd->ops = cops;


	i = -1;
	opi.tag_SKIP = tnode_newnodetag ("SKIP", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_STOP = tnode_newnodetag ("STOP", &i, tnd, NTF_NONE);
	/*}}}*/

	return 0;
}
/*}}}*/
/*{{{  static int occampi_primproc_reg_reducers (void)*/
/*
 *	registers reducers for literal nodes
 */
static int occampi_primproc_reg_reducers (void)
{
	parser_register_reduce ("Roccampi:primproc", occampi_reduce_primproc, NULL);

	return 0;
}
/*}}}*/
/*{{{  static dfattbl_t **occampi_primproc_init_dfatrans (int *ntrans)*/
/*
 *	creates and returns DFA transition tables for literal nodes
 */
static dfattbl_t **occampi_primproc_init_dfatrans (int *ntrans)
{
	DYNARRAY (dfattbl_t *, transtbl);

	dynarray_init (transtbl);
	dynarray_add (transtbl, dfa_bnftotbl ("occampi:primproc ::= ( +@SKIP | +@STOP ) {Roccampi:primproc}"));

	*ntrans = DA_CUR (transtbl);
	return DA_PTR (transtbl);
}
/*}}}*/


/*{{{  occampi_primproc_feunit (feunit_t)*/
feunit_t occampi_primproc_feunit = {
	init_nodes: occampi_primproc_init_nodes,
	reg_reducers: occampi_primproc_reg_reducers,
	init_dfatrans: occampi_primproc_init_dfatrans,
	post_setup: NULL
};
/*}}}*/

