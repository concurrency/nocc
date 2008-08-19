/*
 *	occampi_procdecl.c -- occam-pi PROC declarations for NOCC
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
#include "origin.h"
#include "symbols.h"
#include "keywords.h"
#include "lexer.h"
#include "lexpriv.h"
#include "tnode.h"
#include "parser.h"
#include "dfa.h"
#include "dfaerror.h"
#include "parsepriv.h"
#include "occampi.h"
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


/*}}}*/
/*{{{  private data*/

static compop_t *inparams_scopein_compop = NULL;
static compop_t *inparams_scopeout_compop = NULL;
static compop_t *inparams_namemap_compop = NULL;


/*}}}*/


/*{{{  static int occampi_prescope_procdecl (compops_t *cops, tnode_t **node, prescope_t *ps)*/
/*
 *	called to prescope a PROC definition
 */
static int occampi_prescope_procdecl (compops_t *cops, tnode_t **node, prescope_t *ps)
{
	occampi_prescope_t *ops = (occampi_prescope_t *)(ps->hook);
	char *rawname = (char *)tnode_nthhookof (tnode_nthsubof (*node, 0), 0);

#if 0
nocc_message ("occampi_prescope_procdecl(): rawname = \"%s\", ops->procdepth = %d", rawname, ops->procdepth);
#endif
	if (!ops->procdepth) {
		int x;

		x = library_makepublic (node, rawname);
		if (x) {
			return 1;			/* go round again */
		}
	} else {
		library_makeprivate (node, rawname);
		/* continue processing */
	}


	ops->last_type = NULL;
	if (!tnode_nthsubof (*node, 1)) {
		/* no parameters, create empty list */
		tnode_setnthsub (*node, 1, parser_newlistnode (NULL));
	} else if (tnode_nthsubof (*node, 1) && !parser_islistnode (tnode_nthsubof (*node, 1))) {
		/* turn single parameter into a list-node */
		tnode_t *list = parser_newlistnode (NULL);

		parser_addtolist (list, tnode_nthsubof (*node, 1));
		tnode_setnthsub (*node, 1, list);
	}

	/* prescope params */
	prescope_subtree (tnode_nthsubaddr (*node, 1), ps);

	/* do a prescope on the body, at a higher procdepth */
	ops->procdepth++;
	prescope_subtree (tnode_nthsubaddr (*node, 2), ps);
	ops->procdepth--;

	/* prescope in-scope process */
	prescope_subtree (tnode_nthsubaddr (*node, 3), ps);

	return 0;				/* done them all */
}
/*}}}*/
/*{{{  static int occampi_scopein_procdecl (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	called to scope-in a PROC definition
 */
static int occampi_scopein_procdecl (compops_t *cops, tnode_t **node, scope_t *ss)
{
	tnode_t *name = tnode_nthsubof (*node, 0);
	tnode_t **paramsptr = tnode_nthsubaddr (*node, 1);
	tnode_t **bodyptr = tnode_nthsubaddr (*node, 2);
	tnode_t *traces = NULL;
	chook_t *nnschook = library_getnonamespacechook ();
	void *nsmark;
	char *rawname;
	name_t *procname;
	tnode_t *newname;
	tnode_t *nnsnode = NULL;

	nsmark = name_markscope ();

	/*{{{  walk parameters and body*/
	tnode_modprepostwalktree (paramsptr, scope_modprewalktree, scope_modpostwalktree, (void *)ss);

	/* if we have anything attached which needs parameters to be in scope, do that here */
	if (tnode_hascompop (cops, "inparams_scopein")) {
		tnode_callcompop (cops, "inparams_scopein", 2, node, ss);
	}

	tnode_modprepostwalktree (bodyptr, scope_modprewalktree, scope_modpostwalktree, (void *)ss);

	/*}}}*/
	/*{{{  if we have any attached TRACES, walk these too*/
	traces = (tnode_t *)tnode_getchook (*node, opi.chook_traces);
	if (traces) {
#if 0
fprintf (stderr, "occampi_scopein_procdecl(): have traces!\n");
#endif
		/* won't affect TRACES node, so safe to pass local addr */
		tnode_modprepostwalktree (&traces, scope_modprewalktree, scope_modpostwalktree, (void *)ss);
	}
	/*}}}*/

	/* if there is a corresponding scopeout, call that here (before parameters go out of scope!) */
	if (tnode_hascompop (cops, "inparams_scopeout")) {
		tnode_callcompop (cops, "inparams_scopeoutin", 2, node, ss);
	}

	name_markdescope (nsmark);

	if (nnschook && tnode_haschook (*node, nnschook)) {
		nnsnode = (tnode_t *)tnode_getchook (*node, nnschook);
	}

	/* declare and scope PROC name, then check process in the scope of it */
	rawname = tnode_nthhookof (name, 0);
#if 0
fprintf (stderr, "occampi_scopein_procdecl(): scoping name \"%s\", nnsnode = %p\n", rawname, nnsnode);
#endif
	/* if we have a 'nonamespace' marker, make sure it doesn't end up in one */
	procname = name_addscopenamess (rawname, *node, *paramsptr, NULL, nnsnode ? NULL : ss);
	newname = tnode_createfrom (opi.tag_NPROCDEF, name, procname);
	SetNameNode (procname, newname);
	tnode_setnthsub (*node, 0, newname);

	/* free old name, scope process */
	tnode_free (name);
	tnode_modprepostwalktree (tnode_nthsubaddr (*node, 3), scope_modprewalktree, scope_modpostwalktree, (void *)ss);
	ss->scoped++;

	return 0;		/* already walked child nodes */
}
/*}}}*/
/*{{{  static int occampi_scopeout_procdecl (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	called to scope-out a PROC definition
 */
static int occampi_scopeout_procdecl (compops_t *cops, tnode_t **node, scope_t *ss)
{
	tnode_t *name = tnode_nthsubof (*node, 0);
	name_t *sname;

	if (name->tag != opi.tag_NPROCDEF) {
		scope_error (name, ss, "not NPROCDEF!");
		return 0;
	}
	sname = tnode_nthnameof (name, 0);

	name_descopename (sname);

	return 1;
}
/*}}}*/
/*{{{  static tnode_t *occampi_gettype_procdecl (langops_t *lops, tnode_t *node, tnode_t *defaulttype)*/
/*
 *	returns the type of a PROC definition (= parameter list)
 */
static tnode_t *occampi_gettype_procdecl (langops_t *lops, tnode_t *node, tnode_t *defaulttype)
{
	return tnode_nthsubof (node, 1);
}
/*}}}*/
/*{{{  static int occampi_fetrans_procdecl (compops_t *cops, tnode_t **node, fetrans_t *fe)*/
/*
 *	does front-end transforms on a PROC definition
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_fetrans_procdecl (compops_t *cops, tnode_t **node, fetrans_t *fe)
{
	chook_t *deschook = tnode_lookupchookbyname ("fetrans:descriptor");
	char *dstr = NULL;
	tnode_t *params, **plist;
	int i, nparams;

	if (!deschook) {
		return 1;
	}
	langops_getdescriptor (*node, &dstr);
	if (dstr) {
		tnode_setchook (*node, deschook, (void *)dstr);
	}

	/* do fetrans on the name and parameters */
	fetrans_subtree (tnode_nthsubaddr (*node, 0), fe);
	fetrans_subtree (tnode_nthsubaddr (*node, 1), fe);

	params = tnode_nthsubof (*node, 1);
	/* run through parameters and generate/insert hidden parameters */
	plist = parser_getlistitems (params, &nparams);

	for (i=0; i<nparams; i++) {
		tnode_t *param = plist[i];
		tnode_t *hplist = langops_hiddenparamsof (param);

		if (hplist) {
			int j, nhparams;
			tnode_t **hparams = parser_getlistitems (hplist, &nhparams);

			for (j=0; j<nhparams; j++) {
				i++;
				parser_insertinlist (params, hparams[j], i);
				hparams[j] = NULL;
			}
			plist = parser_getlistitems (params, &nparams);
		}
	}

	/* do fetrans on the PROC body and in-scope process */
	fetrans_subtree (tnode_nthsubaddr (*node, 2), fe);
	fetrans_subtree (tnode_nthsubaddr (*node, 3), fe);

	return 0;
}
/*}}}*/
/*{{{  static int occampi_precheck_procdecl (compops_t *cops, tnode_t *node)*/
/*
 *	does pre-checking on PROC declaration
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_precheck_procdecl (compops_t *cops, tnode_t *node)
{
#if 0
fprintf (stderr, "occampi_precheck_procdecl(): here!\n");
#endif
	precheck_subtree (tnode_nthsubof (node, 2));		/* precheck this body */
	precheck_subtree (tnode_nthsubof (node, 3));		/* precheck in-scope code */
#if 0
fprintf (stderr, "occampi_precheck_procdecl(): returning 0\n");
#endif
	return 0;
}
/*}}}*/
/*{{{  static int occampi_usagecheck_procdecl (langops_t *lops, tnode_t *node, uchk_state_t *ucstate)*/
/*
 *	does usage-checking on a PROC declaration
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_usagecheck_procdecl (langops_t *lops, tnode_t *node, uchk_state_t *ucstate)
{
	usagecheck_begin_branches (node, ucstate);
	usagecheck_newbranch (ucstate);
	usagecheck_subtree (tnode_nthsubof (node, 2), ucstate);		/* usage-check this body */
	usagecheck_subtree (tnode_nthsubof (node, 3), ucstate);		/* usage-check in-scope code */
	usagecheck_endbranch (ucstate);
	usagecheck_end_branches (node, ucstate);
	return 0;
}
/*}}}*/
/*{{{  static int occampi_mobilitycheck_procdecl (compops_t *cops, tnode_t *node, mchk_state_t *mcstate)*/
/*
 *	does mobility checking on a procedure declaration
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_mobilitycheck_procdecl (compops_t *cops, tnode_t *node, mchk_state_t *mcstate)
{
	chook_t *tchktrchook = tracescheck_gettraceschook ();
	tchk_traces_t *ptraces = NULL;

	ptraces = (tchk_traces_t *)tnode_getchook (node, tchktrchook);
#if 1
fprintf (stderr, "occampi_mobilitycheck_procdecl(): here!, traces are:\n");
tracescheck_dumptraces (ptraces, 1, stderr);
#endif
	return 1;
}
/*}}}*/
/*{{{  static int occampi_tracescheck_procdecl (compops_t *cops, tnode_t *node, tchk_state_t *tcstate)*/
/*
 *	does traces checking in a procedure declaration
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_tracescheck_procdecl (compops_t *cops, tnode_t *node, tchk_state_t *tcstate)
{
	tchk_state_t *thispstate = tracescheck_pushstate (tcstate);
	chook_t *tchktrchook = tracescheck_gettraceschook ();
	tchk_traces_t *trs = NULL;

	thispstate->inparams = 1;
	tracescheck_subtree (tnode_nthsubof (node, 1), thispstate);
	thispstate->inparams = 0;

	tracescheck_subtree (tnode_nthsubof (node, 2), thispstate);

#if 0
fprintf (stderr, "occampi_tracescheck_procdecl(): after body check, thispstate =\n");
tracescheck_dumpstate (thispstate, 1, stderr);
#endif
	/* anything left in the bucket in thispstate will be a trace for this procedure */
	tracescheck_buckettotraces (thispstate);
	trs = tracescheck_pulltraces (thispstate);

	tracescheck_simplifytraces (trs);
	tnode_setchook (node, tchktrchook, (void *)trs);

#if 0
// fprintf (stderr, "occampi_tracescheck_procdecl(): done traces check, thispstate =\n");
// tracescheck_dumpstate (thispstate, 1, stderr);
fprintf (stderr, "occampi_tracescheck_procdecl(): done traces check, obtained traces=\n");
// tnode_dumptree (node, 1, stderr);
tracescheck_dumptraces (trs, 1, stderr);
#endif

	/* clean references from formal parameters */
	tracescheck_cleanrefchooks (thispstate, tnode_nthsubof (node, 1));
	tracescheck_popstate (thispstate);

	tracescheck_subtree (tnode_nthsubof (node, 3), tcstate);

	return 0;
}
/*}}}*/
/*{{{  static int occampi_miscnodetrans_procdecl (compops_t *cops, tnode_t **tptr, occampi_miscnodetrans_t *mnt)*/
/*
 *	does miscnode transformations for a PROC declaration -- hoists metadata to the PROC
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_miscnodetrans_procdecl (compops_t *cops, tnode_t **tptr, occampi_miscnodetrans_t *mnt)
{
	chook_t *metahook = tnode_lookupchookbyname ("metadata");
	chook_t *metalisthook = tnode_lookupchookbyname ("metadatalist");

	if (mnt->md_node) {
		metadatalist_t *mdl = NULL;
		
		if (tnode_haschook (*tptr, metalisthook)) {
			mdl = (metadatalist_t *)tnode_getchook (*tptr, metalisthook);
		}
		if (!mdl) {
			/* need a fresh one */
			mdl = metadata_newmetadatalist ();
			tnode_setchook (*tptr, metalisthook, (void *)mdl);
		}

		while (mnt->md_node) {
			tnode_t **nextp = tnode_nthsubaddr (mnt->md_node, 0);
			tnode_t *tmp;

			if (tnode_haschook (mnt->md_node, metahook)) {
				metadata_t *mdata = (metadata_t *)tnode_getchook (mnt->md_node, metahook);

				tnode_clearchook (mnt->md_node, metahook);
				if (mdata) {
					dynarray_add (mdl->items, mdata);
				}
			}

			tmp = *nextp;
			*nextp = NULL;
			tnode_free (mnt->md_node);
			mnt->md_node = tmp;
		}

		mnt->md_iptr = NULL;
	}
	return 1;
}
/*}}}*/
/*{{{  static int occampi_betrans_procdecl (compops_t *cops, tnode_t **node, betrans_t *bt)*/
/*
 *	does back-end mapping for a PROC definition -- pulls out nested PROCs
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_betrans_procdecl (compops_t *cops, tnode_t **node, betrans_t *be)
{
	occampi_betrans_t *opibe = (occampi_betrans_t *)be->priv;

	if (!opibe) {
		/* this is a top-level PROC of some kind, put an insertpoint here */
		opibe = (occampi_betrans_t *)smalloc (sizeof (occampi_betrans_t));
		opibe->procdepth = 0;
		opibe->insertpoint = node;
		be->priv = opibe;

		betrans_subtree (tnode_nthsubaddr (*node, 0), be);
		betrans_subtree (tnode_nthsubaddr (*node, 1), be);
		opibe->procdepth = 1;
		betrans_subtree (tnode_nthsubaddr (*node, 2), be);
		opibe->procdepth = 0;

		sfree (opibe);
		be->priv = NULL;

		/* do in-scope body */
		betrans_subtree (tnode_nthsubaddr (*node, 3), be);
	} else {
		/* this is a nested PROC -- move it up to the insertpoint */
		tnode_t *thisproc = *node;
		tnode_t *ibody = tnode_nthsubof (*node, 3);
		tnode_t *ipproc = *(opibe->insertpoint);

		betrans_subtree (tnode_nthsubaddr (*node, 0), be);
		betrans_subtree (tnode_nthsubaddr (*node, 1), be);
		opibe->procdepth++;
		betrans_subtree (tnode_nthsubaddr (*node, 2), be);
		opibe->procdepth--;
		betrans_subtree (tnode_nthsubaddr (*node, 3), be);

		*(opibe->insertpoint) = thisproc;
		tnode_setnthsub (thisproc, 3, ipproc);
		*node = ibody;

	}

	return 0;
}
/*}}}*/
/*{{{  static int occampi_namemap_procdecl (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	name-maps a PROC definition
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_namemap_procdecl (compops_t *cops, tnode_t **node, map_t *map)
{
	tnode_t *blk;
	tnode_t **paramsptr;
	tnode_t *tmpname;

	blk = map->target->newblock (tnode_nthsubof (*node, 2), map, tnode_nthsubof (*node, 1), map->lexlevel + 1);
	map_pushlexlevel (map, blk, tnode_nthsubaddr (*node, 1));

	/* map formal params and body */
	paramsptr = tnode_nthsubaddr (*node, 1);
	map->inparamlist = 1;
#if 0
fprintf (stderr, "occampi_namemap_procdecl(): about to map parameters:\n");
tnode_dumptree (*paramsptr, 1, stderr);
#endif
	map_submapnames (paramsptr, map);
	if (tnode_hascompop ((*node)->tag->ndef->ops, "inparams_namemap")) {
		tnode_callcompop ((*node)->tag->ndef->ops, "inparams_namemap", 2, node, map);
	}

	map->inparamlist = 0;
#if 0
fprintf (stderr, "occampi_namemap_procdecl(): done mapping parameters, got:\n");
tnode_dumptree (*paramsptr, 1, stderr);
#endif
	map_submapnames (tnode_nthsubaddr (blk, 0), map);		/* do this under the back-end block */

	map_poplexlevel (map);

	/* insert the BLOCK node before the body of the process */
	tnode_setnthsub (*node, 2, blk);

	/* map scoped body */
	map_submapnames (tnode_nthsubaddr (*node, 3), map);

	/* add static-link, etc. if required and return-address */
	if (!parser_islistnode (*paramsptr)) {
		tnode_t *flist = parser_newlistnode (NULL);

		parser_addtolist (flist, *paramsptr);
		*paramsptr = flist;
	}

	tmpname = map->target->newname (tnode_create (opi.tag_HIDDENPARAM, NULL, tnode_create (opi.tag_RETURNADDRESS, NULL)), NULL, map,
			map->target->pointersize, 0, 0, 0, map->target->pointersize, 0);
	parser_addtolist_front (*paramsptr, tmpname);

	return 0;
}
/*}}}*/
/*{{{  static int occampi_precode_procdecl (compops_t *cops, tnode_t **nodep, codegen_t *cgen)*/
/*
 *	pre-code for PROC definition, used to determine the last PROC in a file
 *	(entry-point if main module).
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_precode_procdecl (compops_t *cops, tnode_t **nodep, codegen_t *cgen)
{
	tnode_t *node = *nodep;
	tnode_t *name = tnode_nthsubof (node, 0);

	/* walk body */
	codegen_subprecode (tnode_nthsubaddr (node, 2), cgen);

	codegen_precode_seenproc (cgen, tnode_nthnameof (name, 0), node);

	/* pre-code stuff following declaration */
	codegen_subprecode (tnode_nthsubaddr (node, 3), cgen);
	return 0;
}
/*}}}*/
/*{{{  static int occampi_codegen_procdecl (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	generates code for a PROC definition
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_codegen_procdecl (compops_t *cops, tnode_t *node, codegen_t *cgen)
{
	tnode_t *body = tnode_nthsubof (node, 2);
	tnode_t *name = tnode_nthsubof (node, 0);
	int ws_size, vs_size, ms_size;
	int ws_offset, adjust;
	name_t *pname;

	body = tnode_nthsubof (node, 2);
	cgen->target->be_getblocksize (body, &ws_size, &ws_offset, &vs_size, &ms_size, &adjust, NULL);

	pname = tnode_nthnameof (name, 0);
	codegen_callops (cgen, comment, "PROC %s = %d,%d,%d,%d,%d", pname->me->name, ws_size, ws_offset, vs_size, ms_size, adjust);
	codegen_callops (cgen, setwssize, ws_size, adjust);
	codegen_callops (cgen, setvssize, vs_size);
	codegen_callops (cgen, setmssize, ms_size);
	codegen_callops (cgen, setnamelabel, pname);
	codegen_callops (cgen, procnameentry, pname);
	codegen_callops (cgen, debugline, node);

	/* adjust workspace and generate code for body */
	// codegen_callops (cgen, wsadjust, -(ws_offset - adjust));
	codegen_subcodegen (body, cgen);
	// codegen_callops (cgen, wsadjust, (ws_offset - adjust));

	/* return */
	codegen_callops (cgen, procreturn, adjust);

	/* generate code following declaration */
	codegen_subcodegen (tnode_nthsubof (node, 3), cgen);
#if 0
fprintf (stderr, "occampi_codegen_procdecl!\n");
#endif
	return 0;
}
/*}}}*/
/*{{{  static int occampi_getdescriptor_procdecl (langops_t *lops, tnode_t *node, char **str)*/
/*
 *	generates a descriptor line for a PROC definition
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_getdescriptor_procdecl (langops_t *lops, tnode_t *node, char **str)
{
	tnode_t *name = tnode_nthsubof (node, 0);
	char *realname;
	tnode_t *params = tnode_nthsubof (node, 1);

	if (*str) {
		/* shouldn't get this here, but.. */
		nocc_warning ("occampi_getdescriptor_procdecl(): already had descriptor [%s]", *str);
		sfree (*str);
	}
	realname = NameNameOf (tnode_nthnameof (name, 0));
	*str = (char *)smalloc (strlen (realname) + 10);

	sprintf (*str, "PROC %s (", realname);
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
	return 0;
}
/*}}}*/

/*{{{  static int occampi_prescope_fparam (compops_t *cops, tnode_t **node, prescope_t *ps)*/
/*
 *	called to prescope a formal parameter
 */
static int occampi_prescope_fparam (compops_t *cops, tnode_t **node, prescope_t *ps)
{
	occampi_prescope_t *ops = (occampi_prescope_t *)(ps->hook);

#if 0
fprintf (stderr, "occampi_prescope_fparam(): prescoping formal parameter! *node = \n");
tnode_dumptree (*node, 1, stderr);
#endif

	if (tnode_nthsubof (*node, 1)) {
		tnode_t **namep = tnode_nthsubaddr (*node, 0);

		if (ops->last_type) {
			/* this is always a copy.. */
			tnode_free (ops->last_type);
			ops->last_type = NULL;
		}
		ops->last_type = tnode_nthsubof (*node, 1);
		if ((ops->last_type->tag == opi.tag_ASINPUT) || (ops->last_type->tag == opi.tag_ASOUTPUT)) {
			/* lose this from the type, associated primarily with name in FPARAMs */
			ops->last_type = tnode_nthsubof (ops->last_type, 0);
		}

		/* maybe fixup ASINPUT/ASOUTPUT here too */
		if (((*namep)->tag == opi.tag_ASINPUT) || ((*namep)->tag == opi.tag_ASOUTPUT)) {
			tnode_t *name = tnode_nthsubof (*namep, 0);
			tnode_t *type = *namep;

			tnode_setnthsub (type, 0, tnode_nthsubof (*node, 1));
			*namep = name;
			tnode_setnthsub (*node, 1, type);
		}

		ops->last_type = tnode_copytree (ops->last_type);

	} else if (!ops->last_type) {
		prescope_error (*node, ps, "missing type on formal parameter");
	} else {
		/* set type field for formal parameter */
		tnode_t **namep = tnode_nthsubaddr (*node, 0);

#if 0
fprintf (stderr, "occampi_prescope_fparam(): setting type on formal param, last_type = \n");
tnode_dumptree (ops->last_type, 1, stderr);
#endif
		if (((*namep)->tag == opi.tag_ASINPUT) || ((*namep)->tag == opi.tag_ASOUTPUT)) {
			tnode_t *name = tnode_nthsubof (*namep, 0);
			tnode_t *type = *namep;

			*namep = name;
			tnode_setnthsub (type, 0, tnode_copytree (ops->last_type));
			tnode_setnthsub (*node, 1, type);
		} else {
			tnode_setnthsub (*node, 1, tnode_copytree (ops->last_type));
		}
#if 0
fprintf (stderr, "occampi_prescope_fparam(): put in type, *node = \n");
tnode_dumptree (*node, 1, stderr);
#endif
	}
	return 1;
}
/*}}}*/
/*{{{  static int occampi_scopein_fparam (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	called to scope a formal parmeter
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_scopein_fparam (compops_t *cops, tnode_t **node, scope_t *ss)
{
	tnode_t *name = tnode_nthsubof (*node, 0);
	tnode_t **typep = tnode_nthsubaddr (*node, 1);
	char *rawname;
	name_t *sname = NULL;
	tnode_t *newname;

	if (name->tag != opi.tag_NAME) {
		scope_error (name, ss, "name not raw-name!");
		return 0;
	}
	rawname = tnode_nthhookof (name, 0);
#if 0
fprintf (stderr, "occampi_scopein_fparam: here! rawname = \"%s\"\n", rawname);
#endif

	/* scope the type first */
	if (scope_subtree (typep, ss)) {
		return 0;
	}

	sname = name_addscopename (rawname, *node, *typep, NULL);
	newname = tnode_createfrom (((*node)->tag == opi.tag_VALFPARAM) ? opi.tag_NVALPARAM : opi.tag_NPARAM, name, sname);
	SetNameNode (sname, newname);
	tnode_setnthsub (*node, 0, newname);

	/* free the old name */
	tnode_free (name);
	ss->scoped++;

	return 1;
}
/*}}}*/
/*{{{  static int occampi_tracescheck_fparam (compops_t *cops, tnode_t *node, tchk_state_t *tcstate)*/
/*
 *	does traces checking on a formal parameter -- adds to the list of known things to check
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_tracescheck_fparam (compops_t *cops, tnode_t *node, tchk_state_t *tcstate)
{
	if (tcstate->inparams) {
		tnode_t *pname = tnode_nthsubof (node, 0);
		tnode_t *ptype = tnode_nthsubof (node, 1);
		int issync = 0;

		while (ptype) {
#if 0
fprintf (stderr, "occampi_tracescheck_fparam(): testing type for SYNCTYPE flag:\n");
tnode_dumptree (ptype, 1, stderr);
#endif
			if (tnode_ntflagsof (ptype) & NTF_SYNCTYPE) {
				issync = 1;
				ptype = NULL;
			} else if (tnode_haslangop_i (ptype->tag->ndef->lops, (int)LOPS_GETSUBTYPE)) {
				/* pick the sub-type */
				tnode_t *nptype;

				nptype = (tnode_t *)tnode_calllangop_i (ptype->tag->ndef->lops, (int)LOPS_GETSUBTYPE, 2, ptype, NULL);
				if (nptype == ptype) {
					ptype = NULL;
				} else {
					/* try the sub-type */
					ptype = nptype;
				}
			} else if (tnode_haslangop_i (ptype->tag->ndef->lops, (int)LOPS_TYPEREDUCE)) {
				/* pick the reduced type */
				tnode_t *nptype;

				nptype = (tnode_t *)tnode_calllangop_i (ptype->tag->ndef->lops, (int)LOPS_TYPEREDUCE, 1, ptype);
				if (nptype == ptype) {
					ptype = NULL;
				} else {
					/* try the sub-type */
					ptype = nptype;
				}
			} else {
				ptype = NULL;
			}
		}
		if (issync) {
			tchknode_t *tnref = tracescheck_createnode (TCN_NODEREF, node, pname);
			//tchknode_t *tcn = tracescheck_createatom ();
			//tchknode_t *tcnref = tracescheck_createnode (TCN_ATOMREF, tcn);

			tnode_setchook (pname, tracescheck_getnoderefchook (), tnref);
			tracescheck_addivar (tcstate, tracescheck_dupref (tnref));
#if 0
fprintf (stderr, "FPARAM looks like a sync-type:\n");
tnode_dumptree (pname, 1, stderr);
#endif
		}
	}
	return 0;
}
/*}}}*/
/*{{{  static int occampi_fetrans_fparam (compops_t *cops, tnode_t **node, fetrans_t *fe)*/
/*
 *	does front-end transforms on a formal parameter
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_fetrans_fparam (compops_t *cops, tnode_t **node, fetrans_t *fe)
{
	return 1;
}
/*}}}*/
/*{{{  static int occampi_namemap_fparam (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	transforms a formal parameter into a back-end name
 *	returns 0 to stop walk, 1 to continue;
 */
static int occampi_namemap_fparam (compops_t *cops, tnode_t **node, map_t *map)
{
	tnode_t *t = *node;
	tnode_t **namep = tnode_nthsubaddr (t, 0);
	tnode_t *type = tnode_nthsubof (t, 1);
	tnode_t *bename;
	int tsize, indir;
	int psize;

#if 0
fprintf (stderr, "occampi_namemap_fparam(): here!  target is [%s].  Type is:\n", map->target->name);
tnode_dumptree (type, 1, stderr);
#endif

	if ((t->tag == opi.tag_FPARAM) || (t->tag == opi.tag_VALFPARAM) || (t->tag == opi.tag_RESFPARAM)) {
		if (type->tag == opi.tag_CHAN) {
			/* channels need 1 word */
			tsize = map->target->chansize;
		} else {
			/* see how big this type is */
			tsize = tnode_bytesfor (type, map->target);
		}

		if ((*node)->tag == opi.tag_VALFPARAM) {
			tnode_t *ftype = tnode_nthsubof (*node, 1);

			indir = langops_valbyref (ftype) ? 1 : 0;
		} else {
			indir = 1;
		}

		if (type->tag->ndef->lops && tnode_haslangop (type->tag->ndef->lops, "bytesforparam")) {
			psize = tnode_calllangop (type->tag->ndef->lops, "bytesforparam", 2, type, map->target);
		} else {
			psize = ((indir > 0) ? map->target->pointersize : tsize);
		}

		if (psize == -1) {
			tnode_error (t, "occampi_namemap_fparam(): unknown parameter size for type (%s,%s), assuming %d!",
					type->tag->name, type->tag->ndef->name, map->target->pointersize);
			psize = map->target->pointersize;
		}
	} else {
		nocc_internal ("occampi_namemap_fparam(): not FPARAM/VALFPARAM");
		return 0;
	}

#if 0
fprintf (stderr, "occampi_namemap_fparam(): node is [%s], type is [%s], tsize = %d, indir = %d\n", t->tag->name, type->tag->name, tsize, indir);
#endif
	bename = map->target->newname (*namep, NULL, map, psize, 0, 0, 0, tsize, indir);
	tnode_setchook (*namep, map->mapchook, (void *)bename);

	*node = bename;
	return 0;
}
/*}}}*/
/*{{{  static tnode_t *occampi_gettype_fparam (langops_t *lops, tnode_t *node, tnode_t *defaulttype)*/
/*
 *	returns the type of a formal parameter
 */
static tnode_t *occampi_gettype_fparam (langops_t *lops, tnode_t *node, tnode_t *defaulttype)
{
	return tnode_nthsubof (node, 1);
}
/*}}}*/
/*{{{  static int occampi_getdescriptor_fparam (langops_t *lops, tnode_t *node, char **str)*/
/*
 *	gets descriptor information for a formal parameter
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_getdescriptor_fparam (langops_t *lops, tnode_t *node, char **str)
{
	tnode_t *name = tnode_nthsubof (node, 0);
	tnode_t *type = tnode_nthsubof (node, 1);
	char *typestr = NULL;
	char *pname = NameNameOf (tnode_nthnameof (name, 0));

	/* get type first */
	langops_getdescriptor (type, &typestr);
	if (!typestr) {
		typestr = string_dup ("ANY");
	}

	if (*str) {
		char *newstr = (char *)smalloc (strlen (*str) + strlen (typestr) + strlen (pname) + 16);
		int nslen = 0;

		nslen = sprintf (newstr, "%s", *str);
		if (node->tag == opi.tag_VALFPARAM) {
			nslen += sprintf (newstr + nslen, "VAL ");
		} else if (node->tag == opi.tag_RESFPARAM) {
			nslen += sprintf (newstr + nslen, "RESULT ");
		}
		nslen += sprintf (newstr + nslen, "%s %s", typestr, pname);
		sfree (*str);
		*str = newstr;
	} else {
		int slen = 0;

		*str = (char *)smalloc (strlen (typestr) + strlen (pname) + 16);
		if (node->tag == opi.tag_VALFPARAM) {
			slen += sprintf (*str + slen, "VAL ");
		} else if (node->tag == opi.tag_RESFPARAM) {
			slen += sprintf (*str + slen, "RESULT ");
		}
		slen += sprintf (*str + slen, "%s %s", typestr, pname);
	}

	sfree (typestr);

	return 0;
}
/*}}}*/
/*{{{  static int occampi_getname_fparam (langops_t *lops, tnode_t *node, char **str)*/
/*
 *	gets the name of a formal parameter
 *	returns 0 on success, -ve on failure
 */
static int occampi_getname_fparam (langops_t *lops, tnode_t *node, char **str)
{
	tnode_t *name = tnode_nthsubof (node, 0);
	char *pname = NameNameOf (tnode_nthnameof (name, 0));

	if (*str) {
		sfree (*str);
	}
	*str = (char *)smalloc (strlen (pname) + 2);
	strcpy (*str, pname);

	return 0;
}
/*}}}*/
/*{{{  static tnode_t *occampi_hiddenparamsof_fparam (langops_t *lops, tnode_t *node)*/
/*
 *	gets the hidden-parameters associated with a normal formal parameter -- e.g. open-arrays
 */
static tnode_t *occampi_hiddenparamsof_fparam (langops_t *lops, tnode_t *node)
{
	tnode_t *name = tnode_nthsubof (node, 0);
	tnode_t *type = tnode_nthsubof (node, 1);
	tnode_t *hplist;

	hplist = langops_hiddenparamsof (type);
	if (hplist) {
		int i, nhitems;
		tnode_t **hitems = parser_getlistitems (hplist, &nhitems);

		for (i=0; i<nhitems; i++) {
			tnode_t *hparam = hitems[i];

			if (hparam->tag == opi.tag_HIDDENDIMEN) {
				if (tnode_nthsubof (hparam, 0)->tag == opi.tag_DIMSIZE) {
					/* dimension size of us -- put in reference */
					tnode_setnthsub (tnode_nthsubof (hparam, 0), 0, name);
				}
			}
		}
	}
#if 0
fprintf (stderr, "occampi_hiddenparamsof_fparam(): here! hplist =\n");
tnode_dumptree (hplist, 1, stderr);
#endif

	return hplist;
}
/*}}}*/

/*{{{  static int occampi_namemap_hiddennode (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	transforms a hidden formal parameter into a back-end name
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_namemap_hiddennode (compops_t *cops, tnode_t **node, map_t *map)
{
	tnode_t *bename;

	/*
	 * if we're in a parameter list, create a real back-end name for this node, otherwise
	 * generate a back-end name-reference for it
	 */
	if (map->inparamlist) {
		bename = map->target->newname (*node, NULL, map, map->target->intsize, 0, 0, 0, map->target->intsize, 0);
		tnode_setchook (*node, map->mapchook, (void *)bename);
	} else {
		tnode_t *rname = (tnode_t *)tnode_getchook (*node, map->mapchook);

		if (!rname) {
#if 0
fprintf (stderr, "occampi_namemap_hiddennode(): failing here.  node was:\n");
tnode_dumptree (*node, 1, stderr);
#endif
			nocc_internal ("occampi_namemap_hiddennode(): not in parameters, and no mapchook linkage");
			return 0;
		}

		bename = map->target->newnameref (rname, map);
	}

	*node = bename;
	return 0;
}
/*}}}*/

/*{{{  static int occampi_procdecl_isvar_namenode (langops_t *lops, tnode_t *node)*/
/*
 *	returns non-zero if the specified name is a variable (l-value)
 */
static int occampi_procdecl_isvar_namenode (langops_t *lops, tnode_t *node)
{
	int v = 0;

	if ((node->tag == opi.tag_NPARAM) || (node->tag == opi.tag_NRESPARAM)) {
		v = 1;
	} else {
		if (lops->next && tnode_haslangop (lops, "isvar")) {
			v = tnode_calllangop (lops->next, "isvar", 1, node);
		}
	}

	return v;
}
/*}}}*/


/*{{{  static int occampi_procdecl_init_nodes (void)*/
/*
 *	sets up proc declaration nodes for occam-pi
 *	returns 0 on success, non-zero on error
 */
static int occampi_procdecl_init_nodes (void)
{
	tndef_t *tnd;
	compops_t *cops;
	langops_t *lops;
	int i;

	/*{{{  occampi:namenode -- N_PROCDEF, N_PARAM, N_VALPARAM, N_RESPARAM*/
	tnd = tnode_lookupnodetype ("occampi:namenode");
	if (!tnd) {
		nocc_internal ("occampi_procdecl_init_nodes(): failed to find occampi:namenode node-type!");
		return -1;
	}

	cops = tnode_insertcompops (tnd->ops);
	/* all compiler-ops handled upstairs in occampi_decl */
	tnd->ops = cops;
	lops = tnode_insertlangops (tnd->lops);
	tnode_setlangop (lops, "isvar", 1, LANGOPTYPE (occampi_procdecl_isvar_namenode));

	tnd->lops = lops;

	i = -1;
	opi.tag_NPARAM = tnode_newnodetag ("N_PARAM", &i, opi.node_NAMENODE, NTF_NONE);
	i = -1;
	opi.tag_NVALPARAM = tnode_newnodetag ("N_VALPARAM", &i, opi.node_NAMENODE, NTF_NONE);
	i = -1;
	opi.tag_NPROCDEF = tnode_newnodetag ("N_PROCDEF", &i, opi.node_NAMENODE, NTF_NONE);

	/*}}}*/
	/*{{{  occampi:hiddennode -- HIDDENPARAM, HIDDENDIMEN*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:hiddennode", &i, 1, 0, 0, TNF_NONE);			/* subnodes: hidden-param */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (occampi_namemap_hiddennode));
	tnd->ops = cops;

	i = -1;
	opi.tag_HIDDENPARAM = tnode_newnodetag ("HIDDENPARAM", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_HIDDENDIMEN = tnode_newnodetag ("HIDDENDIMEN", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  occampi:fparam -- FPARAM, VALFPARAM, RESFPARAM*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:fparam", &i, 2, 0, 0, TNF_NONE);			/* subnodes: name; type */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "prescope", 2, COMPOPTYPE (occampi_prescope_fparam));
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (occampi_scopein_fparam));
	tnode_setcompop (cops, "tracescheck", 2, COMPOPTYPE (occampi_tracescheck_fparam));
	tnode_setcompop (cops, "fetrans", 2, COMPOPTYPE (occampi_fetrans_fparam));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (occampi_namemap_fparam));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "getdescriptor", 2, LANGOPTYPE (occampi_getdescriptor_fparam));
	tnode_setlangop (lops, "gettype", 2, LANGOPTYPE (occampi_gettype_fparam));
	tnode_setlangop (lops, "getname", 2, LANGOPTYPE (occampi_getname_fparam));
	tnode_setlangop (lops, "hiddenparamsof", 1, LANGOPTYPE (occampi_hiddenparamsof_fparam));
	tnd->lops = lops;

	i = -1;
	opi.tag_FPARAM = tnode_newnodetag ("FPARAM", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_VALFPARAM = tnode_newnodetag ("VALFPARAM", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_RESFPARAM = tnode_newnodetag ("RESFPARAM", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  occampi:leafnode -- RETURNADDRESS*/
	tnd = tnode_lookupnodetype ("occampi:leafnode");

	i = -1;
	opi.tag_RETURNADDRESS = tnode_newnodetag ("RETURNADDRESS", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  occampi:procdecl -- PROCDECL*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:procdecl", &i, 4, 0, 0, TNF_LONGDECL);		/* subnodes: name; fparams; body; in-scope-body */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "prescope", 2, COMPOPTYPE (occampi_prescope_procdecl));
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (occampi_scopein_procdecl));
	tnode_setcompop (cops, "scopeout", 2, COMPOPTYPE (occampi_scopeout_procdecl));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (occampi_namemap_procdecl));
	tnode_setcompop (cops, "precheck", 1, COMPOPTYPE (occampi_precheck_procdecl));
	tnode_setcompop (cops, "mobilitycheck", 2, COMPOPTYPE (occampi_mobilitycheck_procdecl));
	tnode_setcompop (cops, "tracescheck", 2, COMPOPTYPE (occampi_tracescheck_procdecl));
	tnode_setcompop (cops, "fetrans", 2, COMPOPTYPE (occampi_fetrans_procdecl));
	tnode_setcompop (cops, "betrans", 2, COMPOPTYPE (occampi_betrans_procdecl));
	tnode_setcompop (cops, "precode", 2, COMPOPTYPE (occampi_precode_procdecl));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (occampi_codegen_procdecl));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "getdescriptor", 2, LANGOPTYPE (occampi_getdescriptor_procdecl));
	tnode_setlangop (lops, "gettype", 2, LANGOPTYPE (occampi_gettype_procdecl));
	tnode_setlangop (lops, "do_usagecheck", 2, LANGOPTYPE (occampi_usagecheck_procdecl));
	tnd->lops = lops;

	i = -1;
	opi.tag_PROCDECL = tnode_newnodetag ("PROCDECL", &i, tnd, NTF_INDENTED_PROC);

	/*}}}*/

	/*{{{  compiler operations for handling scoping and other things associated with parameters*/
	if (tnode_newcompop ("inparams_scopein", COPS_INVALID, 2, INTERNAL_ORIGIN) < 0) {
		nocc_error ("occampi_decl_init_nodes(): failed to create inparams_scopein compiler operation");
		return -1;
	}
	inparams_scopein_compop = tnode_findcompop ("inparams_scopein");

	if (tnode_newcompop ("inparams_scopeout", COPS_INVALID, 2, INTERNAL_ORIGIN) < 0) {
		nocc_error ("occampi_decl_init_nodes(): failed to create inparams_scopeout compiler operation");
		return -1;
	}
	inparams_scopeout_compop = tnode_findcompop ("inparams_scopeout");

	if (tnode_newcompop ("inparams_namemap", COPS_INVALID, 2, INTERNAL_ORIGIN) < 0) {
		nocc_error ("occampi_decl_init_nodes(): failed to create inparams_namemap compiler operation");
		return -1;
	}
	inparams_namemap_compop = tnode_findcompop ("inparams_namemap");

	if (!inparams_scopein_compop || !inparams_scopeout_compop) {
		nocc_error ("occampi_decl_init_nodes(): failed to find inparams scoping compiler operations");
		return -1;
	}

	/*}}}*/

	return 0;
}
/*}}}*/
/*{{{  static int occampi_procdecl_post_setup (void)*/
/*
 *	does post-setup for proc declaration nodes
 *	returns 0 on success, non-zero on failure
 */
static int occampi_procdecl_post_setup (void)
{
	tndef_t *tnd = (opi.tag_PROCDECL)->ndef;

	tnode_setcompop (tnd->ops, "miscnodetrans", 2, COMPOPTYPE (occampi_miscnodetrans_procdecl));

	return 0;
}
/*}}}*/


/*{{{  occampi_procdecl_feunit (feunit_t)*/
feunit_t occampi_procdecl_feunit = {
	init_nodes: occampi_procdecl_init_nodes,
	reg_reducers: NULL,
	init_dfatrans: NULL,
	post_setup: occampi_procdecl_post_setup,
	ident: "occampi-procdecl"
};

/*}}}*/

