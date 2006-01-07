/*
 *	mcsp_process.c -- handling for MCSP processes
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
#include "mcsp.h"
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
typedef struct TAG_opmap {
	tokentype_t ttype;
	const char *lookup;
	token_t *tok;
	ntdef_t **tagp;
} opmap_t;

static opmap_t opmap[] = {
	{SYMBOL, "->", NULL, &(mcsp.tag_THEN)},
	{SYMBOL, "||", NULL, &(mcsp.tag_PAR)},
	{SYMBOL, ";", NULL, &(mcsp.tag_SEQ)},
	{SYMBOL, "\\", NULL, &(mcsp.tag_HIDE)},
	{NOTOKEN, NULL, NULL, NULL}
};


/*}}}*/

/*{{{  void *mcsp_nametoken_to_hook (void *ntok)*/
/*
 *	turns a name token into a hooknode for a tag_NAME
 */
void *mcsp_nametoken_to_hook (void *ntok)
{
	token_t *tok = (token_t *)ntok;
	char *rawname;

	rawname = tok->u.name;
	tok->u.name = NULL;

	lexer_freetoken (tok);

	return (void *)rawname;
}
/*}}}*/


/*{{{  static void mcsp_rawnamenode_hook_free (void *hook)*/
/*
 *	frees a rawnamenode hook (name-bytes)
 */
static void mcsp_rawnamenode_hook_free (void *hook)
{
	if (hook) {
		sfree (hook);
	}
	return;
}
/*}}}*/
/*{{{  static void *mcsp_rawnamenode_hook_copy (void *hook)*/
/*
 *	copies a rawnamenode hook (name-bytes)
 */
static void *mcsp_rawnamenode_hook_copy (void *hook)
{
	char *rawname = (char *)hook;

	if (rawname) {
		return string_dup (rawname);
	}
	return NULL;
}
/*}}}*/
/*{{{  static void mcsp_rawnamenode_hook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	dump-tree for rawnamenode hook (name-bytes)
 */
static void mcsp_rawnamenode_hook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	mcsp_isetindent (stream, indent);
	fprintf (stream, "<mcsprawnamenode value=\"%s\" />\n", hook ? (char *)hook : "(null)");
	return;
}
/*}}}*/

/*{{{  static int mcsp_scopein_rawname (tnode_t **node, scope_t *ss)*/
/*
 *	scopes in a free-floating name
 */
static int mcsp_scopein_rawname (tnode_t **node, scope_t *ss)
{
	tnode_t *name = *node;
	char *rawname;
	name_t *sname = NULL;

	if (name->tag != mcsp.tag_NAME) {
		scope_error (name, ss, "name not raw-name!");
		return 0;
	}
	rawname = tnode_nthhookof (name, 0);

#if 0
fprintf (stderr, "mcsp_scopein_rawname: here! rawname = \"%s\"\n", rawname);
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

/*{{{  static int mcsp_prescope_scopenode (tnode_t **node, prescope_t *ps)*/
/*
 *	does pre-scoping on an MCSP scope node (ensures vars are a list)
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_prescope_scopenode (tnode_t **node, prescope_t *ps)
{
	if (!tnode_nthsubof (*node, 0)) {
		/* no vars, make empty list */
		tnode_setnthsub (*node, 0, parser_newlistnode (NULL));
	} else if (!parser_islistnode (tnode_nthsubof (*node, 0))) {
		/* singleton */
		tnode_t *list = parser_newlistnode (NULL);

		parser_addtolist (list, tnode_nthsubof (*node, 0));
		tnode_setnthsub (*node, 0, list);
	}
	return 1;
}
/*}}}*/
/*{{{  static int mcsp_scopein_scopenode (tnode_t **node, scope_t *ss)*/
/*
 *	called to scope in an MCSP something that introduces scope (names)
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_scopein_scopenode (tnode_t **node, scope_t *ss)
{
	void *nsmark;
	tnode_t *vars = tnode_nthsubof (*node, 0);
	tnode_t **bodyptr = tnode_nthsubaddr (*node, 1);
	tnode_t **varlist;
	int nvars, i;

	nsmark = name_markscope ();

	/* go through each name and bring it into scope */
	varlist = parser_getlistitems (vars, &nvars);
	for (i=0; i<nvars; i++) {
		tnode_t *name = varlist[i];
		char *rawname;
		name_t *sname;
		tnode_t *newname;

		if (name->tag != mcsp.tag_NAME) {
			scope_error (name, ss, "not raw name!");
			return 0;
		}
#if 0
fprintf (stderr, "mcsp_scopein_scopenode(): scoping in name =\n");
tnode_dumptree (name, 1, stderr);
#endif
		rawname = (char *)tnode_nthhookof (name, 0);

		sname = name_addscopename (rawname, *node, NULL, NULL);
		newname = tnode_createfrom (mcsp.tag_EVENT, name, sname);
		SetNameNode (sname, newname);
		varlist[i] = newname;		/* put new name in list */
		ss->scoped++;

		/* free old name */
		tnode_free (name);
	}

	/* then walk the body */
	tnode_modprepostwalktree (bodyptr, scope_modprewalktree, scope_modpostwalktree, (void *)ss);

	/* descope declared names */
	name_markdescope (nsmark);
	
	return 0;
}
/*}}}*/
/*{{{  static int mcsp_scopeout_scopenode (tnode_t **node, scope_t *ss)*/
/*
 *	called to scope-out an MCSP something
 */
static int mcsp_scopeout_scopenode (tnode_t **node, scope_t *ss)
{
	/* all done in scope-in */
	return 1;
}
/*}}}*/

/*{{{  static void mcsp_opreduce (dfastate_t *dfast, parsepriv_t *pp, void *rarg)*/
/*
 *	turns an MCSP operator (->, etc.) into a node
 */
static void mcsp_opreduce (dfastate_t *dfast, parsepriv_t *pp, void *rarg)
{
	token_t *tok = parser_gettok (pp);
	ntdef_t *tag = NULL;
	int i;
	tnode_t *dopnode;

	if (!tok) {
		parser_error (pp->lf, "mcsp_opreduce(): no token ?");
		return;
	}
	for (i=0; opmap[i].lookup; i++) {
		if (lexer_tokmatch (opmap[i].tok, tok)) {
			tag = *(opmap[i].tagp);
			break;		/* for() */
		}
	}
	if (!tag) {
		parser_error (pp->lf, "mcsp_opreduce(): unhandled token [%s]", lexer_stokenstr (tok));
		return;
	}

	dopnode = tnode_create (tag, pp->lf, NULL, NULL);
	*(dfast->ptr) = dopnode;
	
	return;
}
/*}}}*/
/*{{{  static void mcsp_folddopreduce (dfastate_t *dfast, parsepriv_t *pp, void *rarg)*/
/*
 *	this folds up a dopnode, taking the operator and its LHS/RHS off the node-stack,
 *	making the result the dopnode
 */
static void mcsp_folddopreduce (dfastate_t *dfast, parsepriv_t *pp, void *rarg)
{
	tnode_t *lhs, *rhs, *dopnode;

	rhs = dfa_popnode (dfast);
	dopnode = dfa_popnode (dfast);
	lhs = dfa_popnode (dfast);

	if (!dopnode || !lhs || !rhs) {
		parser_error (pp->lf, "mcsp_folddopreduce(): missing node, lhs or rhs!");
		return;
	}
	if (tnode_nthsubof (dopnode, 0) || tnode_nthsubof (dopnode, 1)) {
		parser_error (pp->lf, "mcsp_folddopreduce(): dopnode already has lhs or rhs!");
		return;
	}
	
	/* fold in */
	tnode_setnthsub (dopnode, 0, lhs);
	tnode_setnthsub (dopnode, 1, rhs);
	*(dfast->ptr) = dopnode;

#if 0
fprintf (stderr, "mcsp_folddopreduce(): folded up into dopnode =\n");
tnode_dumptree (dopnode, 1, stderr);
#endif
	return;
}
/*}}}*/


/*{{{  static int mcsp_process_init_nodes (void)*/
/*
 *	initialises MCSP process nodes
 *	returns 0 on success, non-zero on failure
 */
static int mcsp_process_init_nodes (void)
{
	tndef_t *tnd;
	int i;
	compops_t *cops;

	/*{{{  mcsp:rawnamenode -- NAME*/
	i = -1;
	tnd = tnode_newnodetype ("mcsp:rawnamenode", &i, 0, 0, 1, TNF_NONE);				/* hooks: raw-name */
	tnd->hook_free = mcsp_rawnamenode_hook_free;
	tnd->hook_copy = mcsp_rawnamenode_hook_copy;
	tnd->hook_dumptree = mcsp_rawnamenode_hook_dumptree;
	cops = tnode_newcompops ();
	cops->scopein = mcsp_scopein_rawname;
	tnd->ops = cops;

	i = -1;
	mcsp.tag_NAME = tnode_newnodetag ("MCSPNAME", &i, tnd, NTF_NONE);

#if 0
fprintf (stderr, "mcsp_process_init_nodes(): tnd->name = [%s], mcsp.tag_NAME->name = [%s], mcsp.tag_NAME->ndef->name = [%s]\n", tnd->name, mcsp.tag_NAME->name, mcsp.tag_NAME->ndef->name);
#endif
	/*}}}*/
	/*{{{  mcsp:dopnode -- THEN, SEQ, PAR, ILEAVE*/
	i = -1;
	tnd = tnode_newnodetype ("mcsp:dopnode", &i, 2, 0, 0, TNF_NONE);				/* subnodes: 0 = event, 1 = process */
	cops = tnode_newcompops ();
	tnd->ops = cops;

	i = -1;
	mcsp.tag_THEN = tnode_newnodetag ("MCSPTHEN", &i, tnd, NTF_NONE);
	i = -1;
	mcsp.tag_SEQ = tnode_newnodetag ("MCSPSEQ", &i, tnd, NTF_NONE);
	i = -1;
	mcsp.tag_PAR = tnode_newnodetag ("MCSPPAR", &i, tnd, NTF_NONE);
	i = -1;
	mcsp.tag_ILEAVE = tnode_newnodetag ("MCSPILEAVE", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  mcsp:scopenode -- HIDE*/
	i = -1;
	tnd = tnode_newnodetype ("mcsp:scopenode", &i, 2, 0, 0, TNF_NONE);				/* subnodes: 0 = process, 1 = vars */
	cops = tnode_newcompops ();
	cops->prescope = mcsp_prescope_scopenode;
	cops->scopein = mcsp_scopein_scopenode;
	cops->scopeout = mcsp_scopeout_scopenode;
	tnd->ops = cops;

	i = -1;
	mcsp.tag_HIDE = tnode_newnodetag ("MCSPHIDE", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  mcsp:declnode -- PROCDECL*/
	i = -1;
	tnd = tnode_newnodetype ("mcsp:declnode", &i, 3, 0, 0, TNF_NONE);				/* subnodes: 0 = name, 1 = params, 2 = body */
	cops = tnode_newcompops ();
	tnd->ops = cops;

	i = -1;
	mcsp.tag_PROCDECL = tnode_newnodetag ("MCSPPROCDECL", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  mcsp:namenode -- EVENT, PROCDEF*/
	i = -1;
	tnd = mcsp.node_NAMENODE = tnode_newnodetype ("mcsp:namenode", &i, 0, 1, 0, TNF_NONE);		/* subnames: 0 = name */
	cops = tnode_newcompops ();
/*	cops->gettype = mcsp_gettype_namenode; */
	tnd->ops = cops;

	i = -1;
	mcsp.tag_EVENT = tnode_newnodetag ("MCSPEVENT", &i, tnd, NTF_NONE);
	i = -1;
	mcsp.tag_PROCDEF = tnode_newnodetag ("MCSPPROCDEF", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  deal with operators*/
        for (i=0; opmap[i].lookup; i++) {
		opmap[i].tok = lexer_newtoken (opmap[i].ttype, opmap[i].lookup);
	}

	/*}}}*/

	return 0;
}
/*}}}*/
/*{{{  static int mcsp_process_reg_reducers (void)*/
/*
 *	registers reducers for MCSP process nodes
 *	returns 0 on success, non-zero on failure
 */
static int mcsp_process_reg_reducers (void)
{
	parser_register_grule ("mcsp:namereduce", parser_decode_grule ("T+St0XC1R-", mcsp_nametoken_to_hook, mcsp.tag_NAME));
	parser_register_grule ("mcsp:hidereduce", parser_decode_grule ("ST0T+@tN+N+C2R-", mcsp.tag_HIDE));

	parser_register_reduce ("Rmcsp:op", mcsp_opreduce, NULL);
	parser_register_reduce ("Rmcsp:folddop", mcsp_folddopreduce, NULL);
	return 0;
}
/*}}}*/
/*{{{  static dfattbl_t **mcsp_process_init_dfatrans (int *ntrans)*/
/*
 *	creates and returns DFA transition tables for MCSP process nodes
 */
static dfattbl_t **mcsp_process_init_dfatrans (int *ntrans)
{
	DYNARRAY (dfattbl_t *, transtbl);

	dynarray_init (transtbl);
	dynarray_add (transtbl, dfa_transtotbl ("mcsp:name ::= [ 0 +Name 1 ] [ 1 {<mcsp:namereduce>} -* ]"));
	dynarray_add (transtbl, dfa_transtotbl ("mcsp:event ::= [ 0 mcsp:name 1 ] [ 1 {<mcsp:nullreduce>} -* ]"));
	dynarray_add (transtbl, dfa_bnftotbl ("mcsp:eventset ::= ( mcsp:event | @@{ { mcsp:event @@, 1 } @@} )"));
	dynarray_add (transtbl, dfa_transtotbl ("mcsp:dop ::= [ 0 +@@-> 1 ] [ 0 +@@; 1 ] [ 0 +@@|| 1 ] [ 1 {Rmcsp:op} -* ]"));
	dynarray_add (transtbl, dfa_transtotbl ("mcsp:hide ::= [ 0 +@@\\ 1 ] [ 1 mcsp:eventset 2 ] [ 2 {<mcsp:hidereduce>} -* ]"));
	dynarray_add (transtbl, dfa_transtotbl ("mcsp:restofprocess ::= [ 0 mcsp:dop 1 ] [ 1 mcsp:process 2 ] [ 2 {Rmcsp:folddop} -* ] " \
				"[ 0 %mcsp:hide <mcsp:hide> ]"));
	dynarray_add (transtbl, dfa_transtotbl ("mcsp:process ::= [ 0 mcsp:event 1 ] [ 0 @@( 3 ] [ 1 %mcsp:restofprocess <mcsp:restofprocess> ] [ 1 -* 2 ] [ 2 {<mcsp:nullreduce>} -* ] " \
				"[ 3 mcsp:process 4 ] [ 4 @@) 5 ] [ 5 %mcsp:restofprocess <mcsp:restofprocess> ] [ 5 -* 6 ] [ 6 {<mcsp:nullreduce>} -* ]"));

	*ntrans = DA_CUR (transtbl);
	return DA_PTR (transtbl);
}
/*}}}*/


/*{{{  mcsp_process_feunit (feunit_t)*/
feunit_t mcsp_process_feunit = {
	init_nodes: mcsp_process_init_nodes,
	reg_reducers: mcsp_process_reg_reducers,
	init_dfatrans: mcsp_process_init_dfatrans,
	post_setup: NULL
};
/*}}}*/

