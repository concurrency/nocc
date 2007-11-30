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

/*{{{  static int occampi_getname_exceptiontypenamenode (langops_t *lops, tnode_t *node, char **str)*/
/*
 *	gets the name of an exception type
 *	returns 0 on success, <0 on error
 */
static int occampi_getname_exceptiontypenamenode (langops_t *lops, tnode_t *node, char **str)
{
	name_t *name = tnode_nthnameof (node, 0);
	char *pname;

	if (!name) {
		nocc_fatal ("occampi_getname_exceptiontypenamenode(): NULL name!");
		return -1;
	}
	pname = NameNameOf (name);
	*str = string_dup (pname);

	return 0;
}
/*}}}*/
/*{{{  static tnode_t *occampi_gettype_exceptiontypenamenode (langops_t *lops, tnode_t *node, tnode_t *default_type)*/
/*
 *	returns the type of an exception type declaration node (trivial)
 */
static tnode_t *occampi_gettype_exceptiontypenamenode (langops_t *lops, tnode_t *node, tnode_t *default_type)
{
	name_t *name = tnode_nthnameof (node, 0);

	if (!name) {
		nocc_fatal ("occampi_gettype_exceptiontypenamenode(): NULL name!");
		return NULL;
	}
	return name->type;
}
/*}}}*/

/*{{{  static int occampi_prescope_trynode (compops_t *cops, tnode_t **nodep, prescope_t *ps)*/
/*
 *	does pre-scoping on a TRY node (checks/arranges CATCH and FINALLY blocks)
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_prescope_trynode (compops_t *cops, tnode_t **nodep, prescope_t *ps)
{
	tnode_t *catches = treeops_findprocess (tnode_nthsubof (*nodep, 2));
	tnode_t *finally = treeops_findprocess (tnode_nthsubof (*nodep, 3));

	if (catches && !finally && (catches->tag == opi.tag_FINALLY)) {
		/* catches might be a finally  -- swap these */
		tnode_setnthsub (*nodep, 3, tnode_nthsubof (*nodep, 2));
		tnode_setnthsub (*nodep, 2, NULL);

		catches = treeops_findprocess (tnode_nthsubof (*nodep, 2));
		finally = treeops_findprocess (tnode_nthsubof (*nodep, 3));
	}

	if (catches) {
		int nitems, i;
		tnode_t **items;

		if (catches->tag != opi.tag_CATCH) {
			prescope_error (catches, ps, "expected CATCH block");
			return 0;
		}

		if (!parser_islistnode (tnode_nthsubof (catches, 0))) {
			/* turn singleton into a list */
			tnode_t *clist = parser_buildlistnode (NULL, tnode_nthsubof (catches, 0), NULL);

			tnode_setnthsub (catches, 0, clist);
		}
		items = parser_getlistitems (tnode_nthsubof (catches, 0), &nitems);

		for (i=0; i<nitems; i++) {
			tnode_t *catch = treeops_findprocess (items[i]);

			if (catch->tag != opi.tag_CATCHEXPR) {
				prescope_error (catch, ps, "CATCH clause is not an exception");
				return 0;
			}
		}
	}

	if (finally) {
		if (finally->tag != opi.tag_FINALLY) {
			prescope_error (finally, ps, "expected FINALLY block");
			return 0;
		}
	}
	return 1;
}
/*}}}*/

/*{{{  static int occampi_prescope_catchexprnode (compops_t *cops, tnode_t **nodep, prescope_t *ps)*/
/*
 *	pre-scopes an exception CATCH expression (make sure expressions are NULL or list)
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_prescope_catchexprnode (compops_t *cops, tnode_t **nodep, prescope_t *ps)
{
	tnode_t **eptr = tnode_nthsubaddr (*nodep, 2);

	if (*eptr && !parser_islistnode (*eptr)) {
		*eptr = parser_buildlistnode (NULL, *eptr, NULL);
	}
	return 1;
}
/*}}}*/
/*{{{  static int occampi_typecheck_catchexprnode (compops_t *cops, tnode_t *node, typecheck_t *tc)*/
/*
 *	does type-checking on an exception CATCH expression (expecting exception name, process and expression-list)
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_typecheck_catchexprnode (compops_t *cops, tnode_t *node, typecheck_t *tc)
{
	tnode_t *name = tnode_nthsubof (node, 0);
	char *cname = NULL;

	/* subtypecheck name and expressions */
	typecheck_subtree (tnode_nthsubof (node, 0), tc);
	typecheck_subtree (tnode_nthsubof (node, 2), tc);

	langops_getname (name, &cname);

	if (name->tag != opi.tag_NEXCEPTIONTYPEDECL) {
		typecheck_error (node, tc, "CATCH exception [%s] is not an EXCEPTION type", cname ?: "(unknown)");
	} else {
		tnode_t *ftype = typecheck_gettype (name, NULL);
		tnode_t *exprlist = tnode_nthsubof (node, 2);

		if (!exprlist && ftype) {
			int nfitems;

			parser_getlistitems (ftype, &nfitems);
			typecheck_error (node, tc, "CATCH exception [%s] expected %d items, but found 0", cname ?: "(unknown)", nfitems);
		} else if (exprlist && !ftype) {
			int naitems;

			parser_getlistitems (exprlist, &naitems);
			typecheck_error (node, tc, "CATCH exception [%s] expected 0 items, but found %d", cname ?: "(unknown)", naitems);
		} else if (!exprlist && !ftype) {
			/* good */
		} else {
			int naitems, nfitems, i;
			tnode_t **alist = parser_getlistitems (exprlist, &naitems);
			tnode_t **flist = parser_getlistitems (ftype, &nfitems);

			if (naitems != nfitems) {
				typecheck_error (node, tc, "CATCH exception [%s] expected %d items, but found %d", cname ?: "(unknown)", nfitems, naitems);
			} else {
				for (i=0; i<nfitems; i++) {
					/* check that the actual is a variable and a good type for the formal */
					if (!langops_isvar (alist[i])) {
						typecheck_error (node, tc, "CATCH exception [%s] expression %d is not a variable", cname ?: "(unknown)", i+1);
					} else {
						tnode_t *atype = typecheck_gettype (alist[i], flist[i]);

#if 0
fprintf (stderr, "occampi_typecheck_catchexprnode(): actual item is:\n");
tnode_dumptree (alist[i], 1, stderr);
fprintf (stderr, "occampi_typecheck_catchexprnode(): actual type is:\n");
tnode_dumptree (atype, 1, stderr);
#endif
						if (!atype) {
							typecheck_error (node, tc, "failed to get type of CATCH exception [%s] expression %d", cname ?: "(unknown)", i+1);
						} else if (!typecheck_fixedtypeactual (flist[i], atype, node, tc, 1)) {
							typecheck_error (node, tc, "incompatible types for CATCH exception [%s] expression %d", cname ?: "(unknown)", i+1);
						}
					}
				}
			}
		}
	}

	if (cname) {
		sfree (cname);
	}

	/* type-check the body */
	typecheck_subtree (tnode_nthsubof (node, 1), tc);

	return 0;
}
/*}}}*/

/*{{{  static int occampi_prescope_exceptionactionnode (compops_t *cops, tnode_t **nodep, prescope_t *ps)*/
/*
 *	pre-scopes an exception action node (THROW)
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_prescope_exceptionactionnode (compops_t *cops, tnode_t **nodep, prescope_t *ps)
{
	tnode_t **eptr = tnode_nthsubaddr (*nodep, 1);

	if (*eptr && !parser_islistnode (*eptr)) {
		*eptr = parser_buildlistnode (NULL, *eptr, NULL);
	}
	return 1;
}
/*}}}*/
/*{{{  static int occampi_typecheck_exceptionactionnode (compops_t *cops, tnode_t *node, typecheck_t *tc)*/
/*
 *	does type-checking on an exception action node (THROW)
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_typecheck_exceptionactionnode (compops_t *cops, tnode_t *node, typecheck_t *tc)
{
	/* FIXME! */
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
	tnode_setlangop (lops, "getname", 2, LANGOPTYPE (occampi_getname_exceptiontypenamenode));
	tnode_setlangop (lops, "gettype", 2, LANGOPTYPE (occampi_gettype_exceptiontypenamenode));
	tnd->lops = lops;

	i = -1;
	opi.tag_NEXCEPTIONTYPEDECL = tnode_newnodetag ("N_EXCEPTIONTYPEDECL", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  occampi:trynode -- TRY*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:trynode", &i, 4, 0, 0, TNF_LONGPROC);			/* subnodes: 0 = (none), 1 = body, 2 = catches, 3 = finally */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "prescope", 2, COMPOPTYPE (occampi_prescope_trynode));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnd->lops = lops;

	i = -1;
	opi.tag_TRY = tnode_newnodetag ("TRY", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  occampi:catchnode -- CATCH*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:catchnode", &i, 2, 0, 0, TNF_LONGPROC);		/* subnodes: 0 = list-of-catchexprs, 1 = body (none) */
	cops = tnode_newcompops ();
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnd->lops = lops;

	i = -1;
	opi.tag_CATCH = tnode_newnodetag ("CATCH", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  occampi:finallynode -- FINALLY*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:finallynode", &i, 2, 0, 0, TNF_LONGPROC);		/* subnodes: 0 = (none), 1 = body */
	cops = tnode_newcompops ();
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnd->lops = lops;

	i = -1;
	opi.tag_FINALLY = tnode_newnodetag ("FINALLY", &i, tnd, NTF_INDENTED_PROC);

	/*}}}*/
	/*{{{  occampi:catchexprnode -- CATCHEXPR*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:catchexprnode", &i, 3, 0, 0, TNF_LONGPROC);		/* subnodes: 0 = exception, 1 = body, 2 = expressions */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "prescope", 2, COMPOPTYPE (occampi_prescope_catchexprnode));
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (occampi_typecheck_catchexprnode));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnd->lops = lops;

	i = -1;
	opi.tag_CATCHEXPR = tnode_newnodetag ("CATCHEXPR", &i, tnd, NTF_INDENTED_PROC);

	/*}}}*/
	/*{{{  occampi:exceptionactionnode -- THROW*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:exceptionactionnode", &i, 2, 0, 0, TNF_NONE);		/* subnodes: 0 = exception, 1 = expressions */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "prescope", 2, COMPOPTYPE (occampi_prescope_exceptionactionnode));
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (occampi_typecheck_exceptionactionnode));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnd->lops = lops;

	i = -1;
	opi.tag_THROW = tnode_newnodetag ("THROW", &i, tnd, NTF_NONE);

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


