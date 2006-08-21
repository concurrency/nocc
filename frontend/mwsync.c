/*
 *	mwsync.c -- multi-way synchronisations in NOCC (new style for ETC)
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

/*
 *	NOTE: this is perhaps a little odd, but separated out because it's used
 *	by both occam-pi and MCSP.  Needs some particular run-time support.
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
#include "opts.h"
#include "symbols.h"
#include "keywords.h"
#include "lexer.h"
#include "lexpriv.h"
#include "tnode.h"
#include "parser.h"
#include "dfa.h"
#include "parsepriv.h"
#include "mwsync.h"
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
/*{{{  private data*/
static int mws_opt_rpp = 0;			/* multi-way syncs resign after PARs: --mws-rpp */

static chook_t *mapchook = NULL;
static chook_t *mwsyncpbihook = NULL;


/*}}}*/
/*{{{  FIXME: externally referenced data*/
extern langparser_t occampi_parser;

/*}}}*/
/*{{{  public data*/
mwsi_t mwsi;

/*}}}*/


/*{{{  int mwsync_opthandler_flag (cmd_option_t *opt, char ***argwalk, int *argleft)*/
/*
 *	option-handler for multiway-sync options
 *	returns 0 on success, non-zero on failure
 */
int mwsync_opthandler_flag (cmd_option_t *opt, char ***argwalk, int *argleft)
{
	int optv = (int)opt->arg;

	switch (optv) {
	case 1:
		/* multi-way syncs resign after parallel */
		nocc_message ("multiway synchronisations will resign after parallel");
		break;
	default:
		return -1;
	}

	return 0;
}
/*}}}*/


/*{{{  static mwsyncpstk_t *mwsync_newmwsyncpstk (void)*/
/*
 *	creates a new mwsyncpstk_t structure
 */
static mwsyncpstk_t *mwsync_newmwsyncpstk (void)
{
	mwsyncpstk_t *mwps = (mwsyncpstk_t *)smalloc (sizeof (mwsyncpstk_t));

	dynarray_init (mwps->parblks);
	dynarray_init (mwps->paripoints);
	dynarray_init (mwps->parbarriers);
	dynarray_init (mwps->bnames);
	dynarray_init (mwps->bipoints);

	return mwps;
}
/*}}}*/
/*{{{  static void mwsync_freemwsyncpstk (mwsyncpstk_t *mwps)*/
/*
 *	frees a mwsyncpstk_t structure
 */
static void mwsync_freemwsyncpstk (mwsyncpstk_t *mwps)
{
	dynarray_trash (mwps->parblks);
	dynarray_trash (mwps->paripoints);
	dynarray_trash (mwps->parbarriers);
	dynarray_trash (mwps->bnames);
	dynarray_trash (mwps->bipoints);
	sfree (mwps);
	return;
}
/*}}}*/
/*{{{  static mwsynctrans_t *mwsync_newmwsynctrans (void)*/
/*
 *	creates a new mwsynctrans_t structure
 */
static mwsynctrans_t *mwsync_newmwsynctrans (void)
{
	mwsynctrans_t *mwi = (mwsynctrans_t *)smalloc (sizeof (mwsynctrans_t));

	dynarray_init (mwi->varptr);
	dynarray_init (mwi->bnames);
	dynarray_init (mwi->pstack);
	mwi->error = 0;

	return mwi;
}
/*}}}*/
/*{{{  static void mwsync_freemwsynctrans (mwsynctrans_t *mwi)*/
/*
 *	frees a mwsynctrans_t structure
 */
static void mwsync_freemwsynctrans (mwsynctrans_t *mwi)
{
	int i;

	dynarray_trash (mwi->varptr);
	dynarray_trash (mwi->bnames);
	for (i=0; i<DA_CUR (mwi->pstack); i++) {
		if (DA_NTHITEM (mwi->pstack, i)) {
			mwsync_freemwsyncpstk (DA_NTHITEM (mwi->pstack, i));
		}
	}
	dynarray_trash (mwi->pstack);

	sfree (mwi);

	return;
}
/*}}}*/
/*{{{  static mwsyncpbinfo_t *mwsync_newmwsyncpbinfo (void)*/
/*
 *	creates a new mwsyncpbinfo_t structure
 */
static mwsyncpbinfo_t *mwsync_newmwsyncpbinfo (void)
{
	mwsyncpbinfo_t *pbinf = (mwsyncpbinfo_t *)smalloc (sizeof (mwsyncpbinfo_t));

	pbinf->ecount = 0;
	pbinf->sadjust = 0;
	pbinf->parent = NULL;

	return pbinf;
}
/*}}}*/
/*{{{  static void mwsync_freemwsyncpbinfo (mwsyncpbinfo_t *pbinf)*/
/*
 *	frees a mwsyncpbinfo_t structure
 */
static void mwsync_freemwsyncpbinfo (mwsyncpbinfo_t *pbinf)
{
	sfree (pbinf);
	return;
}
/*}}}*/


/*{{{  static void mwsync_pbihook_dumptree (tnode_t *node, void *chook, int indent, FILE *stream)*/
/*
 *	displays the contents of a mwsyncpbinfo compiler hook
 */
static void mwsync_pbihook_dumptree (tnode_t *node, void *chook, int indent, FILE *stream)
{
	if (chook) {
		mwsyncpbinfo_t *pbinf = (mwsyncpbinfo_t *)chook;

		occampi_isetindent (stream, indent);
		fprintf (stream, "<mwsync:parbarrierinfo ecount=\"%d\" sadjust=\"%d\" parent=\"0x%8.8x\" addr=\"0x%8.8x\" />\n", pbinf->ecount, pbinf->sadjust, (unsigned int)pbinf->parent, (unsigned int)chook);
	}
	return;
}
/*}}}*/
/*{{{  static void mwsync_pbihook_free (void *chook)*/
/*
 *	frees a mwsyncpbinfo compiler hook
 */
static void mwsync_pbihook_free (void *chook)
{
	mwsyncpbinfo_t *pbinf = (mwsyncpbinfo_t *)chook;

	mwsync_freemwsyncpbinfo (pbinf);
	return;
}
/*}}}*/
/*{{{  static void *mwsync_pbihook_copy (void *chook)*/
/*
 *	duplicates a mwsyncpbinfo compiler hook
 */
static void *mwsync_pbihook_copy (void *chook)
{
	mwsyncpbinfo_t *other = (mwsyncpbinfo_t *)chook;
	mwsyncpbinfo_t *pbinf;

	if (!other) {
		return NULL;
	}

	pbinf = mwsync_newmwsyncpbinfo ();

	pbinf->ecount = other->ecount;
	pbinf->sadjust = other->sadjust;
	pbinf->parent = other->parent;

	return (void *)pbinf;
}
/*}}}*/


/*{{{  static void mwsync_initbarrier (tnode_t *node, codegen_t *cgen, void *arg)*/
/*
 *	does initialiser code-gen for a multi-way synchronisation barrier
 */
static void mwsync_initbarrier (tnode_t *node, codegen_t *cgen, void *arg)
{
	int ws_off;

	cgen->target->be_getoffsets (node, &ws_off, NULL, NULL, NULL);

	codegen_callops (cgen, debugline, node);
	codegen_callops (cgen, loadlocalpointer, ws_off);
	codegen_callops (cgen, tsecondary, I_MWS_BINIT);
	codegen_callops (cgen, comment, "initbarrier");

	return;
}
/*}}}*/


/*{{{  static tnode_t *mwsync_leaftype_gettype (langops_t *lops, tnode_t *t, tnode_t *defaulttype)*/
/*
 *	gets the type for a mwsync leaftype -- do nothing really
 */
static tnode_t *mwsync_leaftype_gettype (langops_t *lops, tnode_t *t, tnode_t *defaulttype)
{
	if (t->tag == mwsi.tag_BARRIERTYPE) {
		return t;
	} else if (t->tag == mwsi.tag_PARBARRIERTYPE) {
		return t;
	} else if (t->tag == mwsi.tag_PROCBARRIERTYPE) {
		return t;
	}

	if (lops->next && lops->next->gettype) {
		return lops->next->gettype (lops->next, t, defaulttype);
	}
	nocc_error ("mwsync_leaftype_gettype(): no next function!");
	return defaulttype;
}
/*}}}*/
/*{{{  static int mwsync_leaftype_bytesfor (langops_t *lops, tnode_t *t, target_t *target)*/
/*
 *	returns the number of bytes required by a basic type
 */
static int mwsync_leaftype_bytesfor (langops_t *lops, tnode_t *t, target_t *target)
{
	if (t->tag == mwsi.tag_BARRIERTYPE) {
		return target->intsize * 4;
	} else if (t->tag == mwsi.tag_PARBARRIERTYPE) {
		return target->intsize * 9;
	} else if (t->tag == mwsi.tag_PROCBARRIERTYPE) {
		return target->intsize * 5;
	}

	if (lops->next && lops->next->bytesfor) {
		return lops->next->bytesfor (lops->next, t, target);
	}
	nocc_error ("mwsync_leaftype_bytesfor(): no next function!");
	return -1;
}
/*}}}*/
/*{{{  static int mwsync_leaftype_issigned (langops_t *lops, tnode_t *t, target_t *target)*/
/*
 *	returns 0 if the given basic type is unsigned
 */
static int mwsync_leaftype_issigned (langops_t *lops, tnode_t *t, target_t *target)
{
	if (t->tag == mwsi.tag_BARRIERTYPE) {
		return 0;
	} else if (t->tag == mwsi.tag_PARBARRIERTYPE) {
		return 0;
	} else if (t->tag == mwsi.tag_PROCBARRIERTYPE) {
		return 0;
	}

	if (lops->next && lops->next->issigned) {
		return lops->next->issigned (lops->next, t, target);
	}
	nocc_error ("mwsync_leaftype_issigned(): no next function!");
	return 0;
}
/*}}}*/
/*{{{  static int mwsync_leaftype_getdescriptor (langops_t *lops, tnode_t *node, char **str)*/
/*
 *	gets descriptor information for a leaf-type
 *	returns 0 to stop walk, 1 to continue
 */
static int mwsync_leaftype_getdescriptor (langops_t *lops, tnode_t *node, char **str)
{
	char *sptr;

	if (node->tag == mwsi.tag_BARRIER) {
		if (*str) {
			char *newstr = (char *)smalloc (strlen (*str) + 16);

			sptr = newstr;
			sptr += sprintf (newstr, "%s", *str);
			sfree (*str);
			*str = newstr;
		} else {
			*str = (char *)smalloc (16);
			sptr = *str;
		}
		sprintf (sptr, "BARRIER");
		return 0;
	} else if (node->tag == mwsi.tag_PARBARRIERTYPE) {
		return 0;
	} else if (node->tag == mwsi.tag_PROCBARRIERTYPE) {
		return 0;
	}
	if (lops->next && lops->next->getdescriptor) {
		return lops->next->getdescriptor (lops->next, node, str);
	}
	nocc_error ("mwsync_leaftype_getdescriptor(): no next function!");

	return 0;
}
/*}}}*/
/*{{{  static int mwsync_leaftype_initialising_decl (langops_t *lops, tnode_t *t, tnode_t *benode, map_t *mdata)*/
/*
 *	called for declarations to handle initialisation if needed
 *	returns 0 if nothing needed, 1 otherwise
 */
static int mwsync_leaftype_initialising_decl (langops_t *lops, tnode_t *t, tnode_t *benode, map_t *mdata)
{
	if (t->tag == mwsi.tag_BARRIER) {
		codegen_setinithook (benode, mwsync_initbarrier, NULL);
		return 1;
	} else if (t->tag == mwsi.tag_PARBARRIERTYPE) {
		return 0;
	} else if (t->tag == mwsi.tag_PROCBARRIERTYPE) {
		return 0;
	}
	if (lops->next && lops->next->initialising_decl) {
		return lops->next->initialising_decl (lops->next, t, benode, mdata);
	}
	return 0;
}
/*}}}*/





/*{{{  static int mwsync_modprewalk (tnode_t **tptr, void *arg)*/
/*
 *	called during tree walk to do multiway-sync checks
 *	returns 0 to stop walk, 1 to continue
 */
static int mwsync_modprewalk (tnode_t **tptr, void *arg)
{
	int i = 1;

#if 0
nocc_message ("mwsync_modprewalk(): *(tptr @ 0x%8.8x) = [%s, %s]", (unsigned int)tptr, (*tptr)->tag->name, (*tptr)->tag->ndef->name);
#endif
	if (*tptr && (*tptr)->tag->ndef->ops && tnode_hascompop ((*tptr)->tag->ndef->ops, "mwsynctrans")) {
		i = tnode_callcompop ((*tptr)->tag->ndef->ops, "mwsynctrans", 2, tptr, (mwsynctrans_t *)arg);
	}
	return i;
}
/*}}}*/
/*{{{  int mwsync_transsubtree (tnode_t **tptr, mwsynctrans_t *mwi)*/
/*
 *	does multi-way synchronisation transforms on a sub-tree
 *	returns 0 on success, non-zero on failure
 */
int mwsync_transsubtree (tnode_t **tptr, mwsynctrans_t *mwi)
{
	tnode_modprewalktree (tptr, mwsync_modprewalk, (void *)mwi);
	return mwi->error;
}
/*}}}*/
/*{{{  static int mwsynctrans_cpass (tnode_t *tree)*/
/*
 *	called for a compiler pass that flattens out BARRIERs for multi-way synchronisations
 *	returns 0 on success, non-zero on failure
 */
static int mwsynctrans_cpass (tnode_t *tree)
{
	mwsynctrans_t *mwi = mwsync_newmwsynctrans ();
	int err = 0;

	mwsync_transsubtree (&tree, mwi);
	err = mwi->error;

	mwsync_freemwsynctrans (mwi);
	return err;
}
/*}}}*/


/*{{{  static int mwsync_init_nodes (void)*/
/*
 *	called to initialise multi-way sync nodes
 *	returns 0 on success, non-zero on failure
 */
static int mwsync_init_nodes (void)
{
	tndef_t *tnd;
	compops_t *cops;
	langops_t *lops;
	int i;

	/*{{{  mwsynctrans -- new compiler pass and compiler operation*/
	if (nocc_addcompilerpass ("mwsynctrans", (void *)&occampi_parser, "fetrans", 1, (int (*)(void *))mwsynctrans_cpass, CPASS_TREE, -1, NULL)) {
		nocc_internal ("mwsync_init_nodes(): failed to add mwsynctrans compiler pass");
		return -1;
	}

	tnode_newcompop ("mwsynctrans", COPS_INVALID, 2, NULL);

	/*}}}*/
	/*{{{  mapchook, mwsyncpbihook -- compiler hooks*/
	mapchook = tnode_lookupornewchook ("map:mapnames");
	mwsyncpbihook = tnode_lookupornewchook ("mwsync:parbarrierinfo");
	mwsyncpbihook->chook_dumptree = mwsync_pbihook_dumptree;
	mwsyncpbihook->chook_free = mwsync_pbihook_free;
	mwsyncpbihook->chook_copy = mwsync_pbihook_copy;

	/*}}}*/
	/*{{{  mwsync:leaftype -- BARRIERTYPE, PARBARRIERTYPE, PROCBARRIERTYPE*/
	i = -1;
	tnd = mwsi.node_LEAFTYPE = tnode_newnodetype ("mwsync:leaftype", &i, 0, 0, 0, TNF_NONE);
	if (!tnd) {
		nocc_error ("mwsync_init_nodes(): failed to find occampi:leaftype");
		return -1;
	}
	cops = tnode_newcompops ();
	tnd->ops = cops;
	lops = tnode_newlangops ();
	lops->getdescriptor = mwsync_leaftype_getdescriptor;
	lops->gettype = mwsync_leaftype_gettype;
	lops->bytesfor = mwsync_leaftype_bytesfor;
	lops->issigned = mwsync_leaftype_issigned;
	lops->initialising_decl = mwsync_leaftype_initialising_decl;
	tnd->lops = lops;

	i = -1;
	mwsi.tag_BARRIERTYPE = tnode_newnodetag ("BARRIERTYPE", &i, tnd, NTF_NONE);
	i = -1;
	mwsi.tag_PARBARRIERTYPE = tnode_newnodetag ("PARBARRIERTYPE", &i, tnd, NTF_NONE);
	i = -1;
	mwsi.tag_PROCBARRIERTYPE = tnode_newnodetag ("PROCBARRIERTYPE", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  mwsync:mwsyncvar -- PARBARRIER, PROCBARRIER*/
	i = -1;
	tnd = tnode_newnodetype ("mwsync:mwsyncvar", &i, 4, 0, 0, TNF_SHORTDECL);			/* subnodes: (name), (type), in-scope-body, expr */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (mwsync_mwsyncvar_namemap));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (mwsync_mwsyncvar_codegen));
	tnd->ops = cops;

	i = -1;
	mwsi.tag_PARBARRIER = tnode_newnodetag ("PARBARRIER", &i, tnd, NTF_NONE);
	i = -1;
	mwsi.tag_PROCBARRIER = tnode_newnodetag ("PROCBARRIER", &i, tnd, NTF_NONE);

	/*}}}*/

	return 0;
}
/*}}}*/
/*{{{  static int mwsync_reg_reducers (void)*/
/*
 *	called to initialise multi-way sync reducers
 *	returns 0 on success, non-zero on failure
 */
static int mwsync_reg_reducers (void)
{
	return 0;
}
/*}}}*/
/*{{{  static dfattbl_t **mwsync_init_dfatrans (int *ntrans)*/
/*
 *	initialises and returns DFA transition tables for multi-way synchronisations
 */
static dfattbl_t **mwsync_init_dfatrans (int *ntrans)
{
	DYNARRAY (dfattbl_t *, transtbl);

	dynarray_init (transtbl);

	*ntrans = DA_CUR (transtbl);
	return DA_PTR (transtbl);
}
/*}}}*/
/*{{{  static int mwsync_post_setup (void)*/
/*
 *	does post-setup for multi-way syncs
 *	returns 0 on success, non-zero on failure
 */
static int mwsync_post_setup (void)
{
	return 0;
}
/*}}}*/


/*{{{  mwsync_feunit (feunit_t)*/
feunit_t mwsync_feunit = {
	init_nodes: mwsync_init_nodes,
	reg_reducers: mwsync_reg_reducers,
	init_dfatrans: mwsync_init_dfatrans,
	post_setup: mwsync_post_setup
};

/*}}}*/


/*{{{  int mwsync_init (int resign_after_par)*/
/*
 *	called by front-end initialisers to initialise multi-way sync bits
 *	returns 0 on success, non-zero on failure
 */
int mwsync_init (int resign_after_par)
{
	mws_opt_rpp = resign_after_par;
	opts_add ("mws-rpp", '\0', mwsync_opthandler_flag, (void *)1, "1multiway synchronisations resign after parallel completes");

	return 0;
}
/*}}}*/
/*{{{  int mwsync_shutdown (void)*/
/*
 *	called by front-end finalisers to shut-down multi-way sync bits
 *	returns 0 on success, non-zero on failure
 */
int mwsync_shutdown (void)
{
	return 0;
}
/*}}}*/



