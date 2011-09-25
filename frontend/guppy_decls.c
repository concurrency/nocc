/*
 *	guppy_decls.c -- variables and other named things
 *	Copyright (C) 2010 Fred Barnes <frmb@kent.ac.uk>
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
#include "map.h"
#include "target.h"
#include "transputer.h"
#include "codegen.h"
#include "langops.h"
#include "fetrans.h"
#include "betrans.h"
#include "metadata.h"
#include "tracescheck.h"
#include "mobilitycheck.h"



/*}}}*/
/*{{{  private types*/
#define FPARAM_IS_NONE 0
#define FPARAM_IS_VAL 1
#define FPARAM_IS_RES 2
#define FPARAM_IS_INIT 3

typedef struct TAG_fparaminfo {
	int flags;
} fparaminfo_t;

/*}}}*/


/*{{{  static fparaminfo_t *guppy_newfparaminfo (int flags)*/
/*
 *	creates a new fparaminfo_t structure
 */
static fparaminfo_t *guppy_newfparaminfo (int flags)
{
	fparaminfo_t *fpi = (fparaminfo_t *)smalloc (sizeof (fparaminfo_t));

	fpi->flags = flags;

	return fpi;
}
/*}}}*/
/*{{{  static void guppy_freefparaminfo (fparaminfo_t *fpi)*/
/*
 *	frees a fparaminfo_t structure
 */
static void guppy_freefparaminfo (fparaminfo_t *fpi)
{
	if (!fpi) {
		nocc_internal ("guppy_freefparaminfo(): NULL argument!");
		return;
	}
	sfree (fpi);
	return;
}
/*}}}*/


/*{{{  static void guppy_fparaminfo_hook_free (void *hook)*/
/*
 *	frees a fparaminfo_t hook
 */
static void guppy_fparaminfo_hook_free (void *hook)
{
	guppy_freefparaminfo ((fparaminfo_t *)hook);
}
/*}}}*/
/*{{{  static void *guppy_fparaminfo_hook_copy (void *hook)*/
/*
 *	copies a fparaminfo_t hook
 */
static void *guppy_fparaminfo_hook_copy (void *hook)
{
	fparaminfo_t *fpi = (fparaminfo_t *)hook;
	fparaminfo_t *nxt;

	if (!fpi) {
		return NULL;
	}
	nxt = guppy_newfparaminfo (fpi->flags);

	return (void *)nxt;
}
/*}}}*/
/*{{{  static void guppy_fparaminfo_hook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	dump-tree for a fparaminfo hook
 */
static void guppy_fparaminfo_hook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	guppy_isetindent (stream, indent);
	if (!hook) {
		fprintf (stream, "<fparaminfo value=\"(null)\" />\n");
	} else {
		fparaminfo_t *fpi = (fparaminfo_t *)hook;

		fprintf (stream, "<fparaminfo flags=\"%d\" />\n",fpi->flags);
	}
}
/*}}}*/


/*{{{  static void guppy_rawnamenode_hook_free (void *hook)*/
/*
 *	frees a rawnamenode hook (name-bytes)
 */
static void guppy_rawnamenode_hook_free (void *hook)
{
	if (hook) {
		sfree (hook);
	}
	return;
}
/*}}}*/
/*{{{  static void *guppy_rawnamenode_hook_copy (void *hook)*/
/*
 *	copies a rawnamenode hook (name-bytes)
 */
static void *guppy_rawnamenode_hook_copy (void *hook)
{
	char *rawname = (char *)hook;

	if (rawname) {
		return string_dup (rawname);
	}
	return NULL;
}
/*}}}*/
/*{{{  static void guppy_rawnamenode_hook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	dump-tree for rawnamenode hook (name-bytes)
 */
static void guppy_rawnamenode_hook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	guppy_isetindent (stream, indent);
	fprintf (stream, "<rawnamenode value=\"%s\" />\n", hook ? (char *)hook : "(null)");
	return;
}
/*}}}*/


/*{{{  static tnode_t *guppy_gettype_namenode (langops_t *lops, tnode_t *node, tnode_t *default_type)*/
/*
 *	returns the type of a name-node (trivial)
 */
static tnode_t *guppy_gettype_namenode (langops_t *lops, tnode_t *node, tnode_t *default_type)
{
	name_t *name = tnode_nthnameof (node, 0);

	if (!name) {
		nocc_fatal ("guppy_gettype_namenode(): NULL name!");
		return NULL;
	}
	if (name->type) {
		return name->type;
	}
	nocc_fatal ("guppy_gettype_namenode(): name has NULL type (FIXME!)");
	return NULL;
}
/*}}}*/
/*{{{  static int guppy_getname_namenode (langops_t *lops, tnode_t *node, char **str)*/
/*
 *	gets the name of a namenode (var/etc. name)
 *	returns 0 on success, -ve on failure
 */
static int guppy_getname_namenode (langops_t *lops, tnode_t *node, char **str)
{
	char *pname = NameNameOf (tnode_nthnameof (node, 0));

	if (*str) {
		sfree (*str);
	}
	*str = string_dup (pname);
	return 0;
}
/*}}}*/


/*{{{  static int guppy_prescope_vardecl (compops_t *cops, tnode_t **node, prescope_t *ps)*/
/*
 *	pre-scopes a variable declaration
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_prescope_vardecl (compops_t *cops, tnode_t **node, prescope_t *ps)
{
	return 1;
}
/*}}}*/
/*{{{  static int guppy_scopein_vardecl (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	scopes-in a variable declaration
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_scopein_vardecl (compops_t *cops, tnode_t **node, scope_t *ss)
{
	return 1;
}
/*}}}*/
/*{{{  static int guppy_scopeout_vardecl (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	scopes-out a variable declaration
 *	return value fairly meaningless (postwalk)
 */
static int guppy_scopeout_vardecl (compops_t *cops, tnode_t **node, scope_t *ss)
{
	return 1;
}
/*}}}*/


/*{{{  static int guppy_scopein_enumdef (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	scopes in an enumerated type definition
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_scopein_enumdef (compops_t *cops, tnode_t **node, scope_t *ss)
{
	tnode_t *name = tnode_nthsubof (*node, 0);
	tnode_t *elist = tnode_nthsubof (*node, 1);
	char *rawname;
	name_t *ename;
	tnode_t *newname, **items;
	int nitems, i;

	/* declare and scope enum name, check processes in scope */
	rawname = tnode_nthhookof (name, 0);

	ename = name_addscopename (rawname, *node, NULL, NULL);
	newname = tnode_createfrom (gup.tag_NENUM, name, ename);
	SetNameNode (ename, newname);
	tnode_setnthsub (*node, 0, newname);

	tnode_free (name);
	ss->scoped++;

	/* then declare and scope individual enumerated entries */
	items = parser_getlistitems (elist, &nitems);
	for (i=0; i<nitems; i++) {
		name_t *einame;
		tnode_t *eitype;
		tnode_t *enewname;

		if (items[i]->tag == gup.tag_NAME) {
			/* auto-assign value later */
			rawname = tnode_nthhookof (items[i], 0);
			einame = name_addscopename (rawname, *node, NULL, NULL);
			enewname = tnode_createfrom (gup.tag_NENUMVAL, items[i], einame);
			SetNameNode (einame, enewname);

			tnode_free (items[i]);
			items[i] = enewname;
			ss->scoped++;
		} else if (items[i]->tag == gup.tag_ASSIGN) {
			/* assign value now */
			rawname = tnode_nthhookof (tnode_nthsubof (items[i], 0), 0);
			eitype = NULL;		/* FIXME! */
			einame = name_addscopename (rawname, *node, eitype, NULL);
			enewname = tnode_createfrom (gup.tag_NENUMVAL, items[i], einame);
			SetNameNode (einame, enewname);

			tnode_free (items[i]);
			items[i] = enewname;
			ss->scoped++;
		} else {
			scope_error (items[i], ss, "unsupported structure type in enumerated list");
		}
	}

	return 0;
}
/*}}}*/
/*{{{  static int guppy_scopeout_enumdef (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	scopes-out an enumerated type definition
 *	return value meaningless (postwalk)
 */
static int guppy_scopeout_enumdef (compops_t *cops, tnode_t **node, scope_t *ss)
{
	return 1;
}
/*}}}*/


/*{{{  static int guppy_autoseq_declblock (compops_t *cops, tnode_t **node, guppy_autoseq_t *gas)*/
/*
 *	auto-sequence on a declaration block
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_autoseq_declblock (compops_t *cops, tnode_t **node, guppy_autoseq_t *gas)
{
	tnode_t **ilistptr = tnode_nthsubaddr (*node, 1);

#if 0
fprintf (stderr, "guppy_autoseq_declblock(): here!\n");
#endif
	if (parser_islistnode (*ilistptr)) {
		guppy_autoseq_listtoseqlist (ilistptr, gas);
	}
	return 1;
}
/*}}}*/
/*{{{  static int guppy_scopein_declblock (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	scope-in a declaration block
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_scopein_declblock (compops_t *cops, tnode_t **node, scope_t *ss)
{
	return 1;
}
/*}}}*/


/*{{{  static int guppy_decls_init_nodes (void)*/
/*
 *	sets up declaration and name nodes for Guppy
 *	returns 0 on success, non-zero on error
 */
static int guppy_decls_init_nodes (void)
{
	tndef_t *tnd;
	compops_t *cops;
	langops_t *lops;
	int i;

	/*{{{  guppy:rawnamenode -- NAME*/
	i = -1;
	tnd = tnode_newnodetype ("guppy:rawnamenode", &i, 0, 0, 1, TNF_NONE);				/* hooks: raw-name */
	tnd->hook_free = guppy_rawnamenode_hook_free;
	tnd->hook_copy = guppy_rawnamenode_hook_copy;
	tnd->hook_dumptree = guppy_rawnamenode_hook_dumptree;
	cops = tnode_newcompops ();
	/* FIXME: scope-in */
	tnd->ops = cops;

	i = -1;
	gup.tag_NAME = tnode_newnodetag ("NAME", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  guppy:namenode -- N_DECL, N_ABBR, N_VALABBR, N_RESABBR, N_PARAM, N_VALPARAM, N_RESPARAM, N_INITPARAM, N_REPL, N_TYPEDECL, N_FIELD, N_FCNDEF, N_ENUM, N_ENUMVAL*/
	i = -1;
	tnd = gup.node_NAMENODE = tnode_newnodetype ("guppy:namenode", &i, 0, 1, 0, TNF_NONE);		/* subnames: name */
	cops = tnode_newcompops ();
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "gettype", 2, LANGOPTYPE (guppy_gettype_namenode));
	tnode_setlangop (lops, "getname", 2, LANGOPTYPE (guppy_getname_namenode));
	tnd->lops = lops;

	i = -1;
	gup.tag_NDECL = tnode_newnodetag ("N_DECL", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_NABBR = tnode_newnodetag ("N_ABBR", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_NVALABBR = tnode_newnodetag ("N_VALABBR", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_NRESABBR = tnode_newnodetag ("N_RESABBR", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_NPARAM = tnode_newnodetag ("N_PARAM", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_NVALPARAM = tnode_newnodetag ("N_VALPARAM", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_NRESPARAM = tnode_newnodetag ("N_RESPARAM", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_NINITPARAM = tnode_newnodetag ("N_INITPARAM", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_NREPL = tnode_newnodetag ("N_REPL", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_NTYPEDECL = tnode_newnodetag ("N_TYPEDECL", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_NFIELD = tnode_newnodetag ("N_FIELD", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_NFCNDEF = tnode_newnodetag ("N_FCNDEF", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_NENUM = tnode_newnodetag ("N_ENUM", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_NENUMVAL = tnode_newnodetag ("N_ENUMVAL", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  guppy:vardecl -- VARDECL*/
	i = -1;
	tnd = tnode_newnodetype ("guppy:vardecl", &i, 2, 0, 0, TNF_NONE);				/* subnodes: name; type */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "prescope", 2, COMPOPTYPE (guppy_prescope_vardecl));
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (guppy_scopein_vardecl));
	tnode_setcompop (cops, "scopeout", 2, COMPOPTYPE (guppy_scopeout_vardecl));
	tnd->ops = cops;

	i = -1;
	gup.tag_VARDECL = tnode_newnodetag ("VARDECL", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  guppy:fparam -- FPARAM*/
	i = -1;
	tnd = tnode_newnodetype ("guppy:fparam", &i, 2, 0, 0, TNF_NONE);				/* subnodes: name; type, hooks: fparaminfo */
	cops = tnode_newcompops ();
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnd->lops = lops;

	i = -1;
	gup.tag_FPARAM = tnode_newnodetag ("FPARAM", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  guppy:declblock -- DECLBLOCK*/
	i = -1;
	tnd = tnode_newnodetype ("guppy:declblock", &i, 2, 0, 0, TNF_NONE);				/* subnodes: decls; process */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "autoseq", 2, COMPOPTYPE (guppy_autoseq_declblock));
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (guppy_scopein_declblock));
	tnd->ops = cops;

	i = -1;
	gup.tag_DECLBLOCK = tnode_newnodetag ("DECLBLOCK", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  guppy:enumdef -- ENUMDEF*/
	i = -1;
	tnd = tnode_newnodetype ("guppy:enumdef", &i, 2, 0, 0, TNF_LONGDECL);				/* subnodes: name; items */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (guppy_scopein_enumdef));
	tnode_setcompop (cops, "scopeout", 2, COMPOPTYPE (guppy_scopeout_enumdef));
	tnd->ops = cops;

	i = -1;
	gup.tag_ENUMDEF = tnode_newnodetag ("ENUMDEF", &i, tnd, NTF_INDENTED_NAME_LIST);

	/*}}}*/

	return 0;
}
/*}}}*/
/*{{{  static int guppy_decls_post_setup (void)*/
/*
 *	called to do any post-setup on declaration nodes
 *	returns 0 on success, non-zero on error
 */
static int guppy_decls_post_setup (void)
{
	return 0;
}
/*}}}*/


/*{{{  guppy_decls_feunit (feunit_t)*/
feunit_t guppy_decls_feunit = {
	.init_nodes = guppy_decls_init_nodes,
	.reg_reducers = NULL,
	.init_dfatrans = NULL,
	.post_setup = guppy_decls_post_setup,
	.ident = "guppy-decls"
};

/*}}}*/

