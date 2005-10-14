/*
 *	occampi_asm.c -- occam-pi inline transputer assembler
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
#include "precheck.h"
#include "typecheck.h"
#include "usagecheck.h"
#include "map.h"
#include "target.h"
#include "transputer.h"
#include "codegen.h"
#include "langops.h"


/*}}}*/


/*{{{  private types*/
typedef struct TAG_asmophook {
	transinstr_t *instr;
	DYNARRAY (tnode_t *, args);
} asmophook_t;

/*}}}*/


/*{{{  static asmophook_t *occampi_newasmophook (void)*/
/*
 *	creates a new, blank, asmophook_t
 */
static asmophook_t *occampi_newasmophook (void)
{
	asmophook_t *oh = (asmophook_t *)smalloc (sizeof (asmophook_t));

	oh->instr = NULL;
	dynarray_init (oh->args);

	return oh;
}
/*}}}*/
/*{{{  static void occampi_freeasmophook (asmophook_t *oh)*/
/*
 *	frees an asmophook_t structure
 */
static void occampi_freeasmophook (asmophook_t *oh)
{
	int i;

	for (i=0; i<DA_CUR (oh->args); i++) {
		tnode_t *node = DA_NTHITEM (oh->args, i);

		if (node) {
			tnode_free (node);
		}
	}
	dynarray_trash (oh->args);

	sfree (oh);
	return;
}
/*}}}*/
/*{{{  static void occampi_asmophook_free (void *hook)*/
/*
 *	free-hook function for asmophook_t's
 */
static void occampi_asmophook_free (void *hook)
{
	asmophook_t *oh = (asmophook_t *)hook;

	if (oh) {
		occampi_freeasmophook (oh);
	}
	return;
}
/*}}}*/
/*{{{  static void *occampi_asmophook_copy (void *hook)*/
/*
 *	copy-hook function for asmophook_t's
 */
static void *occampi_asmophook_copy (void *hook)
{
	asmophook_t *oh = (asmophook_t *)hook;
	asmophook_t *newhook;

	if (!oh) {
		newhook = NULL;
	} else {
		int i;

		newhook = occampi_newasmophook ();
		newhook->instr = oh->instr;
		for (i=0; i<DA_CUR (oh->args); i++) {
			tnode_t *arg = DA_NTHITEM (oh->args, i);

			dynarray_add (newhook->args, tnode_copytree (arg));
		}
	}

	return (void *)newhook;
}
/*}}}*/
/*{{{  static void occampi_asmophook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	hook dump-tree function for asmophook_t's
 */
static void occampi_asmophook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	asmophook_t *oh = (asmophook_t *)hook;

	if (!node || !oh) {
		occampi_isetindent (stream, indent);
		fprintf (stream, "<asmophook:nullnode />\n");
	} else if (DA_CUR (oh->args)) {
		int i;

		occampi_isetindent (stream, indent);
		fprintf (stream, "<asmophook op=\"%s\">\n", oh->instr ? oh->instr->name : "(unknown)");
		for (i=0; i<DA_CUR (oh->args); i++) {
			tnode_t *arg = DA_NTHITEM (oh->args, i);

			tnode_dumptree (arg, indent + 1, stream);
		}
		occampi_isetindent (stream, indent);
		fprintf (stream, "</asmophook>\n");
	} else {
		occampi_isetindent (stream, indent);
		fprintf (stream, "<asmophook op=\"%s\" />\n", oh->instr ? oh->instr->name : "(unknown)");
	}
	return;
}
/*}}}*/
/*{{{  static void occampi_asmophook_postwalktree (tnode_t *node, void *hook, void (*fcn)(tnode_t *, void *), void *arg)*/
/*
 *	does a post-walk-tree through asmophook_t's
 */
static void occampi_asmophook_postwalktree (tnode_t *node, void *hook, void (*fcn)(tnode_t *, void *), void *arg)
{
	asmophook_t *oh = (asmophook_t *)hook;
	int i;

	if (!oh) {
		return;
	}
	for (i=0; i<DA_CUR (oh->args); i++) {
		tnode_t *oparg = DA_NTHITEM (oh->args, i);

		tnode_postwalktree (oparg, fcn, arg);
	}
	return;
}
/*}}}*/
/*{{{  static void occampi_asmophook_prewalktree (tnode_t *node, void *hook, int (*fcn)(tnode_t *, void *), void *arg)*/
/*
 *	does a pre-walk-tree through asmophook_t's
 */
static void occampi_asmophook_prewalktree (tnode_t *node, void *hook, int (*fcn)(tnode_t *, void *), void *arg)
{
	asmophook_t *oh = (asmophook_t *)hook;
	int i;

	if (!node || !oh) {
		return;
	}
	for (i=0; i<DA_CUR (oh->args); i++) {
		tnode_t *oparg = DA_NTHITEM (oh->args, i);

		tnode_prewalktree (oparg, fcn, arg);
	}
	return;
}
/*}}}*/
/*{{{  static void occampi_asmophook_modprewalktree (tnode_t **nodep, void *hook, int (*fcn)(tnode_t **, void *), void *arg)*/
/*
 *	does a mod-pre-walk-tree through asmophook_t's
 */
static void occampi_asmophook_modprewalktree (tnode_t **nodep, void *hook, int (*fcn)(tnode_t **, void *), void *arg)
{
	asmophook_t *oh = (asmophook_t *)hook;
	int i;

	if (!nodep || !*nodep || !oh) {
		return;
	}
	for (i=0; i<DA_CUR (oh->args); i++) {
		tnode_t **opargp = DA_NTHITEMADDR (oh->args, i);

		tnode_modprewalktree (opargp, fcn, arg);
	}
	return;
}
/*}}}*/
/*{{{  static void occampi_asmophook_modprepostwalktree (tnode_t **nodep, void *hook, int (*prefcn)(tnode_t **, void *), int (*postfcn)(tnode_t **, void *), void *arg)*/
/*
 *	does a mod-pre-post-walk-tree through asmophook_t's
 */
static void occampi_asmophook_modprepostwalktree (tnode_t **nodep, void *hook, int (*prefcn)(tnode_t **, void *), int (*postfcn)(tnode_t **, void *), void *arg)
{
	asmophook_t *oh = (asmophook_t *)hook;
	int i;

	if (!oh) {
		return;
	}
	for (i=0; i<DA_CUR (oh->args); i++) {
		tnode_t **opargp = DA_NTHITEMADDR (oh->args, i);

		tnode_modprepostwalktree (opargp, prefcn, postfcn, arg);
	}
	return;
}
/*}}}*/


/*{{{  static int occampi_typecheck_asmop (tnode_t *node, typecheck_t *tc)*/
/*
 *	does type-checks on inline assembly statements
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_typecheck_asmop (tnode_t *node, typecheck_t *tc)
{
	asmophook_t *oh = (asmophook_t *)tnode_nthhookof (node, 0);
	int i;

	if (!oh) {
		nocc_internal ("occampi_typecheck_asmop(): no op!");
		return 0;
	}
	/* do typechecks on any subnodes first */
	for (i=0; i<DA_CUR (oh->args); i++) {
		tnode_t *asmop = DA_NTHITEM (oh->args, i);

		if (asmop) {
			typecheck_subtree (asmop, tc);
		}
	}

	switch (oh->instr->level) {
	case INS_INVALID:
		typecheck_error (node, tc, "invalid instruction");
		break;
	case INS_PRIMARY:
		typecheck_error (node, tc, "occampi_typecheck_asmop(): no primary instructions, yet..");
		break;
	case INS_SECONDARY:
		if (DA_CUR (oh->args)) {
			typecheck_error (node, tc, "secondary instruction %s with operands", oh->instr->name);
		}
		break;
	case INS_OTHER:
		switch (oh->instr->ins) {
		default:
			typecheck_error (node, tc, "occampi_typecheck_asmop(): unknown OTHER instruction [%s]", oh->instr->name);
			break;
		case I_LD:
		case I_ST:
			if (DA_CUR (oh->args) != 1) {
				typecheck_error (node, tc, "instruction %s requires a single operand", oh->instr->name);
			} else {
				tnode_t *type = typecheck_gettype (DA_NTHITEM (oh->args, 0), NULL);

				/* don't actually do anything with it yet.. */
			}
			break;
		}
		break;
	}
	return 0;
}
/*}}}*/
/*{{{  static int occampi_codegen_asmop (tnode_t *node, codegen_t *cgen)*/
/*
 *	generates-code for an inline assembly statement
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_codegen_asmop (tnode_t *node, codegen_t *cgen)
{
	asmophook_t *oh = (asmophook_t *)tnode_nthhookof (node, 0);

	if (!oh) {
		nocc_internal ("occampi_codegen_asmop(): no op!");
		return 0;
	}
	switch (oh->instr->level) {
	case INS_INVALID:
		codegen_error (cgen, "occampi_codegen_asmop(): INVALID instruction");
		break;
	case INS_PRIMARY:
		codegen_warning (cgen, "occampi_codegen_asmop(): no primary instructions, yet..");
		break;
	case INS_SECONDARY:
		codegen_callops (cgen, tsecondary, oh->instr->ins);
		break;
	case INS_OTHER:
		switch (oh->instr->ins) {
		default:
			codegen_error (cgen, "occampi_codegen_asmop(): unknown OTHER instruction [%s]", oh->instr->name);
			break;
		case I_LD:
			codegen_callops (cgen, loadname, DA_NTHITEM (oh->args, 0), 0);
			break;
		case I_ST:
			codegen_callops (cgen, storename, DA_NTHITEM (oh->args, 0), 0);
			break;
		}
		break;
	}
	return 0;
}
/*}}}*/


/*{{{  static void occampi_asmop_reduce (dfastate_t *dfast, parsepriv_t *pp, void *rarg)*/
/*
 *	reduces what is expected to be an ASM mnemonic and operands;  mnemonic is left on
 *	the token-stack, arguments as a list on the node-stack
 */
static void occampi_asmop_reduce (dfastate_t *dfast, parsepriv_t *pp, void *rarg)
{
	token_t *mne = parser_gettok (pp);
	tnode_t *args = dfa_popnode (dfast);
	tnode_t *opnode;
	asmophook_t *oh = occampi_newasmophook ();
	char *tstr = NULL;
	int tlen = 0;

	switch (mne->type) {
	case KEYWORD:
		tstr = mne->u.kw->name;
		tlen = strlen (tstr);
		break;
	case NAME:
		tstr = mne->u.name;
		tlen = strlen (tstr);
		break;
	default:
		parser_error (pp->lf, "occampi_asmop_reduce(): unhandled token mnemonic [%s]", lexer_stokenstr (mne));
		occampi_freeasmophook (oh);
		return;
	}
	oh->instr = transinstr_lookup (tstr, tlen);
	if (!oh->instr) {
		parser_error (pp->lf, "occampi_asmop_reduce(): unknown instruction [%*s]", tlen, tstr);
		occampi_freeasmophook (oh);
		return;
	}

	opnode = tnode_create (opi.tag_ASMOP, pp->lf, oh);
#if 0
fprintf (stderr, "occampi_asmop_reduce(): args =\n");
tnode_dumptree (args, 1, stderr);
#endif
	if (args && parser_islistnode (args)) {
		tnode_t **items;
		int nitems, i;

		items = parser_getlistitems (args, &nitems);
		for (i=0; i<nitems; i++) {
			if (items[i]) {
				dynarray_add (oh->args, items[i]);
				items[i] = NULL;
			}
		}
		tnode_free (args);
	} else if (args) {
		dynarray_add (oh->args, args);
	}

	*(dfast->ptr) = opnode;

#if 0
fprintf (stderr, "occampi_asmop_reduce(): reduced operand.  tstr=[%s], instr=[%s], args=0x%8.8x, opnode=\n", tstr, oh->instr->name, (unsigned int)args);
tnode_dumptree (opnode, 1, stderr);
#endif
	lexer_freetoken (mne);
	return;
}
/*}}}*/


/*{{{  static int occampi_asm_init_nodes (void)*/
/*
 *	sets up nodes for occam-pi inline assembler
 *	returns 0 on success, non-zero on error
 */
static int occampi_asm_init_nodes (void)
{
	tndef_t *tnd;
	compops_t *cops;
	int i;

	/*{{{  occampi:asmnode -- ASM*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:asmnode", &i, 1, 0, 0, TNF_NONE);
	cops = tnode_newcompops ();
	tnd->ops = cops;

	i = -1;
	opi.tag_ASM = tnode_newnodetag ("ASM", &i, tnd, NTF_NONE);
	/*}}}*/
	/*{{{  occampi:asmop -- ASMOP*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:asmop", &i, 0, 0, 1, TNF_NONE);
	tnd->hook_copy = occampi_asmophook_copy;
	tnd->hook_free = occampi_asmophook_free;
	tnd->hook_dumptree = occampi_asmophook_dumptree;
	tnd->hook_postwalktree = occampi_asmophook_postwalktree;
	tnd->hook_prewalktree = occampi_asmophook_prewalktree;
	tnd->hook_modprewalktree = occampi_asmophook_modprewalktree;
	tnd->hook_modprepostwalktree = occampi_asmophook_modprepostwalktree;
	cops = tnode_newcompops ();
	cops->typecheck = occampi_typecheck_asmop;
	cops->codegen = occampi_codegen_asmop;
	tnd->ops = cops;

	i = -1;
	opi.tag_ASMOP = tnode_newnodetag ("ASMOP", &i, tnd, NTF_NONE);
	/*}}}*/

	return 0;
}
/*}}}*/
/*{{{  static int occampi_asm_reg_reducers (void)*/
/*
 *	registers reductions for occam-pi inline assembler nodes
 *	returns 0 on success, non-zero on error
 */
static int occampi_asm_reg_reducers (void)
{
	parser_register_reduce ("Roccampi:asmop", occampi_asmop_reduce, NULL);

	parser_register_grule ("opi:asmblock", parser_decode_grule ("N+X*Sn0C1R-", parser_inlistfixup, opi.tag_ASM));
	return 0;
}
/*}}}*/
/*{{{  static dfattbl_t **occampi_asm_init_dfatrans (int *ntrans)*/
/*
 *	initialises and returns DFA transition tables for occam-pi inline assembler nodes
 */
static dfattbl_t **occampi_asm_init_dfatrans (int *ntrans)
{
	DYNARRAY (dfattbl_t *, transtbl);

	dynarray_init (transtbl);

	dynarray_add (transtbl, dfa_transtotbl ("occampi:asmop ::= [ 0 +* 1 ] [ 1 occampi:exprcommalist 3 ] [ 1 -* 2 ] [ 2 {<opi:nullpush>} -* 3 ] [ 3 {Roccampi:asmop} -* ]"));
	dynarray_add (transtbl, dfa_transtotbl ("occampi:asmoplist ::= [ 0 -Outdent 5 ] [ 0 Newline 4 ] [ 0 -* 1 ] [ 1 occampi:asmop 2 ] [ 2 {<opi:nullreduce>} -* 3 ] [ 3 {Rinlist} Newline 0 ] [ 4 -* 0 ] [ 5 -* ]"));
	dynarray_add (transtbl, dfa_transtotbl ("occampi:asmblock ::= [ 0 @ASM 1 ] [ 1 Newline 2 ] [ 2 Indent 3 ] [ 3 occampi:asmoplist 4 ] [ 4 Outdent 5 ] [ 5 {<opi:asmblock>} -* ]"));

	*ntrans = DA_CUR (transtbl);
	return DA_PTR (transtbl);
}
/*}}}*/
/*{{{  static int occampi_asm_post_setup (void)*/
/*
 *	does post-setup for occam-pi inline assembly nodes
 *	returns 0 on success, non-zero on error
 */
static int occampi_asm_post_setup (void)
{
	return 0;
}
/*}}}*/


/*{{{  occampi_asm_feunit (feunit_t)*/
feunit_t occampi_asm_feunit = {
	init_nodes: occampi_asm_init_nodes,
	reg_reducers: occampi_asm_reg_reducers,
	init_dfatrans: occampi_asm_init_dfatrans,
	post_setup: occampi_asm_post_setup
};
/*}}}*/



