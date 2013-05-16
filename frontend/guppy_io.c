/*
 *	guppy_io.c -- input and output for Guppy
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


/*}}}*/


/*{{{  static int guppy_typecheck_io (compops_t *cops, tnode_t *node, typecheck_t *tc)*/
/*
 *	does type-checking for an input or output
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_typecheck_io (compops_t *cops, tnode_t *node, typecheck_t *tc)
{
	tnode_t *lhstype, *rhstype, *acttype, *prot;

	typecheck_subtree (tnode_nthsubof (node, 0), tc);
	typecheck_subtree (tnode_nthsubof (node, 1), tc);

	lhstype = typecheck_gettype (tnode_nthsubof (node, 0), NULL);

	if (!lhstype) {
		typecheck_error (node, tc, "channel in input/output has unknown type");
		return 0;
	}

	prot = typecheck_getsubtype (lhstype, NULL);
	rhstype = typecheck_gettype (tnode_nthsubof (node, 1), prot);

#if 0
fprintf (stderr, "guppy_typecheck_io(): got lhstype = \n");
tnode_dumptree (lhstype, 1, FHAN_STDERR);
fprintf (stderr, "guppy_typecheck_io(): got rhstype = \n");
tnode_dumptree (rhstype, 1, FHAN_STDERR);
fprintf (stderr, "guppy_typecheck_io(): got prot = \n");
tnode_dumptree (prot, 1, FHAN_STDERR);
#endif
	if (!rhstype) {
		typecheck_error (node, tc, "item in input/output has unknown type");
		return 0;
	}

	acttype = typecheck_typeactual (lhstype, rhstype, node, tc);
	if (!acttype) {
		typecheck_error (node, tc, "incompatible types in input/output");
		return 0;
	}

	tnode_setnthsub (node, 2, acttype);

	return 0;
}
/*}}}*/
/*{{{  static int guppy_fetrans1_io (compops_t *cops, tnode_t **nodep, guppy_fetrans1_t *fe1)*/
/*
 *	does fetrans1 for an input or output
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_fetrans1_io (compops_t *cops, tnode_t **nodep, guppy_fetrans1_t *fe1)
{
	tnode_t *node = *nodep;

	fe1->inspoint = nodep;
	/* do subtrees first -- simplify things like instances into temporaries */
	guppy_fetrans1_subtree (tnode_nthsubaddr (node, 0), fe1);
	guppy_fetrans1_subtree (tnode_nthsubaddr (node, 1), fe1);

	if (node->tag == gup.tag_OUTPUT) {
		/* we need to make sure we can extract the address of anything on the RHS, i.e. must be a name */
		tnode_t *rhs = tnode_nthsubof (node, 1);
		int isaddr;

		/* FIXME: will need to handle lists either here, or earlier, for structured protocols */
		isaddr = langops_isaddressable (rhs);
		if (!isaddr) {
			/* create temporary and assign */
			tnode_t *tname, *type, *ass, *seq, *seqlist;
			tnode_t **newnodep;

			type = tnode_nthsubof (*nodep, 2);
			if (!type) {
				nocc_internal ("guppy_fetrans1_io(): no type!");
				return 0;
			}
			tname = guppy_fetrans1_maketemp (gup.tag_NDECL, rhs, type, NULL, fe1);
			ass = tnode_createfrom (gup.tag_ASSIGN, rhs, tname, rhs, type);
			seqlist = parser_newlistnode (SLOCI);
			parser_addtolist (seqlist, ass);
			newnodep = parser_addtolist (seqlist, node);
			seq = tnode_createfrom (gup.tag_SEQ, rhs, NULL, seqlist);

			*fe1->inspoint = seq;
			fe1->inspoint = newnodep;

			/* and fix the RHS */
			tnode_setnthsub (node, 1, tname);
		}
#if 0
fhandle_printf (FHAN_STDERR, "guppy_fetrans1_io(): OUTPUT, isaddr=%d, rhs =\n", isaddr);
tnode_dumptree (rhs, 1, FHAN_STDERR);
#endif
	}

	fe1->inspoint = NULL;
	return 0;
}
/*}}}*/
/*{{{  static int guppy_namemap_io (compops_t *cops, tnode_t **nodep, map_t *map)*/
/*
 *	does name-mapping for an input or output
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_namemap_io (compops_t *cops, tnode_t **nodep, map_t *map)
{
	int bytes = tnode_bytesfor (tnode_nthsubof (*nodep, 2), map->target);
	tnode_t *newinst, *newparms;
	tnode_t *sizeexp = constprop_newconst (CONST_INT, NULL, NULL, bytes);
	tnode_t *newarg;
	tnode_t *callnum;

	map_submapnames (tnode_nthsubaddr (*nodep, 0), map);
	map_submapnames (tnode_nthsubaddr (*nodep, 1), map);
	map_submapnames (&sizeexp, map);

	// newarg = cccsp_create_addrof (tnode_nthsubof (*nodep, 1), map->target);
	cccsp_set_indir (tnode_nthsubof (*nodep, 1), 1, map->target);

	/* transform into CCSP API call */
	newparms = parser_newlistnode (SLOCI);
	parser_addtolist (newparms, tnode_nthsubof (*nodep, 0));	/* channel */
	parser_addtolist (newparms, tnode_nthsubof (*nodep, 1));	/* data */
	parser_addtolist (newparms, sizeexp);
	if ((*nodep)->tag == gup.tag_INPUT) {
		callnum = cccsp_create_apicallname (CHAN_IN);
	} else if ((*nodep)->tag == gup.tag_OUTPUT) {
		callnum = cccsp_create_apicallname (CHAN_OUT);
	} else {
		nocc_internal ("guppy_namemap_io(): unknown node tag [%s]", (*nodep)->tag->name);
		return 0;
	}
	newinst = tnode_createfrom (gup.tag_APICALL, *nodep, callnum, newparms);

	*nodep = newinst;
	return 0;
}
/*}}}*/
/*{{{  static int guppy_codegen_io (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	does code-generation for an input or output
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_codegen_io (compops_t *cops, tnode_t *node, codegen_t *cgen)
{
	nocc_internal ("guppy_codegen_io(): should not be called!");
	return 0;
}
/*}}}*/


/*{{{  static int guppy_io_init_nodes (void)*/
/*
 *	initialises parse-tree nodes for input/output
 *	returns 0 on success, non-zero on failure
 */
static int guppy_io_init_nodes (void)
{
	tndef_t *tnd;
	int i;
	compops_t *cops;
	langops_t *lops;

	/*{{{  guppy:io -- INPUT, OUTPUT*/
	i = -1;
	tnd = tnode_newnodetype ("guppy:io", &i, 3, 0, 0, TNF_NONE);		/* subnodes: 0 = LHS, 1 = RHS, 2 = type */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (guppy_typecheck_io));
	tnode_setcompop (cops, "fetrans1", 2, COMPOPTYPE (guppy_fetrans1_io));

	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (guppy_namemap_io));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (guppy_codegen_io));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnd->lops = lops;

	i = -1;
	gup.tag_INPUT = tnode_newnodetag ("INPUT", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_OUTPUT = tnode_newnodetag ("OUTPUT", &i, tnd, NTF_NONE);

	/*}}}*/

	return 0;
}
/*}}}*/

/*{{{  guppy_io_feunit (feunit_t)*/
feunit_t guppy_io_feunit = {
	.init_nodes = guppy_io_init_nodes,
	.reg_reducers = NULL,
	.init_dfatrans = NULL,
	.post_setup = NULL,
	.ident = "guppy-io"
};
/*}}}*/

