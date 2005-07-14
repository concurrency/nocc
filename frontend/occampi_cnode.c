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


/*{{{  static int occampi_cnode_init_nodes (void)*/
/*
 *	initialises literal-nodes for occam-pi
 *	return 0 on success, non-zero on failure
 */
static int occampi_cnode_init_nodes (void)
{
	tndef_t *tnd;
	int i;

	/*{{{  occampi:cnode -- SEQ, PAR*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:cnode", &i, 2, 0, 0, TNF_LONGPROC);
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

