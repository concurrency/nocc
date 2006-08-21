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
#include "opts.h"
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


/*}}}*/
/*{{{  private data*/

static chook_t *mapchook = NULL;


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
		return lops->next->getdescriptor (lops->next, node, str);
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
/*{{{  static int occampi_mwsync_procdecl_mwsynctrans (compops_t *cops, tnode_t **tptr, mwsynctrans_t *mwi)*/
/*
 *	does multi-way synchronisation transforms for a procedure declaration
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_mwsync_procdecl_mwsynctrans (compops_t *cops, tnode_t **tptr, mwsynctrans_t *mwi)
{
	tnode_t *params = tnode_nthsubof (*tptr, 1);
	tnode_t **bodyp = tnode_nthsubaddr (*tptr, 2);
	tnode_t **nextp = tnode_nthsubaddr (*tptr, 3);

	/* FIXME: add any variables in the PROC definition to the variable stack */

	/* do PROC body */
	mwsync_transsubtree (bodyp, mwi);

	/* FIXME: remove any variables from the PROC definition */

	/* do in-scope process */
	mwsync_transsubtree (nextp, mwi);

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

					nocc_message ("occampi_mwsync_namenode_mwsynctrans(): no pstack, creating PARBARRIER");

					parbardecl = tnode_create (opi.tag_PARBARRIER, NULL, NULL, tnode_create (opi.tag_PARBARRIERTYPE, NULL), NULL, *tptr);
					/* parbarname = tnode_createfrom (opi.tag_NDECL, *tptr, name_addtempname (parbardecl, NULL, NULL, NULL)); */
					name_addtempname (parbardecl, tnode_nthsubof (parbardecl, 1), opi.tag_NDECL, &parbarname);
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

						nocc_message ("occampi_mwsync_namenode_mwsynctrans(): got pstack, creating PARBARRIER");

						parbardecl = tnode_create (opi.tag_PARBARRIER, NULL, NULL, tnode_create (opi.tag_PARBARRIERTYPE, NULL), NULL, *tptr);
						/* parbarname = tnode_createfrom (opi.tag_NDECL, *tptr, name_addtempname (parbardecl, NULL, NULL, NULL)); */
						name_addtempname (parbardecl, tnode_nthsubof (parbardecl, 1), opi.tag_NDECL, &parbarname);
						tnode_setnthsub (parbardecl, 0, parbarname);

						/* stitch it in at the given insert-point */
						if (!DA_NTHITEM (mwps->paripoints, j)) {
							nocc_internal ("occampi_mwsync_namenode_mwsynctrans(): no PARBARRIER insert point!");
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

					nocc_message ("occampi_mwsync_namenode_mwsynctrans(): no bname, creating PROCBARRIER");

					procbardecl = tnode_create (opi.tag_PROCBARRIER, NULL, NULL, tnode_create (opi.tag_PROCBARRIERTYPE, NULL), NULL, parbarname);
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
/*{{{  static int occampi_mwsync_cnode_mwsynctrans (compops_t *cops, tnode_t **tptr, mwsynctrans_t *mwi)*/
/*
 *	does multi-way synchronisation transforms for a PAR (constructor node, occampi:cnode)
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_mwsync_cnode_mwsynctrans (compops_t *cops, tnode_t **tptr, mwsynctrans_t *mwi)
{
	if ((*tptr)->tag == opi.tag_PAR) {
		tnode_t *parnode = *tptr;
		tnode_t **bodies;
		int i, nbodies;

		/* go through each one in turn */
		for (i=0; i<DA_CUR (mwi->varptr); i++) {
			mwsyncpstk_t *mwps = DA_NTHITEM (mwi->pstack, i);

			/* start to add a PAR block for this node */
#if 1
			nocc_message ("occampi_mwsync_cnode_mwsynctrans(): encountered PAR, adding an entry to its pstk");
#endif

			dynarray_add (mwps->parblks, parnode);
			dynarray_add (mwps->paripoints, tptr);		/* if we create a PARBARRIER, want it here */
			dynarray_add (mwps->parbarriers, NULL);
			dynarray_add (mwps->bnames, NULL);
			dynarray_add (mwps->bipoints, NULL);
		}

		bodies = parser_getlistitems (tnode_nthsubof (parnode, 1), &nbodies);
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
#if 1
#endif
		}

		for (i=0; i<DA_CUR (mwi->varptr); i++) {
			mwsyncpstk_t *mwps = DA_NTHITEM (mwi->pstack, i);
			tnode_t *parbarrier;

			/* remove recent PAR */
			if (!DA_CUR (mwps->parblks) || (DA_NTHITEM (mwps->parblks, DA_CUR (mwps->parblks) - 1) != parnode)) {
				nocc_internal ("occampi_mwsync_cnode_mwsynctrans(): erk, not this PAR!");
			}

			dynarray_delitem (mwps->parblks, DA_CUR (mwps->parblks) - 1);
			dynarray_delitem (mwps->paripoints, DA_CUR (mwps->paripoints) - 1);
			dynarray_delitem (mwps->parbarriers, DA_CUR (mwps->parbarriers) - 1);
			dynarray_delitem (mwps->bnames, DA_CUR (mwps->bnames) - 1);
			dynarray_delitem (mwps->bipoints, DA_CUR (mwps->bipoints) - 1);
		}

#if 0
		nocc_message ("occampi_mwsync_cnode_mwsynctrans(): PAR here (DA_CUR (varptr) = %d)! tree is:", DA_CUR (mwi->varptr));
		tnode_dumptree (*tptr, 1, stderr);
#endif

		return 0;
	}
	return 1;
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
	tnode_t *type = tnode_nthsubof (*node, 1);
	tnode_t **bodyp = tnode_nthsubaddr (*node, 2);
	tnode_t **exprp = tnode_nthsubaddr (*node, 3);
	tnode_t *bename;
	int wssize;

	if ((*node)->tag == opi.tag_PARBARRIER) {
		mwsyncpbinfo_t *pbinf = (mwsyncpbinfo_t *)tnode_getchook (*node, mwsyncpbihook);

		wssize = tnode_bytesfor (type, map->target);
		if (pbinf && pbinf->parent) {
			/* FIXME: map out pbinf->parent perhaps */
		}
	} else if ((*node)->tag == opi.tag_PROCBARRIER) {
		wssize = tnode_bytesfor (type, map->target);
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
	} else if (node->tag == opi.tag_PROCBARRIER) {
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

	/*{{{  mapchook -- compiler hook*/
	mapchook = tnode_lookupornewchook ("map:mapnames");

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
	/*{{{  occampi:vardecl -- (mods for barriers)*/
	tnd = tnode_lookupnodetype ("occampi:vardecl");
	tnode_setcompop (tnd->ops, "mwsynctrans", 2, COMPOPTYPE (occampi_mwsync_vardecl_mwsynctrans));

	/*}}}*/
	/*{{{  occampi:procdecl -- (mods for barriers)*/
	tnd = tnode_lookupnodetype ("occampi:procdecl");
	tnode_setcompop (tnd->ops, "mwsynctrans", 2, COMPOPTYPE (occampi_mwsync_procdecl_mwsynctrans));

	/*}}}*/
	/*{{{  occampi:namenode -- (mods for barriers)*/
	tnd = tnode_lookupnodetype ("occampi:namenode");
	tnode_setcompop (tnd->ops, "mwsynctrans", 2, COMPOPTYPE (occampi_mwsync_namenode_mwsynctrans));

	/*}}}*/
	/*{{{  occampi:cnode -- (mods for barriers)*/
	tnd = tnode_lookupnodetype ("occampi:cnode");
	tnode_setcompop (tnd->ops, "mwsynctrans", 2, COMPOPTYPE (occampi_mwsync_cnode_mwsynctrans));

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

