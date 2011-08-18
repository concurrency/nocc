/*
 *	mcsp_snode.c -- structured nodes for MCSP (choice)
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
#include "origin.h"
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
/*{{{  private data*/

/* this is a chook attached to guard-nodes that indicates what needs to be enabled */
static chook_t *guardexphook = NULL;


/*}}}*/


/*{{{  static void mcsp_guardexphook_dumptree (tnode_t *node, void *chook, int indent, FILE *stream)*/
/*
 *	display the contents of a guardexphook compiler hook (just a node)
 */
static void mcsp_guardexphook_dumptree (tnode_t *node, void *chook, int indent, FILE *stream)
{
	if (chook) {
		mcsp_isetindent (stream, indent);
		fprintf (stream, "<mcsp:guardexphook addr=\"0x%8.8x\">\n", (unsigned int)chook);
		tnode_dumptree ((tnode_t *)chook, indent + 1, stream);
		mcsp_isetindent (stream, indent);
		fprintf (stream, "</mcsp:guardexphook>\n");
	}
	return;
}
/*}}}*/
/*{{{  static void mcsp_guardexphook_free (void *chook)*/
/*
 *	frees a guardexphook
 */
static void mcsp_guardexphook_free (void *chook)
{
	return;
}
/*}}}*/
/*{{{  static void *mcsp_guardexphook_copy (void *chook)*/
/*
 *	copies a guardexphook
 */
static void *mcsp_guardexphook_copy (void *chook)
{
	return chook;
}
/*}}}*/




/*{{{  static int mcsp_fetrans_snode (compops_t *cops, tnode_t **node, fetrans_t *fe)*/
/*
 *	does front-end transforms on an ALT/IF (ALT pass 1+ only)
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_fetrans_snode (compops_t *cops, tnode_t **node, fetrans_t *fe)
{
	mcsp_fetrans_t *mfe = (mcsp_fetrans_t *)fe->langpriv;
	tnode_t *t = *node;

	switch (mfe->parse) {
	case 0:
		/* nothing in this pass! */
		break;
	case 1:
		if (t->tag == mcsp.tag_ALT) {
			tnode_t *glist = tnode_nthsubof (t, 0);
			tnode_t **guards;
			int nguards, i;

			guards = parser_getlistitems (glist, &nguards);
			for (i=0; i<nguards; i++) {
				if (guards[i] && (guards[i]->tag == mcsp.tag_ALT)) {
					tnode_t *xglist;
					tnode_t **xguards;
					int nxguards, j;

					/* flatten */
					fetrans_subtree (&guards[i], fe);
					/* scoop out guards and add to ours */

					xglist = tnode_nthsubof (guards[i], 0);
					xguards = parser_getlistitems (xglist, &nxguards);
					for (j=0; j<nxguards; j++) {
						if (xguards[j] && (xguards[j]->tag == mcsp.tag_GUARD)) {
							/* add this one */
							parser_addtolist (glist, xguards[j]);
							xguards[j] = NULL;
						} else if (xguards[j]) {
							nocc_error ("mcsp_fetrans_snode(): unexpected tag [%s] while flattening ALT guards", xguards[j]->tag->name);
							mfe->errcount++;
						}
					}

					/* assume we got them all (or errored) */
					tnode_free (guards[i]);
					guards[i] = NULL;
				} else if (guards[i] && (guards[i]->tag != mcsp.tag_GUARD)) {
					nocc_error ("mcsp_fetrans_snode(): unexpected tag [%s] in ALT guards", guards[i]->tag->name);
					mfe->errcount++;
				} else {
					/* better do inside guard! */
					fetrans_subtree (&guards[i], fe);
				}
			}

			/* clean up alt list */
			parser_cleanuplist (glist);

			return 0;
		}
		break;
	}
	return 1;
}
/*}}}*/
/*{{{  static int mcsp_mwsynctrans_snode (compops_t *cops, tnode_t **tptr, mwsynctrans_t *mwi)*/
/*
 *	does multiway synchronisation transforms for a structured process node (ALTs)
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_mwsynctrans_snode (compops_t *cops, tnode_t **tptr, mwsynctrans_t *mwi)
{
	if ((*tptr)->tag == mcsp.tag_ALT) {
		/*{{{  ALTing process -- look for EVENT guards*/
		tnode_t *glist = tnode_nthsubof (*tptr, 0);
		int nguards, i;
		tnode_t **guards = parser_getlistitems (glist, &nguards);
		mwsyncaltinfo_t *altinf = mwsync_newmwsyncaltinfo ();

		// mwsync_transsubtree (tnode_nthsubaddr (*tptr, 0), mwi);

		for (i=0; i<nguards; i++) {
			tnode_t *guard = NULL;

			mwsync_transsubtree (guards + i, mwi);
#if 0
			nocc_message ("mcsp_mwsynctrans_snode(): did trans on guard, got back:");
			tnode_dumptree (guards[i], 1, stderr);
#endif
			
			if ((guards[i]->tag == mcsp.tag_GUARD) && (tnode_nthsubof (guards[i], 0)->tag == mcsp.tag_EVENT)) {
				altinf->bcount++;
			} else {
				altinf->nbcount++;
			}
		}

		mwsync_setaltinfo (*tptr, altinf);

		/*}}}*/
	}

	return 1;
}
/*}}}*/
/*{{{  static int mcsp_namemap_snode (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	does name-mapping for a structured node (IF, ALT)
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_namemap_snode (compops_t *cops, tnode_t **node, map_t *map)
{
	tnode_t *glist = tnode_nthsubof (*node, 0);
	tnode_t **guards;
	int nguards, i;
	int extraslots = 1;

	if ((*node)->tag == mcsp.tag_ALT) {
		/*{{{  ALTing process -- do guards and bodies one by one*/
		/* do guards one-by-one */
		guards = parser_getlistitems (glist, &nguards);
#if 0
		nocc_message ("mcsp_namemap_snode(): here! %d guards", nguards);
#endif
		for (i=0; i<nguards; i++) {

#if 0
			nocc_message ("mcsp_namemap_snode(): guard %d is:", i);
			tnode_dumptree (guard, 1, stderr);
#endif
			map_submapnames (guards + i, map);
		}

		/* ALT itself needs a bit of space */
		*node = map->target->newname (*node, NULL, map, map->target->aws.as_alt + (extraslots * map->target->slotsize), map->target->bws.ds_altio, 0, 0, 0, 0);

		return 0;
		/*}}}*/
	}
	return 1;
}
/*}}}*/
/*{{{  static int mcsp_codegen_snode (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	does code-generation for a structured node (IF, ALT)
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_codegen_snode (compops_t *cops, tnode_t *node, codegen_t *cgen)
{
	tnode_t *glist = tnode_nthsubof (node, 0);
	tnode_t **guards;
	int nguards, i;
	int *labels;
	int *dlabels;
	// int chosen_slot = cgen->target->aws.as_alt;

	if (node->tag == mcsp.tag_ALT) {
		int resumelab = codegen_new_label (cgen);

		guards = parser_getlistitems (glist, &nguards);

		/*{{{  invent some labels for guarded processes and disabling sequences*/
		labels = (int *)smalloc (nguards * sizeof (int));
		dlabels = (int *)smalloc (nguards * sizeof (int));

		for (i=0; i<nguards; i++) {
			labels[i] = codegen_new_label (cgen);
			dlabels[i] = codegen_new_label (cgen);
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
				tnode_calllangop_i (guards[i]->tag->ndef->lops, (int)LOPS_CODEGEN_ALTENABLE, 3, guards[i], dlabels[i], cgen);
			} else {
				nocc_warning ("mcsp_codegen_snode(): don\'t know how to generate ALT enable code for (%s,%s)", guards[i]->tag->name, guards[i]->tag->ndef->name);
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
				tnode_calllangop_i (guards[i]->tag->ndef->lops, (int)LOPS_CODEGEN_ALTDISABLE, 4, guards[i], dlabels[i], labels[i], cgen);
			} else {
				nocc_warning ("mcsp_codegen_snode(): don\'t know how to generate ALT disable code for (%s,%s)", guards[i]->tag->name, guards[i]->tag->ndef->name);
			}
		}

		/*}}}*/
		/*{{{  ALT end*/
		if (tnode_haslangop (node->tag->ndef->lops, "codegen_altend")) {
			tnode_calllangop (node->tag->ndef->lops, "codegen_altend", 2, node, cgen);
		}

		/*}}}*/

		/*{{{  guarded processes*/
		for (i=0; i<nguards; i++) {
			tnode_t *guard = guards[i];

			if (guard && (guard->tag == mcsp.tag_GUARD)) {
				codegen_callops (cgen, setlabel, labels[i]);
				codegen_subcodegen (tnode_nthsubof (guard, 1), cgen);
				codegen_callops (cgen, branch, I_J, resumelab);
			}
		}
		/*}}}*/
		/*{{{  next!*/
		codegen_callops (cgen, setlabel, resumelab);

		/*}}}*/
		/*{{{  cleanup*/
		sfree (dlabels);
		sfree (labels);

		/*}}}*/

		return 0;
	}
	return 1;
}
/*}}}*/
/*{{{  static int mcsp_codegen_altstart (langops_t *lops, tnode_t *node, codegen_t *cgen)*/
/*
 *	generates code for MCSP ALT start
 *	returns 0 on success, non-zero on failure
 */
static int mcsp_codegen_altstart (langops_t *lops, tnode_t *node, codegen_t *cgen)
{
	mwsyncaltinfo_t *altinf = mwsync_getaltinfo (node);

#if 0
	nocc_message ("mcsp_codegen_altstart(): altinf at 0x%8.8x, bcount = %d", (unsigned int)altinf, altinf ? altinf->bcount : 0);
#endif
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
/*{{{  static int mcsp_codegen_altwait (langops_t *lops, tnode_t *node, codegen_t *cgen)*/
/*
 *	generates code for basic MCSP ALT wait
 *	returns 0 on success, non-zero on failure
 */
static int mcsp_codegen_altwait (langops_t *lops, tnode_t *node, codegen_t *cgen)
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
/*{{{  static int mcsp_codegen_altend (langops_t *lops, tnode_t *node, codegen_t *cgen)*/
/*
 *	generates code for MCSP ALT end
 *	returns 0 on success, non-zero on failure
 */
static int mcsp_codegen_altend (langops_t *lops, tnode_t *node, codegen_t *cgen)
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



/*{{{  static int mcsp_postcheck_guardnode (compops_t *cops, tnode_t **node, postcheck_t *pc)*/
/*
 *	does post-checking transform on a GUARD
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_postcheck_guardnode (compops_t *cops, tnode_t **node, postcheck_t *pc)
{
	if ((*node)->tag == mcsp.tag_GUARD) {
		/* don't walk LHS */
		postcheck_subtree (tnode_nthsubaddr (*node, 1), pc);
		return 0;
	}
	return 1;
}
/*}}}*/
/*{{{  static int mcsp_fetrans_guardnode (compops_t *cops, tnode_t **node, fetrans_t *fe)*/
/*
 *	does front-end transformation on a GUARD
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_fetrans_guardnode (compops_t *cops, tnode_t **node, fetrans_t *fe)
{
	mcsp_fetrans_t *mfe = (mcsp_fetrans_t *)fe->langpriv;

	switch (mfe->parse) {
	case 0:
		if ((*node)->tag == mcsp.tag_GUARD) {
			/* don't walk LHS */
			fetrans_subtree (tnode_nthsubaddr (*node, 1), fe);
			return 0;
		}
		break;
	case 1:
		/* nothing in this pass! */
		break;
	case 2:
		if ((*node)->tag == mcsp.tag_GUARD) {
			/*{{{  add event to current alphabet*/
			tnode_t *event = tnode_nthsubof (*node, 0);

			if (mfe->curalpha && (event->tag == mcsp.tag_EVENT)) {
				mcsp_addtoalpha (mfe->curalpha, event);
			}
			/*}}}*/
		}
		break;
	}
	return 1;
}
/*}}}*/
/*{{{  static int mcsp_betrans_guardnode (compops_t *cops, tnode_t **node, betrans_t *be)*/
/*
 *	does back-end transformation on a GUARD
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_betrans_guardnode (compops_t *cops, tnode_t **node, betrans_t *be)
{
	if ((*node)->tag == mcsp.tag_GUARD) {
		tnode_t *event = tnode_nthsubof (*node, 0);

#if 0
		nocc_message ("mcsp_betrans_guardnode(): setting guardexphook to event =");
		tnode_dumptree (event, 1, stderr);
#endif
		tnode_setchook (*node, guardexphook, (void *)event);
	}
	return 1;
}
/*}}}*/
/*{{{  static int mcsp_namemap_guardnode (compops_t *cops, tnode_t **nodep, map_t *map)*/
/*
 *	does name-mapping for an ALT guard
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_namemap_guardnode (compops_t *cops, tnode_t **nodep, map_t *map)
{
	tnode_t *guardexp = (tnode_t *)tnode_getchook (*nodep, guardexphook);

#if 0
	nocc_message ("mcsp_namemap_guardnode(): here!");
#endif
	if (guardexp) {
		map_submapnames (&guardexp, map);
#if 0
		nocc_message ("mcsp_namemap_guardnode(): mapped guardexphook and got:");
		tnode_dumptree (guardexp, 1, stderr);
#endif
		tnode_setchook (*nodep, guardexphook, (void *)guardexp);
		tnode_setchook (*nodep, map->allocevhook, (void *)guardexp);
	}

	return 1;
}
/*}}}*/
/*{{{  static int mcsp_codegen_altenable_guardnode (langops_t *lops, tnode_t *guard, int dlabel, codegen_t *cgen)*/
/*
 *	does code-generation for SYNC guard ALT enable
 *	returns 0 on success, non-zero on failure
 */
static int mcsp_codegen_altenable_guardnode (langops_t *lops, tnode_t *guard, int dlabel, codegen_t *cgen)
{
	if (guard->tag == mcsp.tag_GUARD) {
		//tnode_t *precond = tnode_nthsubof (guard, 2);
		tnode_t *precond = NULL;

		codegen_callops (cgen, loadpointer, tnode_nthsubof (guard, 0), 0);
		if (precond) {
			nocc_warning ("mcsp_codegen_altenable_guardnode(): don\'t handle preconditions here!");
		}
		codegen_callops (cgen, loadlabaddr, dlabel);
		codegen_callops (cgen, tsecondary, I_MWS_ENB);
		codegen_callops (cgen, trashistack);
	} else {
		/* down-stream alt-enable */
		if (tnode_haslangop (lops->next, "codegen_altenable")) {
			return tnode_calllangop (lops->next, "codegen_altenable", 3, guard, dlabel, cgen);
		}
	}
	return 0;
}
/*}}}*/
/*{{{  static int mcsp_codegen_altdisable_guardnode (langops_t *lops, tnode_t *guard, int dlabel, int plabel, codegen_t *cgen)*/
/*
 *	does code-generation for SYNC guard ALT disable
 *	returns 0 on success, non-zero on failure
 */
static int mcsp_codegen_altdisable_guardnode (langops_t *lops, tnode_t *guard, int dlabel, int plabel, codegen_t *cgen)
{
	if (guard->tag == mcsp.tag_GUARD) {
		//tnode_t *precond = tnode_nthsubof (guard, 2);
		tnode_t *precond = NULL;

		codegen_callops (cgen, setlabel, dlabel);
		codegen_callops (cgen, loadpointer, tnode_nthsubof (guard, 0), 0);
		if (precond) {
			nocc_warning ("mcsp_codegen_altdisable_guardnode(): don\'t handle preconditions here!");
		}
		codegen_callops (cgen, loadlabaddr, plabel);
		codegen_callops (cgen, tsecondary, I_MWS_DIS);
		codegen_callops (cgen, trashistack);
	} else {
		/* down-stream alt-disable */
		if (tnode_haslangop (lops->next, "codegen_altdisable")) {
			return tnode_calllangop (lops->next, "codegen_altdisable", 4, guard, dlabel, plabel, cgen);
		}
	}
	return 0;
}
/*}}}*/

#if 0
/*{{{  static int mcsp_codegen_altenable_guardnode (langops_t *lops, tnode_t *guard, int dlabel, codegen_t *cgen)*/
/*
 *	does code-generation for an ALT guard enable
 *	returns 0 on success, non-zero on failure
 */
static int mcsp_codegen_altenable_guardnode (langops_t *lops, tnode_t *guard, int dlabel, codegen_t *cgen)
{
	tnode_t *guardexpr = (tnode_t *)tnode_getchook (guard, guardexphook);

	if (guard->tag == mcsp.tag_GUARD) {
		if (!guardexpr) {
			nocc_internal ("mcsp_codegen_altenable_guardnode(): no guard expression on INPUTGUARD!");
		} else {
			// tnode_t *precond = tnode_nthsubof (guard, 2);
			tnode_t *precond = NULL;

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
/*{{{  static int mcsp_codegen_altdisable_guardnode (langops_t *lops, tnode_t *guard, int dlabel, int plabel, codegen_t *cgen)*/
/*
 *	does code-generation for an ALT guard disable
 *	returns 0 on success, non-zero on failure
 */
static int mcsp_codegen_altdisable_guardnode (langops_t *lops, tnode_t *guard, int dlabel, int plabel, codegen_t *cgen)
{
	tnode_t *guardexpr = (tnode_t *)tnode_getchook (guard, guardexphook);

	codegen_callops (cgen, setlabel, dlabel);
	if (guard->tag == mcsp.tag_GUARD) {
		if (!guardexpr) {
			nocc_internal ("mcsp_codegen_altdisable_guardnode(): guard expression on INPUTGUARD vanished!");
		} else {
			// tnode_t *precond = tnode_nthsubof (guard, 2);
			tnode_t *precond = NULL;

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
#endif

/*{{{  static int mcsp_snode_init_nodes (void)*/
/*
 *	initialises MCSP structured nodes
 *	returns 0 on success, non-zero on failure
 */
static int mcsp_snode_init_nodes (void)
{
	tndef_t *tnd;
	int i;
	compops_t *cops;
	langops_t *lops;


	/*{{{  guardexphook -- compiler hook*/
	guardexphook = tnode_lookupornewchook ("mcsp:guardexphook");
	guardexphook->chook_dumptree = mcsp_guardexphook_dumptree;
	guardexphook->chook_free = mcsp_guardexphook_free;
	guardexphook->chook_copy = mcsp_guardexphook_copy;

	/*}}}*/
	/*{{{  ALT codegen language ops*/
	tnode_newlangop ("codegen_altstart", LOPS_INVALID, 2, origin_langparser (&mcsp_parser));
	tnode_newlangop ("codegen_altwait", LOPS_INVALID, 2, origin_langparser (&mcsp_parser));
	tnode_newlangop ("codegen_altend", LOPS_INVALID, 2, origin_langparser (&mcsp_parser));

	/*}}}*/
	/*{{{  mcsp:snode -- ALT*/
	i = -1;
	tnd = mcsp.node_SNODE = tnode_newnodetype ("mcsp:snode", &i, 1, 0, 1, TNF_NONE);		/* subnodes: 0 = list of guards/nested ALTs; hooks: 0 = mcsp_alpha_t */
	tnd->hook_free = mcsp_alpha_hook_free;
	tnd->hook_copy = mcsp_alpha_hook_copy;
	tnd->hook_dumptree = mcsp_alpha_hook_dumptree;
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "fetrans", 2, COMPOPTYPE (mcsp_fetrans_snode));
	tnode_setcompop (cops, "mwsynctrans", 2, COMPOPTYPE (mcsp_mwsynctrans_snode));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (mcsp_namemap_snode));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (mcsp_codegen_snode));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "codegen_altstart", 2, LANGOPTYPE (mcsp_codegen_altstart));
	tnode_setlangop (lops, "codegen_altwait", 2, LANGOPTYPE (mcsp_codegen_altwait));
	tnode_setlangop (lops, "codegen_altend", 2, LANGOPTYPE (mcsp_codegen_altend));
	tnd->lops = lops;

	i = -1;
	mcsp.tag_ALT = tnode_newnodetag ("MCSPALT", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  mcsp:guardnode -- GUARD*/
	i = -1;
	tnd = tnode_newnodetype ("mcsp:guardnode", &i, 2, 0, 0, TNF_NONE);				/* subnodes: 0 = guard, 1 = process */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "postcheck", 2, COMPOPTYPE (mcsp_postcheck_guardnode));
	tnode_setcompop (cops, "fetrans", 2, COMPOPTYPE (mcsp_fetrans_guardnode));
	tnode_setcompop (cops, "betrans", 2, COMPOPTYPE (mcsp_betrans_guardnode));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (mcsp_namemap_guardnode));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "codegen_altenable", 3, LANGOPTYPE (mcsp_codegen_altenable_guardnode));
	tnode_setlangop (lops, "codegen_altdisable", 4, LANGOPTYPE (mcsp_codegen_altdisable_guardnode));
	tnd->lops = lops;

	i = -1;
	mcsp.tag_GUARD = tnode_newnodetag ("MCSPGUARD", &i, tnd, NTF_NONE);

	/*}}}*/

	return 0;
}
/*}}}*/
/*{{{  static int mcsp_snode_reg_reducers (void)*/
/*
 *	registers reducers for MCSP structured nodes
 *	returns 0 on success, non-zero on failure
 */
static int mcsp_snode_reg_reducers (void)
{
	return 0;
}
/*}}}*/
/*{{{  static dfattbl_t **mcsp_snode_init_dfatrans (int *ntrans)*/
/*
 *	creates and returns DFA transition tables for MCSP process nodes
 */
static dfattbl_t **mcsp_snode_init_dfatrans (int *ntrans)
{
	DYNARRAY (dfattbl_t *, transtbl);

	dynarray_init (transtbl);

	*ntrans = DA_CUR (transtbl);
	return DA_PTR (transtbl);
}
/*}}}*/




/*{{{  mcsp_snode_feunit (feunit_t)*/
feunit_t mcsp_snode_feunit = {
	.init_nodes = mcsp_snode_init_nodes,
	.reg_reducers = mcsp_snode_reg_reducers,
	.init_dfatrans = mcsp_snode_init_dfatrans,
	.post_setup = NULL,
	.ident = "mcsp-snode"
};
/*}}}*/

