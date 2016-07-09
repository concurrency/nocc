/*
 *	mcsp_decl.c -- declarations for MCSP
 *	Copyright (C) 2006-2016 Fred Barnes <frmb@kent.ac.uk>
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
#include "postcheck.h"
#include "fetrans.h"
#include "mwsync.h"
#include "betrans.h"
#include "map.h"
#include "codegen.h"
#include "target.h"
#include "transputer.h"


/*}}}*/


/*{{{  static int mcsp_scopenode_checktail_walktree (tnode_t *node, void *arg)*/
/*
 *	checks that all tail nodes of the given tree are instances of itself,
 *	used to turn FIXPOINTs into ILOOPs (tail-recursion only)
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_scopenode_checktail_walktree (tnode_t *node, void *arg)
{
	tnode_t **tmpnodes = (tnode_t **)arg;

	if (!node) {
		nocc_internal ("mcsp_scopenode_checktail(): null node!");
		return 0;
	}
	if (node->tag == mcsp.tag_INSTANCE) {
		if (tnode_nthsubof (node, 0) != tmpnodes[1]) {
			tmpnodes[0] = (tnode_t *)0;			/* instance of something else */
		}
		return 0;
	} else if (node->tag == mcsp.tag_SEQCODE) {
		tnode_t **items;
		int nitems;

		items = parser_getlistitems (tnode_nthsubof (node, 1), &nitems);
		if (nitems > 0) {
			tnode_prewalktree (items[nitems - 1], mcsp_scopenode_checktail_walktree, arg);
		} else {
			tmpnodes[0] = (tnode_t *)0;			/* empty SEQ */
		}
		return 0;
	} else if (node->tag == mcsp.tag_ALT) {
		tnode_t **items;
		int nitems, i;

		items = parser_getlistitems (tnode_nthsubof (node, 0), &nitems);
		for (i=0; i<nitems; i++) {
			if (items[i]->tag == mcsp.tag_GUARD) {
				tnode_prewalktree (tnode_nthsubof (items[i], 1), mcsp_scopenode_checktail_walktree, arg);
			}
			if (tmpnodes[0] == (tnode_t *)0) {
				/* early getout */
				return 0;
			}
		}
		return 0;
	} else if (node->tag == mcsp.tag_PAR) {
		tmpnodes[0] = (tnode_t *)0;				/* PAR makes things complex */
		return 0;
	} else if (node->tag == mcsp.tag_THEN) {
		/* shouldn't really see this after fetrans */
		nocc_warning ("mcsp_scopenode_checktail(): found unexpected [%s]", node->tag->name);

		tnode_prewalktree (tnode_nthsubof (node, 1), mcsp_scopenode_checktail_walktree, arg);
		return 0;
	}

	/* otherwise assume that it's not a tail instance */
	tmpnodes[0] = (tnode_t *)0;
	return 0;
}
/*}}}*/
/*{{{  static int mcsp_scopenode_rminstancetail_modwalktree (tnode_t **nodep, void *arg)*/
/*
 *	removes tail instance nodes, used to turn FIXPOINTs into ILOOPs (tail-recursion only)
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_scopenode_rminstancetail_modwalktree (tnode_t **nodep, void *arg)
{
	tnode_t *iname = (tnode_t *)arg;
	tnode_t *node = *nodep;

	if (!node || !iname) {
		nocc_internal ("mcsp_scopenode_rminstancetail(): null node or instance-name!");
		return 0;
	}
	if (node->tag == mcsp.tag_INSTANCE) {
		/* turn into SKIP */
		*nodep = tnode_create (mcsp.tag_SKIP, NULL);
	} else if (node->tag == mcsp.tag_SEQCODE) {
		tnode_t **items;
		int nitems;

		items = parser_getlistitems (tnode_nthsubof (node, 1), &nitems);
		if (nitems > 0) {
			tnode_modprewalktree (&items[nitems - 1], mcsp_scopenode_rminstancetail_modwalktree, arg);
		}
		return 0;
	} else if (node->tag == mcsp.tag_ALT) {
		tnode_t **items;
		int nitems, i;

		items = parser_getlistitems (tnode_nthsubof (node, 0), &nitems);
		for (i=0; i<nitems; i++) {
			if (items[i]->tag == mcsp.tag_GUARD) {
				tnode_modprewalktree (tnode_nthsubaddr (items[i], 1), mcsp_scopenode_rminstancetail_modwalktree, arg);
			}
		}
		return 0;
	} else if (node->tag == mcsp.tag_THEN) {
		tnode_modprewalktree (tnode_nthsubaddr (node, 1), mcsp_scopenode_rminstancetail_modwalktree, arg);
		return 0;
	}
	return 0;
}
/*}}}*/
/*{{{  static int mcsp_prescope_scopenode (compops_t *cops, tnode_t **node, prescope_t *ps)*/
/*
 *	does pre-scoping on an MCSP scope node (ensures vars are a list)
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_prescope_scopenode (compops_t *cops, tnode_t **node, prescope_t *ps)
{
	if (!tnode_nthsubof (*node, 0)) {
		/* no vars, make empty list */
		tnode_setnthsub (*node, 0, parser_newlistnode (NULL));
	} else if (!parser_islistnode (tnode_nthsubof (*node, 0))) {
		/* singleton */
		tnode_t *list = parser_newlistnode (NULL);

		parser_addtolist (list, tnode_nthsubof (*node, 0));
		tnode_setnthsub (*node, 0, list);
	}
	return 1;
}
/*}}}*/
/*{{{  static int mcsp_scopein_scopenode (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	called to scope in an MCSP something that introduces scope (names)
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_scopein_scopenode (compops_t *cops, tnode_t **node, scope_t *ss)
{
	void *nsmark;
	tnode_t *vars = tnode_nthsubof (*node, 0);
	tnode_t **bodyptr = tnode_nthsubaddr (*node, 1);
	tnode_t **varlist;
	int nvars, i;
	ntdef_t *xtag;
	tnode_t *ntype = NULL;

	if ((*node)->tag == mcsp.tag_HIDE) {
		xtag = mcsp.tag_EVENT;
	} else if ((*node)->tag == mcsp.tag_FIXPOINT) {
		xtag = mcsp.tag_PROCDEF;
		ntype = parser_newlistnode (NULL);
	} else {
		scope_error (*node, ss, "mcsp_scopein_scopename(): can't scope [%s] ?", (*node)->tag->name);
		return 1;
	}

	nsmark = name_markscope ();

	/* go through each name and bring it into scope */
	varlist = parser_getlistitems (vars, &nvars);
	for (i=0; i<nvars; i++) {
		tnode_t *name = varlist[i];
		char *rawname;
		name_t *sname;
		tnode_t *newname;

		if (name->tag != mcsp.tag_NAME) {
			scope_error (name, ss, "not raw name!");
			return 0;
		}
#if 0
fprintf (stderr, "mcsp_scopein_scopenode(): scoping in name =\n");
tnode_dumptree (name, 1, stderr);
#endif
		rawname = (char *)tnode_nthhookof (name, 0);

		sname = name_addscopename (rawname, *node, ntype, NULL);
		newname = tnode_createfrom (xtag, name, sname);
		SetNameNode (sname, newname);
		varlist[i] = newname;		/* put new name in list */
		ss->scoped++;

		/* free old name */
		tnode_free (name);
	}

	/* then walk the body */
	tnode_modprepostwalktree (bodyptr, scope_modprewalktree, scope_modpostwalktree, (void *)ss);

	/* descope declared names */
	name_markdescope (nsmark);
	
	return 0;
}
/*}}}*/
/*{{{  static int mcsp_scopeout_scopenode (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	called to scope-out an MCSP something
 */
static int mcsp_scopeout_scopenode (compops_t *cops, tnode_t **node, scope_t *ss)
{
	/* all done in scope-in */
	return 1;
}
/*}}}*/
/*{{{  static int mcsp_postcheck_scopenode (compops_t *cops, tnode_t **node, postcheck_t *pc)*/
/*
 *	does post-check transforms on a scopenode
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_postcheck_scopenode (compops_t *cops, tnode_t **node, postcheck_t *pc)
{
	tnode_t *t = *node;

	if (t->tag == mcsp.tag_HIDE) {
		postcheck_subtree (tnode_nthsubaddr (t, 1), pc);
		return 0;
	}
	return 1;
}
/*}}}*/
/*{{{  static int mcsp_fetrans_scopenode (compops_t *cops, tnode_t **node, fetrans_t *fe)*/
/*
 *	does front-end transforms on a scopenode -- turns HIDE vars into declarations
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_fetrans_scopenode (compops_t *cops, tnode_t **node, fetrans_t *fe)
{
	mcsp_fetrans_t *mfe = (mcsp_fetrans_t *)fe->langpriv;
	tnode_t *t = *node;

	switch (mfe->parse) {
	case 0:
		if (t->tag == mcsp.tag_HIDE) {
			/*{{{  hiding-operator: turn into VARDECLs*/
			tnode_t *varlist = tnode_nthsubof (t, 0);
			tnode_t *process = tnode_nthsubof (t, 1);
			tnode_t **vars;
			tnode_t **top = node;
			int nvars, i;

			/* create var-declarations for each name in the list */
			vars = parser_getlistitems (varlist, &nvars);
			for (i=0; i<nvars; i++) {
				*node = tnode_createfrom (mcsp.tag_VARDECL, t, vars[i], tnode_create (mcsp.tag_EVENTTYPE, NULL), NULL, NULL);
				SetNameType (tnode_nthnameof (vars[i], 0), tnode_nthsubof (*node, 1));
				node = tnode_nthsubaddr (*node, 3);
				vars[i] = NULL;
			}

			/* drop in process */
			*node = process;

			/* free up HIDE */
			tnode_setnthsub (t, 1, NULL);
			tnode_free (t);

			/* explicitly sub-walk new node */
			fetrans_subtree (top, fe);

			return 0;
			/*}}}*/
		}
		break;
	case 2:
		if (t->tag == mcsp.tag_FIXPOINT) {
			/*{{{  see if we have tail-call recursion only*/
			tnode_t *tmpnodes[2];

			/* walk body first */
			fetrans_subtree (tnode_nthsubaddr (t, 1), fe);

			tmpnodes[0] = (tnode_t *)1;
			tmpnodes[1] = tnode_nthsubof (t, 0);		/* this instance */

			if (parser_islistnode (tmpnodes[1])) {
				tmpnodes[1] = parser_getfromlist (tmpnodes[1], 0);
			}
			tnode_prewalktree (tnode_nthsubof (t, 1), mcsp_scopenode_checktail_walktree, (void *)tmpnodes);

			if (tmpnodes[0] == (tnode_t *)1) {
				/* means we can turn it into a loop :) */
				tnode_modprewalktree (tnode_nthsubaddr (t, 1), mcsp_scopenode_rminstancetail_modwalktree, (void *)tmpnodes[1]);

				tnode_free (tnode_nthsubof (t, 0));
				tnode_setnthsub (t, 0, NULL);

				*node = tnode_create (mcsp.tag_ILOOP, NULL, tnode_nthsubof (t, 1), NULL);
				tnode_setnthsub (t, 1, NULL);

				tnode_free (t);
			}

			return 0;
			/*}}}*/
		}
		break;
	}

	return 1;
}
/*}}}*/


/*{{{  static int mcsp_prescope_declnode (compops_t *cops, tnode_t **node, prescope_t *ps)*/
/*
 *	pre-scopes a process definition
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_prescope_declnode (compops_t *cops, tnode_t **node, prescope_t *ps)
{
	tnode_t **paramptr = tnode_nthsubaddr (*node, 1);
	tnode_t **params;
	int nparams, i;
	
	if (!*paramptr) {
		/* no parameters, make empty list */
		*paramptr = parser_newlistnode (NULL);
	} else if (!parser_islistnode (*paramptr)) {
		/* singleton, make list */
		tnode_t *list = parser_newlistnode (NULL);

		parser_addtolist (list, *paramptr);
		*paramptr = list;
	}

	/* now go through and turn into FPARAM nodes -- needed for allocation later */
	params = parser_getlistitems (*paramptr, &nparams);
	for (i=0; i<nparams; i++) {
		params[i] = tnode_createfrom (mcsp.tag_FPARAM, params[i], params[i], tnode_create (mcsp.tag_EVENTTYPE, NULL));
	}

	return 1;
}
/*}}}*/
/*{{{  static int mcsp_scopein_declnode (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	called to scope-in a process definition
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_scopein_declnode (compops_t *cops, tnode_t **node, scope_t *ss)
{
	mcsp_scope_t *mss = (mcsp_scope_t *)ss->langpriv;
	tnode_t *name = tnode_nthsubof (*node, 0);
	tnode_t **paramptr = tnode_nthsubaddr (*node, 1);
	tnode_t **bodyptr = tnode_nthsubaddr (*node, 2);
	void *nsmark;
	char *rawname;
	name_t *procname;
	tnode_t *newname;
	tnode_t *saved_uvil = mss->uvinsertlist;
	void *saved_uvsm = mss->uvscopemark;

	nsmark = name_markscope ();
	/* scope-in any parameters and walk body */
	tnode_modprepostwalktree (paramptr, scope_modprewalktree, scope_modpostwalktree, (void *)ss);
	mss->uvinsertlist = *paramptr;										/* prescope made sure it's a list */
	mss->uvscopemark = nsmark;
	tnode_modprepostwalktree (bodyptr, scope_modprewalktree, scope_modpostwalktree, (void *)ss);

	name_markdescope (nsmark);
	mss->uvinsertlist = saved_uvil;
	mss->uvscopemark = saved_uvsm;

	/* declare and scope PROCDEF name, then scope process in scope of it */
	rawname = (char *)tnode_nthhookof (name, 0);
	procname = name_addscopenamess (rawname, *node, *paramptr, NULL, ss);
	newname = tnode_createfrom (mcsp.tag_PROCDEF, name, procname);
	SetNameNode (procname, newname);
	tnode_setnthsub (*node, 0, newname);

	/* free old name, scope process */
	tnode_free (name);
	tnode_modprepostwalktree (tnode_nthsubaddr (*node, 3), scope_modprewalktree, scope_modpostwalktree, (void *)ss);
	ss->scoped++;

	return 0;
}
/*}}}*/
/*{{{  static int mcsp_scopeout_declnode (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	called to scope-out a process definition
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_scopeout_declnode (compops_t *cops, tnode_t **node, scope_t *ss)
{
	return 1;
}
/*}}}*/
/*{{{  static int mcsp_fetrans_declnode (compops_t *cops, tnode_t **node, fetrans_t *fe)*/
/*
 *	does front-end transforms on a process declaration node
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_fetrans_declnode (compops_t *cops, tnode_t **node, fetrans_t *fe)
{
	mcsp_fetrans_t *mfe = (mcsp_fetrans_t *)fe->langpriv;

	switch (mfe->parse) {
	case 0:
		if ((*node)->tag == mcsp.tag_PROCDECL) {
			void *saved_uvil = mfe->uvinsertlist;

			mfe->uvinsertlist = tnode_nthsubof (*node, 1);
			fetrans_subtree (tnode_nthsubaddr (*node, 1), fe);		/* params */
			fetrans_subtree (tnode_nthsubaddr (*node, 2), fe);		/* body */
			mfe->uvinsertlist = saved_uvil;
			fetrans_subtree (tnode_nthsubaddr (*node, 3), fe);		/* in-scope body */

			return 0;
		}
		break;
	}

	return 1;
}
/*}}}*/
/*{{{  static int mcsp_mwsynctrans_declnode (compops_t *cops, tnode_t **node, mwsynctrans_t *mwi)*/
/*
 *	does multiway synchronisation transforms on a process declaration node
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_mwsynctrans_declnode (compops_t *cops, tnode_t **node, mwsynctrans_t *mwi)
{
	tnode_t **paramsp = tnode_nthsubaddr (*node, 1);
	tnode_t **bodyp = tnode_nthsubaddr (*node, 2);
	tnode_t **nextp = tnode_nthsubaddr (*node, 3);
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
/*{{{  static int mcsp_betrans_declnode (compops_t *cops, tnode_t **node, betrans_t *be)*/
/*
 *	does back-end transforms on a process declaration node
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_betrans_declnode (compops_t *cops, tnode_t **node, betrans_t *be)
{
	return 1;
}
/*}}}*/
/*{{{  static int mcsp_namemap_declnode (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	does name-mapping for a process declaration node
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_namemap_declnode (compops_t *cops, tnode_t **node, map_t *map)
{
	tnode_t *blk;
	/* tnode_t *saved_blk = map->thisblock;
	tnode_t **saved_params = map->thisprocparams; */
	tnode_t **paramsptr;
	tnode_t *tmpname;

	blk = map->target->newblock (tnode_nthsubof (*node, 2), map, tnode_nthsubof (*node, 1), map->lexlevel + 1);
	map_pushlexlevel (map, blk, tnode_nthsubaddr (*node, 1));
	/* map->thisblock = blk;
	 * map->thisprocparams = tnode_nthsubaddr (*node, 1);
	 * map->lexlevel++; */

	/* map formal params and body */
	paramsptr = tnode_nthsubaddr (*node, 1);
	map_submapnames (paramsptr, map);
	map_submapnames (tnode_nthsubaddr (blk, 0), map);		/* do this under the back-end block */

	map_poplexlevel (map);
	/* map->lexlevel--;
	 * map->thisblock = saved_blk;
	 * map->thisprocparams = saved_params; */

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

	tmpname = map->target->newname (tnode_create (mcsp.tag_HIDDENPARAM, NULL, tnode_create (mcsp.tag_RETURNADDRESS, NULL)), NULL, map,
			map->target->pointersize, 0, 0, 0, map->target->pointersize, 0);
	parser_addtolist_front (*paramsptr, tmpname);

	return 0;
}
/*}}}*/
/*{{{  static int mcsp_precode_declnode (compops_t *cops, tnode_t **node, codegen_t *cgen)*/
/*
 *	does pre-code on a process declaration node
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_precode_declnode (compops_t *cops, tnode_t **node, codegen_t *cgen)
{
	tnode_t *t = *node;
	tnode_t *name = tnode_nthsubof (t, 0);

	/* walk body */
	codegen_subprecode (tnode_nthsubaddr (t, 2), cgen);

	codegen_precode_seenproc (cgen, tnode_nthnameof (name, 0), t);

	/* pre-code stuff following declaration */
	codegen_subprecode (tnode_nthsubaddr (t, 3), cgen);

	return 0;
}
/*}}}*/
/*{{{  static int mcsp_codegen_declnode (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	does code-generation for a process definition
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_codegen_declnode (compops_t *cops, tnode_t *node, codegen_t *cgen)
{
	tnode_t *body = tnode_nthsubof (node, 2);
	tnode_t *name = tnode_nthsubof (node, 0);
	int ws_size, ws_offset, adjust;
	name_t *pname;

	cgen->target->be_getblocksize (body, &ws_size, &ws_offset, NULL, NULL, &adjust, NULL);
	pname = tnode_nthnameof (name, 0);
	codegen_callops (cgen, comment, "PROCESS %s = %d,%d,%d", pname->me->name, ws_size, ws_offset, adjust);
	codegen_callops (cgen, setwssize, ws_size, adjust);
	codegen_callops (cgen, setvssize, 0);
	codegen_callops (cgen, setmssize, 0);
	codegen_callops (cgen, setnamelabel, pname);
	codegen_callops (cgen, procnameentry, pname);
	codegen_callops (cgen, debugline, node);

	/* generate body */
	codegen_subcodegen (body, cgen);
	codegen_callops (cgen, procreturn, adjust);

	/* generate code following declaration */
	codegen_subcodegen (tnode_nthsubof (node, 3), cgen);

	return 0;
}
/*}}}*/


/*{{{  static int mcsp_scopein_spacenode (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	scopes-in a SPACENODE (formal parameters and declarations)
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_scopein_spacenode (compops_t *cops, tnode_t **node, scope_t *ss)
{
	if ((*node)->tag == mcsp.tag_FPARAM) {
		/*{{{  scope in parameter*/
		tnode_t **paramptr = tnode_nthsubaddr (*node, 0);
		char *rawname;
		name_t *name;
		tnode_t *newname;

		rawname = (char *)tnode_nthhookof (*paramptr, 0);
		name = name_addscopename (rawname, *paramptr, NULL, NULL);
		newname = tnode_createfrom (mcsp.tag_EVENT, *paramptr, name);
		SetNameNode (name, newname);
		
		/* free old name, replace with new */
		tnode_free (*paramptr);
		ss->scoped++;
		*paramptr = newname;

		return 0;		/* don't walk beneath */
		/*}}}*/
	}
	return 1;
}
/*}}}*/
/*{{{  static int mcsp_scopeout_spacenode (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	scopes-out a SPACENODE (formal parameters and declarations)
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_scopeout_spacenode (compops_t *cops, tnode_t **node, scope_t *ss)
{
	if ((*node)->tag == mcsp.tag_FPARAM) {
		return 0;
	}
	return 1;
}
/*}}}*/
/*{{{  static int mcsp_typecheck_spacenode (compops_t *cops, tnode_t *node, typecheck_t *tc)*/
/*
 *	does type-checking on a spacenode
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_typecheck_spacenode (compops_t *cops, tnode_t *node, typecheck_t *tc)
{
	return 1;
}
/*}}}*/
/*{{{  static int mcsp_postcheck_spacenode (compops_t *cops, tnode_t **node, postcheck_t *pc)*/
/*
 *	does post-check transformation on a space-node
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_postcheck_spacenode (compops_t *cops, tnode_t **node, postcheck_t *pc)
{
	if (((*node)->tag == mcsp.tag_FPARAM) || ((*node)->tag == mcsp.tag_UPARAM)) {
		/* don't do event underneath, but check for type */
		tnode_t **namep = tnode_nthsubaddr (*node, 0);
		tnode_t **typep = tnode_nthsubaddr (*node, 1);

		if (!*typep) {
			*typep = tnode_create (mcsp.tag_EVENTTYPE, NULL);
		}
		if (*namep) {
			SetNameType (tnode_nthnameof (*namep, 0), *typep);
		}
		return 0;
	}
	return 1;
}
/*}}}*/
/*{{{  static int mcsp_fetrans_spacenode (compops_t *cops, tnode_t **node, fetrans_t *fe)*/
/*
 *	does front-end transformation on an FPARAM
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_fetrans_spacenode (compops_t *cops, tnode_t **node, fetrans_t *fe)
{
	mcsp_fetrans_t *mfe = (mcsp_fetrans_t *)fe->langpriv;

	switch (mfe->parse) {
	case 0:
		if (((*node)->tag == mcsp.tag_FPARAM) || ((*node)->tag == mcsp.tag_UPARAM)) {
			/* nothing to do, don't walk EVENT underneath */
			return 0;
		}
		break;
	}

	return 1;
}
/*}}}*/
/*{{{  static int mcsp_mwsynctrans_spacenode (compops_t *cops, tnode_t **node, mwsynctrans_t *mwi)*/
/*
 *	does multiway sync transforms on an FPARAM/UPARAM
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_mwsynctrans_spacenode (compops_t *cops, tnode_t **node, mwsynctrans_t *mwi)
{
	tnode_t *name = tnode_nthsubof (*node, 0);

	if ((name->tag == mcsp.tag_EVENT) && (NameTypeOf (tnode_nthnameof (name, 0))->tag == mcsp.tag_EVENTTYPE)) {
		mwsync_transsubtree (tnode_nthsubaddr (*node, 1), mwi);			/* transform type */
		SetNameType (tnode_nthnameof (name, 0), tnode_nthsubof (*node, 1));

		mwsync_mwsynctrans_pushparam (*node, name, mwi);
	}
	return 0;
}
/*}}}*/
/*{{{  static int mcsp_namemap_spacenode (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	does name-mapping on s spacenode (formal params, decls)
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_namemap_spacenode (compops_t *cops, tnode_t **node, map_t *map)
{
	tnode_t **namep = tnode_nthsubaddr (*node, 0);
	tnode_t *type = NULL;
	tnode_t *bename;

	bename = map->target->newname (*namep, NULL, map, map->target->pointersize, 0, 0, 0, map->target->pointersize, 1);		/* always pointers.. (FIXME: tsize)*/

	tnode_setchook (*namep, map->mapchook, (void *)bename);
	*node = bename;
	
	return 0;
}
/*}}}*/
/*{{{  static int mcsp_codegen_spacenode (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	does code-gen on a spacenode (formal params, decls)
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_codegen_spacenode (compops_t *cops, tnode_t *node, codegen_t *cgen)
{
	return 0;
}
/*}}}*/


/*{{{  static int mcsp_fetrans_vardeclnode (compops_t *cops, tnode_t **node, fetrans_t *fe)*/
/*
 *	does front-end transforms for vardeclnodes
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_fetrans_vardeclnode (compops_t *cops, tnode_t **node, fetrans_t *fe)
{
	mcsp_fetrans_t *mfe = (mcsp_fetrans_t *)fe->langpriv;
	tnode_t *t = *node;

#if 0
	nocc_message ("mcsp_fetrans_vardeclnode(): mfe->parse = %d, tag = [%s], *node =", mfe->parse, (*node)->tag->name);
	tnode_dumptree (*node, 1, stderr);
#endif
	switch (mfe->parse) {
	case 0:
		if (t->tag == mcsp.tag_VARDECL) {
			/* just walk subnodes 2+3 */
			fetrans_subtree (tnode_nthsubaddr (t, 2), fe);
			fetrans_subtree (tnode_nthsubaddr (t, 3), fe);
			return 0;
		}
		break;
	case 1:
		/* nothing in this pass */
		break;
	case 2:
		if (t->tag == mcsp.tag_VARDECL) {
			/*{{{  collect up events in body of declaration, hide the one declared*/
			mcsp_alpha_t *savedalpha = mfe->curalpha;
			mcsp_alpha_t *myalpha;

			mfe->curalpha = mcsp_newalpha ();
			fetrans_subtree (tnode_nthsubaddr (t, 3), fe);		/* walk process in scope of variable */
			myalpha = mfe->curalpha;
			mfe->curalpha = savedalpha;

			parser_rmfromlist (myalpha->elist, tnode_nthsubof (t, 0));
			if (mfe->curalpha) {
				mcsp_mergealpha (mfe->curalpha, myalpha);
			}
			mcsp_freealpha (myalpha);

			return 0;
			/*}}}*/
		}
		break;
	}

	return 1;
}
/*}}}*/
/*{{{  static int mcsp_namemap_vardeclnode (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	does name-mapping for a vardeclnodes
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_namemap_vardeclnode (compops_t *cops, tnode_t **node, map_t *map)
{
	tnode_t **namep = tnode_nthsubaddr (*node, 0);
	tnode_t *type = tnode_nthsubof (*node, 1);
	tnode_t **bodyp = tnode_nthsubaddr (*node, 3);
	tnode_t *bename;

	int tsize;
	int wssize, vssize, mssize, indir;

#if 0
fprintf (stderr, "mcsp_namemap_vardeclnode(): here!  target is [%s].  Type is:\n", map->target->name);
tnode_dumptree (type, 1, stderr);
#endif

	/* see how big this type is */
	tsize = tnode_bytesfor (type, map->target);

	if (type->tag->ndef->lops && tnode_haslangop (type->tag->ndef->lops, "initsizes") && tnode_calllangop (type->tag->ndef->lops, "initsizes", 7, type, *node, &wssize, &vssize, &mssize, &indir, map)) {
		/* some declarations will need special allocation (e.g. in vectorspace and/or mobilespace) -- collected above */
	} else {
		wssize = tsize;
		vssize = 0;
		mssize = 0;
		indir = 0;
	}
	bename = map->target->newname (*namep, *bodyp, map, (wssize < 0) ? 0 : wssize, (wssize < 0) ? -wssize : 0, vssize, mssize, tsize, indir);

	if (type->tag->ndef->lops && tnode_haslangop (type->tag->ndef->lops, "initialising_decl")) {
		tnode_calllangop (type->tag->ndef->lops, "initialising_decl", 3, type, bename, map);
	}

	tnode_setchook (*namep, map->mapchook, (void *)bename);
#if 0
fprintf (stderr, "got new bename:\n");
tnode_dumptree (bename, 1, stderr);
#endif

	*node = bename;
	bodyp = tnode_nthsubaddr (*node, 1);

	map_submapnames (bodyp, map);			/* map body */
	
	return 0;
}
/*}}}*/
/*{{{  static int mcsp_mwsynctrans_vardeclnode (compops_t *cops, tnode_t **node, mwsynctrans_t *mwi)*/
/*
 *	does multiway sync transforms for a vardeclnode
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_mwsynctrans_vardeclnode (compops_t *cops, tnode_t **node, mwsynctrans_t *mwi)
{
	tnode_t *t = *node;
	tnode_t *name = tnode_nthsubof (t, 0);
	tnode_t *var_to_remove = NULL;
	tnode_t **typep = tnode_nthsubaddr (t, 1);
	tnode_t **bodyp = tnode_nthsubaddr (t, 3);

	if ((name->tag == mcsp.tag_EVENT) && (NameTypeOf (tnode_nthnameof (name, 0))->tag == mcsp.tag_EVENTTYPE)) {
		mwsync_transsubtree (typep, mwi);         /* transform type */
		SetNameType (tnode_nthnameof (name, 0), *typep);

		mwsync_mwsynctrans_pushvar (*node, name, &bodyp, mcsp.tag_EVENT, mwi);
		var_to_remove = *node;
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


/*{{{  static int mcsp_decl_init_nodes (void)*/
/*
 *	initialises MCSP declaration nodes
 *	returns 0 on success, non-zero on failure
 */
static int mcsp_decl_init_nodes (void)
{
	tndef_t *tnd;
	int i;
	compops_t *cops;
	langops_t *lops;

	/*{{{  mcsp:scopenode -- HIDE, FIXPOINT*/
	i = -1;
	tnd = mcsp.node_SCOPENODE = tnode_newnodetype ("mcsp:scopenode", &i, 2, 0, 0, TNF_NONE);	/* subnodes: 0 = vars, 1 = process */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "prescope", 2, COMPOPTYPE (mcsp_prescope_scopenode));
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (mcsp_scopein_scopenode));
	tnode_setcompop (cops, "scopeout", 2, COMPOPTYPE (mcsp_scopeout_scopenode));
	tnode_setcompop (cops, "postcheck", 2, COMPOPTYPE (mcsp_postcheck_scopenode));
	tnode_setcompop (cops, "fetrans", 2, COMPOPTYPE (mcsp_fetrans_scopenode));
	tnd->ops = cops;

	i = -1;
	mcsp.tag_HIDE = tnode_newnodetag ("MCSPHIDE", &i, tnd, NTF_NONE);
	i = -1;
	mcsp.tag_FIXPOINT = tnode_newnodetag ("MCSPFIXPOINT", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  mcsp:declnode -- PROCDECL*/
	i = -1;
	tnd = tnode_newnodetype ("mcsp:declnode", &i, 4, 0, 0, TNF_LONGDECL);				/* subnodes: 0 = name, 1 = params, 2 = body, 3 = in-scope-body */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "prescope", 2, COMPOPTYPE (mcsp_prescope_declnode));
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (mcsp_scopein_declnode));
	tnode_setcompop (cops, "scopeout", 2, COMPOPTYPE (mcsp_scopeout_declnode));
	tnode_setcompop (cops, "fetrans", 2, COMPOPTYPE (mcsp_fetrans_declnode));
	tnode_setcompop (cops, "mwsynctrans", 2, COMPOPTYPE (mcsp_mwsynctrans_declnode));
	tnode_setcompop (cops, "betrans", 2, COMPOPTYPE (mcsp_betrans_declnode));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (mcsp_namemap_declnode));
	tnode_setcompop (cops, "precode", 2, COMPOPTYPE (mcsp_precode_declnode));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (mcsp_codegen_declnode));
	tnd->ops = cops;

	i = -1;
	mcsp.tag_PROCDECL = tnode_newnodetag ("MCSPPROCDECL", &i, tnd, NTF_NONE);
	i = -1;
	mcsp.tag_XPROCDECL = tnode_newnodetag ("MCSPXPROCDECL", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  mcsp:hiddennode -- HIDDENPARAM*/
	i = -1;
	tnd = tnode_newnodetype ("mcsp:hiddennode", &i, 1, 0, 0, TNF_NONE);				/* subnodes: 0 = hidden-param */

	i = -1;
	mcsp.tag_HIDDENPARAM = tnode_newnodetag ("MCSPHIDDENPARAM", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  mcsp:leafnode -- RETURNADDRESS*/
	i = -1;
	tnd = tnode_lookupornewnodetype ("mcsp:leafnode", &i, 0, 0, 0, TNF_NONE);

	i = -1;
	mcsp.tag_RETURNADDRESS = tnode_newnodetag ("MCSPRETURNADDRESS", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  mcsp:spacenode -- FPARAM, UPARAM*/
	/* this is used in front of formal parameters and local variables*/
	i = -1;
	tnd = mcsp.node_SPACENODE = tnode_newnodetype ("mcsp:spacenode", &i, 2, 0, 0, TNF_NONE);	/* subnodes: 0 = namenode/name, 1 = type */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (mcsp_scopein_spacenode));
	tnode_setcompop (cops, "scopeout", 2, COMPOPTYPE (mcsp_scopeout_spacenode));
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (mcsp_typecheck_spacenode));
	tnode_setcompop (cops, "postcheck", 2, COMPOPTYPE (mcsp_postcheck_spacenode));
	tnode_setcompop (cops, "fetrans", 2, COMPOPTYPE (mcsp_fetrans_spacenode));
	tnode_setcompop (cops, "mwsynctrans", 2, COMPOPTYPE (mcsp_mwsynctrans_spacenode));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (mcsp_namemap_spacenode));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (mcsp_codegen_spacenode));
	tnd->ops = cops;

	i = -1;
	mcsp.tag_FPARAM = tnode_newnodetag ("MCSPFPARAM", &i, tnd, NTF_NONE);
	i = -1;
	mcsp.tag_UPARAM = tnode_newnodetag ("MCSPUPARAM", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  mcsp:vardeclnode -- VARDECL*/
	i = -1;
	tnd = tnode_newnodetype ("mcsp:vardeclnode", &i, 4, 0, 0, TNF_SHORTDECL);			/* subnodes: 0 = name, 1 = type, 2 = initialiser, 3 = body */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (mcsp_namemap_vardeclnode));
	tnode_setcompop (cops, "fetrans", 2, COMPOPTYPE (mcsp_fetrans_vardeclnode));
	tnode_setcompop (cops, "mwsynctrans", 2, COMPOPTYPE (mcsp_mwsynctrans_vardeclnode));
	tnd->ops = cops;

	i = -1;
	mcsp.tag_VARDECL = tnode_newnodetag ("MCSPVARDECL", &i, tnd, NTF_NONE);

	/*}}}*/

	return 0;
}
/*}}}*/



/*{{{  mcsp_decl_feunit (feunit_t)*/
feunit_t mcsp_decl_feunit = {
	.init_nodes = mcsp_decl_init_nodes,
	.reg_reducers = NULL,
	.init_dfatrans = NULL,
	.post_setup = NULL,
	.ident = "mcsp-decl"
};

/*}}}*/

