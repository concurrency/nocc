/*
 *	guppy_cnode.c -- constructor nodes for Guppy
 *	Copyright (C) 2010-2014 Fred Barnes <frmb@kent.ac.uk>
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
#include "fhandle.h"
#include "symbols.h"
#include "keywords.h"
#include "lexer.h"
#include "lexpriv.h"
#include "tnode.h"
#include "parser.h"
#include "fcnlib.h"
#include "dfa.h"
#include "parsepriv.h"
#include "guppy.h"
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
#include "cccsp.h"


/*}}}*/
/*{{{  private types/data*/

static tnode_t *guppy_cnode_inttypenode = NULL;
static tnode_t *guppy_cnode_booltypenode = NULL;


/*}}}*/


/*{{{  static int guppy_prescope_cnode (compops_t *cops, tnode_t **node, guppy_prescope_t *ps)*/
/*
 *	does pre-scoping on a constructor node
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_prescope_cnode (compops_t *cops, tnode_t **node, guppy_prescope_t *ps)
{
	return 1;
}
/*}}}*/
/*{{{  static int guppy_declify_cnode (compops_t *cops, tnode_t **nodeptr, guppy_declify_t *gdl)*/
/*
 *	does declify on constructor nodes
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_declify_cnode (compops_t *cops, tnode_t **nodeptr, guppy_declify_t *gdl)
{
	tnode_t *node = *nodeptr;
	tnode_t **bptr = tnode_nthsubaddr (node, 1);

	if (parser_islistnode (*bptr)) {
		if (node->tag == gup.tag_PAR) {
			/* need to be slightly careful: so declarations and processes don't get auto-seq'd */
			guppy_declify_listtodecllist_single (bptr, gdl);
		} else {
			guppy_declify_listtodecllist (bptr, gdl);
		}
	}
	return 0;
}
/*}}}*/
/*{{{  static int guppy_autoseq_cnode (compops_t *cops, tnode_t **node, guppy_autoseq_t *gas)*/
/*
 *	does auto-sequencing processing on constructor nodes (no-op)
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_autoseq_cnode (compops_t *cops, tnode_t **node, guppy_autoseq_t *gas)
{
	return 1;
}
/*}}}*/
/*{{{  static int guppy_flattenseq_cnode (compops_t *cops, tnode_t **nodeptr)*/
/*
 *	does flattening on a constructor node
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_flattenseq_cnode (compops_t *cops, tnode_t **nodeptr)
{
	tnode_t *node = *nodeptr;
	tnode_t **listptr = tnode_nthsubaddr (node, 1);

	if (!parser_islistnode (*listptr)) {
		/* not a problem in practice, just means we were probably here before! */
		// nocc_serious ("guppy_flattenseq_cnode(): node body is not a list..");
		// return 0;
	} else if (parser_countlist (*listptr) == 1) {
		/* single item, remove it and replace */
		tnode_t *item = parser_delfromlist (*listptr, 0);

		tnode_free (node);
		*nodeptr = item;
	} else if (parser_countlist (*listptr) == 0) {
		/* no items, replace with 'skip' */
		tnode_t *item = tnode_createfrom (gup.tag_SKIP, node);

		tnode_free (node);
		*nodeptr = item;
	}
	return 1;
}
/*}}}*/
/*{{{  static int guppy_scopein_cnode (compops_t *cops, tnode_t **nodep, scope_t *ss)*/
/*
 *	does scope-in on a constructor node
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_scopein_cnode (compops_t *cops, tnode_t **nodep, scope_t *ss)
{
	if ((*nodep)->tag == gup.tag_PAR) {
		/* complex: scope each parallel sub-process individually, keeping note of names that cross the PAR boundary */
		guppy_scope_t *gss = (guppy_scope_t *)ss->langpriv;
		tnode_t *parlist = tnode_nthsubof (*nodep, 1);
		tnode_t **pitems;
		int i, nitems;

		if (!gss) {
			nocc_internal ("guppy_scopein_cnode(): NULL guppy_scope!");
			return 0;
		}

		ss->lexlevel++;
		pitems = parser_getlistitems (parlist, &nitems);
		for (i=0; i<nitems; i++) {
			tnode_t *fvlist = parser_newlistnode (SLOCI);
			tnode_t *fvnode;

#if 0
fhandle_printf (FHAN_STDERR, "guppy_scopein_cnode(): set fvlist=\n");
tnode_dumptree (fvlist, 1, FHAN_STDERR);
#endif
			dynarray_add (gss->crosses, fvlist);
			dynarray_add (gss->cross_lexlevels, ss->lexlevel);
			scope_subtree (pitems + i, ss);

			/* anything that refers to high-level stuff will be recorded in fvlist */

			fvnode = tnode_createfrom (gup.tag_FVNODE, pitems[i], pitems[i], fvlist);
			pitems[i] = fvnode;

			/* and lets not forget to remove it after..! */
			dynarray_delitem (gss->cross_lexlevels, DA_CUR (gss->cross_lexlevels) - 1);
			dynarray_rmitem (gss->crosses, fvlist);
		}
		ss->lexlevel--;
		return 0;		/* don't descend! */
	} 
	/* else trivial */
	return 1;
}
/*}}}*/
/*{{{  static int guppy_postscope_cnode (compops_t *cops, tnode_t **nodep)*/
/*
 *	does post-scope on a constructor node.
 *	returns 0 to stop walk, non-zero to continue.
 */
static int guppy_postscope_cnode (compops_t *cops, tnode_t **nodep)
{
	tnode_t **bodyp = tnode_nthsubaddr (*nodep, 1);

	parser_ensurelist (bodyp, *nodep);
	return 1;
}
/*}}}*/
/*{{{  static int guppy_fetrans_cnode (compops_t *cops, tnode_t **nodep, fetrans_t *fe)*/
/*
 *	does front-end transforms for a constructor node
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_fetrans_cnode (compops_t *cops, tnode_t **nodep, fetrans_t *fe)
{
	guppy_fetrans_t *gfe = (guppy_fetrans_t *)fe->langpriv;

	if ((*nodep)->tag == gup.tag_PAR) {
		/* for each process, break into a separate parallel-process instance */
		tnode_t *parlist = tnode_nthsubof (*nodep, 1);
		tnode_t **pitems;
		int i, nitems;

		pitems = parser_getlistitems (parlist, &nitems);
		for (i=0; i<nitems; i++) {
			if (pitems[i]->tag == gup.tag_FVNODE) {
				tnode_t *newdef, *newdefnnode, *fvlist, *body, **fvitems;
				tnode_t *parmlist, *parmnamelist;
				int nfvitems, j;
				name_t *newdefname;
				char *ndname = guppy_maketempname (pitems[i]);
				guppy_fcndefhook_t *fdh = guppy_newfcndefhook ();
				tnode_t *inode;

				body = tnode_nthsubof (pitems[i], 0);
				fvlist = tnode_nthsubof (pitems[i], 1);
				fvitems = parser_getlistitems (fvlist, &nfvitems);
#if 0
fhandle_printf (FHAN_STDERR, "guppy_fetrans_cnode(): thinking about PAR, process %d is:\n", i);
tnode_dumptree (pitems[i], 1, FHAN_STDERR);
fhandle_printf (FHAN_STDERR, "guppy_fetrans_cnode(): fvlist =\n");
tnode_dumptree (fvlist, 1, FHAN_STDERR);
#endif
				parmlist = parser_newlistnode (SLOCI);
				parmnamelist = parser_newlistnode (SLOCI);
				for (j=0; j<nfvitems; j++) {
					/*{{{  parameterise free variables (FIXME: this really needs usagechecker help to get val/var right)*/
					name_t *p_name;
					tnode_t *p_namenode, *p_fparam, *p_type;
					char *vname = NULL;

					/* pick the same names */
					langops_getname (fvitems[j], &vname);
					if (!vname) {
						vname = guppy_maketempname (fvitems[j]);
					}

#if 0
fhandle_printf (FHAN_STDERR, "guppy_fetrans_cnode(): getting type of item [0x%8.8x] (%s)..\n", (unsigned int)fvitems[j], vname);
#endif
					p_type = typecheck_gettype (fvitems[j], NULL);

					p_name = name_addname (vname, NULL, p_type, NULL);
					p_namenode = tnode_createfrom (gup.tag_NPARAM, fvitems[j], p_name);
					SetNameNode (p_name, p_namenode);
					p_fparam = tnode_createfrom (gup.tag_FPARAM, fvitems[j], p_namenode, p_type, NULL);

					parser_addtolist (parmlist, p_fparam);
					parser_addtolist (parmnamelist, p_namenode);
					/*}}}*/
				}
				
				newdefname = name_addname (ndname, NULL, parmlist, NULL);
				newdefnnode = tnode_createfrom (gup.tag_NPFCNDEF, pitems[i], newdefname);
				SetNameNode (newdefname, newdefnnode);

				/* substitute names in tree */
				for (j=0; j<nfvitems; j++) {
					tnode_t *curname = parser_getfromlist (fvlist, j);
					tnode_t *newname = parser_getfromlist (parmnamelist, j);

					tnode_substitute (&body, curname, newname);
				}
				newdef = tnode_createfrom (gup.tag_PFCNDEF, pitems[i], newdefnnode, parmlist, body, NULL, fdh);
				SetNameDecl (newdefname, newdef);

				/* do transform on new definition */
				fetrans_subtree (&newdef, fe);

#if 0
fhandle_printf (FHAN_STDERR, "guppy_fetrans_cnode(): created a definition:\n");
tnode_dumptree (newdef, 1, FHAN_STDERR);
#endif

				/* insert new process definition into tree */
				if (!gfe->preinslist) {
					gfe->preinslist = parser_newlistnode (SLOCI);
				}
				parser_addtolist (gfe->preinslist, newdef);
				
				/* replace process body with instance */
				inode = tnode_createfrom (gup.tag_PPINSTANCE, pitems[i], newdefnnode, NULL /* FIXME! */, fvlist);
				pitems[i] = inode;
			} else if (pitems[i]->tag == gup.tag_PPINSTANCE) {
				/* safe: probably created by the above, or plain instance anyway */
				fetrans_subtree (pitems + i, fe);
			} else {
				nocc_internal ("guppy_fetrans_cnode(): expected FVNODE/PPINSTANCE in PAR but got [%s]", pitems[i]->tag->name);
				return 0;
			}
		}
		return 0;
	}
	/* else trivial */
	return 1;
}
/*}}}*/
/*{{{  static int guppy_fetrans1_cnode (compops_t *cops, tnode_t **nodep, guppy_fetrans1_t *fe1)*/
/*
 *	does fetrans1 on constructor nodes: sets up insert-point for temporaries (at the position of the process)
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_fetrans1_cnode (compops_t *cops, tnode_t **nodep, guppy_fetrans1_t *fe1)
{
	tnode_t *body = tnode_nthsubof (*nodep, 1);

	if (!parser_islistnode (body)) {
		tnode_error (*nodep, "seq/par body not list");
		fe1->error++;
		return 0;
	} else {
		int nbitems, i;
		tnode_t **bitems = parser_getlistitems (body, &nbitems);

		for (i=0; i<nbitems; i++) {
			guppy_fetrans1_subtree_newtemps (bitems + i, fe1);
		}
	}
	return 0;
}
/*}}}*/
/*{{{  static int guppy_fetrans15_cnode (compopts_t *cops, tnode_t **nodep, guppy_fetrans15_t *fe15)*/
/*
 *	does fetrans1.5 for constructor nodes (trivial here)
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_fetrans15_cnode (compopts_t *cops, tnode_t **nodep, guppy_fetrans15_t *fe15)
{
	int saved = fe15->expt_proc;

	fe15->expt_proc = 0;
	guppy_fetrans15_subtree (tnode_nthsubaddr (*nodep, 0), fe15);
	fe15->expt_proc = 1;							/* expecting a list of processes really */
	guppy_fetrans15_subtree (tnode_nthsubaddr (*nodep, 1), fe15);

	fe15->expt_proc = saved;
	return 0;
}
/*}}}*/
/*{{{  static int guppy_namemap_cnode (compops_t *cops, tnode_t **nodep, map_t *map)*/
/*
 *	does name-mapping for a constructor node
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_namemap_cnode (compops_t *cops, tnode_t **nodep, map_t *map)
{
	cccsp_mapdata_t *cmd = (cccsp_mapdata_t *)map->hook;
	guppy_map_t *gmap = (guppy_map_t *)cmd->langhook;

	if ((*nodep)->tag == gup.tag_PAR) {
		/* each parallel process should be a PPINSTANCE node now */
		tnode_t *parlist = tnode_nthsubof (*nodep, 1);
		tnode_t *dblk, *decllist, *oseq, *oseqlist, *beblk;
		tnode_t **pitems;
		int i, nitems;
		tnode_t *pcargs, *pccall, *pccallnum, *pcnprocs;
		tnode_t *saved_dlist = gmap->decllist;
		cccsp_parinfo_t *parinfo = cccsp_newparinfo ();
		tnode_t *freewslist = NULL;

		/* attach par-info structure here; won't change */
		tnode_setchook (*nodep, cccsp_parinfochook, (void *)parinfo);

		decllist = parser_newlistnode (SLOCI);
		freewslist = parser_newlistnode (SLOCI);
		pcargs = parser_newlistnode (SLOCI);

		pitems = parser_getlistitems (parlist, &nitems);
		for (i=0; i<nitems; i++) {
			tnode_t *wsvar;
			cccsp_parinfo_entry_t *pent = cccsp_newparinfoentry ();

			wsvar = cccsp_create_wptr (OrgOf (pitems[i]), map->target);
			parser_addtolist (decllist, wsvar);
			parser_addtolist (freewslist, wsvar);

			if (pitems[i]->tag != gup.tag_PPINSTANCE) {
				nocc_internal ("guppy_namemap_cnode(): expected PPINSTANCE in PAR but got [%s]", pitems[i]->tag->name);
				return 0;
			}
			tnode_setnthsub (pitems[i], 1, wsvar);

			pent->namenode = tnode_nthsubof (pitems[i], 0);
			cccsp_linkparinfo (parinfo, pent);
		}

		oseqlist = parser_newlistnode (SLOCI);
		parser_addtolist (oseqlist, *nodep);
		oseq = tnode_create (gup.tag_SEQ, OrgOf (*nodep), NULL, oseqlist);
		dblk = tnode_create (gup.tag_DECLBLOCK, OrgOf (*nodep), decllist, oseq);
		map_submapnames (tnode_nthsubaddr (dblk, 0), map);

		gmap->decllist = tnode_nthsubof (dblk, 0);

		parser_addtolist (pcargs, cmd->process_id);
		pcnprocs = constprop_newconst (CONST_INT, NULL, NULL, nitems);
		parser_addtolist (pcargs, pcnprocs);

		for (i=0; i<nitems; i++) {
			/* map PPINSTANCEs this after we've mapped the WS declarations */
			/* XXX:
			 *	scoop out some things into a list of arguments for ProcPar later.
			 *	done here because after mapping it, PPINSTANCE won't exist anymore.
			 */
			tnode_t *pp_name = tnode_nthsubof (pitems[i], 0);
			tnode_t *pp_wptr = tnode_nthsubof (pitems[i], 1);
			cccsp_parinfo_entry_t *pent = DA_NTHITEM (parinfo->entries, i);		/* must match! */

			parser_addtolist (pcargs, pp_wptr);
			parser_addtolist (pcargs, pp_name);

			cmd->thisentry = pent;
			map_submapnames (pitems + i, map);
			cmd->thisentry = NULL;
		}

		pccallnum = cccsp_create_apicallname (PROC_PAR);
		pccall = tnode_create (gup.tag_APICALL, SLOCI, pccallnum, pcargs);

		map_submapnames (&pcargs, map);

		parser_addtolist (oseqlist, pccall);

		if (cccsp_get_subtarget () == CCCSP_SUBTARGET_EV3) {
			/* special for the EV3's CCSP */

			/* mapping PPINSTANCEs will have generated allocations, so after running, trash them */
			for (i=0; i<nitems; i++) {
				tnode_t *frcallnum = cccsp_create_apicallname (LIGHT_PROC_FREE);
				tnode_t *frargs = parser_newlistnode (SLOCI);
				tnode_t *frcall = tnode_create (gup.tag_APICALL, SLOCI, frcallnum, frargs);
				tnode_t *pp_wptr = parser_getfromlist (freewslist, i);

				parser_addtolist (frargs, cmd->process_id);
				parser_addtolist (frargs, pp_wptr);

				map_submapnames (&frargs, map);
				parser_addtolist (oseqlist, frcall);
			}
		}

		gmap->decllist = saved_dlist;

		beblk = map->target->newblock (dblk, map, NULL, map->lexlevel);			/* make sure this goes in too! */
		*nodep = beblk;
#if 0
fhandle_printf (FHAN_STDERR, "guppy_namemap_cnode(): map for parallel processes:\n");
tnode_dumptree (dblk, 1, FHAN_STDERR);
#endif

		return 0;
	}
	return 1;
}
/*}}}*/
/*{{{  static int guppy_lpreallocate_cnode (compops_t *cops, tnode_t *node, cccsp_preallocate_t *cpa)*/
/*
 *	does stack allocation for a constructor node: most of all sub-components
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_lpreallocate_cnode (compops_t *cops, tnode_t *node, cccsp_preallocate_t *cpa)
{
	int max = 0;
	tnode_t *body = tnode_nthsubof (node, 1);
	tnode_t **items;
	int nitems, i;
	int saved_collect = cpa->collect;

	if (!parser_islistnode (body)) {
		nocc_internal ("guppy_lpreallocate_cnode(): body not a list!, got [%s]", body->tag->name);
		return 0;
	}
	items = parser_getlistitems (body, &nitems);
	for (i=0; i<nitems; i++) {
		cpa->collect = 0;
		cccsp_preallocate_subtree (items[i], cpa);
		if (cpa->collect > max) {
			max = cpa->collect;
		}
	}
	cpa->collect = saved_collect + max;

	return 0;
}
/*}}}*/
/*{{{  static int guppy_codegen_cnode (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	does code-generation for a constructor node: only have SEQ left by this point
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_codegen_cnode (compops_t *cops, tnode_t *node, codegen_t *cgen)
{
	return 1;
}
/*}}}*/
/*{{{  static int guppy_reallocate_cnode (compops_t *cops, tnode_t *node, cccsp_reallocate_t *cra)*/
/*
 *	does reallocation for a constructor node: only interested in left-over PAR
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_reallocate_cnode (compops_t *cops, tnode_t *node, cccsp_reallocate_t *cra)
{
	if (node->tag == gup.tag_PAR) {
		cccsp_parinfo_t *pset = (cccsp_parinfo_t *)tnode_getchook (node, cccsp_parinfochook);
		int i;
		int bytesum = 0;

		for (i=0; i<DA_CUR (pset->entries); i++) {
			cccsp_parinfo_entry_t *pent = DA_NTHITEM (pset->entries, i);
			tnode_t *ppdecl;
			cccsp_sfi_entry_t *sfient;
			int nwords;

#if 0
fhandle_printf (FHAN_STDERR, "guppy_reallocate_cnode(): here, entry %d namenode=0x%8.8x wsspace=0x%8.8x\n",
		i, (unsigned int)pent->namenode, (unsigned int)pent->wsspace);
tnode_dumptree (pent->namenode, 1, FHAN_STDERR);
tnode_dumptree (pent->wsspace, 1, FHAN_STDERR);
#endif
			if (pent->namenode->tag != gup.tag_NPFCNDEF) {
				nocc_error ("guppy_reallocate_cnode(): parallel process name not N_PFCNDEF, found [%s]", pent->namenode->tag->name);
				cra->error++;
				return 0;
			}
			ppdecl = NameDeclOf (tnode_nthnameof (pent->namenode, 0));
			sfient = (cccsp_sfi_entry_t *)tnode_getchook (ppdecl, cccsp_sfi_entrychook);

			if (!sfient) {
				nocc_error ("guppy_reallocate_cnode(): no SFI entry for process [%s]", NameNameOf (tnode_nthnameof (pent->namenode, 0)));
				cra->error++;
				return 0;
			}
			if (!sfient->parfixup) {
				nocc_warning ("guppy_reallocate_cnode(): SFI entry for [%s] has not been par-fixed yet.. (dynamic?)",
						NameNameOf (tnode_nthnameof (pent->namenode, 0)));
			}

			bytesum += sfient->allocsize;
			if (bytesum & 0x03) {
				bytesum = (bytesum & ~0x03) + 4;		/* round up */
			}

			nwords = sfient->allocsize >> 2;
			if (sfient->allocsize & 0x03) {
				nwords++;
			}

#if 0
			/* slack */
			nwords += 1024;
			bytesum += 4096;
#endif

			/* fixup inside workspace */
			cccsp_set_workspace_nwords (pent->wsspace, nwords);

#if 0
fhandle_printf (FHAN_STDERR, "guppy_reallocate_cnode(): here2: entry %d, allocsize = %d\n", i, sfient->allocsize);
// tnode_dumptree (pent->wsspace, 1, FHAN_STDERR);
#endif
		}

		pset->nwords = (bytesum >> 2);					/* bytes-to-words */
#if 0
fhandle_printf (FHAN_STDERR, "guppy_reallocate_cnode(): here3: did %d entries, got %d bytes (%d words)\n", DA_CUR (pset->entries), bytesum, pset->nwords);
#endif
		if (pset->nwords > cra->maxpar) {
			cra->maxpar = pset->nwords;
		}

		return 0;
	}
	return 1;
}
/*}}}*/

/*{{{  static int guppy_dousagecheck_cnode (langops_t *lops, tnode_t *node, uchk_state_t *ucstate)*/
/*
 *	does usage-checking for a constructor node (only interested in PAR really)
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_dousagecheck_cnode (langops_t *lops, tnode_t *node, uchk_state_t *ucstate)
{
	if (node->tag == gup.tag_PAR) {
		/*{{{  usage-check individual bodies*/
		tnode_t *body = tnode_nthsubof (node, 1);
		tnode_t **bitems;
		int nitems, i;
		tnode_t *parnode = node;

		if (!parser_islistnode (body)) {
			nocc_internal ("guppy_dousagecheck_cnode(): body of PAR not list");
			return 0;
		}

		usagecheck_begin_branches (node, ucstate);
		bitems = parser_getlistitems (body, &nitems);

		for (i=0; i<nitems; i++) {
			usagecheck_branch (bitems[i], ucstate);
		}

		usagecheck_end_branches (node, ucstate);
		if (!usagecheck_no_overlaps (node, ucstate)) {
			usagecheck_mergeall (node, ucstate);
		}

		return 0;
		/*}}}*/
	}
	return 1;
}
/*}}}*/


/*{{{  static int guppy_scopein_replcnode (compops_t *cops, tnode_t **nodep, scope_t *ss)*/
/*
 *	does scope-in for a replicated constructor node.
 *	returns 0 to stop walk, 1 to continue.
 */
static int guppy_scopein_replcnode (compops_t *cops, tnode_t **nodep, scope_t *ss)
{
	tnode_t **bodyp = tnode_nthsubaddr (*nodep, 1);
	tnode_t **rnamep = tnode_nthsubaddr (*nodep, 2);
	tnode_t **rstartp = tnode_nthsubaddr (*nodep, 3);
	tnode_t **rcountp = tnode_nthsubaddr (*nodep, 4);
	char *rawname = NULL;
	void *nsmark;
	name_t *repname;
	tnode_t *type, *newname, *newdecl;
	tnode_t *declblk, *dlist;

	if (*rstartp) {
		scope_subtree (rstartp, ss);
	}
	if (*rcountp) {
		scope_subtree (rcountp, ss);
	}

	nsmark = name_markscope ();

	if (!*rnamep) {
		/* no name, create one */
		rawname = guppy_maketempname (*nodep);
	} else {
		if ((*rnamep)->tag != gup.tag_NAME) {
			scope_error (*nodep, ss, "replicator name not raw-name, found [%s:%s]", (*rnamep)->tag->ndef->name, (*rnamep)->tag->name);
			return 0;
		}
		rawname = (char *)tnode_nthhookof (*rnamep, 0);
	}
	dlist = parser_newlistnode (OrgOf (*nodep));
	declblk = tnode_createfrom (gup.tag_DECLBLOCK, *nodep, dlist, *nodep);
	type = guppy_newprimtype (gup.tag_INT, *rnamep, 0);
	newdecl = tnode_createfrom (gup.tag_VARDECL, *nodep, NULL, type, NULL);
	parser_addtolist (dlist, newdecl);
	repname = name_addscopename (rawname, newdecl, type, NULL);
	newname = tnode_createfrom (gup.tag_NREPL, *rnamep, repname);
	SetNameNode (repname, newname);
	tnode_setnthsub (newdecl, 0, newname);

#if 0
fhandle_printf (FHAN_STDERR, "guppy_scopein_replcnode(): declblk =\n");
tnode_dumptree (declblk, 1, FHAN_STDERR);
#endif


	*rnamep = newname;

	ss->scoped++;

	/* scope-in body */
	if (*bodyp) {
		scope_subtree (bodyp, ss);
	}

	name_markdescope (nsmark);

#if 0
fhandle_printf (FHAN_STDERR, "guppy_scopein_replcnode(): declblk =\n");
tnode_dumptree (declblk, 1, FHAN_STDERR);
#endif
	*nodep = declblk;

	return 0;
}
/*}}}*/
/*{{{  static int guppy_scopeout_replcnode (compops_t *cops, tnode_t **nodep, scope_t *ss)*/
/*
 *	does scope-out for a replicated constructor node (no-op).
 *	return value meaningless (post-walk).
 */
static int guppy_scopeout_replcnode (compops_t *cops, tnode_t **nodep, scope_t *ss)
{
	return 1;
}
/*}}}*/
/*{{{  static int guppy_typecheck_replcnode (compops_t *cops, tnode_t *node, typecheck_t *tc)*/
/*
 *	does type-checking on a replicator constructor node
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_typecheck_replcnode (compops_t *cops, tnode_t *node, typecheck_t *tc)
{
	tnode_t **rstartp = tnode_nthsubaddr (node, 3);
	tnode_t **rcountp = tnode_nthsubaddr (node, 4);
	tnode_t *type;

	if (!*rstartp) {
		*rstartp = constprop_newconst (CONST_INT, NULL, NULL, 0);
	}
	if (!*rcountp) {
		typecheck_error (node, tc, "replicator is missing a count");
		return 0;
	}

	/* type-check start and count expressions, should be INT */
	if (typecheck_subtree (*rstartp, tc)) {
		return 0;
	}
	if (typecheck_subtree (*rcountp, tc)) {
		return 0;
	}

	type = typecheck_gettype (*rstartp, guppy_cnode_inttypenode);
	if (!type) {
		typecheck_error (node, tc, "unable to determine type for replicator start");
		return 0;
	}
	type = typecheck_typeactual (guppy_cnode_inttypenode, type, node, tc);
	if (!type) {
		typecheck_error (node, tc, "incompatible type for replicator start (expected integer)");
		return 0;
	}

	type = typecheck_gettype (*rcountp, guppy_cnode_inttypenode);
	if (!type) {
		typecheck_error (node, tc, "unable to determine type for replicator count");
		return 0;
	}
	type = typecheck_typeactual (guppy_cnode_inttypenode, type, node, tc);
	if (!type) {
		typecheck_error (node, tc, "incompatible type for replicator count (expected integer)");
		return 0;
	}

	/* now type-check the body */
	typecheck_subtree (tnode_nthsubof (node, 1), tc);

	return 0;
}
/*}}}*/
/*{{{  static int guppy_fetrans15_replcnode (compopts_t *cops, tnode_t **nodep, guppy_fetrans15_t *fe15)*/
/*
 *	does fetrans1.5 for replicated constructor nodes (trivial here)
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_fetrans15_replcnode (compopts_t *cops, tnode_t **nodep, guppy_fetrans15_t *fe15)
{
	int saved = fe15->expt_proc;

	fe15->expt_proc = 1;							/* expecting a list of processes really */
	guppy_fetrans15_subtree (tnode_nthsubaddr (*nodep, 1), fe15);

	fe15->expt_proc = saved;
	return 0;
}
/*}}}*/
/*{{{  static int guppy_namemap_replcnode (compops_t *cops, tnode_t **nodep, namemap_t *map)*/
/*
 *	called to do name-mapping on a replicated constructor node
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_namemap_replcnode (compops_t *cops, tnode_t **nodep, map_t *map)
{
	map_submapnames (tnode_nthsubaddr (*nodep, 2), map);
	map_submapnames (tnode_nthsubaddr (*nodep, 3), map);
	map_submapnames (tnode_nthsubaddr (*nodep, 4), map);
	map_submapnames (tnode_nthsubaddr (*nodep, 1), map);
	return 0;
}
/*}}}*/
/*{{{  static int guppy_codegen_replcnode (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	called to do code-generation on a replicated constructor node
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_codegen_replcnode (compops_t *cops, tnode_t *node, codegen_t *cgen)
{
	if (node->tag == gup.tag_REPLSEQ) {
		tnode_t *rname = tnode_nthsubof (node, 2);
		tnode_t *rstart = tnode_nthsubof (node, 3);
		tnode_t *rcount = tnode_nthsubof (node, 4);
		tnode_t *rbody = tnode_nthsubof (node, 1);

		codegen_ssetindent (cgen);
		codegen_write_fmt (cgen, "for (");
		codegen_subcodegen (rname, cgen);
		codegen_write_fmt (cgen, " = ");
		codegen_subcodegen (rstart, cgen);
		codegen_write_fmt (cgen, "; ");
		codegen_subcodegen (rname, cgen);
		codegen_write_fmt (cgen, " < (");
		codegen_subcodegen (rstart, cgen);
		codegen_write_fmt (cgen, "+");
		codegen_subcodegen (rcount, cgen);
		codegen_write_fmt (cgen, "); ");
		codegen_subcodegen (rname, cgen);
		codegen_write_fmt (cgen, "++) {\n");

		cgen->indent++;
		codegen_subcodegen (rbody, cgen);
		cgen->indent--;

		codegen_ssetindent (cgen);
		codegen_write_fmt (cgen, "}\n");
		return 0;
	}
	return 1;
}
/*}}}*/


/*{{{  static int guppy_typecheck_anode (compops_t *cops, tnode_t *node, typecheck_t *tc)*/
/*
 *	does type-checking for an alternative -- ensures that any 'skip' guard(s) are last in a 'pri alt'
 *	and no skip in plain-alt
 */
static int guppy_typecheck_anode (compops_t *cops, tnode_t *node, typecheck_t *tc)
{
	int ispri = (node->tag == gup.tag_PRIALT);
	tnode_t *glist = tnode_nthsubof (node, 1);
	int hasskip = -1;
	int nonskip = -1;
	int i;

	for (i=0; i<parser_countlist (glist); i++) {
		tnode_t *guard = parser_getfromlist (glist, i);
		tnode_t *act;
		
		if (guard->tag == gup.tag_DECLBLOCK) {
			guard = tnode_nthsubof (guard, 1);
		}
		if (guard->tag != gup.tag_GUARD) {
			typecheck_error (guard, tc, "invalid guard in alternative (found '%s')", guard->tag->name);
			/* give up totally here */
			return 1;
		}
		act = tnode_nthsubof (guard, 1);

		if ((act->tag == gup.tag_INPUT) || (act->tag == gup.tag_CASEINPUT)) {
			/* okay */
			nonskip = i;
		} else if (act->tag == gup.tag_SKIP) {
			hasskip = i;
		} else {
			typecheck_error (act, tc, "invalid guard action in alternative (found '%s')", act->tag->name);
		}
	}

	if ((hasskip >= 0) && !ispri) {
		typecheck_error (node, tc, "skip guard only allowed in 'pri alt'");
	} else if ((hasskip >= 0) && (hasskip < nonskip)) {
		typecheck_error (node, tc, "skip guard not last in 'pri alt'");
	}
	return 1;
}
/*}}}*/
/*{{{  static int guppy_fetrans1_anode (compops_t *cops, tnode_t **nodep, guppy_fetrans1_t *fe1)*/
/*
 *	does fetrans1 transform for an alternative -- this creates the temporary that will be used to store the selected guard.
 *	returns 0 to stop walk, non-zero to continue.
 */
static int guppy_fetrans1_anode (compops_t *cops, tnode_t **nodep, guppy_fetrans1_t *fe1)
{
	tnode_t *node = *nodep;

	fe1->inspoint = nodep;
	if ((node->tag == gup.tag_ALT) || (node->tag == gup.tag_PRIALT)) {
		tnode_t *tname, *type;
		tnode_t **newnodep;

		type = guppy_newprimtype (gup.tag_INT, node, 0);
		tname = guppy_fetrans1_maketemp (gup.tag_NDECL, node, type, NULL, fe1);

		tnode_setnthsub (node, 0, tname);
	}

	fe1->inspoint = NULL;
	fe1->decllist = NULL;		/* force fresh */

	/* transform alt list */
	guppy_fetrans1_subtree (tnode_nthsubaddr (node, 1), fe1);

	return 0;
}
/*}}}*/
/*{{{  static int guppy_fetrans15_anode (compops_t *cops, tnode_t **nodep, guppy_fetrans15_t *fe15)*/
/*
 *	does fetrans1.5 on an alt node (look at body only)
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_fetrans15_anode (compops_t *cops, tnode_t **nodep, guppy_fetrans15_t *fe15)
{
	guppy_fetrans15_subtree (tnode_nthsubaddr (*nodep, 1), fe15);
	return 0;
}
/*}}}*/
/*{{{  static int guppy_fetrans3_anode (compops_t *cops, tnode_t **nodep, guppy_fetrans3_t *fe3)*/
/*
 *	does fetrans3 transform on an alternative -- this breaks into the alt and a case on the selection.
 *	returns 0 to stop walk, non-zero to continue.
 */
static int guppy_fetrans3_anode (compops_t *cops, tnode_t **nodep, guppy_fetrans3_t *fe3)
{
	if (((*nodep)->tag == gup.tag_ALT) || ((*nodep)->tag == gup.tag_PRIALT)) {
		tnode_t *node = *nodep;
		tnode_t *newseq, *newseqlist;
		tnode_t *glist = tnode_nthsubof (node, 1);
		tnode_t *gclist, *optlist;
		tnode_t *choice;
		int isprialt = (node->tag == gup.tag_PRIALT);
		int hasskip = 0;
		int i;

		newseqlist = parser_newlistnode (SLOCI);
		newseq = tnode_createfrom (gup.tag_SEQ, node, NULL, newseqlist);
		gclist = parser_newlistnode (SLOCI);
		optlist = parser_newlistnode (SLOCI);
		choice = tnode_createfrom (gup.tag_CASE, node, tnode_nthsubof (node, 0), optlist);

		/* scoop up the channels and put in 'gclist', also construct cases for the choice */
		for (i=0; i<parser_countlist (glist); i++) {
			tnode_t *guard = parser_getfromlist (glist, i);
			tnode_t *decls = NULL;

			/* could well be a DECLBLOCK by now */
			if (guard->tag == gup.tag_DECLBLOCK) {
				decls = tnode_nthsubof (guard, 0);
				guard = tnode_nthsubof (guard, 1);
			}
			if (guard->tag != gup.tag_GUARD) {
				nocc_internal ("guppy_fetrans3_anode(): guard not GUARD, got [%s:%s]", guard->tag->ndef->name, guard->tag->name);
				fe3->error++;
				return 0;
			} else {
				tnode_t *gpcond = tnode_nthsubof (guard, 0);
				tnode_t *gitem = tnode_nthsubof (guard, 1);

				if (gitem->tag == gup.tag_INPUT) {
					tnode_t *chancopy = tnode_nthsubof (gitem, 0);
					tnode_t *optval = constprop_newconst (CONST_INT, NULL, NULL, i);
					tnode_t *optproclist = parser_newlistnode (SLOCI);
					tnode_t *optproc, *opt;

					if (decls) {
						optproc = tnode_createfrom (gup.tag_DECLBLOCK, decls, decls,
								tnode_createfrom (gup.tag_SEQ, gitem, NULL, optproclist));
					} else {
						optproc = tnode_createfrom (gup.tag_SEQ, gitem, NULL, optproclist);
					}
					opt = tnode_createfrom (gup.tag_OPTION, gitem, optval, optproc);

					parser_addtolist (gclist, chancopy);
					parser_addtolist (optproclist, gitem);
					parser_addtolist (optproclist, tnode_nthsubof (guard, 2));

					/* do fetrans3 on the whole option */
					guppy_fetrans3_subtree (&opt, fe3);

					parser_addtolist (optlist, opt);
				} else if (gitem->tag == gup.tag_SKIP) {
					tnode_t *optval = constprop_newconst (CONST_INT, NULL, NULL, -1);
					tnode_t *optproclist = parser_newlistnode (SLOCI);
					tnode_t *optproc, *opt;

					if (decls) {
						optproc = tnode_createfrom (gup.tag_DECLBLOCK, decls, decls,
								tnode_createfrom (gup.tag_SEQ, gitem, NULL, optproclist));
					} else {
						optproc = tnode_createfrom (gup.tag_SEQ, gitem, NULL, optproclist);
					}
					opt = tnode_createfrom (gup.tag_OPTION, gitem, optval, optproc);

					/* nothing to add to the channel list: if skip is not last, will go wrong */
					hasskip = 1;
					parser_addtolist (optproclist, tnode_nthsubof (guard, 2));

					/* do fetrans3 on the whole option */
					guppy_fetrans3_subtree (&opt, fe3);

					parser_addtolist (optlist, opt);

				}
			}
		}

		/* replace ALT body with channel-list */
		tnode_setnthsub (node, 1, gclist);

		if (hasskip && (node->tag == gup.tag_PRIALT)) {
			/* turn into pri-alt-with-skip */
			node->tag = gup.tag_PRIALTSKIP;
		}

		parser_addtolist (newseqlist, node);
		parser_addtolist (newseqlist, choice);

		*nodep = newseq;

		return 0;
	}
	return 1;
}
/*}}}*/
/*{{{  static int guppy_namemap_anode (compops_t *cops, tnode_t **nodep, map_t *map)*/
/*
 *	does name-mapping transform for an alternative.
 *	returns 0 to stop walk, non-zero to continue.
 */
static int guppy_namemap_anode (compops_t *cops, tnode_t **nodep, map_t *map)
{
	if (((*nodep)->tag == gup.tag_ALT) || ((*nodep)->tag == gup.tag_PRIALT) || ((*nodep)->tag == gup.tag_PRIALTSKIP)) {
		tnode_t *pacall, *pacallnum;
		tnode_t *newparams;
		tnode_t *wptr;
		cccsp_mapdata_t *cmd = (cccsp_mapdata_t *)map->hook;
		int useskip = ((*nodep)->tag == gup.tag_PRIALTSKIP);
		int apicall;
		int i;

		if ((*nodep)->tag == gup.tag_PRIALT) {
			apicall = PROC_PRIALT;
		} else if ((*nodep)->tag == gup.tag_PRIALTSKIP) {
			apicall = PROC_PRIALTSKIP;
		} else {
			apicall = PROC_ALT;
		}

		wptr = cmd->process_id;
		map_submapnames (&wptr, map);

		cmd->target_indir = 0;
		map_submapnames (tnode_nthsubaddr (*nodep, 0), map);				/* map selection var */

		newparams = parser_newlistnode (SLOCI);
		pacallnum = cccsp_create_apicallname (apicall);
		pacall = tnode_createfrom (gup.tag_APICALLR, *nodep, pacallnum, newparams, tnode_nthsubof (*nodep, 0));
		parser_addtolist (newparams, wptr);
		for (i=0; i<parser_countlist (tnode_nthsubof (*nodep, 1)); i++) {
			tnode_t *item = parser_getfromlist (tnode_nthsubof (*nodep, 1), i);

			cmd->target_indir = 1;
#if 0
fhandle_printf (FHAN_STDERR, "guppy_namemap_anode(): item %d is:\n", i);
tnode_dumptree (item, 1, FHAN_STDERR);
#endif
			map_submapnames (&item, map);

			parser_addtolist (newparams, item);
		}
		parser_addtolist (newparams, cccsp_create_null (OrgOf (*nodep), map->target));

		*nodep = pacall;

#if 0
fhandle_printf (FHAN_STDERR, "guppy_namemap_anode(): map for alternative process (API call):\n");
tnode_dumptree (*nodep, 1, FHAN_STDERR);
#endif
		return 0;
	}
	return 1;
}
/*}}}*/


/*{{{  static int guppy_typecheck_guard (compops_t *cops, tnode_t *node, typecheck_t *tc)*/
/*
 *	does type-checking for a guard (used in alt).
 *	returns 0 to stop walk, 1 to continue.
 */
static int guppy_typecheck_guard (compops_t *cops, tnode_t *node, typecheck_t *tc)
{
	tnode_t *gpre = tnode_nthsubof (node, 0);
	tnode_t *gexpr = tnode_nthsubof (node, 1);
	tnode_t *gproc = tnode_nthsubof (node, 2);

	if (gpre) {
		tnode_t *ptype;

		typecheck_subtree (gpre, tc);
		ptype = typecheck_gettype (gpre, guppy_cnode_booltypenode);
		if (!ptype) {
			typecheck_error (node, tc, "failed to get pre-condition type");
			return 0;
		}
	}
	/* only sensible things should be parseable as guards, but check anyway */
	if (!gexpr) {
		typecheck_error (node, tc, "missing guard expression");
		return 0;
	}
	if (gexpr->tag == gup.tag_SKIP) {
		/* skip guard, okay */
	} else if ((gexpr->tag == gup.tag_INPUT) || (gexpr->tag == gup.tag_CASEINPUT)) {
		/* input or tagged-input, okay */
	} else {
		typecheck_error (node, tc, "invalid guard type [%s:%s]", gexpr->tag->ndef->name, gexpr->tag->name);
		return 0;
	}

	return 1;
}
/*}}}*/
/*{{{  static int guppy_fetrans1_guard (compops_t *cops, tnode_t **nodep, guppy_fetrans1_t *fe1)*/
/*
 *	does fetrans1 transform for an ALT guard -- just sets insertpoint for new temporaries to be local to the guarded process.
 *	returns 0 to stop walk, 1 to continue.
 */
static int guppy_fetrans1_guard (compops_t *cops, tnode_t **nodep, guppy_fetrans1_t *fe1)
{
	guppy_fetrans1_subtree (tnode_nthsubaddr (*nodep, 0), fe1);
	guppy_fetrans1_subtree (tnode_nthsubaddr (*nodep, 1), fe1);
#if 0
fhandle_printf (FHAN_STDERR, "guppy_fetrans1_guard(): here, body before transform:\n");
tnode_dumptree (tnode_nthsubof (*nodep, 2), 1, FHAN_STDERR);
#endif
	guppy_fetrans1_subtree_newtemps (tnode_nthsubaddr (*nodep, 2), fe1);
#if 0
fhandle_printf (FHAN_STDERR, "guppy_fetrans1_guard(): here, body after transform:\n");
tnode_dumptree (tnode_nthsubof (*nodep, 2), 1, FHAN_STDERR);
#endif

	return 0;
}
/*}}}*/
/*{{{  static int guppy_fetrans15_guard (compopts_t *cops, tnode_t **nodep, guppy_fetrans15_t *fe15)*/
/*
 *	does fetrans1.5 for a guard node (trivial here)
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_fetrans15_guard (compopts_t *cops, tnode_t **nodep, guppy_fetrans15_t *fe15)
{
	int saved = fe15->expt_proc;

	fe15->expt_proc = 1;								/* guard should be a process (input, timeout, skip) */
	guppy_fetrans15_subtree (tnode_nthsubaddr (*nodep, 1), fe15);
	guppy_fetrans15_subtree (tnode_nthsubaddr (*nodep, 2), fe15);			/* and the body */

	fe15->expt_proc = saved;
	return 0;
}
/*}}}*/


/*{{{  static int guppy_cnode_init_nodes (void)*/
/*
 *	called to initialise parse-tree nodes for constructors
 *	returns 0 on success, non-zero on failure
 */
static int guppy_cnode_init_nodes (void)
{
	tndef_t *tnd;
	int i;
	compops_t *cops;
	langops_t *lops;

	/*{{{  guppy:cnode -- SEQ, PAR*/
	i = -1;
	tnd = tnode_newnodetype ("guppy:cnode", &i, 2, 0, 0, TNF_LONGPROC);		/* subnodes: 0 = expr/operand/parspaceref; 1 = body */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "prescope", 2, COMPOPTYPE (guppy_prescope_cnode));
	tnode_setcompop (cops, "declify", 2, COMPOPTYPE (guppy_declify_cnode));
	tnode_setcompop (cops, "autoseq", 2, COMPOPTYPE (guppy_autoseq_cnode));
	tnode_setcompop (cops, "flattenseq", 1, COMPOPTYPE (guppy_flattenseq_cnode));
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (guppy_scopein_cnode));
	tnode_setcompop (cops, "postscope", 1, COMPOPTYPE (guppy_postscope_cnode));
	tnode_setcompop (cops, "fetrans", 2, COMPOPTYPE (guppy_fetrans_cnode));
	tnode_setcompop (cops, "fetrans1", 2, COMPOPTYPE (guppy_fetrans1_cnode));
	tnode_setcompop (cops, "fetrans15", 2, COMPOPTYPE (guppy_fetrans15_cnode));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (guppy_namemap_cnode));
	tnode_setcompop (cops, "lpreallocate", 2, COMPOPTYPE (guppy_lpreallocate_cnode));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (guppy_codegen_cnode));
	tnode_setcompop (cops, "reallocate", 2, COMPOPTYPE (guppy_reallocate_cnode));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "do_usagecheck", 2, LANGOPTYPE (guppy_dousagecheck_cnode));
	tnd->lops = lops;

	i = -1;
	gup.tag_SEQ = tnode_newnodetag ("SEQ", &i, tnd, NTF_INDENTED_PROC_LIST);
	i = -1;
	gup.tag_PAR = tnode_newnodetag ("PAR", &i, tnd, NTF_INDENTED_PROC_LIST);

	/*}}}*/
	/*{{{  guppy:replcnode -- REPLSEQ, REPLPAR*/
	i = -1;
	tnd = tnode_newnodetype ("guppy:replcnode", &i, 5, 0, 0, TNF_LONGPROC);		/* subnodes: 0 = expr/operand/parspaceref; 1 = body; 2 = repl-name; 3 = start-expr; 4 = count-expr */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (guppy_scopein_replcnode));
	tnode_setcompop (cops, "scopeout", 2, COMPOPTYPE (guppy_scopeout_replcnode));
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (guppy_typecheck_replcnode));
	tnode_setcompop (cops, "fetrans15", 2, COMPOPTYPE (guppy_fetrans15_replcnode));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (guppy_namemap_replcnode));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (guppy_codegen_replcnode));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnd->lops = lops;

	i = -1;
	gup.tag_REPLSEQ = tnode_newnodetag ("REPLSEQ", &i, tnd, NTF_INDENTED_PROC_LIST);
	i = -1;
	gup.tag_REPLPAR = tnode_newnodetag ("REPLPAR", &i, tnd, NTF_INDENTED_PROC_LIST);

	/*}}}*/
	/*{{{  guppy:anode -- ALT, PRIALT, PRIALTSKIP*/
	i = -1;
	tnd = tnode_newnodetype ("guppy:anode", &i, 2, 0, 0, TNF_LONGPROC);		/* subnodes: 0 = selection-var; 1 = body (list of guards) */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (guppy_typecheck_anode));
	tnode_setcompop (cops, "fetrans1", 2, COMPOPTYPE (guppy_fetrans1_anode));
	tnode_setcompop (cops, "fetrans15", 2, COMPOPTYPE (guppy_fetrans15_anode));
	tnode_setcompop (cops, "fetrans3", 2, COMPOPTYPE (guppy_fetrans3_anode));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (guppy_namemap_anode));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnd->lops = lops;

	i = -1;
	gup.tag_ALT = tnode_newnodetag ("ALT", &i, tnd, NTF_INDENTED_DGUARD_LIST);
	i = -1;
	gup.tag_PRIALT = tnode_newnodetag ("PRIALT", &i, tnd, NTF_INDENTED_DGUARD_LIST);
	i = -1;
	gup.tag_PRIALTSKIP = tnode_newnodetag ("PRIALTSKIP", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  guppy:guard -- GUARD*/
	i = -1;
	tnd = tnode_newnodetype ("guppy:guard", &i, 3, 0, 0, TNF_NONE);			/* subnodes: 0 = pre-condition, 1 = guard-process, 2 = body */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (guppy_typecheck_guard));
	tnode_setcompop (cops, "fetrans1", 2, COMPOPTYPE (guppy_fetrans1_guard));
	tnode_setcompop (cops, "fetrans15", 2, COMPOPTYPE (guppy_fetrans15_guard));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnd->lops = lops;

	i = -1;
	gup.tag_GUARD = tnode_newnodetag ("GUARD", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  guppy:replanode -- REPLALT*/
	i = -1;
	tnd = tnode_newnodetype ("guppy:replanode", &i, 5, 0, 0, TNF_LONGPROC);		/* subnodes: 0 = unused; 1 = body; 2 = repl-name; 3 = start-expr; 4 = count-expr */
	cops = tnode_newcompops ();
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnd->lops = lops;

	i = -1;
	gup.tag_REPLALT = tnode_newnodetag ("REPLALT", &i, tnd, NTF_INDENTED_DGUARD_LIST);

	/*}}}*/

	return 0;
}
/*}}}*/
/*{{{  static int guppy_cnode_post_setup (void)*/
/*
 *	called to do post-setup for constructors
 *	returns 0 on success, non-zero on failure
 */
static int guppy_cnode_post_setup (void)
{
	guppy_cnode_inttypenode = guppy_newprimtype (gup.tag_INT, NULL, 0);
	guppy_cnode_booltypenode = guppy_newprimtype (gup.tag_BOOL, NULL, 0);

	return 0;
}
/*}}}*/


/*{{{  guppy_cnode_feunit (feunit_t)*/
feunit_t guppy_cnode_feunit = {
	.init_nodes = guppy_cnode_init_nodes,
	.reg_reducers = NULL,
	.init_dfatrans = NULL,
	.post_setup = guppy_cnode_post_setup,
	.ident = "guppy-cnode"
};
/*}}}*/

