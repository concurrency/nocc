/*
 *	occampi_placedpar.c -- occam-pi PLACED PAR
 *	Copyright (C) 2008 Fred Barnes <frmb@kent.ac.uk>
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
#include "occampi.h"
#include "feunit.h"
#include "names.h"
#include "scope.h"
#include "prescope.h"
#include "typecheck.h"
#include "constprop.h"
#include "tracescheck.h"
#include "langops.h"
#include "usagecheck.h"
#include "fetrans.h"
#include "betrans.h"
#include "map.h"
#include "codegen.h"
#include "target.h"
#include "transputer.h"


/*}}}*/


/*{{{  static int occampi_placedparnode_dousagecheck (langops_t *lops, tnode_t *node, uchk_state_t *ucstate)*/
/*
 *	does usage-checking for a PLACED-PAR node
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_placedparnode_dousagecheck (langops_t *lops, tnode_t *node, uchk_state_t *ucstate)
{
	if (node->tag == opi.tag_PLACEDPAR) {
		/*{{{  usage-check PAR bodies*/
		tnode_t *body = tnode_nthsubof (node, 1);
		tnode_t **bodies;
		int nbodies, i;
		tnode_t *parnode = node;

		if (!parser_islistnode (body)) {
			nocc_internal ("occampi_placedparnode_dousagecheck(): body of PAR not list");
			return 0;
		}

		usagecheck_begin_branches (node, ucstate);

		bodies = parser_getlistitems (body, &nbodies);
#if 0
nocc_message ("occampi_placedparnode_dousagecheck(): there are %d PAR bodies", nbodies);
#endif
		for (i=0; i<nbodies; i++) {
			/*{{{  do usage-checks on subnode*/
			if (bodies[i]->tag == opi.tag_PLACEDON) {
				/* skip PLACEDON */
				usagecheck_branch (tnode_nthsubof (bodies[i], 1), ucstate);
			} else {
				usagecheck_branch (bodies[i], ucstate);
			}

			/*}}}*/
		}

		usagecheck_end_branches (node, ucstate);
		if (!usagecheck_no_overlaps (node, ucstate)) {
			usagecheck_mergeall (node, ucstate);
		} /* else don't merge from something that failed (XXX: maybe..) */
		/*}}}*/
		return 0;
	}
	return 1;
}
/*}}}*/
/*{{{  static int occampi_scopein_placedparnode (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	scopes in a PLACED-PAR node
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_scopein_placedparnode (compops_t *cops, tnode_t **node, scope_t *ss)
{
	if ((*node)->tag == opi.tag_PLACEDPAR) {
		occampi_ileaveinfo_t *ilv = (occampi_ileaveinfo_t *)tnode_getchook (*node, opi.chook_ileaveinfo);

		if (ilv) {
			int i;

			for (i=0; (i<DA_CUR (ilv->names)) && (i<DA_CUR (ilv->values)); i++) {
				scope_subtree (DA_NTHITEMADDR (ilv->names, i), ss);
				scope_subtree (DA_NTHITEMADDR (ilv->values, i), ss);
			}
		}
	}
	return 1;
}
/*}}}*/
/*{{{  static int occampi_scopeout_placedparnode (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	scopes out a PLACED-PAR node
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_scopeout_placedparnode (compops_t *cops, tnode_t **node, scope_t *ss)
{
	return 1;
}
/*}}}*/
/*{{{  static int occampi_typecheck_placedparnode (compops_t *cops, tnode_t *node, typecheck_t *tc)*/
/*
 *	does type-checking on a PLACED PAR
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_typecheck_placedparnode (compops_t *cops, tnode_t *node, typecheck_t *tc)
{
	if (node->tag == opi.tag_PLACEDPAR) {
		occampi_ileaveinfo_t *ilv = (occampi_ileaveinfo_t *)tnode_getchook (node, opi.chook_ileaveinfo);
		int nbodies, i;
		tnode_t **bodies;

		if (!parser_islistnode (tnode_nthsubof (node, 1))) {
			typecheck_error (node, tc, "body of PLACED PAR not a list");
			return 0;
		}

		bodies = parser_getlistitems (tnode_nthsubof (node, 1), &nbodies);
		for (i=0; i<nbodies; i++) {
			if (bodies[i]->tag != opi.tag_PLACEDON) {
				typecheck_error (bodies[i], tc, "expected ON statement in PLACED PAR body");
				return 0;
			}
		}

		if (ilv) {
			tnode_t *definttype = tnode_create (opi.tag_INT, NULL);
			tnode_t *defbartype = tnode_create (opi.tag_BARRIER, NULL);
			int i;

			for (i=0; (i<DA_CUR (ilv->names)) && (i<DA_CUR (ilv->values)); i++) {
				tnode_t *nametype, *valuetype;

				typecheck_subtree (DA_NTHITEM (ilv->names, i), tc);
				typecheck_subtree (DA_NTHITEM (ilv->values, i), tc);

				/* name should be a BARRIER */
				nametype = typecheck_gettype (DA_NTHITEM (ilv->names, i), defbartype);
				if (!nametype || !typecheck_typeactual (defbartype, nametype, node, tc)) {
					typecheck_error (node, tc, "interleaving name must be a BARRIER");
				}

				valuetype = typecheck_gettype (DA_NTHITEM (ilv->values, i), definttype);
				if (!valuetype || !typecheck_typeactual (definttype, valuetype, node, tc)) {
					typecheck_error (node, tc, "interleaving value must be integer");
				}
			}

			tnode_free (definttype);
			tnode_free (defbartype);
		}
	}
	return 1;
}
/*}}}*/
/*{{{  static int occampi_constprop_placedparnode (compops_t *cops, tnode_t **tptr)*/
/*
 *	does constant propagation for a PLACED-PAR node
 *	return 0 to stop walk, 1 to continue
 */
static int occampi_constprop_placedparnode (compops_t *cops, tnode_t **tptr)
{
	if ((*tptr)->tag == opi.tag_PLACEDPAR) {
		occampi_ileaveinfo_t *ilv = (occampi_ileaveinfo_t *)tnode_getchook (*tptr, opi.chook_ileaveinfo);

		if (ilv) {
			int i;

			for (i=0; (i<DA_CUR (ilv->names)) && (i<DA_CUR (ilv->values)); i++) {
				constprop_tree (DA_NTHITEMADDR (ilv->names, i));
				constprop_tree (DA_NTHITEMADDR (ilv->values, i));
			}
		}
	}
	return 1;
}
/*}}}*/
/*{{{  static int occampi_tracescheck_placedparnode (compops_t *cops, tnode_t *tptr, tchk_state_t *tcstate)*/
/*
 *	does traces checking on a PLACED-PAR node
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_tracescheck_placedparnode (compops_t *cops, tnode_t *tptr, tchk_state_t *tcstate)
{
	tchk_bucket_t *tcb;
	tchknode_t *tcn;
	int i;

	/* collect up individual PAR items */
	tracescheck_pushbucket (tcstate);
	tracescheck_subtree (tnode_nthsubof (tptr, 1), tcstate);
	tcb = tracescheck_pullbucket (tcstate);

	tcn = tracescheck_createnode (TCN_PAR, tptr, NULL);
	for (i=0; i<DA_CUR (tcb->items); i++) {
		tchknode_t *item = DA_NTHITEM (tcb->items, i);

		tracescheck_addtolistnode (tcn, item);
	}
	dynarray_trash (tcb->items);
	tracescheck_freebucket (tcb);

	tracescheck_addtobucket (tcstate, tcn);

	return 0;
}
/*}}}*/
/*{{{  static int occampi_betrans_placedparnode (compops_t *cops, tnode_t **tptr, betrans_t *be)*/
/*
 *	does back-end transforms for a PLACED-PAR node
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_betrans_placedparnode (compops_t *cops, tnode_t **tptr, betrans_t *be)
{
	int nbodies;
	tnode_t **bodies;
	
	if (!parser_islistnode (tnode_nthsubof (*tptr, 1))) {
		nocc_internal ("occampi_betrans_placedparnode(): body of placedparnode not list");
		return 1;
	}
	bodies = parser_getlistitems (tnode_nthsubof (*tptr, 1), &nbodies);

	return 1;
}
/*}}}*/
/*{{{  static int occampi_namemap_placedparnode (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	does name mapping for a PLACED PAR
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_namemap_placedparnode (compops_t *cops, tnode_t **node, map_t *map)
{
	if ((*node)->tag == opi.tag_PLACEDPAR) {
		/*{{{  map PLACED-PAR bodies*/
		tnode_t *body = tnode_nthsubof (*node, 1);
		tnode_t **bodies;
		int nbodies, i;
		tnode_t *parnode = *node;

		if (!parser_islistnode (body)) {
			nocc_internal ("occampi_namemap_placedparnode(): body of PAR not list");
			return 1;
		}

		bodies = parser_getlistitems (body, &nbodies);
		if (nbodies > 0) {
			/*{{{  create a blank new body at front-of-list which will reserve space for this PAR-starter*/
			tnode_t *newproc = tnode_create (opi.tag_SKIP, NULL);

			parser_addtolist_front (body, newproc);
			/*}}}*/
		}

		bodies = parser_getlistitems (body, &nbodies);
		for (i=0; i<nbodies; i++) {
			/*{{{  turn body into back-end block*/
			tnode_t *blk, *parbodyspace;
			/* tnode_t *saved_blk = map->thisblock;
			tnode_t **saved_params = map->thisprocparams; */

			blk = map->target->newblock (bodies[i], map, NULL, map->lexlevel + 1);
			map_pushlexlevel (map, blk, NULL);
			/* map->thisblock = blk;
			 * map->thisprocparams = NULL;
			 * map->lexlevel++; */

			/* map body */
			map_submapnames (&(bodies[i]), map);
			parbodyspace = map->target->newname (tnode_create (opi.tag_PARSPACE, NULL), bodies[i], map, 0, map->target->bws.ds_min, 0, 0, 0, 0);
			*(map->target->be_blockbodyaddr (blk)) = parbodyspace;

			map_poplexlevel (map);
			/* map->lexlevel--;
			 * map->thisblock = saved_blk;
			 * map->thisprocparams = saved_params; */

			/* make block node the individual PAR process */
			bodies[i] = blk;
			/*}}}*/
		}

		if (nbodies > 0) {
			/*{{{  make space for PAR*/
			tnode_t *bename, *bodyref, *blist;
			tnode_t *fename = tnode_create (opi.tag_PARSPACE, NULL);

			blist = parser_newlistnode (NULL);
			for (i=0; i<nbodies; i++) {
				parser_addtolist (blist, bodies[i]);
			}
			bodyref = map->target->newblockref (blist, *node, map);

			/* FIXME: maybe: don't want this to float away, but ok if we provided a link to the real successor */
			bename = map->target->newname (fename, bodyref, map, map->target->aws.as_par, map->target->bws.ds_min, 0, 0, 0, 0);
			tnode_setchook (fename, map->mapchook, (void *)bename);

			*node = bename;

			tnode_setnthsub (parnode, 0, map->target->newnameref (bename, map));
			/*}}}*/
		}


		/*}}}*/
	} else {
		nocc_internal ("occampi_namemap_placedparnode(): don\'t know how to handle tag [%s]", (*node)->tag->name);
	}
	return 0;
}
/*}}}*/
/*{{{  static int occampi_codegen_placedparnode (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	does code-generation for a PLACED PAR
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_codegen_placedparnode (compops_t *cops, tnode_t *node, codegen_t *cgen)
{
	if (node->tag == opi.tag_PLACEDPAR) {
		/*{{{  generate code for PAR*/
		tnode_t *body = tnode_nthsubof (node, 1);
		tnode_t **bodies;
		int nbodies, i;
		int joinlab = codegen_new_label (cgen);
		int pp_wsoffs = 0;
		tnode_t *parspaceref = tnode_nthsubof (node, 0);

		bodies = parser_getlistitems (body, &nbodies);
		/*{{{  PAR setup*/
		codegen_callops (cgen, debugline, node);
		codegen_callops (cgen, comment, "BEGIN PAR SETUP");
		for (i=0; i<nbodies; i++) {
			int ws_size, vs_size, ms_size;
			int ws_offset, adjust, elab;
			tnode_t *statics = tnode_nthsubof (bodies[i], 1);

			codegen_check_beblock (bodies[i], cgen, 1);
			cgen->target->be_getblocksize (bodies[i], &ws_size, &ws_offset, &vs_size, &ms_size, &adjust, &elab);

			if (!i) {
				/* skip setup for first process (special case for PLACED PAR) */
			} else {
				/*{{{  setup statics in workspace of PAR process*/
				if (statics && parser_islistnode (statics)) {
					int nitems, p, wsoff;
					tnode_t **items = parser_getlistitems (statics, &nitems);

					for (p=nitems - 1, wsoff = pp_wsoffs-4; p>=0; p--, wsoff -= 4) {
						codegen_callops (cgen, loadparam, items[p], PARAM_REF);
						codegen_callops (cgen, storelocal, wsoff);
					}
				} else if (statics) {
					codegen_callops (cgen, loadparam, statics, PARAM_REF);
					codegen_callops (cgen, storelocal, pp_wsoffs-4);
				}
				/*}}}*/
			}

			pp_wsoffs -= ws_size;
		}
		/*{{{  setup local PAR workspace*/
		codegen_callops (cgen, tsecondary, I_GETPAS);		/* priority and affinity */
		codegen_callops (cgen, storename, parspaceref, 8);
		codegen_callops (cgen, loadconst, nbodies);		/* par-count */
		codegen_callops (cgen, storename, parspaceref, 4);
		codegen_callops (cgen, loadlabaddr, joinlab);		/* join-lab */
		codegen_callops (cgen, storename, parspaceref, 0);

		/*}}}*/
		pp_wsoffs = 0;
		for (i=0; i<nbodies; i++) {
			int ws_size, vs_size, ms_size;
			int ws_offset, adjust, elab;

			codegen_check_beblock (bodies[i], cgen, 1);
			cgen->target->be_getblocksize (bodies[i], &ws_size, &ws_offset, &vs_size, &ms_size, &adjust, &elab);

#if 0
fprintf (stderr, "occampi_codegen_placedparnode(): PAR: %d,%d,%d,%d,%d (for STARTP %d)\n", ws_size, ws_offset, vs_size, ms_size, adjust, i);
#endif
			if (!i) {
				/* don't start the first process, but keep track of where it is */
			} else {
				/*{{{  start PAR process*/
				codegen_callops (cgen, loadlabaddr, elab);
				codegen_callops (cgen, loadlocalpointer, pp_wsoffs - adjust);
				codegen_callops (cgen, tsecondary, I_STARTP);
				/*}}}*/
			}

			pp_wsoffs -= ws_size;
		}
		codegen_callops (cgen, comment, "END PAR SETUP");
		/*}}}*/

		pp_wsoffs = 0;
		/*{{{  process doing PAR becomes the first process*/
		if (nbodies > 0) {
			int ws_size, vs_size, ms_size;
			int ws_offset, adjust, elab;

			/* no meaningful statics here, just do ENDP coding */
			cgen->target->be_getblocksize (bodies[0], &ws_size, &ws_offset, &vs_size, &ms_size, &adjust, &elab);
			codegen_callops (cgen, wsadjust, -adjust);			/* XXX: check this! */

			codegen_callops (cgen, loadpointer, parspaceref, adjust);
			codegen_callops (cgen, tsecondary, I_ENDP);
		}
		/*}}}*/

		pp_wsoffs = 0;
		for (i=0; i<nbodies; i++) {
			/*{{{  PAR body*/
			int ws_size, vs_size, ms_size;
			int ws_offset, adjust, elab;

			cgen->target->be_getblocksize (bodies[i], &ws_size, &ws_offset, &vs_size, &ms_size, &adjust, &elab);
			if (!i) {
				codegen_callops (cgen, comment, "PAR = %d,%d,%d,%d,%d (skipped)", ws_size, ws_offset, vs_size, ms_size, adjust);
			} else {
				codegen_callops (cgen, comment, "PAR = %d,%d,%d,%d,%d", ws_size, ws_offset, vs_size, ms_size, adjust);

				codegen_subcodegen (bodies[i], cgen);

				codegen_callops (cgen, loadpointer, parspaceref, pp_wsoffs + adjust);
				codegen_callops (cgen, tsecondary, I_ENDP);
			}
			/*}}}*/

			pp_wsoffs += ws_size;
		}
		/*}}}*/
		/*{{{  PAR cleanup*/
		codegen_callops (cgen, setlabel, joinlab);
		/*}}}*/
	} else {
		nocc_internal ("occampi_codegen_placedparnode(): don\'t know how to handle tag [%s]", node->tag->name);
	}
	return 0;
}
/*}}}*/


/*{{{  static int occampi_codegen_placedonnode (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	does code-gen for a PLACED-ON node
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_codegen_placedonnode (compops_t *cops, tnode_t *node, codegen_t *cgen)
{
	/* skip this for now */
	codegen_subcodegen (tnode_nthsubof (node, 1), cgen);
	return 0;
}
/*}}}*/


/*{{{  static int occampi_placedpar_init_nodes (void)*/
/*
 *	initialises placed-par nodes for occam-pi
 *	returns 0 on success, non-zero on failure
 */
static int occampi_placedpar_init_nodes (void)
{
	tndef_t *tnd;
	int i;
	compops_t *cops;
	langops_t *lops;

	/*{{{  occam:placedparnode -- PLACEDPAR*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:placedparnode", &i, 2, 0, 0, TNF_LONGPROC);		/* subnodes: 0 = expr/operand/parspaceref; 1 = body */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (occampi_scopein_placedparnode));
	tnode_setcompop (cops, "scopeout", 2, COMPOPTYPE (occampi_scopeout_placedparnode));
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (occampi_typecheck_placedparnode));
	tnode_setcompop (cops, "constprop", 1, COMPOPTYPE (occampi_constprop_placedparnode));
	tnode_setcompop (cops, "tracescheck", 2, COMPOPTYPE (occampi_tracescheck_placedparnode));
	tnode_setcompop (cops, "betrans", 2, COMPOPTYPE (occampi_betrans_placedparnode));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (occampi_namemap_placedparnode));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (occampi_codegen_placedparnode));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "do_usagecheck", 2, LANGOPTYPE (occampi_placedparnode_dousagecheck));
	tnd->lops = lops;

	i = -1;
	opi.tag_PLACEDPAR = tnode_newnodetag ("PLACEDPAR", &i, tnd, NTF_INDENTED_PLACEDON_LIST);

	/*}}}*/
	/*{{{  occampi:placedonnode -- PLACEDON*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:placedonnode", &i, 2, 0, 0, TNF_LONGPROC);		/* subnodes: 0 = expr; 1 = body */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (occampi_codegen_placedonnode));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnd->lops = lops;

	i = -1;
	opi.tag_PLACEDON = tnode_newnodetag ("PLACEDON", &i, tnd, NTF_INDENTED_PROC);

	/*}}}*/

	return 0;
}
/*}}}*/
/*{{{  static int occampi_placedpar_post_setup (void)*/
/*
 *	does post-setup for placed-par nodes in occam-pi
 *	returns 0 on success, non-zero on failure
 */
static int occampi_placedpar_post_setup (void)
{
	return 0;
}
/*}}}*/


/*{{{  occampi_placedpar_feunit (feunit_t)*/
feunit_t occampi_placedpar_feunit = {
	init_nodes: occampi_placedpar_init_nodes,
	reg_reducers: NULL,
	init_dfatrans: NULL,
	post_setup: occampi_placedpar_post_setup,
	ident: "occampi-placedpar"
};
/*}}}*/

