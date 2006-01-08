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
typedef struct TAG_rcxb_lithook {
	char *data;
	int len;
} rcxb_lithook_t;


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
	rcxb_lithook_t *ld = (rcxb_lithook_t *)ld;

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

		newlit->data = lit->data ? string_ndup (lit->data, lit->len) : NULL;
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
	fprintf (stream, "<rcxblitnode size=\"%d\" value=\"%s\" />\n", lit ? lit->len : 0, (lit && lit->data) ? lit->data : "(null)");

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
	/*{{{  rcxb:leafnode -- MOTORA, MOTORB, MOTORC, SENSOR1, SENSOR2, SENSOR3*/
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

	/*}}}*/
	/*{{{  rcxb:actionnode -- SETMOTOR, SETSENSOR, SETPOWER, SETDIRECTION*/
	i = -1;
	tnd = rcxb.node_ACTIONNODE = tnode_newnodetype ("rcxb:actionnode", &i, 2, 0, 0, TNF_NONE);	/* subnodes: 0 = motor/sensor ID, 1 = setting */
	cops = tnode_newcompops ();
	tnd->ops = cops;

	i = -1;
	rcxb.tag_SETMOTOR = tnode_newnodetag ("RCXBSETMOTOR", &i, tnd, NTF_NONE);
	i = -1;
	rcxb.tag_SETSENSOR = tnode_newnodetag ("RCXBSETSENSOR", &i, tnd, NTF_NONE);

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
	parser_register_grule ("rcxb:namereduce", parser_decode_grule ("ST0T+XC1R-", rcxb_nametoken_to_hook, rcxb.tag_NAME));
	parser_register_grule ("rcxb:stringreduce", parser_decode_grule ("ST0T+XR-", rcxb_stringtoken_to_node));
	parser_register_grule ("rcxb:idreduce", parser_decode_grule ("ST0T+XR-", rcxb_idtoken_to_node));
	parser_register_grule ("rcxb:setmotorreduce", parser_decode_grule ("SN0N+N+VC2R-", rcxb.tag_SETMOTOR));
	parser_register_grule ("rcxb:setsensorreduce", parser_decode_grule ("SN0N+N+VC2R-", rcxb.tag_SETSENSOR));

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
	dynarray_add (transtbl, dfa_transtotbl ("rcxb:name ::= [ 0 +Name 1 ] [ 1 {<rcxb:namereduce>} -* ]"));
	dynarray_add (transtbl, dfa_transtotbl ("rcxb:expr ::= [ 0 +String 1 ] [ 1 {<rcxb:stringreduce>} -* ]"));
	dynarray_add (transtbl, dfa_transtotbl ("rcxb:id ::= [ 0 +Name 1 ] [ 0 +Integer 1 ] [ 1 {<rcxb:idreduce>} -* ]"));
	dynarray_add (transtbl, dfa_transtotbl ("rcxb:statement ::= [ 0 @set 1 ] [ 0 Comment 8 ] " \
				"[ 1 @motor 2 ] [ 1 @sensor 5 ] [ 2 rcxb:id 3 ] [ 3 rcxb:expr 4 ] [ 4 {<rcxb:setmotorreduce>} -* ] " \
				"[ 5 rcxb:id 6 ] [ 6 rcxb:expr 7 ] [ 7 {<rcxb:setsensorreduce>} -* ] " \
				"[ 8 -* ]"));
	dynarray_add (transtbl, dfa_bnftotbl ("rcxb:program ::= { rcxb:statement Newline 1 }"));

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

