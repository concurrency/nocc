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


/*{{{  static guppy_litdata_t *guppy_newlitdata (void)*/
/*
 *	creates a new guppy_litdata_t structure
 */
static guppy_litdata_t *guppy_newlitdata (void)
{
	guppy_litdata_t *ldat = (guppy_litdata_t *)smalloc (sizeof (guppy_litdata_t));

	ldat->data = NULL;
	ldat->bytes = 0;
	ldat->littype = NOTOKEN;

	return ldat;
}
/*}}}*/
/*{{{  static void guppy_freelitdata (guppy_litdata_t *ldat)*/
/*
 *	frees a guppy_litdata_t structure
 */
static void guppy_freelitdata (guppy_litdata_t *ldat)
{
	if (!ldat) {
		nocc_internal ("guppy_freelitdata(): NULL pointer!");
		return;
	}
	if (ldat->data) {
		sfree (ldat->data);
		ldat->data = NULL;
		ldat->bytes = 0;
	}
	sfree (ldat);
	return;
}
/*}}}*/


/*{{{  void *guppy_token_to_lithook (void *ntok)*/
/*
 *	used to turn a token into a literal hook (int/real/char/string)
 */
void *guppy_token_to_lithook (void *ntok)
{
	token_t *tok = (token_t *)ntok;
	guppy_litdata_t *ldat = guppy_newlitdata ();

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
		guppy_freelitdata (ldat);
		ldat = NULL;
	}

	return (void *)ldat;
}
/*}}}*/


/*{{{  tnode_t *guppy_makeintlit (tnode_t *type, tnode_t *org, const int value)*/
/*
 *	creates a new integer literal (as a node)
 */
tnode_t *guppy_makeintlit (tnode_t *type, tnode_t *org, const int value)
{
	guppy_litdata_t *ldat = guppy_newlitdata ();
	tnode_t *lnode;

	ldat->data = smalloc (sizeof (int));
	ldat->bytes = sizeof (int);
	memcpy (ldat->data, &value, sizeof (int));
	ldat->littype = INTEGER;

	if (!org) {
		lnode = tnode_create (gup.tag_LITINT, NULL, type, ldat);
	} else {
		lnode = tnode_createfrom (gup.tag_LITINT, org, type, ldat);
	}

	return lnode;
}
/*}}}*/
/*{{{  tnode_t *guppy_makereallit (tnode_t *type, tnode_t *org, const double value)*/
/*
 *	creates a new real literal (as a node)
 */
tnode_t *guppy_makereallit (tnode_t *type, tnode_t *org, const double value)
{
	guppy_litdata_t *ldat = guppy_newlitdata ();
	tnode_t *lnode;

	ldat->data = smalloc (sizeof (double));
	ldat->bytes = sizeof (double);
	memcpy (ldat->data, &value, sizeof (double));
	ldat->littype = REAL;

	if (!org) {
		lnode = tnode_create (gup.tag_LITINT, NULL, type, ldat);
	} else {
		lnode = tnode_createfrom (gup.tag_LITINT, org, type, ldat);
	}

	return lnode;
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
	guppy_freelitdata (ldat);
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
	char *hdat;

	guppy_isetindent (stream, indent);
	fprintf (stream, "<litnodehook type=\"");
	switch (ldat->littype) {
	case INTEGER:
		fprintf (stream, "integer");
		break;
	case REAL:
		fprintf (stream, "real");
		break;
	case STRING:
		fprintf (stream, "string");
		break;
	default:
		fprintf (stream, "<unknown %d>", ldat->littype);
		break;
	}
	fprintf (stream, "\" bytes=\"%d\" data=\"", ldat->bytes);
	hdat = mkhexbuf ((unsigned char *)ldat->data, ldat->bytes);
	fprintf (stream, "%s\" />\n", hdat);
	sfree (hdat);

	return;
}
/*}}}*/


/*{{{  static int guppy_codegen_litnode (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	does code-generation for a literal node
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_codegen_litnode (compops_t *cops, tnode_t *node, codegen_t *cgen)
{
	guppy_litdata_t *ldat = (guppy_litdata_t *)tnode_nthhookof (node, 0);

	switch (ldat->littype) {
	case INTEGER:
		switch (ldat->bytes) {
		case 4:
			codegen_write_fmt (cgen, "%d", *(int *)(ldat->data));
			break;
		case 2:
			codegen_write_fmt (cgen, "%d", *(short int *)(ldat->data));
			break;
		default:
			nocc_internal ("guppy_codegen_litnode(): unhandled INTEGER size %d!", ldat->bytes);
			break;
		}
		break;
	case REAL:
		switch (ldat->bytes) {
		case 4:
			codegen_write_fmt (cgen, "%f", *(float *)(ldat->data));
			break;
		case 8:
			codegen_write_fmt (cgen, "%lf", *(double *)(ldat->data));
			break;
		default:
			nocc_internal ("guppy_codegen_litnode(): unhandled REAL size %d!", ldat->bytes);
			break;
		}
		break;
	case STRING:
		codegen_write_fmt (cgen, "\"%*s\"", ldat->bytes, ldat->data);
		break;
	}
	return 0;
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
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (guppy_codegen_litnode));
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

