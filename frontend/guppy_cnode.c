/*
 *	guppy_cnode.c -- constructor nodes for Guppy
 *	Copyright (C) 2010-2013 Fred Barnes <frmb@kent.ac.uk>
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
			scope_subtree (pitems + i, ss);

			/* anything that refers to high-level stuff will be recorded in fvlist */

			fvnode = tnode_createfrom (gup.tag_FVNODE, pitems[i], pitems[i], fvlist);
			pitems[i] = fvnode;
		}
		ss->lexlevel--;
	} 
	/* else trivial */
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

		decllist = parser_newlistnode (SLOCI);
		pcargs = parser_newlistnode (SLOCI);

		pitems = parser_getlistitems (parlist, &nitems);
		for (i=0; i<nitems; i++) {
			tnode_t *wsvar;

			wsvar = cccsp_create_wptr (OrgOf (pitems[i]), map->target);
			parser_addtolist (decllist, wsvar);

			if (pitems[i]->tag != gup.tag_PPINSTANCE) {
				nocc_internal ("guppy_namemap_cnode(): expected PPINSTANCE in PAR but got [%s]", pitems[i]->tag->name);
				return 0;
			}
			tnode_setnthsub (pitems[i], 1, wsvar);
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

			parser_addtolist (pcargs, pp_wptr);
			parser_addtolist (pcargs, pp_name);

			map_submapnames (pitems + i, map);
		}

		pccallnum = cccsp_create_apicallname (PROC_PAR);
		pccall = tnode_create (gup.tag_APICALL, SLOCI, pccallnum, pcargs);

		map_submapnames (&pcargs, map);

		parser_addtolist (oseqlist, pccall);
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
	tnode_setcompop (cops, "fetrans", 2, COMPOPTYPE (guppy_fetrans_cnode));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (guppy_namemap_cnode));
	tnode_setcompop (cops, "lpreallocate", 2, COMPOPTYPE (guppy_lpreallocate_cnode));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (guppy_codegen_cnode));
	tnd->ops = cops;
	lops = tnode_newlangops ();
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

