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
#include "scope.h"
#include "prescope.h"
#include "library.h"
#include "typecheck.h"
#include "precheck.h"
#include "usagecheck.h"
#include "map.h"
#include "target.h"
#include "transputer.h"
#include "codegen.h"
#include "langops.h"
#include "fetrans.h"


/*}}}*/
/*{{{  private data*/
static chook_t *traceschook = NULL;


/*}}}*/


/*{{{  static void *occampi_traceschook_copy (void *chook)*/
/*
 *	copies an occampi:trace chook
 */
static void *occampi_traceschook_copy (void *chook)
{
	tnode_t *tree = (tnode_t *)chook;

	if (tree) {
		return (void *)tnode_copytree (tree);
	}
	return NULL;
}
/*}}}*/
/*{{{  static void occampi_traceschook_free (void *chook)*/
/*
 *	frees an occampi:trace chook
 */
static void occampi_traceschook_free (void *chook)
{
	tnode_t *tree = (tnode_t *)chook;

	if (tree) {
		tnode_free (tree);
	}
	return;
}
/*}}}*/
/*{{{  static void occampi_traceschook_dumptree (tnode_t *node, void *chook, int indent, FILE *stream)*/
/*
 *	dumps an occampi:trace chook (debugging)
 */
static void occampi_traceschook_dumptree (tnode_t *node, void *chook, int indent, FILE *stream)
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

	/*{{{  occampi:formalspec -- TRACES*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:formalspec", &i, 1, 0, 0, TNF_NONE);		/* subnodes: 0 = specification */
	cops = tnode_newcompops ();
	tnd->ops = cops;

	i = -1;
	opi.tag_TRACES = tnode_newnodetag ("TRACES", &i, tnd, NTF_NONE);
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
	traceschook = tnode_lookupornewchook ("occampi:trace");
	traceschook->chook_copy = occampi_traceschook_copy;
	traceschook->chook_free = occampi_traceschook_free;
	traceschook->chook_dumptree = occampi_traceschook_dumptree;

	return 0;
}
/*}}}*/



/*{{{  occampi_traces_feunit (feunit_t)*/
feunit_t occampi_traces_feunit = {
	init_nodes: occampi_traces_init_nodes,
	reg_reducers: NULL,
	init_dfatrans: NULL,
	post_setup: occampi_traces_post_setup
};
/*}}}*/

