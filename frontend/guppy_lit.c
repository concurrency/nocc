/*
 *	guppy_lit.c -- literals for Guppy
 *	Copyright (C) 2011 Fred Barnes <frmb@kent.ac.uk>
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


/*{{{  void *guppy_token_to_lithook (void *ntok)*/
/*
 *	used to turn a token into a literal hook (int/real/char/string)
 */
void *guppy_token_to_lithook (void *ntok)
{
	token_t *tok = (token_t *)ntok;
	guppy_litdata_t *ldat = (guppy_litdata_t *)smalloc (sizeof (guppy_litdata_t));

	if (tok->type == INTEGER) {
		ldat->data = smalloc (sizeof (int));
		ldat->bytes = sizeof (int);
		ldat->littype = INTEGER;

		memcpy (ldat->data, &(tok->u.ival), sizeof (int));
	} else if (tok->type == REAL) {
		ldat->data = smalloc (sizeof (double));
		ldat->bytes = sizeof (double);
		ldat->littype = REAL;

		memcpy (ldat->data, &(tok->u.dval), sizeof (double));
	} else if (tok->type == STRING) {
		ldat->data = smalloc (tok->u.str.len + 1);
		ldat->bytes = tok->u.str.len;
		ldat->littype = STRING;

		memcpy (ldat->data, tok->u.str.ptr, tok->u.str.len);
		((char *)ldat->data)[ldat->bytes] = '\0';
	} else {
		nocc_serious ("guppy_token_to_lithook(): unsupported token type! %d", (int)tok->type);
		sfree (ldat);
		ldat = NULL;
	}

	return (void *)ldat;
}
/*}}}*/


/*{{{  static void guppy_litnode_hook_free (void *hook)*/
/*
 *	frees a litnode hook
 */
static void guppy_litnode_hook_free (void *hook)
{
	guppy_litdata_t *ldat = (guppy_litdata_t *)hook;

	if (!ldat) {
		return;
	}
	if (ldat->data) {
		sfree (ldat->data);
		ldat->data = NULL;
	}
	sfree (ldat);
	return;
}
/*}}}*/
/*{{{  static void *guppy_litnode_hook_copy (void *hook)*/
/*
 *	copies a litnode hook
 */
static void *guppy_litnode_hook_copy (void *hook)
{
	guppy_litdata_t *ldat = (guppy_litdata_t *)hook;
	guppy_litdata_t *lcpy = NULL;

	if (!ldat) {
		return NULL;
	}
	lcpy = (guppy_litdata_t *)smalloc (sizeof (guppy_litdata_t));
	switch (ldat->littype) {
	case INTEGER:
		lcpy->data = smalloc (sizeof (int));
		lcpy->bytes = sizeof (int);
		lcpy->littype = INTEGER;
		memcpy (lcpy->data, ldat->data, sizeof (int));
		break;
	case REAL:
		lcpy->data = smalloc (sizeof (double));
		lcpy->bytes = sizeof (double);
		lcpy->littype = REAL;
		memcpy (lcpy->data, ldat->data, sizeof (double));
		break;
	case STRING:
		lcpy->data = smalloc (ldat->bytes + 1);
		lcpy->bytes = ldat->bytes;
		lcpy->littype = STRING;
		memcpy (lcpy->data, ldat->data, ldat->bytes + 1);
		break;
	default:
		nocc_serious ("guppy_litnode_hook_copy(): unsupported literal type! %d", ldat->littype);
		break;
	}

	return (void *)lcpy;
}
/*}}}*/
/*{{{  static void guppy_litnode_hook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	dump-tree for a litnode hook
 */
static void guppy_litnode_hook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	guppy_litdata_t *ldat = (guppy_litdata_t *)hook;

	guppy_isetindent (stream, indent);
	fprintf (stream, "<litnodehook type=\"");
	fprintf (stream, "\" />\n");

	return;
}
/*}}}*/


/*{{{  static int guppy_lit_init_nodes (void)*/
/*
 *	sets up literal nodes for Guppy
 *	returns 0 on success, non-zero on failure
 */
static int guppy_lit_init_nodes (void)
{
	tndef_t *tnd;
	compops_t *cops;
	langops_t *lops;
	int i;

	fcnlib_addfcn ("guppy_token_to_lithook", (void *)guppy_token_to_lithook, 1, 1);

	/*{{{  guppy:litnode -- LITINT, LITREAL, LITCHAR, LITSTRING, LITBOOL*/
	i = -1;
	tnd = tnode_newnodetype ("guppy:litnode", &i, 1, 0, 1, TNF_NONE);		/* subnodes: actual-type; hooks: litdata_t */
	tnd->hook_free = guppy_litnode_hook_free;
	tnd->hook_copy = guppy_litnode_hook_copy;
	tnd->hook_dumptree = guppy_litnode_hook_dumptree;
	cops = tnode_newcompops ();
	tnd->ops = cops;

	i = -1;
	gup.tag_LITINT = tnode_newnodetag ("LITINT", &i, tnd, NTF_NONE);

	/*}}}*/

	return 0;
}
/*}}}*/
/*{{{  static int guppy_lit_post_setup (void)*/
/*
 *	does post-setup for literal nodes
 *	returns 0 on success, non-zero on failure
 */
static int guppy_lit_post_setup (void)
{
	return 0;
}
/*}}}*/


/*{{{  guppy_lit_feunit (feunit_t)*/
feunit_t guppy_lit_feunit = {
	.init_nodes = guppy_lit_init_nodes,
	.reg_reducers = NULL,
	.init_dfatrans = NULL,
	.post_setup = guppy_lit_post_setup,
	.ident = "guppy-lit"
};

/*}}}*/

