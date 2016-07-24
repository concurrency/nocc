/*
 *	guppy_fcndef.c -- Guppy procedure/function declarations for NOCC
 *	Copyright (C) 2010-2016 Fred Barnes <frmb@kent.ac.uk>
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
#include <stdint.h>
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
/*{{{  private data*/

static compop_t *inparams_scopein_compop = NULL;
static compop_t *inparams_scopeout_compop = NULL;
static compop_t *inparams_namemap_compop = NULL;
static compop_t *inparams_lnamemap_compop = NULL;


/*}}}*/


/*{{{  guppy_fcndefhook_t *guppy_newfcndefhook (void)*/
/*
 *	creates a new guppy_fcndefhook_t structure
 */
guppy_fcndefhook_t *guppy_newfcndefhook (void)
{
	guppy_fcndefhook_t *fdh = (guppy_fcndefhook_t *)smalloc (sizeof (guppy_fcndefhook_t));

	fdh->lexlevel = 0;
	fdh->ispublic = 0;
	fdh->istoplevel = 0;
	fdh->ispar = 0;
	fdh->pfcndef = NULL;

	return fdh;
}
/*}}}*/
/*{{{  void guppy_freefcndefhook (guppy_fcndefhook_t *fdh)*/
/*
 *	frees a guppy_fcndefhook_t structure
 */
void guppy_freefcndefhook (guppy_fcndefhook_t *fdh)
{
	if (!fdh) {
		nocc_internal ("guppy_freefcndefhook(): NULL pointer!");
		return;
	}
	sfree (fdh);
	return;
}
/*}}}*/


/*{{{  static void guppy_fcndef_hook_free (void *hook)*/
/*
 *	frees a function-definition hook
 */
static void guppy_fcndef_hook_free (void *hook)
{
	if (!hook) {
		return;
	}
	guppy_freefcndefhook ((guppy_fcndefhook_t *)hook);
}
/*}}}*/
/*{{{  static void *guppy_fcndef_hook_copy (void *hook)*/
/*
 *	copies a function-definition hook
 */
static void *guppy_fcndef_hook_copy (void *hook)
{
	guppy_fcndefhook_t *fdh, *ofdh;

	if (!hook) {
		return NULL;
	}
	ofdh = (guppy_fcndefhook_t *)hook;
	fdh = guppy_newfcndefhook ();
	memcpy (fdh, ofdh, sizeof (guppy_fcndefhook_t));

	return (void *)fdh;
}
/*}}}*/
/*{{{  static void guppy_fcndef_hook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)*/
/*
 *	dump-tree for function-definition hook
 */
static void guppy_fcndef_hook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)
{
	guppy_fcndefhook_t *fdh = (guppy_fcndefhook_t *)hook;

	if (!fdh) {
		return;
	}
	guppy_isetindent (stream, indent);
	fhandle_printf (stream, "<fcndefhook lexlevel=\"%d\" ispublic=\"%d\" istoplevel=\"%d\" ispar=\"%d\" pfcndef=\"%p\" />\n",
			fdh->lexlevel, fdh->ispublic, fdh->istoplevel, fdh->ispar, fdh->pfcndef);
	return;
}
/*}}}*/


/*{{{  static int guppy_prescope_fcndef (compops_t *cops, tnode_t **node, prescope_t *ps)*/
/*
 *	called to pre-scope a procedure definition
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_prescope_fcndef (compops_t *cops, tnode_t **node, prescope_t *ps)
{
	guppy_prescope_t *gps = (guppy_prescope_t *)ps->hook;
	tnode_t *name = tnode_nthsubof (*node, 0);
	char *rawname = NULL;
	guppy_fcndefhook_t *fdh = (guppy_fcndefhook_t *)tnode_nthhookof (*node, 0);

	if (!fdh) {
		fdh = guppy_newfcndefhook ();
		fdh->lexlevel = gps->procdepth;
		fdh->ispublic = 0;
		fdh->istoplevel = 0;
		fdh->ispar = 0;
		fdh->pfcndef = NULL;
		tnode_setnthhook (*node, 0, fdh);
	}

	if (name->tag == gup.tag_LITSTRING) {
		/* user-defined operator of some kind probably */
		rawname = guppy_udo_maketempfcnname (*node);
	} else {
		rawname = (char *)tnode_nthhookof (tnode_nthsubof (*node, 0), 0);
	}

	if (!gps->procdepth) {
		/* definition at top-level */
		int x;

		x = library_makepublic (node, rawname);
		if (x) {
			return 1;				/* go through subnodes */
		}
	} else {
		library_makeprivate (node, rawname);
		/* continue processing */
	}

	gps->last_type = NULL;
	parser_ensurelist (tnode_nthsubaddr (*node, 1), *node);

	/* prescope params and result types */
	prescope_subtree (tnode_nthsubaddr (*node, 1), ps);

	gps->last_type = NULL;

	if (tnode_nthsubof (*node, 3)) {
		parser_ensurelist (tnode_nthsubaddr (*node, 3), *node);
		prescope_subtree (tnode_nthsubaddr (*node, 3), ps);
	}

	/* do prescope on body, at higher procdepth */
	gps->procdepth++;
	prescope_subtree (tnode_nthsubaddr (*node, 2), ps);
	gps->procdepth--;

	return 0;					/* done all */
}
/*}}}*/
/*{{{  static int guppy_declify_fcndef (compops_t *cops, tnode_t **node, guppy_declify_t *gdl)*/
/*
 *	called to declify a procedure definition (body of)
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_declify_fcndef (compops_t *cops, tnode_t **node, guppy_declify_t *gdl)
{
	tnode_t **bodyptr = tnode_nthsubaddr (*node, 2);

	if (parser_islistnode (*bodyptr)) {
		guppy_declify_listtodecllist (bodyptr, gdl);
	}

	return 1;
}
/*}}}*/
/*{{{  static int guppy_autoseq_fcndef (compops_t *cops, tnode_t **node, guppy_autoseq_t *gas)*/
/*
 *	does auto-sequencing for a procedure definition
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_autoseq_fcndef (compops_t *cops, tnode_t **node, guppy_autoseq_t *gas)
{
	tnode_t **bodyptr = tnode_nthsubaddr (*node, 2);

	if (parser_islistnode (*bodyptr)) {
		guppy_autoseq_listtoseqlist (bodyptr, gas);
	}

	return 1;
}
/*}}}*/
/*{{{  static int guppy_scopein_fcndef (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	called to scope-in a procedure definition
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_scopein_fcndef (compops_t *cops, tnode_t **node, scope_t *ss)
{
	tnode_t *name = tnode_nthsubof (*node, 0);
	tnode_t **paramsptr = tnode_nthsubaddr (*node, 1);
	tnode_t **resultsptr = tnode_nthsubaddr (*node, 3);
	tnode_t **bodyptr = tnode_nthsubaddr (*node, 2);
	chook_t *nnschook = library_getnonamespacechook ();
	void *nsmark;
	char *rawname;
	name_t *fcnname;
	tnode_t *newname, *fcntype;
	tnode_t *nnsnode = NULL;

	nsmark = name_markscope ();

	/*{{{  walk parameters, results and body*/
	tnode_modprepostwalktree (paramsptr, scope_modprewalktree, scope_modpostwalktree, (void *)ss);
	if (*resultsptr) {
		tnode_modprepostwalktree (resultsptr, scope_modprewalktree, scope_modpostwalktree, (void *)ss);
	}

	/* if we have anything attached which needs parameters to be in scope, do that here */
	if (tnode_hascompop (cops, "inparams_scopein")) {
		tnode_callcompop (cops, "inparams_scopein", 2, node, ss);
	}

	tnode_modprepostwalktree (bodyptr, scope_modprewalktree, scope_modpostwalktree, (void *)ss);

	/*}}}*/

	/* if there is a corresponding scope-out, call that here (before parameters go out of scope!) */
	if (tnode_hascompop (cops, "inparams_scopeout")) {
		tnode_callcompop (cops, "inparams_scopeout", 2, node, ss);
	}

	name_markdescope (nsmark);

	if (nnschook && tnode_haschook (*node, nnschook)) {
		nnsnode = (tnode_t *)tnode_getchook (*node, nnschook);
	}

	/* declare and scope PROC name, then check process in the scope of it */
	if (name->tag == gup.tag_LITSTRING) {
		char *fstr = (char *)langops_constvalof (name, NULL);

		rawname = guppy_udo_newfunction (fstr, *resultsptr, *paramsptr);
	} else {
		rawname = tnode_nthhookof (name, 0);
	}

	/* if we have results, encode that in the type */
	if (*resultsptr) {
		fcntype = tnode_createfrom (gup.tag_FCNTYPE, *resultsptr, *paramsptr, *resultsptr);
	} else {
		fcntype = *paramsptr;
	}
	fcnname = name_addscopenamess (rawname, *node, fcntype, NULL, nnsnode ? NULL : ss);
	newname = tnode_createfrom (gup.tag_NFCNDEF, name, fcnname);
	SetNameNode (fcnname, newname);
	tnode_setnthsub (*node, 0, newname);

	/* free old name, scope process */
	tnode_free (name);
	ss->scoped++;

	return 0;				/* already walked children */
}
/*}}}*/
/*{{{  static int guppy_scopeout_fcndef (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	called to scope-out a procedure definition
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_scopeout_fcndef (compops_t *cops, tnode_t **node, scope_t *ss)
{
	/* NOTE: nested function definitions automatically de-scoped by enclosing body */
#if 0
	tnode_t *name = tnode_nthsubof (*node, 0);
	name_t *sname;

	if (name->tag != gup.tag_NFCNDEF) {
		scope_error (name, ss, "not NFCNDEF!");
		return 0;
	}
	sname = tnode_nthnameof (name, 0);

	name_descopename (sname);
#endif
	return 1;
}
/*}}}*/
/*{{{  static int guppy_typecheck_fcndef (compops_t *cops, tnode_t *node, typecheck_t *tc)*/
/*
 *	called to do type-checking on a procedure/function definition
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_typecheck_fcndef (compops_t *cops, tnode_t *node, typecheck_t *tc)
{
	guppy_typecheck_t *gtc = (guppy_typecheck_t *)tc->hook;
	tnode_t *saved;

	if (!gtc) {
		nocc_internal ("guppy_typecheck_fcndef(): missing guppy-specific state in typecheck!");
		return 0;
	}

	typecheck_subtree (tnode_nthsubof (node, 0), tc);
	typecheck_subtree (tnode_nthsubof (node, 1), tc);
	typecheck_subtree (tnode_nthsubof (node, 3), tc);

	saved = gtc->encfcn;
	gtc->encfcn = node;
	gtc->encfcnrtype = tnode_nthsubof (node, 3);

	/* do type-checks on body */
	typecheck_subtree (tnode_nthsubof (node, 2), tc);

	gtc->encfcn = saved;
	return 0;
}
/*}}}*/
/*{{{  static int guppy_fetrans_fcndef (compops_t *cops, tnode_t **nodep, fetrans_t *fe)*/
/*
 *	does front-end transformations for a function definition
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_fetrans_fcndef (compops_t *cops, tnode_t **nodep, fetrans_t *fe)
{
	guppy_fetrans_t *gfe = (guppy_fetrans_t *)fe->langpriv;
	guppy_fcndefhook_t *fdh = (guppy_fcndefhook_t *)tnode_nthhookof (*nodep, 0);
	tnode_t *node = *nodep;

#if 0
fhandle_printf (FHAN_STDERR, "guppy_fetrans_fcndef(): here 1, nodep=0x%8.8x, *nodep=0x%8.8x\n", (unsigned int)nodep, (unsigned int)(*nodep));
#endif
	if (!fdh || !gfe) {
		nocc_internal ("guppy_fetrans_fcndef(): missing stuff!");
		return 0;
	}

	if (node->tag == gup.tag_FCNDEF) {
		chook_t *deschook = tnode_lookupchookbyname ("fetrans:descriptor");
		char *dstr = NULL;

		if (!deschook) {
			return 1;
		}

		langops_getdescriptor (node, &dstr);
		if (dstr) {
			tnode_setchook (node, deschook, (void *)dstr);
		}
	}

	/* do fetrans on names and paramters (results absorbed by this point) */
	fetrans_subtree (tnode_nthsubaddr (node, 0), fe);
	fetrans_subtree (tnode_nthsubaddr (node, 1), fe);

	/* do fetrans on process body */
	fetrans_subtree (tnode_nthsubaddr (node, 2), fe);

	/* if there are parallel sub-processes inside the body, these will have been pulled out into new PFCNDEF's by this point */
#if 0
fhandle_printf (FHAN_STDERR, "guppy_fetrans_fcndef(): considering [%s]...\n", (*nodep)->tag->name);
#endif
	if (node->tag == gup.tag_FCNDEF) {
		/* if at the top-level and public, make a process abstraction */
		char *fname = NameNameOf (tnode_nthnameof (tnode_nthsubof (node, 0), 0));

#if 0
fhandle_printf (FHAN_STDERR, "guppy_fetrans_fcndef(): FCNDEF, lexlevel=%d, ispublic=%d, istoplevel=%d, ispar=%d\n",
		fdh->lexlevel, fdh->ispublic, fdh->istoplevel, fdh->ispar);
#endif
		if (((fdh->lexlevel == 0) && (fdh->ispublic || fdh->istoplevel)) || fdh->ispar) {
			tnode_t *newdef, *newparams, *newbody, *newname, *iplist;
			name_t *curname, *newpfname;
			tnode_t **params;
			int i, nparams;
			guppy_fcndefhook_t *newfdh;

#if 0
fhandle_printf (FHAN_STDERR, "guppy_fetrans_fcndef(): want to make PFCNDEF version of [%s]..\n", fname);
#endif
			newparams = parser_newlistnode (NULL);
			iplist = parser_newlistnode (NULL);
			params = parser_getlistitems (tnode_nthsubof (node, 1), &nparams);
			for (i=0; i<nparams; i++) {
				/* recreate parameters -- minus initialisers */
				tnode_t *nparm, *optype, *nptype, *opname, *npname;
				name_t *opnname, *npnname;

				opname = tnode_nthsubof (params[i], 0);
				opnname = tnode_nthnameof (opname, 0);
				optype = tnode_nthsubof (params[i], 1);

				nptype = tnode_copytree (optype);
				npnname = name_addname (NameNameOf (opnname), NULL, nptype, NULL);
				npname = tnode_createfrom (opname->tag, opname, npnname);
				SetNameNode (npnname, npname);

				if (params[i]->tag == gup.tag_FPARAM) {
					nparm = tnode_createfrom (gup.tag_FPARAM, params[i], npname, nptype, NULL);
				} else {
					nocc_internal ("guppy_fetrans_fcndef(): unexpected parameter type [%s:%s] while generating proc abstraction",
							params[i]->tag->ndef->name, params[i]->tag->name);
					return 0;
				}
				SetNameDecl (npnname, nparm);

				parser_addtolist (newparams, nparm);
				parser_addtolist (iplist, npname);		/* put in instance list */
			}
			newbody = tnode_createfrom (gup.tag_INSTANCE, NULL, tnode_nthsubof (node, 0), iplist);
			// newbody = tnode_createfrom (gup.tag_SKIP, tnode_nthsubof (node, 2));
			curname = tnode_nthnameof (tnode_nthsubof (node, 0), 0);
			newpfname = name_addname (NameNameOf (curname), NULL, newparams, NULL);
			newname = tnode_createfrom (gup.tag_NPFCNDEF, tnode_nthsubof (node, 0), newpfname);
			SetNameNode (newpfname, newname);

			newfdh = guppy_newfcndefhook ();
			newfdh->lexlevel = fdh->lexlevel;
			newfdh->ispar = fdh->ispar;
			newfdh->ispublic = fdh->ispublic;
			newfdh->istoplevel = fdh->istoplevel;
			newfdh->pfcndef = NULL;

			newdef = tnode_createfrom (gup.tag_PFCNDEF, node, newname, newparams, newbody, NULL, newfdh);
			SetNameDecl (newpfname, newdef);

			fdh->pfcndef = newdef;
#if 0
fhandle_printf (FHAN_STDERR, "guppy_fetrans_fcndef(): created new definition, want to insert into list (cur insidx=%d):\n", gfe->insidx);
tnode_dumptree (newdef, 1, FHAN_STDERR);
#endif
			if (!gfe->postinslist) {
				gfe->postinslist = parser_newlistnode (SLOCI);
			}
			parser_addtolist (gfe->postinslist, newdef);
		}
	}

	return 0;
}
/*}}}*/
/*{{{  static int guppy_fetrans1_fcndef (compops_t *cops, tnode_t **nodep, guppy_fetrans1_t *fe1)*/
/*
 *	fetrans1 for function definition (parameterise results)
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_fetrans1_fcndef (compops_t *cops, tnode_t **nodep, guppy_fetrans1_t *fe1)
{
	tnode_t *fcnname = tnode_nthsubof (*nodep, 0);
	tnode_t **pptr = tnode_nthsubaddr (*nodep, 1);
	tnode_t **rptr = tnode_nthsubaddr (*nodep, 3);
	tnode_t **ritems;
	int nritems, i;
	guppy_fetrans1_t *fe1b;

#if 0
fhandle_printf (FHAN_STDERR, "guppy_fetrans1_fcndef(): results are:\n");
tnode_dumptree (*rptr, 1, FHAN_STDERR);
#endif
	if (!*rptr) {
		/* nothing to do, but still need to put insertpoint on body */
		fe1b = guppy_newfetrans1 ();
		guppy_fetrans1_subtree_newtemps (tnode_nthsubaddr (*nodep, 2), fe1b);
		fe1->error += fe1b->error;
		guppy_freefetrans1 (fe1b);

		return 0;
	}
	if (!parser_islistnode (*rptr)) {
		tnode_error (*nodep, "function result is not a list");
		return 1;
	}
	if (!parser_islistnode (*pptr)) {
		tnode_error (*nodep, "function parameters are not a list");
		return 1;
	}

	fe1b = guppy_newfetrans1 ();

	ritems = parser_getlistitems (*rptr, &nritems);
	for (i=0; i<nritems; i++) {
		/* create a new parameter */
		tnode_t *nparam, *nname;
		name_t *tmpname;
		char *pname = guppy_maketempname (ritems[i]);

		tmpname = name_addname (pname, NULL, ritems[i], NULL);
		nname = tnode_createfrom (gup.tag_NRESPARAM, fcnname, tmpname);
		SetNameNode (tmpname, nname);
		nparam = tnode_createfrom (gup.tag_FPARAM, fcnname, nname, ritems[i], NULL);
		SetNameDecl (tmpname, nparam);

		/* put in parameter list and local fetrans1 struct for return processing */
		parser_insertinlist (*pptr, nparam, i);
		dynarray_add (fe1b->rnames, nname);
	}

	guppy_fetrans1_subtree_newtemps (tnode_nthsubaddr (*nodep, 2), fe1b);

	fe1->error += fe1b->error;
	guppy_freefetrans1 (fe1b);

	return 0;
}
/*}}}*/
/*{{{  static int guppy_fetrans15_fcndef (compops_t *cops, tnode_t **nodep, guppy_fetrans15_t *fe15)*/
/*
 *	fetrans1.5 for function definition (walk through, check body)
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_fetrans15_fcndef (compops_t *cops, tnode_t **nodep, guppy_fetrans15_t *fe15)
{
	tnode_t **bodyp = tnode_nthsubaddr (*nodep, 2);
	int saved = fe15->expt_proc;

	fe15->expt_proc = 1;		/* body should be a process */
	guppy_fetrans15_subtree (bodyp, fe15);

	fe15->expt_proc = saved;
	return 0;
}
/*}}}*/
/*{{{  static int guppy_namemap_fcndef (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	called to name-map a function/procedure definition
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_namemap_fcndef (compops_t *cops, tnode_t **nodep, map_t *map)
{
	tnode_t **bodyp = tnode_nthsubaddr (*nodep, 2);
	tnode_t **paramsptr = tnode_nthsubaddr (*nodep, 1);
	tnode_t *blk;
	tnode_t *statics, *wptr, *saved_wptr, *mappedwptr;
	cccsp_mapdata_t *cmd = (cccsp_mapdata_t *)map->hook;
	tnode_t *name = tnode_nthsubof (*nodep, 0);

	if ((*nodep)->tag == gup.tag_PFCNDEF) {
		statics = *paramsptr;
	} else {
		statics = NULL;
	}
	blk = map->target->newblock (*bodyp, map, statics, map->lexlevel + 1);
	*bodyp = blk;
	bodyp = tnode_nthsubaddr (blk, 0);				/* body now here */

	map_pushlexlevel (map, blk, paramsptr);

	/* map parameters */
	map->inparamlist = 1;
	map_submapnames (paramsptr, map);
	if (tnode_hascompop ((*nodep)->tag->ndef->ops, "inparams_namemap")) {
		tnode_callcompop ((*nodep)->tag->ndef->ops, "inparams_namemap", 2, nodep, map);
	}
	map->inparamlist = 0;

	/* add workspace parameter to front-of-list and remember whilst we map body */
	wptr = cccsp_create_wptr (OrgOf (*nodep), map->target);
	saved_wptr = cmd->process_id;
	cmd->process_id = wptr;
	mappedwptr = wptr;
	map_submapnames (&mappedwptr, map);					/* map what will be the declaration in parameters */

	if ((*nodep)->tag == gup.tag_PFCNDEF) {
		int nparams, i;
		tnode_t **mparams;

		/* if we need to attach initialisers to parameters -- do here */
		mparams = parser_getlistitems (*paramsptr, &nparams);
		for (i=0; i<nparams; i++) {
			tnode_t *orig = tnode_nthsubof (mparams[i], 0);		/* original N_PARAM */
			tnode_t *mappedid, *init;
			int indir;

			mappedid = cmd->process_id;
			map_submapnames (&mappedid, map);
#if 0
fhandle_printf (FHAN_STDERR, "guppy_namemap_fcndef(): here, mappedid =\n");
tnode_dumptree (mappedid, 1, FHAN_STDERR);
fhandle_printf (FHAN_STDERR, "guppy_namemap_fcndef(): here 2, mparams[i] =\n");
tnode_dumptree (mparams[i], 1, FHAN_STDERR);
#endif
			indir = cccsp_get_indir (mparams[i], map->target);
			init = tnode_createfrom (gup.tag_FPARAMINIT, orig, mappedid, constprop_newconst (CONST_INT, NULL, NULL, i),
						NameTypeOf (tnode_nthnameof (orig, 0)), constprop_newconst (CONST_INT, NULL, NULL, indir));

			cccsp_set_initialiser (mparams[i], init);
		}

		/* unplug parameters */
		*paramsptr = NULL;
	}

	/* add mapped wptr (parameter) to parameter list */
	if (!*paramsptr) {
		*paramsptr = parser_newlistnode (OrgOf (*nodep));
	}
	parser_addtolist_front (*paramsptr, mappedwptr);

	map_submapnames (bodyp, map);
	map_poplexlevel (map);
	cmd->process_id = saved_wptr;

	tnode_setnthsub (*nodep, 2, blk);				/* insert back-end BLOCK before process body */

	// tnode_setchook (name, map->mapchook, (void *)blk);
#if 0
fprintf (stderr, "guppy_namemap_fcndef(): here!\n");
#endif
	return 0;
}
/*}}}*/
/*{{{  static int guppy_codegen_fcndef (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	generates code for a function/procedure definition
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_codegen_fcndef (compops_t *cops, tnode_t *node, codegen_t *cgen)
{
	tnode_t *body = tnode_nthsubof (node, 2);
	tnode_t *name = tnode_nthsubof (node, 0);
	name_t *pname;
	tnode_t *params;

#if 0
fhandle_printf (FHAN_STDERR, "guppy_codegen_fcndef(): here! [%s]  params =\n", node->tag->name);
tnode_dumptree (tnode_nthsubof (node, 1), 1, FHAN_STDERR);
#endif
	pname = tnode_nthnameof (name, 0);
	params = tnode_nthsubof (node, 1);

	codegen_callops (cgen, comment, "define %s", pname->me->name);

	codegen_callops (cgen, c_procentry, pname, params, (node->tag == gup.tag_PFCNDEF));

	if ((node->tag == gup.tag_PFCNDEF) && !compopts.notmainmodule) {
		/* last instanceable process (called in source order) */
		cccsp_set_toplevelname (pname, cgen->target);
	}

	/* then code the body */
	codegen_subcodegen (body, cgen);

	return 0;
}
/*}}}*/
/*{{{  static int guppy_cccspdcg_fcndef (compops_t *cops, tnode_t *node, cccsp_dcg_t *dcg)*/
/*
 *	used when generating the direct-call-graph for stack allocation
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_cccspdcg_fcndef (compops_t *cops, tnode_t *node, cccsp_dcg_t *dcg)
{
	cccsp_sfi_entry_t *sfient;
	tnode_t *name = tnode_nthsubof (node, 0);
	name_t *pname;

	pname = tnode_nthnameof (name, 0);
	sfient = cccsp_sfiofname (pname, (node->tag == gup.tag_PFCNDEF));

#if 0
fhandle_printf (FHAN_STDERR, "guppy_cccspdcg_fcndef(): here! (sfient->name=[%s])\n", sfient ? sfient->name : "(null)");
#endif
	dcg->thisfcn = sfient;
	cccsp_cccspdcg_subtree (tnode_nthsubof (node, 2), dcg);
	dcg->thisfcn = NULL;

	return 0;
}
/*}}}*/
/*{{{  static int guppy_cccspdcgfix_fcndef (compops_t *cops, tnode_t *node)*/
/*
 *	used when fixing-up stack usage information
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_cccspdcgfix_fcndef (compops_t *cops, tnode_t *node)
{
	cccsp_sfi_entry_t *sfient;
	tnode_t *name = tnode_nthsubof (node, 0);
	name_t *pname;

	pname = tnode_nthnameof (name, 0);
	sfient = cccsp_sfiofname (pname, (node->tag == gup.tag_PFCNDEF));

	/* Note: link directly, so we can keep the children linkage still -- not a problem, as the SFI table is never dropped */
	tnode_setchook (node, cccsp_sfi_entrychook, sfient);
	return 0;
}
/*}}}*/
/*{{{  static int guppy_reallocate_fcndef (compops_t *cops, tnode_t *node, cccsp_reallocate_t *cra)*/
/*
 *	used when reallocating stack usage
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_reallocate_fcndef (compops_t *cops, tnode_t *node, cccsp_reallocate_t *cra)
{
	cccsp_sfi_entry_t *me = (cccsp_sfi_entry_t *)tnode_getchook (node, cccsp_sfi_entrychook);
	tnode_t *blk = tnode_nthsubof (node, 2);
	int nwords;
	int i, wdiff;

	if (blk->tag != cra->target->tag_BLOCK) {
		nocc_internal ("guppy_reallocate_fcndef(): expected back-end BLOCK inside definition, but found [%s:%s]",
				blk->tag->ndef->name, blk->tag->name);
		return 0;
	}

	cra->maxpar = 0;
	me->parfixup = 0;
	cccsp_reallocate_subtree (blk, cra);

	/* cra->maxpar should be right, make sure we have enough space for all of them */
	me->framesize += (cra->maxpar * 4);
	me->allocsize += (cra->maxpar * 4);

#if 0
fhandle_printf (FHAN_STDERR, "guppy_reallocate_fcndef(): here! my-sfi [%s], framesize=%d, allocsize=%d, cra->maxpar=%d\n",
		me->name, me->framesize, me->allocsize, cra->maxpar);
#endif
	cra->maxpar = 0;
	me->parfixup = 1;

	/* difference between framesize and allocsize should handle all child calls */
	wdiff = me->allocsize - me->framesize;
	for (i=0; i<DA_CUR (me->children); i++) {
		cccsp_sfi_entry_t *called = DA_NTHITEM (me->children, i);

		if (called->allocsize > wdiff) {
			/* need some more space */
			me->allocsize += (called->allocsize - wdiff);
			wdiff = me->allocsize - me->framesize;
		}
	}

	nwords = me->allocsize >> 2;
	if (me->allocsize & 0x03) {
		nwords++;
	}


	cccsp_setblockspace (blk, NULL, &nwords);

#if 1
fhandle_printf (FHAN_STDERR, "guppy_reallocate_fcndef(): here! done with [%s], nwords = %d\n", me->name, nwords);
#endif
	return 0;
}
/*}}}*/

/*{{{  static int guppy_getdescriptor_fcndef (langops_t *lops, tnode_t *node, char **str)*/
/*
 *	generates a descriptor line for a procedure/function definition
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_getdescriptor_fcndef (langops_t *lops, tnode_t *node, char **str)
{
	tnode_t *name = tnode_nthsubof (node, 0);
	char *realname;
	tnode_t *params = tnode_nthsubof (node, 1);
	tnode_t *results = tnode_nthsubof (node, 3);

	if (*str) {
		nocc_warning ("guppy_getdescriptor_fcndef(): already had descriptor [%s]", *str);
		sfree (*str);
	}
	realname = NameNameOf (tnode_nthnameof (name, 0));
	*str = (char *)smalloc (strlen (realname) + 16);

	sprintf (*str, "define %s (", realname);
	if (parser_islistnode (params)) {
		int nitems, i;
		tnode_t **items = parser_getlistitems (params, &nitems);

		for (i=0; i<nitems; i++) {
			tnode_t *param = items[i];

			langops_getdescriptor (param, str);
			if (i < (nitems - 1)) {
				char *newstr = (char *)smalloc (strlen (*str) + 5);

				sprintf (newstr, "%s, ", *str);
				sfree (*str);
				*str = newstr;
			}
		}
	} else {
		langops_getdescriptor (params, str);
	}

	{
		char *newstr = (char *)smalloc (strlen (*str) + 5);

		sprintf (newstr, "%s)", *str);
		sfree (*str);
		*str = newstr;
	}

	if (results && parser_islistnode (results)) {
		int nitems, i;
		tnode_t **items = parser_getlistitems (results, &nitems);
		
		if (nitems > 0) {
			/* at least one thing! */
			char *newstr = (char *)smalloc (strlen (*str) + 6);

			sprintf (newstr, "%s -> ", *str);
			sfree (*str);
			*str = newstr;

			for (i=0; i<nitems; i++) {
				tnode_t *rtype = items[i];

				langops_getdescriptor (rtype, str);
				if (i < (nitems - 1)) {
					char *newstr = (char *)smalloc (strlen (*str) + 5);

					sprintf (newstr, "%s, ", *str);
					sfree (*str);
					*str = newstr;
				}
			}
		}
	}
	return 0;
}
/*}}}*/
/*{{{  static int guppy_dousagecheck_fcndef (langops_t *lops, tnode_t *node, uchk_state_t *ucs)*/
/*
 *	does usage-checking on a function definition
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_dousagecheck_fcndef (langops_t *lops, tnode_t *node, uchk_state_t *ucs)
{
	/* just walk the body of this */
	usagecheck_begin_branches (node, ucs);
	usagecheck_newbranch (ucs);

	usagecheck_subtree (tnode_nthsubof (node, 2), ucs);

	usagecheck_endbranch (ucs);
	usagecheck_end_branches (node, ucs);

	return 0;
}
/*}}}*/


/*{{{  static int guppy_prescope_extdef (compops_t *cops, tnode_t **nodep, prescope_t *ps)*/
/*
 *	does pre-scope on an external definition node -- folds up negative on stack-size if present
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_prescope_extdef (compops_t *cops, tnode_t **nodep, prescope_t *ps)
{
	tnode_t **sizep = tnode_nthsubaddr (*nodep, 1);

	/* prescope the function bit of it */
	prescope_subtree (tnode_nthsubaddr (*nodep, 0), ps);
	if (*sizep && ((*sizep)->tag == gup.tag_NEG)) {
		/* negative literal: means figure-it-out */
		*sizep = guppy_makeintlit (NULL, *sizep, -1);
	}

	return 0;
}
/*}}}*/
/*{{{  static int guppy_fetrans_extdef (compops_t *cops, tnode_t **nodep, fetrans_t *fe)*/
/*
 *	wrapper for fetrans on external declarations: creates appropriate PFCNDEF thing
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_fetrans_extdef (compops_t *cops, tnode_t **nodep, fetrans_t *fe)
{
	tnode_t *fcndef = tnode_nthsubof (*nodep, 0);
	guppy_fetrans_t *gfe = (guppy_fetrans_t *)fe->langpriv;

	/* do fetrans on names and parameters */
	fetrans_subtree (tnode_nthsubaddr (fcndef, 0), fe);
	fetrans_subtree (tnode_nthsubaddr (fcndef, 1), fe);

	if ((fcndef->tag == gup.tag_FCNDEF) && !tnode_nthsubof (fcndef, 3)) {
		/* from an external declaration, so create the PFCNDEF assuming we have it, and only if result-less */
		guppy_fcndefhook_t *fdh = (guppy_fcndefhook_t *)tnode_nthhookof (fcndef, 0);
		guppy_fcndefhook_t *newfdh;
		tnode_t *newdef, *newparams, *newname, *newextdecl;
		name_t *curname, *newpfname;
		tnode_t **params;
		int i, nparams;

		newparams = parser_newlistnode (OrgOf (fcndef));
		params = parser_getlistitems (tnode_nthsubof (fcndef, 1), &nparams);
		for (i=0; i<nparams; i++) {
			/* recreate parameters, minus initialisers */
			tnode_t *nparm, *optype, *nptype, *opname, *npname;
			name_t *opnname, *npnname;

			opname = tnode_nthsubof (params[i], 0);
			opnname = tnode_nthnameof (opname, 0);
			optype = tnode_nthsubof (params[i], 1);

			nptype = tnode_copytree (optype);
			npnname = name_addname (NameNameOf (opnname), NULL, nptype, NULL);
			npname = tnode_createfrom (opname->tag, opname, npnname);
			SetNameNode (npnname, npname);

			if (params[i]->tag == gup.tag_FPARAM) {
				nparm = tnode_createfrom (gup.tag_FPARAM, params[i], npname, nptype, NULL);
			} else {
				nocc_internal ("guppy_fetrans_extdef(): unexpected parameter type [%s:%s] while generating proc abstraction",
						params[i]->tag->ndef->name, params[i]->tag->name);
				return 0;
			}
			SetNameDecl (npnname, nparm);

			parser_addtolist (newparams, nparm);
		}

		curname = tnode_nthnameof (tnode_nthsubof (fcndef, 0), 0);
		newpfname = name_addname (NameNameOf (curname), NULL, newparams, NULL);
		newname = tnode_createfrom (gup.tag_NPFCNDEF, tnode_nthsubof (fcndef, 0), newpfname);
		SetNameNode (newpfname, newname);

		newfdh = guppy_newfcndefhook ();
		newfdh->lexlevel = fdh->lexlevel;
		newfdh->ispar = fdh->ispar;
		newfdh->ispublic = fdh->ispublic;
		newfdh->istoplevel = fdh->istoplevel;
		newfdh->pfcndef = NULL;

		newdef = tnode_createfrom (gup.tag_PFCNDEF, fcndef, newname, newparams, NULL, NULL, newfdh);
		SetNameDecl (newpfname, newdef);

		fdh->pfcndef = newdef;

		/* create a new EXTDECL for this and link into the tree */
		newextdecl = tnode_create (gup.tag_EXTDECL, SLOCI, newdef, tnode_copytree (tnode_nthsubof (*nodep, 1)));
		if (!gfe->postinslist) {
			gfe->postinslist = parser_newlistnode (SLOCI);
		}
		parser_addtolist (gfe->postinslist, newextdecl);
	}

	return 0;
}
/*}}}*/
/*{{{  static int guppy_fetrans1_extdef (compops_t *cops, tnode_t **nodep, guppy_fetrans1_t *fe1)*/
/*
 *	wrapper for fetrans1 on external declarations
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_fetrans1_extdef (compops_t *cops, tnode_t **nodep, guppy_fetrans1_t *fe1)
{
	tnode_t *fcndef = tnode_nthsubof (*nodep, 0);
	tnode_t *fcnname = tnode_nthsubof (fcndef, 0);
	tnode_t **pptr = tnode_nthsubaddr (fcndef, 1);
	tnode_t **rptr = tnode_nthsubaddr (fcndef, 3);
	tnode_t **ritems;
	int nritems, i;

	if (!*rptr) {
		/* nothing to do */
		return 0;
	}
	if (!parser_islistnode (*rptr)) {
		tnode_error (*nodep, "function result is not a list");
		return 0;
	}
	if (!parser_islistnode (*pptr)) {
		tnode_error (*nodep, "function parameters are not a list");
		return 0;
	}

	ritems = parser_getlistitems (*rptr, &nritems);
	for (i=0; i<nritems; i++) {
		/* create new parameter */
		tnode_t *nparam, *nname;
		name_t *tmpname;
		char *pname = guppy_maketempname (ritems[i]);

		tmpname = name_addname (pname, NULL, ritems[i], NULL);
		nname = tnode_createfrom (gup.tag_NRESPARAM, fcnname, tmpname);
		SetNameNode (tmpname, nname);
		nparam = tnode_createfrom (gup.tag_FPARAM, fcnname, nname, ritems[i], NULL);
		SetNameDecl (tmpname, nparam);

		/* put in parameter list */
		parser_insertinlist (*pptr, nparam, i);
	}

	return 0;
}
/*}}}*/
/*{{{  static int guppy_fetrans15_extdef (compops_t *cops, tnode_t **nodep, guppy_fetrans15_t *fe15)*/
/*
 *	fetrans1.5 for external definitions (do nothing, but don't walk inside)
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_fetrans15_extdef (compops_t *cops, tnode_t **nodep, guppy_fetrans15_t *fe15)
{
	return 0;
}
/*}}}*/
/*{{{  static int guppy_namemap_extdef (compops_t *cops, tnode_t **nodep, map_t *map)*/
/*
 *	does name-mapping for an external definition
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_namemap_extdef (compops_t *cops, tnode_t **nodep, map_t *map)
{
	tnode_t *fcndef, *beblk;
	int nparams, nwords;

	fcndef = tnode_nthsubof (*nodep, 0);
	nparams = parser_countlist (tnode_nthsubof (fcndef, 1));
	map_submapnames (tnode_nthsubaddr (*nodep, 0), map);
	fcndef = tnode_nthsubof (*nodep, 0);
	nwords = langops_constvalof (tnode_nthsubof (*nodep, 1), NULL);

#if 0
fhandle_printf (FHAN_STDERR, "guppy_namemap_extdef(): nwords = %d\n", nwords);
#endif
	if (nwords < 0) {
		nwords = 0;
	}
	beblk = tnode_nthsubof (fcndef, 2);
	cccsp_setblockspace (beblk, &nparams, &nwords);

	return 0;
}
/*}}}*/
/*{{{  static int guppy_lpreallocate_extdef (compopts_t *cops, tnode_t *node, cccsp_preallocate_t *cpa)*/
/*
 *	does pre-allocation for an external definition
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_lpreallocate_extdef (compopts_t *cops, tnode_t *node, cccsp_preallocate_t *cpa)
{
	int nwords;
	tnode_t *fcndef, *beblk;

	fcndef = tnode_nthsubof (node, 0);
	cccsp_preallocate_subtree (fcndef, cpa);

	nwords = langops_constvalof (tnode_nthsubof (node, 1), NULL);
#if 0
fhandle_printf (FHAN_STDERR, "guppy_lpreallocate_extdef(): nwords = %d\n", nwords);
#endif
	if (nwords < 0) {
		/* means we don't know yet, but don't leave invalid */
		nwords = 0;
	}
	beblk = tnode_nthsubof (fcndef, 2);
	cccsp_setblockspace (beblk, NULL, &nwords);

	return 0;
}
/*}}}*/
/*{{{  static int guppy_codegen_extdef (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	generates code for an external function/procedure definition
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_codegen_extdef (compops_t *cops, tnode_t *node, codegen_t *cgen)
{
	tnode_t *fcndef = tnode_nthsubof (node, 0);
	tnode_t *name = tnode_nthsubof (fcndef, 0);
	name_t *pname;
	tnode_t *params;

#if 0
fhandle_printf (FHAN_STDERR, "guppy_codegen_fcndef(): here! [%s]  params =\n", node->tag->name);
tnode_dumptree (tnode_nthsubof (node, 1), 1, FHAN_STDERR);
#endif
	pname = tnode_nthnameof (name, 0);
	params = tnode_nthsubof (fcndef, 1);

	codegen_callops (cgen, comment, "external define %s", pname->me->name);

	codegen_callops (cgen, c_procexternal, pname, params, (fcndef->tag == gup.tag_PFCNDEF));

	return 0;
}
/*}}}*/
/*{{{  static int guppy_cccspdcgfix_extdef (compops_t *cops, tnode_t *node)*/
/*
 *	used when fixing-up stack usage information
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_cccspdcgfix_extdef (compops_t *cops, tnode_t *node)
{
	cccsp_sfi_entry_t *sfient;
	tnode_t *efcndef = tnode_nthsubof (node, 0);
	tnode_t *efname = tnode_nthsubof (efcndef, 0);
	name_t *pname;
	int nwords;

	cccsp_cccspdcgfix_subtree (efcndef);

	nwords = langops_constvalof (tnode_nthsubof (node, 1), NULL);

	pname = tnode_nthnameof (efname, 0);
	sfient = cccsp_sfiofname (pname, (efcndef->tag == gup.tag_PFCNDEF));

	if (!sfient) {
		cccsp_sfi_error (node, "guppy_cccspdcgfix_extdef(): no SFI entry for [%s]", NameNameOf (pname));
		return 0;
	}

	if (nwords < 0) {
		/* means we're needing external help */
		if ((sfient->framesize <= 0) && (sfient->allocsize <= 0)) {
			cccsp_sfi_error (node, "cannot determine space requirement for [%s]", sfient->name);
			return 0;
		}
	} else if ((sfient->framesize <= 0) && (sfient->allocsize <= 0)) {
		/* fixup */
		sfient->framesize = nwords;
		sfient->allocsize = nwords;
	}
#if 0
fhandle_printf (FHAN_STDERR, "guppy_cccspdcgfix_extdef(): got SFI [%s]: frame=%d, alloc=%d;  spec=%d\n",
		sfient->name, sfient->framesize, sfient->allocsize, nwords);
#endif

	return 0;
}
/*}}}*/


/*{{{  static int guppy_fcndef_init_nodes (void)*/
/*
 *	sets up procedure declaration nodes for Guppy
 *	returns 0 on success, non-zero on error
 */
static int guppy_fcndef_init_nodes (void)
{
	tndef_t *tnd;
	compops_t *cops;
	langops_t *lops;
	int i;

	/*{{{  guppy:fcndef -- FCNDEF, PFCNDEF*/
	i = -1;
	tnd = tnode_newnodetype ("guppy:fcndef", &i, 4, 0, 1, TNF_LONGDECL);			/* subnodes: name, fparams, body, results;  hooks: guppy_fcndefhook_t */
	tnd->hook_free = guppy_fcndef_hook_free;
	tnd->hook_copy = guppy_fcndef_hook_copy;
	tnd->hook_dumptree = guppy_fcndef_hook_dumptree;
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "prescope", 2, COMPOPTYPE (guppy_prescope_fcndef));
	tnode_setcompop (cops, "declify", 2, COMPOPTYPE (guppy_declify_fcndef));
	tnode_setcompop (cops, "autoseq", 2, COMPOPTYPE (guppy_autoseq_fcndef));
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (guppy_scopein_fcndef));
	tnode_setcompop (cops, "scopeout", 2, COMPOPTYPE (guppy_scopeout_fcndef));
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (guppy_typecheck_fcndef));
	tnode_setcompop (cops, "fetrans", 2, COMPOPTYPE (guppy_fetrans_fcndef));
	tnode_setcompop (cops, "fetrans1", 2, COMPOPTYPE (guppy_fetrans1_fcndef));
	tnode_setcompop (cops, "fetrans15", 2, COMPOPTYPE (guppy_fetrans15_fcndef));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (guppy_namemap_fcndef));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (guppy_codegen_fcndef));
	tnode_setcompop (cops, "cccsp:dcg", 2, COMPOPTYPE (guppy_cccspdcg_fcndef));
	tnode_setcompop (cops, "cccsp:dcgfix", 1, COMPOPTYPE (guppy_cccspdcgfix_fcndef));
	tnode_setcompop (cops, "reallocate", 2, COMPOPTYPE (guppy_reallocate_fcndef));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "getdescriptor", 2, LANGOPTYPE (guppy_getdescriptor_fcndef));
	tnode_setlangop (lops, "do_usagecheck", 2, LANGOPTYPE (guppy_dousagecheck_fcndef));
	tnd->lops = lops;

	i = -1;
	gup.tag_FCNDEF = tnode_newnodetag ("FCNDEF", &i, tnd, NTF_INDENTED_PROC_LIST);
	i = -1;
	gup.tag_PFCNDEF = tnode_newnodetag ("PFCNDEF", &i, tnd, NTF_INDENTED_PROC_LIST);

	/*}}}*/
	/*{{{  guppy:extdef -- EXTDECL, LIBDECL*/
	i = -1;
	tnd = tnode_newnodetype ("guppy:extdef", &i, 2, 0, 0, TNF_NONE);			/* subnodes: definition, stack-words */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "prescope", 2, COMPOPTYPE (guppy_prescope_extdef));
	tnode_setcompop (cops, "fetrans", 2, COMPOPTYPE (guppy_fetrans_extdef));
	tnode_setcompop (cops, "fetrans1", 2, COMPOPTYPE (guppy_fetrans1_extdef));
	tnode_setcompop (cops, "fetrans15", 2, COMPOPTYPE (guppy_fetrans15_extdef));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (guppy_namemap_extdef));
	tnode_setcompop (cops, "lpreallocate", 2, COMPOPTYPE (guppy_lpreallocate_extdef));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (guppy_codegen_extdef));
	tnode_setcompop (cops, "cccsp:dcgfix", 1, COMPOPTYPE (guppy_cccspdcgfix_extdef));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnd->lops = lops;

	i = -1;
	gup.tag_EXTDECL = tnode_newnodetag ("EXTDECL", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_LIBDECL = tnode_newnodetag ("LIBDECL", &i, tnd, NTF_NONE);

	/*}}}*/

	/*{{{  compiler operations for handling scoping and other things associated with parameters*/
	if (tnode_newcompop ("inparams_scopein", COPS_INVALID, 2, INTERNAL_ORIGIN) < 0) {
		nocc_error ("guppy_decl_init_nodes(): failed to create inparams_scopein compiler operation");
		return -1;
	}
	inparams_scopein_compop = tnode_findcompop ("inparams_scopein");

	if (tnode_newcompop ("inparams_scopeout", COPS_INVALID, 2, INTERNAL_ORIGIN) < 0) {
		nocc_error ("guppy_decl_init_nodes(): failed to create inparams_scopeout compiler operation");
		return -1;
	}
	inparams_scopeout_compop = tnode_findcompop ("inparams_scopeout");

	if (tnode_newcompop ("inparams_namemap", COPS_INVALID, 2, INTERNAL_ORIGIN) < 0) {
		nocc_error ("guppy_decl_init_nodes(): failed to create inparams_namemap compiler operation");
		return -1;
	}
	inparams_namemap_compop = tnode_findcompop ("inparams_namemap");

	if (tnode_newcompop ("inparams_lnamemap", COPS_INVALID, 2, INTERNAL_ORIGIN) < 0) {
		nocc_error ("guppy_decl_init_nodes(): failed to create inparams_lnamemap compiler operation");
		return -1;
	}
	inparams_lnamemap_compop = tnode_findcompop ("inparams_lnamemap");

	if (!inparams_scopein_compop || !inparams_scopeout_compop) {
		nocc_error ("guppy_decl_init_nodes(): failed to find inparams scoping compiler operations");
		return -1;
	}

	/*}}}*/

	return 0;
}
/*}}}*/
/*{{{  static int guppy_fcndef_post_setup (void)*/
/*
 *	does post-setup for procedure declaration nodes
 *	returns 0 on success, non-zero on failure
 */
static int guppy_fcndef_post_setup (void)
{
	return 0;
}
/*}}}*/


/*{{{  guppy_fcndef_feunit (feunit_t)*/
feunit_t guppy_fcndef_feunit = {
	.init_nodes = guppy_fcndef_init_nodes,
	.reg_reducers = NULL,
	.init_dfatrans = NULL,
	.post_setup = guppy_fcndef_post_setup,
	.ident = "guppy-fcndef"
};

/*}}}*/

