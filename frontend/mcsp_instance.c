/*
 *	mcsp_instance.c -- instance handling for MCSP
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
#include "postcheck.h"
#include "fetrans.h"
#include "betrans.h"
#include "map.h"
#include "codegen.h"
#include "target.h"
#include "transputer.h"


/*}}}*/


/*{{{  static int mcsp_prescope_instancenode (compops_t *cops, tnode_t **node, prescope_t *ps)*/
/*
 *	called to pre-scope an instance node
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_prescope_instancenode (compops_t *cops, tnode_t **node, prescope_t *ps)
{
	tnode_t **paramsptr = tnode_nthsubaddr (*node, 1);

	if (!*paramsptr) {
		/* empty-list */
		*paramsptr = parser_newlistnode (NULL);
	} else if (!parser_islistnode (*paramsptr)) {
		/* singleton */
		tnode_t *list = parser_newlistnode (NULL);

		parser_addtolist (list, *paramsptr);
		*paramsptr = list;
	}
	return 1;
}
/*}}}*/
/*{{{  static int mcsp_scopein_instancenode (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	called to scope-in an instance node
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_scopein_instancenode (compops_t *cops, tnode_t **node, scope_t *ss)
{
	mcsp_scope_t *mss = (mcsp_scope_t *)ss->langpriv;

	mss->inamescope = 1;
	tnode_modprepostwalktree (tnode_nthsubaddr (*node, 0), scope_modprewalktree, scope_modpostwalktree, (void *)ss);
	mss->inamescope = 0;
	tnode_modprepostwalktree (tnode_nthsubaddr (*node, 1), scope_modprewalktree, scope_modpostwalktree, (void *)ss);

	return 0;
}
/*}}}*/
/*{{{  static int mcsp_typecheck_instancenode (compops_t *cops, tnode_t *node, typecheck_t *tc)*/
/*
 *	does type-checking on an instancenode
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_typecheck_instancenode (compops_t *cops, tnode_t *node, typecheck_t *tc)
{
	tnode_t *name = tnode_nthsubof (node, 0);
	tnode_t *aparams = tnode_nthsubof (node, 1);
	name_t *pname;
	tnode_t *fparams;
	tnode_t **fplist, **aplist;
	int nfp, nap;
	int i;

	if (!name || (name->tag->ndef != mcsp.node_NAMENODE)) {
		typecheck_error (node, tc, "instanced object is not a name");
		return 0;
	} else if (name->tag != mcsp.tag_PROCDEF) {
		typecheck_error (node, tc, "called name is not a process");
		return 0;
	}
	pname = tnode_nthnameof (name, 0);
	fparams = NameTypeOf (pname);

#if 0
fprintf (stderr, "mcsp_typecheck_instancenode(): fparams = \n");
tnode_dumptree (fparams, 1, stderr);
fprintf (stderr, "mcsp_typecheck_instancenode(): aparams = \n");
tnode_dumptree (aparams, 1, stderr);
#endif
	fplist = parser_getlistitems (fparams, &nfp);
	aplist = parser_getlistitems (aparams, &nap);

	if (nap > nfp) {
		/* must be wrong */
		typecheck_error (node, tc, "too many actual parameters");
		return 0;
	}

	/* parameters up to a certain point must match */
	for (i = 0; (i < nfp) && (i < nap); i++) {
		if (fplist[i]->tag == mcsp.tag_FPARAM) {
			/* must have an actual event */
			if (aplist[i]->tag != mcsp.tag_EVENT) {
				typecheck_error (node, tc, "parameter %d is not an event", i+1);
				/* keep going.. */
			}
		} else if (fplist[i]->tag == mcsp.tag_UPARAM) {
			/* should not have a matching actual.. */
			typecheck_error (node, tc, "too many actual parameters");
			return 0;
		} else {
			nocc_internal ("mcsp_typecheck_instancenode(): formal parameter (1) is [%s]", fplist[i]->tag->name);
			return 0;
		}
	}
	/* any remaining formals must be UPARAMs */
	for (; i<nfp; i++) {
		if (fplist[i]->tag == mcsp.tag_FPARAM) {
			/* should have had an actual */
			typecheck_error (node, tc, "too few actual parameters");
			return 0;
		} else if (fplist[i]->tag != mcsp.tag_UPARAM) {
			nocc_internal ("mcsp_typecheck_instancenode(): formal parameter (2) is [%s]", fplist[i]->tag->name);
			return 0;
		}
	}

	/* all good :) */

	return 1;
}
/*}}}*/
/*{{{  static int mcsp_postcheck_instancenode (compops_t *cops, tnode_t **node, postcheck_t *pc)*/
/*
 *	does post-check transforms on an instancenode
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_postcheck_instancenode (compops_t *cops, tnode_t **node, postcheck_t *pc)
{
	return 0;
}
/*}}}*/
/*{{{  static int mcsp_fetrans_instancenode (compops_t *cops, tnode_t **node, fetrans_t *fe)*/
/*
 *	does front-end transforms on an instancenode
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_fetrans_instancenode (compops_t *cops, tnode_t **node, fetrans_t *fe)
{
	mcsp_fetrans_t *mfe = (mcsp_fetrans_t *)fe->langpriv;
	tnode_t *t = *node;

	switch (mfe->parse) {
	case 0:
		if (t->tag == mcsp.tag_INSTANCE) {
			/*{{{  add any missing actual parameters to UPARAM formals*/
			tnode_t *aparams = tnode_nthsubof (t, 1);
			tnode_t *iname = tnode_nthsubof (t, 0);
			name_t *pname = tnode_nthnameof (iname, 0);
			tnode_t *fparams = NameTypeOf (pname);
			tnode_t **fplist, **aplist;
			int nfp, nap;

			fplist = parser_getlistitems (fparams, &nfp);
			aplist = parser_getlistitems (aparams, &nap);

#if 0
fprintf (stderr, "mcsp_fetrans_instancenode(): fparams = \n");
tnode_dumptree (fparams, 1, stderr);
fprintf (stderr, "mcsp_fetrans_instancenode(): aparams = \n");
tnode_dumptree (aparams, 1, stderr);
#endif
			if (nap < nfp) {
				/* need to add some missing actuals */
				if (!mfe->uvinsertlist) {
					nocc_internal ("mcsp_fetrans_instancenode(): need to add %d hidden actuals, but no insertlist!", nfp - nap);
					return 0;
				} else {
					int i;

					for (i=nap; i<nfp; i++) {
						/*{{{  add the name manually*/
						tnode_t *decl = tnode_create (mcsp.tag_UPARAM, NULL, NULL);
						tnode_t *newname;
						name_t *sname;
						char *rawname = (char *)smalloc (128);

						sprintf (rawname, "%s.%s", NameNameOf (pname), NameNameOf (tnode_nthnameof (tnode_nthsubof (fplist[i], 0), 0)));
						sname = name_addname (rawname, decl, NULL, NULL);
						sfree (rawname);

						parser_addtolist (mfe->uvinsertlist, decl);
						newname = tnode_createfrom (mcsp.tag_EVENT, decl, sname);
						SetNameNode (sname, newname);
						tnode_setnthsub (decl, 0, newname);

						parser_addtolist (aparams, newname);
						/*}}}*/
					}
				}
			}

			return 0;
			/*}}}*/
		}
		break;
	case 1:
		/* nothing in this pass */
		break;
	case 2:
		if (t->tag == mcsp.tag_INSTANCE) {
			/*{{{  add any events in actual parameters to alphabet*/
			if (mfe->curalpha) {
				tnode_t *aparams = tnode_nthsubof (t, 1);
				tnode_t **aplist;
				int nap, i;

				aplist = parser_getlistitems (aparams, &nap);
				for (i=0; i<nap; i++) {
					if (aplist[i] && (aplist[i]->tag == mcsp.tag_EVENT)) {
						mcsp_addtoalpha (mfe->curalpha, aplist[i]);
					}
				}
			}
			/*}}}*/
		}
		break;
	}

	return 1;
}
/*}}}*/
/*{{{  static int mcsp_namemap_instancenode (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	does name-mapping for an instancenode
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_namemap_instancenode (compops_t *cops, tnode_t **node, map_t *map)
{
	tnode_t *bename, *ibody, *namenode;

	/* map parameters and called name */
	map_submapnames (tnode_nthsubaddr (*node, 0), map);
	map_submapnames (tnode_nthsubaddr (*node, 1), map);

	namenode = tnode_nthsubof (*node, 0);
	if (namenode->tag == mcsp.tag_PROCDEF) {
		tnode_t *instance;
		name_t *name;

		name = tnode_nthnameof (namenode, 0);
		instance = NameDeclOf (name);

#if 0
		nocc_message ("mapping instance of PROCDEF, instance of:");
		tnode_dumptree (instance, 1, stderr);
#endif
		/* body should be a back-end block */
		ibody = tnode_nthsubof (instance, 2);
	} else {
		nocc_internal ("mcsp_namemap_instancenode(): don\'t know how to handle [%s]", namenode->tag->name);
		return 0;
	}

	bename = map->target->newblockref (ibody, *node, map);
	*node = bename;

	return 0;
}
/*}}}*/
/*{{{  static int mcsp_codegen_instancenode (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	does code-generation for an instancenode
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_codegen_instancenode (compops_t *cops, tnode_t *node, codegen_t *cgen)
{
	tnode_t *namenode = tnode_nthsubof (node, 0);
	tnode_t *params = tnode_nthsubof (node, 1);

	if (namenode->tag == mcsp.tag_PROCDEF) {
		int ws_size, ws_offset, vs_size, ms_size, adjust;
		name_t *name = tnode_nthnameof (namenode, 0);
		tnode_t *instance = NameDeclOf (name);
		tnode_t *ibody = tnode_nthsubof (instance, 2);

		codegen_check_beblock (ibody, cgen, 1);

		/* get the size of the block */
		cgen->target->be_getblocksize (ibody, &ws_size, &ws_offset, &vs_size, &ms_size, &adjust, NULL);

		if (!parser_islistnode (params)) {
			nocc_internal ("mcsp_codegen_instancenode(): expected list of parameters, got [%s]", params ? params->tag->name : "(null)");
			return 0;
		} else {
			int nitems, i, wsoff;
			tnode_t **items = parser_getlistitems (params, &nitems);

			for (i=nitems - 1, wsoff = -4; i>=0; i--, wsoff -= 4) {
				codegen_callops (cgen, loadparam, items[i], PARAM_REF);
				codegen_callops (cgen, storelocal, wsoff);
			}
		}
		codegen_callops (cgen, callnamelabel, name, adjust);
	} else {
		nocc_internal ("mcsp_codegen_instancenode(): don\'t know how to handle [%s]", namenode->tag->name);
	}

	return 0;
}
/*}}}*/


/*{{{  static int mcsp_instance_init_nodes (void)*/
/*
 *	initialises MCSP instance nodes
 *	returns 0 on success, non-zero on failure
 */
static int mcsp_instance_init_nodes (void)
{
	tndef_t *tnd;
	int i;
	compops_t *cops;

	/*{{{  mcsp:instancenode -- INSTANCE*/
	i = -1;
	tnd = tnode_newnodetype ("mcsp:instancenode", &i, 2, 0, 0, TNF_NONE);				/* subnodes: 0 = name, 1 = parameters */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "prescope", 2, COMPOPTYPE (mcsp_prescope_instancenode));
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (mcsp_scopein_instancenode));
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (mcsp_typecheck_instancenode));
	tnode_setcompop (cops, "postcheck", 2, COMPOPTYPE (mcsp_postcheck_instancenode));
	tnode_setcompop (cops, "fetrans", 2, COMPOPTYPE (mcsp_fetrans_instancenode));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (mcsp_namemap_instancenode));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (mcsp_codegen_instancenode));
	tnd->ops = cops;

	i = -1;
	mcsp.tag_INSTANCE = tnode_newnodetag ("MCSPINSTANCE", &i, tnd, NTF_NONE);

	/*}}}*/

	return 0;
}
/*}}}*/


/*{{{  mcsp_instance_feunit (feunit_t)*/
feunit_t mcsp_instance_feunit = {
	init_nodes: mcsp_instance_init_nodes,
	reg_reducers: NULL,
	init_dfatrans: NULL,
	post_setup: NULL,
	ident: "mcsp-instance"
};
/*}}}*/

