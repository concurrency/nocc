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
		for (i=0; i<nguards; i++) {
			tnode_t *guard = guards[i];

			if (guard && (guard->tag == mcsp.tag_GUARD)) {
				tnode_t **eventp = tnode_nthsubaddr (guard, 0);
				tnode_t **bodyp = tnode_nthsubaddr (guard, 1);

				map_submapnames (eventp, map);
				map_submapnames (bodyp, map);
			}
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
	int chosen_slot = cgen->target->aws.as_alt;

	if (node->tag == mcsp.tag_ALT) {
		int resumelab = codegen_new_label (cgen);

		guards = parser_getlistitems (glist, &nguards);

		/*{{{  invent some labels for guarded processes and disabling sequences*/
		labels = (int *)smalloc (nguards * sizeof (int));
		dlabels = (int *)smalloc (nguards * sizeof (int));

		/*}}}*/
		/*{{{  ALT start*/
		codegen_callops (cgen, loadconst, -1);
		codegen_callops (cgen, storelocal, chosen_slot);

		codegen_callops (cgen, tsecondary, I_MWALT);

		/*}}}*/
		/*{{{  enabling sequence*/
		for (i=0; i<nguards; i++) {
			tnode_t *guard = guards[i];

			if (guard && (guard->tag == mcsp.tag_GUARD)) {
				tnode_t *event = tnode_nthsubof (guard, 0);

				/* drop in labels */
				labels[i] = codegen_new_label (cgen);
				dlabels[i] = codegen_new_label (cgen);

				codegen_callops (cgen, loadpointer, event, 0);
				codegen_callops (cgen, loadlabaddr, dlabels[i]);

				codegen_callops (cgen, tsecondary, I_MWENB);
			}
		}
		/*}}}*/
		/*{{{  ALT wait*/
		codegen_callops (cgen, tsecondary, I_MWALTWT);

		/*}}}*/
		/*{{{  disabling sequence -- backwards please!*/
		for (i=nguards - 1; i>=0; i--) {
			tnode_t *guard = guards[i];

			codegen_callops (cgen, setlabel, dlabels[i]);
			if (guard && (guard->tag == mcsp.tag_GUARD)) {
				tnode_t *event = tnode_nthsubof (guard, 0);

				codegen_callops (cgen, loadpointer, event, 0);
				codegen_callops (cgen, loadlabaddr, labels[i]);

				codegen_callops (cgen, tsecondary, I_MWDIS);
			}
		}
		/*}}}*/
		/*{{{  ALT end*/
		codegen_callops (cgen, tsecondary, I_MWALTEND);
		codegen_callops (cgen, tsecondary, I_SETERR);		/* if we fell of the ALT */

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

	/*{{{  mcsp:snode -- ALT*/
	i = -1;
	tnd = mcsp.node_SNODE = tnode_newnodetype ("mcsp:snode", &i, 1, 0, 1, TNF_NONE);		/* subnodes: 0 = list of guards/nested ALTs; hooks: 0 = mcsp_alpha_t */
	tnd->hook_free = mcsp_alpha_hook_free;
	tnd->hook_copy = mcsp_alpha_hook_copy;
	tnd->hook_dumptree = mcsp_alpha_hook_dumptree;
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "fetrans", 2, COMPOPTYPE (mcsp_fetrans_snode));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (mcsp_namemap_snode));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (mcsp_codegen_snode));
	tnd->ops = cops;

	i = -1;
	mcsp.tag_ALT = tnode_newnodetag ("MCSPALT", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  mcsp:guardnode -- GUARD*/
	i = -1;
	tnd = tnode_newnodetype ("mcsp:guardnode", &i, 2, 0, 0, TNF_NONE);				/* subnodes: 0 = guard, 1 = process */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "postcheck", 2, COMPOPTYPE (mcsp_postcheck_guardnode));
	tnode_setcompop (cops, "fetrans", 2, COMPOPTYPE (mcsp_fetrans_guardnode));
	tnd->ops = cops;

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
	init_nodes: mcsp_snode_init_nodes,
	reg_reducers: mcsp_snode_reg_reducers,
	init_dfatrans: mcsp_snode_init_dfatrans,
	post_setup: NULL
};
/*}}}*/

