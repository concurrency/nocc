/*
 *	guppy_instance.c -- procedure and function instances for Guppy
 *	Copyright (C) 2013 Fred Barnes <frmb@kent.ac.uk>
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
#include "origin.h"
#include "fhandle.h"
#include "symbols.h"
#include "keywords.h"
#include "lexer.h"
#include "lexpriv.h"
#include "tnode.h"
#include "parser.h"
#include "dfa.h"
#include "dfaerror.h"
#include "parsepriv.h"
#include "guppy.h"
#include "feunit.h"
#include "fcnlib.h"
#include "names.h"
#include "scope.h"
#include "prescope.h"
#include "library.h"
#include "typecheck.h"
#include "constprop.h"
#include "precheck.h"
#include "usagecheck.h"
#include "map.h"
#include "target.h"
#include "transputer.h"
#include "codegen.h"
#include "langops.h"
#include "fetrans.h"
#include "betrans.h"
#include "metadata.h"
#include "tracescheck.h"
#include "mobilitycheck.h"
#include "cccsp.h"


/*}}}*/
/*{{{  private types*/

/*}}}*/


/*{{{  static int guppy_prescope_instance (compops_t *cops, tnode_t **nodep, prescope_t *ps)*/
/*
 *	does pre-scoping on an instance node
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_prescope_instance (compops_t *cops, tnode_t **nodep, prescope_t *ps)
{
	parser_ensurelist (tnode_nthsubaddr (*nodep, 1), *nodep);
	return 1;
}
/*}}}*/
/*{{{  static int guppy_scopein_instance (compops_t *cops, tnode_t **nodep, scope_t *ss)*/
/*
 *	called to scope-in a function instance
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_scopein_instance (compops_t *cops, tnode_t **nodep, scope_t *ss)
{
	return 1;
}
/*}}}*/
/*{{{  static int guppy_typecheck_instance (compops_t *cops, tnode_t *node, typecheck_t *tc)*/
/*
 *	called to do type-checking on a function instance
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_typecheck_instance (compops_t *cops, tnode_t *node, typecheck_t *tc)
{
	tnode_t *fparamlist = typecheck_gettype (tnode_nthsubof (node, 0), NULL);
	char *fcnname = NameNameOf (tnode_nthnameof (tnode_nthsubof (node, 0), 0));
	tnode_t *aparamlist = tnode_nthsubof (node, 1);
	tnode_t **fp_items, **ap_items;
	int fp_nitems, ap_nitems;
	int i;

	if (fparamlist->tag == gup.tag_FCNTYPE) {
		/* use parameters side of function type */
		fparamlist = tnode_nthsubof (fparamlist, 0);
	}
#if 0
fhandle_printf (FHAN_STDERR, "guppy_typecheck_instance(): instance of:\n");
tnode_dumptree (node, 1, FHAN_STDERR);
fhandle_printf (FHAN_STDERR, "fparamlist =\n");
tnode_dumptree (fparamlist, 1, FHAN_STDERR);
fhandle_printf (FHAN_STDERR, "aparamlist =\n");
tnode_dumptree (aparamlist, 1, FHAN_STDERR);
#endif
	if (!parser_islistnode (fparamlist) || !parser_islistnode (aparamlist)) {
		typecheck_error (node, tc, "invalid instance of [%s]", fcnname);
		return 0;
	}
	fp_items = parser_getlistitems (fparamlist, &fp_nitems);
	ap_items = parser_getlistitems (aparamlist, &ap_nitems);

	if (ap_nitems < fp_nitems) {
		typecheck_error (node, tc, "too few parameters to function [%s]", fcnname);
		return 0;
	} else if (ap_nitems > fp_nitems) {
		typecheck_error (node, tc, "too many parameters to function [%s]", fcnname);
		return 0;
	}

	/* do a type-check on actual parameters */
	typecheck_subtree (aparamlist, tc);

	for (i=0; i<fp_nitems; i++) {
		/*{{{  type-check/type-actual parameter*/
		tnode_t *ftype, *atype;
		ntdef_t *ftag;
		tnode_t *rtype;

		ftype = typecheck_gettype (fp_items[i], NULL);
		atype = typecheck_gettype (ap_items[i], ftype);

		ftag = (tnode_nthsubof (fp_items[i], 0))->tag;
		if (ftag != gup.tag_NVALPARAM) {
			/* must be a variable */
			if (!langops_isvar (ap_items[i])) {
				typecheck_error (node, tc, "parameter %d to [%s] must be a variable", i+1, fcnname);
			}
		}

		rtype = typecheck_typeactual (ftype, atype, node, tc);
		if (!rtype) {
			typecheck_error (node, tc, "incompatible types for parameter %d", i+1);
		}

		/*}}}*/
	}

	return 0;
}
/*}}}*/
/*{{{  static int guppy_fetrans1_instance (compops_t *cops, tnode_t **nodep, guppy_fetrans1_t *fe1)*/
/*
 *	does fetrans1 on an instance node (breaks down internal instance nodes into flat assignments to temporaries)
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_fetrans1_instance (compops_t *cops, tnode_t **nodep, guppy_fetrans1_t *fe1)
{
	tnode_t *itype = typecheck_gettype (tnode_nthsubof (*nodep, 0), NULL);
	tnode_t *node = *nodep;
	int lonely = 0;
	tnode_t **saved_inspoint = fe1->inspoint;		/* save */
	tnode_t *saved_decllist = fe1->decllist;

	if (itype && (itype->tag == gup.tag_FCNTYPE)) {
		tnode_t *rtype, **rtitems;
		int i, nrtitems;
		tnode_t *seqitemlist;
		tnode_t *sass, *namelist;

		if (!fe1->inspoint) {
			/* means we are probably a stand-alone instance, but will need temporaries */
			fe1->inspoint = nodep;
			lonely = 1;
		}

		/* do name and params first -- will pull out nested things in params */
		guppy_fetrans1_subtree (tnode_nthsubaddr (node, 0), fe1);
		guppy_fetrans1_subtree (tnode_nthsubaddr (node, 1), fe1);

		rtype = tnode_nthsubof (itype, 1);
#if 0
fhandle_printf (FHAN_STDERR, "guppy_fetrans1_instance(): looking at instance:\n");
tnode_dumptree (node, 1, FHAN_STDERR);
#endif
		if (!parser_islistnode (rtype)) {
			nocc_internal ("guppy_fetrans1_instance(): RHS of FCNTYPE is not a list, got [%s:%s]", rtype->tag->ndef->name, rtype->tag->name);
			return 0;
		}
		rtitems = parser_getlistitems (rtype, &nrtitems);

		namelist = parser_newlistnode (NULL);
		for (i=0; i<nrtitems; i++) {
			tnode_t *nname, *ntype;
			
			ntype = tnode_copytree (rtitems[i]);
			/* create a temporary for this parameter */
			nname = guppy_fetrans1_maketemp (gup.tag_NDECL, node, ntype, NULL, fe1);

			parser_addtolist (namelist, nname);
		}

		/* construct assignment */
		sass = tnode_createfrom (gup.tag_SASSIGN, node, namelist, node, tnode_copytree (rtype));

		if (lonely) {
			/* replace with this simply */
			*nodep = sass;
		} else {
			/* construct SEQ and insert */
			tnode_t *seqlist = parser_newlistnode (NULL);
			tnode_t *newseq = tnode_createfrom (gup.tag_SEQ, node, NULL, seqlist);
			tnode_t **newpt;

			parser_addtolist (seqlist, sass);
			newpt = parser_addtolist (seqlist, *fe1->inspoint);

			*fe1->inspoint = newseq;
			fe1->inspoint = newpt;
		}

		/* modify instance */
		if (!lonely) {
			if (parser_countlist (namelist) == 1) {
				*nodep = parser_getfromlist (namelist, 0);
			} else {
				*nodep = tnode_copytree (namelist);
			}
		} else {
			/* alone: restore fe1 insert-point -- can't put subsequent declarations inside this! */
			fe1->inspoint = saved_inspoint;
			fe1->decllist = saved_decllist;
		}

		return 0;
	}

	/* else might still need to fetrans1 on parameters, and these might need temporaries */
	fe1->inspoint = nodep;
	fe1->decllist = NULL;
	guppy_fetrans1_subtree (tnode_nthsubaddr (node, 0), fe1);
	guppy_fetrans1_subtree (tnode_nthsubaddr (node, 1), fe1);

	fe1->inspoint = saved_inspoint;
	fe1->decllist = saved_decllist;

	return 0;
}
/*}}}*/
/*{{{  static int guppy_namemap_instance (compops_t *cops, tnode_t **nodep, map_t *map)*/
/*
 *	does name-mapping for an instance node
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_namemap_instance (compops_t *cops, tnode_t **nodep, map_t *map)
{
	tnode_t **nameptr = tnode_nthsubaddr (*nodep, 0);
	tnode_t *pname = *nameptr, *pdecl;
	tnode_t *plist = tnode_nthsubof (*nodep, 1);
	tnode_t *flist;
	tnode_t **fparams;
	tnode_t **params;
	int i, nparams, nfparams;
	cccsp_mapdata_t *cmd = (cccsp_mapdata_t *)map->hook;

	/* map name and parameters */
	map_submapnames (nameptr, map);

	/* before anything else, insert the special 'wptr' paramter -- first parameter */
	parser_addtolist_front (plist, cmd->process_id);

	pdecl = NameDeclOf (tnode_nthnameof (pname, 0));
	flist = tnode_nthsubof (pdecl, 1);

	fparams = parser_getlistitems (flist, &nfparams);
	params = parser_getlistitems (plist, &nparams);

#if 0
fhandle_printf (FHAN_STDERR, "guppy_namemap_instance(): instance of:\n");
tnode_dumptree (pname, 1, FHAN_STDERR);
fhandle_printf (FHAN_STDERR, "guppy_namemap_instance(): got %d formals:\n", nfparams);
tnode_dumptree (flist, 1, FHAN_STDERR);
fhandle_printf (FHAN_STDERR, "guppy_namemap_instance(): and %d actuals:\n", nparams);
tnode_dumptree (plist, 1, FHAN_STDERR);
#endif
	if (nparams != nfparams) {
		nocc_internal ("guppy_namemap_instance(): collecting params had %d formal and %d actual", nfparams, nparams);
		return 0;
	}

#if 0
fhandle_printf (FHAN_STDERR, "guppy_namemap_instance(): formals are:\n");
tnode_dumptree (flist, 1, FHAN_STDERR);
#endif
	for (i=0; i<nparams; i++) {
		cccsp_mapdata_t *cmap = (cccsp_mapdata_t *)map->hook;
		int saved_indir = cmap->target_indir;

		cmap->target_indir = cccsp_get_indir (fparams[i], map->target);
#if 0
fhandle_printf (FHAN_STDERR, "guppy_namemap_instance(): target_indir set to %d, doing param..\n", cmap->target_indir);
#endif
		map_submapnames (params + i, map);
		cmap->target_indir = saved_indir;
	}

	return 0;
}
/*}}}*/
/*{{{  static int guppy_lpreallocate_instance (compops_t *cops, tnode_t *node, cccsp_preallocate_t *cpa)*/
/*
 *	does pre-allocation for an instance node
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_lpreallocate_instance (compops_t *cops, tnode_t *node, cccsp_preallocate_t *cpa)
{
	if (node->tag == gup.tag_INSTANCE) {
		name_t *iname = tnode_nthnameof (tnode_nthsubof (node, 0), 0);
		tnode_t *decl = NameDeclOf (iname);
		tnode_t *dbody = tnode_nthsubof (decl, 2);
		tnode_t *parms = tnode_nthsubof (node, 1);
		tnode_t **pitems;
		int npitems, i;
		int blksize;

		/* the body of the instance'd thing should be a back-end-block by now (even if separately compiled) */
		if (dbody->tag != cpa->target->tag_BLOCK) {
			nocc_internal ("guppy_lpreallocate_instance(): body of INSTANCE name not back-end block, got [%s:%s]",
					dbody->tag->ndef->name, dbody->tag->name);
			return 0;
		}
		cccsp_getblockspace (dbody, NULL, &blksize);

		cpa->collect += blksize;
		pitems = parser_getlistitems (parms, &npitems);
		for (i=0; i<npitems; i++) {
			cccsp_preallocate_subtree (pitems[i], cpa);
		}
		cpa->collect += 16;		/* for the call itself probably */
	} else if (node->tag == gup.tag_APICALL) {
		int w = cccsp_stkwords_apicallnode (tnode_nthsubof (node, 0));
		tnode_t *parms = tnode_nthsubof (node, 1);
		tnode_t **pitems;
		int npitems, i;

		if (w > 0) {
			cpa->collect += w;
		}
		pitems = parser_getlistitems (parms, &npitems);
		for (i=0; i<npitems; i++) {
			cccsp_preallocate_subtree (pitems[i], cpa);
		}
		cpa->collect += 16;		/* for the call itself probably */
	} else {
		nocc_internal ("guppy_lpreallocate_instance(): unhandled [%s:%s]", node->tag->ndef->name, node->tag->name);
		return 0;
	}
	return 0;
}
/*}}}*/
/*{{{  static int guppy_codegen_instance (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	does code-generation for an instance node
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_codegen_instance (compops_t *cops, tnode_t *node, codegen_t *cgen)
{
	if (node->tag == gup.tag_INSTANCE) {
		name_t *pname = tnode_nthnameof (tnode_nthsubof (node, 0), 0);

		codegen_callops (cgen, c_proccall, NameNameOf (pname), tnode_nthsubof (node, 1), 0, NULL);
	} else if (node->tag == gup.tag_APICALL) {
		tnode_t *callnum = tnode_nthsubof (node, 0);

		if (!constprop_isconst (callnum)) {
			/* unexpected! */
			nocc_internal ("guppy_codegen_instance(): APICALL but name was [%s]", callnum->tag->name);
			return 0;
		}
		codegen_callops (cgen, c_proccall, NULL, tnode_nthsubof (node, 1), constprop_intvalof (callnum), NULL);
	} else {
		nocc_internal ("guppy_codegen_instance(): unhandled [%s:%s]", node->tag->ndef->name, node->tag->name);
		return 0;
	}
	return 0;
}
/*}}}*/
/*{{{  static int guppy_cccspdcg_instance (compops_t *cops, tnode_t *node, cccsp_dcg_t *dcg)*/
/*
 *	used during direct-call-graph generation
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_cccspdcg_instance (compops_t *cops, tnode_t *node, cccsp_dcg_t *dcg)
{
	if (node->tag == gup.tag_INSTANCE) {
		name_t *pname = tnode_nthnameof (tnode_nthsubof (node, 0), 0);
		cccsp_sfi_entry_t *sfient = cccsp_sfiofname (pname, 0);

		if (dcg->thisfcn && sfient) {
			cccsp_sfi_addchild (dcg->thisfcn, sfient);
		}
	} else if (node->tag == gup.tag_APICALL) {
		tnode_t *callnum = tnode_nthsubof (node, 0);
		char *ename = cccsp_make_apicallname (callnum);
		cccsp_sfi_entry_t *sfient = cccsp_sfi_lookupornew (ename);

		sfree (ename);
		if (dcg->thisfcn) {
			cccsp_sfi_addchild (dcg->thisfcn, sfient);
		}
	}
	return 0;
}
/*}}}*/

/*{{{  static tnode_t *guppy_gettype_instance (langops_t *lops, tnode_t *node, tnode_t *default_type)*/
/*
 *	gets the type of an instance -- return types for function
 */
static tnode_t *guppy_gettype_instance (langops_t *lops, tnode_t *node, tnode_t *default_type)
{
	tnode_t *fname = tnode_nthsubof (node, 0);
	name_t *name = tnode_nthnameof (fname, 0);
	tnode_t *ftype = NameTypeOf (name);

	if (ftype->tag == gup.tag_FCNTYPE) {
		/* function type, returns results */
		return tnode_nthsubof (ftype, 1);
	}
	return NULL;
}
/*}}}*/

/*{{{  static int guppy_lpreallocate_rinstance (compops_t *cops, tnode_t *node, cccsp_preallocate_t *cpa)*/
/*
 *	does pre-allocation for a result-instance node
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_lpreallocate_rinstance (compops_t *cops, tnode_t *node, cccsp_preallocate_t *cpa)
{
	if (node->tag == gup.tag_APICALLR) {
		int w = cccsp_stkwords_apicallnode (tnode_nthsubof (node, 0));
		tnode_t *parms = tnode_nthsubof (node, 1);
		tnode_t **pitems;
		int npitems, i;

		if (w > 0) {
			cpa->collect += w;
		}
		pitems = parser_getlistitems (parms, &npitems);
		for (i=0; i<npitems; i++) {
			cccsp_preallocate_subtree (pitems[i], cpa);
		}
		cpa->collect += 16;		/* for the call itself probably */
	} else {
		nocc_internal ("guppy_lpreallocate_rinstance(): unhanded [%s:%s]", node->tag->ndef->name, node->tag->name);
		return 0;
	}
	return 0;
}
/*}}}*/
/*{{{  static int guppy_codegen_rinstance (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	does code-generation for a result-instance node
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_codegen_rinstance (compops_t *cops, tnode_t *node, codegen_t *cgen)
{
	if (node->tag == gup.tag_APICALLR) {
		tnode_t *callnum = tnode_nthsubof (node, 0);

		if (!constprop_isconst (callnum)) {
			/* unexpected! */
			nocc_internal ("guppy_codegen_instance(): APICALLR but name was [%s]", callnum->tag->name);
			return 0;
		}
		codegen_callops (cgen, c_proccall, NULL, tnode_nthsubof (node, 1), constprop_intvalof (callnum), tnode_nthsubof (node, 2));
	}
	return 0;
}
/*}}}*/
/*{{{  static int guppy_cccspdcg_rinstance (compops_t *cops, tnode_t *node, cccsp_dcg_t *dcg)*/
/*
 *	used during direct-call-graph generation
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_cccspdcg_rinstance (compops_t *cops, tnode_t *node, cccsp_dcg_t *dcg)
{
	if (node->tag == gup.tag_APICALLR) {
		tnode_t *callnum = tnode_nthsubof (node, 0);
		char *ename = cccsp_make_apicallname (callnum);
		cccsp_sfi_entry_t *sfient;

		sfient = cccsp_sfi_lookupornew (ename);
#if 0
fhandle_printf (FHAN_STDERR, "guppy_cccspdcg_rinstance(): here! ename=[%s]\n", ename);
#endif
		sfree (ename);
		if (dcg->thisfcn) {
			cccsp_sfi_addchild (dcg->thisfcn, sfient);
		}
	}
	return 0;
}
/*}}}*/

/*{{{  static int guppy_namemap_ppinstance (compops_t *cops, tnode_t **nodep, map_t *map)*/
/*
 *	does name-mapping for a parallel-process instance node
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_namemap_ppinstance (compops_t *cops, tnode_t **nodep, map_t *map)
{
	tnode_t **nameptr = tnode_nthsubaddr (*nodep, 0);
	tnode_t *pname = *nameptr, *pdecl, *pdeclblk;
	tnode_t *plist = tnode_nthsubof (*nodep, 2);
	tnode_t *newwptr = tnode_nthsubof (*nodep, 1);
	tnode_t *newws, **newwsp;
	tnode_t *flist;
	tnode_t **fparams;
	tnode_t **params;
	int i, nparams, nfparams;
	cccsp_mapdata_t *cmd = (cccsp_mapdata_t *)map->hook;
	guppy_map_t *gmap = (guppy_map_t *)cmd->langhook;
	tnode_t *ppseq, *ppseqlist, *ppinitcall, *ppinitargs;
	name_t *pnamename;

	/* map name and parameters */
	map_submapnames (nameptr, map);

#if 0
fhandle_printf (FHAN_STDERR, "guppy_namemap_ppinstance(): here!  *nodep =\n");
tnode_dumptree (*nodep, 1, FHAN_STDERR);
#endif
	pnamename = tnode_nthnameof (pname, 0);
	pdecl = NameDeclOf (pnamename);

	if (pdecl->tag != gup.tag_PFCNDEF) {
		nocc_internal ("guppy_namemap_ppinstance(): ppinstance'd definition is not a PFCNDEF, got [%s]", pdecl->tag->name);
		return 0;
	}
	pdeclblk = tnode_nthsubof (pdecl, 2);
	if (pdeclblk->tag != map->target->tag_BLOCK) {
		nocc_internal ("guppy_namemap_ppinstance(): ppinstance'd definition body is not back-end block, got [%s]", pdeclblk->tag->name);
		return 0;
	}

	// cccsp_getblockspace (pdeclblk, &blk_my, &blk_nest);
#if 0
fhandle_printf (FHAN_STDERR, "guppy_namemap_ppinstance(): mapped instance space is (%d,%d)\n", blk_my, blk_nest);
#endif

	params = parser_getlistitems (plist, &nparams);
	newws = cccsp_create_workspace (SLOCI, map->target);
	if (cmd->thisentry) {
		cmd->thisentry->wsspace = newws;
	}
	cccsp_set_workspace_nparams (newws, nparams);
	/* NOTE: we don't know how much space is needed, yet (preallocate pass handles that),
	 *       but leave a fixup in the list attached to the back-end block
	 */
	cccsp_addtofixups (pdeclblk, newws);
	// cccsp_set_workspace_nwords (newws, blk_nest);

	/* add new workspace to local declarations and map */
	if (!gmap->decllist) {
		nocc_internal ("guppy_namemap_ppinstance(): no declaration list!");
		return 0;
	}
	newwsp = parser_addtolist (gmap->decllist, newws);
	map_submapnames (newwsp, map);
#if 0
fhandle_printf (FHAN_STDERR, "guppy_namemap_ppinstance(): added newws to declaration list:\n");
tnode_dumptree (gmap->decllist, 1, FHAN_STDERR);
#endif

	ppseqlist = parser_newlistnode (OrgOf (*nodep));
	ppseq = tnode_createfrom (gup.tag_SEQ, *nodep, NULL, ppseqlist);

	ppinitargs = parser_newlistnode (SLOCI);
	parser_addtolist (ppinitargs, cmd->process_id);
	parser_addtolist (ppinitargs, newws);
	parser_addtolist (ppinitargs, constprop_newconst (CONST_INT, NULL, NULL, nparams));

	/* map before adding number of workspace words -- this can change up until code-gen */
	map_submapnames (&ppinitargs, map);
	parser_addtolist (ppinitargs, cccsp_create_workspace_nwordsof (newws, map->target));

	ppinitcall = tnode_create (gup.tag_APICALLR, SLOCI, cccsp_create_apicallname (LIGHT_PROC_INIT), ppinitargs, newwptr);
	map_submapnames (tnode_nthsubaddr (ppinitcall, 2), map);				/* map placed newwptr */

	parser_addtolist (ppseqlist, ppinitcall);

	flist = typecheck_gettype (pname, NULL);
	fparams = parser_getlistitems (flist, &nfparams);

	/* Note: neither 'fparams' nor 'params' should have a leading workspace ID */
#if 0
fhandle_printf (FHAN_STDERR, "guppy_namemap_ppinstance(): here 2!  flist is:\n");
tnode_dumptree (flist, 1, FHAN_STDERR);
#endif
	if (nparams != nfparams) {
		nocc_internal ("guppy_namemap_ppinstance(): nparams=%d but nfparams=%d", nparams, nfparams);
		return 0;
	}
	for (i=0; i<nparams; i++) {
		int saved_indir = cmd->target_indir;
		tnode_t *fpinode, *fpiargs;

		fpiargs = parser_newlistnode (SLOCI);
		parser_addtolist (fpiargs, cmd->process_id);
		parser_addtolist (fpiargs, newwptr);
		parser_addtolist (fpiargs, constprop_newconst (CONST_INT, NULL, NULL, i));
		map_submapnames (&fpiargs, map);
		/* wptr args and param-number get mapped above;  map actual separately */
		/* make sure we map the right amount of indirection */
		cmd->target_indir = cccsp_get_indir (fparams[i], map->target);
		map_submapnames (params + i, map);
		cmd->target_indir = saved_indir;

		parser_addtolist (fpiargs, params[i]);

		fpinode = tnode_create (gup.tag_APICALL, SLOCI, cccsp_create_apicallname (PROC_PARAM), fpiargs);
		parser_addtolist (ppseqlist, fpinode);
	}
#if 0
fhandle_printf (FHAN_STDERR, "guppy_namemap_ppinstance(): here 3!  plist now:\n");
tnode_dumptree (plist, 1, FHAN_STDERR);
#endif

	/* Note: lose PPINSTANCE node here */
	*nodep = ppseq;

	/* Note: clean-up happens *after* the parallel process has been run (in 'par' code, guppy_cnode.c) */

	return 0;
}
/*}}}*/
/*{{{  static int guppy_codegen_ppinstance (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	does code-generation for a parallel-process instance node
 *	returns 0 top stop walk, 1 to continue
 */
static int guppy_codegen_ppinstance (compops_t *cops, tnode_t *node, codegen_t *cgen)
{
#if 0
fhandle_printf (FHAN_STDERR, "guppy_codegen_ppinstance(): here!\n");
#endif
	/* Note: should be nothing to do here, all reduced to API calls */

	return 0;
}
/*}}}*/

/*{{{  static int guppy_instance_init_nodes (void)*/
/*
 *	sets up function instance nodes for Guppy
 *	returns 0 on success, non-zero on error
 */
static int guppy_instance_init_nodes (void)
{
	tndef_t *tnd;
	compops_t *cops;
	langops_t *lops;
	int i;

	/*{{{  guppy:instance -- INSTANCE, APICALL*/
	i = -1;
	tnd = tnode_newnodetype ("guppy:instance", &i, 2, 0, 0, TNF_NONE);		/* subnodes: name, aparams */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "prescope", 2, COMPOPTYPE (guppy_prescope_instance));
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (guppy_scopein_instance));
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (guppy_typecheck_instance));
	tnode_setcompop (cops, "fetrans1", 2, COMPOPTYPE (guppy_fetrans1_instance));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (guppy_namemap_instance));
	tnode_setcompop (cops, "lpreallocate", 2, COMPOPTYPE (guppy_lpreallocate_instance));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (guppy_codegen_instance));
	tnode_setcompop (cops, "cccsp:dcg", 2, COMPOPTYPE (guppy_cccspdcg_instance));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "gettype", 2, LANGOPTYPE (guppy_gettype_instance));
	tnd->lops = lops;

	i = -1;
	gup.tag_INSTANCE = tnode_newnodetag ("INSTANCE", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_APICALL = tnode_newnodetag ("APICALL", &i, tnd, NTF_NONE);		/* only during/after "namemap" */

	/*}}}*/
	/*{{{  guppy:rinstance -- APICALLR*/
	i = -1;
	tnd = tnode_newnodetype ("guppy:rinstance", &i, 3, 0, 0, TNF_NONE);		/* subnodes: name, aparams, result */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "lpreallocate", 2, COMPOPTYPE (guppy_lpreallocate_rinstance));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (guppy_codegen_rinstance));
	tnode_setcompop (cops, "cccsp:dcg", 2, COMPOPTYPE (guppy_cccspdcg_rinstance));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnd->lops = lops;

	i = -1;
	gup.tag_APICALLR = tnode_newnodetag ("APICALLR", &i, tnd, NTF_NONE);		/* only during/after "namemap" */

	/*}}}*/
	/*{{{  guppy:ppinstance -- PPINSTANCE*/
	i = -1;
	tnd = tnode_newnodetype ("guppy:ppinstance", &i, 3, 0, 0, TNF_NONE);		/* subnodes: name, proc-var, aparams */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (guppy_namemap_ppinstance));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (guppy_codegen_ppinstance));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnd->lops = lops;

	i = -1;
	gup.tag_PPINSTANCE = tnode_newnodetag ("PPINSTANCE", &i, tnd, NTF_NONE);

	/*}}}*/

	return 0;
}
/*}}}*/
/*{{{  static int guppy_instance_post_setup (void)*/
/*
 *	does post-setup for function instance nodes
 *	returns 0 on success, non-zero on error
 */
static int guppy_instance_post_setup (void)
{
	return 0;
}
/*}}}*/


/*{{{  fuppy_instance_feunit (feunit_t)*/
feunit_t guppy_instance_feunit = {
	.init_nodes = guppy_instance_init_nodes,
	.reg_reducers = NULL,
	.init_dfatrans = NULL,
	.post_setup = guppy_instance_post_setup,
	.ident = "guppy-instance",
};

/*}}}*/

