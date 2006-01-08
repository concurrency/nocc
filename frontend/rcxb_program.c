/*
 *	rcxb_program.c -- handling for BASIC style programs for the LEGO Mindstorms (tm) RCX
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
#include "rcxb.h"
#include "feunit.h"
#include "names.h"
#include "scope.h"
#include "prescope.h"
#include "typecheck.h"
#include "usagecheck.h"
#include "map.h"
#include "codegen.h"
#include "target.h"
#include "transputer.h"


/*}}}*/

/*{{{  private types/data*/


/*}}}*/


/*{{{  static void rcxb_rawnamenode_hook_free (void *hook)*/
/*
 *	frees a rawnamenode hook (name-bytes)
 */
static void rcxb_rawnamenode_hook_free (void *hook)
{
	if (hook) {
		sfree (hook);
	}
	return;
}
/*}}}*/
/*{{{  static void *rcxb_rawnamenode_hook_copy (void *hook)*/
/*
 *	copies a rawnamenode hook (name-bytes)
 */
static void *rcxb_rawnamenode_hook_copy (void *hook)
{
	char *rawname = (char *)hook;

	if (rawname) {
		return string_dup (rawname);
	}
	return NULL;
}
/*}}}*/
/*{{{  static void rcxb_rawnamenode_hook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	dump-tree for rawnamenode hook (name-bytes)
 */
static void rcxb_rawnamenode_hook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	rcxb_isetindent (stream, indent);
	fprintf (stream, "<rcxbrawnamenode value=\"%s\" />\n", hook ? (char *)hook : "(null)");
	return;
}
/*}}}*/

/*{{{  static int rcxb_scopein_rawname (tnode_t **node, scope_t *ss)*/
/*
 *	scopes in a free-floating name
 */
static int rcxb_scopein_rawname (tnode_t **node, scope_t *ss)
{
	tnode_t *name = *node;
	char *rawname;
	name_t *sname = NULL;

	if (name->tag != rcxb.tag_NAME) {
		scope_error (name, ss, "name not raw-name!");
		return 0;
	}
	rawname = tnode_nthhookof (name, 0);

#if 0
fprintf (stderr, "rcxb_scopein_rawname: here! rawname = \"%s\"\n", rawname);
#endif
	sname = name_lookupss (rawname, ss);
	if (sname) {
		/* resolved */
		*node = NameNodeOf (sname);
		tnode_free (name);
	} else {
		scope_error (name, ss, "unresolved name \"%s\"", rawname);
	}

	return 1;
}
/*}}}*/


/*{{{  static int rcxb_program_init_nodes (void)*/
/*
 *	initialises nodes for RCX-BASIC
 *	returns 0 on success, non-zero on failure
 */
static int rcxb_program_init_nodes (void)
{
	tndef_t *tnd;
	int i;
	compops_t *cops;

	/*{{{  rcxb:rawnamenode -- NAME*/
	i = -1;
	tnd = tnode_newnodetype ("rcxb:rawnamenode", &i, 0, 0, 1, TNF_NONE);				/* hooks: 0 = raw-name */
	tnd->hook_free = rcxb_rawnamenode_hook_free;
	tnd->hook_copy = rcxb_rawnamenode_hook_copy;
	tnd->hook_dumptree = rcxb_rawnamenode_hook_dumptree;
	cops = tnode_newcompops ();
	cops->scopein = rcxb_scopein_rawname;
	tnd->ops = cops;

	i = -1;
	rcxb.tag_NAME = tnode_newnodetag ("RCXBNAME", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  rcxb:actionnode -- SETMOTOR, SETSENSOR, SETPOWER, SETDIRECTION*/
	i = -1;
	tnd = rcxb.node_ACTIONNODE = tnode_newnodetype ("rcxb:actionnode", &i, 2, 0, 0, TNF_NONE);	/* subnodes: 0 = motor/sensor ID, 1 = setting */
	cops = tnode_newcompops ();
	tnd->ops = cops;

	i = -1;
	rcxb.tag_SETMOTOR = tnode_newnodetag ("RCXBSETMOTOR", &i, tnd, NTF_NONE);

	/*}}}*/

	return 0;
}
/*}}}*/
/*{{{  static int rcxb_program_reg_reducers (void)*/
/*
 *	registers reducers for RCX-BASIC
 *	returns 0 on success, non-zero on failure
 */
static int rcxb_program_reg_reducers (void)
{
	return 0;
}
/*}}}*/
/*{{{  dfattbl_t **rcxb_program_init_dfatrans (int *ntrans)*/
/*
 *	initialises and returns DFA transition tables for RCX-BASIC
 */
dfattbl_t **rcxb_program_init_dfatrans (int *ntrans)
{
	DYNARRAY (dfattbl_t *, transtbl);

	dynarray_init (transtbl);

	*ntrans = DA_CUR (transtbl);
	return DA_PTR (transtbl);
}
/*}}}*/


/*{{{  rcxb_program_feunit (feunit_t)*/
feunit_t rcxb_program_feunit = {
	init_nodes: rcxb_program_init_nodes,
	reg_reducers: rcxb_program_reg_reducers,
	init_dfatrans: rcxb_program_init_dfatrans,
	post_setup: NULL
};
/*}}}*/

