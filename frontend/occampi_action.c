/*
 *	occampi_action.c -- occam-pi action handling for NOCC
 *	Copyright (C) 2005-2007 Fred Barnes <frmb@kent.ac.uk>
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
#include "dfaerror.h"
#include "fcnlib.h"
#include "parsepriv.h"
#include "occampi.h"
#include "feunit.h"
#include "names.h"
#include "scope.h"
#include "prescope.h"
#include "typecheck.h"
#include "langops.h"
#include "precheck.h"
#include "usagecheck.h"
#include "tracescheck.h"
#include "fetrans.h"
#include "betrans.h"
#include "map.h"
#include "target.h"
#include "transputer.h"
#include "codegen.h"
#include "langdef.h"


/*}}}*/
/*{{{  private data*/

static chook_t *opi_action_lhstypehook = NULL;


/*}}}*/



/*
 *	this file contains front-end routines for handling action-nodes,
 *	e.g. assignment, input and output
 */


/*{{{  static void occampi_action_dumplhstypehook (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	used to dump the occampi:action:lhstype compiler hook (debugging)
 */
static void occampi_action_dumplhstypehook (tnode_t *node, void *hook, int indent, FILE *stream)
{
	tnode_t *lhstype = (tnode_t *)hook;

	occampi_isetindent (stream, indent);
	fprintf (stream, "<chook id=\"occampi:action:lhstype\" addr=\"0x%8.8x\">\n", (unsigned int)hook);
	tnode_dumptree (lhstype, indent + 1, stream);
	occampi_isetindent (stream, indent);
	fprintf (stream, "</chook>\n");
	return;
}
/*}}}*/



/*{{{  static int occampi_actionscope_prewalk_scopefields (tnode_t *node, void *data)*/
/*
 *	called to scope in tag-names in a variant protocol -- already NAMENODEs
 */
static int occampi_actionscope_prewalk_scopefields (tnode_t *node, void *data)
{
	scope_t *ss = (scope_t *)data;

	if (node->tag == opi.tag_TAGDECL) {
		tnode_t *tagname = tnode_nthsubof (node, 0);

		if (tagname->tag == opi.tag_NTAG) {
			name_scopename (tnode_nthnameof (tagname, 0));
		} else {
			scope_warning (tagname, ss, "TAGDECL does not have NTAG name");
		}
	}
	return 1;
}
/*}}}*/
/*{{{  static int occampi_scopein_action (compops_t *cops, tnode_t **nodep, scope_t *ss)*/
/*
 *	called to scope-in an action-node (needs to be aware of tag-names)
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_scopein_action (compops_t *cops, tnode_t **nodep, scope_t *ss)
{
	if (((*nodep)->tag == opi.tag_INPUT) || ((*nodep)->tag == opi.tag_OUTPUT)) {
		tnode_t **lhsp = tnode_nthsubaddr (*nodep, 0);
		tnode_t **rhsp = tnode_nthsubaddr (*nodep, 1);
		tnode_t *ctype;
		int did_error = 0;
		int did_rhsscope = 0;

		/* scope LHS normally */
		scope_subtree (lhsp, ss);

		/* try and get LHS type by typecheck (which at scope-in is probably risky..) */
		ctype = typecheck_gettype (*lhsp, NULL);
		if (!ctype) {
			/* no checkable type, see if it's a typed namenode */
			if ((*lhsp)->tag->ndef == opi.node_NAMENODE) {
				name_t *name = tnode_nthnameof (*lhsp, 0);
				ctype = NameTypeOf (name);
			}
		}

		if (!ctype) {
			/* give up here */
			scope_error (*nodep, ss, "failed to get LHS type for channel input or output");
			did_error = 1;
		} else {
#if 1
fprintf (stderr, "occampi_scopein_action(): type of LHS is:\n");
tnode_dumptree (ctype, 1, stderr);
#endif
			if (ctype->tag != opi.tag_CHAN) {
				tnode_t *subtype = tnode_nthsubof (ctype, 0);

				/* subtype will be the channel protocol, if a named variant protocol, scope-in fields */
				if (subtype->tag == opi.tag_NVARPROTOCOLDECL) {
					void *namemark = name_markscope ();

					tnode_prewalktree (NameTypeOf (tnode_nthnameof (subtype, 0)), occampi_actionscope_prewalk_scopefields, (void *)ss);

					/* tag-names in scope, do scope of RHS */
					scope_subtree (rhsp, ss);
					did_rhsscope = 1;

					name_markdescope (namemark);
				}
			}
		}

		if (!did_rhsscope) {
			scope_subtree (rhsp, ss);
		}

		return 0;
	}
	return 1;
}
/*}}}*/
/*{{{  static int occampi_typecheck_action (compops_t *cops, tnode_t *node, typecheck_t *tc)*/
/*
 *	called to type-check an action-node
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_typecheck_action (compops_t *cops, tnode_t *node, typecheck_t *tc)
{
	/* FIXME: to handle multiple-assignment more transparently ? */
	tnode_t *lhs = tnode_nthsubof (node, 0);
	tnode_t *rhs = tnode_nthsubof (node, 1);
	tnode_t *acttype = tnode_nthsubof (node, 2);
	tnode_t *lhstype, *rhstype;

#if 0
fprintf (stderr, "occampi_typecheck_action(): here!\n");
#endif
	if (acttype) {
		nocc_warning ("occampi_typecheck_action(): strange.  already type-checked this action..");
		return 0;		/* don't walk sub-nodes */
	}
	
	/* call type-check on LHS and RHS trees */
	typecheck_subtree (lhs, tc);
	typecheck_subtree (rhs, tc);

	if (node->tag == opi.tag_ASSIGN) {
		/*{{{  assignment*/
#if 0
fprintf (stderr, "occampi_typecheck_action(): lhs = \n");
tnode_dumptree (lhs, 1, stderr);
fprintf (stderr, "occampi_typecheck_action(): rhs = \n");
tnode_dumptree (rhs, 1, stderr);
#endif
		lhstype = typecheck_gettype (lhs, NULL);
		rhstype = typecheck_gettype (rhs, lhstype);
		/*}}}*/
	} else {
		/*{{{  other action -- communication*/
		tnode_t *prot;

		lhstype = typecheck_gettype (lhs, NULL);
#if 0
fprintf (stderr, "occampi_typecheck_action(): lhstype =\n");
tnode_dumptree (lhstype, 1, stderr);
#endif

		/* expecting something on which we can communicate -- e.g. channel or port
		 * test is to see if it has a particular codegen_typeaction language-op
		 */
		if (!tnode_haslangop (lhstype->tag->ndef->lops, "codegen_typeaction")) {
			typecheck_error (node, tc, "LHS is not-communicable, got [%s]", lhstype->tag->name);
			return 0;
		}

		/* get the type of the channel (channel protocol) */
		prot = typecheck_getsubtype (lhstype, NULL);
		rhstype = typecheck_gettype (rhs, prot);

		/*}}}*/
	}

	if ((node->tag == opi.tag_OUTPUT) || (node->tag == opi.tag_INPUT)) {
		/*{{{  check for output channel*/
		occampi_typeattr_t tattr = occampi_typeattrof (lhstype);
		
		if ((tattr & TYPEATTR_MARKED_IN) && (node->tag == opi.tag_OUTPUT)) {
			typecheck_error (node, tc, "cannot output on channel marked as input");
		} else if ((tattr & TYPEATTR_MARKED_OUT) && (node->tag == opi.tag_INPUT)) {
			typecheck_error (node, tc, "cannot input from channel marked as output");
		}

		/*}}}*/
	}

#if 0
fprintf (stderr, "occampi_typecheck_action(): lhstype = \n");
tnode_dumptree (lhstype, 1, stderr);
fprintf (stderr, "occampi_typecheck_action(): rhstype = \n");
tnode_dumptree (rhstype, 1, stderr);
#endif

	/* got two valid types, check that the RHS type is good for the LHS */
	acttype = typecheck_typeactual (lhstype, rhstype, node, tc);
	if (!acttype) {
		typecheck_error (node, tc, "incompatible types");
		return 0;
	} else {
		tnode_setnthsub (node, 2, acttype);
	}

	tnode_setchook (node, opi_action_lhstypehook, (void *)lhstype);

	return 0;	/* don't walk sub-nodes */
}
/*}}}*/
/*{{{  static tnode_t *occampi_gettype_action (langops_t *lops, tnode_t *node, tnode_t *default_type)*/
/*
 *	called to get the type of an action -- just returns the held type
 */
static tnode_t *occampi_gettype_action (langops_t *lops, tnode_t *node, tnode_t *default_type)
{
	return tnode_nthsubof (node, 2);
}
/*}}}*/
/*{{{  static int occampi_precheck_action (compops_t *cops, tnode_t *node)*/
/*
 *	called to do pre-checks on an action-node
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_precheck_action (compops_t *cops, tnode_t *node)
{
	if (node->tag == opi.tag_INPUT) {
		usagecheck_marknode (tnode_nthsubof (node, 0), USAGE_INPUT, 0);
		usagecheck_marknode (tnode_nthsubof (node, 1), USAGE_WRITE, 0);
	} else if (node->tag == opi.tag_OUTPUT) {
		usagecheck_marknode (tnode_nthsubof (node, 0), USAGE_OUTPUT, 0);
		usagecheck_marknode (tnode_nthsubof (node, 1), USAGE_READ, 0);
	} else if (node->tag == opi.tag_ASSIGN) {
		/* deeper usage-checking may sort these out later on */
		usagecheck_marknode (tnode_nthsubof (node, 0), USAGE_WRITE, 0);
		usagecheck_marknode (tnode_nthsubof (node, 1), USAGE_READ, 0);
	}
	return 1;
}
/*}}}*/
/*{{{  static int occampi_tracescheck_action (compops_t *cops, tnode_t *node, tchk_state_t *tcstate)*/
/*
 *	called to do traces checking on an action-node
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_tracescheck_action (compops_t *cops, tnode_t *node, tchk_state_t *tcstate)
{
	if ((node->tag == opi.tag_INPUT) || (node->tag == opi.tag_OUTPUT)) {
		tnode_t *lhs = tnode_nthsubof (node, 0);
		tnode_t *baselhs = langops_getbasename (lhs);
		tnode_t *fieldlhs = langops_getfieldnamelist (lhs);
		// tnode_t *lhstype = (tnode_t *)tnode_getchook (node, opi_action_lhstypehook);
		chook_t *tchkhook = tracescheck_getnoderefchook ();
		tchknode_t *lhstcn;

#if 0
fprintf (stderr, "occampi_tracescheck_action(): INPUT or OUTPUT, lhs is:\n");
tnode_dumptree (lhs, 1, stderr);
fprintf (stderr, "                          (): baselhs is:\n");
tnode_dumptree (baselhs, 1, stderr);
#endif
		if (baselhs && (baselhs != lhs)) {
			/* use the base-name */
			lhs = baselhs;
		}

		/* grab the tchknode_t hook placed on the base (if any) */
		lhstcn = (tchknode_t *)tnode_getchook (lhs, tchkhook);

		if (lhstcn) {
			tchknode_t *newtcn = tracescheck_dupref (lhstcn);

#if 1
fprintf (stderr, "occampi_tracescheck_action(): got hook for traces on LHS/base-of, field-list is:\n");
tnode_dumptree (fieldlhs, 1, stderr);
#endif
			newtcn = tracescheck_createnode ((node->tag == opi.tag_INPUT) ? TCN_INPUT : TCN_OUTPUT, node, newtcn);
			tracescheck_addtobucket (tcstate, newtcn);
		}
	}

	return 1;
}
/*}}}*/
/*{{{  static int occampi_do_usagecheck_action (langops_t *lops, tnode_t *node, uchk_state_t *ucs)*/
/*
 *	called to do usage-checking on an action-node
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_do_usagecheck_action (langops_t *lops, tnode_t *node, uchk_state_t *ucs)
{
#if 0
	fprintf (stderr, "occampi_do_usagecheck_action(): here!\n");
#endif
	if (node->tag == opi.tag_INPUT) {
		/* RHS must be an l-value */
		if (!langops_isvar (tnode_nthsubof (node, 1))) {
			usagecheck_error (node, ucs, "target for input must be a variable");
		}
	} else if (node->tag == opi.tag_ASSIGN) {
		/* LHS must be an l-value */
		if (!langops_isvar (tnode_nthsubof (node, 0))) {
			usagecheck_error (node, ucs, "target for assignment must be a variable");
		}
	}
	return 1;
}
/*}}}*/
/*{{{  static int occampi_fetrans_action (compops_t *cops, tnode_t **node, fetrans_t *fe)*/
/*
 *	called to do front-end transforms on action nodes
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_fetrans_action (compops_t *cops, tnode_t **node, fetrans_t *fe)
{
	tnode_t *t = *node;
	tnode_t **saved_insertpoint = fe->insertpoint;
	tnode_t **rhsp = tnode_nthsubaddr (t, 1);

	fe->insertpoint = node;				/* before process is a good place to insert temporaries */

	if (t->tag == opi.tag_OUTPUT) {
		/*{{{  if RHS looks complex, or is not a natural pointer, add temporary and assignment*/
		if (langops_iscomplex (*rhsp, 1) || !langops_isvar (*rhsp)) {
			tnode_t *temp = fetrans_maketemp (tnode_nthsubof (t, 2), fe);

			/* now assignment.. */
			fetrans_makeseqassign (temp, *rhsp, tnode_nthsubof (t, 2), fe);

			tnode_setnthsub (t, 1, temp);
		}
		/*}}}*/
	}

	fe->insertpoint = saved_insertpoint;

	return 1;
}
/*}}}*/
/*{{{  static int occampi_betrans_action (compops_t *cops, tnode_t **node, betrans_t *be)*/
/*
 *	called to do back-end transforms on action nodes
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_betrans_action (compops_t *cops, tnode_t **node, betrans_t *be)
{
	tnode_t *t = *node;
	tnode_t **saved_insertpoint = be->insertpoint;

	be->insertpoint = node;		/* before node is a good place */

	if (t->tag == opi.tag_ASSIGN) {
		/*{{{  sort out some complex FUNCTION calls*/
		tnode_t *rhs = tnode_nthsubof (t, 1);
		tnode_t **lhsp = tnode_nthsubaddr (t, 0);
		int nlhs, single = 0;
		int modified = 0;

		if (!parser_islistnode (*lhsp)) {
			/* single result */
			nlhs = single = 1;
		} else {
			lhsp = parser_getlistitems (*lhsp, &nlhs);
		}

		if (rhs->tag == opi.tag_FINSTANCE) {
			/*{{{  special-case: check RHS for parameterised results*/
			tnode_t *fnamenode = tnode_nthsubof (rhs, 0);
			// name_t *fname = tnode_nthnameof (fnamenode, 0);
			tnode_t *ftype = typecheck_gettype (fnamenode, NULL); // NameTypeOf (fname);
			tnode_t **fparams, *aparams;
			int nfparams, i;

			if (!ftype || (ftype->tag != opi.tag_FUNCTIONTYPE)) {
				tnode_error (rhs, "type of function not FUNCTIONTYPE");
				return 0;
			}

			aparams = tnode_nthsubof (rhs, 1);
			fparams = parser_getlistitems (tnode_nthsubof (ftype, 1), &nfparams);

			/* look for those fparams tagged with VALOF */
			for (i=0; i<nfparams; i++) {
				int x;
				ntdef_t *tag = betrans_gettag (fparams[i], &x, be);

				if (tag) {
					/* this one was! */
					if ((x < 0) || (x >= nlhs)) {
						tnode_error (t, "occampi_betrans_action(): RHS function instance has missing result %d on LHS", x);
						return 0;
					}

					/* move it over */
#if 0
fprintf (stderr, "occampi_betrans_action(): FINSTANCE on ASSIGN, fparam %d used to be result %d.  corresponding result is:\n", i, x);
if (x < nlhs) {
	tnode_dumptree (lhsp[x], 1, stderr);
} else {
	fprintf (stderr, "    (out of range!)\n");
}
#endif
					if (single) {
						parser_addtolist (aparams, *lhsp);
						*lhsp = NULL;
						nlhs--;
					} else {
						tnode_t *lhs = parser_delfromlist (*lhsp, x);

						nlhs--;
						parser_addtolist (aparams, lhs);
					}
					modified = 1;
				}
			}

			/* did we get all of them ? */
			if (!nlhs) {
				/* nothing left on LHS, remove assignment */
				if (*lhsp) {
					tnode_free (*lhsp);
				}

				*node = tnode_nthsubof (t, 1);
				tnode_setnthsub (t, 1, NULL);

				tnode_setnthsub (t, 2, NULL); 		/* leave the type alone */

				tnode_free (t);
				modified = 1;
			}
			/*}}}*/
		}

		if (modified) {
			/* if modified, transform tree again */
			betrans_subtree (node, be);
			return 0;
		}
		/*}}}*/
	} else if ((t->tag == opi.tag_INPUT) || (t->tag == opi.tag_OUTPUT)) {
		/*{{{  channel or port I/O, if the LHS is complex simplify into a temporary*/
		tnode_t **lhsp = tnode_nthsubaddr (t, 0);

		if (langops_iscomplex (*lhsp, 1)) {
#if 0
fprintf (stderr, "occampi_betrans_action(): I/O with complex LHS!\n");
#endif
			betrans_simplifypointer (lhsp, be);
		}

		/*}}}*/
	}

	betrans_subtree (tnode_nthsubaddr (t, 0), be);		/* betrans LHS */
	betrans_subtree (tnode_nthsubaddr (t, 1), be);		/* betrans RHS */

	be->insertpoint = saved_insertpoint;
	return 0;
}
/*}}}*/
/*{{{  static int occampi_premap_action (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	does per-mapping for an action -- turns expression nodes into RESULT nodes
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_premap_action (compops_t *cops, tnode_t **node, map_t *map)
{
	/* premap LHS and RHS */
	map_subpremap (tnode_nthsubaddr (*node, 0), map);
	map_subpremap (tnode_nthsubaddr (*node, 1), map);

	return 0;
}
/*}}}*/
/*{{{  static int occampi_namemap_action (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	allocates space necessary for an action
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_namemap_action (compops_t *cops, tnode_t **node, map_t *map)
{
	/* do left/right sides first */
	map_submapnames (tnode_nthsubaddr (*node, 0), map);
	map_submapnames (tnode_nthsubaddr (*node, 1), map);

	if (((*node)->tag == opi.tag_OUTPUT) || ((*node)->tag == opi.tag_INPUT)) {
		tnode_t *bename;

		bename = map->target->newname (*node, NULL, map, 0, map->target->bws.ds_io, 0, 0, 0, 0);
		*node = bename;
	}

	return 0;
}
/*}}}*/
/*{{{  static int occampi_codegen_action (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	generates code for an action
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_codegen_action (compops_t *cops, tnode_t *node, codegen_t *cgen)
{
	tnode_t *lhs = tnode_nthsubof (node, 0);
	tnode_t *rhs = tnode_nthsubof (node, 1);
	tnode_t *type = tnode_nthsubof (node, 2);
	int bytes = tnode_bytesfor (type, cgen->target);
	tnode_t *lhstype = (tnode_t *)tnode_getchook (node, opi_action_lhstypehook);

#if 0
fprintf (stderr, "occampi_codegen_action(): %s: bytes = %d, type =\n", node->tag->name, bytes);
tnode_dumptree (type, 1, stderr);
#endif
	if (!lhstype) {
		lhstype = typecheck_gettype (lhs, NULL);
	}

	codegen_callops (cgen, debugline, node);
	/* some special cases for assignment, input and output -- these have codegen_typeaction() set in language-ops */
	if (type && type->tag->ndef->lops && tnode_haslangop (type->tag->ndef->lops, "codegen_typeaction")) {
		int i;

#if 0
fprintf (stderr, "occampi_codegen_action(): type has codegen_typeaction.\n");
#endif
		i = tnode_calllangop (type->tag->ndef->lops, "codegen_typeaction", 3, type, node, cgen);
		if (i >= 0) {
			/* did something */
			return i;
		}	/* else try a normal action handling on it */
	} else if (lhstype && lhstype->tag->ndef->lops && tnode_haslangop (lhstype->tag->ndef->lops, "codegen_typeaction")) {
		/* left-hand side has type-action, but the operation itself does not;  offer it up */
		int i;

#if 0
fprintf (stderr, "occampi_codegen_action(): lhstype has codegen_typeaction.\n");
#endif
		i = tnode_calllangop (lhstype->tag->ndef->lops, "codegen_typeaction", 3, lhstype, node, cgen);
		if (i >= 0) {
			/* did something */
			return i;
		}	/* else try a normal action handling on it */
	}

	if (node->tag == opi.tag_ASSIGN) {
#if 0
fprintf (stderr, "occampi_codegen_action(): ASSIGN: bytes = %d, cgen->target->intsize = %d\n", bytes, cgen->target->intsize);
fprintf (stderr, "occampi_codegen_action(): ASSIGN: lhs =\n");
tnode_dumptree (lhs, 1, stderr);
fprintf (stderr, "occampi_codegen_action(): ASSIGN: rhs =\n");
tnode_dumptree (rhs, 1, stderr);
fprintf (stderr, "occampi_codegen_action(): ASSIGN: type =\n");
tnode_dumptree (type, 1, stderr);
#endif
		if ((bytes < 0)) {
			/* maybe need alternate code-gen for this! */
			codegen_error (cgen, "occampi_codegen_action(): unknown size for node [%s]", type->tag->name);
		} else if (bytes <= cgen->target->intsize) {
			/* simple load and store */
			codegen_callops (cgen, loadname, rhs, 0);
			codegen_callops (cgen, storename, lhs, 0);
		} else {
			/* load pointers, block move */
			codegen_callops (cgen, loadpointer, rhs, 0);
			codegen_callops (cgen, loadpointer, lhs, 0);
			codegen_callops (cgen, loadconst, bytes);
			codegen_callops (cgen, tsecondary, I_MOVE);
		}
	} else if (node->tag == opi.tag_OUTPUT) {
		/* load a pointer to value, pointer to channel, size */
		codegen_callops (cgen, loadpointer, rhs, 0);
		codegen_callops (cgen, loadpointer, lhs, 0);
		codegen_callops (cgen, loadconst, bytes);
		codegen_callops (cgen, tsecondary, I_OUT);
	} else if (node->tag == opi.tag_INPUT) {
		/* same as output really.. */
		codegen_callops (cgen, loadpointer, rhs, 0);
		codegen_callops (cgen, loadpointer, lhs, 0);
		codegen_callops (cgen, loadconst, bytes);
		codegen_callops (cgen, tsecondary, I_IN);
	} else {
		codegen_callops (cgen, comment, "FIXME!");
	}

	return 0;
}
/*}}}*/


/*{{{  static int occampi_action_init_nodes (void)*/
/*
 *	initialises nodes for occam-pi actions
 *	returns 0 on success, non-zero on failure
 */
static int occampi_action_init_nodes (void)
{
	tndef_t *tnd;
	int i;
	compops_t *cops;
	langops_t *lops;

	/*{{{  occampi:action:lhstype compiler hook*/
	opi_action_lhstypehook = tnode_lookupornewchook ("occampi:action:lhstype");
	opi_action_lhstypehook->chook_dumptree = occampi_action_dumplhstypehook;

	/*}}}*/
	/*{{{  occampi:actionnode -- ASSIGN, INPUT, OUTPUT*/
	i = -1;
	opi.node_ACTIONNODE = tnd = tnode_newnodetype ("occampi:actionnode", &i, 3, 0, 0, TNF_NONE);		/* subnodes: left, right, type */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (occampi_scopein_action));
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (occampi_typecheck_action));
	tnode_setcompop (cops, "precheck", 1, COMPOPTYPE (occampi_precheck_action));
	tnode_setcompop (cops, "tracescheck", 2, COMPOPTYPE (occampi_tracescheck_action));
	tnode_setcompop (cops, "fetrans", 2, COMPOPTYPE (occampi_fetrans_action));
	tnode_setcompop (cops, "betrans", 2, COMPOPTYPE (occampi_betrans_action));
	tnode_setcompop (cops, "premap", 2, COMPOPTYPE (occampi_premap_action));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (occampi_namemap_action));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (occampi_codegen_action));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "gettype", 2, LANGOPTYPE (occampi_gettype_action));
	tnode_setlangop (lops, "do_usagecheck", 2, LANGOPTYPE (occampi_do_usagecheck_action));
	tnd->lops = lops;

	i = -1;
	opi.tag_ASSIGN = tnode_newnodetag ("ASSIGN", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_INPUT = tnode_newnodetag ("INPUT", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_OUTPUT = tnode_newnodetag ("OUTPUT", &i, tnd, NTF_NONE);
	/*}}}*/

	return 0;
}
/*}}}*/


/*{{{  occampi_action_feunit (feunit_t)*/
feunit_t occampi_action_feunit = {
	init_nodes: occampi_action_init_nodes,
	reg_reducers: NULL,
	init_dfatrans: NULL,
	post_setup: NULL,
	ident: "occampi-action"
};
/*}}}*/

