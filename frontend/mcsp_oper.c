/*
 *	mcsp_oper.c -- handling for MCSP operators
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
#include "dfa.h"
#include "parsepriv.h"
#include "mcsp.h"
#include "feunit.h"
#include "names.h"
#include "scope.h"
#include "prescope.h"
#include "constprop.h"
#include "typecheck.h"
#include "usagecheck.h"
#include "postcheck.h"
#include "fetrans.h"
#include "betrans.h"
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
	{SYMBOL, "|||", NULL, &(mcsp.tag_ILEAVE)},
	{SYMBOL, ";", NULL, &(mcsp.tag_SEQ)},
	{SYMBOL, "\\", NULL, &(mcsp.tag_HIDE)},
	{SYMBOL, "|~|", NULL, &(mcsp.tag_ICHOICE)},
	{NOTOKEN, NULL, NULL, NULL}
};


/*}}}*/


/*{{{  static int mcsp_checkisevent (tnode_t *node)*/
/*
 *	checks to see if the given tree is an event
 */
static int mcsp_checkisevent (tnode_t *node)
{
	if (node->tag == mcsp.tag_EVENT) {
		return 1;
	} else if (node->tag == mcsp.tag_CHAN) {
		return 1;
	} else if (node->tag == mcsp.tag_SUBEVENT) {
		return 1;
	}
	return 0;
}
/*}}}*/
/*{{{  static int mcsp_checkisprocess (tnode_t *node)*/
/*
 *	checks to see if the given tree is a process
 */
static int mcsp_checkisprocess (tnode_t *node)
{
	if (node->tag == mcsp.tag_PROCDEF) {
		return 1;
	} else if (node->tag->ndef == mcsp.node_DOPNODE) {
		return 1;
	} else if (node->tag->ndef == mcsp.node_SCOPENODE) {
		return 1;
	} else if (node->tag->ndef == mcsp.node_LEAFPROC) {
		return 1;
	} else if (node->tag == mcsp.tag_INSTANCE) {
		return 1;
	} else if (node->tag == mcsp.tag_REPLSEQ) {
		return 1;
	}
	return 0;
}
/*}}}*/
/*{{{  static int mcsp_checkisexpr (tnode_t *node)*/
/*
 *	checks to see if the given tree is an expression
 */
static int mcsp_checkisexpr (tnode_t *node)
{
	if (node->tag == mcsp.tag_EVENT) {
		return 1;
	} else if (node->tag == mcsp.tag_STRING) {
		return 1;
	} else if (node->tag == mcsp.tag_INTEGER) {
		return 1;
	}
	return 0;
}
/*}}}*/


/*{{{  static int mcsp_typecheck_dopnode (compops_t *cops, tnode_t *node, typecheck_t *tc)*/
/*
 *	does type-checking on a dop-node
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_typecheck_dopnode (compops_t *cops, tnode_t *node, typecheck_t *tc)
{
	if (node->tag == mcsp.tag_THEN) {
		/*{{{  LHS should be an event, RHS should be process*/
		if (!mcsp_checkisevent (tnode_nthsubof (node, 0))) {
			typecheck_error (node, tc, "LHS of -> must be an event");
		}
		if (!mcsp_checkisprocess (tnode_nthsubof (node, 1))) {
			typecheck_error (node, tc, "RHS of -> must be a process");
		}
		/*}}}*/
	} else if (node->tag == mcsp.tag_SUBEVENT) {
		/*{{{  LHS should be an event, RHS can be a name or string*/
		if (!mcsp_checkisevent (tnode_nthsubof (node, 0))) {
			typecheck_error (node, tc, "LHS of . must be an event");
		}
		if (!mcsp_checkisexpr (tnode_nthsubof (node, 1))) {
			typecheck_error (node, tc, "RHS of . must be an expression");
		}
		/*}}}*/
	} else {
		/*{{{  all others take processes on the LHS and RHS*/
		if (!mcsp_checkisprocess (tnode_nthsubof (node, 0))) {
			typecheck_error (node, tc, "LHS of %s must be a process", node->tag->name);
		}
		if (!mcsp_checkisprocess (tnode_nthsubof (node, 1))) {
			typecheck_error (node, tc, "RHS of %s must be a process", node->tag->name);
		}
		/*}}}*/
	}

	/* deal with -> collection here */
	if (node->tag == mcsp.tag_THEN) {
		tnode_t *event = tnode_nthsubof (node, 0);
		mcsp_alpha_t **alphap = (mcsp_alpha_t **)tnode_nthhookaddr (node, 0);

		if (!*alphap) {
			*alphap = mcsp_newalpha ();
		}
		mcsp_addtoalpha (*alphap, event);
	}

	return 1;
}
/*}}}*/
/*{{{  static int mcsp_postcheck_dopnode (compopts_t *cops, tnode_t **node, postcheck_t *pc)*/
/*
 *	does post-check transformations on a DOPNODE
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_postcheck_dopnode (compopts_t *cops, tnode_t **node, postcheck_t *pc)
{
	tnode_t *t = *node;

	if (t->tag == mcsp.tag_THEN) {
		/* don't walk LHS in this pass */
		postcheck_subtree (tnode_nthsubaddr (*node, 1), pc);
		return 0;
	}
	return 1;
}
/*}}}*/
/*{{{  static int mcsp_fetrans_dopnode (compops_t *cops, tnode_t **node, fetrans_t *fe)*/
/*
 *	does front-end transformations on a DOPNODE (quite a lot done here)
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_fetrans_dopnode (compops_t *cops, tnode_t **node, fetrans_t *fe)
{
	mcsp_fetrans_t *mfe = (mcsp_fetrans_t *)fe->langpriv;
	tnode_t *t = *node;

	switch (mfe->parse) {
	case 0:
		/* these have to be done in the right order..! */
		if (t->tag == mcsp.tag_ECHOICE) {
			/*{{{  external-choice: scoop up events and build ALT*/
			tnode_t **lhsp = tnode_nthsubaddr (t, 0);
			tnode_t **rhsp = tnode_nthsubaddr (t, 1);
			tnode_t *list, *altnode;

			if ((*lhsp)->tag == mcsp.tag_THEN) {
				tnode_t *event = tnode_nthsubof (*lhsp, 0);
				tnode_t *process = tnode_nthsubof (*lhsp, 1);
				tnode_t *guard;

				if (event->tag == mcsp.tag_SUBEVENT) {
					/* sub-event, just pick LHS */
					event = tnode_nthsubof (*lhsp, 0);
				}
				guard = tnode_create (mcsp.tag_GUARD, NULL, event, process);

				tnode_setnthsub (*lhsp, 0, NULL);
				tnode_setnthsub (*lhsp, 1, NULL);
				tnode_free (*lhsp);

				*lhsp = guard;
			}
			if ((*rhsp)->tag == mcsp.tag_THEN) {
				tnode_t *event = tnode_nthsubof (*rhsp, 0);
				tnode_t *process = tnode_nthsubof (*rhsp, 1);
				tnode_t *guard;

				if (event->tag == mcsp.tag_SUBEVENT) {
					/* sub-event, just pick LHS */
					event = tnode_nthsubof (*rhsp, 0);
				}
				guard = tnode_create (mcsp.tag_GUARD, NULL, event, process);

				tnode_setnthsub (*rhsp, 0, NULL);
				tnode_setnthsub (*rhsp, 1, NULL);
				tnode_free (*rhsp);

				*rhsp = guard;
			}

			list = parser_buildlistnode (NULL, *lhsp, *rhsp, NULL);
			altnode = tnode_create (mcsp.tag_ALT, NULL, list, NULL);

			tnode_setnthsub (*node, 0, NULL);
			tnode_setnthsub (*node, 1, NULL);
			tnode_free (*node);

			*node = altnode;
			/*}}}*/
		} else if (t->tag == mcsp.tag_THEN) {
			/*{{{  then: scoop up "event-train" and build SEQ*/
			tnode_t *list = parser_newlistnode (NULL);
			tnode_t *next = NULL;

			while ((*node)->tag == mcsp.tag_THEN) {
				next = tnode_nthsubof (*node, 1);
				parser_addtolist (list, tnode_nthsubof (*node, 0));
				tnode_setnthsub (*node, 0, NULL);
				tnode_setnthsub (*node, 1, NULL);
				tnode_free (*node);
				*node = next;
			}

			/* add final process, left in *node */
			parser_addtolist (list, *node);
			list = tnode_create (mcsp.tag_SEQCODE, NULL, NULL, list, NULL);

#if 0
fprintf (stderr, "mcsp_fetrans_dopnode(): list is now:\n");
tnode_dumptree (list, 1, stderr);
#endif
			*node = list;
			/*}}}*/
		} else if (t->tag == mcsp.tag_PAR) {
			/*{{{  parallel: scoop up and build PARCODE*/
			tnode_t *list, *parnode;
			tnode_t *lhs = tnode_nthsubof (t, 0);
			tnode_t *rhs = tnode_nthsubof (t, 1);

			list = parser_buildlistnode (NULL, lhs, rhs, NULL);
			parnode = tnode_create (mcsp.tag_PARCODE, NULL, NULL, list, NULL);

			tnode_setnthsub (t, 0, NULL);
			tnode_setnthsub (t, 1, NULL);
			tnode_free (t);

			*node = parnode;
			/*}}}*/
		} else if (t->tag == mcsp.tag_SEQ) {
			/*{{{  serial: scoop up and build SEQCODE*/
			tnode_t *list, *seqnode;
			tnode_t *lhs = tnode_nthsubof (t, 0);
			tnode_t *rhs = tnode_nthsubof (t, 1);

			list = parser_buildlistnode (NULL, lhs, rhs, NULL);
			seqnode = tnode_create (mcsp.tag_SEQCODE, NULL, NULL, list, NULL);

			tnode_setnthsub (t, 0, NULL);
			tnode_setnthsub (t, 1, NULL);
			tnode_free (t);

			*node = seqnode;
			/*}}}*/
		}
		break;
	}

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

	dopnode = tnode_create (tag, pp->lf, NULL, NULL, NULL, NULL, NULL);
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


/*{{{  static int mcsp_oper_init_nodes (void)*/
/*
 *	initialises MCSP operator nodes
 *	returns 0 on success, non-zero on failure
 */
static int mcsp_oper_init_nodes (void)
{
	tndef_t *tnd;
	int i;
	compops_t *cops;

	/*{{{  mcsp:dopnode -- SUBEVENT, THEN, SEQ, PAR, ALPHAPAR, ILEAVE, ICHOICE, ECHOICE*/
	i = -1;
	tnd = mcsp.node_DOPNODE = tnode_newnodetype ("mcsp:dopnode", &i, 3, 0, 1, TNF_NONE);		/* subnodes: 0 = LHS, 1 = RHS, 2 = type;  hooks: 0 = mcsp_alpha_t */
	tnd->hook_free = mcsp_alpha_hook_free;
	tnd->hook_copy = mcsp_alpha_hook_copy;
	tnd->hook_dumptree = mcsp_alpha_hook_dumptree;
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (mcsp_typecheck_dopnode));
	tnode_setcompop (cops, "postcheck", 2, COMPOPTYPE (mcsp_postcheck_dopnode));
	tnode_setcompop (cops, "fetrans", 2, COMPOPTYPE (mcsp_fetrans_dopnode));
	tnd->ops = cops;

	i = -1;
	mcsp.tag_SUBEVENT = tnode_newnodetag ("MCSPSUBEVENT", &i, tnd, NTF_NONE);
	i = -1;
	mcsp.tag_THEN = tnode_newnodetag ("MCSPTHEN", &i, tnd, NTF_NONE);
	i = -1;
	mcsp.tag_SEQ = tnode_newnodetag ("MCSPSEQ", &i, tnd, NTF_NONE);
	i = -1;
	mcsp.tag_PAR = tnode_newnodetag ("MCSPPAR", &i, tnd, NTF_NONE);
	i = -1;
	mcsp.tag_ALPHAPAR = tnode_newnodetag ("MCSPALPHAPAR", &i, tnd, NTF_NONE);
	i = -1;
	mcsp.tag_ILEAVE = tnode_newnodetag ("MCSPILEAVE", &i, tnd, NTF_NONE);
	i = -1;
	mcsp.tag_ICHOICE = tnode_newnodetag ("MCSPICHOICE", &i, tnd, NTF_NONE);
	i = -1;
	mcsp.tag_ECHOICE = tnode_newnodetag ("MCSPECHOICE", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  deal with operators*/
        for (i=0; opmap[i].lookup; i++) {
		opmap[i].tok = lexer_newtoken (opmap[i].ttype, opmap[i].lookup);
	}

	/*}}}*/

	return 0;
}
/*}}}*/
/*{{{  static int mcsp_oper_reg_reducers (void)*/
/*
 *	registers reducers for MCSP operator nodes
 *	returns 0 on success, non-zero on failure
 */
static int mcsp_oper_reg_reducers (void)
{
	parser_register_grule ("mcsp:nullechoicereduce", parser_decode_grule ("ST0T+@t0000C4R-", mcsp.tag_ECHOICE));

	parser_register_reduce ("Rmcsp:op", mcsp_opreduce, NULL);
	parser_register_reduce ("Rmcsp:folddop", mcsp_folddopreduce, NULL);
	return 0;
}
/*}}}*/
/*{{{  static dfattbl_t **mcsp_oper_init_dfatrans (int *ntrans)*/
/*
 *	creates and returns DFA transition tables for MCSP operator nodes
 */
static dfattbl_t **mcsp_oper_init_dfatrans (int *ntrans)
{
	DYNARRAY (dfattbl_t *, transtbl);

	dynarray_init (transtbl);
	dynarray_add (transtbl, dfa_transtotbl ("mcsp:dop ::= [ 0 +@@-> 1 ] [ 0 +@@; 1 ] [ 0 +@@|| 1 ] [ 0 +@@||| 1 ] [ 0 +@@|~| 1 ] [ 0 +@@[ 3 ] [ 1 Newline 1 ] [ 1 -* 2 ] [ 2 {Rmcsp:op} -* ] "\
				"[ 3 @@] 4 ] [ 4 Newline 4 ] [ 4 -* 5 ] [ 5 {<mcsp:nullechoicereduce>} -* ]"));

	*ntrans = DA_CUR (transtbl);
	return DA_PTR (transtbl);
}
/*}}}*/



/*{{{  mcsp_oper_feunit (feunit_t)*/
feunit_t mcsp_oper_feunit = {
	init_nodes: mcsp_oper_init_nodes,
	reg_reducers: mcsp_oper_reg_reducers,
	init_dfatrans: mcsp_oper_init_dfatrans,
	post_setup: NULL
};

/*}}}*/

