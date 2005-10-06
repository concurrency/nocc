/*
 *	occampi_function.c -- occam-pi FUNCTIONs
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
#include "dfa.h"
#include "parsepriv.h"
#include "occampi.h"
#include "feunit.h"
#include "names.h"
#include "scope.h"
#include "prescope.h"
#include "precheck.h"
#include "typecheck.h"
#include "usagecheck.h"
#include "map.h"
#include "target.h"
#include "transputer.h"
#include "codegen.h"
#include "langops.h"


/*}}}*/


/*{{{  static int occampi_typecheck_finstance (tnode_t *node, typecheck_t *tc)*/
/*
 *	does type-checking for a function instance-node
 *	returns 1 to continue walk, 0 to stop
 */
static int occampi_typecheck_finstance (tnode_t *node, typecheck_t *tc)
{
	tnode_t *fname = tnode_nthsubof (node, 0);
	tnode_t *functype;
	tnode_t *fparamlist;
	tnode_t *aparamlist;
	tnode_t **fp_items, **ap_items;
	int fp_nitems, ap_nitems;
	int fp_ptr, ap_ptr;
	int paramno;

	if (fname->tag != opi.tag_NFUNCDEF) {
		typecheck_error (node, tc, "called name is not a function");
	}

	functype = typecheck_gettype (fname, NULL);
	fparamlist = tnode_nthsubof (functype, 1);
	aparamlist = tnode_nthsubof (node, 1);
#if 0
fprintf (stderr, "occampi_typecheck_finstance: fparamlist=\n");
tnode_dumptree (fparamlist, 1, stderr);
fprintf (stderr, "occampi_typecheck_finstance: aparamlist=\n");
tnode_dumptree (aparamlist, 1, stderr);
#endif

	if (!fparamlist) {
		fp_items = NULL;
		fp_nitems = 0;
	} else if (parser_islistnode (fparamlist)) {
		fp_items = parser_getlistitems (fparamlist, &fp_nitems);
	} else {
		fp_items = &fparamlist;
		fp_nitems = 1;
	}
	if (!aparamlist) {
		ap_items = NULL;
		ap_nitems = 0;
	} else if (parser_islistnode (aparamlist)) {
		ap_items = parser_getlistitems (aparamlist, &ap_nitems);
	} else {
		ap_items = &aparamlist;
		ap_nitems = 1;
	}

	for (paramno = 1, fp_ptr = 0, ap_ptr = 0; (fp_ptr < fp_nitems) && (ap_ptr < ap_nitems);) {
		tnode_t *ftype, *atype;

		/* skip over hidden parameters */
		if (fp_items[fp_ptr]->tag == opi.tag_HIDDENPARAM) {
			fp_ptr++;
			continue;
		}
		if (ap_items[ap_ptr]->tag == opi.tag_HIDDENPARAM) {
			ap_ptr++;
			continue;
		}
		ftype = typecheck_gettype (fp_items[fp_ptr], NULL);
		atype = typecheck_gettype (ap_items[ap_ptr], NULL);
#if 0
fprintf (stderr, "occampi_typecheck_finstance: ftype=\n");
tnode_dumptree (ftype, 1, stderr);
fprintf (stderr, "occampi_typecheck_finstance: atype=\n");
tnode_dumptree (atype, 1, stderr);
#endif

		if (!typecheck_typeactual (ftype, atype, node, tc)) {
			typecheck_error (node, tc, "incompatible types for parameter %d", paramno);
		}
		fp_ptr++;
		ap_ptr++;
		paramno++;
	}

	/* skip over any left-over hidden params */
	for (; (fp_ptr < fp_nitems) && (fp_items[fp_ptr]->tag == opi.tag_HIDDENPARAM); fp_ptr++);
	for (; (ap_ptr < ap_nitems) && (ap_items[ap_ptr]->tag == opi.tag_HIDDENPARAM); ap_ptr++);
	if (fp_ptr < fp_nitems) {
		typecheck_error (node, tc, "too few actual parameters");
	} else if (ap_ptr < ap_nitems) {
		typecheck_error (node, tc, "too many actual parameters");
	}

	return 0;
}
/*}}}*/
/*{{{  static int occampi_namemap_finstance (tnode_t **node, map_t *map)*/
/*
 *	does name-mapping for a function instance-node
 *	returns 1 to continue walk, 0 to stop
 */
static int occampi_namemap_finstance (tnode_t **node, map_t *map)
{
	tnode_t *bename, *finstance, *ibody, *namenode;
	name_t *name;

	/* map parameters and called name */
	map_submapnames (tnode_nthsubaddr (*node, 1), map);
	map_submapnames (tnode_nthsubaddr (*node, 0), map);

	namenode = tnode_nthsubof (*node, 0);
#if 0
fprintf (stderr, "occampi_namemap_finstance(): finstance of:\n");
tnode_dumptree (namenode, 1, stderr);
#endif
	if (namenode->tag->ndef == opi.node_NAMENODE) {
		name = tnode_nthnameof (namenode, 0);
		finstance = NameDeclOf (name);

		/* body should be a back-end BLOCK */
		ibody = tnode_nthsubof (finstance, 2);
	} else {
		nocc_internal ("occampi_namemap_finstance(): don\'t know how to handle [%s]", namenode->tag->name);
		return 0;
	}
#if 0
fprintf (stderr, "occampi_namemap_finstance: finstance body is:\n");
tnode_dumptree (ibody, 1, stderr);
#endif

	bename = map->target->newblockref (ibody, *node, map);
#if 0
fprintf (stderr, "occampi_namemap_finstance: created new blockref =\n");
tnode_dumptree (bename, 1, stderr);
#endif

	*node = bename;
	return 0;
}
/*}}}*/
/*{{{  static int occampi_codegen_finstance (tnode_t *node, codegen_t *cgen)*/
/*
 *	generates code for an finstance
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_codegen_finstance (tnode_t *node, codegen_t *cgen)
{
	/* FIXME! */
#if 0
	name_t *name;
	tnode_t *namenode;
	tnode_t *params = tnode_nthsubof (node, 1);
	tnode_t *finstance, *ibody;
	int ws_size, ws_offset, vs_size, ms_size, adjust;

	namenode = tnode_nthsubof (node, 0);

	if (namenode->tag->ndef == opi.node_NAMENODE) {
		/*{{{  finstance of a name (e.g. PROC definition)*/
		name = tnode_nthnameof (namenode, 0);
		finstance = NameDeclOf (name);
		ibody = tnode_nthsubof (finstance, 2);

		codegen_check_beblock (ibody, cgen, 1);

		/* get size of this block */
		cgen->target->be_getblocksize (ibody, &ws_size, &ws_offset, &vs_size, &ms_size, &adjust, NULL);

		/* FIXME: load parameters in reverse order, into -4, -8, ... */
		if (!params) {
			/* no parameters! */
		} else if (parser_islistnode (params)) {
			int nitems, i, wsoff;
			tnode_t **items = parser_getlistitems (params, &nitems);

			for (i=nitems - 1, wsoff = -4; i>=0; i--, wsoff -= 4) {
				codegen_callops (cgen, loadparam, items[i], PARAM_REF);
				codegen_callops (cgen, storelocal, wsoff);
			}
		} else {
			/* single parameter */
			codegen_callops (cgen, loadparam, params, PARAM_REF);
			codegen_callops (cgen, storelocal, -4);
		}

		codegen_callops (cgen, callnamedlabel, NameNameOf (name), adjust);
		/*}}}*/
	} else if (namenode->tag == opi.tag_BUILTINPROC) {
		/*{{{  finstance of a built-in PROC*/
		builtinprochook_t *bph = (builtinprochook_t *)tnode_nthhookof (namenode, 0);
		builtinproc_t *builtin = bph->biptr;

		if (builtin->codegen) {
			builtin->codegen (node, builtin, cgen);
		} else {
			nocc_warning ("occampi_codegen_finstance(): don\'t know how to code for built-in PROC [%s]", builtin->name);
			codegen_callops (cgen, comment, "BUILTINPROC finstance of [%s]", builtin->name);
		}
		/*}}}*/
	} else {
		nocc_internal ("occampi_codegen_finstance(): don\'t know how to handle [%s]", namenode->tag->name);
	}
#endif
	return 0;
}
/*}}}*/


/*{{{  static int occampi_prescope_funcdecl (tnode_t **node, prescope_t *ps)*/
/*
 *	called to prescope a long FUNCTION definition
 */
static int occampi_prescope_funcdecl (tnode_t **node, prescope_t *ps)
{
	occampi_prescope_t *ops = (occampi_prescope_t *)(ps->hook);
	tnode_t **fparamsp = tnode_nthsubaddr (tnode_nthsubof (*node, 1), 1);		/* LHS is return-type */

	ops->last_type = NULL;
	if (!*fparamsp) {
		/* no parameters, create empty list */
		*fparamsp = parser_newlistnode (NULL);
	} else if (*fparamsp && !parser_islistnode (*fparamsp)) {
		/* turn single parameter into a list-node */
		tnode_t *list = parser_newlistnode (NULL);

		parser_addtolist (list, *fparamsp);
		*fparamsp = list;
	}
	return 1;
}
/*}}}*/
/*{{{  static int occampi_scopein_funcdecl (tnode_t **node, scope_t *ss)*/
/*
 *	called to scope a FUNCTION definition
 */
static int occampi_scopein_funcdecl (tnode_t **node, scope_t *ss)
{
	tnode_t *name = tnode_nthsubof (*node, 0);
	tnode_t **paramsptr = tnode_nthsubaddr (tnode_nthsubof (*node, 1), 1);		/* LHS is return-type */
	tnode_t **bodyptr = tnode_nthsubaddr (*node, 2);
	void *nsmark;
	char *rawname;
	name_t *funcname;
	tnode_t *newname;

	nsmark = name_markscope ();

	/* walk parameters and body */
	tnode_modprepostwalktree (paramsptr, scope_modprewalktree, scope_modpostwalktree, (void *)ss);
	tnode_modprepostwalktree (bodyptr, scope_modprewalktree, scope_modpostwalktree, (void *)ss);

	name_markdescope (nsmark);

	/* declare and scope FUNCTION name, then check process in the scope of it */
	rawname = tnode_nthhookof (name, 0);
	funcname = name_addscopename (rawname, *node, tnode_nthsubof (*node, 1), NULL);
	newname = tnode_createfrom (opi.tag_NFUNCDEF, name, funcname);
	SetNameNode (funcname, newname);
	tnode_setnthsub (*node, 0, newname);

	/* free old name, check process */
	tnode_free (name);
	tnode_modprepostwalktree (tnode_nthsubaddr (*node, 3), scope_modprewalktree, scope_modpostwalktree, (void *)ss);
	ss->scoped++;

	return 0;		/* already walked child nodes */
}
/*}}}*/
/*{{{  static int occampi_scopeout_funcdecl (tnode_t **node, scope_t *ss)*/
/*
 *	called to scope a FUNCTION definition
 */
static int occampi_scopeout_funcdecl (tnode_t **node, scope_t *ss)
{
	tnode_t *name = tnode_nthsubof (*node, 0);
	name_t *sname;

	if (name->tag != opi.tag_NFUNCDEF) {
		scope_error (name, ss, "not NFUNCDEF!");
		return 0;
	}
	sname = tnode_nthnameof (name, 0);

	name_descopename (sname);

	return 1;
}
/*}}}*/
/*{{{  static tnode_t *occampi_gettype_funcdecl (tnode_t *node, tnode_t *defaulttype)*/
/*
 *	returns the type of a FUNCTION definition (= FUNCTIONTYPE(return-type, parameter-list))
 */
static tnode_t *occampi_gettype_funcdecl (tnode_t *node, tnode_t *defaulttype)
{
	return tnode_nthsubof (node, 1);
}
/*}}}*/
/*{{{  static int occampi_fetrans_funcdecl (tnode_t **node)*/
/*
 *	does front-end transforms on a FUNCTION definition
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_fetrans_funcdecl (tnode_t **node)
{
	chook_t *deschook = tnode_lookupchookbyname ("fetrans:descriptor");
	char *dstr = NULL;

	if (!deschook) {
		return 1;
	}
	langops_getdescriptor (*node, &dstr);
	if (dstr) {
		tnode_setchook (*node, deschook, (void *)dstr);
	}

	return 1;
}
/*}}}*/
/*{{{  static int occampi_precheck_funcdecl (tnode_t *node)*/
/*
 *	does pre-checking on FUNCTION declaration
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_precheck_funcdecl (tnode_t *node)
{
#if 0
fprintf (stderr, "occampi_precheck_funcdecl(): here!\n");
#endif
	precheck_subtree (tnode_nthsubof (node, 2));		/* precheck this body */
	precheck_subtree (tnode_nthsubof (node, 3));		/* precheck in-scope code */
#if 0
fprintf (stderr, "occampi_precheck_funcdecl(): returning 0\n");
#endif
	return 0;
}
/*}}}*/
/*{{{  static int occampi_usagecheck_funcdecl (tnode_t *node, uchk_state_t *ucstate)*/
/*
 *	does usage-checking on a FUNCTION declaration
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_usagecheck_funcdecl (tnode_t *node, uchk_state_t *ucstate)
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
/*{{{  static int occampi_namemap_funcdecl (tnode_t **node, map_t *map)*/
/*
 *	name-maps a FUNCTION definition
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_namemap_funcdecl (tnode_t **node, map_t *map)
{
	tnode_t *blk;
	tnode_t *saved_blk = map->thisblock;
	tnode_t **saved_params = map->thisprocparams;
	tnode_t **paramsptr;
	tnode_t *tmpname;

	blk = map->target->newblock (tnode_nthsubof (*node, 2), map, tnode_nthsubof (*node, 1), map->lexlevel + 1);
	map->thisblock = blk;
	map->thisprocparams = tnode_nthsubaddr (tnode_nthsubof (*node, 1), 1);
	map->lexlevel++;

	/* map formal params and body */
	paramsptr = map->thisprocparams;
	map_submapnames (paramsptr, map);
	map_submapnames (tnode_nthsubaddr (blk, 0), map);		/* do this under the back-end block */
	map->lexlevel--;
	map->thisblock = saved_blk;
	map->thisprocparams = saved_params;

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
/*{{{  static int occampi_codegen_funcdecl (tnode_t *node, codegen_t *cgen)*/
/*
 *	generates code for a FUNCTION definition
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_codegen_funcdecl (tnode_t *node, codegen_t *cgen)
{
	tnode_t *body = tnode_nthsubof (node, 2);
	tnode_t *name = tnode_nthsubof (node, 0);
	int ws_size, vs_size, ms_size;
	int ws_offset, adjust;
	name_t *pname;

	body = tnode_nthsubof (node, 2);
	cgen->target->be_getblocksize (body, &ws_size, &ws_offset, &vs_size, &ms_size, &adjust, NULL);

	pname = tnode_nthnameof (name, 0);
	codegen_callops (cgen, comment, "FUNCTION %s = %d,%d,%d,%d,%d", pname->me->name, ws_size, ws_offset, vs_size, ms_size, adjust);
	codegen_callops (cgen, setwssize, ws_size, adjust);
	codegen_callops (cgen, setvssize, vs_size);
	codegen_callops (cgen, setmssize, ms_size);
	codegen_callops (cgen, setnamedlabel, pname->me->name);

	/* adjust workspace and generate code for body */
	// codegen_callops (cgen, wsadjust, -(ws_offset - adjust));
	codegen_subcodegen (body, cgen);
	// codegen_callops (cgen, wsadjust, (ws_offset - adjust));

	/* return */
	codegen_callops (cgen, procreturn, adjust);

	/* generate code following declaration */
	codegen_subcodegen (tnode_nthsubof (node, 3), cgen);
#if 0
fprintf (stderr, "occampi_codegen_funcdecl!\n");
#endif
	return 0;
}
/*}}}*/
/*{{{  static int occampi_getdescriptor_funcdecl (tnode_t *node, char **str)*/
/*
 *	generates a descriptor line for a FUNCTION definition
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_getdescriptor_funcdecl (tnode_t *node, char **str)
{
	tnode_t *name = tnode_nthsubof (node, 0);
	char *realname;
	tnode_t *rtypes = tnode_nthsubof (tnode_nthsubof (node, 1), 0);
	tnode_t *params = tnode_nthsubof (tnode_nthsubof (node, 1), 1);

	if (*str) {
		/* shouldn't get this here, but.. */
		nocc_warning ("occampi_getdescriptor_funcdecl(): already had descriptor [%s]", *str);
		sfree (*str);
	}
	realname = NameNameOf (tnode_nthnameof (name, 0));
	*str = (char *)smalloc (10);

	strcpy (*str, "");

	if (parser_islistnode (rtypes)) {
		int nitems, i;
		tnode_t **items = parser_getlistitems (rtypes, &nitems);

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
	} else {
		langops_getdescriptor (rtypes, str);
	}

	{
		char *newstr = (char *)smalloc (strlen (*str) + strlen (realname) + 15);

		sprintf (newstr, "%s FUNCTION %s (", *str, realname);
		sfree (*str);
		*str = newstr;
	}

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


/*{{{  static int occampi_function_init_nodes (void)*/
/*
 *	sets up nodes for occam-pi functionators (monadic, dyadic)
 *	returns 0 on success, non-zero on error
 */
static int occampi_function_init_nodes (void)
{
	tndef_t *tnd;
	compops_t *cops;
	langops_t *lops;
	int i;

	/*{{{  occampi:finstancenode -- FINSTANCE*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:finstancenode", &i, 2, 0, 0, TNF_NONE);	/* subnodes: name; params */
	cops = tnode_newcompops ();
	cops->typecheck = occampi_typecheck_finstance;
	cops->namemap = occampi_namemap_finstance;
	cops->codegen = occampi_codegen_finstance;
	tnd->ops = cops;
	i = -1;
	opi.tag_FINSTANCE = tnode_newnodetag ("FINSTANCE", &i, tnd, NTF_NONE);
	/*}}}*/
	/*{{{  occampi:functiontype -- FUNCTIONTYPE*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:functiontype", &i, 2, 0, 0, TNF_NONE);	/* subnodes: return-type; fparams */
	cops = tnode_newcompops ();
	tnd->ops = cops;

	i = -1;
	opi.tag_FUNCTIONTYPE = tnode_newnodetag ("FUNCTIONTYPE", &i, tnd, NTF_NONE);
	/*}}}*/
	/*{{{  occampi:funcdecl -- FUNCDECL*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:funcdecl", &i, 4, 0, 0, TNF_LONGDECL);	/* subnodes: name; (return-type/fparams); body; in-scope-body */
	cops = tnode_newcompops ();
	cops->prescope = occampi_prescope_funcdecl;
	cops->scopein = occampi_scopein_funcdecl;
	cops->scopeout = occampi_scopeout_funcdecl;
	cops->namemap = occampi_namemap_funcdecl;
	cops->gettype = occampi_gettype_funcdecl;
	cops->precheck = occampi_precheck_funcdecl;
	cops->fetrans = occampi_fetrans_funcdecl;
	cops->codegen = occampi_codegen_funcdecl;
	tnd->ops = cops;
	lops = tnode_newlangops ();
	lops->getdescriptor = occampi_getdescriptor_funcdecl;
	tnd->lops = lops;

	i = -1;
	opi.tag_FUNCDECL = tnode_newnodetag ("FUNCDECL", &i, tnd, NTF_NONE);
	/*}}}*/
	/*{{{  occampi:shortfuncdecl -- SHORTFUNCDECL*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:shortfuncdecl", &i, 4, 0, 0, TNF_SHORTDECL);		/* subnodes: name; (return-type/fparams); in-scope-body; expr */
	cops = tnode_newcompops ();
#if 0
	cops->prescope = occampi_prescope_shortfuncdecl;
	cops->scopein = occampi_scopein_shortfuncdecl;
	cops->scopeout = occampi_scopeout_shortfuncdecl;
	cops->namemap = occampi_namemap_shortfuncdecl;
	cops->gettype = occampi_gettype_shortfuncdecl;
	cops->precheck = occampi_precheck_shortfuncdecl;
	cops->fetrans = occampi_fetrans_shortfuncdecl;
	cops->precode = occampi_precode_shortfuncdecl;
	cops->codegen = occampi_codegen_shortfuncdecl;
#endif
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnd->lops = lops;

	i = -1;
	opi.tag_SHORTFUNCDECL = tnode_newnodetag ("SHORTFUNCDECL", &i, tnd, NTF_NONE);
	/*}}}*/

	return 0;
}
/*}}}*/
/*{{{  static int occampi_function_reg_reducers (void)*/
/*
 *	registers reductions for occam-pi functionators
 *	returns 0 on success, non-zero on error
 */
static int occampi_function_reg_reducers (void)
{
	parser_register_grule ("opi:funcdefreduce", parser_decode_grule ("SN1N+N+N+>VC2V00C4R-", opi.tag_FUNCTIONTYPE, opi.tag_FUNCDECL));
	
	return 0;
}
/*}}}*/
/*{{{  static dfattbl_t **occampi_function_init_dfatrans (int *ntrans)*/
/*
 *	initialises and returns DFA transition tables for occam-pi functionators
 */
static dfattbl_t **occampi_function_init_dfatrans (int *ntrans)
{
	DYNARRAY (dfattbl_t *, transtbl);

	dynarray_init (transtbl);

	dynarray_add (transtbl, dfa_transtotbl ("occampi:fdeclstarttype ::= [ 0 @FUNCTION 1 ] [ 1 occampi:name 2 ] [ 2 @@( 3 ] [ 3 occampi:fparamlist 4 ] [ 4 @@) 5 ] [ 5 {<opi:funcdefreduce>} -* ]"));

	*ntrans = DA_CUR (transtbl);
	return DA_PTR (transtbl);
}
/*}}}*/
/*{{{  static int occampi_function_post_setup (void)*/
/*
 *	does post-setup for occam-pi functionator nodes
 *	returns 0 on success, non-zero on error
 */
static int occampi_function_post_setup (void)
{
	return 0;
}
/*}}}*/


/*{{{  occampi_function_feunit (feunit_t)*/
feunit_t occampi_function_feunit = {
	init_nodes: occampi_function_init_nodes,
	reg_reducers: occampi_function_reg_reducers,
	init_dfatrans: occampi_function_init_dfatrans,
	post_setup: occampi_function_post_setup
};
/*}}}*/


