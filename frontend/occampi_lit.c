/*
 *	occampi_lit.c -- occam-pi literal processing for nocc
 *	Copyright (C) 2005 Fred Barnes <frmb@kent.ac.uk>
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
#include "typecheck.h"
#include "map.h"
#include "codegen.h"
#include "target.h"


/*}}}*/


/*{{{  static void occampi_litnode_hook_free (void *hook)*/
/*
 *	frees a litnode hook (value-bytes)
 */
static void occampi_litnode_hook_free (void *hook)
{
	occampi_litdata_t *ldata = (occampi_litdata_t *)hook;

	if (ldata) {
		sfree (ldata);
	}
	return;
}
/*}}}*/
/*{{{  static void *occampi_litnode_hook_copy (void *hook)*/
/*
 *	copies a litnode hook (name-bytes)
 */
static void *occampi_litnode_hook_copy (void *hook)
{
	occampi_litdata_t *ldata = (occampi_litdata_t *)hook;

	if (ldata) {
		occampi_litdata_t *tmplit = (occampi_litdata_t *)smalloc (sizeof (occampi_litdata_t));

		tmplit->bytes = ldata->bytes;
		tmplit->data = mem_ndup (ldata->data, ldata->bytes);

		return tmplit;
	}
	return NULL;
}
/*}}}*/
/*{{{  static void occampi_litnode_hook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	dump-tree for litnode hook (name-bytes)
 */
static void occampi_litnode_hook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	occampi_litdata_t *ldata = (occampi_litdata_t *)hook;
	char *hdata;

	if (ldata) {
		hdata = mkhexbuf ((unsigned char *)(ldata->data), ldata->bytes);
	} else {
		hdata = string_dup ("");
	}

	occampi_isetindent (stream, indent);
	fprintf (stream, "<litnode bytes=\"%d\" data=\"%s\" />\n", ldata ? ldata->bytes : 0, hdata);

	sfree (hdata);
	return;
}
/*}}}*/

/*{{{  static tnode_t *occampi_gettype_lit (tnode_t *node, tnode_t *default_type)*/
/*
 *	returns the type of a literal.
 *	If the type is not set, the default_type is used to guess it
 */
static tnode_t *occampi_gettype_lit (tnode_t *node, tnode_t *default_type)
{
	tnode_t *type = tnode_nthsubof (node, 0);

	if (!type && !default_type) {
		nocc_fatal ("occampi_gettype_lit(): literal not typed, and no default_type!");
	} else if (!type) {
		/* no type yet, use default_type */
		occampi_litdata_t *tmplit = (occampi_litdata_t *)tnode_nthhookof (node, 0);

		/* FIXME: make sure that the literal fits the type given */
		type = tnode_copytree (default_type);
		tnode_setnthsub (node, 0, type);
	}
	return type;
}
/*}}}*/
/*{{{  static int occampi_namemap_lit (tnode_t **node, map_t *map)*/
/*
 *	name-maps a literal
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_namemap_lit (tnode_t **node, map_t *map)
{
	occampi_litdata_t *ldata = (occampi_litdata_t *)tnode_nthhookof (*node, 0);
	tnode_t *cnst;

	cnst = map->target->newconst (*node, map, ldata->data, ldata->bytes);
	*node = cnst;

	return 0;
}
/*}}}*/


/*{{{  static int occampi_lit_init_nodes (void)*/
/*
 *	initialises literal-nodes for occam-pi
 *	return 0 on success, non-zero on failure
 */
static int occampi_lit_init_nodes (void)
{
	tndef_t *tnd;
	int i;
	compops_t *cops;

	/*{{{  occampi:litnode -- LITBYTE, LITINT, LITREAL*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:litnode", &i, 1, 0, 1, TNF_NONE);
	tnd->hook_free = occampi_litnode_hook_free;
	tnd->hook_copy = occampi_litnode_hook_copy;
	tnd->hook_dumptree = occampi_litnode_hook_dumptree;
	cops = tnode_newcompops ();
	cops->gettype = occampi_gettype_lit;
	cops->namemap = occampi_namemap_lit;
	tnd->ops = cops;

	i = -1;
	opi.tag_LITBYTE = tnode_newnodetag ("LITBYTE", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_LITINT = tnode_newnodetag ("LITINT", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_LITREAL = tnode_newnodetag ("LITREAL", &i, tnd, NTF_NONE);
	/*}}}*/

	return 0;
}
/*}}}*/
/*{{{  static int occampi_lit_reg_reducers (void)*/
/*
 *	registers reducers for literal nodes
 */
static int occampi_lit_reg_reducers (void)
{
	parser_register_grule ("opi:integerreduce", parser_decode_grule ("T+St0X0VC2R-", occampi_integertoken_to_hook, opi.tag_LITINT));
	parser_register_grule ("opi:realreduce", parser_decode_grule ("0T+St0XC2R-", occampi_realtoken_to_hook, opi.tag_LITREAL));

	return 0;
}
/*}}}*/
/*{{{  static dfattbl_t **occampi_lit_init_dfatrans (int *ntrans)*/
/*
 *	creates and returns DFA transition tables for literal nodes
 */
static dfattbl_t **occampi_lit_init_dfatrans (int *ntrans)
{
	DYNARRAY (dfattbl_t *, transtbl);

	dynarray_init (transtbl);
	/* FIXME: ... */

	*ntrans = DA_CUR (transtbl);
	return DA_PTR (transtbl);
}
/*}}}*/


/*{{{  occampi_lit_feunit (feunit_t)*/
feunit_t occampi_lit_feunit = {
	init_nodes: occampi_lit_init_nodes,
	reg_reducers: occampi_lit_reg_reducers,
	init_dfatrans: occampi_lit_init_dfatrans,
	post_setup: NULL
};
/*}}}*/

