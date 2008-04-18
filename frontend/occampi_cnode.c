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


/*{{{  static occampi_ileaveinfo_t *occampi_newileaveinfo (void)*/
/*
 *	creates a blank occampi_ileaveinfo_t structure
 */
static occampi_ileaveinfo_t *occampi_newileaveinfo (void)
{
	occampi_ileaveinfo_t *ilv = (occampi_ileaveinfo_t *)smalloc (sizeof (occampi_ileaveinfo_t));

	dynarray_init (ilv->names);
	dynarray_init (ilv->values);

	return ilv;
}
/*}}}*/
/*{{{  static void occampi_freeileaveinfo (occampi_ileaveinfo_t *ilv)*/
/*
 *	frees an occampi_ileaveinfo_t structure
 */
static void occampi_freeileaveinfo (occampi_ileaveinfo_t *ilv)
{
	int i;

	if (!ilv) {
		nocc_warning ("occampi_freeileaveinfo(): null pointer!");
		return;
	}
	for (i=0; i<DA_CUR (ilv->names); i++) {
		tnode_free (DA_NTHITEM (ilv->names, i));
	}
	for (i=0; i<DA_CUR (ilv->values); i++) {
		tnode_free (DA_NTHITEM (ilv->values, i));
	}
	dynarray_trash (ilv->names);
	dynarray_trash (ilv->values);
	sfree (ilv);
	return;
}
/*}}}*/
/*{{{  static void occampi_ileaveinfo_chook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	dumps an interleave chook tree
 */
static void occampi_ileaveinfo_chook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	occampi_ileaveinfo_t *ilv = (occampi_ileaveinfo_t *)hook;
	int i;

	occampi_isetindent (stream, indent);
	fprintf (stream, "<ileaveinfo addr=\"0x%8.8x\" nnames=\"%d\" nvalues=\"%d\">\n", (unsigned int)ilv, DA_CUR (ilv->names), DA_CUR (ilv->values));
	for (i=0; (i<DA_CUR (ilv->names)) && (i<DA_CUR (ilv->values)); i++) {
		occampi_isetindent (stream, indent + 1);
		fprintf (stream, "<ileaveinfo:namevaluepair>\n");
		tnode_dumptree (DA_NTHITEM (ilv->names, i), indent + 2, stream);
		tnode_dumptree (DA_NTHITEM (ilv->values, i), indent + 2, stream);
		occampi_isetindent (stream, indent + 1);
		fprintf (stream, "</ileaveinfo:namevaluepair>\n");
	}
	occampi_isetindent (stream, indent);
	fprintf (stream, "</ileaveinfo>\n");
	return;
}
/*}}}*/
/*{{{  static void occampi_ileaveinfo_chook_free (void *hook)*/
/*
 *	frees an interleave chook tree
 */
static void occampi_ileaveinfo_chook_free (void *hook)
{
	occampi_ileaveinfo_t *ilv = (occampi_ileaveinfo_t *)hook;

	occampi_freeileaveinfo (ilv);
	return;
}
/*}}}*/
/*{{{  static void *occampi_ileaveinfo_chook_copy (void *hook)*/
/*
 *	copies an interleave chook tree
 */
static void *occampi_ileaveinfo_chook_copy (void *hook)
{
	occampi_ileaveinfo_t *ilv = (occampi_ileaveinfo_t *)hook;
	occampi_ileaveinfo_t *newlv = NULL;

	if (ilv) {
		int i;

		newlv = occampi_newileaveinfo ();
		for (i=0; (i<DA_CUR (ilv->names)) && (i<DA_CUR (ilv->values)); i++) {
			tnode_t *namecopy, *valuecopy;

			namecopy = tnode_copytree (DA_NTHITEM (ilv->names, i));
			valuecopy = tnode_copytree (DA_NTHITEM (ilv->values, i));

			dynarray_add (newlv->names, namecopy);
			dynarray_add (newlv->values, valuecopy);
		}
	}

	return (void *)newlv;
}
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
/*{{{  static void occampi_reduce_replcnode (dfastate_t *dfast, parsepriv_t *pp, void *rarg)*/
/*
 *	reduces a replicated constructor node (e.g. REPLSEQ, REPLPAR)
 */
static void occampi_reduce_replcnode (dfastate_t *dfast, parsepriv_t *pp, void *rarg)
{
	token_t *tok;
	ntdef_t *tag;
	tnode_t *rname, *rstart, *rlength;

	tok = parser_gettok (pp);
	tag = tnode_lookupnodetag (tok->u.kw->name);
	if (tag == opi.tag_SEQ) {
		tag = opi.tag_REPLSEQ;
	} else if (tag == opi.tag_PAR) {
		tag = opi.tag_REPLPAR;
	} else {
		parser_error (pp->lf, "occampi_reduce_replcnode(): unknown tag to replicate [%s]", tag->name);
		return;
	}
	rlength = dfa_popnode (dfast);
	rstart = dfa_popnode (dfast);
	rname = dfa_popnode (dfast);
	*(dfast->ptr) = tnode_create (tag, tok->origin, NULL, NULL, rname, rstart, rlength);
	lexer_freetoken (tok);

	return;
}
/*}}}*/
/*{{{  static void occampi_reduce_ileave (dfastate_t *dfast, parsepriv_t *pp, void *rarg)*/
/*
 *	takes two nodes off the nodestack and adds them to a occampi_ileaveinfo_t chook in the PAR result node
 */
static void occampi_reduce_ileave (dfastate_t *dfast, parsepriv_t *pp, void *rarg)
{
	tnode_t *value = dfa_popnode (dfast);
	tnode_t *name = dfa_popnode (dfast);
	tnode_t *parnode = *(dfast->ptr);
	occampi_ileaveinfo_t *ilv = (occampi_ileaveinfo_t *)tnode_getchook (parnode, opi.chook_ileaveinfo);

	if (!ilv) {
		ilv = occampi_newileaveinfo ();
		tnode_setchook (parnode, opi.chook_ileaveinfo, (void *)ilv);
	}
	dynarray_add (ilv->names, name);
	dynarray_add (ilv->values, value);

	return;
}
/*}}}*/


/*{{{  static int occampi_cnode_dousagecheck (langops_t *lops, tnode_t *node, uchk_state_t *ucstate)*/
/*
 *	does usage-checking for a CNODE
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_cnode_dousagecheck (langops_t *lops, tnode_t *node, uchk_state_t *ucstate)
{
	if (node->tag == opi.tag_PAR) {
		/*{{{  usage-check PAR bodies*/
		tnode_t *body = tnode_nthsubof (node, 1);
		tnode_t **bodies;
		int nbodies, i;
		tnode_t *parnode = node;

		if (!parser_islistnode (body)) {
			nocc_internal ("occampi_cnode_dousagecheck(): body of PAR not list");
			return 0;
		}

		usagecheck_begin_branches (node, ucstate);

		bodies = parser_getlistitems (body, &nbodies);
#if 0
nocc_message ("occampi_cnode_dousagecheck(): there are %d PAR bodies", nbodies);
#endif
		for (i=0; i<nbodies; i++) {
			/*{{{  do usage-checks on subnode*/
			usagecheck_branch (bodies[i], ucstate);
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
/*{{{  static int occampi_scopein_cnode (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	scopes in a constructor node
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_scopein_cnode (compops_t *cops, tnode_t **node, scope_t *ss)
{
	if ((*node)->tag == opi.tag_PAR) {
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
/*{{{  static int occampi_scopeout_cnode (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	scopes out a constructor node
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_scopeout_cnode (compops_t *cops, tnode_t **node, scope_t *ss)
{
	return 1;
}
/*}}}*/
/*{{{  static int occampi_typecheck_cnode (compops_t *cops, tnode_t *node, typecheck_t *tc)*/
/*
 *	does type-checking on a constructor node (SEQ, PAR)
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_typecheck_cnode (compops_t *cops, tnode_t *node, typecheck_t *tc)
{
	if (node->tag == opi.tag_PAR) {
		occampi_ileaveinfo_t *ilv = (occampi_ileaveinfo_t *)tnode_getchook (node, opi.chook_ileaveinfo);

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
/*{{{  static int occampi_constprop_cnode (compops_t *cops, tnode_t **tptr)*/
/*
 *	does constant propagation for a constructor node (SEQ, PAR)
 *	return 0 to stop walk, 1 to continue
 */
static int occampi_constprop_cnode (compops_t *cops, tnode_t **tptr)
{
	if ((*tptr)->tag == opi.tag_PAR) {
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
/*{{{  static int occampi_tracescheck_cnode (compops_t *cops, tnode_t *tptr, tchk_state_t *tcstate)*/
/*
 *	does traces checking on a SEQ or PAR node
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_tracescheck_cnode (compops_t *cops, tnode_t *tptr, tchk_state_t *tcstate)
{
	tchk_bucket_t *tcb;
	tchknode_t *tcn;
	int i;

	/* collect up SEQ or PAR individual items */
	tracescheck_pushbucket (tcstate);
	tracescheck_subtree (tnode_nthsubof (tptr, 1), tcstate);
	tcb = tracescheck_pullbucket (tcstate);

	tcn = tracescheck_createnode ((tptr->tag == opi.tag_PAR) ? TCN_PAR : TCN_SEQ, tptr, NULL);
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
/*{{{  static int occampi_betrans_cnode (compops_t *cops, tnode_t **tptr, betrans_t *be)*/
/*
 *	does back-end transforms for constructor nodes (SEQ, PAR)
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_betrans_cnode (compops_t *cops, tnode_t **tptr, betrans_t *be)
{
	int nbodies;
	tnode_t **bodies;
	
	if (!parser_islistnode (tnode_nthsubof (*tptr, 1))) {
		nocc_internal ("occampi_betrans_cnode(): body of cnode not list");
		return 1;
	}
	bodies = parser_getlistitems (tnode_nthsubof (*tptr, 1), &nbodies);

	if (nbodies == 1) {
		tnode_t *node = *tptr;

		betrans_subtree (bodies, be);
		*tptr = bodies[0];
		bodies[0] = NULL;

		tnode_free (node);
		return 0;
	}
	return 1;
}
/*}}}*/
/*{{{  static int occampi_namemap_cnode (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	does name mapping for constructor nodes (SEQ, PAR)
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_namemap_cnode (compops_t *cops, tnode_t **node, map_t *map)
{
	if ((*node)->tag == opi.tag_SEQ) {
		/* SEQ nodes don't need any special processing */
		return 1;
	} else if ((*node)->tag == opi.tag_PAR) {
		/*{{{  map PAR bodies*/
		tnode_t *body = tnode_nthsubof (*node, 1);
		tnode_t **bodies;
		int nbodies, i;
		tnode_t *parnode = *node;

		if (!parser_islistnode (body)) {
			nocc_internal ("occampi_namemap_cnode(): body of PAR not list");
			return 1;
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

		if (nbodies > 1) {
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
		nocc_internal ("occampi_namemap_cnode(): don\'t know how to handle tag [%s]", (*node)->tag->name);
	}
	return 0;
}
/*}}}*/
/*{{{  static int occampi_codegen_cnode (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	does code-generation for certain conditional-nodes
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_codegen_cnode (compops_t *cops, tnode_t *node, codegen_t *cgen)
{
	if (node->tag == opi.tag_SEQ) {
		/* SEQ nodes don't need any special processing */
		return 1;
	} else if (node->tag == opi.tag_PAR) {
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
				/* skip setup for first process, do last */
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

#if 1
fprintf (stderr, "occampi_codegen_cnode(): PAR: %d,%d,%d,%d,%d (for STARTP %d)\n", ws_size, ws_offset, vs_size, ms_size, adjust, i);
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
			tnode_t *statics = tnode_nthsubof (bodies[0], 1);

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
			cgen->target->be_getblocksize (bodies[0], &ws_size, &ws_offset, &vs_size, &ms_size, &adjust, &elab);
			codegen_callops (cgen, wsadjust, -adjust);			/* XXX: check this! */
			codegen_callops (cgen, branch, I_J, elab);
		}
		/*}}}*/
		pp_wsoffs = 0;
		for (i=0; i<nbodies; i++) {
			/*{{{  PAR body*/
			int ws_size, vs_size, ms_size;
			int ws_offset, adjust, elab;

			cgen->target->be_getblocksize (bodies[i], &ws_size, &ws_offset, &vs_size, &ms_size, &adjust, &elab);
			codegen_callops (cgen, comment, "PAR = %d,%d,%d,%d,%d", ws_size, ws_offset, vs_size, ms_size, adjust);

			codegen_subcodegen (bodies[i], cgen);

			codegen_callops (cgen, loadpointer, parspaceref, pp_wsoffs + adjust);
			codegen_callops (cgen, tsecondary, I_ENDP);
			/*}}}*/

			pp_wsoffs += ws_size;
		}
		/*}}}*/
		/*{{{  PAR cleanup*/
		codegen_callops (cgen, setlabel, joinlab);
		/*}}}*/
	} else {
		nocc_internal ("occampi_codegen_cnode(): don\'t know how to handle tag [%s]", node->tag->name);
	}
	return 0;
}
/*}}}*/


/*{{{  static int occampi_replcnode_dousagecheck (langops_t *lops, tnode_t *node, uchk_state_t *ucstate)*/
/*
 *	does usage-checking for a replicated constructor-node (REPLSEQ, REPLPAR)
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_replcnode_dousagecheck (langops_t *lops, tnode_t *node, uchk_state_t *ucstate)
{
	return 1;
}
/*}}}*/
/*{{{  static int occampi_scopein_replcnode (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	scopes in a replicated constructor node
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_scopein_replcnode (compops_t *cops, tnode_t **node, scope_t *ss)
{
	tnode_t *replname = tnode_nthsubof (*node, 2);
	tnode_t *type = tnode_create (opi.tag_INT, NULL);
	char *rawname;
	tnode_t *newname;
	name_t *sname = NULL;

	if (replname->tag != opi.tag_NAME) {
		scope_error (replname, ss, "occampi_scopein_replcnode(): name not raw-name!");
		return 0;
	}
	rawname = (char *)tnode_nthhookof (replname, 0);

	/* scope the start and length expressions */
	if (scope_subtree (tnode_nthsubaddr (*node, 3), ss)) {
		/* failed to scope start */
		return 0;
	}
	if (scope_subtree (tnode_nthsubaddr (*node, 4), ss)) {
		/* failed to scope length */
		return 0;
	}

	sname = name_addscopename (rawname, *node, type, NULL);
	newname = tnode_createfrom (opi.tag_NREPL, replname, sname);
	SetNameNode (sname, newname);
	tnode_setnthsub (*node, 2, newname);

	/* free the old name */
	tnode_free (replname);
	ss->scoped++;

	return 1;
}
/*}}}*/
/*{{{  static int occampi_scopeout_replcnode (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	scopes out a replicated constructor node
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_scopeout_replcnode (compops_t *cops, tnode_t **node, scope_t *ss)
{
	tnode_t *replname = tnode_nthsubof (*node, 2);
	name_t *sname;

	if (replname->tag != opi.tag_NREPL) {
		scope_error (replname, ss, "not NREPL!");
		return 0;
	}
	sname = tnode_nthnameof (replname, 0);

	name_descopename (sname);

	return 1;
}
/*}}}*/
/*{{{  static int occampi_typecheck_replcnode (compops_t *cops, tnode_t *node, typecheck_t *tc)*/
/*
 *	does type-checking on a replicated constructor node (REPLSEQ, REPLPAR)
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_typecheck_replcnode (compops_t *cops, tnode_t *node, typecheck_t *tc)
{
	tnode_t *start = tnode_nthsubof (node, 3);
	tnode_t *length = tnode_nthsubof (node, 4);
	tnode_t *type;
	tnode_t *defaulttype = tnode_create (opi.tag_INT, NULL);

	/* typecheck start and length first */
	typecheck_subtree (start, tc);
	typecheck_subtree (length, tc);

	type = typecheck_gettype (start, defaulttype);
	if (!type || !typecheck_typeactual (defaulttype, type, node, tc)) {
		typecheck_error (node, tc, "replicator start must be integer");
	}

	type = typecheck_gettype (length, defaulttype);
	if (!type || !typecheck_typeactual (defaulttype, type, node, tc)) {
		typecheck_error (node, tc, "replicator length must be integer");
	}

	tnode_free (defaulttype);

	return 1;
}
/*}}}*/
/*{{{  static int occampi_constprop_replcnode (compops_t *cops, tnode_t **tptr)*/
/*
 *	does constant propagation for a replicated constructor node (REPLSEQ, REPLPAR)
 *	returns 0 to stop walk, 1 to continue (post walk)
 */
static int occampi_constprop_replcnode (compops_t *cops, tnode_t **tptr)
{
	tnode_t **startp = tnode_nthsubaddr (*tptr, 3);
	tnode_t **lengthp = tnode_nthsubaddr (*tptr, 4);

	if ((*tptr)->tag == opi.tag_REPLPAR) {
		/*{{{  length must be constant*/
		if (!constprop_isconst (*lengthp)) {
			constprop_error (*tptr, "replicator length must be constant");
		}
		/*}}}*/
	}
	return 1;
}
/*}}}*/
/*{{{  static int occampi_namemap_replcnode (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	does name-mapping for replicated constructor nodes (REPLSEQ, REPLPAR)
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_namemap_replcnode (compops_t *cops, tnode_t **node, map_t *map)
{
	tnode_t *orgnode = *node;
	tnode_t **namep = tnode_nthsubaddr (*node, 2);
	tnode_t **bodyp = tnode_nthsubaddr (*node, 1);
	int tsize = map->target->intsize;
	tnode_t *bename;

	/* map the start and length expressions first */
	map_submapnames (tnode_nthsubaddr (*node, 3), map);
	map_submapnames (tnode_nthsubaddr (*node, 4), map);

	bename = map->target->newname (*namep, *node, map, tsize * 2, 0, 0, 0, tsize, 0);
	tnode_setchook (*namep, map->mapchook, (void *)bename);
	*node = bename;

	/* map the name in the replicator, turning it into a NAMEREF */
	map_submapnames (namep, map);

	if (orgnode->tag == opi.tag_REPLPAR) {
		tnode_t *blk, *params = NULL;
		tnode_t *parbodyspace;
		/* tnode_t *saved_blk = map->thisblock;
		tnode_t **saved_params = map->thisprocparams; */
		int replcount = constprop_intvalof (tnode_nthsubof (orgnode, 4));

		blk = map->target->newblock (tnode_nthsubof (bename, 1), map, NULL, map->lexlevel + 1);
		map_pushlexlevel (map, blk, &params);
		/* map->thisblock = blk;
		 * map->thisprocparams = &params;
		 * map->lexlevel++; */

		/* map original body in new lexlevel */
		map_submapnames (bodyp, map);
		parbodyspace = map->target->newname (tnode_create (opi.tag_PARSPACE, NULL), *bodyp, map, 0, 16, 0, 0, 0, 0);		/* FIXME! */
		*bodyp = parbodyspace;

		map_poplexlevel (map);
		/* map->lexlevel--;
		 * map->thisprocparams = saved_params;
		 * map->thisblock = saved_blk; */

		/* attach block to the earlier name */
		tnode_setnthsub (bename, 1, blk);

		/*{{{  local space for parallel processes*/
		bodyp = tnode_nthsubaddr (bename, 1);

		*bodyp = map->target->newblockref (*bodyp, *bodyp, map);
		/*}}}*/
	} else if (orgnode->tag == opi.tag_REPLSEQ) {
		/* map the body (original, not what we just placed) */
		map_submapnames (bodyp, map);
	}

	return 0;
}
/*}}}*/
/*{{{  static int occampi_codegen_replcnode (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	does code-generation for replicated constructor nodes (REPLSEQ, REPLPAR)
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_codegen_replcnode (compops_t *cops, tnode_t *node, codegen_t *cgen)
{
	tnode_t *replname = tnode_nthsubof (node, 2);
	tnode_t *start = tnode_nthsubof (node, 3);
	tnode_t *length = tnode_nthsubof (node, 4);
	int hlab, tlab;

	codegen_callops (cgen, loadname, start, 0);
	codegen_callops (cgen, storename, replname, 0);
	codegen_callops (cgen, loadname, length, 0);
	codegen_callops (cgen, storename, replname, cgen->target->intsize);

	hlab = codegen_new_label (cgen);
	tlab = codegen_new_label (cgen);

	codegen_callops (cgen, setlabel, hlab);
	codegen_callops (cgen, loadname, replname, cgen->target->intsize);
	codegen_callops (cgen, branch, I_CJ, tlab);
	
	/* generate the replicated body */
	codegen_subcodegen (tnode_nthsubof (node, 1), cgen);

	codegen_callops (cgen, loadname, replname, 0);
	codegen_callops (cgen, addconst, 1);
	codegen_callops (cgen, storename, replname, 0);
	codegen_callops (cgen, loadname, replname, cgen->target->intsize);
	codegen_callops (cgen, addconst, -1);
	codegen_callops (cgen, storename, replname, cgen->target->intsize);
	codegen_callops (cgen, branch, I_J, hlab);

	codegen_callops (cgen, setlabel, tlab);
	return 0;
}
/*}}}*/


/*{{{  static int occampi_typecheck_cexpnode (compops_t *cops, tnode_t *node, typecheck_t *tc)*/
/*
 *	does type-checking for a cexpnode
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_typecheck_cexpnode (compops_t *cops, tnode_t *node, typecheck_t *tc)
{
	tnode_t *littype;

	typecheck_subtree (tnode_nthsubof (node, 0), tc);

	littype = typecheck_gettype (tnode_nthsubof (node, 0), NULL);
	if (!littype || (littype->tag != opi.tag_BOOL)) {
		typecheck_error (node, tc, "boolean expression expected");
	}

	return 1;
}
/*}}}*/
/*{{{  static int occampi_tracescheck_cexpnode (compops_t *cops, tnode_t *tptr, tchk_state_t *tcstate)*/
/*
 *	does traces checking on a WHILE or SHORTIF node
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_tracescheck_cexpnode (compops_t *cops, tnode_t *tptr, tchk_state_t *tcstate)
{
	tnode_t *expr = tnode_nthsubof (tptr, 0);
	tnode_t *body = tnode_nthsubof (tptr, 1);
	tchk_bucket_t *tcb;
	int isconst;
	int val = 0;
	tchknode_t *tcn = NULL;

	/* determine traces of the body */
	tracescheck_pushbucket (tcstate);
	tracescheck_subtree (body, tcstate);
	tcb = tracescheck_pullbucket (tcstate);

	isconst = langops_isconst (expr);
	if (isconst) {
		if (!langops_constvalof (expr, &val)) {
#if 0
fprintf (stderr, "occampi_tracescheck_cexpnode(): WHILE: failed to get constant value of expr:\n");
tnode_dumptree (expr, 1, stderr);
#endif
			isconst = 0;
		}
	}

	if (tptr->tag == opi.tag_WHILE) {
#if 0
fprintf (stderr, "occampi_tracescheck_cexpnode(): WHILE: isconst=%d, val=%d\n", isconst, val);
#endif
		/*{{{  WHILE -- repeating actions*/
		if (isconst && val) {
			/* infinite loop */
			if (!DA_CUR (tcb->items)) {
				/* infinite loop with no items -- divergence */
				tcn = tracescheck_createnode (TCN_DIV, tptr);
			} else if (DA_CUR (tcb->items) == 1) {
				/* infinite loop with a single item */
				tchknode_t *atom = tracescheck_createatom ();

				tcn = DA_NTHITEM (tcb->items, 0);
				DA_SETNTHITEM (tcb->items, 0, NULL);
				tcn = tracescheck_createnode (TCN_FIXPOINT, tptr, atom,
						tracescheck_createnode (TCN_SEQ, tptr, tcn,
							tracescheck_createnode (TCN_ATOMREF, tptr, atom),
							NULL));
			} else {
				/* infinite loop with multiple items -- unexpected! */
				tracescheck_error (tptr, tcstate, "occampi_tracescheck_cexpnode(): (infinite) %d traces in bucket",
						DA_CUR (tcb->items));
				tracescheck_freebucket (tcb);
				return 0;
			}
		} else if (isconst && !val) {
			/* zero-loop -- doesn't matter what the body does */
		} else {
			/* indeterminate loop */
			if (!DA_CUR (tcb->items)) {
				/* indeterminate loop with no items -- can behave like divergence */
				tchknode_t *atom = tracescheck_createatom ();

				tcn = tracescheck_createnode (TCN_FIXPOINT, tptr, atom,
						tracescheck_createnode (TCN_NDET, tptr,
							tracescheck_createnode (TCN_SKIP, tptr),
							tracescheck_createnode (TCN_ATOMREF, tptr, atom),
							NULL));
			} else if (DA_CUR (tcb->items) == 1) {
				/* indeterminate loop with a single item */
				tchknode_t *atom = tracescheck_createatom ();

				tcn = DA_NTHITEM (tcb->items, 0);
				DA_SETNTHITEM (tcb->items, 0, NULL);
				tcn = tracescheck_createnode (TCN_FIXPOINT, tptr, atom,
						tracescheck_createnode (TCN_NDET, tptr,
							tcn,
							tracescheck_createnode (TCN_ATOMREF, tptr, atom),
							NULL));
			} else {
				/* indeterminate loop with multiple items -- unexpected! */
				tracescheck_error (tptr, tcstate, "occampi_tracescheck_cexpnode(): (indeterminate) %d traces in bucket",
						DA_CUR (tcb->items));
				tracescheck_freebucket (tcb);
				return 0;
			}
		}
		/*}}}*/
	} else if (tptr->tag == opi.tag_SHORTIF) {
		/*{{{  SHORTIF -- conditional*/
		if (isconst && val) {
			/* definite action */
			if (!DA_CUR (tcb->items)) {
				/* nothing! */
			} else if (DA_CUR (tcb->items) == 1) {
				tcn = DA_NTHITEM (tcb->items, 0);
				DA_SETNTHITEM (tcb->items, 0, NULL);
			} else {
				tracescheck_error (tptr, tcstate, "occampi_tracescheck_cexpnode(): (definite) %d traces in bucket",
						DA_CUR (tcb->items));
				tracescheck_freebucket (tcb);
				return 0;
			}
		} else if (isconst && !val) {
			/* definite non-action */
		} else {
			/* indefinite action */
			if (!DA_CUR (tcb->items)) {
				/* nothing! */
			} else if (DA_CUR (tcb->items) == 1) {
				tcn = DA_NTHITEM (tcb->items, 0);
				DA_SETNTHITEM (tcb->items, 0, NULL);

				tcn = tracescheck_createnode (TCN_NDET, tptr,
						tcn,
						tracescheck_createnode (TCN_SKIP, tptr),
						NULL);
			} else {
				tracescheck_error (tptr, tcstate, "occampi_tracescheck_cexpnode(): (indefinite) %d traces in bucket",
						DA_CUR (tcb->items));
				tracescheck_freebucket (tcb);
				return 0;
			}
		}
		/*}}}*/
	}
	tracescheck_freebucket (tcb);
	if (tcn) {
		tracescheck_addtobucket (tcstate, tcn);
	}
	return 0;
}
/*}}}*/
/*{{{  static int occampi_namemap_cexpnode (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	does name-mapping for a cexpnode
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_namemap_cexpnode (compops_t *cops, tnode_t **node, map_t *map)
{
	if ((*node)->tag == opi.tag_SHORTIF) {
		/* no special processing for short IFs */
	} else if ((*node)->tag == opi.tag_WHILE) {
		/* no special processing for WHILEs either */
	} else {
		nocc_internal ("occampi_namemap_cexpnode(): don\'t know how to handle tag [%s]", (*node)->tag->name);
	}
	return 1;
}
/*}}}*/
/*{{{  static int occampi_codegen_cexpnode (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	does code-gen for a cexpnode
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_codegen_cexpnode (compops_t *cops, tnode_t *node, codegen_t *cgen)
{
	if (node->tag == opi.tag_SHORTIF) {
		/*{{{  generate code for a short IF*/
		int joinlab = codegen_new_label (cgen);
		tnode_t *cond = tnode_nthsubof (node, 0);
		tnode_t *body = tnode_nthsubof (node, 1);

		codegen_callops (cgen, loadname, cond, 0);
		codegen_callops (cgen, branch, I_CJ, joinlab);
		codegen_subcodegen (body, cgen);
		codegen_callops (cgen, setlabel, joinlab);
		/*}}}*/
	} else if (node->tag == opi.tag_WHILE) {
		int looplab = codegen_new_label (cgen);
		int exitlab = codegen_new_label (cgen);
		tnode_t *cond = tnode_nthsubof (node, 0);
		tnode_t *body = tnode_nthsubof (node, 1);

		codegen_callops (cgen, setlabel, looplab);
		codegen_callops (cgen, loadname, cond, 0);
		codegen_callops (cgen, branch, I_CJ, exitlab);

		codegen_subcodegen (body, cgen);
		codegen_callops (cgen, branch, I_J, looplab);
		codegen_callops (cgen, setlabel, exitlab);
	} else {
		nocc_internal ("occampi_codegen_cexpnode(): don\'t know how to handle tag [%s]", node->tag->name);
	}
	return 0;
}
/*}}}*/


/*{{{  static int occampi_cnode_init_nodes (void)*/
/*
 *	initialises constructor-process nodes for occam-pi
 *	return 0 on success, non-zero on failure
 */
static int occampi_cnode_init_nodes (void)
{
	tndef_t *tnd;
	int i;
	compops_t *cops;
	langops_t *lops;

	/*{{{  register reduction functions*/
	fcnlib_addfcn ("occampi_reduce_cnode", (void *)occampi_reduce_cnode, 0, 3);
	fcnlib_addfcn ("occampi_reduce_replcnode", (void *)occampi_reduce_replcnode, 0, 3);
	fcnlib_addfcn ("occampi_reduce_ileave", (void *)occampi_reduce_ileave, 0, 3);

	/*}}}*/
	/*{{{  ileaveinfo -- compiler hook*/
	opi.chook_ileaveinfo = tnode_lookupornewchook ("ileaveinfo");
	opi.chook_ileaveinfo->chook_dumptree = occampi_ileaveinfo_chook_dumptree;
	opi.chook_ileaveinfo->chook_free = occampi_ileaveinfo_chook_free;
	opi.chook_ileaveinfo->chook_copy = occampi_ileaveinfo_chook_copy;

	/*}}}*/
	/*{{{  occampi:cnode -- SEQ, PAR*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:cnode", &i, 2, 0, 0, TNF_LONGPROC);		/* subnodes: 0 = expr/operand/parspaceref; 1 = body */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (occampi_scopein_cnode));
	tnode_setcompop (cops, "scopeout", 2, COMPOPTYPE (occampi_scopeout_cnode));
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (occampi_typecheck_cnode));
	tnode_setcompop (cops, "constprop", 1, COMPOPTYPE (occampi_constprop_cnode));
	tnode_setcompop (cops, "tracescheck", 2, COMPOPTYPE (occampi_tracescheck_cnode));
	tnode_setcompop (cops, "betrans", 2, COMPOPTYPE (occampi_betrans_cnode));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (occampi_namemap_cnode));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (occampi_codegen_cnode));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "do_usagecheck", 2, LANGOPTYPE (occampi_cnode_dousagecheck));
	tnd->lops = lops;

	i = -1;
	opi.tag_SEQ = tnode_newnodetag ("SEQ", &i, tnd, NTF_INDENTED_PROC_LIST);
	i = -1;
	opi.tag_PAR = tnode_newnodetag ("PAR", &i, tnd, NTF_INDENTED_PROC_LIST);

	/*}}}*/
	/*{{{  occampi:replcnode -- REPLSEQ, REPLPAR*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:replcnode", &i, 5, 0, 0, TNF_LONGPROC);		/* subnodes: 0 = expr/operand/parspaceref; 1 = body; 2 = name; 3 = start; 4 = length */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (occampi_scopein_replcnode));
	tnode_setcompop (cops, "scopeout", 2, COMPOPTYPE (occampi_scopeout_replcnode));
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (occampi_typecheck_replcnode));
	tnode_setcompop (cops, "constprop", 1, COMPOPTYPE (occampi_constprop_replcnode));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (occampi_namemap_replcnode));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (occampi_codegen_replcnode));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "do_usagecheck", 2, LANGOPTYPE (occampi_replcnode_dousagecheck));
	tnd->lops = lops;

	i = -1;
	opi.tag_REPLSEQ = tnode_newnodetag ("REPLSEQ", &i, tnd, NTF_INDENTED_PROC);
	i = -1;
	opi.tag_REPLPAR = tnode_newnodetag ("REPLPAR", &i, tnd, NTF_INDENTED_PROC);

	/*}}}*/
	/*{{{  occampi:cexpnode -- SHORTIF, WHILE*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:cexpnode", &i, 2, 0, 0, TNF_LONGPROC);	/* subnodes: 0 = expr; 1 = body */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (occampi_typecheck_cexpnode));
	tnode_setcompop (cops, "tracescheck", 2, COMPOPTYPE (occampi_tracescheck_cexpnode));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (occampi_namemap_cexpnode));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (occampi_codegen_cexpnode));
	tnd->ops = cops;

	i = -1;
	opi.tag_WHILE = tnode_newnodetag ("WHILE", &i, tnd, NTF_INDENTED_PROC);
	i = -1;
	opi.tag_SHORTIF = tnode_newnodetag ("SHORTIF", &i, tnd, NTF_INDENTED_PROC);

	/*}}}*/
	/*{{{  occampi:leafnode -- PARSPACE*/
	tnd = tnode_lookupnodetype ("occampi:leafnode");

	i = -1;
	opi.tag_PARSPACE = tnode_newnodetag ("PARSPACE", &i, tnd, NTF_NONE);

	/*}}}*/

	return 0;
}
/*}}}*/


/*{{{  occampi_cnode_feunit (feunit_t)*/
feunit_t occampi_cnode_feunit = {
	init_nodes: occampi_cnode_init_nodes,
	reg_reducers: NULL,
	init_dfatrans: NULL,
	post_setup: NULL,
	ident: "occampi-cnode"
};
/*}}}*/

