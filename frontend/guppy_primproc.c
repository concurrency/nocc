/*
 *	guppy_primproc.c -- guppy primitive processes for NOCC
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
#include "map.h"
#include "codegen.h"
#include "target.h"
#include "transputer.h"


/*}}}*/


/*{{{  static void guppy_reduce_primproc (dfastate_t *dfast, parsepriv_t *pp, void *rarg)*/
/*
 *	reduces a primitive process
 */
static void guppy_reduce_primproc (dfastate_t *dfast, parsepriv_t *pp, void *rarg)
{
	token_t *tok;
	ntdef_t *tag;

	tok = parser_gettok (pp);
	if (lexer_tokmatchlitstr (tok, "skip")) {
		tag = gup.tag_SKIP;
	} else if (lexer_tokmatchlitstr (tok, "stop")) {
		tag = gup.tag_STOP;
	} else {
		parser_error (pp->lf, "unknown primitive process in guppy_reduce_primproc()");
		return;
	}
	*(dfast->ptr) = tnode_create (tag, tok->origin);
	lexer_freetoken (tok);

	return;
}
/*}}}*/


/*{{{  static int guppy_namemap_leafnode (compops_t *cops, tnode_t **nodep, map_t *mapdata)*/
/*
 *	called to do name-mapping on a primitive process
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_namemap_leafnode (compops_t *cops, tnode_t **nodep, map_t *mapdata)
{
	if ((*nodep)->tag == gup.tag_STOP) {
		tnode_t *bename;

		bename = mapdata->target->newname (*nodep, NULL, mapdata, 0, mapdata->target->bws.ds_min, 0, 0, 0, 0);
		*nodep = bename;
	}
	return 0;
}
/*}}}*/
/*{{{  static int guppy_codegen_leafnode (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	called to do code-generation for a primitive process
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_codegen_leafnode (compops_t *cops, tnode_t *node, codegen_t *cgen)
{
	codegen_callops (cgen, debugline, node);
	if (node->tag == gup.tag_STOP) {
		codegen_callops (cgen, tsecondary, I_SETERR);
	}
	return 0;
}
/*}}}*/


/*{{{  static int guppy_primproc_init_nodes (void)*/
/*
 *	initialises literal-nodes for occam-pi
 *	return 0 on success, non-zero on failure
 */
static int guppy_primproc_init_nodes (void)
{
	tndef_t *tnd;
	compops_t *cops;
	int i;

	/*{{{  register reduction functions*/
	fcnlib_addfcn ("guppy_reduce_primproc", guppy_reduce_primproc, 0, 3);

	/*}}}*/
	/*{{{  guppy:leafnode -- SKIP, STOP*/
	i = -1;
	tnd = gup.node_LEAFNODE = tnode_newnodetype ("guppy:leafnode", &i, 0, 0, 0, TNF_NONE);
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (guppy_namemap_leafnode));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (guppy_codegen_leafnode));
	tnd->ops = cops;


	i = -1;
	gup.tag_SKIP = tnode_newnodetag ("SKIP", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_STOP = tnode_newnodetag ("STOP", &i, tnd, NTF_NONE);
	/*}}}*/

	return 0;
}
/*}}}*/


/*{{{  guppy_primproc_feunit (feunit_t)*/
feunit_t guppy_primproc_feunit = {
	init_nodes: guppy_primproc_init_nodes,
	reg_reducers: NULL,
	init_dfatrans: NULL,
	post_setup: NULL,
	ident: "guppy-primproc"
};
/*}}}*/

