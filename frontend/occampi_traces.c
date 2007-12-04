/*
 *	occampi_traces.c -- this deals with TRACES specifications
 *	Copyright (C) 2006-2007 Fred Barnes <frmb@kent.ac.uk>
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
#include "traceslang.h"


/*}}}*/
/*{{{  private types*/

typedef struct TAG_importtrace {
	DYNARRAY (char *, traces);
	DYNARRAY (tnode_t *, trees);		/* of parsed traces */
} importtrace_t;


/*}}}*/
/*{{{  private data*/

static compop_t *inparams_scopein_compop = NULL;
static compop_t *inparams_scopeout_compop = NULL;

static chook_t *trimplchook = NULL;
static chook_t *trtracechook = NULL;
static chook_t *trbvarschook = NULL;

static chook_t *trimportchook = NULL;

/*}}}*/


/*{{{  static importtrace_t *opi_newimporttrace (void)*/
/*
 *	creates a new importtrace_t structure
 */
static importtrace_t *opi_newimporttrace (void)
{
	importtrace_t *ipt = (importtrace_t *)smalloc (sizeof (importtrace_t));

	dynarray_init (ipt->traces);
	return ipt;
}
/*}}}*/
/*{{{  static void opi_freeimporttrace (importtrace_t *ipt)*/
/*
 *	frees an importtrace_t structure
 */
static void opi_freeimporttrace (importtrace_t *ipt)
{
	int i;

	if (!ipt) {
		nocc_serious ("opi_freeimporttrace(): NULL traces!");
		return;
	}
	for (i=0; i<DA_CUR (ipt->traces); i++) {
		char *str = DA_NTHITEM (ipt->traces, i);

		if (str) {
			sfree (str);
		}
	}
	dynarray_trash (ipt->traces);
	sfree (ipt);
	return;
}
/*}}}*/


/*{{{  static void *occampi_chook_traces_copy (void *chook)*/
/*
 *	copies an occampi:trace chook
 */
static void *occampi_chook_traces_copy (void *chook)
{
	tnode_t *tree = (tnode_t *)chook;

	if (tree) {
		return (void *)tnode_copytree (tree);
	}
	return NULL;
}
/*}}}*/
/*{{{  static void occampi_chook_traces_free (void *chook)*/
/*
 *	frees an occampi:trace chook
 */
static void occampi_chook_traces_free (void *chook)
{
	tnode_t *tree = (tnode_t *)chook;

	if (tree) {
		tnode_free (tree);
	}
	return;
}
/*}}}*/
/*{{{  static void occampi_chook_traces_dumptree (tnode_t *node, void *chook, int indent, FILE *stream)*/
/*
 *	dumps an occampi:trace chook (debugging)
 */
static void occampi_chook_traces_dumptree (tnode_t *node, void *chook, int indent, FILE *stream)
{
	tnode_t *traces = (tnode_t *)chook;

	occampi_isetindent (stream, indent);
	fprintf (stream, "<chook:occampi:trace addr=\"0x%8.8x\">\n", (unsigned int)chook);
	if (traces) {
		tnode_dumptree (traces, indent+1, stream);
	}
	occampi_isetindent (stream, indent);
	fprintf (stream, "</chook:occampi:trace>\n");

	return;
}
/*}}}*/

/*{{{  static void *occampi_chook_importtrace_copy (void *chook)*/
/*
 *	copies a trimportchook compiler hook (imported TRACEs)
 */
static void *occampi_chook_importtrace_copy (void *chook)
{
	importtrace_t *ipt = (importtrace_t *)chook;

	if (ipt) {
		importtrace_t *newipt = opi_newimporttrace ();
		int i;

		for (i=0; i<DA_CUR (ipt->traces); i++) {
			char *str = DA_NTHITEM (ipt->traces, i);

			dynarray_add (newipt->traces, str ? string_dup (str) : NULL);
		}
		return newipt;
	}
	return NULL;
}
/*}}}*/
/*{{{  static void occampi_chook_importtrace_free (void *chook)*/
/*
 *	frees a trimportchook compiler hook (imported TRACEs)
 */
static void occampi_chook_importtrace_free (void *chook)
{
	importtrace_t *ipt = (importtrace_t *)chook;

	if (ipt) {
		opi_freeimporttrace (ipt);
	}
	return;
}
/*}}}*/
/*{{{  static void occampi_chook_importtrace_dumptree (tnode_t *node, void *chook, int indent, FILE *stream)*/
/*
 *	dumps an occampi:importtrace compiler hook (debugging)
 */
static void occampi_chook_importtrace_dumptree (tnode_t *node, void *chook, int indent, FILE *stream)
{
	importtrace_t *ipt = (importtrace_t *)chook;

	occampi_isetindent (stream, indent);
	if (ipt) {
		int i;

		fprintf (stream, "<chook:occampi:importtrace addr=\"0x%8.8x\">\n", (unsigned int)ipt);
		for (i=0; i<DA_CUR (ipt->traces); i++) {
			char *str = DA_NTHITEM (ipt->traces, i);

			occampi_isetindent (stream, indent+1);
			fprintf (stream, "<trace value=\"%s\" />\n", str ?: "(null)");
		}
		for (i=0; i<DA_CUR (ipt->trees); i++) {
			tnode_t *tree = DA_NTHITEM (ipt->trees, i);

			tnode_dumptree (tree, indent+1, stream);
		}
		occampi_isetindent (stream, indent);
		fprintf (stream, "</chook:occampi:importtrace>\n");
	} else {
		fprintf (stream, "<chook:occampi:importtrace />\n");
	}
}
/*}}}*/

/*{{{  static int occampi_scopein_traces (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	called to scope-in a trace
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_scopein_traces (compops_t *cops, tnode_t **node, scope_t *ss)
{
	/* falls through into the MCSP below */
	return 1;
}
/*}}}*/
/*{{{  static int occampi_scopeout_traces (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	called to scope-out a trace
 */
static int occampi_scopeout_traces (compops_t *cops, tnode_t **node, scope_t *ss)
{
	return 1;
}
/*}}}*/

/*{{{  static int occampi_prescope_tracetypedecl (compops_t *cops, tnode_t **node, prescope_t *ps)*/
/*
 *	called to do pre-scoping on a TRACETYPEDECL node -- will parse actual specification
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_prescope_tracetypedecl (compops_t *cops, tnode_t **node, prescope_t *ps)
{
	tnode_t **rhsptr = tnode_nthsubaddr (*node, 3);
	lexfile_t *lf;
	char *lstr, *fname, *newfname;
	tnode_t *newtree;
	tnode_t *params;

	lstr = occampi_litstringcopy (*rhsptr);
	if (!lstr) {
		prescope_error (*node, ps, "RHS of TRACE TYPE declaration must be a string literal");
		return 1;
	}

	/* get this filename and line-number */
	fname = tnode_copytextlocationof (*node);
	if (!fname) {
		newfname = (char *)smalloc (64);
	} else {
		newfname = (char *)smalloc (strlen (fname) + 16);
	}
	sprintf (newfname, "%s$traceslang", fname ?: "(unknown file)");
	if (fname) {
		sfree (fname);
	}

	lf = lexer_openbuf (newfname, "traceslang", lstr);
	sfree (newfname);
	if (!lf) {
		prescope_error (*node, ps, "occampi_prescope_tracetypedecl(): failed to open traces string for parsing");
		sfree (lstr);
		return 1;
	}

	newtree = parser_parse (lf);

	/* accumulate errors and warnings */
	ps->err += lf->errcount;
	ps->warn += lf->warncount;

	lexer_close (lf);

	if (!newtree) {
		prescope_error (*node, ps, "failed to parse traces \"%s\"", lstr);
		sfree (lstr);
		return 1;
	}

	/* destroy the existing RHS and put in ours */
	tnode_free (*rhsptr);
	*rhsptr = newtree;
	sfree (lstr);

	/* if parameter set is not a list, make it one */
	params = tnode_nthsubof (*node, 1);
	if (!parser_islistnode (params)) {
		params = parser_buildlistnode (OrgFileOf (*node), params, NULL);
		tnode_setnthsub (*node, 1, params);
	}

#if 0
fprintf (stderr, "occampi_prescope_tracetypedecl(): parsed RHS is:\n");
tnode_dumptree (newtree, 1, stderr);
#endif
	/* prescope subnodes */
	return 1;
}
/*}}}*/
/*{{{  static int occampi_scopein_tracetypedecl (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	called to scope-in a traces type declaration
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_scopein_tracetypedecl (compops_t *cops, tnode_t **node, scope_t *ss)
{
	tnode_t *name = tnode_nthsubof (*node, 0);
	tnode_t *type = tnode_nthsubof (*node, 1);
	name_t *sname = NULL;
	tnode_t *newname = NULL;
	char *rawname;
	tnode_t **litems;
	int nlitems, i;
	void *nsmark;

	nsmark = name_markscope ();

	if (name->tag != opi.tag_NAME) {
		scope_error (name, ss, "name not raw-name!");
		return 0;
	}

	rawname = (char *)tnode_nthhookof (name, 0);

	/* type will be a parameter list of some form (tagged names) */
	litems = parser_getlistitems (type, &nlitems);
	for (i=0; i<nlitems; i++) {
		tnode_t *tparam = litems[i];
		occampi_typeattr_t tattr = (occampi_typeattr_t)tnode_getchook (tparam, opi.chook_typeattr);

		if (tparam->tag != opi.tag_NAME) {
			scope_error (tparam, ss, "name not raw-name!");
		} else {
			char *prawname = (char *)tnode_nthhookof (tparam, 0);
			name_t *pname;
			tnode_t *ptype = traceslang_newevent (tparam);
			tnode_t *pnewname;

			pname = name_addscopename (prawname, *node, ptype, NULL);
			pnewname = traceslang_newnparam (tparam);
			tnode_setnthname (pnewname, 0, pname);
			SetNameNode (pname, pnewname);

			/* free old, plant new */
			tnode_free (litems[i]);
			litems[i] = pnewname;
			tnode_setchook (pnewname, opi.chook_typeattr, (void *)tattr);
			ss->scoped++;
		}
	}
#if 0
fprintf (stderr, "occampi_scopein_tracetypedecl(): parameter list is now:\n");
tnode_dumptree (type, 1, stderr);
#endif

	/* scope expression */
	if (scope_subtree (tnode_nthsubaddr (*node, 3), ss)) {
		/* failed in here somewhere, descope params first! */
		name_markdescope (nsmark);
		return 0;
	}

	/* descope trace parameters */
	name_markdescope (nsmark);

	sname = name_addscopename (rawname, *node, type, NULL);
	newname = tnode_createfrom (opi.tag_NTRACETYPEDECL, name, sname);
	SetNameNode (sname, newname);
	tnode_setnthsub (*node, 0, newname);

	/* free old name */
	tnode_free (name);
	ss->scoped++;

	/* scope body */
	if (scope_subtree (tnode_nthsubaddr (*node, 2), ss)) {
		name_descopename (sname);
		return 0;
	}

	name_descopename (sname);
	return 0;
}
/*}}}*/
/*{{{  static int occampi_scopeout_tracetypedecl (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	called to scope-out a traces type declaration
 *	returns 0 to stop walk, 1 to continue [irrelevant, postorder]
 */
static int occampi_scopeout_tracetypedecl (compops_t *cops, tnode_t **node, scope_t *ss)
{
	/* FIXME! */
	return 1;
}
/*}}}*/
/*{{{  static int occampi_precheck_tracetypedecl (compops_t *cops, tnode_t *node)*/
/*
 *	called to do pre-checking on a traces type declaration, tidies up anything type-resolution may have done
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_precheck_tracetypedecl (compops_t *cops, tnode_t *node)
{
	tnode_t **traceptr = tnode_nthsubaddr (node, 3);

	*traceptr = traceslang_simplifyexpr (*traceptr);
#if 0
fprintf (stderr, "occampi_precheck_tracetypedecl(): got trace name:\n");
tnode_dumptree (tnode_nthsubof (node, 0), 1, stderr);
fprintf (stderr, "occampi_precheck_tracetypedecl(): got traces:\n");
tnode_dumptree (*traceptr, 1, stderr);
#endif
	return 1;
}
/*}}}*/

/*{{{  static int occampi_prescope_traceimplspec (compops_t *cops, tnode_t **node, prescope_t *ps)*/
/*
 *	called to pre-scope a traces implementation specification (found on the RHS of PROC declarations)
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_prescope_traceimplspec (compops_t *cops, tnode_t **node, prescope_t *ps)
{
	tnode_t *rhs = tnode_nthsubof (*node, 1);

	/* expecting the RHS to be a parameter list of some form */
	if (!parser_islistnode (rhs)) {
		rhs = parser_buildlistnode (NULL, rhs, NULL);
		tnode_setnthsub (*node, 1, rhs);
	}

	return 1;
}
/*}}}*/

/*{{{  static int occampi_getname_tracenamenode (langops_t *lops, tnode_t *node, char **str)*/
/*
 *	gets the name of a traces name-node (trivial)
 *	returns 0 on success, -ve on failure
 */
static int occampi_getname_tracenamenode (langops_t *lops, tnode_t *node, char **str)
{
	name_t *name = tnode_nthnameof (node, 0);
	char *pname;

	if (!name) {
		nocc_fatal ("occampi_getname_tracenamenode(): NULL name!");
		return -1;
	}
	pname = NameNameOf (name);
	*str = string_dup (pname);

	return 0;
}
/*}}}*/
/*{{{  static tnode_t *occampi_gettype_tracenamenode (langops_t *lops, tnode_t *node, tnode_t *default_type)*/
/*
 *	returns the type of a traces name-node (trivial)
 */
static tnode_t *occampi_gettype_tracenamenode (langops_t *lops, tnode_t *node, tnode_t *default_type)
{
	name_t *name = tnode_nthnameof (node, 0);

	if (!name) {
		nocc_fatal ("cocampi_gettype_tracenamenode(): NULL name!");
		return NULL;
	}
	if (name->type) {
		return name->type;
	}
	nocc_fatal ("occampi_gettype_tracenamenode(): name has NULL type (FIXME!)");
	return NULL;
}
/*}}}*/
/*{{{  static tnode_t *occampi_traceslang_getparams_tracenamenode (langops_t *lops, tnode_t *node)*/
/*
 *	this is used to extract the parameters for a named trace type (by traceslang)
 *	returns the parameter tree on success, NULL if none
 */
static tnode_t *occampi_traceslang_getparams_tracenamenode (langops_t *lops, tnode_t *node)
{
	name_t *name = tnode_nthnameof (node, 0);
	tnode_t *decl;

	if (!name) {
		nocc_fatal ("occampi_traceslang_getparams_tracenamenode(): NULL name!");
		return NULL;
	}
	decl = NameDeclOf (name);
	if (decl->tag != opi.tag_TRACETYPEDECL) {
		tnode_error (node, "named trace is not an occam-pi trace type!");
		return NULL;
	}
	return tnode_nthsubof (decl, 1);
}
/*}}}*/
/*{{{  static tnode_t *occampi_traceslang_getbody_tracenamenode (langops_t *lops, tnode_t *node)*/
/*
 *	this is used to extract the body for a named trace type (by traceslang)
 *	returns the body on success, NULL on failure
 */
static tnode_t *occampi_traceslang_getbody_tracenamenode (langops_t *lops, tnode_t *node)
{
	name_t *name = tnode_nthnameof (node, 0);
	tnode_t *decl;

	if (!name) {
		nocc_fatal ("occampi_traceslang_getbody_tracenamenode(): NULL name!");
		return NULL;
	}
	decl = NameDeclOf (name);
	if (decl->tag != opi.tag_TRACETYPEDECL) {
		tnode_error (node, "named trace is not an occam-pi trace type!");
		return NULL;
	}
	return tnode_nthsubof (decl, 3);
}
/*}}}*/


/*{{{  static int occampi_prescope_procdecl_tracetypeimpl (compops_t *cops, tnode_t **node, prescope_t *ps)*/
/*
 *	does pre-scoping on a PROCDECL node to handle prescoping of traces
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_prescope_procdecl_tracetypeimpl (compops_t *cops, tnode_t **node, prescope_t *ps)
{
	int v = 1;
	tnode_t *trimpl;
	importtrace_t *ipt;

#if 0
fprintf (stderr, "occampi_prescope_procdecl_tracetypeimpl(): PROCDECL, name =\n");
tnode_dumptree (tnode_nthsubof (*node, 0), 1, stderr);
#endif
	ipt = (importtrace_t *)tnode_getchook (*node, trimportchook);
	if (ipt) {
		/*{{{  got some imported traces here, need to parse*/
		int i;

		for (i=0; i<DA_CUR (ipt->traces); i++) {
			char *str = DA_NTHITEM (ipt->traces, i);
			char *fname, *newfname;
			lexfile_t *lf;
			tnode_t *tree = NULL;

			fname = tnode_copytextlocationof (*node);
			if (fname) {
				newfname = string_fmt ("%s$traceslang", fname);
				sfree (fname);
			} else {
				newfname = string_dup ("(unknown file)$traceslang");
			}

			lf = lexer_openbuf (newfname, "traceslang", str);
			sfree (newfname);
			if (!lf) {
				prescope_error (*node, ps, "occampi_prescope_procdecl_tracetypeimpl(): failed to open traces string for parsing");
			} else {

				tree = parser_parse (lf);
				ps->err += lf->errcount;
				ps->warn += lf->warncount;

				lexer_close (lf);
				if (!tree) {
					prescope_error (*node, ps, "failed to parse imported trace \"%s\"", str);
				} else {
					/* expecting tree to be a list, but will have a single trace in it */
					if (parser_islistnode (tree)) {
						int nitems;
						tnode_t **items = parser_getlistitems (tree, &nitems);

						if (nitems == 1) {
							tnode_t *newtree = items[0];

							parser_delfromlist (tree, 0);
							tnode_free (tree);
							tree = newtree;
						} else {
							prescope_error (*node, ps, "expected 1 trace in imported trace, got %d", nitems);
						}
					} else {
						prescope_warning (*node, ps, "expected list of imported traces, got (%s,%s)",
								tree->tag->name, tree->tag->ndef->name);
					}
				}
				dynarray_add (ipt->trees, tree);
			}
		}
		/*}}}*/
	}

	if (tnode_hascompop (cops->next, "prescope")) {
		v = tnode_callcompop (cops->next, "prescope", 2, node, ps);
	}

	trimpl = (tnode_t *)tnode_getchook (*node, trimplchook);

	if (trimpl) {
		/* got something here! */
		tnode_clearchook (*node, trimplchook);
		prescope_subtree (&trimpl, ps);

		/* if trimpl is not a list, make it into one */
		if (!parser_islistnode (trimpl)) {
			trimpl = parser_buildlistnode (NULL, trimpl, NULL);
		}
		tnode_setchook (*node, trimplchook, trimpl);
#if 0
fprintf (stderr, "occampi_prescope_procdecl_tracetypeimpl(): got trace implementation on PROCDECL:\n");
tnode_dumptree (trimpl, 1, stderr);
#endif
	}
	return v;
}
/*}}}*/
/*{{{  static int occampi_inparams_scopein_procdecl_tracetypeimpl (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	does scope-in on a PROCDECL node to handle scoping of traces
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_inparams_scopein_procdecl_tracetypeimpl (compops_t *cops, tnode_t **node, scope_t *ss)
{
	int v = 1;
	tnode_t *trimpl;
	importtrace_t *ipt;

#if 0
fprintf (stderr, "occampi_inparams_scopein_procdecl_tracetypeimpl(): here!\n");
#endif
	/* scope the proc declaration proper first -- any traces implementation hook would be ignored  */
	if (tnode_hascompop (cops->next, "inparams_scopein")) {
		v = tnode_callcompop (cops->next, "inparams_scopein", 2, node, ss);
	}

	trimpl = (tnode_t *)tnode_getchook (*node, trimplchook);
	if (trimpl) {
		/* got something here! */
		tnode_clearchook (*node, trimplchook);
		scope_subtree (&trimpl, ss);
		tnode_setchook (*node, trimplchook, trimpl);
	}

	ipt = (importtrace_t *)tnode_getchook (*node, trimportchook);
	if (ipt) {
		int i;

		/* allow occam-pi parameters to be used in these traces */
		traceslang_registertracetype (opi.tag_NPARAM);
		for (i=0; i<DA_CUR (ipt->trees); i++) {
			scope_subtree (DA_NTHITEMADDR (ipt->trees, i), ss);
		}
		traceslang_unregistertracetype (opi.tag_NPARAM);
	}

	return v;
}
/*}}}*/
/*{{{  static int occampi_typecheck_procdecl_tracetypeimpl (compops_t *cops, tnode_t *node, typecheck_t *tc)*/
/*
 *	does type-checking on a PROCDECL node to handle trace specifications
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_typecheck_procdecl_tracetypeimpl (compops_t *cops, tnode_t *node, typecheck_t *tc)
{
	int v = 1;
	tnode_t *trimpl;

	if (tnode_hascompop (cops->next, "typecheck")) {
		v = tnode_callcompop (cops->next, "typecheck", 2, node, tc);
	}

	trimpl = (tnode_t *)tnode_getchook (node, trimplchook);

	if (trimpl) {
		int nitems, i;
		tnode_t **items = parser_getlistitems (trimpl, &nitems);
#if 0
fprintf (stderr, "occampi_typecheck_procdecl_tracetypeimpl(): got traces implementation here:\n");
tnode_dumptree (trimpl, 1, stderr);
#endif

		for (i=0; i<nitems; i++) {
			tnode_t *trim = items[i];

			if (trim->tag == opi.tag_TRACEIMPLSPEC) {
				/*{{{  check that the LHS is a TRACETYPEDECL name, RHS is a parameter list of synchronisation types*/
				tnode_t *lhs = tnode_nthsubof (trim, 0);
				tnode_t *rhs = tnode_nthsubof (trim, 1);
				char *name = NULL;

				langops_getname (lhs, &name);

				if (!parser_islistnode (rhs)) {
					typecheck_error (node, tc, "missing parameter list for trace implementation [%s]", name ?: "(unknown)");
				} else if (lhs->tag != opi.tag_NTRACETYPEDECL) {
					typecheck_error (node, tc, "name [%s] is not a trace-type", name ?: "(unknown)");
				} else {
					int nparams, j;
					int nfparams, k;
					tnode_t **params = parser_getlistitems (rhs, &nparams);
					tnode_t **fparams = NULL;
					tnode_t *lhstype = typecheck_gettype (lhs, NULL);

#if 0
fprintf (stderr, "occampi_typecheck_procdecl_tracetypeimpl(): LHS type is:\n");
tnode_dumptree (lhstype, 1, stderr);
#endif
					if (!parser_islistnode (lhstype)) {
						typecheck_error (node, tc, "trace [%s] has a broken type", name ?: "(unknown)");
						nfparams = 0;
					} else {
						fparams = parser_getlistitems (lhstype, &nfparams);
					}

					/* check each parameter given */
					for (j=k=0; (j<nparams) && (k<nfparams); j++, k++) {
						tnode_t *ptype = typecheck_gettype (params[j], NULL);

#if 0
fprintf (stderr, "occampi_typecheck_procdecl_tracetypeimpl(): actual parameter type for %d is:\n", j);
tnode_dumptree (ptype, 1, stderr);
#endif
						if (!(tnode_ntflagsof (ptype) & NTF_SYNCTYPE)) {
							char *pname = NULL;

							langops_getname (params[j], &pname);

							typecheck_error (node, tc, "parameter [%s] is not a synchronisation type", pname ?: "(unknown)");
							if (pname) {
								sfree (pname);
							}
						} else {
							occampi_typeattr_t pattr, tattr;

							if (tnode_haslangop (params[j]->tag->ndef->lops, "occampi_typeattrof")) {
								tnode_calllangop (params[j]->tag->ndef->lops, "occampi_typeattrof", 2, params[j], &pattr);
							} else {
								pattr = TYPEATTR_NONE;
							}
							/* formal attribute will be on a compiler hook (opi.chook_typeattr) */
							if (tnode_haschook (fparams[k], opi.chook_typeattr)) {
								tattr = (occampi_typeattr_t)tnode_getchook (fparams[k], opi.chook_typeattr);
							} else {
								tattr = TYPEATTR_NONE;
							}

							if ((pattr != TYPEATTR_NONE) && (tattr != TYPEATTR_NONE) && (pattr ^ tattr)) {
								typecheck_error (node, tc, "type mismatch for parameter %d on trace type", j);
							}
						}
					}
				}

				if (name) {
					sfree (name);
				}
				/*}}}*/
			} else {
				typecheck_error (node, tc, "unsupported trace implementation type [%s]", trim->tag->name);
			}
		}
	}

	return v;
}
/*}}}*/
/*{{{  static int occampi_typeresolve_procdecl_tracetypeimpl (compops_t *cops, tnode_t **nodep, typecheck_t *tc)*/
/*
 *	used to remove any TYPESPEC nodes that have accumulated in actual parameters prior to real checks for PROCDECL trace specifications
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_typeresolve_procdecl_tracetypeimpl (compops_t *cops, tnode_t **nodep, typecheck_t *tc)
{
	int v = 1;
	tnode_t *trimpl;

	if (tnode_hascompop (cops->next, "typeresolve")) {
		v = tnode_callcompop (cops->next, "typeresolve", 2, nodep, tc);
	}

	trimpl = (tnode_t *)tnode_getchook (*nodep, trimplchook);

	if (trimpl) {
		int nitems, i;
		tnode_t **items = parser_getlistitems (trimpl, &nitems);

		for (i=0; i<nitems; i++) {
			tnode_t *item = items[i];

			if (item->tag == opi.tag_TRACEIMPLSPEC) {
				tnode_t *rhs = tnode_nthsubof (item, 1);
				int nsparams, j;
				tnode_t **sparams = parser_getlistitems (rhs, &nsparams);

				for (j=0; j<nsparams; j++) {
					tnode_t **sitemp = sparams + j;

					if ((*sitemp)->tag == opi.tag_TYPESPEC) {
						*sitemp = tnode_nthsubof (*sitemp, 0);
					}
				}
			}
		}
	}
	return v;
}
/*}}}*/
/*{{{  static int occampi_precheck_procdecl_tracetypeimpl (compops_t *cops, tnode_t *node)*/
/*
 *	does pre-checks on a PROCDECL node, does parameter substitution on trace implementations for later checking
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_precheck_procdecl_tracetypeimpl (compops_t *cops, tnode_t *node)
{
	int v = 1;
	tnode_t *trimpl;

	if (tnode_hascompop (cops->next, "precheck")) {
		v = tnode_callcompop (cops->next, "precheck", 1, node);
	}

	trimpl = (tnode_t *)tnode_getchook (node, trimplchook);

	if (trimpl) {
		int nitems, i;
		tnode_t **items = parser_getlistitems (trimpl, &nitems);

		for (i=0; i<nitems; i++) {
			tnode_t **itemp = items + i;

#if 0
fprintf (stderr, "occampi_precheck_procdecl_tracetypeimpl(): got trace specification item:\n");
tnode_dumptree (*itemp, 1, stderr);
#endif
			if ((*itemp)->tag == opi.tag_TRACEIMPLSPEC) {
				tnode_t *lhs = tnode_nthsubof (*itemp, 0);
				tnode_t *rhs = tnode_nthsubof (*itemp, 1);
				name_t *name;
				tnode_t *ndecl, *trtype, *trparams;
				tnode_t *trcopy, *bvlist;
				tnode_t **fpset, **apset;
				int nfp, nap, j;

				/* lhs should be a NTRACETYPEDECL, rhs is a list of parameters */
				if (lhs->tag != opi.tag_NTRACETYPEDECL) {
					nocc_internal ("occampi_precheck_procdecl_tracetypeimpl(): expected trace type, got [%s]", lhs->tag->name);
					return 0;
				}
				name = tnode_nthnameof (lhs, 0);
				ndecl = NameDeclOf (name);
				if (ndecl->tag != opi.tag_TRACETYPEDECL) {
					nocc_internal ("occampi_precheck_procdecl_tracetypeimpl(): expected trace type declaration, got [%s]", ndecl->tag->name);
					return 0;
				}
				trtype = tnode_nthsubof (ndecl, 3);
				trparams = tnode_nthsubof (ndecl, 1);

				trcopy = traceslang_structurecopy (trtype);

				fpset = parser_getlistitems (trparams, &nfp);
				apset = parser_getlistitems (rhs, &nap);

				if (nfp != nap) {
					nocc_internal ("occampi_precheck_procdecl_tracetypeimpl(): expected %d parameters on trace type specification, got %d",
							nfp, nap);
					return 0;
				}

				trcopy = treeops_substitute (trcopy, fpset, apset, nfp);
#if 0
fprintf (stderr, "occampi_precheck_procdecl_tracetypeimpl(): got trace type(s):\n");
tnode_dumptree (trtype, 1, stderr);
fprintf (stderr, "occampi_precheck_procdecl_tracetypeimpl(): got trace type(s) copy:\n");
tnode_dumptree (trcopy, 1, stderr);
fprintf (stderr, "occampi_precheck_procdecl_tracetypeimpl(): got formal parameters:\n");
tnode_dumptree (trparams, 1, stderr);
fprintf (stderr, "occampi_precheck_procdecl_tracetypeimpl(): got actual parameters:\n");
tnode_dumptree (rhs, 1, stderr);
#endif

				/* right, put this in there! */
				*itemp = trcopy;

				/* save a copy of the actual parameters, so we know what we is relevant */
				bvlist = parser_newlistnode (NULL);
				for (j=0; j<nap; j++) {
					parser_addtolist (bvlist, apset[j]);
				}
				tnode_setchook (trcopy, trbvarschook, bvlist);
			}
		}
	}

	return v;
}
/*}}}*/
/*{{{  static int occampi_tracescheck_procdecl_tracetypeimpl (compops_t *cops, tnode_t *node, tchk_state_t *tcstate)*/
/*
 *	does traces-checking on a PROCDECL node, checks against specification
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_tracescheck_procdecl_tracetypeimpl (compops_t *cops, tnode_t *node, tchk_state_t *tcstate)
{
	int v = 0;
	tnode_t *trimpl;

	if (tnode_hascompop (cops->next, "tracescheck")) {
		v = tnode_callcompop (cops->next, "tracescheck", 2, node, tcstate);
	}

	trimpl = (tnode_t *)tnode_getchook (node, trimplchook);

	if (trimpl) {
		tchk_traces_t *trs = (tchk_traces_t *)tnode_getchook (node, trtracechook);

		if (trs) {
			int nitems, i;
			tnode_t **items = parser_getlistitems (trimpl, &nitems);

			/* each item comes from a list of disjoint possible traces */
			for (i=0; i<nitems; i++) {
				int ntrspecs, j;
				tnode_t **trspecs = parser_getlistitems (items[i], &ntrspecs);
				tnode_t *bvars = (tnode_t *)tnode_getchook (items[i], trbvarschook);
				int okaycount = 0;
			
				for (j=0; j<ntrspecs; j++) {
					tchk_traces_t *trscopy = tracescheck_copytraces (trs);

					tracescheck_prunetraces (trscopy, bvars);
					tracescheck_simplifytraces (trscopy);
#if 0
fprintf (stderr, "occampi_tracescheck_procdecl_tracetypeimpl(): want to check trace:\n");
tracescheck_dumptraces (trscopy, 1, stderr);
fprintf (stderr, "occampi_tracescheck_procdecl_tracetypeimpl(): bound variables in trace are:\n");
tnode_dumptree (bvars, 1, stderr);
fprintf (stderr, "occampi_tracescheck_procdecl_tracetypeimpl(): against specification:\n");
tnode_dumptree (trspecs[j], 1, stderr);
#endif

					if (!tracescheck_docheckspec (trspecs[j], trscopy, tcstate, node)) {
						okaycount++;
					}

					tracescheck_freetraces (trscopy);
				}
				if (!okaycount) {
					tracescheck_error (node, tcstate, "PROC failed to meet TRACES specification");
				}
			}
		}
	}

	return v;
}
/*}}}*/
/*{{{  static int occampi_fetrans_procdecl_tracetypeimpl (compops_t *cops, tnode_t **nodep, fetrans_t *fe)*/
/*
 *	inserted fetrans to attach traces to PROCDECL metadata
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_fetrans_procdecl_tracetypeimpl (compops_t *cops, tnode_t **nodep, fetrans_t *fe)
{
	int v = 1;
	
	if (tnode_haschook (*nodep, trtracechook)) {
		tchk_traces_t *tr = (tchk_traces_t *)tnode_getchook (*nodep, trtracechook);
		int i;

		for (i=0; i<DA_CUR (tr->items); i++) {
			tchknode_t *tcn = DA_NTHITEM (tr->items, i);
			char *str = NULL;

			if (tracescheck_formattraces (tcn, &str)) {
				tnode_error (*nodep, "occampi_fetrans_procdecl_tracetypeimpl(): failed to format traces..");
			} else {
				metadata_addtonodelist (*nodep, "traces", str);
#if 0
fprintf (stderr, "occampi_fetrans_procdecl_tracetypeimpl(): formatted traces: [%s]\n", str);
#endif
				sfree (str);
			}
		}
	}

	if (cops->next && tnode_hascompop (cops->next, "fetrans")) {
		v = tnode_callcompop (cops->next, "fetrans", 2, nodep, fe);
	}

	return v;
}
/*}}}*/

/*{{{  static int occampi_importmetadata_procdecl_tracetypeimpl (langops_t *lops, tnode_t *node, const char *name, const char *data)*/
/*
 *	called to import metadata on PROC declaration
 *	returns 0 on success, non-zero on failure
 */
static int occampi_importmetadata_procdecl_tracetypeimpl (langops_t *lops, tnode_t *node, const char *name, const char *data)
{
	int r = 0;

	if (!strcmp (name, "traces")) {
		/* this one is for us! */
		importtrace_t *ipt = (importtrace_t *)tnode_getchook (node, trimportchook);

		if (!ipt) {
			ipt = opi_newimporttrace ();
			tnode_setchook (node, trimportchook, (void *)ipt);
		}
		dynarray_add (ipt->traces, string_dup (data));
#if 0
fprintf (stderr, "occampi_importmetadata_procdecl_tracetypeimpl(): got some traces metadata [%s]\n", data);
#endif
	} else {
		if (lops->next && tnode_haslangop (lops->next, "importmetadata")) {
			r = tnode_calllangop (lops->next, "importmetadata", 3, node, name, data);
		}
	}
	return r;
}
/*}}}*/


/*{{{  static void occampi_traces_attachtraces (dfastate_t *dfast, parsepriv_t *pp, void *rarg)*/
/*
 *	called to attach traces to a PROC declaration, will find a list of parameterised things or strings on the RHS,
 *	PROC declaration node is already in the result
 */
static void occampi_traces_attachtraces (dfastate_t *dfast, parsepriv_t *pp, void *rarg)
{
	tnode_t *rhs = dfa_popnode (dfast);
	tnode_t *node = *(dfast->ptr);

	if (!node || !rhs) {
		parser_error (pp->lf, "occampi_traces_attachtraces(): NULL rhs, node or tracesimplchook..");
		return;
	}

	tnode_setchook (node, trimplchook, (void *)rhs);
	return;
}
/*}}}*/


/*{{{  static int occampi_traces_init_nodes (void)*/
/*
 *	initialises TRACES nodes
 *	returns 0 on success, non-zero on failure
 */
static int occampi_traces_init_nodes (void)
{
	tndef_t *tnd;
	int i;
	compops_t *cops;
	langops_t *lops;

	/*{{{  register reduction functions*/
	fcnlib_addfcn ("occampi_traces_attachtraces", occampi_traces_attachtraces, 0, 3);

	/*}}}*/
	/*{{{  occampi:formalspec -- TRACES*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:formalspec", &i, 1, 0, 0, TNF_NONE);		/* subnodes: 0 = specification */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (occampi_scopein_traces));
	tnode_setcompop (cops, "scopeout", 2, COMPOPTYPE (occampi_scopeout_traces));
	tnd->ops = cops;

	i = -1;
	opi.tag_TRACES = tnode_newnodetag ("TRACES", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  occampi:traceimplspec -- TRACEIMPLSPEC*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:traceimplspec", &i, 2, 0, 0, TNF_NONE);
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "prescope", 2, COMPOPTYPE (occampi_prescope_traceimplspec));
	tnd->ops = cops;

	i = -1;
	opi.tag_TRACEIMPLSPEC = tnode_newnodetag ("TRACEIMPLSPEC", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  occampi:tracetypedecl -- TRACETYPEDECL*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:tracetypedecl", &i, 4, 0, 0, TNF_SHORTDECL);	/* subnodes: 0 = name, 1 = type/params, 2 = in-scope-body, 3 = traces */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "prescope", 2, COMPOPTYPE (occampi_prescope_tracetypedecl));
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (occampi_scopein_tracetypedecl));
	tnode_setcompop (cops, "scopeout", 2, COMPOPTYPE (occampi_scopeout_tracetypedecl));
	tnode_setcompop (cops, "precheck", 1, COMPOPTYPE (occampi_precheck_tracetypedecl));
	tnd->ops = cops;

	i = -1;
	opi.tag_TRACETYPEDECL = tnode_newnodetag ("TRACETYPEDECL", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  occampi:tracenamenode -- N_TRACETYPEDECL*/
	i = -1;
	tnd = opi.node_TRACENAMENODE = tnode_newnodetype ("occampi:tracenamenode", &i, 0, 1, 0, TNF_NONE);		/* subnames: name */
	cops = tnode_newcompops ();
	//tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (occampi_namemap_tracenamenode));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "getname", 2, LANGOPTYPE (occampi_getname_tracenamenode));
	tnode_setlangop (lops, "gettype", 2, LANGOPTYPE (occampi_gettype_tracenamenode));
	tnode_setlangop (lops, "traceslang_getparams", 1, LANGOPTYPE (occampi_traceslang_getparams_tracenamenode));
	tnode_setlangop (lops, "traceslang_getbody", 1, LANGOPTYPE (occampi_traceslang_getbody_tracenamenode));
	tnd->lops = lops;

	i = -1;
	opi.tag_NTRACETYPEDECL = tnode_newnodetag ("N_TRACETYPEDECL", &i, tnd, NTF_NONE);

	/*}}}*/

	return 0;
}
/*}}}*/
/*{{{  static int occampi_traces_post_setup (void)*/
/*
 *	does post-setup for TRACES nodes
 *	returns 0 on success, non-zero on failure
 */
static int occampi_traces_post_setup (void)
{
	langops_t *lops;
	compops_t *cops;
	tndef_t *tnd;

	/*{{{  occampi:trace chook setup*/
	opi.chook_traces = tnode_lookupornewchook ("occampi:trace");
	opi.chook_traces->chook_copy = occampi_chook_traces_copy;
	opi.chook_traces->chook_free = occampi_chook_traces_free;
	opi.chook_traces->chook_dumptree = occampi_chook_traces_dumptree;

	/*}}}*/
	/*{{{  occampi:importtrace chook setup*/
	trimportchook = tnode_lookupornewchook ("occampi:importtrace");
	trimportchook->chook_copy = occampi_chook_importtrace_copy;
	trimportchook->chook_free = occampi_chook_importtrace_free;
	trimportchook->chook_dumptree = occampi_chook_importtrace_dumptree;

	/*}}}*/
	/*{{{  find inparams scoping compiler operations*/
	inparams_scopein_compop = tnode_findcompop ("inparams_scopein");
	inparams_scopeout_compop = tnode_findcompop ("inparams_scopeout");

	/*}}}*/
	/*{{{  grab traces compiler hooks*/
	trimplchook = tracescheck_getimplchook ();
	trtracechook = tracescheck_gettraceschook ();
	trbvarschook = tracescheck_getbvarschook ();

	if (!trimplchook || !trtracechook || !trbvarschook) {
		nocc_internal ("occampi_traces_post_setup(): failed to find traces compiler hooks");
		return -1;
	}

	/*}}}*/
	/*{{{  intefere with PROC declaration nodes to capture/handle TRACES*/
	tnd = tnode_lookupnodetype ("occampi:procdecl");
	tnode_setcompop (tnd->ops, "inparams_scopein", 2, COMPOPTYPE (occampi_inparams_scopein_procdecl_tracetypeimpl));
	cops = tnode_insertcompops (tnd->ops);
	tnode_setcompop (cops, "prescope", 2, COMPOPTYPE (occampi_prescope_procdecl_tracetypeimpl));
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (occampi_typecheck_procdecl_tracetypeimpl));
	tnode_setcompop (cops, "typeresolve", 2, COMPOPTYPE (occampi_typeresolve_procdecl_tracetypeimpl));
	tnode_setcompop (cops, "precheck", 1, COMPOPTYPE (occampi_precheck_procdecl_tracetypeimpl));
	tnode_setcompop (cops, "tracescheck", 2, COMPOPTYPE (occampi_tracescheck_procdecl_tracetypeimpl));
	tnode_setcompop (cops, "fetrans", 2, COMPOPTYPE (occampi_fetrans_procdecl_tracetypeimpl));
	tnd->ops = cops;
	lops = tnode_insertlangops (tnd->lops);
	tnode_setlangop (lops, "importmetadata", 3, LANGOPTYPE (occampi_importmetadata_procdecl_tracetypeimpl));
	tnd->lops = lops;

	/*}}}*/
	/*{{{  tell the traceslang part of the compiler that N_TRACETYPEDECLs can represent traces*/
	traceslang_registertracetype (opi.tag_NTRACETYPEDECL);

	/*}}}*/

	return 0;
}
/*}}}*/



/*{{{  occampi_traces_feunit (feunit_t)*/
feunit_t occampi_traces_feunit = {
	init_nodes: occampi_traces_init_nodes,
	reg_reducers: NULL,
	init_dfatrans: NULL,
	post_setup: occampi_traces_post_setup,
	ident: "occampi-traces"
};
/*}}}*/

