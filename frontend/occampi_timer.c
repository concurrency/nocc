/*
 *	occampi_timer.c -- occam-pi timer handling for NOCC
 *	Copyright (C) 2007 Fred Barnes <frmb@kent.ac.uk>
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
#include "fetrans.h"
#include "betrans.h"
#include "map.h"
#include "target.h"
#include "transputer.h"
#include "codegen.h"
#include "langdef.h"


/*}}}*/
/*{{{  private data*/
static chook_t *actionlhstypechook = NULL;
/*}}}*/


/*{{{  static int occampi_typecheck_timertype (compops_t *cops, tnode_t *t, typecheck_t *tc)*/
/*
 *	does type-checking on a TIMER type node (sets sub-type)
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_typecheck_timertype (compops_t *cops, tnode_t *t, typecheck_t *tc)
{
	tnode_t **typep = tnode_nthsubaddr (t, 0);

	if (!*typep) {
		*typep = tnode_createfrom (opi.tag_INT, t);
	}

	return 0;
}
/*}}}*/


/*{{{  static int occampi_timertype_getdescriptor (langops_t *lops, tnode_t *node, char **str)*/
/*
 *	gets descriptor information for a timer-type
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_timertype_getdescriptor (langops_t *lops, tnode_t *node, char **str)
{
	char *sptr;

	if (*str) {
		char *newstr = (char *)smalloc (strlen (*str) + 16);

		sptr = newstr;
		sptr += sprintf (newstr, "%s", *str);
		sfree (*str);
		*str = newstr;
	} else {
		*str = (char *)smalloc (16);
		sptr = *str;
	}

	if (node->tag == opi.tag_TIMER) {
		sprintf (sptr, "TIMER");
	}

	return 0;
}
/*}}}*/
/*{{{  static tnode_t *occampi_timertype_gettype (langops_t *lops, tnode_t *t, tnode_t *defaulttype)*/
/*
 *	gets the type for a timertype -- do nothing really
 */
static tnode_t *occampi_timertype_gettype (langops_t *lops, tnode_t *t, tnode_t *defaulttype)
{
	return t;
}
/*}}}*/
/*{{{  static tnode_t *occampi_timertype_getsubtype (langops_t *lops, tnode_t *t, tnode_t *defaulttype)*/
/*
 *	gets the sub-type for a timertype
 */
static tnode_t *occampi_timertype_getsubtype (langops_t *lops, tnode_t *t, tnode_t *defaulttype)
{
	tnode_t **typep = tnode_nthsubaddr (t, 0);

	return *typep;
}
/*}}}*/
/*{{{  static int occampi_timertype_cantypecast (langops_t *lops, tnode_t *node, tnode_t *srctype)*/
/*
 *	checks to see if one type can be cast into another
 *	returns non-zero if valid cast, zero otherwise
 */
static int occampi_timertype_cantypecast (langops_t *lops, tnode_t *node, tnode_t *srctype)
{
	/* cannot cast timer types */
	return 0;
}
/*}}}*/
/*{{{  static int occampi_timertype_bytesfor (langops_t *lops, tnode_t *t, target_t *target)*/
/*
 *	returns the number of bytes required by a timer type
 */
static int occampi_timertype_bytesfor (langops_t *lops, tnode_t *t, target_t *target)
{
	/* timers don't occupy any space */
	return 0;
}
/*}}}*/
/*{{{  static int occampi_timertype_issigned (langops_t *lops, tnode_t *t, target_t *target)*/
/*
 *	returns 0 if the given basic type is unsigned
 */
static int occampi_timertype_issigned (langops_t *lops, tnode_t *t, target_t *target)
{
	/* yes, timers are signed */
	return 1;
}
/*}}}*/
/*{{{  static int occampi_timertype_istype (langops_t *lops, tnode_t *t)*/
/*
 *	returns non-zero if this a type node (always)
 */
static int occampi_timertype_istype (langops_t *lops, tnode_t *t)
{
	return 1;
}
/*}}}*/
/*{{{  static typecat_e occampi_timertype_typetype (langops_t *lops, tnode_t *t)*/
/*
 *	returns the type category for a timer-type
 */
static typecat_e occampi_timertype_typetype (langops_t *lops, tnode_t *t)
{
	if (t->tag == opi.tag_TIMER) {
		return TYPE_INTEGER;
	}
	return TYPE_NOTTYPE;
}
/*}}}*/
/*{{{  static tnode_t *occampi_timertype_typeactual (langops_t *lops, tnode_t *formaltype, tnode_t *actualtype, tnode_t *node, typecheck_t *tc)*/
/*
 *	does type-compatibility checking on a TIMER type node (for I/O usually), returns the actual type used
 */
static tnode_t *occampi_timertype_typeactual (langops_t *lops, tnode_t *formaltype, tnode_t *actualtype, tnode_t *node, typecheck_t *tc)
{
	tnode_t *atype = actualtype;

	if (formaltype->tag == opi.tag_TIMER) {
		if (node->tag == opi.tag_INPUT) {
			atype = tnode_nthsubof (formaltype, 0);
			atype = typecheck_typeactual (atype, actualtype, node, tc);
		} else if (node->tag == opi.tag_OUTPUT) {
			typecheck_error (node, tc, "cannot set TIMER with output");
			atype = NULL;
		} else {
			/* must be two TIMERs then */

			if (actualtype->tag != opi.tag_TIMER) {
				typecheck_error (node, tc, "expected TIMER, found [%s]", actualtype->tag->name);
			}
			atype = actualtype;

			if (!typecheck_typeactual (tnode_nthsubof (formaltype, 0), tnode_nthsubof (actualtype, 0), node, tc)) {
				return NULL;
			}
		}
	} else {
		nocc_fatal ("occampi_timertype_typeactual(): don\'t know how to handle a non-TIMER here, got [%s]", formaltype->tag->name);
		atype = NULL;
	}
	return atype;
}
/*}}}*/
/*{{{  static int occampi_timertype_codegen_typeaction (langops_t *lops, tnode_t *type, tnode_t *anode, codegen_t *cgen)*/
/*
 *	handles code-generation actions for TIMERs -- not relevant, transformed into "occampi:timerinputnode"s
 *	returns 0 to stop the code-gen walk, 1 to continue, -1 to resort to normal action handling
 */
static int occampi_timertype_codegen_typeaction (langops_t *lops, tnode_t *type, tnode_t *anode, codegen_t *cgen)
{
	codegen_error (cgen, "occampi_timertype_codegen_typeaction(): cannot generate I/O for TIMER!");
	return 0;
}
/*}}}*/


/*{{{  static int occampi_timer_vardecl_betrans (compops_t *cops, tnode_t **tptr, betrans_t *be)*/
/*
 *	betrans on VARDECL nodes -- used to remove TIMER declarations
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_timer_vardecl_betrans (compops_t *cops, tnode_t **tptr, betrans_t *be)
{
	tnode_t *t = *tptr;
	tnode_t *name = tnode_nthsubof (t, 0);
	tnode_t *type = tnode_nthsubof (t, 1);
	tnode_t **bodyp = tnode_nthsubaddr (t, 2);

	if (type->tag == opi.tag_TIMER) {
		/* yes, remove this */
		*tptr = *bodyp;

		/* do betrans on new body */
		betrans_subtree (tptr, be);
		return 0;
	}

	if (tnode_hascompop (cops->next, "betrans")) {
		return tnode_callcompop (cops->next, "betrans", 2, tptr, be);
	}
	return 1;
}
/*}}}*/


/*{{{  static int occampi_timer_actionnode_fetrans (compops_t *cops, tnode_t **tptr, fetrans_t *fe)*/
/*
 *	fetrans on action-nodes -- used to transform TIMER inputs
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_timer_actionnode_fetrans (compops_t *cops, tnode_t **tptr, fetrans_t *fe)
{
	tnode_t *t = *tptr;
	tnode_t *lhstype = (tnode_t *)tnode_getchook (t, actionlhstypechook);

	if (lhstype->tag == opi.tag_TIMER) {
		tnode_t *rhs = tnode_nthsubof (t, 1);
		ntdef_t *newtag = NULL;

		/* this is a timer action */
#if 0
fprintf (stderr, "occampi_timer_actionnode_fetrans(): found TIMER action!\n");
#endif
		if (rhs->tag == opi.tag_MAFTER) {
			/* after expression */
			rhs = tnode_nthsubof (rhs, 0);
			newtag = opi.tag_TIMERINPUTAFTER;
		} else {
			newtag = opi.tag_TIMERINPUT;
		}

		if (t->tag != opi.tag_INPUT) {
			nocc_warning ("occampi_timer_actionnode_fetrans(): action is not input!, got [%s]", t->tag->name);
		} else {
			tnode_t *lhs = tnode_nthsubof (t, 0);

			tnode_setnthsub (t, 0, NULL);
			tnode_setnthsub (t, 1, NULL);
			*tptr = tnode_createfrom (newtag, t, lhs, rhs);

			/* do fetrans on new node */
			fetrans_subtree (tptr, fe);
		}
		return 0;
	}

	if (tnode_hascompop (cops->next, "fetrans")) {
		return tnode_callcompop (cops->next, "fetrans", 2, tptr, fe);
	}
	return 1;
}
/*}}}*/


/*{{{  static int occampi_timer_init_nodes (void)*/
/*
 *	initialises nodes for occam-pi timers
 *	returns 0 on success, non-zero on failure
 */
static int occampi_timer_init_nodes (void)
{
	tndef_t *tnd;
	compops_t *cops;
	langops_t *lops;
	int i;

	/*{{{  occampi:timerinputnode -- TIMERINPUT, TIMERINPUTAFTER*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:timerinputnode", &i, 2, 0, 0, TNF_NONE);		/* subnodes: 0 = timer var, 1 = input-var or timeout expr */
	cops = tnode_newcompops ();
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnd->lops = lops;

	i = -1;
	opi.tag_TIMERINPUT = tnode_newnodetag ("TIMERINPUT", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_TIMERINPUTAFTER = tnode_newnodetag ("TIMERINPUTAFTER", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  occampi:timertype -- TIMER*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:timertype", &i, 1, 0, 0, TNF_NONE);			/* subnodes: 1 = subtype */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (occampi_typecheck_timertype));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "getdescriptor", 2, LANGOPTYPE (occampi_timertype_getdescriptor));
	tnode_setlangop (lops, "gettype", 2, LANGOPTYPE (occampi_timertype_gettype));
	tnode_setlangop (lops, "getsubtype", 2, LANGOPTYPE (occampi_timertype_getsubtype));
	tnode_setlangop (lops, "cantypecast", 2, LANGOPTYPE (occampi_timertype_cantypecast));
	tnode_setlangop (lops, "bytesfor", 2, LANGOPTYPE (occampi_timertype_bytesfor));
	tnode_setlangop (lops, "issigned", 2, LANGOPTYPE (occampi_timertype_issigned));
	tnode_setlangop (lops, "istype", 1, LANGOPTYPE (occampi_timertype_istype));
	tnode_setlangop (lops, "typetype", 1, LANGOPTYPE (occampi_timertype_typetype));
	tnode_setlangop (lops, "typeactual", 4, LANGOPTYPE (occampi_timertype_typeactual));
	tnode_setlangop (lops, "codegen_typeaction", 3, LANGOPTYPE (occampi_timertype_codegen_typeaction));
	tnd->lops = lops;

	i = -1;
	opi.tag_TIMER = tnode_newnodetag ("TIMER", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  occampi:timeroper -- MAFTER*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:timeroper", &i, 1, 0, 0, TNF_NONE);			/* subnodes: 1 = operand */
	cops = tnode_newcompops ();
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnd->lops = lops;

	i = -1;
	opi.tag_MAFTER = tnode_newnodetag ("MAFTER", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  occampi:vardecl -- overrides for VARDECL*/
	tnd = tnode_lookupnodetype ("occampi:vardecl");
	cops = tnode_insertcompops (tnd->ops);
	tnode_setcompop (cops, "betrans", 2, COMPOPTYPE (occampi_timer_vardecl_betrans));
	tnd->ops = cops;

	/*}}}*/
	/*{{{  occampi:actionnode -- overrides for INPUT (TIMERs)*/
	tnd = tnode_lookupnodetype ("occampi:actionnode");
	cops = tnode_insertcompops (tnd->ops);
	tnode_setcompop (cops, "fetrans", 2, COMPOPTYPE (occampi_timer_actionnode_fetrans));
	tnd->ops = cops;

	/*}}}*/

	return 0;
}
/*}}}*/
/*{{{  static int occampi_timer_post_setup (void)*/
/*
 *	does post-setup for timer handling
 *	returns 0 on success, non-zero on failure
 */
static int occampi_timer_post_setup (void)
{
	actionlhstypechook = tnode_lookupchookbyname ("occampi:action:lhstype");
	if (!actionlhstypechook) {
		nocc_error ("occampi_timer_post_setup(): failed to find \"occampi:action:lhstype\" compiler-hook!");
		return 1;
	}
	return 0;
}
/*}}}*/


/*{{{  occampi_timer_feunit (feunit_t)*/
feunit_t occampi_timer_feunit = {
	init_nodes: occampi_timer_init_nodes,
	reg_reducers: NULL,
	init_dfatrans: NULL,
	post_setup: occampi_timer_post_setup,
	ident: "occampi-timer"
};

/*}}}*/


