/*
 *	occampi_snode.c -- occam-pi structured processes for NOCC (IF, ALT, etc.)
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
#include "treeops.h"
#include "parser.h"
#include "dfa.h"
#include "parsepriv.h"
#include "occampi.h"
#include "feunit.h"
#include "names.h"
#include "scope.h"
#include "prescope.h"
#include "typecheck.h"
#include "usagecheck.h"
#include "fetrans.h"
#include "betrans.h"
#include "map.h"
#include "codegen.h"
#include "target.h"
#include "transputer.h"
#include "mwsync.h"


/*}}}*/
/*{{{  private data*/

/* this is a chook attached to guard-nodes that indicates what needs to be enabled */
static chook_t *guardexphook = NULL;


/*}}}*/


/*{{{  static void occampi_guardexphook_dumptree (tnode_t *node, void *chook, int indent, FILE *stream)*/
/*
 *	display the contents of a guardexphook compiler hook (just a node)
 */
static void occampi_guardexphook_dumptree (tnode_t *node, void *chook, int indent, FILE *stream)
{
	if (chook) {
		occampi_isetindent (stream, indent);
		fprintf (stream, "<occampi:guardexphook addr=\"0x%8.8x\">\n", (unsigned int)chook);
		tnode_dumptree ((tnode_t *)chook, indent + 1, stream);
		occampi_isetindent (stream, indent);
		fprintf (stream, "</occampi:guardexphook>\n");
	}
	return;
}
/*}}}*/
/*{{{  static void occampi_guardexphook_free (void *chook)*/
/*
 *	frees a guardexphook
 */
static void occampi_guardexphook_free (void *chook)
{
	return;
}
/*}}}*/
/*{{{  static void *occampi_guardexphook_copy (void *chook)*/
/*
 *	copies a guardexphook
 */
static void *occampi_guardexphook_copy (void *chook)
{
	return chook;
}
/*}}}*/


/*{{{  static int occampi_betrans_guardnode (compops_t *cops, tnode_t **nodep, betrans_t *be)*/
/*
 *	does back-end transformations for a guard node (pulls out guarded expression before mapping)
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_betrans_guardnode (compops_t *cops, tnode_t **nodep, betrans_t *be)
{
	if ((*nodep)->tag == opi.tag_INPUTGUARD) {
		/*{{{  pull out channel expression*/
		tnode_t *input = treeops_findintree (tnode_nthsubof (*nodep, 0), opi.tag_INPUT);

		if (!input) {
			tnode_error (*nodep, "did not find INPUT in INPUTGUARD!");
		} else {
			tnode_t *lhs = tnode_nthsubof (input, 0);

			tnode_setchook (*nodep, guardexphook, (void *)lhs);
		}
		/*}}}*/
	} else if ((*nodep)->tag == opi.tag_TIMERGUARD) {
		/*{{{  pull out timeout expression*/
		/* FIXME! */
		/*}}}*/
	}
	return 1;
}
/*}}}*/
/*{{{  static int occampi_namemap_guardnode (compops_t *cops, tnode_t **nodep, map_t *map)*/
/*
 *	does name-mapping for an ALT guard
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_namemap_guardnode (compops_t *cops, tnode_t **nodep, map_t *map)
{
	tnode_t *guardexp = (tnode_t *)tnode_getchook (*nodep, guardexphook);

	if (guardexp) {
		map_submapnames (&guardexp, map);
		tnode_setchook (*nodep, guardexphook, (void *)guardexp);
		tnode_setchook (*nodep, map->allocevhook, (void *)guardexp);
	}

	return 1;
}
/*}}}*/
/*{{{  static int occampi_codegen_altenable_guardnode (langops_t *lops, tnode_t *guard, int dlabel, codegen_t *cgen)*/
/*
 *	does code-generation for an ALT guard enable
 *	returns 0 on success, non-zero on failure
 */
static int occampi_codegen_altenable_guardnode (langops_t *lops, tnode_t *guard, int dlabel, codegen_t *cgen)
{
	tnode_t *guardexpr = (tnode_t *)tnode_getchook (guard, guardexphook);

	if (guard->tag == opi.tag_INPUTGUARD) {
		if (!guardexpr) {
			nocc_internal ("occampi_codegen_snode(): no guard expression on INPUTGUARD!");
		} else {
			tnode_t *precond = tnode_nthsubof (guard, 2);

			codegen_callops (cgen, loadpointer, guardexpr, 0);
			if (precond) {
				codegen_subcodegen (precond, cgen);
			} else {
				codegen_callops (cgen, loadconst, 1);
			}
			codegen_callops (cgen, loadlabaddr, dlabel);
			codegen_callops (cgen, tsecondary, I_ENBC);
			codegen_callops (cgen, trashistack);
		}
	}
	return 0;
}
/*}}}*/
/*{{{  static int occampi_codegen_altdisable_guardnode (langops_t *lops, tnode_t *guard, int dlabel, int plabel, codegen_t *cgen)*/
/*
 *	does code-generation for an ALT guard disable
 *	returns 0 on success, non-zero on failure
 */
static int occampi_codegen_altdisable_guardnode (langops_t *lops, tnode_t *guard, int dlabel, int plabel, codegen_t *cgen)
{
	tnode_t *guardexpr = (tnode_t *)tnode_getchook (guard, guardexphook);

	codegen_callops (cgen, setlabel, dlabel);
	if (guard->tag == opi.tag_INPUTGUARD) {
		if (!guardexpr) {
			nocc_internal ("occampi_codegen_snode(): guard expression on INPUTGUARD vanished!");
		} else {
			tnode_t *precond = tnode_nthsubof (guard, 2);

			codegen_callops (cgen, loadpointer, guardexpr, 0);
			if (precond) {
				codegen_subcodegen (precond, cgen);
			} else {
				codegen_callops (cgen, loadconst, 1);
			}
			codegen_callops (cgen, loadlabaddr, plabel);
			codegen_callops (cgen, tsecondary, I_DISC);
			codegen_callops (cgen, trashistack);
		}
	}
	return 0;
}
/*}}}*/


/*{{{  static int occampi_codegen_altstart (langops_t *lops, tnode_t *node, codegen_t *cgen)*/
/*
 *	generates code for occam-pi ALT start
 *	returns 0 on success, non-zero on failure
 */
static int occampi_codegen_altstart (langops_t *lops, tnode_t *node, codegen_t *cgen)
{
	mwsyncaltinfo_t *altinf = mwsync_getaltinfo (node);

	if (altinf && altinf->bcount) {
		/* need a multiway sync start */
		codegen_callops (cgen, tsecondary, I_MWS_ALTLOCK);
		codegen_callops (cgen, tsecondary, I_MWS_ALT);
	} else {
		/* down-stream alt-start */
		if (tnode_haslangop (lops->next, "codegen_altstart")) {
			return tnode_calllangop (lops->next, "codegen_altstart", 2, node, cgen);
		} else {
			/* basic */
			codegen_callops (cgen, tsecondary, I_ALT);
		}
	}
	return 0;
}
/*}}}*/
/*{{{  static int occampi_codegen_altwait (langops_t *lops, tnode_t *node, codegen_t *cgen)*/
/*
 *	generates code for basic occam-pi ALT wait
 *	returns 0 on success, non-zero on failure
 */
static int occampi_codegen_altwait (langops_t *lops, tnode_t *node, codegen_t *cgen)
{
	mwsyncaltinfo_t *altinf = mwsync_getaltinfo (node);

	if (altinf && altinf->bcount) {
		/* we're multi-way synching, better unlock before wait */
		codegen_callops (cgen, tsecondary, I_MWS_ALTUNLOCK);
	}

	/* down-stream alt-wait */
	if (tnode_haslangop (lops->next, "codegen_altwait")) {
		tnode_calllangop (lops->next, "codegen_altwait", 2, node, cgen);
	} else {
		codegen_callops (cgen, tsecondary, I_ALTWT);
	}

	if (altinf && altinf->bcount) {
		/* and re-lock afterwards */
		codegen_callops (cgen, tsecondary, I_MWS_ALTPOSTLOCK);
	}
	return 0;
}
/*}}}*/
/*{{{  static int occampi_codegen_altend (langops_t *lops, tnode_t *node, codegen_t *cgen)*/
/*
 *	generates code for occam-pi ALT end
 *	returns 0 on success, non-zero on failure
 */
static int occampi_codegen_altend (langops_t *lops, tnode_t *node, codegen_t *cgen)
{
	mwsyncaltinfo_t *altinf = mwsync_getaltinfo (node);

	if (altinf && altinf->bcount) {
		/* need a multiway sync end */
		codegen_callops (cgen, tsecondary, I_MWS_ALTEND);
		codegen_callops (cgen, tsecondary, I_SETERR);
	} else {
		/* down-stream alt-end */
		if (tnode_haslangop (lops->next, "codegen_altend")) {
			return tnode_calllangop (lops->next, "codegen_altend", 2, node, cgen);
		} else {
			codegen_callops (cgen, tsecondary, I_ALTEND);
			codegen_callops (cgen, tsecondary, I_SETERR);
		}
	}
	return 0;
}
/*}}}*/


/*{{{  static int occampi_typecheck_snode (compops_t *cops, tnode_t *node, typecheck_t *tc)*/
/*
 *	does type-checking for a structured node (IF/ALT/CASE)
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_typecheck_snode (compops_t *cops, tnode_t *node, typecheck_t *tc)
{
	if (node->tag == opi.tag_CASE) {
		tnode_t *definttype = tnode_create (opi.tag_INT, NULL);
		tnode_t *swtype = NULL;
		tnode_t **items;
		int nitems, i;

		typecheck_subtree (tnode_nthsubof (node, 0), tc);		/* check expression */

		swtype = typecheck_gettype (tnode_nthsubof (node, 0), definttype);
		if (!swtype || !typecheck_typeactual (definttype, swtype, node, tc)) {
			typecheck_error (node, tc, "case expression is non-integer");
			return 1;
		}
		items = parser_getlistitems (tnode_nthsubof (node, 1), &nitems);

		for (i=0; i<nitems; i++) {
			if (items[i] && (items[i]->tag != opi.tag_CONDITIONAL)) {
				nocc_error ("occampi_typecheck_snode(): item not CONDITIONAL! (was [%s])", items[i]->tag->name);
				return 0;
			} else if (items[i]) {
				tnode_t *ctype = NULL;

				typecheck_subtree (tnode_nthsubof (items[i], 0), tc);		/* check constant case */
				typecheck_subtree (tnode_nthsubof (items[i], 1), tc);		/* check process */

				ctype = typecheck_gettype (tnode_nthsubof (items[i], 0), definttype);
				if (!ctype || !typecheck_typeactual (definttype, ctype, items[i], tc)) {
					typecheck_error (items[i], tc, "case constant is non-integer");
				}
			}
		}

		tnode_setnthsub (node, 2, swtype);
		tnode_free (definttype);

		return 0;
	}
	return 1;
}
/*}}}*/
/*{{{  static int occampi_namemap_snode (compops_t *cops, tnode_t **nodep, map_t *map)*/
/*
 *	does name-mapping for structured process nodes
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_namemap_snode (compops_t *cops, tnode_t **nodep, map_t *map)
{
	tnode_t *glist = tnode_nthsubof (*nodep, 1);
	int extraslots = 1;		/* FIXME: depends */

	if ((*nodep)->tag == opi.tag_IF) {
		/* FIXME: name-mapping for IF */
	} else if ((*nodep)->tag == opi.tag_ALT) {
		/*{{{  ALTing process -- do guards and bodies one-by-one*/
		int nguards, i;
		tnode_t **guards = parser_getlistitems (glist, &nguards);

		for (i=0; i<nguards; i++) {
			map_submapnames (guards + i, map);
		}

		/* ALT itself needs a bit of space */
		*nodep = map->target->newname (*nodep, NULL, map, map->target->aws.as_alt + (extraslots * map->target->slotsize), map->target->bws.ds_altio, 0, 0, 0, 0);

		/*}}}*/
		return 0;
	}
	return 1;
}
/*}}}*/
/*{{{  static int occampi_codegen_snode (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	does code-generation for structured process nodes
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_codegen_snode (compops_t *cops, tnode_t *node, codegen_t *cgen)
{
	if (node->tag == opi.tag_IF) {
		/*{{{  structured IF -- list of condition-process*/
		tnode_t *body = tnode_nthsubof (node, 1);
		tnode_t **bodies;
		int nbodies, i;
		int joinlab = codegen_new_label (cgen);

		if (!parser_islistnode (body)) {
			nocc_error ("occampi_codegen_snode(): body of IF not list!");
			return 0;
		}
		bodies = parser_getlistitems (body, &nbodies);

		for (i=0; i<nbodies; i++) {
			tnode_t *ifcond = tnode_nthsubof (bodies[i], 0);
			tnode_t *ifbody = tnode_nthsubof (bodies[i], 1);
			int skiplab = codegen_new_label (cgen);

			codegen_callops (cgen, loadname, ifcond, 0);
			codegen_callops (cgen, branch, I_CJ, skiplab);
			codegen_subcodegen (ifbody, cgen);
			codegen_callops (cgen, branch, I_J, joinlab);
			codegen_callops (cgen, setlabel, skiplab);
		}

		codegen_callops (cgen, tsecondary, I_SETERR);
		codegen_callops (cgen, setlabel, joinlab);
		/*}}}*/
	} else if (node->tag == opi.tag_ALT) {
		/*{{{  ALTing process -- alt-start, enabling, wait, disabling, alt-end*/
		int nguards, i;
		tnode_t **guards = parser_getlistitems (tnode_nthsubof (node, 1), &nguards);
		int *p_labels, *d_labels;
		int joinlab = codegen_new_label (cgen);
		int have_timeout_guard = 0;

		/*{{{  invent some labels for ALT bodies*/
		p_labels = (int *)smalloc (nguards * sizeof (int));
		d_labels = (int *)smalloc (nguards * sizeof (int));

		for (i=0; i<nguards; i++) {
			p_labels[i] = codegen_new_label (cgen);
			d_labels[i] = codegen_new_label (cgen);
		}

		/*}}}*/
		/*{{{  ALT start*/
		if (tnode_haslangop (node->tag->ndef->lops, "codegen_altstart")) {
			tnode_calllangop (node->tag->ndef->lops, "codegen_altstart", 2, node, cgen);
		}

		/*}}}*/
		/*{{{  ALT enabling sequence*/
		for (i=0; i<nguards; i++) {
			if (tnode_haslangop_i (guards[i]->tag->ndef->lops, (int)LOPS_CODEGEN_ALTENABLE)) {
				tnode_calllangop_i (guards[i]->tag->ndef->lops, (int)LOPS_CODEGEN_ALTENABLE, 3, guards[i], d_labels[i], cgen);
			} else {
				nocc_warning ("occampi_codegen_snode(): don\'t know how to generate ALT enable code for (%s,%s)", guards[i]->tag->name, guards[i]->tag->ndef->name);
			}
		}

		/*}}}*/
		/*{{{  ALT wait*/
		if (tnode_haslangop (node->tag->ndef->lops, "codegen_altwait")) {
			tnode_calllangop (node->tag->ndef->lops, "codegen_altwait", 2, node, cgen);
		}

		/*}}}*/
		/*{{{  ALT disabling sequence*/
		for (i--; i >= 0; i--) {
			if (tnode_haslangop_i (guards[i]->tag->ndef->lops, (int)LOPS_CODEGEN_ALTDISABLE)) {
				tnode_calllangop_i (guards[i]->tag->ndef->lops, (int)LOPS_CODEGEN_ALTDISABLE, 4, guards[i], d_labels[i], p_labels[i], cgen);
			} else {
				nocc_warning ("occampi_codegen_snode(): don\'t know how to generate ALT disable code for (%s,%s)", guards[i]->tag->name, guards[i]->tag->ndef->name);
			}
		}

		/*}}}*/
		/*{{{  ALT end*/
		if (tnode_haslangop (node->tag->ndef->lops, "codegen_altend")) {
			tnode_calllangop (node->tag->ndef->lops, "codegen_altend", 2, node, cgen);
		}

		/*}}}*/

		/*{{{  generate code for guarded processes*/
		for (i=0; i<nguards; i++) {
			codegen_callops (cgen, setlabel, p_labels[i]);

			if (guards[i]->tag == opi.tag_INPUTGUARD) {
				codegen_subcodegen (tnode_nthsubof (guards[i], 0), cgen);		/* generate input */
				codegen_subcodegen (tnode_nthsubof (guards[i], 1), cgen);		/* generate body */
				codegen_callops (cgen, branch, I_J, joinlab);
			}
		}

		/*}}}*/
		codegen_callops (cgen, setlabel, joinlab);

		/*}}}*/
	}
	return 0;
}
/*}}}*/


/*{{{  static int occampi_snode_init_nodes (void)*/
/*
 *	initailises structured process nodes for occam-pi
 *	returns 0 on success, non-zero on failure
 */
static int occampi_snode_init_nodes (void)
{
	tndef_t *tnd;
	int i;
	compops_t *cops;
	langops_t *lops;

	/*{{{  guardexphook -- compiler hook*/
	guardexphook = tnode_lookupornewchook ("occampi:guardexphook");
	guardexphook->chook_dumptree = occampi_guardexphook_dumptree;
	guardexphook->chook_free = occampi_guardexphook_free;
	guardexphook->chook_copy = occampi_guardexphook_copy;

	/*}}}*/
	/*{{{  ALT codegen language ops*/
	tnode_newlangop ("codegen_altstart", LOPS_INVALID, 2, (void *)&occampi_parser);
	tnode_newlangop ("codegen_altwait", LOPS_INVALID, 2, (void *)&occampi_parser);
	tnode_newlangop ("codegen_altend", LOPS_INVALID, 2, (void *)&occampi_parser);

	/*}}}*/
	/*{{{  occampi:snode -- IF, ALT, CASE*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:snode", &i, 3, 0, 0, TNF_LONGPROC);		/* subnodes: 0 = expr, 1 = body, 2 = type-of-expression */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (occampi_typecheck_snode));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (occampi_namemap_snode));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (occampi_codegen_snode));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "codegen_altstart", 2, LANGOPTYPE (occampi_codegen_altstart));
	tnode_setlangop (lops, "codegen_altwait", 2, LANGOPTYPE (occampi_codegen_altwait));
	tnode_setlangop (lops, "codegen_altend", 2, LANGOPTYPE (occampi_codegen_altend));
	tnd->lops = lops;

	i = -1;
	opi.tag_ALT = tnode_newnodetag ("ALT", &i, tnd, NTF_INDENTED_GUARDPROC_LIST);
	i = -1;
	opi.tag_IF = tnode_newnodetag ("IF", &i, tnd, NTF_INDENTED_CONDPROC_LIST);
	i = -1;
	opi.tag_CASE = tnode_newnodetag ("CASE", &i, tnd, NTF_INDENTED_CONDPROC_LIST);

	/*}}}*/
	/*{{{  occampi:condnode -- CONDITIONAL*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:condnode", &i, 2, 0, 0, TNF_LONGPROC);	/* subnodes: 0 = expr; 1 = body */
	cops = tnode_newcompops ();
	tnd->ops = cops;

	i = -1;
	opi.tag_CONDITIONAL = tnode_newnodetag ("CONDITIONAL", &i, tnd, NTF_INDENTED_PROC);
	/*}}}*/
	/*{{{  occampi:guardnode -- SKIPGUARD, INPUTGUARD, TIMERGUARD*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:guardnode", &i, 3, 0, 0, TNF_LONGPROC);	/* subnodes: 0 = guard-expr, 1 = body, 2 = pre-condition */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "betrans", 2, COMPOPTYPE (occampi_betrans_guardnode));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (occampi_namemap_guardnode));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "codegen_altenable", 3, LANGOPTYPE (occampi_codegen_altenable_guardnode));
	tnode_setlangop (lops, "codegen_altdisable", 4, LANGOPTYPE (occampi_codegen_altdisable_guardnode));
	tnd->lops = lops;

	i = -1;
	opi.tag_SKIPGUARD = tnode_newnodetag ("SKIPGUARD", &i, tnd, NTF_INDENTED_PROC);
	i = -1;
	opi.tag_INPUTGUARD = tnode_newnodetag ("INPUTGUARD", &i, tnd, NTF_INDENTED_PROC);
	i = -1;
	opi.tag_TIMERGUARD = tnode_newnodetag ("TIMERGUARD", &i, tnd, NTF_INDENTED_PROC);

	/*}}}*/

	return 0;
}
/*}}}*/
/*{{{  static int occampi_snode_reg_reducers (void)*/
/*
 *	registers reducers for structured process nodes
 */
static int occampi_snode_reg_reducers (void)
{
	parser_register_grule ("opi:altsnode", parser_decode_grule ("ST0T+@t000C3R-", opi.tag_ALT));
	parser_register_grule ("opi:casenode", parser_decode_grule ("ST0T+@t0N+V0C3R-", opi.tag_CASE));
	parser_register_grule ("opi:ifcond", parser_decode_grule ("SN0N+0C2R-", opi.tag_CONDITIONAL));
	parser_register_grule ("opi:skipguard", parser_decode_grule ("ST0T+@t00N+C3R-", opi.tag_SKIPGUARD));
	parser_register_grule ("opi:inputguard", parser_decode_grule ("SN0N+0N+C3R-", opi.tag_INPUTGUARD));
	parser_register_grule ("opi:timerguard", parser_decode_grule ("SN0N+0N+C3R-", opi.tag_TIMERGUARD));

	return 0;
}
/*}}}*/
/*{{{  static dfattbl_t **occampi_snode_init_dfatrans (int *ntrans)*/
/*
 *	creates and returns DFA transition tables for structured process nodes
 */
static dfattbl_t **occampi_snode_init_dfatrans (int *ntrans)
{
	DYNARRAY (dfattbl_t *, transtbl);

	dynarray_init (transtbl);
	dynarray_add (transtbl, dfa_transtotbl ("occampi:snode +:= [ 0 +@ALT 1 ] [ 0 +@CASE 3 ] [ 1 -Newline 2 ] [ 2 {<opi:altsnode>} -* ] [ 3 occampi:expr 4 ] [ 4 -Newline 5 ] [ 5 {<opi:casenode>} -* ]"));
	dynarray_add (transtbl, dfa_transtotbl ("occampi:ifcond ::= [ 0 occampi:expr 1 ] [ 1 -Newline 2 ] [ 2 {<opi:ifcond>} -* ]"));
	dynarray_add (transtbl, dfa_transtotbl ("occampi:subaltinputguard ::= [ 0 occampi:name 1 ] [ 1 -* <occampi:namestartname> ]"));
	dynarray_add (transtbl, dfa_transtotbl ("occampi:altinputguard ::= [ 0 occampi:subaltinputguard 1 ] [ 1 {<opi:inputguard>} -* ]"));
	dynarray_add (transtbl, dfa_transtotbl ("occampi:altguard ::= [ 0 +@SKIP 1 ] [ 0 +Name 12 ] [ 0 -* 3 ] [ 1 {<opi:nullpush>} -* 2 ] [ 2 {<opi:skipguard>} -* ] " \
				"[ 3 +@@? 4 ] [ 3 @@& 5 ] [ 4 {<opi:nullpush>} -* 8 ] " \
				"[ 5 +@SKIP 6 ] [ 5 occampi:expr 7 ] [ 6 {<opi:skipguard>} -* ] " \
				"[ 7 +@@? 8 ] [ 8 @AFTER 9 ] [ 9 occampi:expr 10 ] [ 10 {<opi:timerguard>} -* ] " \
				"[ 11 occampi:expr 3 ] [ 12 +@@? 13 ] [ 12 -* 16 ] [ 13 +Name 14 ] [ 13 -* 16 ] [ 14 +@@: 15 ] [ 14 +@@, 15 ] [ 14 -* 16 ] " \
				"[ 15 {<parser:rewindtokens>} -* <occampi:vardecl> ] [ 16 {<parser:rewindtokens>} -* 17 ] [ 17 {<opi:nullpush>} -* <occampi:altinputguard> ]"));

	*ntrans = DA_CUR (transtbl);
	return DA_PTR (transtbl);
}
/*}}}*/


/*{{{  occampi_snode_feunit (feunit_t)*/
feunit_t occampi_snode_feunit = {
	init_nodes: occampi_snode_init_nodes,
	reg_reducers: occampi_snode_reg_reducers,
	init_dfatrans: occampi_snode_init_dfatrans,
	post_setup: NULL
};
/*}}}*/


