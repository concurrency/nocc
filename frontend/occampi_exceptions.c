/*
 *	occampi_exceptions.c -- EXCEPTION mechanism for occam-pi
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
#include "treeops.h"
#include "dfa.h"
#include "parsepriv.h"
#include "occampi.h"
#include "feunit.h"
#include "names.h"
#include "fcnlib.h"
#include "scope.h"
#include "prescope.h"
#include "library.h"
#include "typecheck.h"
#include "precheck.h"
#include "usagecheck.h"
#include "tracescheck.h"
#include "map.h"
#include "target.h"
#include "transputer.h"
#include "codegen.h"
#include "langops.h"
#include "fetrans.h"


/*}}}*/


/*{{{  static int occampi_scopein_exceptiontypedecl (compops_t *cops, tnode_t **nodep, scope_t *ss)*/
/*
 *	scopes-in an exception type declaration
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_scopein_exceptiontypedecl (compops_t *cops, tnode_t **nodep, scope_t *ss)
{
	tnode_t *name = tnode_nthsubof (*nodep, 0);
	tnode_t **typep = tnode_nthsubaddr (*nodep, 1);
	name_t *sname = NULL;
	tnode_t *newname = NULL;
	char *rawname;

	if (name->tag != opi.tag_NAME) {
		scope_error (name, ss, "name not raw-name!");
		return 0;
	}

	rawname = (char *)tnode_nthhookof (name, 0);

	/* the type is the exception sub-type, may be NULL */
	if (*typep) {
		if (scope_subtree (typep, ss)) {
			/* failed to scope in sub-type */
			return 0;
		}
	}

	sname = name_addscopename (rawname, *nodep, *typep, NULL);
	newname = tnode_createfrom (opi.tag_NEXCEPTIONTYPEDECL, name, sname);
	SetNameNode (sname, newname);
	tnode_setnthsub (*nodep, 0, newname);

	/* free old name */
	tnode_free (name);
	ss->scoped++;

	/* scope body */
	if (scope_subtree (tnode_nthsubaddr (*nodep, 2), ss)) {
		/* failed to scope body */
		name_descopename (sname);
		return 0;
	}

	name_descopename (sname);
	return 0;
}
/*}}}*/
/*{{{  static int occampi_scopeout_exceptiontypedecl (compops_t *cops, tnode_t **nodep, scope_t *ss)*/
/*
 *	scopes-out an exception type declaration
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_scopeout_exceptiontypedecl (compops_t *cops, tnode_t **nodep, scope_t *ss)
{
	return 1;
}
/*}}}*/


/*{{{  static int occampi_exceptions_init_nodes (void)*/
/*
 *	initialises exception handling nodes
 *	returns 0 on success, non-zero on failure
 */
static int occampi_exceptions_init_nodes (void)
{
	tndef_t *tnd;
	int i;
	compops_t *cops;
	langops_t *lops;

	/*{{{  occampi:exceptiontypedecl -- EXCEPTIONTYPEDECL*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:exceptiontypedecl", &i, 3, 0, 0, TNF_SHORTDECL);	/* subnodes: 0 = name, 1 = type, 2 = in-scope-body */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (occampi_scopein_exceptiontypedecl));
	tnode_setcompop (cops, "scopeout", 2, COMPOPTYPE (occampi_scopeout_exceptiontypedecl));
	tnd->ops = cops;

	i = -1;
	opi.tag_EXCEPTIONTYPEDECL = tnode_newnodetag ("EXCEPTIONTYPEDECL", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  occampi:exceptiontypenamenode -- N_EXCEPTIONTYPEDECL*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:exceptiontypenamenode", &i, 0, 1, 0, TNF_NONE);	/* subnames: name */
	cops = tnode_newcompops ();
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnd->lops = lops;

	i = -1;
	opi.tag_NEXCEPTIONTYPEDECL = tnode_newnodetag ("N_EXCEPTIONTYPEDECL", &i, tnd, NTF_NONE);

	/*}}}*/

	return 0;
}
/*}}}*/
/*{{{  static int occampi_exceptions_post_setup (void)*/
/*
 *	does post-setup for exceptions nodes
 *	returns 0 on success, non-zero on failure
 */
static int occampi_exceptions_post_setup (void)
{
	return 0;
}
/*}}}*/


/*{{{  occampi_exceptions_feunit (feunit_t)*/
feunit_t occampi_exceptions_feunit = {
	init_nodes: occampi_exceptions_init_nodes,
	reg_reducers: NULL,
	init_dfatrans: NULL,
	post_setup: occampi_exceptions_post_setup,
	ident: "occampi-exceptions"
};
/*}}}*/


