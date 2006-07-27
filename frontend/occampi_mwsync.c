/*
 *	occampi_mwsync.c -- occam-pi multi-way synchronisations
 *	Copyright (C) 2006 Fred Barnes <frmb@kent.ac.uk>
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
#include "dfa.h"
#include "parsepriv.h"
#include "occampi.h"
#include "feunit.h"
#include "names.h"
#include "scope.h"
#include "prescope.h"
#include "precheck.h"
#include "typecheck.h"
#include "usagecheck.h"
#include "map.h"
#include "target.h"
#include "transputer.h"
#include "codegen.h"
#include "langops.h"


/*}}}*/



/*{{{  static int occampi_mwsync_init_nodes (void)*/
/*
 *	sets up nodes for occam-pi multi-way synchronisations
 *	returns 0 on success, non-zero on error
 */
static int occampi_mwsync_init_nodes (void)
{
	tndef_t *tnd;
	compops_t *cops;
	langops_t *lops;
	int i;

#if 0
	/*{{{  occampi:mobiletypenode -- MOBILE, DYNMOBARRAY, CTCLI, CTSVR, CTSHCLI, CTSHSVR*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:mobiletypenode", &i, 1, 0, 0, TNF_NONE);		/* subnodes: subtype */
	cops = tnode_newcompops ();
	cops->bytesfor = occampi_mobiletypenode_bytesfor;
	cops->typereduce = occampi_mobiletypenode_typereduce;
	tnd->ops = cops;
	lops = tnode_newlangops ();
	lops->initsizes = occampi_mobiletypenode_initsizes;
	lops->initialising_decl = occampi_mobiletypenode_initialising_decl;
	lops->iscomplex = occampi_mobiletypenode_iscomplex;
	lops->codegen_typeaction = occampi_mobiletypenode_typeaction;
	tnd->lops = lops;

	i = -1;
	opi.tag_MOBILE = tnode_newnodetag ("MOBILE", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_DYNMOBARRAY = tnode_newnodetag ("DYNMOBARRAY", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_DYNMOBCTCLI = tnode_newnodetag ("DYNMOBCTCLI", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_DYNMOBCTSVR = tnode_newnodetag ("DYNMOBCTSVR", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_DYNMOBCTSHCLI = tnode_newnodetag ("DYNMOBCTSHCLI", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_DYNMOBCTSHSVR = tnode_newnodetag ("DYNMOBCTSHSVR", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_DYNMOBPROC = tnode_newnodetag ("DYNMOBPROC", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  occampi:mobilealloc -- NEWDYNMOBARRAY*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:mobilealloc", &i, 3, 0, 0, TNF_NONE);			/* subnodes: subtype, dimtree, type */
	cops = tnode_newcompops ();
	cops->typecheck = occampi_mobilealloc_typecheck;
	cops->gettype = occampi_mobilealloc_gettype;
	cops->premap = occampi_mobilealloc_premap;
	cops->namemap = occampi_mobilealloc_namemap;
	cops->codegen = occampi_mobilealloc_codegen;
	tnd->ops = cops;

	i = -1;
	opi.tag_NEWDYNMOBARRAY = tnode_newnodetag ("NEWDYNMOBARRAY", &i, tnd, NTF_NONE);

	/*}}}*/
#endif

	return 0;
}
/*}}}*/
/*{{{  static int occampi_mwsync_reg_reducers (void)*/
/*
 *	registers reductions for occam-pi multi-way synchronisation reductions
 *	returns 0 on success, non-zero on error
 */
static int occampi_mwsync_reg_reducers (void)
{
#if 0
	parser_register_grule ("opi:mobilise", parser_decode_grule ("SN0N+C1N-", opi.tag_MOBILE));
	parser_register_grule ("opi:dynmobilearray", parser_decode_grule ("SN0N+C1N-", opi.tag_DYNMOBARRAY));
	parser_register_grule ("opi:dynmobarrayallocreduce", parser_decode_grule ("SN0N+N+0C3R-", opi.tag_NEWDYNMOBARRAY));
#endif

	return 0;
}
/*}}}*/
/*{{{  static dfattbl_t **occampi_mwsync_init_dfatrans (int *ntrans)*/
/*
 *	initialises and returns DFA transition tables for occam-pi multi-way synchronisations
 */
static dfattbl_t **occampi_mwsync_init_dfatrans (int *ntrans)
{
	DYNARRAY (dfattbl_t *, transtbl);

	dynarray_init (transtbl);

	/* FIXME! */
	/* dynarray_add (transtbl, dfa_transtotbl ("occampi:mobileprocdecl ::= [ 0 @MOBILE 1 ] [ 1 @PROC 2 ] [ 2 occampi:name 3 ] [ 3 {<opi:nullreduce>} -* ]")); */

#if 0
	dynarray_add (transtbl, dfa_transtotbl ("occampi:type +:= [ 0 -@MOBILE 1 ] [ 1 occampi:mobiletype 2 ] [ 2 {<opi:nullreduce>} -* ]"));
	dynarray_add (transtbl, dfa_transtotbl ("occampi:expr +:= [ 0 -@MOBILE 1 ] [ 1 occampi:mobileallocexpr 2 ] [ 2 {<opi:nullreduce>} -* ]"));
#endif

	*ntrans = DA_CUR (transtbl);
	return DA_PTR (transtbl);
}
/*}}}*/
/*{{{  static int occampi_mwsync_post_setup (void)*/
/*
 *	does post-setup for occam-pi multi-way synchronisation nodes
 *	returns 0 on success, non-zero on error
 */
static int occampi_mwsync_post_setup (void)
{
	return 0;
}
/*}}}*/



/*{{{  occampi_mwsync_feunit (feunit_t)*/
feunit_t occampi_mwsync_feunit = {
	init_nodes: occampi_mwsync_init_nodes,
	reg_reducers: occampi_mwsync_reg_reducers,
	init_dfatrans: occampi_mwsync_init_dfatrans,
	post_setup: occampi_mwsync_post_setup
};
/*}}}*/

