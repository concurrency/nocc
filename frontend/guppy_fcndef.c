/*
 *	guppy_fcndef.c -- Guppy procedure/function declarations for NOCC
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


/*}}}*/
/*{{{  private data*/

static compop_t *inparams_scopein_compop = NULL;
static compop_t *inparams_scopeout_compop = NULL;
static compop_t *inparams_namemap_compop = NULL;
static compop_t *inparams_lnamemap_compop = NULL;


/*}}}*/


/*{{{  static int guppy_prescope_fcndef (compops_t *cops, tnode_t **node, prescope_t *ps)*/
/*
 *	called to pre-scope a procedure definition
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_prescope_fcndef (compops_t *cops, tnode_t **node, prescope_t *ps)
{
	guppy_prescope_t *gps = (guppy_prescope_t *)ps->hook;
	char *rawname = (char *)tnode_nthhookof (tnode_nthsubof (*node, 0), 0);

	if (!gps->procdepth) {
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

	/* prescope params */
	prescope_subtree (tnode_nthsubaddr (*node, 1), ps);

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
	tnode_t **bodyptr = tnode_nthsubaddr (*node, 2);
	chook_t *nnschook = library_getnonamespacechook ();
	void *nsmark;
	char *rawname;
	name_t *fcnname;
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

	/* if there is a corresponding scope-out, call that here (before parameters go out of scope!) */
	if (tnode_hascompop (cops, "inparams_scopeout")) {
		tnode_callcompop (cops, "inparams_scopeout", 2, node, ss);
	}

	name_markdescope (nsmark);

	if (nnschook && tnode_haschook (*node, nnschook)) {
		nnsnode = (tnode_t *)tnode_getchook (*node, nnschook);
	}

	/* declare and scope PROC name, then check process in the scope of it */
	rawname = tnode_nthhookof (name, 0);

	fcnname = name_addscopenamess (rawname, *node, *paramsptr, NULL, nnsnode ? NULL : ss);
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
	tnode_t *name = tnode_nthsubof (*node, 0);
	name_t *sname;

	if (name->tag != gup.tag_NFCNDEF) {
		scope_error (name, ss, "not NFCNDEF!");
		return 0;
	}
	sname = tnode_nthnameof (name, 0);

	name_descopename (sname);

	return 1;
}
/*}}}*/
/*{{{  static int guppy_namemap_fcndef (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	called to name-map a function/procedure definition
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_namemap_fcndef (compops_t *cops, tnode_t **node, map_t *map)
{
	tnode_t *body = tnode_nthsubof (*node, 2);

#if 1
fprintf (stderr, "guppy_namemap_fcndef(): here!\n");
#endif
	if (body->tag == gup.tag_DECLBLOCK) {
		/* generates a block of its own ultimately */
		map_submapnames (tnode_nthsubaddr (*node, 2), map);
	} else {
		tnode_t *blk;
		tnode_t **paramsptr = tnode_nthsubaddr (*node, 1);

		blk = map->target->newblock (body, map, *paramsptr, map->lexlevel + 1);
		map_pushlexlevel (map, blk, paramsptr);

		map_submapnames (tnode_nthsubaddr (blk, 0), map);					/* do under back-end block */

		map_poplexlevel (map);
		tnode_setnthsub (*node, 2, blk);			/* insert BLOCK before process body */
	}
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

#if 1
fprintf (stderr, "guppy_codegen_fcndef(): here!\n");
#endif
	pname = tnode_nthnameof (name, 0);
	codegen_callops (cgen, comment, "define %s", pname->me->name);
	codegen_callops (cgen, c_procentry, pname, tnode_nthsubof (node, 1));

	/* then code the body */
	codegen_subcodegen (body, cgen);

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

	/*{{{  guppy:fcndef -- FCNDEF*/
	i = -1;
	tnd = tnode_newnodetype ("guppy:fcndef", &i, 3, 0, 0, TNF_LONGDECL);			/* subnodes: name; fparams; body */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "prescope", 2, COMPOPTYPE (guppy_prescope_fcndef));
	tnode_setcompop (cops, "declify", 2, COMPOPTYPE (guppy_declify_fcndef));
	tnode_setcompop (cops, "autoseq", 2, COMPOPTYPE (guppy_autoseq_fcndef));
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (guppy_scopein_fcndef));
	tnode_setcompop (cops, "scopeout", 2, COMPOPTYPE (guppy_scopeout_fcndef));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (guppy_namemap_fcndef));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (guppy_codegen_fcndef));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "getdescriptor", 2, LANGOPTYPE (guppy_getdescriptor_fcndef));
	tnd->lops = lops;

	i = -1;
	gup.tag_FCNDEF = tnode_newnodetag ("FCNDEF", &i, tnd, NTF_INDENTED_PROC_LIST);

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

