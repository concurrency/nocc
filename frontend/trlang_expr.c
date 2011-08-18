/*
 *	trlang_expr.c -- tree-rewriting language expression handling
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
#include "fcnlib.h"
#include "dfa.h"
#include "parsepriv.h"
#include "trlang.h"
#include "trlang_fe.h"
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

/*{{{  private types*/
typedef struct TAG_trlang_lithook {
	char *data;
	int len;
} trlang_lithook_t;

/*}}}*/
/*{{{  private data*/

/*}}}*/


/*{{{  static void *trlang_nametoken_to_hook (void *ntok)*/
/*
 *	turns a name token into a hooknode for a tag_NAME
 */
static void *trlang_nametoken_to_hook (void *ntok)
{
	token_t *tok = (token_t *)ntok;
	char *rawname;

	rawname = tok->u.name;
	tok->u.name = NULL;

	lexer_freetoken (tok);

	return (void *)rawname;
}
/*}}}*/
/*{{{  static void *trlang_stringtoken_to_node (void *ntok)*/
/*
 *	turns a string token in to a LITSTR node
 */
static void *trlang_stringtoken_to_node (void *ntok)
{
	token_t *tok = (token_t *)ntok;
	tnode_t *node = NULL;
	trlang_lithook_t *litdata = (trlang_lithook_t *)smalloc (sizeof (trlang_lithook_t));

	if (tok->type != STRING) {
		lexer_error (tok->origin, "expected string, found [%s]", lexer_stokenstr (tok));
		sfree (litdata);
		lexer_freetoken (tok);
		return NULL;
	} 
	litdata->data = string_ndup (tok->u.str.ptr, tok->u.str.len);
	litdata->len = tok->u.str.len;

	node = tnode_create (trlang.tag_LITSTR, tok->origin, (void *)litdata);
	lexer_freetoken (tok);

	return (void *)node;
}
/*}}}*/
/*{{{  static void *trlang_integertoken_to_node (void *ntok)*/
/*
 *	turns an integer token into a LITINT node
 */
static void *trlang_integertoken_to_node (void *ntok)
{
	token_t *tok = (token_t *)ntok;
	tnode_t *node = NULL;
	trlang_lithook_t *litdata = (trlang_lithook_t *)smalloc (sizeof (trlang_lithook_t));

	if (tok->type != INTEGER) {
		lexer_error (tok->origin, "expected integer, found [%s]", lexer_stokenstr (tok));
		sfree (litdata);
		lexer_freetoken (tok);
		return NULL;
	} 
	litdata->data = mem_ndup (&(tok->u.ival), sizeof (int));
	litdata->len = 4;

	node = tnode_create (trlang.tag_LITINT, tok->origin, (void *)litdata);
	lexer_freetoken (tok);

	return (void *)node;
}
/*}}}*/


/*{{{  static void trlang_rawnamenode_hook_free (void *hook)*/
/*
 *	frees a rawnamenode hook (name-bytes)
 */
static void trlang_rawnamenode_hook_free (void *hook)
{
	if (hook) {
		sfree (hook);
	}
	return;
}
/*}}}*/
/*{{{  static void *trlang_rawnamenode_hook_copy (void *hook)*/
/*
 *	copies a rawnamenode hook (name-bytes)
 */
static void *trlang_rawnamenode_hook_copy (void *hook)
{
	char *rawname = (char *)hook;

	if (rawname) {
		return string_dup (rawname);
	}
	return NULL;
}
/*}}}*/
/*{{{  static void trlang_rawnamenode_hook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	dump-tree for rawnamenode hook (name-bytes)
 */
static void trlang_rawnamenode_hook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	trlang_isetindent (stream, indent);
	fprintf (stream, "<trlangrawnamenode value=\"%s\" />\n", hook ? (char *)hook : "(null)");
	return;
}
/*}}}*/

/*{{{  static void trlang_litnode_hook_free (void *hook)*/
/*
 *	frees a litnode hook
 */
static void trlang_litnode_hook_free (void *hook)
{
	trlang_lithook_t *ld = (trlang_lithook_t *)hook;

	if (ld) {
		if (ld->data) {
			sfree (ld->data);
		}
		sfree (ld);
	}

	return;
}
/*}}}*/
/*{{{  static void *trlang_litnode_hook_copy (void *hook)*/
/*
 *	copies a litnode hook (name-bytes)
 */
static void *trlang_litnode_hook_copy (void *hook)
{
	trlang_lithook_t *lit = (trlang_lithook_t *)hook;

	if (lit) {
		trlang_lithook_t *newlit = (trlang_lithook_t *)smalloc (sizeof (trlang_lithook_t));

		newlit->data = lit->data ? mem_ndup (lit->data, lit->len) : NULL;
		newlit->len = lit->len;

		return (void *)newlit;
	}
	return NULL;
}
/*}}}*/
/*{{{  static void trlang_litnode_hook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	dump-tree for litnode hook (name-bytes)
 */
static void trlang_litnode_hook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	trlang_lithook_t *lit = (trlang_lithook_t *)hook;

	trlang_isetindent (stream, indent);
	if (node->tag == trlang.tag_LITSTR) {
		fprintf (stream, "<trlanglitnode size=\"%d\" value=\"%s\" />\n", lit ? lit->len : 0, (lit && lit->data) ? lit->data : "(null)");
	} else {
		char *sdata = mkhexbuf ((unsigned char *)lit->data, lit->len);

		fprintf (stream, "<trlanglitnode size=\"%d\" value=\"%s\" />\n", lit ? lit->len : 0, sdata);
		sfree (sdata);
	}

	return;
}
/*}}}*/

/*{{{  static int trlang_scopein_rawname (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	scopes in a free-floating name
 */
static int trlang_scopein_rawname (compops_t *cops, tnode_t **node, scope_t *ss)
{
	tnode_t *name = *node;
	char *rawname;
	name_t *sname = NULL;

	if (name->tag != trlang.tag_NAME) {
		scope_error (name, ss, "name not raw-name!");
		return 0;
	}
	rawname = (char *)tnode_nthhookof (name, 0);

#if 0
fprintf (stderr, "trlang_scopein_rawname: here! rawname = \"%s\"\n", rawname);
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


/*{{{  static int trlang_expr_init_nodes (void)*/
/*
 *	initialises nodes for tree-rewriting language
 *	returns 0 on success, non-zero on failure
 */
static int trlang_expr_init_nodes (void)
{
	tndef_t *tnd;
	int i;
	compops_t *cops;

	/*{{{  register reduction functions*/
	fcnlib_addfcn ("trlang_nametoken_to_hook", (void *)trlang_nametoken_to_hook, 1, 1);
	fcnlib_addfcn ("trlang_stringtoken_to_node", (void *)trlang_stringtoken_to_node, 1, 1);
	fcnlib_addfcn ("trlang_integertoken_to_node", (void *)trlang_integertoken_to_node, 1, 1);

	/*}}}*/
	/*{{{  trlang:rawnamenode -- TRLANGNAME*/
	i = -1;
	tnd = tnode_newnodetype ("trlang:rawnamenode", &i, 0, 0, 1, TNF_NONE);				/* hooks: 0 = raw-name */
	tnd->hook_free = trlang_rawnamenode_hook_free;
	tnd->hook_copy = trlang_rawnamenode_hook_copy;
	tnd->hook_dumptree = trlang_rawnamenode_hook_dumptree;
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (trlang_scopein_rawname));
	tnd->ops = cops;

	i = -1;
	trlang.tag_NAME = tnode_newnodetag ("TRLANGNAME", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  trlang:litnode -- TRLANGLITSTR, TRLANGLITINT*/
	i = -1;
	tnd = tnode_newnodetype ("trlang:litnode", &i, 0, 0, 1, TNF_NONE);				/* hooks: 0 = trlang_lithook_t */
	tnd->hook_free = trlang_litnode_hook_free;
	tnd->hook_copy = trlang_litnode_hook_copy;
	tnd->hook_dumptree = trlang_litnode_hook_dumptree;
	cops = tnode_newcompops ();
	tnd->ops = cops;

	i = -1;
	trlang.tag_LITSTR = tnode_newnodetag ("TRLANGLITSTR", &i, tnd, NTF_NONE);
	i = -1;
	trlang.tag_LITINT = tnode_newnodetag ("TRLANGLITINT", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  trlang:functiondef -- TRLANGFUNCTIONDEF*/
	i = -1;
	tnd = tnode_newnodetype ("trlang:functiondef", &i, 3, 0, 0, TNF_NONE);				/* subnodes: 0 = name; 1 = params; 2 = body */
	cops = tnode_newcompops ();
	tnd->ops = cops;

	i = -1;
	trlang.tag_FUNCTIONDEF = tnode_newnodetag ("TRLANGFUNCTIONDEF", &i, tnd, NTF_NONE);

	/*}}}*/

	return 0;
}
/*}}}*/


/*{{{  trlang_expr_feunit (feunit_t)*/
feunit_t trlang_expr_feunit = {
	.init_nodes = trlang_expr_init_nodes,
	.reg_reducers = NULL,
	.init_dfatrans = NULL,
	.post_setup = NULL,
	.ident = "trlang-expr"
};


/*}}}*/

