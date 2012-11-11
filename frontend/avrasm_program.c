/*
 *	avrasm_program.c -- handling for AVR assembler programs
 *	Copyright (C) 2012 Fred Barnes <frmb@kent.ac.uk>
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
#include "avrasm.h"
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
typedef struct TAG_avrasm_lithook {
	char *data;
	int len;
} avrasm_lithook_t;

/*}}}*/


/*{{{  static avrasm_lithook_t *new_avrasmlithook (void)*/
/*
 *	creates a new avrasm_lithook_t structure
 */
static avrasm_lithook_t *new_avrasmlithook (void)
{
	avrasm_lithook_t *litdata = (avrasm_lithook_t *)smalloc (sizeof (avrasm_lithook_t));

	litdata->data = NULL;
	litdata->len = 0;

	return litdata;
}
/*}}}*/
/*{{{  static void free_avrasmlithook (avrasm_lithook_t *lh)*/
/*
 *	frees an avrasm_lithook_t structure
 */
static void free_avrasmlithook (avrasm_lithook_t *lh)
{
	if (!lh) {
		nocc_warning ("free_avrasmlithook(): NULL pointer!");
		return;
	}
	if (lh->data && lh->len) {
		sfree (lh->data);
		lh->data = NULL;
		lh->len = 0;
	}
	sfree (lh);
	return;
}
/*}}}*/


/*{{{  static void *avrasm_nametoken_to_hook (void *ntok)*/
/*
 *	turns a name token into a hooknode for a tag_NAME
 */
static void *avrasm_nametoken_to_hook (void *ntok)
{
	token_t *tok = (token_t *)ntok;
	char *rawname;

	rawname = tok->u.name;
	tok->u.name = NULL;

	lexer_freetoken (tok);

	return (void *)rawname;
}
/*}}}*/
/*{{{  static void *avrasm_regtoken_to_node (void *ntok)*/
/*
 *	turns a token representing a register into a literal (LITREG)
 */
static void *avrasm_regtoken_to_node (void *ntok)
{
	token_t *tok = (token_t *)ntok;
	int r = -1;
	tnode_t *node = NULL;

	if (lexer_tokmatchlitstr (tok, "r0")) {
		r = 0;
	} else if (lexer_tokmatchlitstr (tok, "r1")) {
		r = 1;
	} else {
		lexer_error (tok->origin, "expected register identifier, found [%s]", lexer_stokenstr (tok));
	}
	if (r >= 0) {
		avrasm_lithook_t *lh = new_avrasmlithook ();

		lh->len = sizeof (r);
		lh->data = mem_ndup (&r, lh->len);

		node = tnode_create (avrasm.tag_LITREG, tok->origin, lh);
	}

	lexer_freetoken (tok);

	return (void *)node;
}
/*}}}*/
/*{{{  static void *avrasm_stringtoken_to_node (void *ntok)*/
/*
 *	turns a string token in to a LITSTR node
 */
static void *avrasm_stringtoken_to_node (void *ntok)
{
	token_t *tok = (token_t *)ntok;
	tnode_t *node = NULL;
	avrasm_lithook_t *litdata = new_avrasmlithook ();

	if (tok->type != STRING) {
		lexer_error (tok->origin, "expected string, found [%s]", lexer_stokenstr (tok));
		sfree (litdata);
		lexer_freetoken (tok);
		return NULL;
	} 
	litdata->data = string_ndup (tok->u.str.ptr, tok->u.str.len);
	litdata->len = tok->u.str.len;

	node = tnode_create (avrasm.tag_LITSTR, tok->origin, (void *)litdata);
	lexer_freetoken (tok);

	return (void *)node;
}
/*}}}*/
/*{{{  static void *avrasm_integertoken_to_node (void *ntok)*/
/*
 *	turns an integer token into a LITINT node
 */
static void *avrasm_integertoken_to_node (void *ntok)
{
	token_t *tok = (token_t *)ntok;
	tnode_t *node = NULL;
	avrasm_lithook_t *litdata = new_avrasmlithook ();

	if (tok->type != INTEGER) {
		lexer_error (tok->origin, "expected integer, found [%s]", lexer_stokenstr (tok));
		sfree (litdata);
		lexer_freetoken (tok);
		return NULL;
	} 
	litdata->len = sizeof (int);
	litdata->data = mem_ndup (&(tok->u.ival), litdata->len);

	node = tnode_create (avrasm.tag_LITINT, tok->origin, (void *)litdata);
	lexer_freetoken (tok);

	return (void *)node;
}
/*}}}*/


/*{{{  static void avrasm_rawnamenode_hook_free (void *hook)*/
/*
 *	frees a rawnamenode hook (name-bytes)
 */
static void avrasm_rawnamenode_hook_free (void *hook)
{
	if (hook) {
		sfree (hook);
	}
	return;
}
/*}}}*/
/*{{{  static void *avrasm_rawnamenode_hook_copy (void *hook)*/
/*
 *	copies a rawnamenode hook (name-bytes)
 */
static void *avrasm_rawnamenode_hook_copy (void *hook)
{
	char *rawname = (char *)hook;

	if (rawname) {
		return string_dup (rawname);
	}
	return NULL;
}
/*}}}*/
/*{{{  static void avrasm_rawnamenode_hook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	dump-tree for rawnamenode hook (name-bytes)
 */
static void avrasm_rawnamenode_hook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	avrasm_isetindent (stream, indent);
	fprintf (stream, "<avrasmrawnamenode value=\"%s\" />\n", hook ? (char *)hook : "(null)");
	return;
}
/*}}}*/

/*{{{  static void avrasm_litnode_hook_free (void *hook)*/
/*
 *	frees a litnode hook
 */
static void avrasm_litnode_hook_free (void *hook)
{
	avrasm_lithook_t *ld = (avrasm_lithook_t *)hook;

	if (ld) {
		free_avrasmlithook (ld);
	}

	return;
}
/*}}}*/
/*{{{  static void *avrasm_litnode_hook_copy (void *hook)*/
/*
 *	copies a litnode hook (name-bytes)
 */
static void *avrasm_litnode_hook_copy (void *hook)
{
	avrasm_lithook_t *lit = (avrasm_lithook_t *)hook;

	if (lit) {
		avrasm_lithook_t *newlit = new_avrasmlithook ();

		newlit->data = lit->data ? mem_ndup (lit->data, lit->len) : NULL;
		newlit->len = lit->len;

		return (void *)newlit;
	}
	return NULL;
}
/*}}}*/
/*{{{  static void avrasm_litnode_hook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	dump-tree for litnode hook (name-bytes)
 */
static void avrasm_litnode_hook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	avrasm_lithook_t *lit = (avrasm_lithook_t *)hook;

	avrasm_isetindent (stream, indent);
	if (node->tag == avrasm.tag_LITSTR) {
		fprintf (stream, "<avrasmlitnode size=\"%d\" value=\"%s\" />\n", lit ? lit->len : 0, (lit && lit->data) ? lit->data : "(null)");
	} else if (node->tag == avrasm.tag_LITREG) {
		fprintf (stderr, "<avrasmlitnode size=\"%d\" value=\"r%d\" />\n", lit ? lit->len : 0, (lit && lit->data) ? *(int *)(lit->data) : -1);
	} else {
		char *sdata = mkhexbuf ((unsigned char *)lit->data, lit->len);

		fprintf (stream, "<avrasmlitnode size=\"%d\" value=\"%s\" />\n", lit ? lit->len : 0, sdata);
		sfree (sdata);
	}

	return;
}
/*}}}*/


/*{{{  static int avrasm_scopein_rawname (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	scopes in a free-floating name
 *	returns 0 to stop walk, 1 to continue
 */
static int avrasm_scopein_rawname (compops_t *cops, tnode_t **node, scope_t *ss)
{
	tnode_t *name = *node;
	char *rawname;
	name_t *sname = NULL;

	if (name->tag != avrasm.tag_NAME) {
		scope_error (name, ss, "name not raw-name!");
		return 0;
	}
	rawname = tnode_nthhookof (name, 0);

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


/*{{{  static int avrasm_program_init_nodes (void)*/
/*
 *	initialises nodes for AVR assembler
 *	returns 0 on success, non-zero on failure
 */
static int avrasm_program_init_nodes (void)
{
	tndef_t *tnd;
	int i;
	compops_t *cops;

	/*{{{  register reduction functions*/
	fcnlib_addfcn ("avrasm_nametoken_to_hook", (void *)avrasm_nametoken_to_hook, 1, 1);
	fcnlib_addfcn ("avrasm_stringtoken_to_node", (void *)avrasm_stringtoken_to_node, 1, 1);
	fcnlib_addfcn ("avrasm_integertoken_to_node", (void *)avrasm_integertoken_to_node, 1, 1);
	fcnlib_addfcn ("avrasm_regtoken_to_node", (void *)avrasm_regtoken_to_node, 1, 1);

	/*}}}*/
	/*{{{  avrasm:rawnamenode -- NAME*/
	i = -1;
	tnd = tnode_newnodetype ("avrasm:rawnamenode", &i, 0, 0, 1, TNF_NONE);			/* hooks: 0 = raw-name */
	tnd->hook_free = avrasm_rawnamenode_hook_free;
	tnd->hook_copy = avrasm_rawnamenode_hook_copy;
	tnd->hook_dumptree = avrasm_rawnamenode_hook_dumptree;
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (avrasm_scopein_rawname));
	tnd->ops = cops;

	i = -1;
	avrasm.tag_NAME = tnode_newnodetag ("AVRASMNAME", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  avrasm:litnode -- LITSTR, LITINT, LITREG*/
	i = -1;
	tnd = tnode_newnodetype ("avrasm:litnode", &i, 0, 0, 1, TNF_NONE);			/* hooks: 0 = avrasm_lithook_t */
	tnd->hook_free = avrasm_litnode_hook_free;
	tnd->hook_copy = avrasm_litnode_hook_copy;
	tnd->hook_dumptree = avrasm_litnode_hook_dumptree;
	cops = tnode_newcompops ();
	tnd->ops = cops;

	i = -1;
	avrasm.tag_LITSTR = tnode_newnodetag ("AVRASMLITSTR", &i, tnd, NTF_NONE);
	i = -1;
	avrasm.tag_LITINT = tnode_newnodetag ("AVRASMLITINT", &i, tnd, NTF_NONE);
	i = -1;
	avrasm.tag_LITREG = tnode_newnodetag ("AVRASMLITREG", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  avrasm:orgnode -- ORG*/
	i = -1;
	tnd = tnode_newnodetype ("avrasm:dirnode", &i, 1, 0, 0, TNF_NONE);			/* subnodes: 0 = origin-expression */
	cops = tnode_newcompops ();
	tnd->ops = cops;

	i = -1;
	avrasm.tag_ORG = tnode_newnodetag ("AVRASMORG", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  avrasm:labdefnode -- GLABELDEF, LLABELDEF*/
	i = -1;
	tnd = tnode_newnodetype ("avrasm:labdefnode", &i, 1, 0, 0, TNF_NONE);			/* subnodes: 0 = label-name */
	cops = tnode_newcompops ();
	tnd->ops = cops;

	i = -1;
	avrasm.tag_GLABELDEF = tnode_newnodetag ("AVRASMGLABELDEF", &i, tnd, NTF_NONE);
	i = -1;
	avrasm.tag_LLABELDEF = tnode_newnodetag ("AVRASMLLABELDEF", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  avrasm:leafnode -- GLABEL, LLABEL*/
	/* Note: used as types */
	i = -1;
	tnd = tnode_newnodetype ("avrasm:glabel", &i, 0, 0, 0, TNF_NONE);
	cops = tnode_newcompops ();
	tnd->ops = cops;

	i = -1;
	avrasm.tag_GLABEL = tnode_newnodetag ("AVRASMGLABEL", &i, tnd, NTF_NONE);
	i = -1;
	avrasm.tag_LLABEL = tnode_newnodetag ("AVRASMLLABEL", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  avrasm:insnode -- [lots of things]*/
	i = -1;
	tnd = tnode_newnodetype ("avrasm:insnode", &i, 1, 0, 0, TNF_NONE);			/* subnodes: 0 = arguments */
	cops = tnode_newcompops ();
	tnd->ops = cops;

	i = -1;
	avrasm.tag_RJMP = tnode_newnodetag ("AVRASMRJMP", &i, tnd, NTF_NONE);

	/*}}}*/

	return 0;
}
/*}}}*/

/*{{{  avrasm_program_feunit (feunit_t)*/
feunit_t avrasm_program_feunit = {
	.init_nodes = avrasm_program_init_nodes,
	.reg_reducers = NULL,
	.init_dfatrans = NULL,
	.post_setup = NULL,
	.ident = "avrasm-program"
};

/*}}}*/

