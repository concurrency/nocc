/*
 *	guppy_timer.c -- timers for Guppy
 *	Copyright (C) 2013 Fred Barnes <frmb@kent.ac.uk>
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
#include "origin.h"
#include "symbols.h"
#include "keywords.h"
#include "lexer.h"
#include "lexpriv.h"
#include "tnode.h"
#include "parser.h"
#include "dfa.h"
#include "dfaerror.h"
#include "parsepriv.h"
#include "guppy.h"
#include "feunit.h"
#include "fcnlib.h"
#include "names.h"
#include "scope.h"
#include "prescope.h"
#include "library.h"
#include "typecheck.h"
#include "constprop.h"
#include "precheck.h"
#include "usagecheck.h"
#include "postcheck.h"
#include "map.h"
#include "target.h"
#include "codegen.h"
#include "langops.h"
#include "fetrans.h"
#include "betrans.h"
#include "metadata.h"
#include "cccsp.h"

/*}}}*/
/*{{{  private types/data*/

static tnode_t *guppy_timer_inttypenode;


/*}}}*/

/*{{{  static tnode_t *guppy_gettype_timertype (langops_t *lops, tnode_t *node, tnode_t *default_type)*/
/*
 *	gets the type of a timer (trivial)
 */
static tnode_t *guppy_gettype_timertype (langops_t *lops, tnode_t *node, tnode_t *default_type)
{
	return node;
}
/*}}}*/
/*{{{  static tnode_t *guppy_getsubtype_timertype (langops_t *lops, tnode_t *node, tnode_t *default_type)*/
/*
 *	gets the subtype of a timer (trivial -- INT or INT64)
 */
static tnode_t *guppy_getsubtype_timertype (langops_t *lops, tnode_t *node, tnode_t *default_type)
{
	return guppy_timer_inttypenode;
}
/*}}}*/
/*{{{  static tnode_t *guppy_typeactual_timertype (langops_t *lops, tnode_t *formaltype, tnode_t *actualtype, tnode_t *node, typecheck_t *tc)*/
/*
 *	does actual-type checking for a timer (called during I/O usage-check)
 *	returns actual type involved
 */
static tnode_t *guppy_typeactual_timertype (langops_t *lops, tnode_t *formaltype, tnode_t *actualtype, tnode_t *node, typecheck_t *tc)
{
	tnode_t *atype = NULL;

	if (formaltype->tag == gup.tag_TIMER) {
		if (node->tag == gup.tag_INPUT) {
			/* reading current time */
			atype = guppy_timer_inttypenode;
			atype = typecheck_typeactual (atype, actualtype, node, tc);
		} else {
			typecheck_error (node, tc, "impossible action [%s] on timer", node->tag->name);
		}
	} else {
		nocc_internal ("guppy_typeactual_timertype(): unhandled [%s]", formaltype->tag->name);
	}
	return atype;
}
/*}}}*/

/*{{{  static int guppy_typecheck_timerop (compops_t *cops, tnode_t *node, typecheck_t *tc)*/
/*
 *	does type-checking for a timer-op node (AFTER)
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_typecheck_timerop (compops_t *cops, tnode_t *node, typecheck_t *tc)
{
	tnode_t *op = tnode_nthsubof (node, 0);
	tnode_t *optype;

	typecheck_subtree (op, tc);
	optype = typecheck_gettype (op, guppy_timer_inttypenode);
	if (!optype) {
		typecheck_error (node, tc, "invalid type for timer operator");
		return 0;
	}
	tnode_setnthsub (node, 1, optype);

	return 0;
}
/*}}}*/
/*{{{  static tnode_t *guppy_gettype_timerop (langops_t *lops, tnode_t *node, tnode_t *default_type)*/
/* 
 *	gets the type of a timer-op node (AFTER)
 */
static tnode_t *guppy_gettype_timerop (langops_t *lops, tnode_t *node, tnode_t *default_type)
{
	return tnode_nthsubof (node, 1);
}
/*}}}*/

/*{{{  static int guppy_fetrans15_timeraction (compops_t *cops, tnode_t **nodep, guppy_fetrans15_t *fe15)*/
/*
 *	does fetrans1.5 on a timer action node (do nothing, don't look inside)
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_fetrans15_timeraction (compops_t *cops, tnode_t **nodep, guppy_fetrans15_t *fe15)
{
	return 0;
}
/*}}}*/
/*{{{  static int guppy_namemap_timeraction (compops_t *cops, tnode_t **nodep, map_t *map)*/
/*
 *	does name-mapping for a timer action node (turns into APICALL things)
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_namemap_timeraction (compops_t *cops, tnode_t **nodep, map_t *map)
{
	cccsp_mapdata_t *cmd = (cccsp_mapdata_t *)map->hook;

	if ((*nodep)->tag == gup.tag_TIMERREAD) {
		tnode_t *newinst;
		tnode_t *newparms;
		tnode_t *var;

		newparms = parser_newlistnode (SLOCI);
		parser_addtolist (newparms, cmd->process_id);
		map_submapnames (&newparms, map);
		var = tnode_nthsubof (*nodep, 0);
		map_submapnames (&var, map);

		newinst = tnode_createfrom (gup.tag_APICALLR, *nodep, cccsp_create_apicallname (TIMER_READ), newparms, var);

		*nodep = newinst;
		return 0;
	} else if ((*nodep)->tag == gup.tag_TIMERWAIT) {
		tnode_t *newinst;
		tnode_t *newparms;

		newparms = parser_newlistnode (SLOCI);
		parser_addtolist (newparms, cmd->process_id);
		parser_addtolist (newparms, tnode_nthsubof (*nodep, 0));
		map_submapnames (&newparms, map);

		newinst = tnode_createfrom (gup.tag_APICALL, *nodep, cccsp_create_apicallname (TIMER_WAIT), newparms);

		*nodep = newinst;
		return 0;
	} else {
		nocc_internal ("guppy_namemap_timeraction(): unhandled [%s]", (*nodep)->tag->name);
		return 0;
	}
	return 1;
}
/*}}}*/

/*{{{  static int guppy_timerdeclblock_postcheck (compops_t *cops, tnode_t **nodep, postcheck_t *pc)*/
/*
 *	does post-checks on a declaration block to remove timer declarations.
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_timerdeclblock_postcheck (compops_t *cops, tnode_t **nodep, postcheck_t *pc)
{
	if ((*nodep)->tag == gup.tag_DECLBLOCK) {
		tnode_t *vlist = tnode_nthsubof (*nodep, 0);
		int i;

		if (!parser_islistnode (vlist)) {
			nocc_internal ("guppy_timerdeclblock_postcheck(): DECLBLOCK set not list, got [%s]", vlist->tag->name);
			return 0;
		}
		for (i=0; i<parser_countlist (vlist); i++) {
			tnode_t *ditem = parser_getfromlist (vlist, i);

			if (ditem->tag == gup.tag_VARDECL) {
				tnode_t *dtype = tnode_nthsubof (ditem, 1);

				if (dtype->tag == gup.tag_TIMER) {
					/* this one! */
					parser_delfromlist (vlist, i);
					i--;
					continue;			/* for() */
				}
			}
		}
	}
	if (cops->next && tnode_hascompop_i (cops->next, (int)COPS_POSTCHECK)) {
		return tnode_callcompop_i (cops->next, (int)COPS_POSTCHECK, 2, nodep, pc);
	}
	return 1;
}
/*}}}*/
/*{{{  static int guppy_timerio_typeresolve (compops_t *cops, tnode_t **nodep, typecheck_t *tc)*/
/*
 *	does type-resolve on an IO node to transform timer inputs into appropriate actions
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_timerio_typeresolve (compops_t *cops, tnode_t **nodep, typecheck_t *tc)
{
	if ((*nodep)->tag == gup.tag_INPUT) {
		tnode_t *lhstype;

		lhstype = typecheck_gettype (tnode_nthsubof (*nodep, 0), NULL);
		if (lhstype && (lhstype->tag == gup.tag_TIMER)) {
			/* this one! */
			tnode_t *rhs = tnode_nthsubof (*nodep, 1);

			if (rhs->tag == gup.tag_AFTER) {
				/* assuming timeout after expression */
				tnode_t *expr = tnode_nthsubof (rhs, 0);
				tnode_t *type = tnode_nthsubof (rhs, 1);
				tnode_t *newnode = tnode_create (gup.tag_TIMERWAIT, OrgOf (*nodep), expr, type);

				typeresolve_subtree (&newnode, tc);
				*nodep = newnode;
			} else {
				/* assuming read timer */
				tnode_t *type = tnode_nthsubof (*nodep, 2);		/* from the input node */
				tnode_t *newnode = tnode_create (gup.tag_TIMERREAD, OrgOf (*nodep), rhs, type);

				typeresolve_subtree (&newnode, tc);
				*nodep = newnode;
			}
			return 0;
		}
	}

	if (cops->next && tnode_hascompop_i (cops->next, (int)COPS_TYPERESOLVE)) {
		return tnode_callcompop_i (cops->next, (int)COPS_TYPERESOLVE, 2, nodep, tc);
	}
	return 1;
}
/*}}}*/
/*{{{  static int guppy_timerfvnode_postcheck (compops_t *cops, tnode_t **nodep, postcheck_t *pc)*/
/*
 *	does post-check on free-variable node to remove TIMERs -- should not occur elsewhere.
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_timerfvnode_postcheck (compops_t *cops, tnode_t **nodep, postcheck_t *pc)
{
	if ((*nodep)->tag == gup.tag_FVNODE) {
		tnode_t *fvlist = tnode_nthsubof (*nodep, 1);
		int i;

		if (!parser_islistnode (fvlist)) {
			nocc_internal ("guppy_timerfvnode_postcheck(): FVNODE set not list, got [%s]", fvlist->tag->name);
			return 0;
		}
		for (i=0; i<parser_countlist (fvlist); i++) {
			tnode_t *fvitem = parser_getfromlist (fvlist, i);
			tnode_t *fvtype = typecheck_gettype (fvitem, NULL);

			if (fvtype->tag == gup.tag_TIMER) {
				/* this one! */
				parser_delfromlist (fvlist, i);
				i--;
				continue;				/* for() */
			}
		}
	}
	if (cops->next && tnode_hascompop_i (cops->next, (int)COPS_POSTCHECK)) {
		return tnode_callcompop_i (cops->next, (int)COPS_POSTCHECK, 2, nodep, pc);
	}
	return 1;
}
/*}}}*/

/*{{{  static int guppy_timer_init_nodes (void)*/
/*
 *	sets up node types for timer handling
 *	returns 0 on success, non-zero on failure
 */
static int guppy_timer_init_nodes (void)
{
	tndef_t *tnd;
	compops_t *cops;
	langops_t *lops;
	int i;

	/*{{{  guppy:timertype -- TIMER*/
	i = -1;
	tnd = tnode_newnodetype ("guppy:timertype", &i, 0, 0, 0, TNF_NONE);
	cops = tnode_newcompops ();
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "gettype", 2, LANGOPTYPE (guppy_gettype_timertype));
	tnode_setlangop (lops, "getsubtype", 2, LANGOPTYPE (guppy_getsubtype_timertype));
	tnode_setlangop (lops, "typeactual", 4, LANGOPTYPE (guppy_typeactual_timertype));
	tnd->lops = lops;

	i = -1;
	gup.tag_TIMER = tnode_newnodetag ("TIMER", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  guppy:timerop -- AFTER*/
	i = -1;
	tnd = tnode_newnodetype ("guppy:timerop", &i, 2, 0, 0, TNF_NONE);		/* subnodes: expr, type */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (guppy_typecheck_timerop));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "gettype", 2, LANGOPTYPE (guppy_gettype_timerop));
	tnd->lops = lops;

	i = -1;
	gup.tag_AFTER = tnode_newnodetag ("AFTER", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  guppy:timeraction -- TIMERREAD, TIMERWAIT*/
	i = -1;
	tnd = tnode_newnodetype ("guppy:timeraction", &i, 2, 0, 0, TNF_NONE);		/* subnodes: var/expr, type */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "fetrans15", 2, COMPOPTYPE (guppy_fetrans15_timeraction));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (guppy_namemap_timeraction));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnd->lops = lops;

	i = -1;
	gup.tag_TIMERREAD = tnode_newnodetag ("TIMERREAD", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_TIMERWAIT = tnode_newnodetag ("TIMERWAIT", &i, tnd, NTF_NONE);

	/*}}}*/

	/*{{{  interfere with guppy:declblock, guppy:io and guppy:fvnode node types*/
	/* -> postcheck in DECLBLOCK/VARDECL to remove TIMER declarations,
	 * -> typeresolve in INPUT to transform into TIMERREAD,TIMERWAIT
	 * -> postcheck in FVNODE to remove TIMERs
	 */
	tnd = tnode_lookupnodetype ("guppy:declblock");
	if (!tnd) {
		nocc_error ("guppy_timer_init_nodes(): failed to find \"guppy:declblock\" node type");
		return -1;
	}

	cops = tnode_insertcompops (tnd->ops);
	tnode_setcompop (cops, "postcheck", 2, COMPOPTYPE (guppy_timerdeclblock_postcheck));
	tnd->ops = cops;

	tnd = tnode_lookupnodetype ("guppy:io");
	if (!tnd) {
		nocc_error ("guppy_timer_init_nodes(): failed to find \"guppy:io\" node type");
		return -1;
	}

	cops = tnode_insertcompops (tnd->ops);
	tnode_setcompop (cops, "typeresolve", 2, COMPOPTYPE (guppy_timerio_typeresolve));
	tnd->ops = cops;

	tnd = tnode_lookupnodetype ("guppy:fvnode");
	if (!tnd) {
		nocc_error ("guppy_timer_init_nodes(): failed to find \"guppy:fvnode\" node type");
		return -1;
	}

	cops = tnode_insertcompops (tnd->ops);
	tnode_setcompop (cops, "postcheck", 2, COMPOPTYPE (guppy_timerfvnode_postcheck));
	tnd->ops = cops;

	/*}}}*/

	return 0;
}
/*}}}*/
/*{{{  static int guppy_timer_post_setup (void)*/
/*
 *	does post-setup for timer handling
 *	returns 0 on success, non-zero on failure
 */
static int guppy_timer_post_setup (void)
{
	guppy_timer_inttypenode = guppy_newprimtype (gup.tag_INT, NULL, 0);

	return 0;
}
/*}}}*/

/*{{{  guppy_timer_feunit (feunit_t)*/
feunit_t guppy_timer_feunit = {
	.init_nodes = guppy_timer_init_nodes,
	.reg_reducers = NULL,
	.init_dfatrans = NULL,
	.post_setup = guppy_timer_post_setup,
	.ident = "guppy-timer"
};
/*}}}*/

