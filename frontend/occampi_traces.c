/*
 *	occampi_traces.c -- this deals with TRACES specifications
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
#include "traceslang.h"


/*}}}*/
/*{{{  private data*/

static compop_t *inparams_scopein_compop = NULL;
static compop_t *inparams_scopeout_compop = NULL;

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


/*{{{  static int occampi_prescope_procdecl_tracetypeimpl (compops_t *cops, tnode_t **node, prescope_t *ps)*/
/*
 *	does pre-scoping on a PROCDECL node to handle prescoping of traces
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_prescope_procdecl_tracetypeimpl (compops_t *cops, tnode_t **node, prescope_t *ps)
{
	chook_t *trimplchook = tracescheck_getimplchook ();
	int v = 1;

	if (tnode_hascompop (cops->next, "prescope")) {
		v = tnode_callcompop (cops->next, "prescope", 2, node, ps);
	}
	if (trimplchook) {
		tnode_t *trimpl = (tnode_t *)tnode_getchook (*node, trimplchook);

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
	chook_t *trimplchook = tracescheck_getimplchook ();
	int v = 1;

#if 0
fprintf (stderr, "occampi_inparams_scopein_procdecl_tracetypeimpl(): here!\n");
#endif
	/* scope the proc declaration proper first -- any traces implementation hook would be ignored  */
	if (tnode_hascompop (cops->next, "inparams_scopein")) {
		v = tnode_callcompop (cops->next, "inparams_scopein", 2, node, ss);
	}

	if (trimplchook) {
		tnode_t *trimpl = (tnode_t *)tnode_getchook (*node, trimplchook);

		if (trimpl) {
			/* got something here! */
			tnode_clearchook (*node, trimplchook);
			scope_subtree (&trimpl, ss);
			tnode_setchook (*node, trimplchook, trimpl);
		}
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
	chook_t *trimplchook = tracescheck_getimplchook ();
	int v = 1;

	if (tnode_hascompop (cops->next, "typecheck")) {
		v = tnode_callcompop (cops->next, "typecheck", 2, node, tc);
	}

	if (trimplchook) {
		tnode_t *trimpl = (tnode_t *)tnode_getchook (node, trimplchook);

		if (trimpl) {
			int nitems, i;
			tnode_t **items = parser_getlistitems (trimpl, &nitems);
#if 1
fprintf (stderr, "occampi_typecheck_procdecl_tracetypeimpl(): got traces implementation here:\n");
tnode_dumptree (trimpl, 1, stderr);
#endif

			for (i=0; i<nitems; i++) {
				tnode_t *trim = items[i];

				if (trim->tag == opi.tag_TRACEIMPLSPEC) {
					/*{{{  check that the LHS is a TRACETYPEDECL name, RHS is a parameter list*/
					/*}}}*/
				} else {
					typecheck_error (node, tc, "unsupported trace implementation type [%s]", trim->tag->name);
				}
			}
		}
	}

	return v;
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
	chook_t *trimplchook = tracescheck_getimplchook ();

	if (!trimplchook || !node || !rhs) {
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
	/*{{{  find inparams scoping compiler operations*/
	inparams_scopein_compop = tnode_findcompop ("inparams_scopein");
	inparams_scopeout_compop = tnode_findcompop ("inparams_scopeout");

	/*}}}*/
	/*{{{  intefere with PROC declaration nodes to capture/handle TRACES*/
	tnd = tnode_lookupnodetype ("occampi:procdecl");
	tnode_setcompop (tnd->ops, "inparams_scopein", 2, COMPOPTYPE (occampi_inparams_scopein_procdecl_tracetypeimpl));
	cops = tnode_insertcompops (tnd->ops);
	tnode_setcompop (cops, "prescope", 2, COMPOPTYPE (occampi_prescope_procdecl_tracetypeimpl));
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (occampi_typecheck_procdecl_tracetypeimpl));
	tnd->ops = cops;
	lops = tnode_insertlangops (tnd->lops);
	/* FIXME: langops */
	tnd->lops = lops;

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

