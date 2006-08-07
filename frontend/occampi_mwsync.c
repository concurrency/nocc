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
/*{{{  private types*/

typedef struct TAG_mwsyncpstk {
	DYNARRAY (tnode_t *, parblks);		/* PAR nodes themselves */
	DYNARRAY (tnode_t *, parbarriers);	/* associated PAR barrier variables */
	DYNARRAY (tnode_t *, bnames);		/* barrier name variables */
	DYNARRAY (tnode_t **, bipoints);	/* barrier name insert point */
} mwsyncpstk_t;

typedef struct TAG_mwsynctrans {
	DYNARRAY (tnode_t *, varptr);		/* barrier var-decl */
	DYNARRAY (tnode_t *, bnames);		/* barrier name variables (in declarations) */
	DYNARRAY (mwsyncpstk_t *, pstack);	/* PAR stack */
	int error;
} mwsynctrans_t;

typedef struct TAG_mwsyncpbinfo {
	int ecount;				/* enroll count */
	int sadjust;				/* sync adjust */
	tnode_t *parent;			/* parent PARBARRIER */
} mwsyncpbinfo_t;


/*}}}*/
/*{{{  private data*/

static chook_t *mapchook = NULL;
static chook_t *mwsyncpbihook = NULL;


/*}}}*/
/*{{{  forward decls.*/

static int mwsync_transsubtree (tnode_t **tptr, mwsynctrans_t *mwi);


/*}}}*/


/*{{{  static mwsyncpstk_t *mwsync_newmwsyncpstk (void)*/
/*
 *	creates a new mwsyncpstk_t structure
 */
static mwsyncpstk_t *mwsync_newmwsyncpstk (void)
{
	mwsyncpstk_t *mwps = (mwsyncpstk_t *)smalloc (sizeof (mwsyncpstk_t));

	dynarray_init (mwps->parblks);
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


/*{{{  static void occampi_mwsync_initbarrier (tnode_t *node, codegen_t *cgen, void *arg)*/
/*
 *	does initialiser code-gen for a multi-way synchronisation barrier
 */
static void occampi_mwsync_initbarrier (tnode_t *node, codegen_t *cgen, void *arg)
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


/*{{{  static tnode_t *occampi_mwsync_leaftype_gettype (langops_t *lops, tnode_t *t, tnode_t *defaulttype)*/
/*
 *	gets the type for a mwsync leaftype -- do nothing really
 */
static tnode_t *occampi_mwsync_leaftype_gettype (langops_t *lops, tnode_t *t, tnode_t *defaulttype)
{
	if (t->tag == opi.tag_BARRIER) {
		return t;
	}
	if (lops->next && lops->next->gettype) {
		return lops->next->gettype (lops->next, t, defaulttype);
	}
	nocc_error ("occampi_mwsync_leaftype_gettype(): no next function!");
	return defaulttype;
}
/*}}}*/
/*{{{  static int occampi_mwsync_leaftype_bytesfor (langops_t *lops, tnode_t *t, target_t *target)*/
/*
 *	returns the number of bytes required by a basic type
 */
static int occampi_mwsync_leaftype_bytesfor (langops_t *lops, tnode_t *t, target_t *target)
{
	if (t->tag == opi.tag_BARRIER) {
		return target->intsize * 4;
	}
	if (lops->next && lops->next->bytesfor) {
		return lops->next->bytesfor (lops->next, t, target);
	}
	nocc_error ("occampi_mwsync_leaftype_bytesfor(): no next function!");
	return -1;
}
/*}}}*/
/*{{{  static int occampi_mwsync_leaftype_issigned (langops_t *lops, tnode_t *t, target_t *target)*/
/*
 *	returns 0 if the given basic type is unsigned
 */
static int occampi_mwsync_leaftype_issigned (langops_t *lops, tnode_t *t, target_t *target)
{
	if (t->tag == opi.tag_BARRIER) {
		return 0;
	}
	if (lops->next && lops->next->issigned) {
		return lops->next->issigned (lops->next, t, target);
	}
	nocc_error ("occampi_mwsync_leaftype_issigned(): no next function!");
	return 0;
}
/*}}}*/
/*{{{  static int occampi_mwsync_leaftype_getdescriptor (langops_t *lops, tnode_t *node, char **str)*/
/*
 *	gets descriptor information for a leaf-type
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_mwsync_leaftype_getdescriptor (langops_t *lops, tnode_t *node, char **str)
{
	char *sptr;

	if (node->tag == opi.tag_BARRIER) {
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
	}
	if (lops->next && lops->next->getdescriptor) {
		lops->next->getdescriptor (lops->next, node, str);
	}
	nocc_error ("occampi_mwsync_leaftype_getdescriptor(): no next function!");

	return 0;
}
/*}}}*/
/*{{{  static int occampi_mwsync_leaftype_initialising_decl (langops_t *lops, tnode_t *t, tnode_t *benode, map_t *mdata)*/
/*
 *	called for declarations to handle initialisation if needed
 *	returns 0 if nothing needed, 1 otherwise
 */
static int occampi_mwsync_leaftype_initialising_decl (langops_t *lops, tnode_t *t, tnode_t *benode, map_t *mdata)
{
	if (t->tag == opi.tag_BARRIER) {
		codegen_setinithook (benode, occampi_mwsync_initbarrier, NULL);
		return 1;
	}
	if (lops->next && lops->next->initialising_decl) {
		return lops->next->initialising_decl (lops->next, t, benode, mdata);
	}
	return 0;
}
/*}}}*/


/*{{{  static int occampi_mwsync_action_typecheck (compops_t *cops, tnode_t *node, typecheck_t *tc)*/
/*
 *	called to type-check a sync action-node
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_mwsync_action_typecheck (compops_t *cops, tnode_t *node, typecheck_t *tc)
{
	int i = 1;

	if (node->tag == opi.tag_SYNC) {
		tnode_t *lhs = tnode_nthsubof (node, 0);
		tnode_t *acttype = tnode_nthsubof (node, 2);
		tnode_t *lhstype;

		if (acttype) {
			nocc_warning ("occampi_mwsync_action_typecheck(): strange, already type-checked this action");
			return 0;
		}
		lhstype = typecheck_gettype (lhs, NULL);
		i = 0;
		if (!lhstype || (lhstype->tag != opi.tag_BARRIER)) {
			typecheck_error (node, tc, "can only synchronise on a BARRIER");
		} else {
			tnode_setnthsub (node, 2, lhstype);
		}
	} else {
		/* down-stream typecheck */
		if (cops->next && tnode_hascompop_i (cops->next, (int)COPS_TYPECHECK)) {
			i = tnode_callcompop_i (cops->next, (int)COPS_TYPECHECK, 2, node, tc);
		}
	}
	return i;
}
/*}}}*/
/*{{{  static int occampi_mwsync_action_namemap (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	does name-mapping for SYNC action-nodes
 *	returns 0 to stop walk, non-zero to continue
 */
static int occampi_mwsync_action_namemap (compops_t *cops, tnode_t **node, map_t *map)
{
	int i = 1;

	if ((*node)->tag == opi.tag_SYNC) {
		tnode_t *bename;

		map_submapnames (tnode_nthsubaddr (*node, 0), map);		/* map barrier operand */
		bename = map->target->newname (*node, NULL, map, 0, map->target->bws.ds_min, 0, 0, 0, 0);
		*node = bename;
		i = 0;
	} else {
		if (cops->next && tnode_hascompop_i (cops->next, (int)COPS_NAMEMAP)) {
			i = tnode_callcompop_i (cops->next, (int)COPS_NAMEMAP, 2, node, map);
		}
	}
	return i;
}
/*}}}*/
/*{{{  static int occampi_mwsync_action_codegen (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	does code-generation for SYNC action-nodes
 *	returns 0 to stop walk, non-zero to continue
 */
static int occampi_mwsync_action_codegen (compops_t *cops, tnode_t *node, codegen_t *cgen)
{
	int i = 1;

	if (node->tag == opi.tag_SYNC) {
		tnode_t *bar = tnode_nthsubof (node, 0);

		codegen_callops (cgen, debugline, node);
		codegen_callops (cgen, loadpointer, bar, 0);
		codegen_callops (cgen, tsecondary, I_MWS_SYNC);
		i = 0;
	} else {
		if (cops->next && tnode_hascompop_i (cops->next, (int)COPS_CODEGEN)) {
			i = tnode_callcompop_i (cops->next, (int)COPS_CODEGEN, 2, node, cgen);
		}
	}
	return i;
}
/*}}}*/


/*{{{  static int occampi_mwsync_vardecl_mwsynctrans (compops_t *cops, tnode_t **tptr, mwsynctrans_t *mwi)*/
/*
 *	does multi-way synchronisation transforms for a variable declaration
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_mwsync_vardecl_mwsynctrans (compops_t *cops, tnode_t **tptr, mwsynctrans_t *mwi)
{
	tnode_t *name = tnode_nthsubof (*tptr, 0);
	int vptr = DA_CUR (mwi->varptr);

	if ((name->tag == opi.tag_NDECL) && (NameTypeOf (tnode_nthnameof (name, 0))->tag == opi.tag_BARRIER)) {
		/*{{{  BARRIER variable declaration, add to stack*/
		dynarray_add (mwi->varptr, *tptr);
		dynarray_add (mwi->bnames, name);
		dynarray_add (mwi->pstack, mwsync_newmwsyncpstk ());

		/*}}}*/
	}

	/* walk over body */
	mwsync_transsubtree (tnode_nthsubaddr (*tptr, 2), mwi);

	while (DA_CUR (mwi->varptr) > vptr) {
		/* some names got added, remove them */
		int i = DA_CUR (mwi->varptr) - 1;
		mwsyncpstk_t *mwps = DA_NTHITEM (mwi->pstack, i);

		dynarray_delitem (mwi->varptr, i);
		dynarray_delitem (mwi->bnames, i);
		dynarray_delitem (mwi->pstack, i);

		if (mwps) {
			mwsync_freemwsyncpstk (mwps);
		}
	}

	return 0;
}
/*}}}*/
/*{{{  static int occampi_mwsync_namenode_mwsynctrans (compops_t *cops, tnode_t **tptr, mwsynctrans_t *mwi)*/
/*
 *	does multi-way synchronisation transforms for a name-node (not in a declaration)
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_mwsync_namenode_mwsynctrans (compops_t *cops, tnode_t **tptr, mwsynctrans_t *mwi)
{
	if (((*tptr)->tag == opi.tag_NDECL) || ((*tptr)->tag == opi.tag_NPARAM)) {
		name_t *name = tnode_nthnameof (*tptr, 0);

		if (NameTypeOf (name)->tag == opi.tag_BARRIER) {
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
				nocc_warning ("occampi_mwsync_namenode_mwsynctrans(): name not on barrier stack ..");
			} else {
				tnode_t *vdecl = DA_NTHITEM (mwi->varptr, i);
				tnode_t *parbarname = NULL, *procbarname = NULL;
				tnode_t *parbardecl = NULL, *procbardecl = NULL;
				int j;

				mwps = DA_NTHITEM (mwi->pstack, i);
				if (!DA_CUR (mwps->parblks)) {
					/*{{{  outside of any PAR block, won't have local*/
					mwsyncpbinfo_t *pbinf = NULL;

					nocc_message ("occampi_mwsync_namenode_mwsynctrans(): no pstack, creating PARBARRIER");

					parbardecl = tnode_create (opi.tag_PARBARRIER, NULL, NULL, tnode_create (opi.tag_BARRIER, NULL), NULL, *tptr);
					/* parbarname = tnode_createfrom (opi.tag_NDECL, *tptr, name_addtempname (parbardecl, NULL, NULL, NULL)); */
					name_addtempname (parbardecl, tnode_nthsubof (parbardecl, 1), opi.tag_NDECL, &parbarname);
					tnode_setnthsub (parbardecl, 0, parbarname);

					dynarray_add (mwps->parblks, NULL);
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
					pbinf->sadjust = 1;
					pbinf->parent = NULL;
					tnode_setchook (parbardecl, mwsyncpbihook, (void *)pbinf);

#if 0
					nocc_message ("occampi_mwsync_namenode_mwsynctrans(): parbardecl is:");
					tnode_dumptree (parbardecl, 1, stderr);
#endif

					/*}}}*/
				} else {
					j = DA_CUR (mwps->parblks) - 1;
				}

				procbarname = DA_NTHITEM (mwps->bnames, j);
				if (!procbarname) {
					/*{{{  no name, put one in at the insert-point*/
					tnode_t **bipoint = DA_NTHITEM (mwps->bipoints, j);

					nocc_message ("occampi_mwsync_namenode_mwsynctrans(): no bname, creating PROCBARRIER");

					procbardecl = tnode_create (opi.tag_PROCBARRIER, NULL, NULL, tnode_create (opi.tag_BARRIER, NULL), NULL, parbarname);
					name_addtempname (procbardecl, tnode_nthsubof (procbardecl, 1), opi.tag_NDECL, &procbarname);
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
	}
	return 0;
}
/*}}}*/


/*{{{  static int occampi_mwsyncvar_namemap (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	does name-mapping for an occampi:mwsyncvar node
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_mwsyncvar_namemap (compops_t *cops, tnode_t **node, map_t *map)
{
	tnode_t **namep = tnode_nthsubaddr (*node, 0);
	tnode_t **bodyp = tnode_nthsubaddr (*node, 2);
	tnode_t **exprp = tnode_nthsubaddr (*node, 3);
	tnode_t *bename;
	int wssize;

	if ((*node)->tag == opi.tag_PARBARRIER) {
		mwsyncpbinfo_t *pbinf = (mwsyncpbinfo_t *)tnode_getchook (*node, mwsyncpbihook);

		wssize = map->target->slotsize * 9;
		if (pbinf && pbinf->parent) {
			/* FIXME: map out pbinf->parent perhaps */
		}
	} else if ((*node)->tag == opi.tag_PROCBARRIER) {
		wssize = map->target->slotsize * 4;
	} else {
		nocc_error ("occampi_mwsyncvar_namemap(): not PARBARRIER/PROCBARRIER: [%s, %s]", (*node)->tag->name, (*node)->tag->ndef->name);
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
/*{{{  static int occampi_mwsyncvar_codegen (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	does code-generation for an occampi:mwsyncvar node
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_mwsyncvar_codegen (compops_t *cops, tnode_t *node, codegen_t *cgen)
{
	tnode_t *thisvar = (tnode_t *)tnode_getchook (tnode_nthsubof (node, 0), mapchook);
	tnode_t *othervar = tnode_nthsubof (node, 3);
	int ws_off;		/* of thisvar */

#if 0
	nocc_message ("occampi_mwsyncvar_codegen(): thisvar =");
	tnode_dumptree (thisvar, 1, stderr);
	nocc_message ("occampi_mwsyncvar_codegen(): othervar =");
	tnode_dumptree (othervar, 1, stderr);
#endif

	cgen->target->be_getoffsets (thisvar, &ws_off, NULL, NULL, NULL);

	if (node->tag == opi.tag_PARBARRIER) {
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
	} else if (node->tag == opi.tag_PROCBARRIER) {
		/*{{{  initialise PROCBARRIER structure*/

		codegen_callops (cgen, loadpointer, othervar, 0);
		codegen_callops (cgen, loadlocalpointer, ws_off);
		codegen_callops (cgen, tsecondary, I_MWS_PPILNK);
		codegen_callops (cgen, comment, "initprocbarrier");

		/*}}}*/
	}

	/* generate body */
	codegen_subcodegen (tnode_nthsubof (node, 2), cgen);

	if (node->tag == opi.tag_PARBARRIER) {
		/*{{{  dismantle PARBARRIER structure*/

		codegen_callops (cgen, loadlocalpointer, ws_off);
		codegen_callops (cgen, tsecondary, I_MWS_PBRULNK);
		codegen_callops (cgen, comment, "unlinkparbarrier");

		/*}}}*/
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
/*{{{  static int mwsync_transsubtree (tnode_t **tptr, mwsynctrans_t *mwi)*/
/*
 *	does multi-way synchronisation transforms on a sub-tree
 *	returns 0 on success, non-zero on failure
 */
static int mwsync_transsubtree (tnode_t **tptr, mwsynctrans_t *mwi)
{
	tnode_modprewalktree (tptr, mwsync_modprewalk, (void *)mwi);
	return mwi->error;
}
/*}}}*/
/*{{{  static int occampi_mwsynctrans_cpass (tnode_t *tree)*/
/*
 *	called for a compiler pass that flattens out BARRIERs for multi-way synchronisations
 *	returns 0 on success, non-zero on failure
 */
static int occampi_mwsynctrans_cpass (tnode_t *tree)
{
	mwsynctrans_t *mwi = mwsync_newmwsynctrans ();
	int err = 0;

	mwsync_transsubtree (&tree, mwi);
	err = mwi->error;

	mwsync_freemwsynctrans (mwi);
	return err;
}
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

	/*{{{  mwsynctrans -- new compiler pass and compiler operation*/
	if (nocc_addcompilerpass ("mwsynctrans", (void *)&occampi_parser, "fetrans", 1, (int (*)(void *))occampi_mwsynctrans_cpass, CPASS_TREE, -1, NULL)) {
		nocc_internal ("occampi_mwsync_init_nodes(): failed to add mwsynctrans compiler pass");
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
	/*{{{  occampi:leaftype -- BARRIER*/
	tnd = tnode_lookupnodetype ("occampi:leaftype");
	if (!tnd) {
		nocc_error ("occampi_mwsync_init_nodes(): failed to find occampi:leaftype");
		return -1;
	}
	cops = tnode_insertcompops (tnd->ops);
	tnd->ops = cops;
	lops = tnode_insertlangops (tnd->lops);
	lops->getdescriptor = occampi_mwsync_leaftype_getdescriptor;
	lops->gettype = occampi_mwsync_leaftype_gettype;
	lops->bytesfor = occampi_mwsync_leaftype_bytesfor;
	lops->issigned = occampi_mwsync_leaftype_issigned;
	lops->initialising_decl = occampi_mwsync_leaftype_initialising_decl;
	tnd->lops = lops;

	i = -1;
	opi.tag_BARRIER = tnode_newnodetag ("BARRIER", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  occampi:actionnode -- SYNC*/
	tnd = tnode_lookupnodetype ("occampi:actionnode");

	cops = tnode_insertcompops (tnd->ops);
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (occampi_mwsync_action_typecheck));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (occampi_mwsync_action_namemap));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (occampi_mwsync_action_codegen));
	tnd->ops = cops;
	lops = tnode_insertlangops (tnd->lops);
	tnd->lops = lops;

	i = -1;
	opi.tag_SYNC = tnode_newnodetag ("SYNC", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  occampi:mwsyncvar -- PARBARRIER, PROCBARRIER*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:mwsyncvar", &i, 4, 0, 0, TNF_SHORTDECL);			/* subnodes: (name), (type), in-scope-body, expr */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (occampi_mwsyncvar_namemap));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (occampi_mwsyncvar_codegen));
	tnd->ops = cops;

	i = -1;
	opi.tag_PARBARRIER = tnode_newnodetag ("PARBARRIER", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_PROCBARRIER = tnode_newnodetag ("PROCBARRIER", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  occampi:vardecl -- (mods for barriers)*/
	tnd = tnode_lookupnodetype ("occampi:vardecl");
	tnode_setcompop (tnd->ops, "mwsynctrans", 2, COMPOPTYPE (occampi_mwsync_vardecl_mwsynctrans));

	/*}}}*/
	/*{{{  occampi:namenode -- (mods for barriers)*/
	tnd = tnode_lookupnodetype ("occampi:namenode");
	tnode_setcompop (tnd->ops, "mwsynctrans", 2, COMPOPTYPE (occampi_mwsync_namenode_mwsynctrans));

	/*}}}*/

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
	parser_register_grule ("opi:barrierreduce", parser_decode_grule ("ST0T+@tC0R-", opi.tag_BARRIER));
	parser_register_grule ("opi:syncreduce", parser_decode_grule ("ST0T+@tN+00C3R-", opi.tag_SYNC));

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

	dynarray_add (transtbl, dfa_transtotbl ("occampi:primtype +:= [ 0 +@BARRIER 1 ] [ 1 {<opi:barrierreduce>} -* ]"));
	dynarray_add (transtbl, dfa_transtotbl ("occampi:declorprocstart +:= [ 0 +@SYNC 1 ] [ 1 occampi:operand 2 ] [ 2 {<opi:syncreduce>} -* ]"));

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

