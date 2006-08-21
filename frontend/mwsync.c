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


/*{{{  static int mwsync_opthandler_flag (cmd_option_t *opt, char ***argwalk, int *argleft)*/
/*
 *	option-handler for multiway-sync options
 *	returns 0 on success, non-zero on failure
 */
static int mwsync_opthandler_flag (cmd_option_t *opt, char ***argwalk, int *argleft)
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
/*{{{  void mwsync_isetindent (FILE *stream, int indent)*/
/*
 *	set-indent for debugging output
 */
void mwsync_isetindent (FILE *stream, int indent)
{
	int i;

	for (i=0; i<indent; i++) {
		fprintf (stream, "    ");
	}
	return;
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

		mwsync_isetindent (stream, indent);
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

	if (node->tag == mwsi.tag_BARRIERTYPE) {
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
	if (t->tag == mwsi.tag_BARRIERTYPE) {
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


/*{{{  static int mwsync_mwsyncvar_namemap (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	does name-mapping for an mwsync:mwsyncvar node
 *	returns 0 to stop walk, 1 to continue
 */
static int mwsync_mwsyncvar_namemap (compops_t *cops, tnode_t **node, map_t *map)
{
	tnode_t **namep = tnode_nthsubaddr (*node, 0);
	tnode_t *type = tnode_nthsubof (*node, 1);
	tnode_t **bodyp = tnode_nthsubaddr (*node, 2);
	tnode_t **exprp = tnode_nthsubaddr (*node, 3);
	tnode_t *bename;
	int wssize;

	if ((*node)->tag == mwsi.tag_PARBARRIER) {
		mwsyncpbinfo_t *pbinf = (mwsyncpbinfo_t *)tnode_getchook (*node, mwsyncpbihook);

		wssize = tnode_bytesfor (type, map->target);
		if (pbinf && pbinf->parent) {
			/* FIXME: map out pbinf->parent perhaps */
		}
	} else if ((*node)->tag == mwsi.tag_PROCBARRIER) {
		wssize = tnode_bytesfor (type, map->target);
	} else {
		nocc_error ("mwsync_mwsyncvar_namemap(): not PARBARRIER/PROCBARRIER: [%s, %s]", (*node)->tag->name, (*node)->tag->ndef->name);
		return 0;
	}

	bename = map->target->newname (*namep, *node, map, wssize, 0, 0, 0, wssize, 0);
	tnode_setchook (*namep, mapchook, (void *)bename);

	*node = bename;

	/* map expression */
	map_submapnames (exprp, map);

	/* map body */
	map_submapnames (bodyp, map);

	return 0;
}
/*}}}*/
/*{{{  static int mwsync_mwsyncvar_codegen (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	does code-generation for an mwsync:mwsyncvar node
 *	returns 0 to stop walk, 1 to continue
 */
static int mwsync_mwsyncvar_codegen (compops_t *cops, tnode_t *node, codegen_t *cgen)
{
	tnode_t *thisvar = (tnode_t *)tnode_getchook (tnode_nthsubof (node, 0), mapchook);
	tnode_t *othervar = tnode_nthsubof (node, 3);
	int ws_off;		/* of thisvar */

#if 0
	nocc_message ("mwsyncvar_codegen(): thisvar =");
	tnode_dumptree (thisvar, 1, stderr);
	nocc_message ("mwsyncvar_codegen(): othervar =");
	tnode_dumptree (othervar, 1, stderr);
#endif

	cgen->target->be_getoffsets (thisvar, &ws_off, NULL, NULL, NULL);

	if (node->tag == mwsi.tag_PARBARRIER) {
		mwsyncpbinfo_t *pbinf = (mwsyncpbinfo_t *)tnode_getchook (node, mwsyncpbihook);

		/*{{{  initialise PARBARRIER structure*/

		codegen_callops (cgen, loadpointer, othervar, 0);
		codegen_callops (cgen, loadlocalpointer, ws_off);
		codegen_callops (cgen, tsecondary, I_MWS_PBRILNK);
		codegen_callops (cgen, comment, "initparbarrier");

		/*}}}*/
		if (pbinf && pbinf->ecount) {
			/*{{{  enroll processes on barrier*/
			codegen_callops (cgen, loadconst, pbinf->ecount);
			if (pbinf->parent) {
				codegen_callops (cgen, comment, "FIXME! -- get address of parent (via nameref)");
				codegen_callops (cgen, loadconst, 0);
			} else {
				codegen_callops (cgen, loadconst, 0);
			}
			codegen_callops (cgen, loadlocalpointer, ws_off);
			codegen_callops (cgen, tsecondary, I_MWS_PBENROLL);
			codegen_callops (cgen, comment, "parbarrierenroll");
			/*}}}*/
		}
		if (pbinf && pbinf->sadjust) {
			/*{{{  adjust synchronisation count on barrier*/
			codegen_callops (cgen, loadconst, pbinf->sadjust);
			codegen_callops (cgen, loadlocalpointer, ws_off);
			codegen_callops (cgen, tsecondary, I_MWS_PBADJSYNC);
			codegen_callops (cgen, comment, "parbarrieradjustsync");
			/*}}}*/
		}
	} else if (node->tag == mwsi.tag_PROCBARRIER) {
		/*{{{  initialise PROCBARRIER structure*/

		codegen_callops (cgen, loadpointer, othervar, 0);
		codegen_callops (cgen, loadlocalpointer, ws_off);
		codegen_callops (cgen, tsecondary, I_MWS_PPILNK);
		codegen_callops (cgen, comment, "initprocbarrier");

		/*}}}*/
	}

	/* generate body */
	codegen_subcodegen (tnode_nthsubof (node, 2), cgen);

	if (node->tag == mwsi.tag_PARBARRIER) {
		/*{{{  maybe resign processes if they leave here*/
		if (mws_opt_rpp) {
			mwsyncpbinfo_t *pbinf = (mwsyncpbinfo_t *)tnode_getchook (node, mwsyncpbihook);

			if (pbinf && pbinf->ecount) {
				/*{{{  resign processes from barrier*/
				codegen_callops (cgen, loadconst, pbinf->ecount);
				codegen_callops (cgen, loadlocalpointer, ws_off);
				codegen_callops (cgen, tsecondary, I_MWS_PBRESIGN);
				codegen_callops (cgen, comment, "parbarrierresign(post-par)");
				/*}}}*/
			}
		}
		/*}}}*/
		/*{{{  dismantle PARBARRIER structure*/

		codegen_callops (cgen, loadlocalpointer, ws_off);
		codegen_callops (cgen, tsecondary, I_MWS_PBRULNK);
		codegen_callops (cgen, comment, "unlinkparbarrier");

		/*}}}*/
	} else if (node->tag == mwsi.tag_PROCBARRIER) {
		/*{{{  maybe resign processes as they leave a PAR*/
		if (!mws_opt_rpp) {
			codegen_callops (cgen, loadconst, 1);
			codegen_callops (cgen, loadpointer, othervar, 0);
			codegen_callops (cgen, tsecondary, I_MWS_PBRESIGN);
			codegen_callops (cgen, comment, "parbarrierresign(in-par)");
		}
		/*}}}*/
	}

	return 0;
}
/*}}}*/


/*{{{  int mwsync_mwsynctrans_makebarriertype (tnode_t **typep, mwsynctrans_t *mwi)*/
/*
 *	turns a language-level type into a mwsync barrier type
 *	returns 0 on success, non-zero on failure
 */
int mwsync_mwsynctrans_makebarriertype (tnode_t **typep, mwsynctrans_t *mwi)
{
	*typep = tnode_createfrom (mwsi.tag_BARRIERTYPE, *typep);
	return 0;
}
/*}}}*/
/*{{{  int mwsync_mwsynctrans_pushvar (tnode_t *varptr, tnode_t *bnames, mwsynctrans_t *mwi)*/
/*
 *	pushes a new name onto the mwsync stack -- used to track the usage of it
 *	returns 0 on success, non-zero on failure
 */
int mwsync_mwsynctrans_pushvar (tnode_t *varptr, tnode_t *bnames, mwsynctrans_t *mwi)
{
	if (!mwi) {
		nocc_internal ("mwsync_mwsynctrans_pushvar(): no mwi!");
		return -1;
	}

	dynarray_add (mwi->varptr, varptr);
	dynarray_add (mwi->bnames, bnames);
	dynarray_add (mwi->pstack, mwsync_newmwsyncpstk ());

	return 0;
}
/*}}}*/
/*{{{  int mwsync_mwsynctrans_popvar (tnode_t *varptr, mwsynctrans_t *mwi)*/
/*
 *	pops a name from the mwsync stack
 *	returns 0 on success, non-zero on failure
 */
int mwsync_mwsynctrans_popvar (tnode_t *varptr, mwsynctrans_t *mwi)
{
	int i;
	mwsyncpstk_t *mwps = NULL;

	if (!mwi) {
		nocc_internal ("mwsync_mwsynctrans_popvar(): no mwi!");
		return -1;
	}
	i = DA_CUR (mwi->varptr) - 1;
	if (i < 0) {
		nocc_internal ("mwsync_mwsynctrans_popvar(): nothing to pop!");
		return -1;
	}

	if (varptr != DA_NTHITEM (mwi->varptr, i)) {
		nocc_internal ("mwsync_mwsynctrans_popvar(): specified var not at top of stack!");
		return -1;
	}

	mwps = DA_NTHITEM (mwi->pstack, i);

	dynarray_delitem (mwi->varptr, i);
	dynarray_delitem (mwi->bnames, i);
	dynarray_delitem (mwi->pstack, i);

	if (mwps) {
		mwsync_freemwsyncpstk (mwps);
	}

	return 0;
}
/*}}}*/
/*{{{  int mwsync_mwsynctrans_nameref (tnode_t **tptr, name_t *name, ntdef_t *decltag, mwsynctrans_t *mwi)*/
/*
 *	called to handle a reference to a multi-way sync variable name
 *	returns 0 on success, non-zero on failure
 */
int mwsync_mwsynctrans_nameref (tnode_t **tptr, name_t *name, ntdef_t *decltag, mwsynctrans_t *mwi)
{
	if (!mwi || !name || !*tptr) {
		nocc_internal ("mwsync_mwsynctrans_nameref(): bad parameters!");
		return -1;
	}

	if (NameTypeOf (name)->tag == mwsi.tag_BARRIERTYPE) {
		mwsyncpstk_t *mwps = NULL;
		int i;

#if 0
		nocc_message ("occampi_mwsync_namenode_mwsynctrans(): BARRIER here (DA_CUR (varptr) = %d)! tree is:", DA_CUR (mwi->varptr));
		tnode_dumptree (*tptr, 1, stderr);
#endif

		for (i=0; i<DA_CUR (mwi->bnames); i++) {
			if (DA_NTHITEM (mwi->bnames, i) == *tptr) {
				break;
			}
		}
		if (i == DA_CUR (mwi->bnames)) {
			nocc_warning ("mwsync_mwsynctrans_nameref(): name not on barrier stack ..");
#if 1
			tnode_dumptree (*tptr, 1, stderr);
#endif
		} else {
			tnode_t *vdecl = DA_NTHITEM (mwi->varptr, i);
			tnode_t *parbarname = NULL, *procbarname = NULL;
			tnode_t *parbardecl = NULL, *procbardecl = NULL;
			int j;

			mwps = DA_NTHITEM (mwi->pstack, i);
			if (!DA_CUR (mwps->parblks)) {
				/*{{{  outside of any PAR block, won't have local*/
				mwsyncpbinfo_t *pbinf = NULL;

				nocc_message ("mwsync_mwsynctrans_nameref(): no pstack, creating PARBARRIER");

				parbardecl = tnode_create (mwsi.tag_PARBARRIER, NULL, NULL, tnode_create (mwsi.tag_PARBARRIERTYPE, NULL), NULL, *tptr);
				name_addtempname (parbardecl, tnode_nthsubof (parbardecl, 1), decltag, &parbarname);
				tnode_setnthsub (parbardecl, 0, parbarname);

				dynarray_add (mwps->parblks, NULL);
				dynarray_add (mwps->paripoints, NULL);
				dynarray_add (mwps->parbarriers, parbarname);
				dynarray_add (mwps->bnames, NULL);
				dynarray_add (mwps->bipoints, NULL);
				j = 0;

				/* stitch it into the vardecl */
				tnode_setnthsub (parbardecl, 2, tnode_nthsubof (vdecl, 2));
				tnode_setnthsub (vdecl, 2, parbardecl);
				DA_SETNTHITEM (mwps->bipoints, j, tnode_nthsubaddr (parbardecl, 2));		/* inside the PAR-BARRIER decl */

				/* setup info hook (single process) */
				pbinf = mwsync_newmwsyncpbinfo ();
				pbinf->ecount = 1;
				pbinf->sadjust = 0;
				pbinf->parent = NULL;
				tnode_setchook (parbardecl, mwsyncpbihook, (void *)pbinf);

#if 0
				nocc_message ("occampi_mwsync_namenode_mwsynctrans(): parbardecl is:");
				tnode_dumptree (parbardecl, 1, stderr);
#endif

				/*}}}*/
			} else {
				/*{{{  inside a PAR block, check to see if it's got a PARBARRIER*/
				j = DA_CUR (mwps->parblks) - 1;

				/* FIXME: may need to work backwards if nested PARs */
				if (!DA_NTHITEM (mwps->parbarriers, j)) {
					mwsyncpbinfo_t *pbinf = NULL;

					nocc_message ("mwsync_mwsynctrans_nameref(): got pstack, creating PARBARRIER");

					parbardecl = tnode_create (mwsi.tag_PARBARRIER, NULL, NULL, tnode_create (mwsi.tag_PARBARRIERTYPE, NULL), NULL, *tptr);
					name_addtempname (parbardecl, tnode_nthsubof (parbardecl, 1), decltag, &parbarname);
					tnode_setnthsub (parbardecl, 0, parbarname);

					/* stitch it in at the given insert-point */
					if (!DA_NTHITEM (mwps->paripoints, j)) {
						nocc_internal ("mwsync_mwsynctrans_nameref(): no PARBARRIER insert point!");
						return -1;
					}

					tnode_setnthsub (parbardecl, 2, *(DA_NTHITEM (mwps->paripoints, j)));
					*(DA_NTHITEM (mwps->paripoints, j)) = parbardecl;

					DA_SETNTHITEM (mwps->paripoints, j, tnode_nthsubaddr (parbardecl, 2));		/* inside the new PAR-BARRIER decl */
					DA_SETNTHITEM (mwps->parbarriers, j, parbarname);

					/* setup info hook (filled in after PAR) */
					pbinf = mwsync_newmwsyncpbinfo ();
					pbinf->ecount = 0;
					pbinf->sadjust = 0;
					pbinf->parent = NULL;
					tnode_setchook (parbardecl, mwsyncpbihook, (void *)pbinf);
				} else {
					/* else we've already got one here */
					parbarname = DA_NTHITEM (mwps->parbarriers, j);
				}
				/*}}}*/
			}

			procbarname = DA_NTHITEM (mwps->bnames, j);
			if (!procbarname) {
				/*{{{  no name, put one in at the insert-point*/
				tnode_t **bipoint = DA_NTHITEM (mwps->bipoints, j);

				nocc_message ("mwsync_mwsynctrans_nameref(): no bname, creating PROCBARRIER");

				procbardecl = tnode_create (mwsi.tag_PROCBARRIER, NULL, NULL, tnode_create (mwsi.tag_PROCBARRIERTYPE, NULL), NULL, parbarname);
				name_addtempname (procbardecl, tnode_nthsubof (procbardecl, 1), decltag, &procbarname);
				tnode_setnthsub (procbardecl, 0, procbarname);

				/* stitch it in at the insert-point */
				tnode_setnthsub (procbardecl, 2, *bipoint);
				*bipoint = procbardecl;
				DA_SETNTHITEM (mwps->bnames, j, procbarname);

#if 0
				nocc_message ("occampi_mwsync_namenode_mwsynctrans(): procbardecl is:");
				tnode_dumptree (procbardecl, 1, stderr);
#endif

				/*}}}*/
			}

			/* finally, replace this namenode with the proc-barrier name */
			*tptr = procbarname;
		}
	}
	return 0;
}
/*}}}*/
/*{{{  int mwsync_mwsynctrans_parallel (tnode_t *parnode, tnode_t **ipoint, tnode_t **bodies, int nbodies, mwsynctrans_t *mwi)*/
/*
 *	does mwsync processing for a parallel node
 *	returns 0 on success, non-zero on failure
 */
int mwsync_mwsynctrans_parallel (tnode_t *parnode, tnode_t **ipoint, tnode_t **bodies, int nbodies, mwsynctrans_t *mwi)
{
	int i;

	/* go through each one in turn */
	for (i=0; i<DA_CUR (mwi->varptr); i++) {
		mwsyncpstk_t *mwps = DA_NTHITEM (mwi->pstack, i);

		/* start to add a PAR block for this node */
#if 1
		nocc_message ("mwsync_mwsynctrans_parallel(): parallel in var-scope, adding an entry to its pstk");
#endif

		dynarray_add (mwps->parblks, parnode);
		dynarray_add (mwps->paripoints, ipoint);		/* if we create a PARBARRIER, want it here */
		dynarray_add (mwps->parbarriers, NULL);
		dynarray_add (mwps->bnames, NULL);
		dynarray_add (mwps->bipoints, NULL);
	}

	/* do bodies */
	for (i=0; i<nbodies; i++) {
		tnode_t **bodyp = bodies + i;
		int j;

		/* setup bipoints for each var */
		for (j=0; j<DA_CUR (mwi->varptr); j++) {
			mwsyncpstk_t *mwps = DA_NTHITEM (mwi->pstack, j);
			int k = DA_CUR (mwps->parblks) - 1;

			DA_SETNTHITEM (mwps->bipoints, k, bodyp);		/* insert things at the PAR body */
		}

		mwsync_transsubtree (bodyp, mwi);

		/* check each var to see if something in the body used it -- should be favourable to freevars and the like */
		for (j=0; j<DA_CUR (mwi->varptr); j++) {
			mwsyncpstk_t *mwps = DA_NTHITEM (mwi->pstack, j);
			int k = DA_CUR (mwps->parblks) - 1;
			tnode_t *parbarrier = DA_NTHITEM (mwps->parbarriers, k);
			tnode_t *procbarrier = DA_NTHITEM (mwps->bnames, k);
			mwsyncpbinfo_t *pbinf = (mwsyncpbinfo_t *)tnode_getchook (NameDeclOf (tnode_nthnameof (parbarrier, 0)), mwsyncpbihook);

			if (procbarrier) {
				/* yes, this one was used, kick up enroll count */
				pbinf->ecount++;
			}
			DA_SETNTHITEM (mwps->bnames, k, NULL);
		}
	}

	for (i=0; i<DA_CUR (mwi->varptr); i++) {
		mwsyncpstk_t *mwps = DA_NTHITEM (mwi->pstack, i);
		tnode_t *parbarrier;

		/* remove recent PAR */
		if (!DA_CUR (mwps->parblks) || (DA_NTHITEM (mwps->parblks, DA_CUR (mwps->parblks) - 1) != parnode)) {
			nocc_internal ("mwsync_mwsynctrans_parallel(): erk, not this PAR!");
		}

		dynarray_delitem (mwps->parblks, DA_CUR (mwps->parblks) - 1);
		dynarray_delitem (mwps->paripoints, DA_CUR (mwps->paripoints) - 1);
		dynarray_delitem (mwps->parbarriers, DA_CUR (mwps->parbarriers) - 1);
		dynarray_delitem (mwps->bnames, DA_CUR (mwps->bnames) - 1);
		dynarray_delitem (mwps->bipoints, DA_CUR (mwps->bipoints) - 1);
	}

#if 0
	nocc_message ("mwsync_mwsynctrans_parallel(): here (DA_CUR (varptr) = %d)! tree is:", DA_CUR (mwi->varptr));
	tnode_dumptree (*tptr, 1, stderr);
#endif
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


