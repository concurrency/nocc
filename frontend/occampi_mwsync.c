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
#include "constprop.h"
#include "fetrans.h"
#include "betrans.h"
#include "map.h"
#include "target.h"
#include "transputer.h"
#include "codegen.h"
#include "langops.h"
#include "mwsync.h"


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

	if (lops->next && tnode_haslangop_i (lops->next, (int)LOPS_GETTYPE)) {
		return (tnode_t *)tnode_calllangop_i (lops->next, (int)LOPS_GETTYPE, 2, t, defaulttype);
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
		nocc_warning ("occampi_mwsync_leaftype_bytesfor(): unreplaced BARRIER type probably!");
		return 0;
	}

	if (lops->next && tnode_haslangop_i (lops->next, (int)LOPS_BYTESFOR)) {
		return tnode_calllangop_i (lops->next, (int)LOPS_BYTESFOR, 2, t, target);
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

	if (lops->next && tnode_haslangop_i (lops->next, (int)LOPS_ISSIGNED)) {
		return tnode_calllangop_i (lops->next, (int)LOPS_ISSIGNED, 2, t, target);
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
	if (lops->next && tnode_haslangop_i (lops->next, (int)LOPS_GETDESCRIPTOR)) {
		return tnode_calllangop_i (lops->next, (int)LOPS_GETDESCRIPTOR, 2, node, str);
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
		nocc_warning ("occampi_mwsync_leaftype_initialising_decl(): not expecting a BARRIER here..");
		return 0;
	}
	if (lops->next && tnode_haslangop_i (lops->next, (int)LOPS_INITIALISING_DECL)) {
		return tnode_calllangop_i (lops->next, (int)LOPS_INITIALISING_DECL, 3, t, benode, mdata);
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


/*{{{  static int occampi_mwsync_guard_betrans (compops_t *cops, tnode_t **nodep, betrans_t *be)*/
/*
 *	does back-end transforms for a SYNC guard node
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_mwsync_guard_betrans (compops_t *cops, tnode_t **nodep, betrans_t *be)
{
	if ((*nodep)->tag == opi.tag_SYNCGUARD) {
		/* FIXME! */
	} else {
		/* down-stream betrans */
		if (tnode_hascompop (cops->next, "betrans")) {
			return tnode_callcompop (cops->next, "betrans", 2, nodep, be);
		}
	}
	return 1;
}
/*}}}*/
/*{{{  static int occampi_mwsync_guard_namemap (compops_t *cops, tnode_t **nodep, map_t *map)*/
/*
 *	does name-mapping for a SYNC guard node
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_mwsync_guard_namemap (compops_t *cops, tnode_t **nodep, map_t *map)
{
	if ((*nodep)->tag == opi.tag_SYNCGUARD) {
		/* FIXME! */
	} else {
		/* down-stream name-map */
		if (tnode_hascompop (cops->next, "namemap")) {
			return tnode_callcompop (cops->next, "namemap", 2, nodep, map);
		}
	}
	return 1;
}
/*}}}*/
/*{{{  static int occampi_mwsync_codegen_altenable (langops_t *lops, tnode_t *guard, int dlabel, codegen_t *cgen)*/
/*
 *	does code-generation for SYNC guard ALT enable
 *	returns 0 on success, non-zero on failure
 */
static int occampi_mwsync_codegen_altenable (langops_t *lops, tnode_t *guard, int dlabel, codegen_t *cgen)
{
	if (guard->tag == opi.tag_SYNCGUARD) {
		/* FIXME! */
	} else {
		/* down-stream alt-enable */
		if (tnode_haslangop (lops, "codegen_altenable")) {
			return tnode_calllangop (lops->next, "codegen_altenable", 3, guard, dlabel, cgen);
		}
	}
	return 0;
}
/*}}}*/
/*{{{  static int occampi_mwsync_codegen_altdisable (langops_t *lops, tnode_t *guard, int dlabel, int plabel, codegen_t *cgen)*/
/*
 *	does code-generation for SYNC guard ALT disable
 *	returns 0 on success, non-zero on failure
 */
static int occampi_mwsync_codegen_altdisable (langops_t *lops, tnode_t *guard, int dlabel, int plabel, codegen_t *cgen)
{
	if (guard->tag == opi.tag_SYNCGUARD) {
		/* FIXME! */
	} else {
		/* down-stream alt-disable */
		if (tnode_haslangop (lops, "codegen_altdisable")) {
			return tnode_calllangop (lops->next, "codegen_altdisable", 4, guard, dlabel, plabel, cgen);
		}
	}
	return 0;
}
/*}}}*/


/*{{{  static int occampi_mwsync_leaftype_mwsynctrans (compops_t *cops, tnode_t **tptr, mwsynctrans_t *mwi)*/
/*
 *	does mwsync transforms on an occampi:leaftype -- to turn occam-pi BARRIERs into mwsync BARRIERTYPEs
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_mwsync_leaftype_mwsynctrans (compops_t *cops, tnode_t **tptr, mwsynctrans_t *mwi)
{
	if ((*tptr)->tag == opi.tag_BARRIER) {
		mwsync_mwsynctrans_makebarriertype (tptr, mwi);
		return 0;
	}
	return 1;
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
	tnode_t *var_to_remove = NULL;
	tnode_t **bodyp = tnode_nthsubaddr (*tptr, 2);

	if ((name->tag == opi.tag_NDECL) && (NameTypeOf (tnode_nthnameof (name, 0))->tag == opi.tag_BARRIER)) {
		mwsync_transsubtree (tnode_nthsubaddr (*tptr, 1), mwi);		/* transform type */
		SetNameType (tnode_nthnameof (name, 0), tnode_nthsubof (*tptr, 1));

		mwsync_mwsynctrans_pushvar (*tptr, name, &bodyp, opi.tag_NDECL, mwi);
		var_to_remove = *tptr;
	}

	/* walk over body */
	mwsync_transsubtree (bodyp, mwi);

	if (var_to_remove) {
		/* a name got added, remove it */
		mwsync_mwsynctrans_popvar (var_to_remove, mwi);
		var_to_remove = NULL;
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
	tnode_t **paramsp = tnode_nthsubaddr (*tptr, 1);
	tnode_t **bodyp = tnode_nthsubaddr (*tptr, 2);
	tnode_t **nextp = tnode_nthsubaddr (*tptr, 3);
	int mlvl = mwsync_mwsynctrans_pushvarmark (mwi);
	
	/* add any variables in the PROC definition to the variable stack (done in occampi:fparam node) */
	mwsync_mwsynctrans_startnamerefs (mwi);
	mwsync_transsubtree (paramsp, mwi);
	mwsync_mwsynctrans_endnamerefs (mwi);

	/* do PROC body */
	mwsync_transsubtree (bodyp, mwi);

	/* remove parameters */
	mwsync_mwsynctrans_popvarto (mlvl, mwi);

	/* do in-scope process */
	mwsync_transsubtree (nextp, mwi);

	return 0;
}
/*}}}*/
/*{{{  static int occampi_mwsync_fparam_mwsynctrans (compops_t *cops, tnode_t **tptr, mwsynctrans_t *mwi)*/
/*
 *	does multi-way synchronisation transforms for a formal-parameter declaration
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_mwsync_fparam_mwsynctrans (compops_t *cops, tnode_t **tptr, mwsynctrans_t *mwi)
{
	tnode_t *name = tnode_nthsubof (*tptr, 0);

	if ((name->tag == opi.tag_NPARAM) && (NameTypeOf (tnode_nthnameof (name, 0))->tag == opi.tag_BARRIER)) {
		mwsync_transsubtree (tnode_nthsubaddr (*tptr, 1), mwi);			/* transform type */
		SetNameType (tnode_nthnameof (name, 0), tnode_nthsubof (*tptr, 1));

		mwsync_mwsynctrans_pushparam (*tptr, name, mwi);
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

		mwsync_mwsynctrans_nameref (tptr, name, opi.tag_NDECL, mwi);
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
		int nbodies;
		occampi_ileaveinfo_t *ilv = (occampi_ileaveinfo_t *)tnode_getchook (*tptr, opi.chook_ileaveinfo);

		bodies = parser_getlistitems (tnode_nthsubof (parnode, 1), &nbodies);

#if 1
		nocc_message ("occampi_mwsync_cnode_mwsynctrans(): about to do parallel transform");
#endif
		mwsync_mwsynctrans_parallel (parnode, tptr, bodies, nbodies, mwi);
#if 1
		nocc_message ("occampi_mwsync_cnode_mwsynctrans(): done with parallel transform");
#endif
		if (ilv) {
			/* means we have some interleaving info which needs to be taken into account */
			int i;

			for (i=0; i<DA_CUR (ilv->names); i++) {
				mwsyncpbinfo_t *binf = NULL;

				binf = mwsync_findpbinfointree (*tptr, DA_NTHITEM (ilv->names, i));
				if (!binf) {
					tnode_warning (DA_NTHITEM (ilv->names, i), "barrier not used by PAR");
				} else {
					tnode_t *value = DA_NTHITEM (ilv->values, i);

					if (constprop_isconst (value)) {
						int icount = constprop_intvalof (value);

						/* icount is how many we need to synchronise */
						/* FIXME: maybe want to check this for being in a sensible range */
						if (!binf->ecount_expr) {
							/* constant enroll count */
							binf->sadjust = icount - binf->ecount;
						} else {
							/* non-constant enroll count, build an expression */
							binf->sadjust_expr = tnode_createfrom (opi.tag_MINUS, value, value, binf->ecount_expr, tnode_createfrom (opi.tag_INT, NULL));
						}
					} else {
						if (!binf->ecount_expr) {
							/* constant enroll count, but non-const interleave count -- build an expression */
							binf->sadjust_expr = tnode_createfrom (opi.tag_MINUS, value, value,
									constprop_newconst (CONST_INT, NULL, tnode_createfrom (opi.tag_INT, NULL), binf->ecount), tnode_createfrom (opi.tag_INT, NULL));
						} else {
							/* non-constant enroll count, build an expression */
							binf->sadjust_expr = tnode_createfrom (opi.tag_MINUS, value, value, binf->ecount_expr, tnode_createfrom (opi.tag_INT, NULL));
						}
					}
#if 0
					nocc_message ("here!  values[i] = ");
					tnode_dumptree (DA_NTHITEM (ilv->values, i), 1, stderr);
#endif
				}
			}
		}

		return 0;
	}
	return 1;
}
/*}}}*/


/*{{{  static int occampi_mwsync_init_nodes (void)*/
/*
 *	sets up nodes for occam-pi multi-way synchronisations
 *	returns 0 on success, non-zero on failure
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
	tnode_setcompop (tnd->ops, "mwsynctrans", 2, COMPOPTYPE (occampi_mwsync_leaftype_mwsynctrans));

	lops = tnode_insertlangops (tnd->lops);
	tnode_setlangop (lops, "getdescriptor", 2, LANGOPTYPE (occampi_mwsync_leaftype_getdescriptor));
	tnode_setlangop (lops, "gettype", 2, LANGOPTYPE (occampi_mwsync_leaftype_gettype));
	tnode_setlangop (lops, "bytesfor", 2, LANGOPTYPE (occampi_mwsync_leaftype_bytesfor));
	tnode_setlangop (lops, "issigned", 2, LANGOPTYPE (occampi_mwsync_leaftype_issigned));
	tnode_setlangop (lops, "initialising_decl", 3, LANGOPTYPE (occampi_mwsync_leaftype_initialising_decl));
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
	/*{{{  occampi:guardnode -- SYNCGUARD*/
	tnd = tnode_lookupnodetype ("occampi:guardnode");

	cops = tnode_insertcompops (tnd->ops);
	tnode_setcompop (cops, "betrans", 2, COMPOPTYPE (occampi_mwsync_guard_betrans));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (occampi_mwsync_guard_namemap));
	tnd->ops = cops;
	lops = tnode_insertlangops (tnd->lops);
	tnode_setlangop (lops, "codegen_altenable", 3, LANGOPTYPE (occampi_mwsync_codegen_altenable));
	tnode_setlangop (lops, "codegen_altdisable", 4, LANGOPTYPE (occampi_mwsync_codegen_altdisable));
	tnd->lops = lops;

	i = -1;
	opi.tag_SYNCGUARD = tnode_newnodetag ("SYNCGUARD", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  occampi:vardecl -- (mods for barriers)*/
	tnd = tnode_lookupnodetype ("occampi:vardecl");
	tnode_setcompop (tnd->ops, "mwsynctrans", 2, COMPOPTYPE (occampi_mwsync_vardecl_mwsynctrans));

	/*}}}*/
	/*{{{  occampi:procdecl -- (mods for barriers)*/
	tnd = tnode_lookupnodetype ("occampi:procdecl");
	tnode_setcompop (tnd->ops, "mwsynctrans", 2, COMPOPTYPE (occampi_mwsync_procdecl_mwsynctrans));

	/*}}}*/
	/*{{{  occampi:fparam -- (mods for barriers)*/
	tnd = tnode_lookupnodetype ("occampi:fparam");
	tnode_setcompop (tnd->ops, "mwsynctrans", 2, COMPOPTYPE (occampi_mwsync_fparam_mwsynctrans));

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
	parser_register_grule ("opi:syncguardreduce", parser_decode_grule ("ST0T+@tN+00C3R-", opi.tag_SYNCGUARD));

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
	dynarray_add (transtbl, dfa_transtotbl ("occampi:altguard +:= [ 0 +@SYNC 1 ] [ 1 occampi:operand 2 ] [ 2 {<opi:syncguardreduce>} -* ]"));

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

