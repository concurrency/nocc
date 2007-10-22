/*
 *	traceslang_expr.c -- traces language expression handling
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
#include "symbols.h"
#include "keywords.h"
#include "lexer.h"
#include "lexpriv.h"
#include "tnode.h"
#include "parser.h"
#include "fcnlib.h"
#include "dfa.h"
#include "parsepriv.h"
#include "traceslang.h"
#include "traceslang_fe.h"
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

typedef struct TAG_traceslang_lithook {
	char *data;
	int len;
} traceslang_lithook_t;

/*}}}*/
/*{{{  private data*/

/*}}}*/


/*{{{  static void *traceslang_nametoken_to_hook (void *ntok)*/
/*
 *	turns a name token into a hooknode for a tag_NAME
 */
static void *traceslang_nametoken_to_hook (void *ntok)
{
	token_t *tok = (token_t *)ntok;
	char *rawname;

	rawname = tok->u.name;
	tok->u.name = NULL;

	lexer_freetoken (tok);

	return (void *)rawname;
}
/*}}}*/
/*{{{  static void *traceslang_stringtoken_to_node (void *ntok)*/
/*
 *	turns a string token in to a LITSTR node
 */
static void *traceslang_stringtoken_to_node (void *ntok)
{
	token_t *tok = (token_t *)ntok;
	tnode_t *node = NULL;
	traceslang_lithook_t *litdata = (traceslang_lithook_t *)smalloc (sizeof (traceslang_lithook_t));

	if (tok->type != STRING) {
		lexer_error (tok->origin, "expected string, found [%s]", lexer_stokenstr (tok));
		sfree (litdata);
		lexer_freetoken (tok);
		return NULL;
	} 
	litdata->data = string_ndup (tok->u.str.ptr, tok->u.str.len);
	litdata->len = tok->u.str.len;

	node = tnode_create (traceslang.tag_LITSTR, tok->origin, (void *)litdata);
	lexer_freetoken (tok);

	return (void *)node;
}
/*}}}*/
/*{{{  static void *traceslang_integertoken_to_node (void *ntok)*/
/*
 *	turns an integer token into a LITINT node
 */
static void *traceslang_integertoken_to_node (void *ntok)
{
	token_t *tok = (token_t *)ntok;
	tnode_t *node = NULL;
	traceslang_lithook_t *litdata = (traceslang_lithook_t *)smalloc (sizeof (traceslang_lithook_t));

	if (tok->type != INTEGER) {
		lexer_error (tok->origin, "expected integer, found [%s]", lexer_stokenstr (tok));
		sfree (litdata);
		lexer_freetoken (tok);
		return NULL;
	} 
	litdata->data = mem_ndup (&(tok->u.ival), sizeof (int));
	litdata->len = 4;

	node = tnode_create (traceslang.tag_LITINT, tok->origin, (void *)litdata);
	lexer_freetoken (tok);

	return (void *)node;
}
/*}}}*/


/*{{{  static void traceslang_rawnamenode_hook_free (void *hook)*/
/*
 *	frees a rawnamenode hook (name-bytes)
 */
static void traceslang_rawnamenode_hook_free (void *hook)
{
	if (hook) {
		sfree (hook);
	}
	return;
}
/*}}}*/
/*{{{  static void *traceslang_rawnamenode_hook_copy (void *hook)*/
/*
 *	copies a rawnamenode hook (name-bytes)
 */
static void *traceslang_rawnamenode_hook_copy (void *hook)
{
	char *rawname = (char *)hook;

	if (rawname) {
		return string_dup (rawname);
	}
	return NULL;
}
/*}}}*/
/*{{{  static void traceslang_rawnamenode_hook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	dump-tree for rawnamenode hook (name-bytes)
 */
static void traceslang_rawnamenode_hook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	traceslang_isetindent (stream, indent);
	fprintf (stream, "<traceslangrawnamenode value=\"%s\" />\n", hook ? (char *)hook : "(null)");
	return;
}
/*}}}*/

/*{{{  static void traceslang_litnode_hook_free (void *hook)*/
/*
 *	frees a litnode hook
 */
static void traceslang_litnode_hook_free (void *hook)
{
	traceslang_lithook_t *ld = (traceslang_lithook_t *)hook;

	if (ld) {
		if (ld->data) {
			sfree (ld->data);
		}
		sfree (ld);
	}

	return;
}
/*}}}*/
/*{{{  static void *traceslang_litnode_hook_copy (void *hook)*/
/*
 *	copies a litnode hook (name-bytes)
 */
static void *traceslang_litnode_hook_copy (void *hook)
{
	traceslang_lithook_t *lit = (traceslang_lithook_t *)hook;

	if (lit) {
		traceslang_lithook_t *newlit = (traceslang_lithook_t *)smalloc (sizeof (traceslang_lithook_t));

		newlit->data = lit->data ? mem_ndup (lit->data, lit->len) : NULL;
		newlit->len = lit->len;

		return (void *)newlit;
	}
	return NULL;
}
/*}}}*/
/*{{{  static void traceslang_litnode_hook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	dump-tree for litnode hook (name-bytes)
 */
static void traceslang_litnode_hook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	traceslang_lithook_t *lit = (traceslang_lithook_t *)hook;

	traceslang_isetindent (stream, indent);
	if (node->tag == traceslang.tag_LITSTR) {
		fprintf (stream, "<traceslanglitnode size=\"%d\" value=\"%s\" />\n", lit ? lit->len : 0, (lit && lit->data) ? lit->data : "(null)");
	} else {
		char *sdata = mkhexbuf ((unsigned char *)lit->data, lit->len);

		fprintf (stream, "<traceslanglitnode size=\"%d\" value=\"%s\" />\n", lit ? lit->len : 0, sdata);
		sfree (sdata);
	}

	return;
}
/*}}}*/


/*{{{  tnode_t *traceslang_newevent (tnode_t *locn)*/
/*
 *	creates a new TRACESLANGEVENT (leaf) node
 *	returns node on success, NULL on failure
 */
tnode_t *traceslang_newevent (tnode_t *locn)
{
	tnode_t *enode = tnode_createfrom (traceslang.tag_EVENT, locn);

	return enode;
}
/*}}}*/
/*{{{  tnode_t *traceslang_newnparam (tnode_t *locn)*/
/*
 *	creates a new TRACESLANGNPARAM (name) node
 *	returns node on success, NULL on failure
 */
tnode_t *traceslang_newnparam (tnode_t *locn)
{
	tnode_t *nnode = tnode_createfrom (traceslang.tag_NPARAM, locn, NULL);

	return nnode;
}
/*}}}*/


/*{{{  static int traceslang_expr_init_nodes (void)*/
/*
 *	initialises nodes for traces language
 *	returns 0 on success, non-zero on failure
 */
static int traceslang_expr_init_nodes (void)
{
	tndef_t *tnd;
	int i;
	compops_t *cops;

	/*{{{  register reduction functions*/
	fcnlib_addfcn ("traceslang_nametoken_to_hook", (void *)traceslang_nametoken_to_hook, 1, 1);
	fcnlib_addfcn ("traceslang_stringtoken_to_node", (void *)traceslang_stringtoken_to_node, 1, 1);
	fcnlib_addfcn ("traceslang_integertoken_to_node", (void *)traceslang_integertoken_to_node, 1, 1);

	/*}}}*/
	/*{{{  traceslang:rawnamenode -- TRACESLANGNAME*/
	i = -1;
	tnd = tnode_newnodetype ("traceslang:rawnamenode", &i, 0, 0, 1, TNF_NONE);		/* hooks: 0 = raw-name */
	tnd->hook_free = traceslang_rawnamenode_hook_free;
	tnd->hook_copy = traceslang_rawnamenode_hook_copy;
	tnd->hook_dumptree = traceslang_rawnamenode_hook_dumptree;
	cops = tnode_newcompops ();
	// tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (traceslang_scopein_rawname));
	tnd->ops = cops;

	i = -1;
	traceslang.tag_NAME = tnode_newnodetag ("TRACESLANGNAME", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  traceslang:litnode -- TRACESLANGLITSTR, TRACESLANGLITINT*/
	i = -1;
	tnd = tnode_newnodetype ("traceslang:litnode", &i, 0, 0, 1, TNF_NONE);			/* hooks: 0 = traceslang_lithook_t */
	tnd->hook_free = traceslang_litnode_hook_free;
	tnd->hook_copy = traceslang_litnode_hook_copy;
	tnd->hook_dumptree = traceslang_litnode_hook_dumptree;
	cops = tnode_newcompops ();
	tnd->ops = cops;

	i = -1;
	traceslang.tag_LITSTR = tnode_newnodetag ("TRACESLANGLITSTR", &i, tnd, NTF_NONE);
	i = -1;
	traceslang.tag_LITINT = tnode_newnodetag ("TRACESLANGLITINT", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  traceslang:setnode -- TRACESLANGSEQ, TRACESLANGPAR, TRACESLANGDET, TRACESLANGNDET*/
	i = -1;
	tnd = tnode_newnodetype ("traceslang:setnode", &i, 1, 0, 0, TNF_NONE);			/* subnodes: 0 = list-of-items */
	cops = tnode_newcompops ();
	tnd->ops = cops;

	i = -1;
	traceslang.tag_SEQ = tnode_newnodetag ("TRACESLANGSEQ", &i, tnd, NTF_NONE);
	i = -1;
	traceslang.tag_PAR = tnode_newnodetag ("TRACESLANGPAR", &i, tnd, NTF_NONE);
	i = -1;
	traceslang.tag_DET = tnode_newnodetag ("TRACESLANGDET", &i, tnd, NTF_NONE);
	i = -1;
	traceslang.tag_NDET = tnode_newnodetag ("TRACESLANGNDET", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  traceslang:ionode -- TRACESLANGINPUT, TRACESLANGOUTPUT*/
	i = -1;
	tnd = tnode_newnodetype ("traceslang:ionode", &i, 1, 0, 0, TNF_NONE);			/* subnodes: 0 = item */
	cops = tnode_newcompops ();
	tnd->ops = cops;

	i = -1;
	traceslang.tag_INPUT = tnode_newnodetag ("TRACESLANGINPUT", &i, tnd, NTF_NONE);
	i = -1;
	traceslang.tag_OUTPUT = tnode_newnodetag ("TRACESLANGOUTPUT", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  traceslang:leafnode -- TRACESLANGEVENT*/
	i = -1;
	tnd = tnode_newnodetype ("traceslang:leafnode", &i, 0, 0, 0, TNF_NONE);
	cops = tnode_newcompops ();
	tnd->ops = cops;

	i = -1;
	traceslang.tag_EVENT = tnode_newnodetag ("TRACESLANGEVENT", &i, tnd, NTF_NONE);

	/*}}}*/

	/*{{{  traceslang:namenode -- TRACESLANGNPARAM*/
	i = -1;
	tnd = tnode_newnodetype ("traceslang:namenode", &i, 0, 1, 0, TNF_NONE);			/* subnames: 0 = name */
	cops = tnode_newcompops ();
	tnd->ops = cops;

	i = -1;
	traceslang.tag_NPARAM = tnode_newnodetag ("TRACESLANGNPARAM", &i, tnd, NTF_NONE);

	/*}}}*/

	return 0;
}
/*}}}*/


/*{{{  traceslang_expr_feinit (feunit_t)*/
feunit_t traceslang_expr_feunit = {
	init_nodes: traceslang_expr_init_nodes,
	reg_reducers: NULL,
	init_dfatrans: NULL,
	post_setup: NULL,
	ident: "traceslang-expr"
};

/*}}}*/


