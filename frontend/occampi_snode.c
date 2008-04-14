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
#include "origin.h"
#include "symbols.h"
#include "keywords.h"
#include "lexer.h"
#include "lexpriv.h"
#include "tnode.h"
#include "treeops.h"
#include "parser.h"
#include "dfa.h"
#include "parsepriv.h"
#include "origin.h"
#include "occampi.h"
#include "feunit.h"
#include "names.h"
#include "scope.h"
#include "prescope.h"
#include "typecheck.h"
#include "usagecheck.h"
#include "tracescheck.h"
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
static chook_t *branchlabelhook = NULL;
static chook_t *actionlhstypechook = NULL;

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

/*{{{  static void occampi_branchlabelhook_dumptree (tnode_t *node, void *chook, int indent, FILE *stream)*/
/*
 *	display the contents of a branchlabelhook compiler hook (numeric label)
 */
static void occampi_branchlabelhook_dumptree (tnode_t *node, void *chook, int indent, FILE *stream)
{
	if (chook) {
		occampi_isetindent (stream, indent);
		fprintf (stream, "<occampi:branchlabelhook label=\"%d\" />\n", (unsigned int)chook);
	}
	return;
}
/*}}}*/
/*{{{  static void occampi_branchlabelhook_free (void *chook)*/
/*
 *	frees a branchlabelhook
 */
static void occampi_branchlabelhook_free (void *chook)
{
	return;
}
/*}}}*/
/*{{{  static void *occampi_branchlabelhook_copy (void *chook)*/
/*
 *	copies a branchlabelhook
 */
static void *occampi_branchlabelhook_copy (void *chook)
{
	return chook;
}
/*}}}*/


/*{{{  static int occampi_typecheck_guardnode (compops_t *cops, tnode_t *tnode, typecheck_t *tc)*/
/*
 *	does type-checking on a guard-node -- will turn pending INPUTGUARDs into TIMERGUARDs if LHS is timer type
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_typecheck_guardnode (compops_t *cops, tnode_t *tnode, typecheck_t *tc)
{
	tnode_t *acttype;

	typecheck_subtree (tnode_nthsubof (tnode, 0), tc);
	typecheck_subtree (tnode_nthsubof (tnode, 1), tc);

	acttype = (tnode_t *)tnode_getchook (tnode_nthsubof (tnode, 0), actionlhstypechook);
	if (acttype && (tnode->tag == opi.tag_INPUTGUARD) && (acttype->tag == opi.tag_TIMER)) {
		tnode_changetag (tnode, opi.tag_TIMERGUARD);
	}
	return 0;
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
		tnode_t *timeout = treeops_findintree (tnode_nthsubof (*nodep, 0), opi.tag_TIMERINPUTAFTER);

		if (!timeout) {
			tnode_error (*nodep, "did not find TIMERINPUTAFTER in TIMERGUARD!");
		} else {
			tnode_t *lhs = tnode_nthsubof (timeout, 1);

			tnode_setchook (*nodep, guardexphook, (void *)lhs);
		}
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

	codegen_callops (cgen, debugline, guard);
	if (guard->tag == opi.tag_INPUTGUARD) {
		if (!guardexpr) {
			nocc_internal ("occampi_codegen_altenable_guardnode(): no guard expression on INPUTGUARD!");
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
	} else if (guard->tag == opi.tag_TIMERGUARD) {
		if (!guardexpr) {
			nocc_internal ("occampi_codegen_altenable_guardnode(): no guard expression on TIMERGUARD!");
		} else {
			tnode_t *precond = tnode_nthsubof (guard, 2);

			/* FIXME: code for timeout enable */
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

	codegen_callops (cgen, debugline, guard);
	codegen_callops (cgen, setlabel, dlabel);
	if (guard->tag == opi.tag_INPUTGUARD) {
		if (!guardexpr) {
			nocc_internal ("occampi_codegen_altdisable_guardnode(): guard expression on INPUTGUARD vanished!");
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
	} else if (guard->tag == opi.tag_TIMERGUARD) {
		if (!guardexpr) {
			nocc_internal ("occampi_codegen_altdisable_guardnode(): no guard expression on TIMERGUARD!");
		} else {
			tnode_t *precond = tnode_nthsubof (guard, 2);

			/* FIXME: code for timeout enable */
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
		/*{{{  do type-check for CASE*/
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
		/*}}}*/
	} else if (node->tag == opi.tag_IF) {
		/*{{{  do type-check for IF*/
		tnode_t *defbooltype = tnode_create (opi.tag_BOOL, NULL);
		tnode_t *definttype = tnode_create (opi.tag_INT, NULL);
		tnode_t *swtype = NULL;
		tnode_t **items;
		int nitems, i;

		items = parser_getlistitems (tnode_nthsubof (node, 1), &nitems);

		for (i=0; i<nitems; i++) {
			if (items[i] && (items[i]->tag != opi.tag_CONDITIONAL)) {
				nocc_error ("occampi_typecheck_snode(): item not CONDITIONAL! (was [%s])", items[i]->tag->name);
				return 0;
			} else if (items[i]) {
				tnode_t *ctype = NULL;

				typecheck_subtree (tnode_nthsubof (items[i], 0), tc);		/* check condition */
				typecheck_subtree (tnode_nthsubof (items[i], 1), tc);		/* check process */

				ctype = typecheck_gettype (tnode_nthsubof (items[i], 0), definttype);
				if (!ctype || (ctype->tag != opi.tag_BOOL)) {
					typecheck_error (items[i], tc, "condition is not boolean");
				}
			}
		}

		tnode_free (definttype);
		tnode_free (defbooltype);

		return 0;
		/*}}}*/
	}
	return 1;
}
/*}}}*/
/*{{{  static int occampi_tracescheck_snode (compops_t *cops, tnode_t *node, tchk_state_t *tcstate)*/
/*
 *	does traces checking on a structured node (IF/ALT/CASE)
 *	return 0 to stop walk, 1 to continue
 */
static int occampi_tracescheck_snode (compops_t *cops, tnode_t *node, tchk_state_t *tcstate)
{
	tchk_bucket_t *tcb;
	tchknode_t *tcn;
	int i;

	/* collect up individual items */
	tracescheck_pushbucket (tcstate);
	tracescheck_subtree (tnode_nthsubof (node, 1), tcstate);
	tcb = tracescheck_pullbucket (tcstate);

	if (node->tag == opi.tag_ALT) {
		tcn = tracescheck_createnode (TCN_DET, node, NULL);
	} else {
		tcn = tracescheck_createnode (TCN_NDET, node, NULL);
	}

	for (i=0; i<DA_CUR (tcb->items); i++) {
		tchknode_t *item = DA_NTHITEM (tcb->items, i);

		tracescheck_addtolistnode (tcn, item);
	}
	dynarray_trash (tcb->items);
	tracescheck_freebucket (tcb);

	tracescheck_addtobucket (tcstate, tcn);

	return 0;
}
/*}}}*/
/*{{{  static int occampi_namemap_snode (compops_t *cops, tnode_t **nodep, map_t *map)*/
/*
 *	does name-mapping for structured process nodes
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_namemap_snode (compops_t *cops, tnode_t **nodep, map_t *map)
{
	if ((*nodep)->tag == opi.tag_IF) {
		/*{{{  IF process -- nothing special here*/
		return 1;
		/*}}}*/
	} else if ((*nodep)->tag == opi.tag_CASE) {
		/*{{{  CASE process -- nothing special here */
		return 1;
		/*}}}*/
	} else if ((*nodep)->tag == opi.tag_ALT) {
		/*{{{  ALTing process -- do guards and bodies one-by-one*/
		tnode_t *glist = tnode_nthsubof (*nodep, 1);
		int extraslots = 1;		/* FIXME: depends */

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
	codegen_callops (cgen, debugline, node);

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
	} else if (node->tag == opi.tag_CASE) {
		/*{{{  CASE selection -- list of condition-process*/
		tnode_t *body = tnode_nthsubof (node, 1);
		tnode_t **bodies;
		int nbodies, i;
		int dfllab = codegen_new_label (cgen);
		int joinlab = codegen_new_label (cgen);
		int *blabs;

		if (!parser_islistnode (body)) {
			nocc_error ("occampi_codegen_snode(): body of CASE not list!");
			return 0;
		}
		bodies = parser_getlistitems (body, &nbodies);

		/* assign labels to bodies */
		blabs = (int *)smalloc (nbodies * sizeof (int));
		for (i=0; i<nbodies; i++) {
			blabs[i] = codegen_new_label (cgen);
			tnode_setchook (bodies[i], branchlabelhook, (void *)(blabs[i]));
		}

		/* FIXME: generate jump-table or other suitable mechanisms */

		codegen_callops (cgen, setlabel, dfllab);
		codegen_callops (cgen, tsecondary, I_SETERR);

		/* generate code bodies */
		for (i=0; i<nbodies; i++) {
			codegen_callops (cgen, setlabel, blabs[i]);
			codegen_subcodegen (tnode_nthsubof (bodies[i], 1), cgen);
			codegen_callops (cgen, branch, I_J, joinlab);
		}

		codegen_callops (cgen, setlabel, joinlab);

		/*}}}*/
	}
	return 0;
}
/*}}}*/


/*{{{  static int occampi_scopein_replsnode (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	scopes in a replicated structured node
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_scopein_replsnode (compops_t *cops, tnode_t **node, scope_t *ss)
{
	tnode_t *replname = tnode_nthsubof (*node, 2);
	tnode_t *type = tnode_create (opi.tag_INT, NULL);
	char *rawname;
	tnode_t *newname;
	name_t *sname = NULL;

	if (replname->tag != opi.tag_NAME) {
		scope_error (replname, ss, "occampi_scopein_replsnode(): name not raw-name!");
		return 0;
	}
	rawname = (char *)tnode_nthhookof (replname, 0);

	/* scope the start and length expressions */
	if (scope_subtree (tnode_nthsubaddr (*node, 3), ss)) {
		/* failed to scope start */
		return 0;
	}
	if (scope_subtree (tnode_nthsubaddr (*node, 4), ss)) {
		/* failed to scope length */
		return 0;
	}

	sname = name_addscopename (rawname, *node, type, NULL);
	newname = tnode_createfrom (opi.tag_NREPL, replname, sname);
	SetNameNode (sname, newname);
	tnode_setnthsub (*node, 2, newname);

	/* free the old name */
	tnode_free (replname);
	ss->scoped++;

	return 1;
}
/*}}}*/
/*{{{  static int occampi_scopeout_replsnode (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	scopes out a replicated structured node
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_scopeout_replsnode (compops_t *cops, tnode_t **node, scope_t *ss)
{
	tnode_t *replname = tnode_nthsubof (*node, 2);
	name_t *sname;

	if (replname->tag != opi.tag_NREPL) {
		scope_error (replname, ss, "not NREPL!");
		return 0;
	}
	sname = tnode_nthnameof (replname, 0);

	name_descopename (sname);

	return 1;
}
/*}}}*/
/*{{{  static int occampi_typecheck_replsnode (compops_t *cops, tnode_t *node, typecheck_t *tc)*/
/*
 *	does type-checking on a replicated structured node (REPLIF, REPLALT)
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_typecheck_replsnode (compops_t *cops, tnode_t *node, typecheck_t *tc)
{
	tnode_t *start = tnode_nthsubof (node, 3);
	tnode_t *length = tnode_nthsubof (node, 4);
	tnode_t *type;
	tnode_t *defaulttype = tnode_create (opi.tag_INT, NULL);

	/* typecheck start and length first */
	typecheck_subtree (start, tc);
	typecheck_subtree (length, tc);

	type = typecheck_gettype (start, defaulttype);
	if (!type || !typecheck_typeactual (defaulttype, type, node, tc)) {
		typecheck_error (node, tc, "replicator start must be integer");
	}

	type = typecheck_gettype (length, defaulttype);
	if (!type || !typecheck_typeactual (defaulttype, type, node, tc)) {
		typecheck_error (node, tc, "replicator length must be integer");
	}

	tnode_free (defaulttype);

	return 1;
}
/*}}}*/
/*{{{  static int occampi_constprop_replsnode (compops_t *cops, tnode_t **tptr)*/
/*
 *	does constant propagation for a replicated structured node (REPLIF, REPLALT)
 *	returns 0 to stop walk, 1 to continue (post walk)
 */
static int occampi_constprop_replsnode (compops_t *cops, tnode_t **tptr)
{
	return 1;
}
/*}}}*/
/*{{{  static int occampi_namemap_replsnode (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	does name-mapping for replicated structured nodes (REPLIF, REPLALT)
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_namemap_replsnode (compops_t *cops, tnode_t **node, map_t *map)
{
	tnode_t *orgnode = *node;
	tnode_t **namep = tnode_nthsubaddr (*node, 2);
	tnode_t **bodyp = tnode_nthsubaddr (*node, 1);
	int tsize = map->target->intsize;
	tnode_t *bename;

	/* map the start and length expressions first */
	map_submapnames (tnode_nthsubaddr (*node, 3), map);
	map_submapnames (tnode_nthsubaddr (*node, 4), map);

	bename = map->target->newname (*namep, *node, map, tsize * 2, 0, 0, 0, tsize, 0);
	tnode_setchook (*namep, map->mapchook, (void *)bename);
	*node = bename;

	/* map the name in the replicator, turning it into a NAMEREF */
	map_submapnames (namep, map);

	/* map the body (original, not what we just placed) */
	map_submapnames (bodyp, map);

	return 0;
}
/*}}}*/
/*{{{  static int occampi_codegen_replsnode (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	does code-generation for replicated structured nodes (REPLIF, REPLALT)
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_codegen_replsnode (compops_t *cops, tnode_t *node, codegen_t *cgen)
{
	tnode_t *replname = tnode_nthsubof (node, 2);
	tnode_t *start = tnode_nthsubof (node, 3);
	tnode_t *length = tnode_nthsubof (node, 4);
	int hlab, tlab;

	codegen_callops (cgen, loadname, start, 0);
	codegen_callops (cgen, storename, replname, 0);
	codegen_callops (cgen, loadname, length, 0);
	codegen_callops (cgen, storename, replname, cgen->target->intsize);

	hlab = codegen_new_label (cgen);
	tlab = codegen_new_label (cgen);

	codegen_callops (cgen, setlabel, hlab);
	codegen_callops (cgen, loadname, replname, cgen->target->intsize);
	codegen_callops (cgen, branch, I_CJ, tlab);
	
	/* generate the replicated body */
	codegen_subcodegen (tnode_nthsubof (node, 1), cgen);

	codegen_callops (cgen, loadname, replname, 0);
	codegen_callops (cgen, addconst, 1);
	codegen_callops (cgen, storename, replname, 0);
	codegen_callops (cgen, loadname, replname, cgen->target->intsize);
	codegen_callops (cgen, addconst, -1);
	codegen_callops (cgen, storename, replname, cgen->target->intsize);
	codegen_callops (cgen, branch, I_J, hlab);

	codegen_callops (cgen, setlabel, tlab);
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
	/*{{{  branchlabelhook -- compiler hook*/
	branchlabelhook = tnode_lookupornewchook ("occampi:branchlabelhook");
	branchlabelhook->chook_dumptree = occampi_branchlabelhook_dumptree;
	branchlabelhook->chook_free = occampi_branchlabelhook_free;
	branchlabelhook->chook_copy = occampi_branchlabelhook_copy;

	/*}}}*/
	/*{{{  ALT codegen language ops*/
	tnode_newlangop ("codegen_altstart", LOPS_INVALID, 2, origin_langparser (&occampi_parser));
	tnode_newlangop ("codegen_altwait", LOPS_INVALID, 2, origin_langparser (&occampi_parser));
	tnode_newlangop ("codegen_altend", LOPS_INVALID, 2, origin_langparser (&occampi_parser));

	/*}}}*/
	/*{{{  occampi:snode -- IF, ALT, CASE*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:snode", &i, 3, 0, 0, TNF_LONGPROC);		/* subnodes: 0 = expr, 1 = body, 2 = type-of-expression */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (occampi_typecheck_snode));
	tnode_setcompop (cops, "tracescheck", 2, COMPOPTYPE (occampi_tracescheck_snode));
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
	/*{{{  occampi:replsnode -- REPLIF, REPLALT*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:replsnode", &i, 5, 0, 0, TNF_LONGPROC);	/* subnodes: 0 = expr, 1 = body, 2 = name, 3 = start, 4 = length */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (occampi_scopein_replsnode));
	tnode_setcompop (cops, "scopeout", 2, COMPOPTYPE (occampi_scopeout_replsnode));
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (occampi_typecheck_replsnode));
	tnode_setcompop (cops, "constprop", 1, COMPOPTYPE (occampi_constprop_replsnode));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (occampi_namemap_replsnode));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (occampi_codegen_replsnode));
	tnd->ops = cops;
	lops = tnode_newlangops ();
#if 0
	tnode_setlangop (lops, "codegen_altstart", 2, LANGOPTYPE (occampi_replsnode_codegen_altstart));
	tnode_setlangop (lops, "codegen_altwait", 2, LANGOPTYPE (occampi_replsnode__codegen_altwait));
	tnode_setlangop (lops, "codegen_altend", 2, LANGOPTYPE (occampi_replsnode__codegen_altend));
#endif
	tnd->lops = lops;

	i = -1;
	opi.tag_REPLALT = tnode_newnodetag ("REPLALT", &i, tnd, NTF_INDENTED_GUARDPROC_LIST);
	i = -1;
	opi.tag_REPLIF = tnode_newnodetag ("REPLIF", &i, tnd, NTF_INDENTED_CONDPROC_LIST);

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
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (occampi_typecheck_guardnode));
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
/*{{{  static int occampi_snode_post_setup (void)*/
/*
 *	does post-setup for structured process nodes
 *	returns 0 on success, non-zero on failure
 */
static int occampi_snode_post_setup (void)
{
	actionlhstypechook = tnode_lookupchookbyname ("occampi:action:lhstype");
	if (!actionlhstypechook) {
		nocc_error ("occampi_snode_post_setup(): failed to find \"occampi:action:lhstype\" compiler-hook!");
		return 1;
	}
	return 0;
}
/*}}}*/


/*{{{  occampi_snode_feunit (feunit_t)*/
feunit_t occampi_snode_feunit = {
	init_nodes: occampi_snode_init_nodes,
	reg_reducers: NULL,
	init_dfatrans: NULL,
	post_setup: occampi_snode_post_setup,
	ident: "occampi-snode"
};
/*}}}*/


