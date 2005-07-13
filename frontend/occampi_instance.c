/*
 *	occampi_instance.c -- instance (PROC calls, etc.) handling for occampi
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
#include "typecheck.h"
#include "map.h"
#include "target.h"
#include "transputer.h"
#include "codegen.h"

/*}}}*/


/*{{{  static int occampi_typecheck_instance (tnode_t *node, typecheck_t *tc)*/
/*
 *	does type-checking for an instance-node
 *	returns 1 to continue walk, 0 to stop
 */
static int occampi_typecheck_instance (tnode_t *node, typecheck_t *tc)
{
	tnode_t *fparamlist = typecheck_gettype (tnode_nthsubof (node, 0), NULL);
	tnode_t *aparamlist = tnode_nthsubof (node, 1);
	tnode_t **fp_items, **ap_items;
	int fp_nitems, ap_nitems;
	int fp_ptr, ap_ptr;
	int paramno;

#if 0
fprintf (stderr, "occampi_typecheck_instance: fparamlist=\n");
tnode_dumptree (fparamlist, 1, stderr);
fprintf (stderr, "occampi_typecheck_instance: aparamlist=\n");
tnode_dumptree (aparamlist, 1, stderr);
#endif

	if (parser_islistnode (fparamlist)) {
		fp_items = parser_getlistitems (fparamlist, &fp_nitems);
	} else {
		fp_items = &fparamlist;
		fp_nitems = 1;
	}
	if (parser_islistnode (aparamlist)) {
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
fprintf (stderr, "occampi_typecheck_instance: ftype=\n");
tnode_dumptree (ftype, 1, stderr);
fprintf (stderr, "occampi_typecheck_instance: atype=\n");
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
/*{{{  static int occampi_namemap_instance (tnode_t **node, map_t *map)*/
/*
 *	does name-mapping for an instance-node
 *	returns 1 to continue walk, 0 to stop
 */
static int occampi_namemap_instance (tnode_t **node, map_t *map)
{
	tnode_t *bename, *instance, *ibody;
	name_t *name;

	/* map parameters and called name */
	map_submapnames (tnode_nthsubaddr (*node, 1), map);
	map_submapnames (tnode_nthsubaddr (*node, 0), map);

#if 0
fprintf (stderr, "occampi_namemap_instance: instance of:\n");
tnode_dumptree (tnode_nthsubof (*node, 0), 1, stderr);
#endif
	name = tnode_nthnameof (tnode_nthsubof (*node, 0), 0);
	instance = NameDeclOf (name);

	/* body should be a back-end BLOCK */
	ibody = tnode_nthsubof (instance, 2);

#if 0
fprintf (stderr, "occampi_namemap_instance: instance body is:\n");
tnode_dumptree (ibody, 1, stderr);
#endif

	bename = map->target->newblockref (ibody, *node, map);
#if 0
fprintf (stderr, "occampi_namemap_instance: created new blockref =\n");
tnode_dumptree (bename, 1, stderr);
#endif

	*node = bename;
	return 0;
}
/*}}}*/
/*{{{  static int occampi_codegen_instance (tnode_t *node, codegen_t *cgen)*/
/*
 *	generates code for an instance
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_codegen_instance (tnode_t *node, codegen_t *cgen)
{
	name_t *name = tnode_nthnameof (tnode_nthsubof (node, 0), 0);
	tnode_t *params = tnode_nthsubof (node, 1);
	tnode_t *instance, *ibody;
	int ws_size, ws_offset, vs_size, ms_size, adjust;

	instance = NameDeclOf (name);
	ibody = tnode_nthsubof (instance, 2);

	codegen_check_beblock (ibody, cgen, 1);

	/* get size of this block */
	cgen->target->be_getblocksize (ibody, &ws_size, &ws_offset, &vs_size, &ms_size, &adjust);

	/* FIXME: load parameters in reverse order, into -4, -8, ... */
	if (parser_islistnode (params)) {
		int nitems, i, wsoff;
		tnode_t **items = parser_getlistitems (params, &nitems);

		for (i=nitems - 1, wsoff = -4; i>=0; i--, wsoff += 4) {
			codegen_callops (cgen, loadparam, items[i], PARAM_REF);
			codegen_callops (cgen, storelocal, wsoff);
		}
	} else {
		/* single parameter */
		codegen_callops (cgen, loadparam, params, PARAM_REF);
		codegen_callops (cgen, storelocal, -4);
	}

	codegen_callops (cgen, callnamedlabel, NameNameOf (name), adjust);

	return 0;
}
/*}}}*/


/*{{{  static int occampi_instance_init_nodes (void)*/
/*
 *	initialises nodes for occam-pi instances
 */
static int occampi_instance_init_nodes (void)
{
	tndef_t *tnd;
	int i;
	compops_t *cops;

	/*{{{  occampi:instancenode -- PINSTANCE, FINSTANCE*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:instancenode", &i, 2, 0, 1, TNF_NONE);
	cops = tnode_newcompops ();
	cops->typecheck = occampi_typecheck_instance;
	cops->namemap = occampi_namemap_instance;
	cops->codegen = occampi_codegen_instance;
	tnd->ops = cops;

	i = -1;
	opi.tag_PINSTANCE = tnode_newnodetag ("PINSTANCE", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_FINSTANCE = tnode_newnodetag ("FINSTANCE", &i, tnd, NTF_NONE);

	/*}}}*/

	return 0;
}
/*}}}*/
/*{{{  static int occampi_instance_reg_reducers (void)*/
/*
 *	registers reducers for instance nodes
 */
static int occampi_instance_reg_reducers (void)
{
	/* FIXME: ... */
	return 0;
}
/*}}}*/
/*{{{  static dfattbl_t **occampi_instance_init_dfatrans (int *ntrans)*/
/*
 *	creates and returns DFA transition tables for instance nodes
 */
static dfattbl_t **occampi_instance_init_dfatrans (int *ntrans)
{
	DYNARRAY (dfattbl_t *, transtbl);

	dynarray_init (transtbl);
	/* FIXME: ... */

	*ntrans = DA_CUR (transtbl);
	return DA_PTR (transtbl);
}
/*}}}*/


/*{{{  occampi_instance_feunit (feunit_t)*/
feunit_t occampi_instance_feunit = {
	init_nodes: occampi_instance_init_nodes,
	reg_reducers: occampi_instance_reg_reducers,
	init_dfatrans: occampi_instance_init_dfatrans,
	post_setup: NULL
};
/*}}}*/

