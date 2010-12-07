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
	init_nodes: guppy_decls_init_nodes,
	reg_reducers: NULL,
	init_dfatrans: NULL,
	post_setup: guppy_decls_post_setup,
	ident: "guppy-decls"
};

/*}}}*/

