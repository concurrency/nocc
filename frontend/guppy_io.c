/*
 *	guppy_io.c -- input and output for Guppy
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
	int is_input = 0, is_output = 0;

	typecheck_subtree (tnode_nthsubof (node, 0), tc);
	typecheck_subtree (tnode_nthsubof (node, 1), tc);

	lhstype = typecheck_gettype (tnode_nthsubof (node, 0), NULL);

	if (!lhstype) {
		typecheck_error (node, tc, "channel in input/output has unknown type");
		return 0;
	}

	/* see if we're restricted to input or output in the channel */
	guppy_chantype_getinout (lhstype, &is_input, &is_output);

	if (is_input && (node->tag == gup.tag_OUTPUT)) {
		typecheck_error (node, tc, "cannot output to channel marked as input");
		return 0;
	} else if (is_output && (node->tag == gup.tag_INPUT)) {
		typecheck_error (node, tc, "cannot input from channel marked as output");
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
	if (parser_islistnode (rhstype) && (parser_countlist (rhstype) == 1)) {
		/* singleton list, assume trivial */
		rhstype = parser_getfromlist (rhstype, 0);
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
/*{{{  static int guppy_precheck_io (compops_t *cops, tnode_t *node)*/
/*
 *	pre-checks for an I/O node -- marks out for checking
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_precheck_io (compops_t *cops, tnode_t *node)
{
	if (node->tag == gup.tag_INPUT) {
		usagecheck_marknode (tnode_nthsubaddr (node, 0), USAGE_INPUT, 0);
		usagecheck_marknode (tnode_nthsubaddr (node, 1), USAGE_WRITE, 0);
	} else if (node->tag == gup.tag_OUTPUT) {
		usagecheck_marknode (tnode_nthsubaddr (node, 0), USAGE_OUTPUT, 0);
		usagecheck_marknode (tnode_nthsubaddr (node, 1), USAGE_READ, 0);
	}
	return 1;
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
	tnode_t **saved_inspoint = fe1->inspoint;
	tnode_t *saved_decllist = fe1->decllist;

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

	fe1->inspoint = saved_inspoint;		/* continue into parent (or new) */
	fe1->decllist = saved_decllist;
	return 0;
}
/*}}}*/
/*{{{  static int guppy_fetrans15_io (compops_t *cops, tnode_t **nodep, guppy_fetrans15_t *fe15)*/
/*
 *	does fetrans1.5 on input/output node (do nothing, don't look inside)
 */
static int guppy_fetrans15_io (compops_t *cops, tnode_t **nodep, guppy_fetrans15_t *fe15)
{
	return 0;
}
/*}}}*/
/*{{{  static int guppy_fetrans3_io (compops_t *cops, tnode_t **nodep, guppy_fetrans3_t *fe3)*/
/*
 *	does fetrans3 for an input or output
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_fetrans3_io (compops_t *cops, tnode_t **nodep, guppy_fetrans3_t *fe3)
{
	tnode_t *node = *nodep;
	tnode_t *type = typecheck_gettype (tnode_nthsubof (node, 0), NULL);

	if (type) {
		type = typecheck_getsubtype (type, NULL);
	}
	if (!type) {
		nocc_internal ("guppy_fetrans3_io(): unknown protocol for channel I/O");
		return 0;
	}

	if (type->tag == gup.tag_ANY) {
		if (node->tag == gup.tag_OUTPUT) {
			tnode_t *atype = tnode_nthsubof (node, 2);
			unsigned int thash;
			tnode_t *seqnode, *seqlist;
			tnode_t *hashout, *hashtype;

			langops_typehash (atype, sizeof (thash), (void *)&thash);
			seqlist = parser_newlistnode (SLOCI);
			seqnode = tnode_createfrom (gup.tag_SEQ, node, NULL, seqlist);

			hashtype = guppy_newprimtype (gup.tag_INT, node, 32);
			hashout = tnode_createfrom (gup.tag_OUTPUT, node, tnode_nthsubof (node, 0),
					constprop_newconst (CONST_INT, NULL, hashtype, thash), hashtype);

			/* output hash-code followed by actual thing */
			parser_addtolist (seqlist, hashout);
			parser_addtolist (seqlist, node);

			*nodep = seqnode;
#if 0
fhandle_printf (FHAN_STDERR, "guppy_fetrans3_io(): protocol type (of channel):\n");
tnode_dumptree (type, 1, FHAN_STDERR);
fhandle_printf (FHAN_STDERR, "guppy_fetrans3_io(): hash output:\n");
tnode_dumptree (hashout, 1, FHAN_STDERR);
#endif
		}
	}

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
	tnode_t *type = tnode_nthsubof (*nodep, 2);
	int r = -1;

	if (type && type->tag->ndef->lops && tnode_haslangop_i (type->tag->ndef->lops, LOPS_NAMEMAP_TYPEACTION)) {
		/* let the type decide how best to deal with channel IO */
		r = tnode_calllangop_i (type->tag->ndef->lops, LOPS_NAMEMAP_TYPEACTION, 3, type, nodep, map);
	}
	if (r < 0) {
		int bytes = tnode_bytesfor (type, map->target);
		tnode_t *newinst, *newparms;
		tnode_t *sizeexp = constprop_newconst (CONST_INT, NULL, NULL, bytes);
		tnode_t *newarg;
		tnode_t *callnum, *wptr;
		cccsp_mapdata_t *cmd = (cccsp_mapdata_t *)map->hook;
		int saved_indir = cmd->target_indir;

		wptr = cmd->process_id;
		map_submapnames (&wptr, map);
		/* map LHS and RHS: LHS channel must be a pointer */
		cmd->target_indir = 1;
		map_submapnames (tnode_nthsubaddr (*nodep, 0), map);
#if 0
fhandle_printf (FHAN_STDERR, "guppy_namemap_io(): action type is:\n");
tnode_dumptree (type, 1, FHAN_STDERR);
#endif
		if (type->tag == gup.tag_NTYPEDECL) {
			/* FIXME: this should be dealt with inside type-specific code [namemap_typeaction?] */
			cmd->target_indir = 2;
		} else {
			cmd->target_indir = 1;
		}
		map_submapnames (tnode_nthsubaddr (*nodep, 1), map);
		cmd->target_indir = 0;
		map_submapnames (&sizeexp, map);

		// newarg = cccsp_create_addrof (tnode_nthsubof (*nodep, 1), map->target);
		// cccsp_set_indir (tnode_nthsubof (*nodep, 1), 1, map->target);

		/* transform into CCSP API call */
		newparms = parser_newlistnode (SLOCI);
		parser_addtolist (newparms, wptr);
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

		cmd->target_indir = saved_indir;
		*nodep = newinst;
		r = 0;
	}

	return r;
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

/*{{{  static int guppy_dousagecheck_io (langops_t *lops, tnode_t *node, uchk_state_t *ucs)*/
/*
 *	does usage-checking for an I/O node
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_dousagecheck_io (langops_t *lops, tnode_t *node, uchk_state_t *ucs)
{
	if (node->tag == gup.tag_INPUT) {
		if (!langops_isvar (tnode_nthsubof (node, 1))) {
			usagecheck_error (node, ucs, "right hand side of input must be writeable");
		}
	}
	return 1;
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
	tnode_setcompop (cops, "precheck", 1, COMPOPTYPE (guppy_precheck_io));
	tnode_setcompop (cops, "fetrans1", 2, COMPOPTYPE (guppy_fetrans1_io));
	tnode_setcompop (cops, "fetrans15", 2, COMPOPTYPE (guppy_fetrans15_io));
	tnode_setcompop (cops, "fetrans3", 2, COMPOPTYPE (guppy_fetrans3_io));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (guppy_namemap_io));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (guppy_codegen_io));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "do_usagecheck", 2, LANGOPTYPE (guppy_dousagecheck_io));
	tnd->lops = lops;

	i = -1;
	gup.tag_INPUT = tnode_newnodetag ("INPUT", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_OUTPUT = tnode_newnodetag ("OUTPUT", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  guppy:caseio -- CASEINPUT*/
	i = -1;
	tnd = tnode_newnodetype ("guppy:caseio", &i, 3, 0, 0, TNF_LONGACTION);	/* subnodes: 0 = LHS, 1 = RHS-list, 2 = type */
	cops = tnode_newcompops ();
	/* FIXME: need some stuff here.. */
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnd->lops = lops;

	i = -1;
	gup.tag_CASEINPUT = tnode_newnodetag ("CASEINPUT", &i, tnd, NTF_INDENTED_TCASE_LIST);

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

