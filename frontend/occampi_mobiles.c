/*
 *	occampi_mobiles.c -- occam-pi MOBILE data, channels and processes
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
#include "precheck.h"
#include "typecheck.h"
#include "usagecheck.h"
#include "map.h"
#include "target.h"
#include "transputer.h"
#include "codegen.h"
#include "langops.h"


/*}}}*/


/*{{{  static void occampi_mobiletypenode_initmobile (tnode_t *node, codegen_t *cgen, void *arg)*/
/*
 *	generates code to initialise a mobile
 */
static void occampi_mobiletypenode_initmobile (tnode_t *node, codegen_t *cgen, void *arg)
{
	tnode_t *mtype = (tnode_t *)arg;
	int ws_off, vs_off, ms_off, ms_shdw;

	cgen->target->be_getoffsets (node, &ws_off, &vs_off, &ms_off, &ms_shdw);

	codegen_callops (cgen, loadconst, 0);
	codegen_callops (cgen, storelocal, ws_off);
	codegen_callops (cgen, comment, "initmobile");

	return;
}
/*}}}*/


/*{{{  static int occampi_mobiletypenode_bytesfor (tnode_t *t, target_t *target)*/
/*
 *	determines the number of bytes needed for a MOBILE
 */
static int occampi_mobiletypenode_bytesfor (tnode_t *t, target_t *target)
{
	if (t->tag == opi.tag_MOBILE) {
		/* static mobile of some variety */
		return tnode_bytesfor (tnode_nthsubof (t, 0), target);
	}
	return -1;
}
/*}}}*/
/*{{{  static int occampi_mobiletypenode_initsizes (tnode_t *t, tnode_t *declnode, int *wssize, int *vssize, int *mssize, int *indir, map_t *mdata)*/
/*
 *	returns special allocation requirements for MOBILEs
 *	return value is non-zero if settings were made
 */
static int occampi_mobiletypenode_initsizes (tnode_t *t, tnode_t *declnode, int *wssize, int *vssize, int *mssize, int *indir, map_t *mdata)
{
	if (t->tag == opi.tag_MOBILE) {
		/* static mobile, single pointer in workspace, real sized allocation in mobilespace */
		*wssize = mdata->target->pointersize;
		*vssize = 0;
		*mssize = tnode_bytesfor (tnode_nthsubof (t, 0), mdata->target);
		*indir = 1;		/* pointer left in the workspace */
		return 1;
	}
	return 0;
}
/*}}}*/
/*{{{  static tnode_t *occampi_mobiletypenode_typereduce (tnode_t *type)*/
/*
 *	de-mobilises a type and return it (used in type-checking)
 */
static tnode_t *occampi_mobiletypenode_typereduce (tnode_t *type)
{
	if (type->tag == opi.tag_MOBILE) {
		return tnode_nthsubof (type, 0);
	}
	return NULL;
}
/*}}}*/
/*{{{  static int occampi_mobiletypenode_initialising_decl (tnode_t *t, tnode_t *benode, map_t *mdata)*/
/*
 *	initialises a mobile declaration node of some form
 */
static int occampi_mobiletypenode_initialising_decl (tnode_t *t, tnode_t *benode, map_t *mdata)
{
	if (t->tag == opi.tag_MOBILE) {
		/* static mobile */
		codegen_setinithook (benode, occampi_mobiletypenode_initmobile, (void *)t);
		return 1;
	}
	return 0;
}
/*}}}*/


/*{{{  static int occampi_mobiles_init_nodes (void)*/
/*
 *	sets up nodes for occam-pi mobiles
 *	returns 0 on success, non-zero on error
 */
static int occampi_mobiles_init_nodes (void)
{
	tndef_t *tnd;
	compops_t *cops;
	langops_t *lops;
	int i;

	/*{{{  occampi:mobiletypenode -- MOBILE, DYNMOBARRAY, CTCLI, CTSVR, CTSHCLI, CTSHSVR*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:mobiletypenode", &i, 1, 0, 0, TNF_NONE);
	cops = tnode_newcompops ();
	cops->bytesfor = occampi_mobiletypenode_bytesfor;
	cops->typereduce = occampi_mobiletypenode_typereduce;
	tnd->ops = cops;
	lops = tnode_newlangops ();
	lops->initsizes = occampi_mobiletypenode_initsizes;
	lops->initialising_decl = occampi_mobiletypenode_initialising_decl;
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

	return 0;
}
/*}}}*/
/*{{{  static int occampi_mobiles_reg_reducers (void)*/
/*
 *	registers reductions for occam-pi mobile reductions
 *	returns 0 on success, non-zero on error
 */
static int occampi_mobiles_reg_reducers (void)
{
	parser_register_grule ("opi:mobilise", parser_decode_grule ("SN0N+C1N-", opi.tag_MOBILE));

	return 0;
}
/*}}}*/
/*{{{  static dfattbl_t **occampi_mobiles_init_dfatrans (int *ntrans)*/
/*
 *	initialises and returns DFA transition tables for occam-pi mobiles
 */
static dfattbl_t **occampi_mobiles_init_dfatrans (int *ntrans)
{
	DYNARRAY (dfattbl_t *, transtbl);

	dynarray_init (transtbl);

	dynarray_add (transtbl, dfa_transtotbl ("occampi:mobileprocdecl ::= [ 0 @MOBILE 1 ] [ 1 @PROC 2 ] [ 2 occampi:name 3 ] [ 3 {<opi:nullreduce>} -* ]"));			/* FIXME! */
	dynarray_add (transtbl, dfa_transtotbl ("occampi:mobilevardecl ::= [ 0 @MOBILE 1 ] [ 1 occampi:primtype 2 ] [ 1 occampi:name 2 ] [ 2 {<opi:mobilise>} ] " \
				"[ 2 occampi:namelist 3 ] [ 3 @@: 4 ] [ 4 {<opi:declreduce>} -* ]"));
	dynarray_add (transtbl, dfa_transtotbl ("occampi:mobiledecl ::= [ 0 +@MOBILE 1 ] [ 1 +@PROC 2 ] [ 1 -* 3 ] " \
				"[ 2 {<parser:rewindtokens>} -* <occampi:mobileprocdecl> ] " \
				"[ 3 {<parser:rewindtokens>} -* <occampi:mobilevardecl> ]"));

	*ntrans = DA_CUR (transtbl);
	return DA_PTR (transtbl);
}
/*}}}*/
/*{{{  static int occampi_mobiles_post_setup (void)*/
/*
 *	does post-setup for occam-pi mobile nodes
 *	returns 0 on success, non-zero on error
 */
static int occampi_mobiles_post_setup (void)
{
	return 0;
}
/*}}}*/


/*{{{  occampi_mobiles_feunit (feunit_t)*/
feunit_t occampi_mobiles_feunit = {
	init_nodes: occampi_mobiles_init_nodes,
	reg_reducers: occampi_mobiles_reg_reducers,
	init_dfatrans: occampi_mobiles_init_dfatrans,
	post_setup: occampi_mobiles_post_setup
};
/*}}}*/


