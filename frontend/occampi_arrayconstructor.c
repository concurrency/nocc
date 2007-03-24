/*
 *	occampi_arrayconstructor.c -- array constructors (including constant and variable constructors)
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
#include "fcnlib.h"
#include "dfa.h"
#include "dfaerror.h"
#include "parsepriv.h"
#include "occampi.h"
#include "feunit.h"
#include "names.h"
#include "scope.h"
#include "prescope.h"
#include "typecheck.h"
#include "constprop.h"
#include "precheck.h"
#include "usagecheck.h"
#include "map.h"
#include "target.h"
#include "transputer.h"
#include "codegen.h"
#include "langops.h"


/*}}}*/


/*{{{  static int occampi_typecheck_ac (compops_t *cops, tnode_t *node, typecheck_t *tc)*/
/*
 *	does type-checking on an array-constructor node -- used to decide whether this should be
 *	a variable array constructor or a constant array constructor
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_typecheck_ac (compops_t *cops, tnode_t *node, typecheck_t *tc)
{
	tnode_t *list = tnode_nthsubof (node, 0);

	if (list) {
		tnode_t **items;
		int nitems, i;
		tnode_t *dtype = NULL;

		/* type-check the various items */
		items = parser_getlistitems (list, &nitems);
		if (nitems > 0) {
			int isvar = 1;		/* assume it is */

			for (i=0; i<nitems; i++) {
				if (items[i]) {
					int iisvar;
					tnode_t *thistype;

					typecheck_subtree (items[i], tc);
					iisvar = langops_isvar (items[i]);
					if (!iisvar) {
						isvar = 0;
					}

					thistype = typecheck_gettype (items[i], NULL);
					if (!dtype && thistype) {
						/* choose this as the default type */
						dtype = thistype;
					} else if (dtype && thistype) {
						if (!typecheck_typeactual (dtype, thistype, node, tc)) {
							typecheck_error (node, tc, "array elements are of different types");
						}
					}
				}
			}

			if (!isvar) {
				node->tag = opi.tag_CONSTCONSTRUCTOR;
			}
		} else {
			/* no list (empty) */
			node->tag = opi.tag_CONSTCONSTRUCTOR;
		}
	} else {
		/* no list (empty) */
		node->tag = opi.tag_CONSTCONSTRUCTOR;
	}
	return 0;
}
/*}}}*/
/*{{{  static int occampi_constprop_ac (compopts_t *, tnode_t **node)*/
/*
 *	does constant propagation on an array-constructor node
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_constprop_ac (compopts_t *cops, tnode_t **node)
{
	tnode_t *list = tnode_nthsubof (*node, 0);
	int allconst = 1;

	if (list) {
		int i, nitems;
		tnode_t **items;

		items = parser_getlistitems (list, &nitems);
		for (i=0; i<nitems; i++) {
			constprop_tree (&(items[i]));

			if (!constprop_isconst (items[i])) {
				allconst = 0;
			}
		}
	}

	if (allconst) {
		(*node)->tag = opi.tag_ALLCONSTCONSTRUCTOR;
	}

	return 0;
}
/*}}}*/
/*{{{  static tnode_t *occampi_gettype_ac (langops_t *lops, tnode_t *node, tnode_t *defaulttype)*/
/*
 *	gets the type of an array-constructor node
 */
static tnode_t *occampi_gettype_ac (langops_t *lops, tnode_t *node, tnode_t *defaulttype)
{
	tnode_t **typep = tnode_nthsubaddr (node, 1);
	tnode_t *list = tnode_nthsubof (node, 0);
	tnode_t *dsubtype = NULL;

	if (*typep) {
		return *typep;
	}

	/* get the subtype of the default type if set */
	if (defaulttype) {
		dsubtype = typecheck_getsubtype (defaulttype, NULL);
	}

	if (!list) {
		/* this means it's an empty array expression */
		*typep = tnode_createfrom (opi.tag_ARRAY, node, constprop_newconst (CONST_INT, NULL, NULL, 0), dsubtype);
	} else {
		tnode_t **items;
		int nitems, i;
		tnode_t *asubtype = NULL;

		items = parser_getlistitems (list, &nitems);
		for (i=0; i<nitems; i++) {
			tnode_t *thistype = typecheck_gettype (items[i], dsubtype);

			if (!asubtype && thistype) {
				/* pick the first actual type as the default */
				asubtype = thistype;
			}
		}

		*typep = tnode_createfrom (opi.tag_ARRAY, node, constprop_newconst (CONST_INT, NULL, NULL, nitems), asubtype);
	}
	return *typep;
}
/*}}}*/
/*{{{  static int occampi_isvar_ac (langops_t *lops, tnode_t *node)*/
/*
 *	determines whether or not an array-constructor node is a variable
 *	returns 0 if value, non-zero if variable
 */
static int occampi_isvar_ac (langops_t *lops, tnode_t *node)
{
	if (node->tag == opi.tag_ARRAYCONSTRUCTOR) {
		return 1;
	}
	return 0;
}
/*}}}*/


/*{{{  static int occampi_scopein_varac (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	scopes in a variable const-constructor node
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_scopein_varac (compops_t *cops, tnode_t **node, scope_t *ss)
{
	tnode_t *replname = tnode_nthsubof (*node, 0);
	tnode_t *type = tnode_create (opi.tag_INT, NULL);
	char *rawname;
	tnode_t *newname;
	name_t *sname;

	if (replname->tag != opi.tag_NAME) {
		scope_error (replname, ss, "occampi_scopein_varac(): replicator name not raw-name!");
		return 0;
	}
	rawname = (char *)tnode_nthhookof (replname, 0);

	/* scope the start and length expressions */
	if (scope_subtree (tnode_nthsubaddr (*node, 1), ss)) {
		/* failed */
		return 0;
	}
	if (scope_subtree (tnode_nthsubaddr (*node, 2), ss)) {
		/* failed */
		return 0;
	}

	sname = name_addscopename (rawname, *node, type, NULL);
	newname = tnode_createfrom (opi.tag_NREPL, replname, sname);
	SetNameNode (sname, newname);
	tnode_setnthsub (*node, 0, newname);

	/* free old name */
	tnode_free (replname);
	ss->scoped++;

	/* scope expression */
	if (!scope_subtree (tnode_nthsubaddr (*node, 3), ss)) {
		/* failed */
		return 0;
	}

	return 0;
}
/*}}}*/
/*{{{  static int occampi_scopeout_varac (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	scopes out a variable const-constructor node
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_scopeout_varac (compops_t *cops, tnode_t **node, scope_t *ss)
{
	tnode_t *replname = tnode_nthsubof (*node, 0);
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
/*{{{  static int occampi_typecheck_varac (compops_t *cops, tnode_t *node, typecheck_t *tc)*/
/*
 *	does type-checking on a variable const-constructor node
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_typecheck_varac (compops_t *cops, tnode_t *node, typecheck_t *tc)
{
	return 1;
}
/*}}}*/
/*{{{  static int occampi_constprop_varac (compops_t *cops, tnode_t **node)*/
/*
 *	does constant-propagation on a variable const-constructor node
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_constprop_varac (compops_t *cops, tnode_t **node)
{
	return 1;
}
/*}}}*/
/*{{{  static tnode_t *occampi_gettype_varac (langops_t *lops, tnode_t *node, tnode_t *defaulttype)*/
/*
 *	gets the type of a variable array-constructor node
 */
static tnode_t *occampi_gettype_varac (langops_t *lops, tnode_t *node, tnode_t *defaulttype)
{
	tnode_t **typep = tnode_nthsubaddr (node, 4);

	if (*typep) {
		return *typep;
	}
	/* FIXME! */

	return defaulttype;
}
/*}}}*/


/*{{{  static int occampi_ac_init_nodes (void)*/
/*
 *	sets up array-constructor nodes for occampi
 *	returns 0 on success, non-zero on error
 */
static int occampi_ac_init_nodes (void)
{
	tndef_t *tnd;
	compops_t *cops;
	langops_t *lops;
	int i;

	/*{{{  occampi:ac -- ARRAYCONSTRUCTOR, CONSTCONSTRUCTOR*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:ac", &i, 2, 0, 0, TNF_NONE);				/* subnodes: 0 = items, 1 = type */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (occampi_typecheck_ac));
	tnode_setcompop (cops, "constprop", 1, COMPOPTYPE (occampi_constprop_ac));
//	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (occampi_namemap_arraynode));
//	tnode_setcompop (cops, "precode", 2, COMPOPTYPE (occampi_precode_arraynode));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "gettype", 2, LANGOPTYPE (occampi_gettype_ac));
	tnode_setlangop (lops, "isvar", 1, LANGOPTYPE (occampi_isvar_ac));
//	tnode_setlangop (lops, "getdescriptor", 2, LANGOPTYPE (occampi_getdescriptor_arraynode));
//	tnode_setlangop (lops, "typeactual", 4, LANGOPTYPE (occampi_typeactual_arraynode));
//	tnode_setlangop (lops, "bytesfor", 2, LANGOPTYPE (occampi_bytesfor_arraynode));
	tnd->lops = lops;

	i = -1;
	opi.tag_CONSTCONSTRUCTOR = tnode_newnodetag ("CONSTCONSTRUCTOR", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_ARRAYCONSTRUCTOR = tnode_newnodetag ("ARRAYCONSTRUCTOR", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_ALLCONSTCONSTRUCTOR = tnode_newnodetag ("ALLCONSTCONSTRUCTOR", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  occampi:varac -- VARCONSTCONSTRUCTOR*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:varac", &i, 5, 0, 0, TNF_NONE);			/* subnodes: 0 = var-name, 1 = start, 2 = length, 3 = expr, 4 = type */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (occampi_scopein_varac));
	tnode_setcompop (cops, "scopeout", 2, COMPOPTYPE (occampi_scopeout_varac));
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (occampi_typecheck_varac));
	tnode_setcompop (cops, "constprop", 1, COMPOPTYPE (occampi_constprop_varac));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "gettype", 2, LANGOPTYPE (occampi_gettype_varac));
	tnd->lops = lops;

	i = -1;
	opi.tag_VARCONSTCONSTRUCTOR = tnode_newnodetag ("VARCONSTCONSTRUCTOR", &i, tnd, NTF_NONE);

	/*}}}*/

	return 0;
}
/*}}}*/


/*{{{  occampi_arrayconstructor_feunit (feunit_t)*/
feunit_t occampi_arrayconstructor_feunit = {
	init_nodes: occampi_ac_init_nodes,
	reg_reducers: NULL,
	init_dfatrans: NULL,
	post_setup: NULL,
	ident: "occampi-arrayconstructor"
};

/*}}}*/


