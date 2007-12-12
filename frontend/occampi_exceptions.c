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
#include "origin.h"
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
#include "metadata.h"
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
/*{{{  private types*/

typedef struct TAG_opiexception {
	DYNARRAY (tnode_t *, elist);		/* list of exceptions collected so far */
	int doprune;				/* non-zero if we are pruning things */
	int err;
	int warn;
} opiexception_t;

typedef struct TAG_opithrowshook {
	DYNARRAY (tnode_t *, elist);		/* list of exceptions */
} opithrowshook_t;

typedef struct TAG_opiimportthrowshook {
	DYNARRAY (char *, names);		/* list of exception names */
	DYNARRAY (char *, thashs);		/* list of exception type-hashes */
	DYNARRAY (tnode_t *, resolved);		/* list of associated resolved names */
} opiimportthrowshook_t;


/*}}}*/
/*{{{  private data*/

static chook_t *exceptioncheck_throwschook = NULL;
static chook_t *exceptioncheck_importthrowschook = NULL;

/*}}}*/


/*{{{  static opiexception_t *opi_newopiexception (void)*/
/*
 *	creates a new, blank, opiexception_t structure
 */
static opiexception_t *opi_newopiexception (void)
{
	opiexception_t *oex = (opiexception_t *)smalloc (sizeof (opiexception_t));

	dynarray_init (oex->elist);
	oex->doprune = 0;
	oex->err = 0;
	oex->warn = 0;

	return oex;
}
/*}}}*/
/*{{{  static void opi_freeopiexception (opiexception_t *oex)*/
/*
 *	frees an opiexception_t structure
 */
static void opi_freeopiexception (opiexception_t *oex)
{
	if (!oex) {
		nocc_serious ("opi_freeopiexception(): NULL exception!");
		return;
	}
	dynarray_trash (oex->elist);
	sfree (oex);
	return;
}
/*}}}*/
/*{{{  static opithrowshook_t *opi_newopithrowshook (void)*/
/*
 *	creates a new, blank, opithrowshook_t structure
 */
static opithrowshook_t *opi_newopithrowshook (void)
{
	opithrowshook_t *opith = (opithrowshook_t *)smalloc (sizeof (opithrowshook_t));

	dynarray_init (opith->elist);
	return opith;
}
/*}}}*/
/*{{{  static void opi_freeopithrowshook (opithrowshook_t *opith)*/
/*
 *	frees an opithrowshook_t structure
 */
static void opi_freeopithrowshook (opithrowshook_t *opith)
{
	if (!opith) {
		nocc_serious ("opi_freeopithrowshook(): NULL hook!");
		return;
	}
	dynarray_trash (opith->elist);
	sfree (opith);
	return;
}
/*}}}*/
/*{{{  static opiimportthrowshook_t *opi_newopiimportthrowshook (void)*/
/*
 *	creates a new, blank, opiimportthrowshook_t structure
 */
static opiimportthrowshook_t *opi_newopiimportthrowshook (void)
{
	opiimportthrowshook_t *opiith = (opiimportthrowshook_t *)smalloc (sizeof (opiimportthrowshook_t));

	dynarray_init (opiith->names);
	dynarray_init (opiith->thashs);
	dynarray_init (opiith->resolved);
	return opiith;
}
/*}}}*/
/*{{{  static void opi_freeopiimportthrowshook (opiimportthrowshook_t *opiith)*/
/*
 *	frees an opiimportthrowshook_t structure
 */
static void opi_freeopiimportthrowshook (opiimportthrowshook_t *opiith)
{
	int i;

	if (!opiith) {
		nocc_serious ("opi_freeopiimportthrowshook(): NULL hook!");
		return;
	}
	for (i=0; i<DA_CUR (opiith->names); i++) {
		char *desc = DA_NTHITEM (opiith->names, i);
		char *thash = DA_NTHITEM (opiith->thashs, i);

		if (desc) {
			sfree (desc);
		}
		if (thash) {
			sfree (thash);
		}
	}
	dynarray_trash (opiith->names);
	dynarray_trash (opiith->thashs);
	dynarray_trash (opiith->resolved);
	sfree (opiith);
	return;
}
/*}}}*/

/*{{{  static void *exceptioncheck_throwschook_copy (void *hook)*/
/*
 *	copies an occampi:throws compiler hook
 */
static void *exceptioncheck_throwschook_copy (void *hook)
{
	opithrowshook_t *opith = (opithrowshook_t *)hook;
	opithrowshook_t *newth;
	int i;

	if (!opith) {
		nocc_serious ("exceptioncheck_throwschook_copy(): NULL hook!");
		return NULL;
	}
	newth = opi_newopithrowshook ();
	for (i=0; i<DA_CUR (opith->elist); i++) {
		dynarray_add (newth->elist, DA_NTHITEM (opith->elist, i));
	}
	return newth;
}
/*}}}*/
/*{{{  static void exceptioncheck_throwschook_free (void *hook)*/
/*
 *	frees an occampi:throws compiler hook
 */
static void exceptioncheck_throwschook_free (void *hook)
{
	opithrowshook_t *opith = (opithrowshook_t *)hook;

	if (!opith) {
		nocc_serious ("exceptioncheck_throwschook_free(): NULL hook!");
		return;
	}
	opi_freeopithrowshook (opith);
	return;
}
/*}}}*/
/*{{{  static void exceptioncheck_throwschook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	dumps an occampi:throws compiler hook (debugging)
 */
static void exceptioncheck_throwschook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	opithrowshook_t *opith = (opithrowshook_t *)hook;

	occampi_isetindent (stream, indent);
	fprintf (stream, "<chook:occampi:throws addr=\"0x%8.8x\">\n", (unsigned int)opith);
	if (opith) {
		int i;

		for (i=0; i<DA_CUR (opith->elist); i++) {
			tnode_t *ename = DA_NTHITEM (opith->elist, i);

			tnode_dumptree (ename, indent + 1, stream);
		}
	}
	occampi_isetindent (stream, indent);
	fprintf (stream, "</chook:occampi:throws>\n");
	return;
}
/*}}}*/

/*{{{  static void *exceptioncheck_importthrowschook_copy (void *hook)*/
/*
 *	copies an occampi:importthrows compiler hook
 */
static void *exceptioncheck_importthrowschook_copy (void *hook)
{
	opiimportthrowshook_t *opiith = (opiimportthrowshook_t *)hook;
	opiimportthrowshook_t *newith;
	int i;

	if (!opiith) {
		nocc_serious ("exceptioncheck_importthrowschook_copy(): NULL hook!");
		return NULL;
	}
	newith = opi_newopiimportthrowshook ();
	for (i=0; i<DA_CUR (opiith->names); i++) {
		char *desc = DA_NTHITEM (opiith->names, i);
		char *thash = DA_NTHITEM (opiith->thashs, i);

		dynarray_add (newith->names, desc ? string_dup (desc) : NULL);
		dynarray_add (newith->thashs, thash ? string_dup (thash) : NULL);
		dynarray_add (newith->resolved, DA_NTHITEM (opiith->resolved, i));
	}
	return newith;
}
/*}}}*/
/*{{{  static void exceptioncheck_importthrowschook_free (void *hook)*/
/*
 *	frees an occampi:importthrows compiler hook
 */
static void exceptioncheck_importthrowschook_free (void *hook)
{
	opiimportthrowshook_t *opiith = (opiimportthrowshook_t *)hook;

	if (!opiith) {
		nocc_serious ("exceptioncheck_importthrowschook_free(): NULL hook!");
		return;
	}
	opi_freeopiimportthrowshook (opiith);
	return;
}
/*}}}*/
/*{{{  static void exceptioncheck_importthrowschook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	dumps an occampi:importthrows compiler hook (debugging)
 */
static void exceptioncheck_importthrowschook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	opiimportthrowshook_t *opiith = (opiimportthrowshook_t *)hook;

	occampi_isetindent (stream, indent);
	fprintf (stream, "<chook:occampi:importthrows addr=\"0x%8.8x\">\n", (unsigned int)opiith);
	if (opiith) {
		int i;

		for (i=0; i<DA_CUR (opiith->names); i++) {
			char *desc = DA_NTHITEM (opiith->names, i);
			char *thash = DA_NTHITEM (opiith->thashs, i);
			tnode_t *res = DA_NTHITEM (opiith->resolved, i);

			occampi_isetindent (stream, indent + 1);
			fprintf (stream, "<throws name=\"%s\" typehash=\"0x%s\" resolved=\"%s\"%s>\n",
					desc ?: "(null)", thash ?: "00000000", res ? "yes" : "no", res ? "" : " /");
			if (res) {
				tnode_dumptree (res, indent + 2, stream);
				occampi_isetindent (stream, indent + 1);
				fprintf (stream, "</throws>\n");
			}
		}
	}
	occampi_isetindent (stream, indent);
	fprintf (stream, "</chook:occampi:importthrows>\n");
	return;
}
/*}}}*/

/*{{{  static int exceptioncheck_modprewalk (tnode_t **tptr, void *arg)*/
/*
 *	called in prewalk order to do exception checking
 *	returns 0 to stop walk, 1 to continue
 */
static int exceptioncheck_modprewalk (tnode_t **tptr, void *arg)
{
	int i = 1;

	if (*tptr && (*tptr)->tag->ndef->ops && tnode_hascompop ((*tptr)->tag->ndef->ops, "exceptioncheck")) {
		i = tnode_callcompop ((*tptr)->tag->ndef->ops, "exceptioncheck", 2, tptr, (opiexception_t *)arg);
	}
	return i;
}
/*}}}*/
/*{{{  static int exceptioncheck_subtree (tnode_t **tptr, opiexception_t *oex)*/
/*
 *	does exception checking on a sub-tree
 *	returns 0 on success, non-zero on failure
 */
static int exceptioncheck_subtree (tnode_t **tptr, opiexception_t *oex)
{
	tnode_modprewalktree (tptr, exceptioncheck_modprewalk, (void *)oex);
	return oex->err;
}
/*}}}*/

/*{{{  static void exceptioncheck_warning (tnode_t *node, opiexception_t *tc, const char *fmt, ...)*/
/*
 *	called by exception-check routines to report a warning
 */
static void exceptioncheck_warning (tnode_t *node, opiexception_t *tc, const char *fmt, ...)
{
	va_list ap;
	int n;
	char *warnbuf = (char *)smalloc (512);
	lexfile_t *orgfile;

	if (!node) {
		orgfile = NULL;
	} else {
		orgfile = node->org_file;
	}

	va_start (ap, fmt);
	n = sprintf (warnbuf, "%s:%d (warning) ", orgfile ? orgfile->fnptr : "(unknown)", node->org_line);
	vsnprintf (warnbuf + n, 512 - n, fmt, ap);
	va_end (ap);

	if (orgfile) {
		orgfile->warncount++;
	}
	tc->warn++;
	nocc_message (warnbuf);
	sfree (warnbuf);

	return;
}
/*}}}*/
/*{{{  static void exceptioncheck_error (tnode_t *node, opiexception_t *tc, const char *fmt, ...)*/
/*
 *	called by exception-checking bits to report an error
 */
static void exceptioncheck_error (tnode_t *node, opiexception_t *tc, const char *fmt, ...)
{
	va_list ap;
	int n;
	char *warnbuf = (char *)smalloc (512);
	lexfile_t *orgfile;

	if (!node) {
		orgfile = NULL;
	} else {
		orgfile = node->org_file;
	}

	va_start (ap, fmt);
	n = sprintf (warnbuf, "%s:%d (error) ", orgfile ? orgfile->fnptr : "(unknown)", node->org_line);
	vsnprintf (warnbuf + n, 512 - n, fmt, ap);
	va_end (ap);

	if (orgfile) {
		orgfile->errcount++;
	}
	tc->err++;
	nocc_message (warnbuf);
	sfree (warnbuf);

	return;
}
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
/*{{{  static int occampi_typehash_exceptiontypenamenode (langops_t *lops, tnode_t *node, int hsize, void *ptr)*/
/*
 *	generates a type-hash for an exception type declaration name node
 *	returns 0 on success, non-zero on failure
 */
static int occampi_typehash_exceptiontypenamenode (langops_t *lops, tnode_t *node, int hsize, void *ptr)
{
	name_t *sname = tnode_nthnameof (node, 0);
	tnode_t *stype = NameTypeOf (sname);
	unsigned int basehash = 0x3402fc90;
	char *pname = NameNameOf (sname);

	langops_typehash_blend (hsize, ptr, sizeof (basehash), (void *)&basehash);
	/* also blend in the name */
	langops_typehash_blend (hsize, ptr, strlen (pname), (unsigned char *)pname);
	if (stype) {
		int nitems, i;
		tnode_t **eitems = parser_getlistitems (stype, &nitems);

		for (i=0; i<nitems; i++) {
			unsigned int thishash = 0;

			if (langops_typehash (eitems[i], sizeof (thishash), (void *)&thishash)) {
				nocc_internal ("occampi_typehash_exceptiontypenamenode(): failed to get type-hash for (%s,%s)",
						eitems[i]->tag->name, eitems[i]->tag->ndef->name);
			} else {
				langops_typehash_blend (hsize, ptr, sizeof (thishash), (void *)&thishash);
			}
		}
	}

	return 0;
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
/*{{{  static int occampi_exceptioncheck_trynode (compops_t *cops, tnode_t **nodep, opiexception_t *oex)*/
/*
 *	called to exception-checking on a TRY block -- determines what isn't thrown back
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_exceptioncheck_trynode (compops_t *cops, tnode_t **nodep, opiexception_t *oex)
{
	opiexception_t *tryex = opi_newopiexception ();
	tnode_t *catches = treeops_findprocess (tnode_nthsubof (*nodep, 2));
	int i;

	/* see what exceptions are generated by the TRY */
	if (exceptioncheck_subtree (tnode_nthsubaddr (*nodep, 1), tryex)) {
		/* failed in here */
		opi_freeopiexception (tryex);
		oex->err++;
		return 0;
	}

	/* now prune them according to the ones caught here */
	tryex->doprune = 1;
	if (exceptioncheck_subtree (tnode_nthsubaddr (*nodep, 2), tryex)) {
		/* failed in here -- unlikely */
		opi_freeopiexception (tryex);
		oex->err++;
		return 0;
	}

	/* anything left is propagated */
	for (i=0; i<DA_CUR (tryex->elist); i++) {
		tnode_t *ename = DA_NTHITEM (tryex->elist, i);

		dynarray_maybeadd (oex->elist, ename);
	}
	oex->err += tryex->err;
	oex->warn += tryex->warn;

	opi_freeopiexception (tryex);

	/* now go through the exception handlers and any FINALLY to see what other exceptions are generated */
	if (exceptioncheck_subtree (tnode_nthsubaddr (*nodep, 2), oex) ||
			exceptioncheck_subtree (tnode_nthsubaddr (*nodep, 3), oex)) {
		/* failed in here */
		return 0;
	}

	return 0;
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
/*{{{  static int occampi_exceptioncheck_catchexprnode (compops_t *cops, tnode_t **nodep, opiexception_t *oex)*/
/*
 *	does exception-checking on a catch-expression node
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_exceptioncheck_catchexprnode (compops_t *cops, tnode_t **nodep, opiexception_t *oex)
{
	tnode_t *ename = tnode_nthsubof (*nodep, 0);

	if (oex->doprune) {
		/* pruning pass -- remove exception (it is caught) */
		if (!dynarray_hasitem (oex->elist, ename)) {
			/* not here */
			char *estr = NULL;

			langops_getname (ename, &estr);
			if (!estr) {
				estr = string_dup ("(unknown)");
			}

			exceptioncheck_warning (*nodep, oex, "exception [%s] is caught but never thrown", estr);
			sfree (estr);
		} else {
			dynarray_rmitem (oex->elist, ename);
		}
	} else {
		/* else non-prune pass, check body */
		exceptioncheck_subtree (tnode_nthsubaddr (*nodep, 1), oex);
	}

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
	if ((*nodep)->tag == opi.tag_THROW) {
		/*{{{  THROW*/
		tnode_t **eptr = tnode_nthsubaddr (*nodep, 1);

		if (*eptr && !parser_islistnode (*eptr)) {
			*eptr = parser_buildlistnode (NULL, *eptr, NULL);
		}
		/*}}}*/
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
	if (node->tag == opi.tag_THROW) {
		/*{{{  THROW*/
		tnode_t *name = tnode_nthsubof (node, 0);
		char *cname = NULL;

		/* subtypecheck name and expressions */
		typecheck_subtree (tnode_nthsubof (node, 0), tc);
		typecheck_subtree (tnode_nthsubof (node, 1), tc);

		langops_getname (name, &cname);

		if (name->tag != opi.tag_NEXCEPTIONTYPEDECL) {
			typecheck_error (node, tc, "THROW exception [%s] is not an EXCEPTION type", cname ?: "(unknown)");
		} else {
			tnode_t *ftype = typecheck_gettype (name, NULL);
			tnode_t *exprlist = tnode_nthsubof (node, 1);

			if (!exprlist && ftype) {
				int nfitems;

				parser_getlistitems (ftype, &nfitems);
				typecheck_error (node, tc, "THROW exception [%s] expected %d items, but found 0", cname ?: "(unknown)", nfitems);
			} else if (exprlist && !ftype) {
				int naitems;

				parser_getlistitems (exprlist, &naitems);
				typecheck_error (node, tc, "THROW exception [%s] expected 0 items, but found %d", cname ?: "(unknown)", naitems);
			} else if (!exprlist && !ftype) {
				/* good */
			} else {
				int naitems, nfitems, i;
				tnode_t **alist = parser_getlistitems (exprlist, &naitems);
				tnode_t **flist = parser_getlistitems (ftype, &nfitems);

				if (naitems != nfitems) {
					typecheck_error (node, tc, "THROW exception [%s] expected %d items, but found %d", cname ?: "(unknown)", nfitems, naitems);
				} else {
					for (i=0; i<nfitems; i++) {
						/* check that the actual is a good type for the formal */
						tnode_t *atype = typecheck_gettype (alist[i], flist[i]);

	#if 0
	fprintf (stderr, "occampi_typecheck_catchexprnode(): actual item is:\n");
	tnode_dumptree (alist[i], 1, stderr);
	fprintf (stderr, "occampi_typecheck_catchexprnode(): actual type is:\n");
	tnode_dumptree (atype, 1, stderr);
	#endif
						if (!atype) {
							typecheck_error (node, tc, "failed to get type of THROW exception [%s] expression %d", cname ?: "(unknown)", i+1);
						} else if (!typecheck_fixedtypeactual (flist[i], atype, node, tc, 1)) {
							typecheck_error (node, tc, "incompatible types for THROW exception [%s] expression %d", cname ?: "(unknown)", i+1);
						}
					}
				}
			}
		}

		if (cname) {
			sfree (cname);
		}
		/*}}}*/
	}

	return 0;
}
/*}}}*/
/*{{{  static int occampi_exceptioncheck_exceptionactionnode (compops_t *cops, tnode_t **nodep, opiexception_t *oex)*/
/*
 *	called to do exception-checking on a THROW node
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_exceptioncheck_exceptionactionnode (compops_t *cops, tnode_t **nodep, opiexception_t *oex)
{
	if ((*nodep)->tag == opi.tag_THROW) {
		/*{{{  THROW -- generates exception*/
		dynarray_maybeadd (oex->elist, tnode_nthsubof (*nodep, 0));

		/*}}}*/
	}
	return 1;
}
/*}}}*/

/*{{{  static int occampi_exceptioncheck_noexnode (compops_t *cops, tnode_t **nodep, opiexception_t *oex)*/
/*
 *	called to do exception-checking on a NOEXCEPTIONS block -- determines what is thrown back
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_exceptioncheck_noexnode (compops_t *cops, tnode_t **nodep, opiexception_t *oex)
{
	opiexception_t *noex = opi_newopiexception ();

	/* walk body */
	if (exceptioncheck_subtree (tnode_nthsubaddr (*nodep, 1), noex)) {
		opi_freeopiexception (noex);
		oex->err++;
		return 0;
	}

	if (DA_CUR (noex->elist)) {
		/* some exceptions thrown past here */
		char *elist = NULL;
		int i;

		for (i=0; i<DA_CUR (noex->elist); i++) {
			tnode_t *ename = DA_NTHITEM (noex->elist, i);
			char *estr = NULL;

			langops_getname (ename, &estr);
			if (estr) {
				char *newlist = string_fmt ("%s%s%s", (elist ?: ""), (elist ? "," : ""), estr);

				if (elist) {
					sfree (elist);
				}
				elist = newlist;
				sfree (estr);
			}
		}

		exceptioncheck_error (*nodep, oex, "NOEXCEPTIONS block has the following exceptions: %s", elist ?: "(unknown)");
		if (elist) {
			sfree (elist);
		}
	}
	oex->err += noex->err;
	oex->warn += noex->warn;

	opi_freeopiexception (noex);

	return 0;
}
/*}}}*/


/*{{{  static int occampi_exceptioncheck_scopein_procdecl (compops_t *cops, tnode_t **nodep, scope_t *ss)*/
/*
 *	inserted into scope-in pass for PROC declarations to scope in imported THROWs
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_exceptioncheck_scopein_procdecl (compops_t *cops, tnode_t **nodep, scope_t *ss)
{
	int v = 1;
	opiimportthrowshook_t *oph = (opiimportthrowshook_t *)tnode_getchook (*nodep, exceptioncheck_importthrowschook);

	if (oph) {
		int i;

		for (i=0; i<DA_CUR (oph->names); i++) {
			char *desc = DA_NTHITEM (oph->names, i);
			tnode_t **resp = DA_NTHITEMADDR (oph->resolved, i);

			if (*resp) {
				scope_warning (*nodep, ss, "in imported THROWs, already scoped in [%s]", desc);
			} else {
				name_t *name = name_lookupss (desc, ss);

				if (!name) {
					scope_error (*nodep, ss, "in imported THROWs, unresolved exception [%s]", desc);
				} else {
					*resp = NameNodeOf (name);
				}
			}
		}
	}

	if (cops->next && tnode_hascompop (cops->next, "scopein")) {
		v = tnode_callcompop (cops->next, "scopein", 2, nodep, ss);
	}
	return v;
}
/*}}}*/
/*{{{  static int occampi_exceptioncheck_scopeout_procdecl (compops_t *cops, tnode_t **nodep, scope_t *ss)*/
/*
 *	inserted into scope-out pass for PROC declarations (dummy)
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_exceptioncheck_scopeout_procdecl (compops_t *cops, tnode_t **nodep, scope_t *ss)
{
	int v = 1;

	if (cops->next && tnode_hascompop (cops->next, "scopeout")) {
		v = tnode_callcompop (cops->next, "scopeout", 2, nodep, ss);
	}
	return v;
}
/*}}}*/
/*{{{  static int occampi_exceptioncheck_importedtypecheck_procdecl (compops_t *cops, tnode_t *node, typecheck_t *tc)*/
/*
 *	called specifically for imported PROC declarations, to type-check thrown exceptions
 *	returns 0 to stop walk, 1 to continue (not relevant)
 */
static int occampi_exceptioncheck_importedtypecheck_procdecl (compops_t *cops, tnode_t *node, typecheck_t *tc)
{
	int v = 1;
	opiimportthrowshook_t *oph = (opiimportthrowshook_t *)tnode_getchook (node, exceptioncheck_importthrowschook);

	if (oph) {
		int i;
		
		for (i=0; i<DA_CUR (oph->resolved); i++) {
			tnode_t *exc = DA_NTHITEM (oph->resolved, i);

			if (exc->tag != opi.tag_NEXCEPTIONTYPEDECL) {
				typecheck_error (node, tc, "in imported THROWs, name [%s] is not an EXCEPTION type", DA_NTHITEM (oph->names, i));
			}
#if 0
fprintf (stderr, "occampi_exceptioncheck_importedtypecheck_procdecl(): here!\n");
#endif
		}
	}

	if (cops->next && tnode_hascompop (cops->next, "importedtypecheck")) {
		v = tnode_callcompop (cops->next, "importedtypecheck", 2, node, tc);
	}
	return v;
}
/*}}}*/
/*{{{  static int occampi_exceptioncheck_importedtyperesolve_procdecl (compops_t *cops, tnode_t **nodep, typecheck_t *tc)*/
/*
 *	called specifically for imported PROC declarations, to type-resolve thrown exceptions
 *	returns 0 to stop walk, 1 to continue (not relevant)
 */
static int occampi_exceptioncheck_importedtyperesolve_procdecl (compops_t *cops, tnode_t **nodep, typecheck_t *tc)
{
	int v = 1;
	opiimportthrowshook_t *oph = (opiimportthrowshook_t *)tnode_getchook (*nodep, exceptioncheck_importthrowschook);

	if (oph) {
		int i;

		for (i=0; i<DA_CUR (oph->resolved); i++) {
			char *echash = DA_NTHITEM (oph->thashs, i);
			tnode_t *exc = DA_NTHITEM (oph->resolved, i);
			unsigned int ethash = 0;

			if (sscanf (echash, "%x", &ethash) != 1) {
				typecheck_error (*nodep, tc, "bad type-hash [%s] on exception [%s] in imported THROWs",
						echash ?: "(null)", DA_NTHITEM (oph->names, i));
			} else {
				unsigned int myhash = 0;

				if (langops_typehash (exc, sizeof (myhash), &myhash)) {
					nocc_internal ("occampi_exceptioncheck_importedtyperesolve_procdecl(): failed to get type-hash for (%s,%s)",
							exc->tag->name, exc->tag->ndef->name);
				} else {
					if (myhash != ethash) {
#if 0
fprintf (stderr, "occampi_exceptioncheck_importedtyperesolve_procdecl(): import thash = 0x%8.8x, local thash = 0x%8.8x\n", ethash, myhash);
#endif
						typecheck_error (*nodep, tc, "imported type-hash for exception [%s] in imported THROWs differs from actual",
								DA_NTHITEM (oph->names, i));
					}
				}
			}
		}
	}

	if (cops->next && tnode_hascompop (cops->next, "importedtyperesolve")) {
		v = tnode_callcompop (cops->next, "importedtyperesolve", 2, nodep, tc);
	}
	return v;
}
/*}}}*/
/*{{{  static int occampi_exceptioncheck_precheck_procdecl (compops_t *cops, tnode_t *node)*/
/*
 *	called specifically for imported PROC declarations, to move imported exceptions over to real ones
 *	returns 0 to stop walk, 1 to continue (not relevant)
 */
static int occampi_exceptioncheck_precheck_procdecl (compops_t *cops, tnode_t *node)
{
	opiimportthrowshook_t *opiith = (opiimportthrowshook_t *)tnode_getchook (node, exceptioncheck_importthrowschook);
	opithrowshook_t *opith = (opithrowshook_t *)tnode_getchook (node, exceptioncheck_throwschook);

	if (opiith) {
		int i;

		for (i=0; i<DA_CUR (opiith->resolved); i++) {
			tnode_t *exc = DA_NTHITEM (opiith->resolved, i);

			if (exc) {
				if (!opith) {
					opith = opi_newopithrowshook ();
					tnode_setchook (node, exceptioncheck_throwschook, (void *)opith);
				}
				dynarray_maybeadd (opith->elist, exc);
			}
		}

		/* now get rid of it, all done! */
		opi_freeopiimportthrowshook (opiith);
		tnode_clearchook (node, exceptioncheck_importthrowschook);
	}
#if 0
fprintf (stderr, "occampi_exceptioncheck_precheck_procdecl(): opiith=%p, opith=%p\n", opiith, opith);
#endif
	return 1;
}
/*}}}*/
/*{{{  static int occampi_exceptioncheck_procdecl (compops_t *cops, tnode_t **nodep, opiexception_t *oex)*/
/*
 *	called to do exception-checking on a proc declaration -- determines what the PROC throws
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_exceptioncheck_procdecl (compops_t *cops, tnode_t **nodep, opiexception_t *oex)
{
	opiexception_t *procex = opi_newopiexception ();

#if 0
fprintf (stderr, "occampi_exceptioncheck_procdecl(): here!\n");
#endif
	/* check PROC body for exceptions generated */
	if (exceptioncheck_subtree (tnode_nthsubaddr (*nodep, 2), procex)) {
		/* failed */
		opi_freeopiexception (procex);
		oex->err++;
		return 0;
	}

	/* if there are any exceptions, attach them to the PROC */
	if (DA_CUR (procex->elist)) {
		opithrowshook_t *opith = opi_newopithrowshook ();
		int i;

		for (i=0; i<DA_CUR (procex->elist); i++) {
			dynarray_add (opith->elist, DA_NTHITEM (procex->elist, i));
		}
#if 0
fprintf (stderr, "occampi_exceptioncheck_procdecl(): setting throwschook to %p\n", opith);
#endif
		tnode_setchook (*nodep, exceptioncheck_throwschook, (void *)opith);
	}

	opi_freeopiexception (procex);

	/* check the in-scope body of the PROC */
	exceptioncheck_subtree (tnode_nthsubaddr (*nodep, 3), oex);

	return 0;
}
/*}}}*/
/*{{{  static int occampi_exceptioncheck_fetrans_procdecl (compops_t *cops, tnode_t **nodep, fetrans_t *fe)*/
/*
 *	inserted into front-end transformations on proc declaration nodes
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_exceptioncheck_fetrans_procdecl (compops_t *cops, tnode_t **nodep, fetrans_t *fe)
{
	int v = 1;
	
	if (tnode_haschook (*nodep, exceptioncheck_throwschook)) {
		opithrowshook_t *opith = (opithrowshook_t *)tnode_getchook (*nodep, exceptioncheck_throwschook);

		if (opith) {
			int i;

#if 0
fprintf (stderr, "occampi_exceptioncheck_fetrans_procdecl(): opith = %p\n", opith);
#endif
			/* PROC throws some exceptions, include in metadata */
			for (i=0; i<DA_CUR (opith->elist); i++) {
				tnode_t *ename = DA_NTHITEM (opith->elist, i);
				unsigned int thash = 0;

				if (langops_typehash (ename, sizeof (thash), &thash)) {
					nocc_internal ("occampi_exceptioncheck_fetrans_procdecl(): failed to get type-hash for (%s,%s)",
							ename->tag->name, ename->tag->ndef->name);
				} else {
					char *estr = NULL;
					char *sdata;

					langops_getname (ename, &estr);
					sdata = string_fmt ("%s#%8.8x", estr ?: "(unknown)", thash);
					if (estr) {
						sfree (estr);
					}

					metadata_addtonodelist (*nodep, "throws", sdata);
					sfree (sdata);
				}
			}
		}
	}

	/* do existing fetrans */
	if (cops->next && tnode_hascompop (cops->next, "fetrans")) {
		v = tnode_callcompop (cops->next, "fetrans", 2, nodep, fe);
	}
	return v;
}
/*}}}*/
/*{{{  static int occampi_exceptioncheck_inparams_namemap_procdecl (compops_t *cops, tnode_t **nodep, map_t *map)*/
/*
 *	inserted name-map on a PROC declaration (will be called for local and foreign PROCs)
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_exceptioncheck_inparams_namemap_procdecl (compops_t *cops, tnode_t **nodep, map_t *map)
{
	int v = 1;
	opithrowshook_t *opith = (opithrowshook_t *)tnode_getchook (*nodep, exceptioncheck_throwschook);

	if (opith && DA_CUR (opith->elist)) {
		tnode_t **paramsptr = tnode_nthsubaddr (*nodep, 1);
		tnode_t *tmpname;

		tmpname = map->target->newname (tnode_create (opi.tag_HIDDENPARAM, NULL, tnode_create (opi.tag_EXCEPTIONLINK, NULL)), NULL, map,
				map->target->pointersize, 0, 0, 0, map->target->pointersize, 1);
		parser_addtolist (*paramsptr, tmpname);
#if 0
fprintf (stderr, "occampi_exceptioncheck_inparams_namemap_procdecl(): here, PROC being declared is:\n");
tnode_dumptree (tnode_nthsubof (*nodep, 0), 1, stderr);
fprintf (stderr, "occampi_exceptioncheck_inparams_namemap_procdecl(): temporary to add to parameter-list is:\n");
tnode_dumptree (tmpname, 1, stderr);
#endif
	}
	if (cops->next && tnode_hascompop (cops->next, "inparams_namemap")) {
		v = tnode_callcompop (cops->next, "inparams_namemap", 2, nodep, map);
	}
	return v;
}
/*}}}*/
/*{{{  static int occampi_exceptioncheck_importmetadata_procdecl (langops_t *lops, tnode_t *node, const char *name, const char *data)*/
/*
 *	called to import metadata on a PROC declaration
 *	returns 0 on success, non-zero on failure
 */
static int occampi_exceptioncheck_importmetadata_procdecl (langops_t *lops, tnode_t *node, const char *name, const char *data)
{
	int r = 0;

	if (!strcmp (name, "throws")) {
		/* this one is for us! */
		opiimportthrowshook_t *oph = (opiimportthrowshook_t *)tnode_getchook (node, exceptioncheck_importthrowschook);
		const char *ch;

		if (!oph) {
			oph = opi_newopiimportthrowshook ();
			tnode_setchook (node, exceptioncheck_importthrowschook, (void *)oph);
		}

		/* find type-hash portion */
		for (ch=data; (*ch != '\0') && (*ch != '#'); ch++);

		dynarray_add (oph->names, string_ndup (data, (int)(ch - data)));
		if (*ch == '#') {
			dynarray_add (oph->thashs, string_dup (ch + 1));
		} else {
			dynarray_add (oph->thashs, NULL);
		}
		dynarray_add (oph->resolved, NULL);
#if 0
fprintf (stderr, "occampi_exceptioncheck_importmetadata_procdecl(): got some throws metadata [%s]\n", data);
#endif
	} else {
		if (lops->next && tnode_haslangop (lops->next, "importmetadata")) {
			r = tnode_calllangop (lops->next, "importmetadata", 3, node, name, data);
		}
	}
	return r;
}
/*}}}*/

/*{{{  static int occampi_exceptioncheck_instancenode (compops_t *cops, tnode_t **nodep, opiexception_t *oex)*/
/*
 *	called to do exception-checking on a proc instance -- determines what is thrown by the instanced PROC
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_exceptioncheck_instancenode (compops_t *cops, tnode_t **nodep, opiexception_t *oex)
{
	if ((*nodep)->tag == opi.tag_PINSTANCE) {
		/*{{{  PROC instance*/
		tnode_t *name = tnode_nthsubof (*nodep, 0);
		name_t *pname = tnode_nthnameof (name, 0);
		tnode_t *pdecl = NameDeclOf (pname);
		opithrowshook_t *opith = (opithrowshook_t *)tnode_getchook (pdecl, exceptioncheck_throwschook);

#if 0
fprintf (stderr, "occampi_exceptioncheck_instancenode(): PROC instance of:\n");
tnode_dumptree (pdecl, 1, stderr);
#endif
		if (opith) {
			/* local or foreign PROC throwing stuff */
			int i;

			for (i=0; i<DA_CUR (opith->elist); i++) {
				dynarray_maybeadd (oex->elist, DA_NTHITEM (opith->elist, i));
			}
		}
		/*}}}*/
	}
	return 0;
}
/*}}}*/
/*{{{  static int occampi_exceptioncheck_namemap_instancenode (compops_t *cops, tnode_t **nodep, map_t *map)*/
/*
 *	inserted name-map on a PROC instance
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_exceptioncheck_namemap_instancenode (compops_t *cops, tnode_t **nodep, map_t *map)
{
	int v = 1;

	if ((*nodep)->tag == opi.tag_PINSTANCE) {
		/*{{{  PROC instance*/
		tnode_t *name = tnode_nthsubof (*nodep, 0);
		name_t *pname = tnode_nthnameof (name, 0);
		tnode_t *pdecl = NameDeclOf (pname);
		opithrowshook_t *opith = (opithrowshook_t *)tnode_getchook (pdecl, exceptioncheck_throwschook);

		if (opith && DA_CUR (opith->elist)) {
#if 1
fprintf (stderr, "occampi_exceptioncheck_namemap_instancenode(): here, PROC being instanced is:\n");
tnode_dumptree (tnode_nthsubof (*nodep, 0), 1, stderr);
#endif
		}
		/*}}}*/
	}

	if (cops->next && tnode_hascompop (cops->next, "namemap")) {
		v = tnode_callcompop (cops->next, "namemap", 2, nodep, map);
	}
	return v;
}
/*}}}*/

/*{{{  static int exceptioncheck_cpass (tnode_t **treeptr)*/
/*
 *	called to do the compiler-pass for exceptions checking -- modprewalk for "exceptioncheck"
 *	returns 0 on success, non-zero on failure
 */
static int exceptioncheck_cpass (tnode_t **treeptr)
{
	opiexception_t *oex = opi_newopiexception ();
	int err;

	exceptioncheck_subtree (treeptr, oex);

	if (compopts.verbose) {
		nocc_message ("exceptioncheck_cpass(): exception-checked.  %d error(s), %d warning(s)", oex->err, oex->warn);
	}

	err = oex->err;
	opi_freeopiexception (oex);

	return err;
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

	/*{{{  compiler pass and operation for exception checking*/
	/* we'll need another pass (and compiler operation) which does an exceptions analysis,
	 * i.e. detemining which PROCs generate which exceptions
	 */
	if (nocc_addcompilerpass ("exception-check", INTERNAL_ORIGIN, "fetrans", 1, (int (*)(void *))exceptioncheck_cpass, CPASS_TREEPTR, -1, NULL)) {
		nocc_serious ("occampi_exceptions_post_setup(): failed to add \"exception-check\" pass!");
		return -1;
	}
	if (tnode_newcompop ("exceptioncheck", COPS_INVALID, 2, INTERNAL_ORIGIN) < 0) {
		nocc_serious ("occampi_exceptions_post_setup(): failed to add \"exceptioncheck\" compiler operation!");
		return -1;
	}

	/*}}}*/
	/*{{{  occampi:leafnode*/
	tnd = tnode_lookupnodetype ("occampi:leafnode");

	i = -1;
	opi.tag_EXCEPTIONLINK = tnode_newnodetag ("EXCEPTIONLINK", &i, tnd, NTF_NONE);

	/*}}}*/
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
	tnode_setlangop (lops, "typehash", 3, LANGOPTYPE (occampi_typehash_exceptiontypenamenode));
	tnd->lops = lops;

	i = -1;
	opi.tag_NEXCEPTIONTYPEDECL = tnode_newnodetag ("N_EXCEPTIONTYPEDECL", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  occampi:trynode -- TRY*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:trynode", &i, 4, 0, 0, TNF_LONGPROC);			/* subnodes: 0 = (none), 1 = body, 2 = catches, 3 = finally */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "prescope", 2, COMPOPTYPE (occampi_prescope_trynode));
	tnode_setcompop (cops, "exceptioncheck", 2, COMPOPTYPE (occampi_exceptioncheck_trynode));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnd->lops = lops;

	i = -1;
	opi.tag_TRY = tnode_newnodetag ("TRY", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  occampi:noexnode -- NOEXCEPTIONS*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:noexnode", &i, 2, 0, 0, TNF_LONGPROC);		/* subnodes: 0 = (none), 1 = body */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "exceptioncheck", 2, COMPOPTYPE (occampi_exceptioncheck_noexnode));
	tnd->ops = cops;

	i = -1;
	opi.tag_NOEXCEPTIONS = tnode_newnodetag ("NOEXCEPTIONS", &i, tnd, NTF_INDENTED_PROC);

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
	tnode_setcompop (cops, "exceptioncheck", 2, COMPOPTYPE (occampi_exceptioncheck_catchexprnode));
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
	tnode_setcompop (cops, "exceptioncheck", 2, COMPOPTYPE (occampi_exceptioncheck_exceptionactionnode));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnd->lops = lops;

	i = -1;
	opi.tag_THROW = tnode_newnodetag ("THROW", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  occampi:throws compiler hook*/
	exceptioncheck_throwschook = tnode_lookupornewchook ("occampi:throws");
	exceptioncheck_throwschook->chook_copy = exceptioncheck_throwschook_copy;
	exceptioncheck_throwschook->chook_free = exceptioncheck_throwschook_free;
	exceptioncheck_throwschook->chook_dumptree = exceptioncheck_throwschook_dumptree;

	/*}}}*/
	/*{{{  occampi:importthrows compiler hook*/
	exceptioncheck_importthrowschook = tnode_lookupornewchook ("occampi:importthrows");
	exceptioncheck_importthrowschook->chook_copy = exceptioncheck_importthrowschook_copy;
	exceptioncheck_importthrowschook->chook_free = exceptioncheck_importthrowschook_free;
	exceptioncheck_importthrowschook->chook_dumptree = exceptioncheck_importthrowschook_dumptree;

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
	tndef_t *tnd;
	compops_t *cops;
	langops_t *lops;

	/*{{{  intefere with PROC declaration and instance nodes for exception checking (and run-time handling)*/
	tnd = tnode_lookupnodetype ("occampi:procdecl");
	if (!tnd) {
		nocc_serious ("occampi_exceptions_post_setup(): failed to find \"occampi:procdecl\" node type");
		return -1;
	}
	cops = tnode_insertcompops (tnd->ops);
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (occampi_exceptioncheck_scopein_procdecl));
	tnode_setcompop (cops, "scopeout", 2, COMPOPTYPE (occampi_exceptioncheck_scopeout_procdecl));
	tnode_setcompop (cops, "importedtypecheck", 2, COMPOPTYPE (occampi_exceptioncheck_importedtypecheck_procdecl));
	tnode_setcompop (cops, "importedtyperesolve", 2, COMPOPTYPE (occampi_exceptioncheck_importedtyperesolve_procdecl));
	tnode_setcompop (cops, "importedprecheck", 1, COMPOPTYPE (occampi_exceptioncheck_precheck_procdecl));
	tnode_setcompop (cops, "exceptioncheck", 2, COMPOPTYPE (occampi_exceptioncheck_procdecl));
	tnode_setcompop (cops, "fetrans", 2, COMPOPTYPE (occampi_exceptioncheck_fetrans_procdecl));
	tnode_setcompop (cops, "inparams_namemap", 2, COMPOPTYPE (occampi_exceptioncheck_inparams_namemap_procdecl));
	tnd->ops = cops;
	lops = tnode_insertlangops (tnd->lops);
	tnode_setlangop (lops, "importmetadata", 3, LANGOPTYPE (occampi_exceptioncheck_importmetadata_procdecl));
	tnd->lops = lops;

	tnd = tnode_lookupnodetype ("occampi:instancenode");
	if (!tnd) {
		nocc_serious ("occampi_exceptions_post_setup(): failed to find \"occampi:instancenode\" node type");
		return -1;
	}
	cops = tnode_insertcompops (tnd->ops);
	tnode_setcompop (cops, "exceptioncheck", 2, COMPOPTYPE (occampi_exceptioncheck_instancenode));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (occampi_exceptioncheck_namemap_instancenode));
	tnd->ops = cops;

	/*}}}*/
	/*{{{  miscellaneous*/
	metadata_addreservedname ("throws");

	/*}}}*/
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


