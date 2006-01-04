/*
 *	occampi_mobiles.c -- occam-pi MOBILE data, channels and processes
 *	Copyright (C) 2005-2006 Fred Barnes <frmb@kent.ac.uk>
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


/*{{{  private vars*/
static chook_t *chook_demobiletype = NULL;

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

	codegen_callops (cgen, debugline, node);
	codegen_callops (cgen, loadmsp, 0);
	codegen_callops (cgen, loadnonlocal, ms_shdw);
	codegen_callops (cgen, storelocal, ws_off);
	codegen_callops (cgen, comment, "initmobile");

	return;
}
/*}}}*/
/*{{{  static void occampi_mobiletypenode_finalmobile (tnode_t *node, codegen_t *cgen, void *arg)*/
/*
 *	generates code to descope a mobile
 */
static void occampi_mobiletypenode_finalmobile (tnode_t *node, codegen_t *cgen, void *arg)
{
	tnode_t *mtype = (tnode_t *)arg;
	int ws_off, vs_off, ms_off, ms_shdw;

	cgen->target->be_getoffsets (node, &ws_off, &vs_off, &ms_off, &ms_shdw);

	codegen_callops (cgen, debugline, node);
	codegen_callops (cgen, loadlocal, ws_off);
	codegen_callops (cgen, loadmsp, 0);
	codegen_callops (cgen, storenonlocal, ms_shdw);
	codegen_callops (cgen, comment, "finalmobile");

	return;
}
/*}}}*/
/*{{{  static void occampi_mobiletypenode_initdynmobarray (tnode_t *node, codegen_t *cgen, void *arg)*/
/*
 *	generates code to initialise a dynamic mobile array
 */
static void occampi_mobiletypenode_initdynmobarray (tnode_t *node, codegen_t *cgen, void *arg)
{
	tnode_t *mtype = (tnode_t *)arg;
	int ws_off;

	cgen->target->be_getoffsets (node, &ws_off, NULL, NULL, NULL);

	codegen_callops (cgen, debugline, node);
	codegen_callops (cgen, tsecondary, I_NULL);
	codegen_callops (cgen, storelocal, ws_off);
	/* FIXME: number of dimensions */
	codegen_callops (cgen, loadconst, 0);
	codegen_callops (cgen, storelocal, ws_off + cgen->target->pointersize);
	codegen_callops (cgen, comment, "initdynmobarray");

	return;
}
/*}}}*/
/*{{{  static void occampi_mobiletypenode_finaldynmobarray (tnode_t *node, codegen_t *cgen, void *arg)*/
/*
 *	generates code to release a dynamic mobile array
 */
static void occampi_mobiletypenode_finaldynmobarray (tnode_t *node, codegen_t *cgen, void *arg)
{
	tnode_t *mtype = (tnode_t *)arg;
	int ws_off, skiplab;

	cgen->target->be_getoffsets (node, &ws_off, NULL, NULL, NULL);

	skiplab = codegen_new_label (cgen);
	codegen_callops (cgen, debugline, node);
	codegen_callops (cgen, loadlocal, ws_off + cgen->target->pointersize);		/* load first dimension */
	codegen_callops (cgen, branch, I_CJ, skiplab);
	codegen_callops (cgen, loadlocal, ws_off);					/* load pointer */
	codegen_callops (cgen, tsecondary, I_MRELEASE);
	codegen_callops (cgen, loadconst, 0);
	codegen_callops (cgen, storelocal, ws_off + cgen->target->pointersize);		/* zero first dimension */
	codegen_callops (cgen, setlabel, skiplab);
	codegen_callops (cgen, comment, "finaldynmobarray");

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
	} else if (t->tag == opi.tag_DYNMOBARRAY) {
		/* don't know */
		return -1;
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
	} else if (t->tag == opi.tag_DYNMOBARRAY) {
		/* dynamic mobile array, 1 + <ndim> words in workspace, no allocation elsewhere */
		*wssize = mdata->target->pointersize + (1 * mdata->target->slotsize);		/* FIXME: ndim */
		*vssize = 0;
		*mssize = 0;
		*indir = 1;
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
	} else if (type->tag == opi.tag_DYNMOBARRAY) {
		tnode_t *rtype = (tnode_t *)tnode_getchook (type, chook_demobiletype);

		if (!rtype) {
			/* reducing into an array-type */
			rtype = tnode_createfrom (opi.tag_ARRAY, type, NULL, tnode_nthsubof (type, 0));
			tnode_setchook (type, chook_demobiletype, (void *)rtype);
		}

		return rtype;
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
		codegen_setfinalhook (benode, occampi_mobiletypenode_finalmobile, (void *)t);
		return 1;
	} else if (t->tag == opi.tag_DYNMOBARRAY) {
		/* dynamic mobile array */
		codegen_setinithook (benode, occampi_mobiletypenode_initdynmobarray, (void *)t);
		codegen_setfinalhook (benode, occampi_mobiletypenode_finaldynmobarray, (void *)t);
	}
	return 0;
}
/*}}}*/


/*{{{  static tnode_t *occampi_mobilealloc_gettype (tnode_t *node, tnode_t *default_type)*/
/*
 *	gets the type of a mobile allocation node
 */
static tnode_t *occampi_mobilealloc_gettype (tnode_t *node, tnode_t *default_type)
{
	return default_type;
}
/*}}}*/


/*{{{  static void *occampi_copy_demobilechook (void *chook)*/
/*
 *	copies a demobile-type compiler hook
 */
static void *occampi_copy_demobilechook (void *chook)
{
	tnode_t *type = (tnode_t *)chook;

	if (type) {
		type = tnode_copytree (type);
	}
	return (void *)type;
}
/*}}}*/
/*{{{  static void occampi_free_demobilechook (void *chook)*/
/*
 *	frees a demobile-type compiler hook
 */
static void occampi_free_demobilechook (void *chook)
{
	tnode_t *type = (tnode_t *)chook;

	if (type) {
		tnode_free (type);
	}
	return;
}
/*}}}*/
/*{{{  static void occampi_dumptree_demobilechook (tnode_t *t, void *chook, int indent, FILE *stream)*/
/*
 *	dumps a demobile-type compiler hook (debugging)
 */
static void occampi_dumptree_demobilechook (tnode_t *t, void *chook, int indent, FILE *stream)
{
	tnode_t *type = (tnode_t *)chook;

	if (type) {
		occampi_isetindent (stream, indent);
		fprintf (stream, "<chook:occampi:demobiletype>\n");
		tnode_dumptree (type, indent + 1, stream);
		occampi_isetindent (stream, indent);
		fprintf (stream, "</chook:occampi:demobiletype>\n");
	}
	return;
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
	/*{{{  occampi:mobilealloc -- NEWDYNMOBARRAY*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:mobilealloc", &i, 2, 0, 0, TNF_NONE);
	cops = tnode_newcompops ();
	cops->gettype = occampi_mobilealloc_gettype;
	tnd->ops = cops;

	i = -1;
	opi.tag_NEWDYNMOBARRAY = tnode_newnodetag ("NEWDYNMOBARRAY", &i, tnd, NTF_NONE);
	/*}}}*/
	/*{{{  compiler hooks*/
	if (!chook_demobiletype) {
		chook_demobiletype = tnode_lookupornewchook ("occampi:demobiletype");
		chook_demobiletype->chook_copy = occampi_copy_demobilechook;
		chook_demobiletype->chook_free = occampi_free_demobilechook;
		chook_demobiletype->chook_dumptree = occampi_dumptree_demobilechook;
	}
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
	parser_register_grule ("opi:dynmobilearray", parser_decode_grule ("SN0N+C1N-", opi.tag_DYNMOBARRAY));
	parser_register_grule ("opi:dynmobarrayallocreduce", parser_decode_grule ("SN0N+N+VC2R-", opi.tag_NEWDYNMOBARRAY));

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
	dynarray_add (transtbl, dfa_transtotbl ("occampi:mobiletype ::= [ 0 @MOBILE 1 ] [ 1 @@[ 4 ] [ 1 occampi:primtype 2 ] [ 1 occampi:name 2 ] [ 2 {<opi:mobilise>} -* 3 ] " \
				"[ 3 {<opi:nullreduce>} -* ] " \
				"[ 4 @@] 5 ] [ 5 occampi:type 6 ] [ 6 {<opi:dynmobilearray>} -* 3 ]"));
	dynarray_add (transtbl, dfa_transtotbl ("occampi:mobilevardecl ::= [ 0 occampi:mobiletype 1 ] " \
				"[ 1 occampi:namelist 2 ] [ 2 @@: 3 ] [ 3 {<opi:declreduce>} -* ] "));
	dynarray_add (transtbl, dfa_transtotbl ("occampi:mobiledecl ::= [ 0 +@MOBILE 1 ] [ 1 +@PROC 2 ] [ 1 -* 3 ] " \
				"[ 2 {<parser:rewindtokens>} -* <occampi:mobileprocdecl> ] " \
				"[ 3 {<parser:rewindtokens>} -* <occampi:mobilevardecl> ]"));
	dynarray_add (transtbl, dfa_transtotbl ("occampi:dynmobarrayallocexpr ::= [ 0 @@[ 1 ] [ 1 occampi:expr 2 ] [ 2 @@] 3 ] [ 3 -@@[ 5 ] [ 3 -* 4 ] [ 4 occampi:type 6 ] " \
				"[ 5 occampi:dynmobarrayallocexpr 6 ] [ 6 {<opi:dynmobarrayallocreduce>} -* ]"));
	dynarray_add (transtbl, dfa_transtotbl ("occampi:mobileallocexpr ::= [ 0 @MOBILE 1 ] [ 1 -@@[ <occampi:dynmobarrayallocexpr> ]"));

	dynarray_add (transtbl, dfa_transtotbl ("occampi:type +:= [ 0 -@MOBILE 1 ] [ 1 occampi:mobiletype 2 ] [ 2 {<opi:nullreduce>} -* ]"));
	dynarray_add (transtbl, dfa_transtotbl ("occampi:expr +:= [ 0 -@MOBILE 1 ] [ 1 occampi:mobileallocexpr 2 ] [ 2 {<opi:nullreduce>} -* ]"));

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


