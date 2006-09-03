/*
 *	mcsp_cnode.c -- constructor node handling for MCSP
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
#include "mcsp.h"
#include "feunit.h"
#include "names.h"
#include "scope.h"
#include "prescope.h"
#include "constprop.h"
#include "typecheck.h"
#include "usagecheck.h"
#include "fetrans.h"
#include "betrans.h"
#include "map.h"
#include "codegen.h"
#include "target.h"
#include "transputer.h"


/*}}}*/


/*{{{  static int mcsp_fetrans_cnode (compops_t *cops, tnode_t **node, fetrans_t *fe)*/
/*
 *	does front-end transforms on SEQ/PAR nodes (pass 1+ only)
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_fetrans_cnode (compops_t *cops, tnode_t **node, fetrans_t *fe)
{
	mcsp_fetrans_t *mfe = (mcsp_fetrans_t *)fe->langpriv;
	tnode_t *t = *node;

	switch (mfe->parse) {
	case 0:
		/* nothing in this pass! */
		break;
	case 1:
		if (t->tag == mcsp.tag_SEQCODE) {
			/*{{{  flatten SEQCODEs*/
			tnode_t *slist = tnode_nthsubof (t, 1);
			tnode_t **procs;
			int nprocs, i;
			tnode_t *newlist = parser_newlistnode (NULL);

			procs = parser_getlistitems (slist, &nprocs);
			for (i=0; i<nprocs; i++) {
				if (procs[i] && (procs[i]->tag == mcsp.tag_SEQCODE)) {
					tnode_t *xslist;
					tnode_t **xprocs;
					int nxprocs, j;

					/* flatten */
					fetrans_subtree (&procs[i], fe);

					xslist = tnode_nthsubof (procs[i], 1);
					xprocs = parser_getlistitems (xslist, &nxprocs);
					for (j=0; j<nxprocs; j++) {
						parser_addtolist (newlist, xprocs[j]);
						xprocs[j] = NULL;
					}

					/* assume we got them all */
					tnode_free (procs[i]);
					procs[i] = NULL;
				} else {
					/* move over to new list after sub-fetrans (preserves order) */
					fetrans_subtree (&procs[i], fe);

					parser_addtolist (newlist, procs[i]);
					procs[i] = NULL;
				}
			}

			/* park new list and free old */
			tnode_setnthsub (t, 1, newlist);
			tnode_free (slist);

			return 0;
			/*}}}*/
		}
		break;
	case 2:
		if (t->tag == mcsp.tag_PARCODE) {
			/*{{{  build the alphabet for this node, store unbound ones in parent*/
			mcsp_alpha_t *savedalpha = mfe->curalpha;
			mcsp_alpha_t *a_lhs, *a_rhs;
			mcsp_alpha_t *paralpha;
			tnode_t **subnodes;
			int nsnodes;

			/* always two subtrees */
			subnodes = parser_getlistitems (tnode_nthsubof (t, 1), &nsnodes);
			if (nsnodes != 2) {
				nocc_internal ("mcsp_fetrans_dopnode(): pass2 for PARCODE: have %d items", nsnodes);
				return 0;
			}

			mfe->curalpha = mcsp_newalpha ();
			fetrans_subtree (&subnodes[0], fe);
			a_lhs = mfe->curalpha;

			mfe->curalpha = mcsp_newalpha ();
			fetrans_subtree (&subnodes[1], fe);
			a_rhs = mfe->curalpha;

			mfe->curalpha = savedalpha;
			paralpha = mcsp_newalpha ();

			mcsp_sortandmergealpha (a_lhs, a_rhs, paralpha, mfe->curalpha);

			mcsp_freealpha (a_lhs);
			mcsp_freealpha (a_rhs);

			tnode_setnthhook (t, 0, (void *)paralpha);
			return 0;
			/*}}}*/
		}
		break;
	}
	return 1;
}
/*}}}*/
/*{{{  static int mcsp_namemap_cnode (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	does mapping for a constructor node (SEQ/PAR)
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_namemap_cnode (compops_t *cops, tnode_t **node, map_t *map)
{
	if ((*node)->tag == mcsp.tag_SEQCODE) {
		/* nothing special */
		return 1;
	} else if ((*node)->tag == mcsp.tag_PARCODE) {
		/*{{{  map PAR bodies*/
		tnode_t *body = tnode_nthsubof (*node, 1);
		tnode_t **bodies;
		int nbodies, i;
		tnode_t *parnode = *node;
		mcsp_alpha_t *alpha = (mcsp_alpha_t *)tnode_nthhookof (*node, 0);		/* events the _two_ processes synchronise on */

		if (!parser_islistnode (body)) {
			nocc_internal ("mcsp_namemap_cnode(): body of PARCODE not list");
			return 1;
		}

		/* if we have an alphabet, map these first (done in PAR context) */
		if (alpha) {
			map_submapnames (&(alpha->elist), map);
#if 0
fprintf (stderr, "mcsp_namemap_cnode(): mapped alphabet, got list:\n");
tnode_dumptree (alpha->elist, 1, stderr);
#endif
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
			parbodyspace = map->target->newname (tnode_create (mcsp.tag_PARSPACE, NULL), bodies[i], map, 0, 16, 0, 0, 0, 0);	/* FIXME! */
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
			tnode_t *fename = tnode_create (mcsp.tag_PARSPACE, NULL);

			blist = parser_newlistnode (NULL);
			for (i=0; i<nbodies; i++) {
				parser_addtolist (blist, bodies[i]);
			}
			bodyref = map->target->newblockref (blist, *node, map);
			bename = map->target->newname (fename, bodyref, map, map->target->aws.as_par, 0, 0, 0, 0, 0);
			tnode_setchook (fename, map->mapchook, (void *)bename);

			*node = bename;

			tnode_setnthsub (parnode, 0, map->target->newnameref (bename, map));
			/*}}}*/
			/*{{{  if we have an alphabet, link in with alloc:extravars*/
			if (alpha) {
				tnode_setchook (bename, map->allocevhook, alpha->elist);
			}
			/*}}}*/
		}

		/*}}}*/
	} else {
		nocc_internal ("mcsp_namemap_cnode(): don\'t know how to handle tag [%s]", (*node)->tag->name);
	}
	return 0;
}
/*}}}*/
/*{{{  static int mcsp_codegen_cnode (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	does code-generation for a constructor node (SEQ/PAR)
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_codegen_cnode (compops_t *cops, tnode_t *node, codegen_t *cgen)
{
	if (node->tag == mcsp.tag_SEQCODE) {
		return 1;
	} else if (node->tag == mcsp.tag_PARCODE) {
		/*{{{  generate code for PAR*/
		tnode_t *body = tnode_nthsubof (node, 1);
		tnode_t **bodies;
		int nbodies, i;
		int joinlab = codegen_new_label (cgen);
		int pp_wsoffs = 0;
		tnode_t *parspaceref = tnode_nthsubof (node, 0);
		mcsp_alpha_t *alpha = (mcsp_alpha_t *)tnode_nthhookof (node, 0);

		bodies = parser_getlistitems (body, &nbodies);
		/*{{{  if we've got an alphabet, up ref-counts by (nbodies), enroll-counts by (nbodies - 1), down-counts by (nbodies - 1)*/
		if (alpha) {
			tnode_t **events;
			int nevents, j;

			codegen_callops (cgen, comment, "start barrier enrolls");
			events = parser_getlistitems (alpha->elist, &nevents);
			for (j=0; j<nevents; j++) {
				codegen_callops (cgen, loadpointer, events[j], 0);
				codegen_callops (cgen, loadnonlocal, 0);		/* refcount */
				codegen_callops (cgen, loadconst, nbodies);
				codegen_callops (cgen, tsecondary, I_SUM);
				codegen_callops (cgen, loadpointer, events[j], 0);
				codegen_callops (cgen, storenonlocal, 0);		/* refcount */

				codegen_callops (cgen, loadpointer, events[j], 0);
				codegen_callops (cgen, loadnonlocal, 4);		/* enroll-count */
				codegen_callops (cgen, loadconst, nbodies - 1);
				codegen_callops (cgen, tsecondary, I_SUM);
				codegen_callops (cgen, loadpointer, events[j], 0);
				codegen_callops (cgen, storenonlocal, 4);		/* enroll-count */

				codegen_callops (cgen, loadpointer, events[j], 0);
				codegen_callops (cgen, loadnonlocal, 8);		/* down-count */
				codegen_callops (cgen, loadconst, nbodies - 1);
				codegen_callops (cgen, tsecondary, I_SUM);
				codegen_callops (cgen, loadpointer, events[j], 0);
				codegen_callops (cgen, storenonlocal, 8);		/* down-count */
			}
			codegen_callops (cgen, comment, "finish barrier enrolls");
		}
		/*}}}*/
		/*{{{  PAR setup*/
		codegen_callops (cgen, comment, "BEGIN PAR SETUP");
		for (i=0; i<nbodies; i++) {
			int ws_size, vs_size, ms_size;
			int ws_offset, adjust, elab;
			tnode_t *statics = tnode_nthsubof (bodies[i], 1);

			codegen_check_beblock (bodies[i], cgen, 1);
			cgen->target->be_getblocksize (bodies[i], &ws_size, &ws_offset, &vs_size, &ms_size, &adjust, &elab);

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

			pp_wsoffs -= ws_size;
		}
		/*{{{  setup local PAR workspace*/
		codegen_callops (cgen, loadconst, nbodies + 1);		/* par-count */
		codegen_callops (cgen, storename, parspaceref, 4);
		codegen_callops (cgen, loadconst, 0);			/* priority */
		codegen_callops (cgen, storename, parspaceref, 8);
		codegen_callops (cgen, loadlabaddr, joinlab);		/* join-lab */
		codegen_callops (cgen, storename, parspaceref, 0);
		/*}}}*/
		pp_wsoffs = 0;
		for (i=0; i<nbodies; i++) {
			int ws_size, vs_size, ms_size;
			int ws_offset, adjust, elab;

			codegen_check_beblock (bodies[i], cgen, 1);
			cgen->target->be_getblocksize (bodies[i], &ws_size, &ws_offset, &vs_size, &ms_size, &adjust, &elab);

			/*{{{  start PAR process*/
			codegen_callops (cgen, loadlabaddr, elab);
			codegen_callops (cgen, loadlocalpointer, pp_wsoffs - adjust);
			codegen_callops (cgen, tsecondary, I_STARTP);
			/*}}}*/

			pp_wsoffs -= ws_size;
		}
		codegen_callops (cgen, comment, "END PAR SETUP");
		/*}}}*/
		/*{{{  end process doing PAR*/
		codegen_callops (cgen, loadpointer, parspaceref, 0);
		codegen_callops (cgen, tsecondary, I_ENDP);
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
		codegen_callops (cgen, comment, "END PAR BODIES");
		codegen_callops (cgen, setlabel, joinlab);
		/*}}}*/
		/*{{{  if we've got an alphabet, down ref-counts by (nbodies), enroll-counts by (nbodies - 1), down-counts by (nbodies - 1)*/
		if (alpha) {
			tnode_t **events;
			int nevents, j;

			codegen_callops (cgen, comment, "start barrier resigns");
			events = parser_getlistitems (alpha->elist, &nevents);
			for (j=0; j<nevents; j++) {
				codegen_callops (cgen, loadpointer, events[j], 0);
				codegen_callops (cgen, loadnonlocal, 0);		/* refcount */
				codegen_callops (cgen, loadconst, nbodies);
				codegen_callops (cgen, tsecondary, I_DIFF);
				codegen_callops (cgen, loadpointer, events[j], 0);
				codegen_callops (cgen, storenonlocal, 0);		/* refcount */

				codegen_callops (cgen, loadpointer, events[j], 0);
				codegen_callops (cgen, loadnonlocal, 4);		/* enroll-count */
				codegen_callops (cgen, loadconst, nbodies - 1);
				codegen_callops (cgen, tsecondary, I_DIFF);
				codegen_callops (cgen, loadpointer, events[j], 0);
				codegen_callops (cgen, storenonlocal, 4);		/* enroll-count */

				codegen_callops (cgen, loadpointer, events[j], 0);
				codegen_callops (cgen, loadnonlocal, 8);		/* down-count */
				codegen_callops (cgen, loadconst, nbodies - 1);
				codegen_callops (cgen, tsecondary, I_DIFF);
				codegen_callops (cgen, loadpointer, events[j], 0);
				codegen_callops (cgen, storenonlocal, 8);		/* down-count */
			}
			codegen_callops (cgen, comment, "finish barrier resigns");
		}
		/*}}}*/
	} else {
		codegen_error (cgen, "mcsp_codegen_cnode(): how to handle [%s] ?", node->tag->name);
	}
	return 0;
}
/*}}}*/


/*{{{  static int mcsp_namemap_loopnode (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	does name-mapping for a loopnode
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_namemap_loopnode (compops_t *cops, tnode_t **node, map_t *map)
{
	tnode_t *t = *node;

	if (t->tag == mcsp.tag_ILOOP) {
		/*{{{  loop*/
		tnode_t **condp = tnode_nthsubaddr (t, 1);

		if (*condp) {
			map_submapnames (condp, map);
		}
		map_submapnames (tnode_nthsubaddr (t, 0), map);

		return 0;
		/*}}}*/
	} else if (t->tag == mcsp.tag_PRIDROP) {
		/*{{{  drop priority*/
		/* allocate a temporary anyway, not used yet */
		*node = map->target->newname (t, NULL, map, map->target->slotsize, map->target->bws.ds_min, 0, 0, 0, 0);

		/* map body */
		map_submapnames (tnode_nthsubaddr (t, 0), map);

		return 0;
		/*}}}*/
	}
	return 1;
}
/*}}}*/
/*{{{  static int mcsp_codegen_loopnode (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	does code-generation for a loopnode
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_codegen_loopnode (compops_t *cops, tnode_t *node, codegen_t *cgen)
{
	if (node->tag == mcsp.tag_ILOOP) {
		/*{{{  infinite loop!*/
		int looplab = codegen_new_label (cgen);

		codegen_callops (cgen, setlabel, looplab);
		codegen_subcodegen (tnode_nthsubof (node, 0), cgen);
		codegen_callops (cgen, branch, I_J, looplab);

		return 0;
		/*}}}*/
	} else if (node->tag == mcsp.tag_PRIDROP) {
		/*{{{  drop priority*/
		codegen_callops (cgen, tsecondary, I_GETPRI);
		codegen_callops (cgen, loadconst, 1);
		codegen_callops (cgen, tsecondary, I_SUM);
		codegen_callops (cgen, tsecondary, I_SETPRI);

		codegen_subcodegen (tnode_nthsubof (node, 0), cgen);

		codegen_callops (cgen, tsecondary, I_GETPRI);
		codegen_callops (cgen, loadconst, 1);
		codegen_callops (cgen, tsecondary, I_DIFF);
		codegen_callops (cgen, tsecondary, I_SETPRI);

		return 0;
		/*}}}*/
	}
	return 1;
}
/*}}}*/

/*{{{  static int mcsp_scopein_replnode (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	called to scope-in a replicator node
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_scopein_replnode (compops_t *cops, tnode_t **node, scope_t *ss)
{
	tnode_t *name = tnode_nthsubof (*node, 1);
	tnode_t **bodyptr = tnode_nthsubaddr (*node, 0);
	void *nsmark;
	char *rawname;
	name_t *replname;
	tnode_t *newname;

	if (!name && !tnode_nthsubof (*node, 2)) {
		/* just have a length expression, nothing to scope in */
		tnode_modprepostwalktree (tnode_nthsubaddr (*node, 3), scope_modprewalktree, scope_modpostwalktree, (void *)ss);
		tnode_modprepostwalktree (bodyptr, scope_modprewalktree, scope_modpostwalktree, (void *)ss);
	} else {
		/* scope the start and end expressions */
		tnode_modprepostwalktree (tnode_nthsubaddr (*node, 2), scope_modprewalktree, scope_modpostwalktree, (void *)ss);
		tnode_modprepostwalktree (tnode_nthsubaddr (*node, 3), scope_modprewalktree, scope_modpostwalktree, (void *)ss);

		nsmark = name_markscope ();
		/* scope in replicator name and walk body */
		rawname = (char *)tnode_nthhookof (name, 0);
		replname = name_addscopenamess (rawname, *node, NULL, NULL, ss);
		newname = tnode_createfrom (mcsp.tag_VAR, name, replname);
		SetNameNode (replname, newname);
		tnode_setnthsub (*node, 1, newname);

		/* free old name, scope body */
		tnode_free (name);

		tnode_modprepostwalktree (bodyptr, scope_modprewalktree, scope_modpostwalktree, (void *)ss);

		name_markdescope (nsmark);
	}

	return 0;
}
/*}}}*/
/*{{{  static int mcsp_scopeout_replnode (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	called to scope-out a replicator node
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_scopeout_replnode (compops_t *cops, tnode_t **node, scope_t *ss)
{
	return 1;
}
/*}}}*/
/*{{{  static int mcsp_fetrans_replnode (compops_t *cops, tnode_t **node, fetrans_t *fe)*/
/*
 *	does front-end transform for a replnode
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_fetrans_replnode (compops_t *cops, tnode_t **node, fetrans_t *fe)
{
	tnode_t *rname = tnode_nthsubof (*node, 1);
	tnode_t **rstartptr = tnode_nthsubaddr (*node, 2);
	tnode_t **rendptr = tnode_nthsubaddr (*node, 3);

	fetrans_subtree (tnode_nthsubaddr (*node, 0), fe);			/* fetrans body */
	if (!rname && !(*rstartptr)) {
		/* only got replicator length, start start to constant 1 */
		*rstartptr = constprop_newconst (CONST_INT, NULL, NULL, 1);
	}
	if (rname) {
		fetrans_subtree (tnode_nthsubaddr (*node, 1), fe);		/* fetrans name */
	}
	/* trans start/end expressions */
	fetrans_subtree (rstartptr, fe);
	fetrans_subtree (rendptr, fe);

	return 0;
}
/*}}}*/
/*{{{  static int mcsp_namemap_replnode (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	does name-mapping for a replnode
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_namemap_replnode (compops_t *cops, tnode_t **node, map_t *map)
{
	if ((*node)->tag == mcsp.tag_REPLSEQ) {
		tnode_t **rptr = tnode_nthsubaddr (*node, 1);
		tnode_t *fename;
		tnode_t *nameref = NULL;

		map_submapnames (tnode_nthsubaddr (*node, 0), map);		/* map body */
		map_submapnames (tnode_nthsubaddr (*node, 2), map);		/* map start expression */
		map_submapnames (tnode_nthsubaddr (*node, 3), map);		/* map end expression */

		/* reserve two words for replicator */
		if (*rptr) {
			fename = *rptr;
			*rptr = NULL;
		} else {
			fename = tnode_create (mcsp.tag_LOOPSPACE, NULL);
		}

		*node = map->target->newname (fename, *node, map, 2 * map->target->intsize, 0, 0, 0, map->target->intsize, 0);
		tnode_setchook (fename, map->mapchook, (void *)*node);

		/* transform replicator name into a reference to the space just reserved */
		nameref = map->target->newnameref (*node, map);
		/* *rptr = nameref; */
#if 0
fprintf (stderr, "got nameref for REPLSEQ space:\n");
tnode_dumptree (nameref, 1, stderr);
fprintf (stderr, "*rptr is:\n");
tnode_dumptree (*rptr, 1, stderr);
#endif
		/* *rptr = map->target->newnameref (*node, map); */
		if (*rptr) {
			tnode_free (*rptr);
		}
		*rptr = nameref;

		return 0;
	}
	return 1;
}
/*}}}*/
/*{{{  static int mcsp_codegen_replnode (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	does code-generation for a replnode
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_codegen_replnode (compops_t *cops, tnode_t *node, codegen_t *cgen)
{
	if (node->tag == mcsp.tag_REPLSEQ) {
		int toplab = codegen_new_label (cgen);
		int botlab = codegen_new_label (cgen);
		tnode_t *rref = tnode_nthsubof (node, 1);

		/*{{{  loop-head*/
		codegen_callops (cgen, loadname, tnode_nthsubof (node, 2), 0);		/* start */
		codegen_callops (cgen, storename, rref, 0);				/* => value */
		codegen_callops (cgen, loadname, tnode_nthsubof (node, 3), 0);		/* end */
		codegen_callops (cgen, loadname, tnode_nthsubof (node, 2), 0);		/* start */
		codegen_callops (cgen, tsecondary, I_SUB);
		codegen_callops (cgen, storename, rref, cgen->target->intsize);		/* => count */

		codegen_callops (cgen, setlabel, toplab);

		/*}}}*/
		/*{{{  loop-body*/
		codegen_subcodegen (tnode_nthsubof (node, 0), cgen);

		/*}}}*/
		/*{{{  loop-end*/

		codegen_callops (cgen, loadname, rref, cgen->target->intsize);		/* count */
		codegen_callops (cgen, branch, I_CJ, botlab);
		codegen_callops (cgen, loadname, rref, 0);				/* value */
		codegen_callops (cgen, loadconst, 1);
		codegen_callops (cgen, tsecondary, I_ADD);
		codegen_callops (cgen, storename, rref, 0);				/* value = value - 1*/
		codegen_callops (cgen, loadname, rref, cgen->target->intsize);		/* count */
		codegen_callops (cgen, loadconst, 1);
		codegen_callops (cgen, tsecondary, I_SUB);
		codegen_callops (cgen, storename, rref, cgen->target->intsize);		/* count = count - 1*/
		codegen_callops (cgen, branch, I_J, toplab);

		codegen_callops (cgen, setlabel, botlab);
		/*}}}*/
		return 0;
	}
	return 1;
}
/*}}}*/



/*{{{  static int mcsp_cnode_init_nodes (void)*/
/*
 *	initialises MCSP constructor nodes
 *	returns 0 on success, non-zero on failure
 */
static int mcsp_cnode_init_nodes (void)
{
	tndef_t *tnd;
	int i;
	compops_t *cops;
	langops_t *lops;

	/*{{{  mcsp:leafnode -- PARSPACE, LOOPSPACE*/
	i = -1;
	tnd = tnode_lookupornewnodetype ("mcsp:leafnode", &i, 0, 0, 0, TNF_NONE);

	i = -1;
	mcsp.tag_PARSPACE = tnode_newnodetag ("MCSPPARSPACE", &i, tnd, NTF_NONE);
	i = -1;
	mcsp.tag_LOOPSPACE = tnode_newnodetag ("MCSPLOOPSPACE", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  mcsp:cnode -- SEQCODE, PARCODE*/
	i = -1;
	tnd = mcsp.node_CNODE = tnode_newnodetype ("mcsp:cnode", &i, 2, 0, 1, TNF_NONE);		/* subnodes: 0 = back-end space reference, 1 = list of processes;  hooks: 0 = mcsp_alpha_t */
	tnd->hook_free = mcsp_alpha_hook_free;
	tnd->hook_copy = mcsp_alpha_hook_copy;
	tnd->hook_dumptree = mcsp_alpha_hook_dumptree;
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "fetrans", 2, COMPOPTYPE (mcsp_fetrans_cnode));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (mcsp_namemap_cnode));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (mcsp_codegen_cnode));
	tnd->ops = cops;

	i = -1;
	mcsp.tag_SEQCODE = tnode_newnodetag ("MCSPSEQNODE", &i, tnd, NTF_NONE);
	i = -1;
	mcsp.tag_PARCODE = tnode_newnodetag ("MCSPPARNODE", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  mcsp:loopnode -- ILOOP, PRIDROP*/
	i = -1;
	tnd = tnode_newnodetype ("mcsp:loopnode", &i, 2, 0, 0, TNF_NONE);				/* subnodes: 0 = body; 1 = condition */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (mcsp_namemap_loopnode));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (mcsp_codegen_loopnode));
	tnd->ops = cops;

	i = -1;
	mcsp.tag_ILOOP = tnode_newnodetag ("MCSPILOOP", &i, tnd, NTF_NONE);
	i = -1;
	mcsp.tag_PRIDROP = tnode_newnodetag ("MCSPPRIDROP", &i, tnd, NTF_NONE);		/* maybe not a loopnode as such, but will do for now */

	/*}}}*/
	/*{{{  mcsp:replnode -- REPLSEQ, REPLPAR, REPLILEAVE*/
	i = -1;
	tnd = mcsp.node_REPLNODE = tnode_newnodetype ("mcsp:replnode", &i, 4, 0, 0, TNF_NONE);		/* subnodes: 0 = body; 1 = repl-var, 2 = start, 3 = end */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (mcsp_scopein_replnode));
	tnode_setcompop (cops, "scopeout", 2, COMPOPTYPE (mcsp_scopeout_replnode));
	tnode_setcompop (cops, "fetrans", 2, COMPOPTYPE (mcsp_fetrans_replnode));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (mcsp_namemap_replnode));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (mcsp_codegen_replnode));
	tnd->ops = cops;

	i = -1;
	mcsp.tag_REPLSEQ = tnode_newnodetag ("MCSPREPLSEQ", &i, tnd, NTF_NONE);
	i = -1;
	mcsp.tag_REPLPAR = tnode_newnodetag ("MCSPREPLSEQ", &i, tnd, NTF_NONE);
	i = -1;
	mcsp.tag_REPLILEAVE = tnode_newnodetag ("MCSPREPLSEQ", &i, tnd, NTF_NONE);

	/*}}}*/

	return 0;
}
/*}}}*/
/*{{{  static int mcsp_cnode_reg_reducers (void)*/
/*
 *	registers reducers for MCSP constructor nodes
 *	returns 0 on success, non-zero on failure
 */
static int mcsp_cnode_reg_reducers (void)
{
	parser_register_grule ("mcsp:replseqreduce", parser_decode_grule ("N+N+N+VN+VN-VN+C4R-", mcsp.tag_REPLSEQ));
	parser_register_grule ("mcsp:replseqlreduce", parser_decode_grule ("N+00N+C4R-", mcsp.tag_REPLSEQ));

	return 0;
}
/*}}}*/
/*{{{  static dfattbl_t **mcsp_cnode_init_dfatrans (int *ntrans)*/
/*
 *	creates and returns DFA transition tables for MCSP constructor nodes
 */
static dfattbl_t **mcsp_cnode_init_dfatrans (int *ntrans)
{
	DYNARRAY (dfattbl_t *, transtbl);

	dynarray_init (transtbl);
	dynarray_add (transtbl, dfa_transtotbl ("mcsp:replseq ::= [ 0 @@; 1 ] [ 1 @@[ 2 ] [ 2 mcsp:expr 3 ] [ 3 @@= 4 ] [ 3 @@] 10 ] [ 4 mcsp:expr 5 ] [ 5 @@, 6 ] [ 6 mcsp:expr 7 ] " \
				"[ 7 @@] 8 ] [ 8 mcsp:process 9 ] [ 9 {<mcsp:replseqreduce>} -* ] [ 10 mcsp:process 11 ] [ 11 {<mcsp:replseqlreduce>} -* ]"));

	*ntrans = DA_CUR (transtbl);
	return DA_PTR (transtbl);
}
/*}}}*/



/*{{{  mcsp_cnode_feunit (feunit_t)*/
feunit_t mcsp_cnode_feunit = {
	init_nodes: mcsp_cnode_init_nodes,
	reg_reducers: mcsp_cnode_reg_reducers,
	init_dfatrans: mcsp_cnode_init_dfatrans,
	post_setup: NULL
};
/*}}}*/

