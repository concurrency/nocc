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
#include "fcnlib.h"
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
typedef struct TAG_rcxb_lithook {
	char *data;
	int len;
} rcxb_lithook_t;

typedef struct TAG_dopmap {
	tokentype_t ttype;
	const char *lookup;
	token_t *tok;
	ntdef_t **tagp;
	// transinstr_e instr;
} dopmap_t;

typedef struct TAG_mopmap {
	tokentype_t ttype;
	const char *lookup;
	token_t *tok;
	ntdef_t **tagp;
	// transinstr_e instr;
} mopmap_t;

typedef struct TAG_relmap {
	tokentype_t ttype;
	const char *lookup;
	token_t *tok;
	ntdef_t **tagp;
	// void (*cgfunc)(codegen_t *, int);
	// int cgarg;
} relmap_t;

/*}}}*/
/*{{{  private data*/
static dopmap_t dopmap[] = {
	{SYMBOL, "+", NULL, &(rcxb.tag_ADD)},
	{SYMBOL, "-", NULL, &(rcxb.tag_SUB)},
	{SYMBOL, "*", NULL, &(rcxb.tag_MUL)},
	{SYMBOL, "/", NULL, &(rcxb.tag_DIV)},
	{NOTOKEN, NULL, NULL, NULL}
};

static relmap_t relmap[] = {
	{SYMBOL, "=", NULL, &(rcxb.tag_RELEQ)},
	{SYMBOL, "<>", NULL, &(rcxb.tag_RELNEQ)},
	{SYMBOL, "<", NULL, &(rcxb.tag_RELLT)},
	{SYMBOL, ">=", NULL, &(rcxb.tag_RELGEQ)},
	{SYMBOL, ">", NULL, &(rcxb.tag_RELGT)},
	{SYMBOL, "<=", NULL, &(rcxb.tag_RELLEQ)},
	{NOTOKEN, NULL, NULL, NULL}
};

static mopmap_t mopmap[] = {
	{SYMBOL, "-", NULL, &(rcxb.tag_UMINUS)},
	{NOTOKEN, NULL, NULL, NULL}
};


/*}}}*/


/*{{{  static void *rcxb_nametoken_to_hook (void *ntok)*/
/*
 *	turns a name token into a hooknode for a tag_NAME
 */
static void *rcxb_nametoken_to_hook (void *ntok)
{
	token_t *tok = (token_t *)ntok;
	char *rawname;

	rawname = tok->u.name;
	tok->u.name = NULL;

	lexer_freetoken (tok);

	return (void *)rawname;
}
/*}}}*/
/*{{{  static void *rcxb_idtoken_to_node (void *ntok)*/
/*
 *	turns a token representing a sensor or motor into a leafnode
 */
static void *rcxb_idtoken_to_node (void *ntok)
{
	token_t *tok = (token_t *)ntok;
	ntdef_t *tag = NULL;
	tnode_t *node = NULL;

	if (tok->type == NAME) {
		if (lexer_tokmatchlitstr (tok, "A")) {
			tag = rcxb.tag_MOTORA;
		} else if (lexer_tokmatchlitstr (tok, "B")) {
			tag = rcxb.tag_MOTORB;
		} else if (lexer_tokmatchlitstr (tok, "C")) {
			tag = rcxb.tag_MOTORC;
		} else {
			lexer_error (tok->origin, "expected A, B or C, found [%s]", lexer_stokenstr (tok));
		}
	} else if (tok->type == INTEGER) {
		switch (tok->u.ival) {
		case 1:
			tag = rcxb.tag_SENSOR1;
			break;
		case 2:
			tag = rcxb.tag_SENSOR2;
			break;
		case 3:
			tag = rcxb.tag_SENSOR3;
			break;
		default:
			lexer_error (tok->origin, "expected 1, 2 or 3, found [%s]", lexer_stokenstr (tok));
			break;
		}
	} else {
		lexer_error (tok->origin, "expected motor or sensor identifier, found [%s]", lexer_stokenstr (tok));
	}
	if (tag) {
		node = tnode_create (tag, tok->origin);
	}
	lexer_freetoken (tok);

	return (void *)node;
}
/*}}}*/
/*{{{  static void *rcxb_dirtoken_to_node (void *ntok)*/
/*
 *	turns a token representing a direction into a leafnode
 */
static void *rcxb_dirtoken_to_node (void *ntok)
{
	token_t *tok = (token_t *)ntok;
	tnode_t *node = NULL;

	if (lexer_tokmatch (tok, rcxb.tok_FORWARD)) {
		node = tnode_create (rcxb.tag_FORWARD, tok->origin);
	} else if (lexer_tokmatch (tok, rcxb.tok_REVERSE)) {
		node = tnode_create (rcxb.tag_REVERSE, tok->origin);
	} else {
		lexer_error (tok->origin, "expected forward or reverse, found [%s]", lexer_stokenstr (tok));
	}

	return (void *)node;
}
/*}}}*/
/*{{{  static void *rcxb_stringtoken_to_node (void *ntok)*/
/*
 *	turns a string token in to a LITSTR node
 */
static void *rcxb_stringtoken_to_node (void *ntok)
{
	token_t *tok = (token_t *)ntok;
	tnode_t *node = NULL;
	rcxb_lithook_t *litdata = (rcxb_lithook_t *)smalloc (sizeof (rcxb_lithook_t));

	if (tok->type != STRING) {
		lexer_error (tok->origin, "expected string, found [%s]", lexer_stokenstr (tok));
		sfree (litdata);
		lexer_freetoken (tok);
		return NULL;
	} 
	litdata->data = string_ndup (tok->u.str.ptr, tok->u.str.len);
	litdata->len = tok->u.str.len;

	node = tnode_create (rcxb.tag_LITSTR, tok->origin, (void *)litdata);
	lexer_freetoken (tok);

	return (void *)node;
}
/*}}}*/
/*{{{  static void *rcxb_integertoken_to_node (void *ntok)*/
/*
 *	turns an integer token into a LITINT node
 */
static void *rcxb_integertoken_to_node (void *ntok)
{
	token_t *tok = (token_t *)ntok;
	tnode_t *node = NULL;
	rcxb_lithook_t *litdata = (rcxb_lithook_t *)smalloc (sizeof (rcxb_lithook_t));

	if (tok->type != INTEGER) {
		lexer_error (tok->origin, "expected integer, found [%s]", lexer_stokenstr (tok));
		sfree (litdata);
		lexer_freetoken (tok);
		return NULL;
	} 
	litdata->data = mem_ndup (&(tok->u.ival), sizeof (int));
	litdata->len = 4;

	node = tnode_create (rcxb.tag_LITINT, tok->origin, (void *)litdata);
	lexer_freetoken (tok);

	return (void *)node;
}
/*}}}*/


/*{{{  static void rcxb_reduce_dop (dfastate_t *dfast, parsepriv_t *pp, void *rarg)*/
/*
 *	reduces a dyadic operator, expects 2 nodes on the node-stack,
 *	and the relevant operator token on the token-stack
 */
static void rcxb_reduce_dop (dfastate_t *dfast, parsepriv_t *pp, void *rarg)
{
	token_t *tok = parser_gettok (pp);
	tnode_t *lhs, *rhs;
	ntdef_t *tag = NULL;
	int i;

	if (!tok) {
		parser_error (pp->lf, "rcxb_reduce_dop(): no token ?");
		return;
	}
	rhs = dfa_popnode (dfast);
	lhs = dfa_popnode (dfast);
	if (!rhs || !lhs) {
		parser_error (pp->lf, "rcxb_reduce_dop(): lhs=0x%8.8x, rhs=0x%8.8x", (unsigned int)lhs, (unsigned int)rhs);
		return;
	}
	for (i=0; dopmap[i].lookup; i++) {
		if (lexer_tokmatch (dopmap[i].tok, tok)) {
			tag = *(dopmap[i].tagp);
			break;
		}
	}
	if (!tag) {
		parser_error (pp->lf, "rcxb_reduce_dop(): unhandled token [%s]", lexer_stokenstr (tok));
		return;
	}
	*(dfast->ptr) = tnode_create (tag, pp->lf, lhs, rhs);

	return;
}
/*}}}*/
/*{{{  static void rcxb_reduce_rel (dfastate_t *dfast, parsepriv_t *pp, void *rarg)*/
/*
 *	reduces a relational operator, expects 2 nodes on the node-stack,
 *	and the relevant operator token on the token-stack
 */
static void rcxb_reduce_rel (dfastate_t *dfast, parsepriv_t *pp, void *rarg)
{
	token_t *tok = parser_gettok (pp);
	tnode_t *lhs, *rhs;
	ntdef_t *tag = NULL;
	int i;

	if (!tok) {
		parser_error (pp->lf, "rcxb_reduce_rel(): no token ?");
		return;
	}
	rhs = dfa_popnode (dfast);
	lhs = dfa_popnode (dfast);
	if (!rhs || !lhs) {
		parser_error (pp->lf, "rcxb_reduce_rel(): lhs=0x%8.8x, rhs=0x%8.8x", (unsigned int)lhs, (unsigned int)rhs);
		return;
	}
	for (i=0; relmap[i].lookup; i++) {
		if (lexer_tokmatch (relmap[i].tok, tok)) {
			tag = *(relmap[i].tagp);
			break;
		}
	}
#if 0
if (!tag) {
lexer_dumptoken (stderr, tok);
for (i=0; relmap[i].lookup; i++) {
	fprintf (stderr, "--> tagname [%s], token: ", *(relmap[i].tagp) ? (*(relmap[i].tagp))->name : "(null)");
	lexer_dumptoken (stderr, relmap[i].tok);
}
}
#endif
	if (!tag) {
		parser_error (pp->lf, "rcxb_reduce_rel(): unhandled token [%s]", lexer_stokenstr (tok));
		return;
	}
	*(dfast->ptr) = tnode_create (tag, pp->lf, lhs, rhs);

	return;
}
/*}}}*/
/*{{{  static void rcxb_reduce_mop (dfastate_t *dfast, parsepriv_t *pp, void *rarg)*/
/*
 *	reduces a monadic operator, expects operand on the node-stack, operator on
 *	the token-stack.
 */
static void rcxb_reduce_mop (dfastate_t *dfast, parsepriv_t *pp, void *rarg)
{
	token_t *tok = parser_gettok (pp);
	tnode_t *operand;
	ntdef_t *tag = NULL;
	int i;

	if (!tok) {
		parser_error (pp->lf, "rcxb_reduce_mop(): no token ?");
		return;
	}
	operand = dfa_popnode (dfast);
	if (!operand) {
		parser_error (pp->lf, "rcxb_reduce_mop(): operand=0x%8.8x", (unsigned int)operand);
		return;
	}
	for (i=0; mopmap[i].lookup; i++) {
		if (lexer_tokmatch (mopmap[i].tok, tok)) {
			tag = *(mopmap[i].tagp);
			break;
		}
	}
	if (!tag) {
		parser_error (pp->lf, "rcxb_reduce_mop(): unhandled token [%s]", lexer_stokenstr (tok));
		return;
	}
	*(dfast->ptr) = tnode_create (tag, pp->lf, operand);

	return;
}
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

/*{{{  static void rcxb_litnode_hook_free (void *hook)*/
/*
 *	frees a litnode hook
 */
static void rcxb_litnode_hook_free (void *hook)
{
	rcxb_lithook_t *ld = (rcxb_lithook_t *)hook;

	if (ld) {
		if (ld->data) {
			sfree (ld->data);
		}
		sfree (ld);
	}

	return;
}
/*}}}*/
/*{{{  static void *rcxb_litnode_hook_copy (void *hook)*/
/*
 *	copies a litnode hook (name-bytes)
 */
static void *rcxb_litnode_hook_copy (void *hook)
{
	rcxb_lithook_t *lit = (rcxb_lithook_t *)hook;

	if (lit) {
		rcxb_lithook_t *newlit = (rcxb_lithook_t *)smalloc (sizeof (rcxb_lithook_t));

		newlit->data = lit->data ? mem_ndup (lit->data, lit->len) : NULL;
		newlit->len = lit->len;

		return (void *)newlit;
	}
	return NULL;
}
/*}}}*/
/*{{{  static void rcxb_litnode_hook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	dump-tree for litnode hook (name-bytes)
 */
static void rcxb_litnode_hook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	rcxb_lithook_t *lit = (rcxb_lithook_t *)hook;

	rcxb_isetindent (stream, indent);
	if (node->tag == rcxb.tag_LITSTR) {
		fprintf (stream, "<rcxblitnode size=\"%d\" value=\"%s\" />\n", lit ? lit->len : 0, (lit && lit->data) ? lit->data : "(null)");
	} else {
		char *sdata = mkhexbuf ((unsigned char *)lit->data, lit->len);

		fprintf (stream, "<rcxblitnode size=\"%d\" value=\"%s\" />\n", lit ? lit->len : 0, sdata);
		sfree (sdata);
	}

	return;
}
/*}}}*/

/*{{{  static int rcxb_scopein_rawname (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	scopes in a free-floating name
 */
static int rcxb_scopein_rawname (compops_t *cops, tnode_t **node, scope_t *ss)
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

	/*{{{  register reduction functions*/
	fcnlib_addfcn ("rcxb_nametoken_to_hook", (void *)rcxb_nametoken_to_hook, 1, 1);
	fcnlib_addfcn ("rcxb_stringtoken_to_node", (void *)rcxb_stringtoken_to_node, 1, 1);
	fcnlib_addfcn ("rcxb_integertoken_to_node", (void *)rcxb_integertoken_to_node, 1, 1);
	fcnlib_addfcn ("rcxb_idtoken_to_node", (void *)rcxb_idtoken_to_node, 1, 1);
	fcnlib_addfcn ("rcxb_dirtoken_to_node", (void *)rcxb_dirtoken_to_node, 1, 1);

	fcnlib_addfcn ("rcxb_reduce_mop", (void *)rcxb_reduce_mop, 0, 3);
	fcnlib_addfcn ("rcxb_reduce_dop", (void *)rcxb_reduce_dop, 0, 3);
	fcnlib_addfcn ("rcxb_reduce_rel", (void *)rcxb_reduce_rel, 0, 3);

	/*}}}*/
	/*{{{  rcxb:rawnamenode -- NAME*/
	i = -1;
	tnd = tnode_newnodetype ("rcxb:rawnamenode", &i, 0, 0, 1, TNF_NONE);				/* hooks: 0 = raw-name */
	tnd->hook_free = rcxb_rawnamenode_hook_free;
	tnd->hook_copy = rcxb_rawnamenode_hook_copy;
	tnd->hook_dumptree = rcxb_rawnamenode_hook_dumptree;
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (rcxb_scopein_rawname));
	tnd->ops = cops;

	i = -1;
	rcxb.tag_NAME = tnode_newnodetag ("RCXBNAME", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  rcxb:leafnode -- MOTORA, MOTORB, MOTORC, SENSOR1, SENSOR2, SENSOR3, OFF, FORWARD, REVERSE*/
	i = -1;
	tnd = rcxb.node_LEAFNODE = tnode_newnodetype ("rcxb:leafnode", &i, 0, 0, 0, TNF_NONE);
	cops = tnode_newcompops ();
	tnd->ops = cops;

	i = -1;
	rcxb.tag_MOTORA = tnode_newnodetag ("RCXBMOTORA", &i, tnd, NTF_NONE);
	i = -1;
	rcxb.tag_MOTORB = tnode_newnodetag ("RCXBMOTORB", &i, tnd, NTF_NONE);
	i = -1;
	rcxb.tag_MOTORC = tnode_newnodetag ("RCXBMOTORC", &i, tnd, NTF_NONE);
	i = -1;
	rcxb.tag_SENSOR1 = tnode_newnodetag ("RCXBSENSOR1", &i, tnd, NTF_NONE);
	i = -1;
	rcxb.tag_SENSOR2 = tnode_newnodetag ("RCXBSENSOR2", &i, tnd, NTF_NONE);
	i = -1;
	rcxb.tag_SENSOR3 = tnode_newnodetag ("RCXBSENSOR3", &i, tnd, NTF_NONE);
	i = -1;
	rcxb.tag_SENSOR3 = tnode_newnodetag ("RCXBSENSOR3", &i, tnd, NTF_NONE);
	i = -1;
	rcxb.tag_OFF = tnode_newnodetag ("RCXBOFF", &i, tnd, NTF_NONE);
	i = -1;
	rcxb.tag_FORWARD = tnode_newnodetag ("RCXBFORWARD", &i, tnd, NTF_NONE);
	i = -1;
	rcxb.tag_REVERSE = tnode_newnodetag ("RCXBREVERSE", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  rcxb:opernode -- GOTO, SETLABEL, SLEEP, SOUND*/
	i = -1;
	tnd = rcxb.node_OPERNODE = tnode_newnodetype ("rcxb:opernode", &i, 1, 0, 0, TNF_NONE);		/* subnodes: 0 = argument */
	cops = tnode_newcompops ();
	tnd->ops = cops;

	i = -1;
	rcxb.tag_GOTO = tnode_newnodetag ("RCXBGOTO", &i, tnd, NTF_NONE);
	i = -1;
	rcxb.tag_SETLABEL = tnode_newnodetag ("RCXBSETLABEL", &i, tnd, NTF_NONE);
	i = -1;
	rcxb.tag_SLEEP = tnode_newnodetag ("RCXBSLEEP", &i, tnd, NTF_NONE);
	i = -1;
	rcxb.tag_SOUND = tnode_newnodetag ("RCXBSOUND", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  rcxb:actionnode -- SETMOTOR, SETSENSOR, SETPOWER, SETDIRECTION, ASSIGN*/
	i = -1;
	tnd = rcxb.node_ACTIONNODE = tnode_newnodetype ("rcxb:actionnode", &i, 2, 0, 0, TNF_NONE);	/* subnodes: 0 = motor/sensor ID, 1 = setting */
	cops = tnode_newcompops ();
	tnd->ops = cops;

	i = -1;
	rcxb.tag_SETMOTOR = tnode_newnodetag ("RCXBSETMOTOR", &i, tnd, NTF_NONE);
	i = -1;
	rcxb.tag_SETSENSOR = tnode_newnodetag ("RCXBSETSENSOR", &i, tnd, NTF_NONE);
	i = -1;
	rcxb.tag_SETPOWER = tnode_newnodetag ("RCXBSETPOWER", &i, tnd, NTF_NONE);
	i = -1;
	rcxb.tag_SETDIRECTION = tnode_newnodetag ("RCXBSETDIRECTION", &i, tnd, NTF_NONE);
	i = -1;
	rcxb.tag_ASSIGN = tnode_newnodetag ("RCXBASSIGN", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  rcxb:mopnode -- UMINUS*/
	i = -1;
	tnd = rcxb.node_MOPNODE = tnode_newnodetype ("rcxb:mopnode", &i, 1, 0, 0, TNF_NONE);	/* subnodes: 0 = sub-expression */
	cops = tnode_newcompops ();
	tnd->ops = cops;

	i = -1;
	rcxb.tag_UMINUS = tnode_newnodetag ("RCXBUMINUS", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  rcxb:dopnode -- ADD, SUB, MUL, DIV*/
	i = -1;
	tnd = rcxb.node_DOPNODE = tnode_newnodetype ("rcxb:dopnode", &i, 2, 0, 0, TNF_NONE);	/* subnodes: 0 = left, 1 = right */
	cops = tnode_newcompops ();
	tnd->ops = cops;

	i = -1;
	rcxb.tag_ADD = tnode_newnodetag ("RCXBADD", &i, tnd, NTF_NONE);
	i = -1;
	rcxb.tag_SUB = tnode_newnodetag ("RCXBSUB", &i, tnd, NTF_NONE);
	i = -1;
	rcxb.tag_MUL = tnode_newnodetag ("RCXBMUL", &i, tnd, NTF_NONE);
	i = -1;
	rcxb.tag_DIV = tnode_newnodetag ("RCXBDIV", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  rcxb:relnode -- RELEQ, RELNEQ, RELLT, RELGEQ, RELGT, RELLEQ*/
	i = -1;
	tnd = rcxb.node_RELNODE = tnode_newnodetype ("rcxb:relnode", &i, 2, 0, 0, TNF_NONE);	/* subnodes: 0 = left, 1 = right */
	cops = tnode_newcompops ();
	tnd->ops = cops;

	i = -1;
	rcxb.tag_RELEQ = tnode_newnodetag ("RCXBRELEQ", &i, tnd, NTF_NONE);
	i = -1;
	rcxb.tag_RELNEQ = tnode_newnodetag ("RCXBRELNEQ", &i, tnd, NTF_NONE);
	i = -1;
	rcxb.tag_RELLT = tnode_newnodetag ("RCXBRELLT", &i, tnd, NTF_NONE);
	i = -1;
	rcxb.tag_RELGEQ = tnode_newnodetag ("RCXBRELGEQ", &i, tnd, NTF_NONE);
	i = -1;
	rcxb.tag_RELGT = tnode_newnodetag ("RCXBRELGT", &i, tnd, NTF_NONE);
	i = -1;
	rcxb.tag_RELLEQ = tnode_newnodetag ("RCXBRELLEQ", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  rcxb:litnode -- LITSTR, LITINT*/
	i = -1;
	tnd = tnode_newnodetype ("rcxb:litnode", &i, 0, 0, 1, TNF_NONE);				/* hooks: 0 = rcxb_lithook_t */
	tnd->hook_free = rcxb_litnode_hook_free;
	tnd->hook_copy = rcxb_litnode_hook_copy;
	tnd->hook_dumptree = rcxb_litnode_hook_dumptree;
	cops = tnode_newcompops ();
	tnd->ops = cops;

	i = -1;
	rcxb.tag_LITSTR = tnode_newnodetag ("RCXBLITSTR", &i, tnd, NTF_NONE);
	i = -1;
	rcxb.tag_LITINT = tnode_newnodetag ("RCXBLITINT", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  rcxb:loopnode -- FOR, NEXT, WHILE*/
	i = -1;
	tnd = rcxb.node_LOOPNODE = tnode_newnodetype ("rcxb:loopnode", &i, 4, 0, 0, TNF_NONE);		/* subnodes: 0 = variable/condition, 1 = start, 2 = stop, 3 = step */
	cops = tnode_newcompops ();
	tnd->ops = cops;

	i = -1;
	rcxb.tag_FOR = tnode_newnodetag ("RCXBFOR", &i, tnd, NTF_NONE);
	i = -1;
	rcxb.tag_NEXT = tnode_newnodetag ("RCXBNEXT", &i, tnd, NTF_NONE);
	i = -1;
	rcxb.tag_WHILE = tnode_newnodetag ("RCXBWHILE", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  rcxb:eventnode -- ONEVENT*/
	i = -1;
	tnd = rcxb.node_EVENTNODE = tnode_newnodetype ("rcxb:eventnode", &i, 3, 0, 0, TNF_NONE);	/* subnodes: 0 = sensor id, 1 = event, 2 = label */
	cops = tnode_newcompops ();
	tnd->ops = cops;

	i = -1;
	rcxb.tag_ONEVENT = tnode_newnodetag ("RCXBONEVENT", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  lookup various tokens*/
	rcxb.tok_REM = lexer_newtoken (KEYWORD, "REM");
	rcxb.tok_FORWARD = lexer_newtoken (KEYWORD, "forward");
	rcxb.tok_REVERSE = lexer_newtoken (KEYWORD, "reverse");

	/*}}}*/
	/*{{{  setup operator tokens*/
	for (i=0; dopmap[i].lookup; i++) {
		dopmap[i].tok = lexer_newtoken (dopmap[i].ttype, dopmap[i].lookup);
	}
	for (i=0; relmap[i].lookup; i++) {
		relmap[i].tok = lexer_newtoken (relmap[i].ttype, relmap[i].lookup);
	}
	for (i=0; mopmap[i].lookup; i++) {
		mopmap[i].tok = lexer_newtoken (mopmap[i].ttype, mopmap[i].lookup);
	}

	/*}}}*/

	return 0;
}
/*}}}*/


/*{{{  rcxb_program_feunit (feunit_t)*/
feunit_t rcxb_program_feunit = {
	init_nodes: rcxb_program_init_nodes,
	reg_reducers: NULL,
	init_dfatrans: NULL,
	post_setup: NULL,
	ident: "rcxb-program"
};
/*}}}*/

