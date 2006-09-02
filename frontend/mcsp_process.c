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
#include "constprop.h"
#include "typecheck.h"
#include "usagecheck.h"
#include "fetrans.h"
#include "betrans.h"
#include "map.h"
#include "codegen.h"
#include "target.h"
#include "transputer.h"


/*}}}*/

/*{{{  private types/data*/
typedef struct TAG_mcsp_consthook {
	char *data;
	int length;
	int valtype;		/* whether 'data' should be interpreted as a number */
} mcsp_consthook_t;


/*}}}*/

/*{{{  static void *mcsp_nametoken_to_hook (void *ntok)*/
/*
 *	turns a name token into a hooknode for a tag_NAME
 */
static void *mcsp_nametoken_to_hook (void *ntok)
{
	token_t *tok = (token_t *)ntok;
	char *rawname;

	rawname = tok->u.name;
	tok->u.name = NULL;

	lexer_freetoken (tok);

	return (void *)rawname;
}
/*}}}*/
/*{{{  static void *mcsp_stringtoken_to_hook (void *ntok)*/
/*
 *	turns a string token into a hooknode for a tag_STRING
 */
static void *mcsp_stringtoken_to_hook (void *ntok)
{
	token_t *tok = (token_t *)ntok;
	mcsp_consthook_t *ch;

	ch = (mcsp_consthook_t *)smalloc (sizeof (mcsp_consthook_t));
	ch->data = string_ndup (tok->u.str.ptr, tok->u.str.len);
	ch->length = tok->u.str.len;
	ch->valtype = 0;

	lexer_freetoken (tok);
	return (void *)ch;
}
/*}}}*/
/*{{{  static void *mcsp_integertoken_to_hook (void *ntok)*/
/*
 *	turns an integer token into a hooknode for a tag_INTEGER
 */
static void *mcsp_integertoken_to_hook (void *ntok)
{
	token_t *tok = (token_t *)ntok;
	mcsp_consthook_t *ch;

	ch = (mcsp_consthook_t *)smalloc (sizeof (mcsp_consthook_t));
	ch->data = mem_ndup (&(tok->u.ival), sizeof (int));
	ch->length = sizeof (int);
	ch->valtype = 1;

	lexer_freetoken (tok);
	return (void *)ch;
}
/*}}}*/
/*{{{  static void *mcsp_pptoken_to_node (void *ntok)*/
/*
 *	turns a keyword token for a primitive process into a mcsp:leafproc node
 */
static void *mcsp_pptoken_to_node (void *ntok)
{
	token_t *tok = (token_t *)ntok;
	char *sbuf;
	tnode_t *node;
	ntdef_t *tag;

	if (tok->type != KEYWORD) {
		nocc_error ("mcsp_pptoken_to_node(): token not keyword: [%s]", lexer_stokenstr (tok));
		lexer_freetoken (tok);
		return NULL;
	}
	sbuf = (char *)smalloc (128);
	snprintf (sbuf, 127, "MCSP%s", tok->u.kw->name);

	tag = tnode_lookupnodetag (sbuf);
	if (!tag) {
		nocc_error ("mcsp_pptoken_to_node(): keyword not node-tag: [%s]", sbuf);
		sfree (sbuf);
		lexer_freetoken (tok);
		return NULL;
	}
	sfree (sbuf);

	node = tnode_create (tag, tok->origin);
	lexer_freetoken (tok);

	return node;
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


/*{{{  static void mcsp_constnode_hook_free (void *hook)*/
/*
 *	frees a constnode hook (bytes)
 */
static void mcsp_constnode_hook_free (void *hook)
{
	mcsp_consthook_t *ch = (mcsp_consthook_t *)hook;

	if (ch) {
		if (ch->data) {
			sfree (ch->data);
		}
		sfree (ch);
	}

	return;
}
/*}}}*/
/*{{{  static void *mcsp_constnode_hook_copy (void *hook)*/
/*
 *	copies a constnode hook (name-bytes)
 */
static void *mcsp_constnode_hook_copy (void *hook)
{
	mcsp_consthook_t *ch = (mcsp_consthook_t *)hook;

	if (ch) {
		mcsp_consthook_t *newch = (mcsp_consthook_t *)smalloc (sizeof (mcsp_consthook_t));

		if (ch->valtype) {
			newch->data = ch->data ? mem_ndup (ch->data, ch->length) : NULL;
		} else {
			newch->data = ch->data ? string_dup (ch->data) : NULL;
		}
		newch->length = ch->length;
		newch->valtype = ch->valtype;

		return (void *)newch;
	}
	return NULL;
}
/*}}}*/
/*{{{  static void mcsp_constnode_hook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	dump-tree for constnode hook (name-bytes)
 */
static void mcsp_constnode_hook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	mcsp_consthook_t *ch = (mcsp_consthook_t *)hook;

	mcsp_isetindent (stream, indent);
	if (!ch) {
		fprintf (stream, "<mcspconsthook length=\"0\" value=\"(null)\" />\n");
	} else if (!ch->valtype) {
		fprintf (stream, "<mcspconsthook length=\"%d\" value=\"%s\" />\n", ch->length, ch->data ? ch->data : "(null)");
	} else if (ch->length == 4) {
		fprintf (stream, "<mcspconsthook length=\"%d\" value=\"%d\" />\n", ch->length, ch->data ? *(int *)ch->data : 0);
	} else {
		char *vstr = mkhexbuf ((unsigned char *)ch->data, ch->length);

		fprintf (stream, "<mcspconsthook length=\"%d\" hexvalue=\"%s\" />\n", ch->length, vstr);
		sfree (vstr);
	}
	return;
}
/*}}}*/


/*{{{  static int mcsp_scopein_rawname (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	scopes in a free-floating name
 */
static int mcsp_scopein_rawname (compops_t *cops, tnode_t **node, scope_t *ss)
{
	tnode_t *name = *node;
	char *rawname;
	name_t *sname = NULL;
	mcsp_lex_t *lmp;
	mcsp_scope_t *mss = (mcsp_scope_t *)ss->langpriv;

	if ((*node)->org_file && (*node)->org_file->priv) {
		lexpriv_t *lp = (lexpriv_t *)(*node)->org_file->priv;
		
		lmp = (mcsp_lex_t *)lp->langpriv;
	} else {
		lmp = NULL;
	}

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

		/* if it looks like a PROCDEF, turn into an INSTANCE -- if we're not already in an instance!*/
		if (!mss->inamescope && ((*node)->tag == mcsp.tag_PROCDEF)) {
			*node = tnode_createfrom (mcsp.tag_INSTANCE, name, *node, parser_newlistnode (NULL));
		}

		tnode_free (name);
	} else {
#if 0
fprintf (stderr, "mcsp_scopein_rawname(): unresolved name \"%s\", unbound-events = %d, mss->uvinsertlist = 0x%8.8x\n", rawname, lmp ? lmp->unboundvars : -1, (unsigned int)mss->uvinsertlist);
#endif
		if (lmp && lmp->unboundvars) {
			if (mss && mss->uvinsertlist) {
				/*{{{  add the name manually*/
				tnode_t *decl = tnode_create (mcsp.tag_UPARAM, NULL, NULL);
				tnode_t *newname;

				sname = name_addsubscopenamess (rawname, mss->uvscopemark, decl, NULL, name, ss);
				parser_addtolist (mss->uvinsertlist, decl);
				newname = tnode_createfrom (mcsp.tag_EVENT, decl, sname);
				SetNameNode (sname, newname);
				tnode_setnthsub (decl, 0, newname);

				/* and replace local node! */
				*node = newname;
				tnode_free (name);

				ss->scoped++;
				/*}}}*/
			} else {
				scope_error (name, ss, "unresolved name \"%s\" cannot be captured", rawname);
			}
		} else {
			scope_error (name, ss, "unresolved name \"%s\"", rawname);
		}
	}

	return 1;
}
/*}}}*/


/*{{{  static int mcsp_scopenode_checktail_walktree (tnode_t *node, void *arg)*/
/*
 *	checks that all tail nodes of the given tree are instances of itself,
 *	used to turn FIXPOINTs into ILOOPs (tail-recursion only)
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_scopenode_checktail_walktree (tnode_t *node, void *arg)
{
	tnode_t **tmpnodes = (tnode_t **)arg;

	if (!node) {
		nocc_internal ("mcsp_scopenode_checktail(): null node!");
		return 0;
	}
	if (node->tag == mcsp.tag_INSTANCE) {
		if (tnode_nthsubof (node, 0) != tmpnodes[1]) {
			tmpnodes[0] = (tnode_t *)0;			/* instance of something else */
		}
		return 0;
	} else if (node->tag == mcsp.tag_SEQCODE) {
		tnode_t **items;
		int nitems;

		items = parser_getlistitems (tnode_nthsubof (node, 1), &nitems);
		if (nitems > 0) {
			tnode_prewalktree (items[nitems - 1], mcsp_scopenode_checktail_walktree, arg);
		} else {
			tmpnodes[0] = (tnode_t *)0;			/* empty SEQ */
		}
		return 0;
	} else if (node->tag == mcsp.tag_ALT) {
		tnode_t **items;
		int nitems, i;

		items = parser_getlistitems (tnode_nthsubof (node, 0), &nitems);
		for (i=0; i<nitems; i++) {
			if (items[i]->tag == mcsp.tag_GUARD) {
				tnode_prewalktree (tnode_nthsubof (items[i], 1), mcsp_scopenode_checktail_walktree, arg);
			}
			if (tmpnodes[0] == (tnode_t *)0) {
				/* early getout */
				return 0;
			}
		}
		return 0;
	} else if (node->tag == mcsp.tag_PAR) {
		tmpnodes[0] = (tnode_t *)0;				/* PAR makes things complex */
		return 0;
	} else if (node->tag == mcsp.tag_THEN) {
		/* shouldn't really see this after fetrans */
		nocc_warning ("mcsp_scopenode_checktail(): found unexpected [%s]", node->tag->name);

		tnode_prewalktree (tnode_nthsubof (node, 1), mcsp_scopenode_checktail_walktree, arg);
		return 0;
	}

	/* otherwise assume that it's not a tail instance */
	tmpnodes[0] = (tnode_t *)0;
	return 0;
}
/*}}}*/
/*{{{  static int mcsp_scopenode_rminstancetail_modwalktree (tnode_t **nodep, void *arg)*/
/*
 *	removes tail instance nodes, used to turn FIXPOINTs into ILOOPs (tail-recursion only)
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_scopenode_rminstancetail_modwalktree (tnode_t **nodep, void *arg)
{
	tnode_t *iname = (tnode_t *)arg;
	tnode_t *node = *nodep;

	if (!node || !iname) {
		nocc_internal ("mcsp_scopenode_rminstancetail(): null node or instance-name!");
		return 0;
	}
	if (node->tag == mcsp.tag_INSTANCE) {
		/* turn into SKIP */
		*nodep = tnode_create (mcsp.tag_SKIP, NULL);
	} else if (node->tag == mcsp.tag_SEQCODE) {
		tnode_t **items;
		int nitems;

		items = parser_getlistitems (tnode_nthsubof (node, 1), &nitems);
		if (nitems > 0) {
			tnode_modprewalktree (&items[nitems - 1], mcsp_scopenode_rminstancetail_modwalktree, arg);
		}
		return 0;
	} else if (node->tag == mcsp.tag_ALT) {
		tnode_t **items;
		int nitems, i;

		items = parser_getlistitems (tnode_nthsubof (node, 0), &nitems);
		for (i=0; i<nitems; i++) {
			if (items[i]->tag == mcsp.tag_GUARD) {
				tnode_modprewalktree (tnode_nthsubaddr (items[i], 1), mcsp_scopenode_rminstancetail_modwalktree, arg);
			}
		}
		return 0;
	} else if (node->tag == mcsp.tag_THEN) {
		tnode_modprewalktree (tnode_nthsubaddr (node, 1), mcsp_scopenode_rminstancetail_modwalktree, arg);
		return 0;
	}
	return 0;
}
/*}}}*/
/*{{{  static int mcsp_prescope_scopenode (compops_t *cops, tnode_t **node, prescope_t *ps)*/
/*
 *	does pre-scoping on an MCSP scope node (ensures vars are a list)
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_prescope_scopenode (compops_t *cops, tnode_t **node, prescope_t *ps)
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
/*{{{  static int mcsp_scopein_scopenode (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	called to scope in an MCSP something that introduces scope (names)
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_scopein_scopenode (compops_t *cops, tnode_t **node, scope_t *ss)
{
	void *nsmark;
	tnode_t *vars = tnode_nthsubof (*node, 0);
	tnode_t **bodyptr = tnode_nthsubaddr (*node, 1);
	tnode_t **varlist;
	int nvars, i;
	ntdef_t *xtag;
	tnode_t *ntype = NULL;

	if ((*node)->tag == mcsp.tag_HIDE) {
		xtag = mcsp.tag_EVENT;
	} else if ((*node)->tag == mcsp.tag_FIXPOINT) {
		xtag = mcsp.tag_PROCDEF;
		ntype = parser_newlistnode (NULL);
	} else {
		scope_error (*node, ss, "mcsp_scopein_scopename(): can't scope [%s] ?", (*node)->tag->name);
		return 1;
	}

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

		sname = name_addscopename (rawname, *node, ntype, NULL);
		newname = tnode_createfrom (xtag, name, sname);
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
/*{{{  static int mcsp_scopeout_scopenode (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	called to scope-out an MCSP something
 */
static int mcsp_scopeout_scopenode (compops_t *cops, tnode_t **node, scope_t *ss)
{
	/* all done in scope-in */
	return 1;
}
/*}}}*/
/*{{{  static int mcsp_fetrans_scopenode (compops_t *cops, tnode_t **node, fetrans_t *fe)*/
/*
 *	does front-end transforms on a scopenode -- turns HIDE vars into declarations
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_fetrans_scopenode (compops_t *cops, tnode_t **node, fetrans_t *fe)
{
	mcsp_fetrans_t *mfe = (mcsp_fetrans_t *)fe->langpriv;
	tnode_t *t = *node;

	switch (mfe->parse) {
	case 0:
		if (t->tag == mcsp.tag_HIDE) {
			/*{{{  hiding-operator: turn into VARDECLs*/
			tnode_t *varlist = tnode_nthsubof (t, 0);
			tnode_t *process = tnode_nthsubof (t, 1);
			tnode_t **vars;
			tnode_t **top = node;
			int nvars, i;

			/* create var-declarations for each name in the list */
			vars = parser_getlistitems (varlist, &nvars);
			for (i=0; i<nvars; i++) {
				*node = tnode_createfrom (mcsp.tag_VARDECL, t, vars[i], NULL, NULL);
				node = tnode_nthsubaddr (*node, 2);
				vars[i] = NULL;
			}

			/* drop in process */
			*node = process;

			/* free up HIDE */
			tnode_setnthsub (t, 1, NULL);
			tnode_free (t);

			/* explicitly sub-walk new node */
			fetrans_subtree (top, fe);

			return 0;
			/*}}}*/
		}
		break;
	case 2:
		if (t->tag == mcsp.tag_FIXPOINT) {
			/*{{{  see if we have tail-call recursion only*/
			tnode_t *tmpnodes[2];

			/* walk body first */
			fetrans_subtree (tnode_nthsubaddr (t, 1), fe);

			tmpnodes[0] = (tnode_t *)1;
			tmpnodes[1] = tnode_nthsubof (t, 0);		/* this instance */

			if (parser_islistnode (tmpnodes[1])) {
				tmpnodes[1] = parser_getfromlist (tmpnodes[1], 0);
			}
			tnode_prewalktree (tnode_nthsubof (t, 1), mcsp_scopenode_checktail_walktree, (void *)tmpnodes);

			if (tmpnodes[0] == (tnode_t *)1) {
				/* means we can turn it into a loop :) */
				tnode_modprewalktree (tnode_nthsubaddr (t, 1), mcsp_scopenode_rminstancetail_modwalktree, (void *)tmpnodes[1]);

				tnode_free (tnode_nthsubof (t, 0));
				tnode_setnthsub (t, 0, NULL);

				*node = tnode_create (mcsp.tag_ILOOP, NULL, tnode_nthsubof (t, 1), NULL);
				tnode_setnthsub (t, 1, NULL);

				tnode_free (t);
			}

			return 0;
			/*}}}*/
		}
		break;
	}

	return 1;
}
/*}}}*/


/*{{{  static int mcsp_prescope_declnode (compops_t *cops, tnode_t **node, prescope_t *ps)*/
/*
 *	pre-scopes a process definition
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_prescope_declnode (compops_t *cops, tnode_t **node, prescope_t *ps)
{
	tnode_t **paramptr = tnode_nthsubaddr (*node, 1);
	tnode_t **params;
	int nparams, i;
	
	if (!*paramptr) {
		/* no parameters, make empty list */
		*paramptr = parser_newlistnode (NULL);
	} else if (!parser_islistnode (*paramptr)) {
		/* singleton, make list */
		tnode_t *list = parser_newlistnode (NULL);

		parser_addtolist (list, *paramptr);
		*paramptr = list;
	}

	/* now go through and turn into FPARAM nodes -- needed for allocation later */
	params = parser_getlistitems (*paramptr, &nparams);
	for (i=0; i<nparams; i++) {
		params[i] = tnode_createfrom (mcsp.tag_FPARAM, params[i], params[i]);
	}

	return 1;
}
/*}}}*/
/*{{{  static int mcsp_scopein_declnode (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	called to scope-in a process definition
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_scopein_declnode (compops_t *cops, tnode_t **node, scope_t *ss)
{
	mcsp_scope_t *mss = (mcsp_scope_t *)ss->langpriv;
	tnode_t *name = tnode_nthsubof (*node, 0);
	tnode_t **paramptr = tnode_nthsubaddr (*node, 1);
	tnode_t **bodyptr = tnode_nthsubaddr (*node, 2);
	void *nsmark;
	char *rawname;
	name_t *procname;
	tnode_t *newname;
	tnode_t *saved_uvil = mss->uvinsertlist;
	void *saved_uvsm = mss->uvscopemark;

	nsmark = name_markscope ();
	/* scope-in any parameters and walk body */
	tnode_modprepostwalktree (paramptr, scope_modprewalktree, scope_modpostwalktree, (void *)ss);
	mss->uvinsertlist = *paramptr;										/* prescope made sure it's a list */
	mss->uvscopemark = nsmark;
	tnode_modprepostwalktree (bodyptr, scope_modprewalktree, scope_modpostwalktree, (void *)ss);

	name_markdescope (nsmark);
	mss->uvinsertlist = saved_uvil;
	mss->uvscopemark = saved_uvsm;

	/* declare and scope PROCDEF name, then scope process in scope of it */
	rawname = (char *)tnode_nthhookof (name, 0);
	procname = name_addscopenamess (rawname, *node, *paramptr, NULL, ss);
	newname = tnode_createfrom (mcsp.tag_PROCDEF, name, procname);
	SetNameNode (procname, newname);
	tnode_setnthsub (*node, 0, newname);

	/* free old name, scope process */
	tnode_free (name);
	tnode_modprepostwalktree (tnode_nthsubaddr (*node, 3), scope_modprewalktree, scope_modpostwalktree, (void *)ss);
	ss->scoped++;

	return 0;
}
/*}}}*/
/*{{{  static int mcsp_scopeout_declnode (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	called to scope-out a process definition
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_scopeout_declnode (compops_t *cops, tnode_t **node, scope_t *ss)
{
	return 1;
}
/*}}}*/
/*{{{  static int mcsp_fetrans_declnode (compops_t *cops, tnode_t **node, fetrans_t *fe)*/
/*
 *	does front-end transforms on a process declaration node
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_fetrans_declnode (compops_t *cops, tnode_t **node, fetrans_t *fe)
{
	mcsp_fetrans_t *mfe = (mcsp_fetrans_t *)fe->langpriv;

	switch (mfe->parse) {
	case 0:
		if ((*node)->tag == mcsp.tag_PROCDECL) {
			void *saved_uvil = mfe->uvinsertlist;

			mfe->uvinsertlist = tnode_nthsubof (*node, 1);
			fetrans_subtree (tnode_nthsubaddr (*node, 1), fe);		/* params */
			fetrans_subtree (tnode_nthsubaddr (*node, 2), fe);		/* body */
			mfe->uvinsertlist = saved_uvil;
			fetrans_subtree (tnode_nthsubaddr (*node, 3), fe);		/* in-scope body */

			return 0;
		}
		break;
	}

	return 1;
}
/*}}}*/
/*{{{  static int mcsp_betrans_declnode (compops_t *cops, tnode_t **node, betrans_t *be)*/
/*
 *	does back-end transforms on a process declaration node
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_betrans_declnode (compops_t *cops, tnode_t **node, betrans_t *be)
{
	return 1;
}
/*}}}*/
/*{{{  static int mcsp_namemap_declnode (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	does name-mapping for a process declaration node
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_namemap_declnode (compops_t *cops, tnode_t **node, map_t *map)
{
	tnode_t *blk;
	/* tnode_t *saved_blk = map->thisblock;
	tnode_t **saved_params = map->thisprocparams; */
	tnode_t **paramsptr;
	tnode_t *tmpname;

	blk = map->target->newblock (tnode_nthsubof (*node, 2), map, tnode_nthsubof (*node, 1), map->lexlevel + 1);
	map_pushlexlevel (map, blk, tnode_nthsubaddr (*node, 1));
	/* map->thisblock = blk;
	 * map->thisprocparams = tnode_nthsubaddr (*node, 1);
	 * map->lexlevel++; */

	/* map formal params and body */
	paramsptr = tnode_nthsubaddr (*node, 1);
	map_submapnames (paramsptr, map);
	map_submapnames (tnode_nthsubaddr (blk, 0), map);		/* do this under the back-end block */

	map_poplexlevel (map);
	/* map->lexlevel--;
	 * map->thisblock = saved_blk;
	 * map->thisprocparams = saved_params; */

	/* insert the BLOCK node before the body of the process */
	tnode_setnthsub (*node, 2, blk);

	/* map scoped body */
	map_submapnames (tnode_nthsubaddr (*node, 3), map);

	/* add static-link, etc. if required and return-address */
	if (!parser_islistnode (*paramsptr)) {
		tnode_t *flist = parser_newlistnode (NULL);

		parser_addtolist (flist, *paramsptr);
		*paramsptr = flist;
	}

	tmpname = map->target->newname (tnode_create (mcsp.tag_HIDDENPARAM, NULL, tnode_create (mcsp.tag_RETURNADDRESS, NULL)), NULL, map,
			map->target->pointersize, 0, 0, 0, map->target->pointersize, 0);
	parser_addtolist_front (*paramsptr, tmpname);

	return 0;
}
/*}}}*/
/*{{{  static int mcsp_precode_declnode (compops_t *cops, tnode_t **node, codegen_t *cgen)*/
/*
 *	does pre-code on a process declaration node
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_precode_declnode (compops_t *cops, tnode_t **node, codegen_t *cgen)
{
	tnode_t *t = *node;
	tnode_t *name = tnode_nthsubof (t, 0);

	/* walk body */
	codegen_subprecode (tnode_nthsubaddr (t, 2), cgen);

	codegen_precode_seenproc (cgen, tnode_nthnameof (name, 0), t);

	/* pre-code stuff following declaration */
	codegen_subprecode (tnode_nthsubaddr (t, 3), cgen);

	return 0;
}
/*}}}*/
/*{{{  static int mcsp_codegen_declnode (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	does code-generation for a process definition
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_codegen_declnode (compops_t *cops, tnode_t *node, codegen_t *cgen)
{
	tnode_t *body = tnode_nthsubof (node, 2);
	tnode_t *name = tnode_nthsubof (node, 0);
	int ws_size, ws_offset, adjust;
	name_t *pname;

	cgen->target->be_getblocksize (body, &ws_size, &ws_offset, NULL, NULL, &adjust, NULL);
	pname = tnode_nthnameof (name, 0);
	codegen_callops (cgen, comment, "PROCESS %s = %d,%d,%d", pname->me->name, ws_size, ws_offset, adjust);
	codegen_callops (cgen, setwssize, ws_size, adjust);
	codegen_callops (cgen, setvssize, 0);
	codegen_callops (cgen, setmssize, 0);
	codegen_callops (cgen, setnamelabel, pname);
	codegen_callops (cgen, procnameentry, pname);
	codegen_callops (cgen, debugline, node);

	/* generate body */
	codegen_subcodegen (body, cgen);
	codegen_callops (cgen, procreturn, adjust);

	/* generate code following declaration */
	codegen_subcodegen (tnode_nthsubof (node, 3), cgen);

	return 0;
}
/*}}}*/


/*{{{  static int mcsp_namemap_leafproc (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	does name-mapping for a leaf-process
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_namemap_leafproc (compops_t *cops, tnode_t **node, map_t *map)
{
	return 0;
}
/*}}}*/
/*{{{  static int mcsp_codegen_leafproc (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	does code-generation for a leaf-process
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_codegen_leafproc (compops_t *cops, tnode_t *node, codegen_t *cgen)
{
	if (node->tag == mcsp.tag_SKIP) {
		/* nothing :) */
		return 0;
	} else if (node->tag == mcsp.tag_STOP) {
		codegen_callops (cgen, tsecondary, I_SETERR);
		return 0;
	} else if ((node->tag == mcsp.tag_DIV) || (node->tag == mcsp.tag_CHAOS)) {
		/* just stop for now -- maybe get creative later ;) */
		codegen_callops (cgen, tsecondary, I_SETERR);
		return 0;
	}
	codegen_warning (cgen, "don\'t know how to generate code for [%s]", node->tag->name);
	return 1;
}
/*}}}*/


/*{{{  static int mcsp_fetrans_namenode (compops_t *cops, tnode_t **node, fetrans_t *fe)*/
/*
 *	does front-end transformation on an EVENT
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_fetrans_namenode (compops_t *cops, tnode_t **node, fetrans_t *fe)
{
	mcsp_fetrans_t *mfe = (mcsp_fetrans_t *)fe->langpriv;
	tnode_t *t = *node;

	switch (mfe->parse) {
	case 0:
		if (t->tag == mcsp.tag_EVENT) {
			/*{{{  turn event into SYNC -- not a declaration occurence*/
			*node = tnode_create (mcsp.tag_SYNC, NULL, *node, NULL);
			return 0;
			/*}}}*/
		}
		break;
	}

	return 1;
}
/*}}}*/
/*{{{  static int mcsp_namemap_namenode (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	does name-mapping for namenodes (unbound references)
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_namemap_namenode (compops_t *cops, tnode_t **node, map_t *map)
{
	tnode_t *bename = (tnode_t *)tnode_getchook (*node, map->mapchook);
	tnode_t *tname;

	if (bename) {
		tname = map->target->newnameref (bename, map);
		*node = tname;
	}

	return 0;
}
/*}}}*/
/*{{{  static void mcsp_namenode_initevent (tnode_t *node, codegen_t *cgen, void *arg)*/
/*
 *	generates code to initialise an EVENT
 */
static void mcsp_namenode_initevent (tnode_t *node, codegen_t *cgen, void *arg)
{
	int ws_offs;

	cgen->target->be_getoffsets (node, &ws_offs, NULL, NULL, NULL);
	/* FIXME: this needs tidying */
	codegen_callops (cgen, loadconst, 5 * cgen->target->slotsize);
	codegen_callops (cgen, tsecondary, I_MALLOC);
	codegen_callops (cgen, storelocal, ws_offs);

	/* initialise */
	codegen_callops (cgen, loadconst, 1);
	codegen_callops (cgen, loadlocal, ws_offs);
	codegen_callops (cgen, storenonlocal, 0);		/* ref-count */

	codegen_callops (cgen, loadconst, 1);
	codegen_callops (cgen, loadlocal, ws_offs);
	codegen_callops (cgen, storenonlocal, 4);		/* enrolled-count */

	codegen_callops (cgen, loadconst, 1);
	codegen_callops (cgen, loadlocal, ws_offs);
	codegen_callops (cgen, storenonlocal, 8);		/* count-down */

	codegen_callops (cgen, tsecondary, I_NULL);
	codegen_callops (cgen, loadlocal, ws_offs);
	codegen_callops (cgen, storenonlocal, 12);		/* fptr */

	codegen_callops (cgen, tsecondary, I_NULL);
	codegen_callops (cgen, loadlocal, ws_offs);
	codegen_callops (cgen, storenonlocal, 16);		/* bptr */

	return;
}
/*}}}*/
/*{{{  static void mcsp_namenode_finalevent (tnode_t *node, codegen_t *cgen, void *arg)*/
/*
 *	generates code to destroy an EVENT
 */
static void mcsp_namenode_finalevent (tnode_t *node, codegen_t *cgen, void *arg)
{
	int ws_offs;

	cgen->target->be_getoffsets (node, &ws_offs, NULL, NULL, NULL);

	codegen_callops (cgen, loadlocal, ws_offs);
	codegen_callops (cgen, tsecondary, I_MRELEASE);
	codegen_callops (cgen, tsecondary, I_NULL);
	codegen_callops (cgen, storelocal, ws_offs);

	return;
}
/*}}}*/
/*{{{  static int mcsp_namenode_initialising_decl (langops_t *lops, tnode_t *node, tnode_t *bename, map_t *map)*/
/*
 *	used to initialise EVENTs
 */
static int mcsp_namenode_initialising_decl (langops_t *lops, tnode_t *node, tnode_t *bename, map_t *map)
{
	if (node->tag == mcsp.tag_EVENT) {
		codegen_setinithook (bename, mcsp_namenode_initevent, (void *)node);
		codegen_setfinalhook (bename, mcsp_namenode_finalevent, (void *)node);
	}
	return 0;
}
/*}}}*/


/*{{{  static int mcsp_fetrans_cnode (compops_t *cops, tnode_t **node, fetrans_t *fe)*/
/*
 *	does front-end transforms on SEQ/PAR nodes (pass 1+ only)
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_fetrans_cnode (compops_t *cops, tnode_t **node, fetrans_t *fe)
{
	mcsp_fetrans_t *mfe = (mcsp_fetrans_t *)fe->langpriv;
	tnode_t *t = *node;

	switch (mfe->parse) {
	case 0:
		/* nothing in this pass! */
		break;
	case 1:
		if (t->tag == mcsp.tag_SEQCODE) {
			/*{{{  flatten SEQCODEs*/
			tnode_t *slist = tnode_nthsubof (t, 1);
			tnode_t **procs;
			int nprocs, i;
			tnode_t *newlist = parser_newlistnode (NULL);

			procs = parser_getlistitems (slist, &nprocs);
			for (i=0; i<nprocs; i++) {
				if (procs[i] && (procs[i]->tag == mcsp.tag_SEQCODE)) {
					tnode_t *xslist;
					tnode_t **xprocs;
					int nxprocs, j;

					/* flatten */
					fetrans_subtree (&procs[i], fe);

					xslist = tnode_nthsubof (procs[i], 1);
					xprocs = parser_getlistitems (xslist, &nxprocs);
					for (j=0; j<nxprocs; j++) {
						parser_addtolist (newlist, xprocs[j]);
						xprocs[j] = NULL;
					}

					/* assume we got them all */
					tnode_free (procs[i]);
					procs[i] = NULL;
				} else {
					/* move over to new list after sub-fetrans (preserves order) */
					fetrans_subtree (&procs[i], fe);

					parser_addtolist (newlist, procs[i]);
					procs[i] = NULL;
				}
			}

			/* park new list and free old */
			tnode_setnthsub (t, 1, newlist);
			tnode_free (slist);

			return 0;
			/*}}}*/
		}
		break;
	case 2:
		if (t->tag == mcsp.tag_PARCODE) {
			/*{{{  build the alphabet for this node, store unbound ones in parent*/
			mcsp_alpha_t *savedalpha = mfe->curalpha;
			mcsp_alpha_t *a_lhs, *a_rhs;
			mcsp_alpha_t *paralpha;
			tnode_t **subnodes;
			int nsnodes;

			/* always two subtrees */
			subnodes = parser_getlistitems (tnode_nthsubof (t, 1), &nsnodes);
			if (nsnodes != 2) {
				nocc_internal ("mcsp_fetrans_dopnode(): pass2 for PARCODE: have %d items", nsnodes);
				return 0;
			}

			mfe->curalpha = mcsp_newalpha ();
			fetrans_subtree (&subnodes[0], fe);
			a_lhs = mfe->curalpha;

			mfe->curalpha = mcsp_newalpha ();
			fetrans_subtree (&subnodes[1], fe);
			a_rhs = mfe->curalpha;

			mfe->curalpha = savedalpha;
			paralpha = mcsp_newalpha ();

			mcsp_sortandmergealpha (a_lhs, a_rhs, paralpha, mfe->curalpha);

			mcsp_freealpha (a_lhs);
			mcsp_freealpha (a_rhs);

			tnode_setnthhook (t, 0, (void *)paralpha);
			return 0;
			/*}}}*/
		}
		break;
	}
	return 1;
}
/*}}}*/
/*{{{  static int mcsp_namemap_cnode (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	does mapping for a constructor node (SEQ/PAR)
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_namemap_cnode (compops_t *cops, tnode_t **node, map_t *map)
{
	if ((*node)->tag == mcsp.tag_SEQCODE) {
		/* nothing special */
		return 1;
	} else if ((*node)->tag == mcsp.tag_PARCODE) {
		/*{{{  map PAR bodies*/
		tnode_t *body = tnode_nthsubof (*node, 1);
		tnode_t **bodies;
		int nbodies, i;
		tnode_t *parnode = *node;
		mcsp_alpha_t *alpha = (mcsp_alpha_t *)tnode_nthhookof (*node, 0);		/* events the _two_ processes synchronise on */

		if (!parser_islistnode (body)) {
			nocc_internal ("mcsp_namemap_cnode(): body of PARCODE not list");
			return 1;
		}

		/* if we have an alphabet, map these first (done in PAR context) */
		if (alpha) {
			map_submapnames (&(alpha->elist), map);
#if 0
fprintf (stderr, "mcsp_namemap_cnode(): mapped alphabet, got list:\n");
tnode_dumptree (alpha->elist, 1, stderr);
#endif
		}

		bodies = parser_getlistitems (body, &nbodies);
		for (i=0; i<nbodies; i++) {
			/*{{{  turn body into back-end block*/
			tnode_t *blk, *parbodyspace;
			/* tnode_t *saved_blk = map->thisblock;
			tnode_t **saved_params = map->thisprocparams; */

			blk = map->target->newblock (bodies[i], map, NULL, map->lexlevel + 1);
			map_pushlexlevel (map, blk, NULL);
			/* map->thisblock = blk;
			 * map->thisprocparams = NULL;
			 * map->lexlevel++; */

			/* map body */
			map_submapnames (&(bodies[i]), map);
			parbodyspace = map->target->newname (tnode_create (mcsp.tag_PARSPACE, NULL), bodies[i], map, 0, 16, 0, 0, 0, 0);	/* FIXME! */
			*(map->target->be_blockbodyaddr (blk)) = parbodyspace;

			map_poplexlevel (map);
			/* map->lexlevel--;
			 * map->thisblock = saved_blk;
			 * map->thisprocparams = saved_params; */

			/* make block node the individual PAR process */
			bodies[i] = blk;
			/*}}}*/
		}

		if (nbodies > 1) {
			/*{{{  make space for PAR*/
			tnode_t *bename, *bodyref, *blist;
			tnode_t *fename = tnode_create (mcsp.tag_PARSPACE, NULL);

			blist = parser_newlistnode (NULL);
			for (i=0; i<nbodies; i++) {
				parser_addtolist (blist, bodies[i]);
			}
			bodyref = map->target->newblockref (blist, *node, map);
			bename = map->target->newname (fename, bodyref, map, map->target->aws.as_par, 0, 0, 0, 0, 0);
			tnode_setchook (fename, map->mapchook, (void *)bename);

			*node = bename;

			tnode_setnthsub (parnode, 0, map->target->newnameref (bename, map));
			/*}}}*/
			/*{{{  if we have an alphabet, link in with alloc:extravars*/
			if (alpha) {
				tnode_setchook (bename, map->allocevhook, alpha->elist);
			}
			/*}}}*/
		}

		/*}}}*/
	} else {
		nocc_internal ("mcsp_namemap_cnode(): don\'t know how to handle tag [%s]", (*node)->tag->name);
	}
	return 0;
}
/*}}}*/
/*{{{  static int mcsp_codegen_cnode (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	does code-generation for a constructor node (SEQ/PAR)
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_codegen_cnode (compops_t *cops, tnode_t *node, codegen_t *cgen)
{
	if (node->tag == mcsp.tag_SEQCODE) {
		return 1;
	} else if (node->tag == mcsp.tag_PARCODE) {
		/*{{{  generate code for PAR*/
		tnode_t *body = tnode_nthsubof (node, 1);
		tnode_t **bodies;
		int nbodies, i;
		int joinlab = codegen_new_label (cgen);
		int pp_wsoffs = 0;
		tnode_t *parspaceref = tnode_nthsubof (node, 0);
		mcsp_alpha_t *alpha = (mcsp_alpha_t *)tnode_nthhookof (node, 0);

		bodies = parser_getlistitems (body, &nbodies);
		/*{{{  if we've got an alphabet, up ref-counts by (nbodies), enroll-counts by (nbodies - 1), down-counts by (nbodies - 1)*/
		if (alpha) {
			tnode_t **events;
			int nevents, j;

			codegen_callops (cgen, comment, "start barrier enrolls");
			events = parser_getlistitems (alpha->elist, &nevents);
			for (j=0; j<nevents; j++) {
				codegen_callops (cgen, loadpointer, events[j], 0);
				codegen_callops (cgen, loadnonlocal, 0);		/* refcount */
				codegen_callops (cgen, loadconst, nbodies);
				codegen_callops (cgen, tsecondary, I_SUM);
				codegen_callops (cgen, loadpointer, events[j], 0);
				codegen_callops (cgen, storenonlocal, 0);		/* refcount */

				codegen_callops (cgen, loadpointer, events[j], 0);
				codegen_callops (cgen, loadnonlocal, 4);		/* enroll-count */
				codegen_callops (cgen, loadconst, nbodies - 1);
				codegen_callops (cgen, tsecondary, I_SUM);
				codegen_callops (cgen, loadpointer, events[j], 0);
				codegen_callops (cgen, storenonlocal, 4);		/* enroll-count */

				codegen_callops (cgen, loadpointer, events[j], 0);
				codegen_callops (cgen, loadnonlocal, 8);		/* down-count */
				codegen_callops (cgen, loadconst, nbodies - 1);
				codegen_callops (cgen, tsecondary, I_SUM);
				codegen_callops (cgen, loadpointer, events[j], 0);
				codegen_callops (cgen, storenonlocal, 8);		/* down-count */
			}
			codegen_callops (cgen, comment, "finish barrier enrolls");
		}
		/*}}}*/
		/*{{{  PAR setup*/
		codegen_callops (cgen, comment, "BEGIN PAR SETUP");
		for (i=0; i<nbodies; i++) {
			int ws_size, vs_size, ms_size;
			int ws_offset, adjust, elab;
			tnode_t *statics = tnode_nthsubof (bodies[i], 1);

			codegen_check_beblock (bodies[i], cgen, 1);
			cgen->target->be_getblocksize (bodies[i], &ws_size, &ws_offset, &vs_size, &ms_size, &adjust, &elab);

			/*{{{  setup statics in workspace of PAR process*/
			if (statics && parser_islistnode (statics)) {
				int nitems, p, wsoff;
				tnode_t **items = parser_getlistitems (statics, &nitems);

				for (p=nitems - 1, wsoff = pp_wsoffs-4; p>=0; p--, wsoff -= 4) {
					codegen_callops (cgen, loadparam, items[p], PARAM_REF);
					codegen_callops (cgen, storelocal, wsoff);
				}
			} else if (statics) {
				codegen_callops (cgen, loadparam, statics, PARAM_REF);
				codegen_callops (cgen, storelocal, pp_wsoffs-4);
			}
			/*}}}*/

			pp_wsoffs -= ws_size;
		}
		/*{{{  setup local PAR workspace*/
		codegen_callops (cgen, loadconst, nbodies + 1);		/* par-count */
		codegen_callops (cgen, storename, parspaceref, 4);
		codegen_callops (cgen, loadconst, 0);			/* priority */
		codegen_callops (cgen, storename, parspaceref, 8);
		codegen_callops (cgen, loadlabaddr, joinlab);		/* join-lab */
		codegen_callops (cgen, storename, parspaceref, 0);
		/*}}}*/
		pp_wsoffs = 0;
		for (i=0; i<nbodies; i++) {
			int ws_size, vs_size, ms_size;
			int ws_offset, adjust, elab;

			codegen_check_beblock (bodies[i], cgen, 1);
			cgen->target->be_getblocksize (bodies[i], &ws_size, &ws_offset, &vs_size, &ms_size, &adjust, &elab);

			/*{{{  start PAR process*/
			codegen_callops (cgen, loadlabaddr, elab);
			codegen_callops (cgen, loadlocalpointer, pp_wsoffs - adjust);
			codegen_callops (cgen, tsecondary, I_STARTP);
			/*}}}*/

			pp_wsoffs -= ws_size;
		}
		codegen_callops (cgen, comment, "END PAR SETUP");
		/*}}}*/
		/*{{{  end process doing PAR*/
		codegen_callops (cgen, loadpointer, parspaceref, 0);
		codegen_callops (cgen, tsecondary, I_ENDP);
		/*}}}*/
		pp_wsoffs = 0;
		for (i=0; i<nbodies; i++) {
			/*{{{  PAR body*/
			int ws_size, vs_size, ms_size;
			int ws_offset, adjust, elab;

			cgen->target->be_getblocksize (bodies[i], &ws_size, &ws_offset, &vs_size, &ms_size, &adjust, &elab);
			codegen_callops (cgen, comment, "PAR = %d,%d,%d,%d,%d", ws_size, ws_offset, vs_size, ms_size, adjust);

			codegen_subcodegen (bodies[i], cgen);

			codegen_callops (cgen, loadpointer, parspaceref, pp_wsoffs + adjust);
			codegen_callops (cgen, tsecondary, I_ENDP);
			/*}}}*/

			pp_wsoffs += ws_size;
		}
		/*}}}*/
		/*{{{  PAR cleanup*/
		codegen_callops (cgen, comment, "END PAR BODIES");
		codegen_callops (cgen, setlabel, joinlab);
		/*}}}*/
		/*{{{  if we've got an alphabet, down ref-counts by (nbodies), enroll-counts by (nbodies - 1), down-counts by (nbodies - 1)*/
		if (alpha) {
			tnode_t **events;
			int nevents, j;

			codegen_callops (cgen, comment, "start barrier resigns");
			events = parser_getlistitems (alpha->elist, &nevents);
			for (j=0; j<nevents; j++) {
				codegen_callops (cgen, loadpointer, events[j], 0);
				codegen_callops (cgen, loadnonlocal, 0);		/* refcount */
				codegen_callops (cgen, loadconst, nbodies);
				codegen_callops (cgen, tsecondary, I_DIFF);
				codegen_callops (cgen, loadpointer, events[j], 0);
				codegen_callops (cgen, storenonlocal, 0);		/* refcount */

				codegen_callops (cgen, loadpointer, events[j], 0);
				codegen_callops (cgen, loadnonlocal, 4);		/* enroll-count */
				codegen_callops (cgen, loadconst, nbodies - 1);
				codegen_callops (cgen, tsecondary, I_DIFF);
				codegen_callops (cgen, loadpointer, events[j], 0);
				codegen_callops (cgen, storenonlocal, 4);		/* enroll-count */

				codegen_callops (cgen, loadpointer, events[j], 0);
				codegen_callops (cgen, loadnonlocal, 8);		/* down-count */
				codegen_callops (cgen, loadconst, nbodies - 1);
				codegen_callops (cgen, tsecondary, I_DIFF);
				codegen_callops (cgen, loadpointer, events[j], 0);
				codegen_callops (cgen, storenonlocal, 8);		/* down-count */
			}
			codegen_callops (cgen, comment, "finish barrier resigns");
		}
		/*}}}*/
	} else {
		codegen_error (cgen, "mcsp_codegen_cnode(): how to handle [%s] ?", node->tag->name);
	}
	return 0;
}
/*}}}*/

/*{{{  static int mcsp_fetrans_actionnode (compops_t *cops, tnode_t **node, fetrans_t *fe)*/
/*
 *	does front-end transforms for an action-node
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_fetrans_actionnode (compops_t *cops, tnode_t **node, fetrans_t *fe)
{
	mcsp_fetrans_t *mfe = (mcsp_fetrans_t *)fe->langpriv;
	tnode_t *t = *node;

	switch (mfe->parse) {
	case 0:
	case 1:
		/* nothing in these passes! */
		break;
	case 2:
		if (t->tag == mcsp.tag_SYNC) {
			/*{{{  add event to list*/
			if (mfe->curalpha) {
				mcsp_addtoalpha (mfe->curalpha, tnode_nthsubof (t, 0));
			}
			/*}}}*/
		}
		break;
	}
	return 1;
}
/*}}}*/
/*{{{  static int mcsp_betrans_actionnode (compops_t *cops, tnode_t **node, betrans_t *be)*/
/*
 *	does back-end transforms for an action-node
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_betrans_actionnode (compops_t *cops, tnode_t **node, betrans_t *be)
{
	tnode_t *t = *node;

	if (t->tag == mcsp.tag_SYNC) {
		/* we need to turn this into a single-guard ALT */
		tnode_t *altnode, *guard, *glist, *event;

		event = tnode_nthsubof (t, 0);
		guard = tnode_createfrom (mcsp.tag_GUARD, t, event, tnode_create (mcsp.tag_SKIP, NULL));
		glist = parser_newlistnode (NULL);
		parser_addtolist (glist, guard);
		altnode = tnode_createfrom (mcsp.tag_ALT, t, glist, NULL);

		tnode_setnthsub (t, 0, NULL);
		tnode_free (t);
		*node = altnode;

		betrans_subtree (node, be);
		return 0;
	}
	return 1;
}
/*}}}*/
/*{{{  static int mcsp_namemap_actionnode (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	does name-mapping for an action-node
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_namemap_actionnode (compops_t *cops, tnode_t **node, map_t *map)
{
	tnode_t *t = *node;

	if (t->tag == mcsp.tag_SYNC) {
		nocc_internal ("mcsp_namemap_actionnode(): should not see SYNC here!");
		return 0;
	} else if (t->tag == mcsp.tag_CHANWRITE) {
		tnode_t *bename;
		tnode_t **opp;
		int aslots = 0;		/* above workspace slots that we might need */

		/* do channel (and argument if not an event) */
		map_submapnames (tnode_nthsubaddr (*node, 0), map);

		opp = tnode_nthsubaddr (*node, 1);
		if (*opp && ((*opp)->tag != mcsp.tag_EVENT)) {
			map_submapnames (opp, map);
		} else if (*opp) {
			/* slightly crafty -- turn operand into a back-end constant containing the EVENT's name */
			name_t *evname = tnode_nthnameof (*opp, 0);
			char *thename = NameNameOf (evname);
			char *localname = (char *)smalloc (strlen (thename) + 3);

			sprintf (localname, "%s\n", thename);
			*opp = map->target->newconst (*opp, map, (void *)localname, strlen (localname) + 1);		/* null terminator for free */
			sfree (localname);
			aslots = 1;											/* need a counter for output */
		}

		/* make space for output */
		bename = map->target->newname (*node, NULL, map, aslots * map->target->slotsize, map->target->bws.ds_io, 0, 0, 0, 0);
		*node = bename;

		return 0;
	}

	return 1;
}
/*}}}*/
/*{{{  static int mcsp_codegen_actionnode (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	does code-generation for an action-node
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_codegen_actionnode (compops_t *cops, tnode_t *node, codegen_t *cgen)
{
	if (node->tag == mcsp.tag_SYNC) {
		codegen_error (cgen, "mcsp_codegen_actionnode(): cannot generate code for SYNC!");
		return 0;
	} else if (node->tag == mcsp.tag_CHANWRITE) {
		tnode_t *chan = tnode_nthsubof (node, 0);
		tnode_t *data = tnode_nthsubof (node, 1);
		int tempslot = 0;				/* reserved a slot -- should really be_getoffsets for it */
		int looplab, skiplab;

		/* FIXME: this is crude.. */
		looplab = codegen_new_label (cgen);
		skiplab = codegen_new_label (cgen);

		codegen_callops (cgen, loadconst, 0);
		codegen_callops (cgen, storelocal, tempslot);

		codegen_callops (cgen, setlabel, looplab);
		codegen_callops (cgen, loadpointer, data, 0);
		codegen_callops (cgen, loadlocal, tempslot);
		codegen_callops (cgen, tsecondary, I_SUM);
		codegen_callops (cgen, tsecondary, I_LB);
		codegen_callops (cgen, branch, I_CJ, skiplab);		/* if we saw the NULL terminator */
		codegen_callops (cgen, trashistack);

		codegen_callops (cgen, loadpointer, data, 0);
		codegen_callops (cgen, loadlocal, tempslot);
		codegen_callops (cgen, tsecondary, I_SUM);

		codegen_callops (cgen, loadpointer, chan, 0);
		codegen_callops (cgen, loadconst, 1);			/* BYTE output only! */
		codegen_callops (cgen, tsecondary, I_OUT);

		codegen_callops (cgen, loadlocal, tempslot);
		codegen_callops (cgen, loadconst, 1);
		codegen_callops (cgen, tsecondary, I_SUM);
		codegen_callops (cgen, storelocal, tempslot);

		codegen_callops (cgen, branch, I_J, looplab);

		codegen_callops (cgen, setlabel, skiplab);

		return 0;
	}
	codegen_warning (cgen, "don\'t know how to generate code for [%s]", node->tag->name);
	return 1;
}
/*}}}*/

/*{{{  static int mcsp_namemap_loopnode (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	does name-mapping for a loopnode
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_namemap_loopnode (compops_t *cops, tnode_t **node, map_t *map)
{
	tnode_t *t = *node;

	if (t->tag == mcsp.tag_ILOOP) {
		/*{{{  loop*/
		tnode_t **condp = tnode_nthsubaddr (t, 1);

		if (*condp) {
			map_submapnames (condp, map);
		}
		map_submapnames (tnode_nthsubaddr (t, 0), map);

		return 0;
		/*}}}*/
	} else if (t->tag == mcsp.tag_PRIDROP) {
		/*{{{  drop priority*/
		/* allocate a temporary anyway, not used yet */
		*node = map->target->newname (t, NULL, map, map->target->slotsize, map->target->bws.ds_min, 0, 0, 0, 0);

		/* map body */
		map_submapnames (tnode_nthsubaddr (t, 0), map);

		return 0;
		/*}}}*/
	}
	return 1;
}
/*}}}*/
/*{{{  static int mcsp_codegen_loopnode (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	does code-generation for a loopnode
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_codegen_loopnode (compops_t *cops, tnode_t *node, codegen_t *cgen)
{
	if (node->tag == mcsp.tag_ILOOP) {
		/*{{{  infinite loop!*/
		int looplab = codegen_new_label (cgen);

		codegen_callops (cgen, setlabel, looplab);
		codegen_subcodegen (tnode_nthsubof (node, 0), cgen);
		codegen_callops (cgen, branch, I_J, looplab);

		return 0;
		/*}}}*/
	} else if (node->tag == mcsp.tag_PRIDROP) {
		/*{{{  drop priority*/
		codegen_callops (cgen, tsecondary, I_GETPRI);
		codegen_callops (cgen, loadconst, 1);
		codegen_callops (cgen, tsecondary, I_SUM);
		codegen_callops (cgen, tsecondary, I_SETPRI);

		codegen_subcodegen (tnode_nthsubof (node, 0), cgen);

		codegen_callops (cgen, tsecondary, I_GETPRI);
		codegen_callops (cgen, loadconst, 1);
		codegen_callops (cgen, tsecondary, I_DIFF);
		codegen_callops (cgen, tsecondary, I_SETPRI);

		return 0;
		/*}}}*/
	}
	return 1;
}
/*}}}*/

/*{{{  static int mcsp_scopein_replnode (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	called to scope-in a replicator node
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_scopein_replnode (compops_t *cops, tnode_t **node, scope_t *ss)
{
	tnode_t *name = tnode_nthsubof (*node, 1);
	tnode_t **bodyptr = tnode_nthsubaddr (*node, 0);
	void *nsmark;
	char *rawname;
	name_t *replname;
	tnode_t *newname;

	if (!name && !tnode_nthsubof (*node, 2)) {
		/* just have a length expression, nothing to scope in */
		tnode_modprepostwalktree (tnode_nthsubaddr (*node, 3), scope_modprewalktree, scope_modpostwalktree, (void *)ss);
		tnode_modprepostwalktree (bodyptr, scope_modprewalktree, scope_modpostwalktree, (void *)ss);
	} else {
		/* scope the start and end expressions */
		tnode_modprepostwalktree (tnode_nthsubaddr (*node, 2), scope_modprewalktree, scope_modpostwalktree, (void *)ss);
		tnode_modprepostwalktree (tnode_nthsubaddr (*node, 3), scope_modprewalktree, scope_modpostwalktree, (void *)ss);

		nsmark = name_markscope ();
		/* scope in replicator name and walk body */
		rawname = (char *)tnode_nthhookof (name, 0);
		replname = name_addscopenamess (rawname, *node, NULL, NULL, ss);
		newname = tnode_createfrom (mcsp.tag_VAR, name, replname);
		SetNameNode (replname, newname);
		tnode_setnthsub (*node, 1, newname);

		/* free old name, scope body */
		tnode_free (name);

		tnode_modprepostwalktree (bodyptr, scope_modprewalktree, scope_modpostwalktree, (void *)ss);

		name_markdescope (nsmark);
	}

	return 0;
}
/*}}}*/
/*{{{  static int mcsp_scopeout_replnode (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	called to scope-out a replicator node
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_scopeout_replnode (compops_t *cops, tnode_t **node, scope_t *ss)
{
	return 1;
}
/*}}}*/
/*{{{  static int mcsp_fetrans_replnode (compops_t *cops, tnode_t **node, fetrans_t *fe)*/
/*
 *	does front-end transform for a replnode
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_fetrans_replnode (compops_t *cops, tnode_t **node, fetrans_t *fe)
{
	tnode_t *rname = tnode_nthsubof (*node, 1);
	tnode_t **rstartptr = tnode_nthsubaddr (*node, 2);
	tnode_t **rendptr = tnode_nthsubaddr (*node, 3);

	fetrans_subtree (tnode_nthsubaddr (*node, 0), fe);			/* fetrans body */
	if (!rname && !(*rstartptr)) {
		/* only got replicator length, start start to constant 1 */
		*rstartptr = constprop_newconst (CONST_INT, NULL, NULL, 1);
	}
	if (rname) {
		fetrans_subtree (tnode_nthsubaddr (*node, 1), fe);		/* fetrans name */
	}
	/* trans start/end expressions */
	fetrans_subtree (rstartptr, fe);
	fetrans_subtree (rendptr, fe);

	return 0;
}
/*}}}*/
/*{{{  static int mcsp_namemap_replnode (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	does name-mapping for a replnode
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_namemap_replnode (compops_t *cops, tnode_t **node, map_t *map)
{
	if ((*node)->tag == mcsp.tag_REPLSEQ) {
		tnode_t **rptr = tnode_nthsubaddr (*node, 1);
		tnode_t *fename;
		tnode_t *nameref = NULL;

		map_submapnames (tnode_nthsubaddr (*node, 0), map);		/* map body */
		map_submapnames (tnode_nthsubaddr (*node, 2), map);		/* map start expression */
		map_submapnames (tnode_nthsubaddr (*node, 3), map);		/* map end expression */

		/* reserve two words for replicator */
		if (*rptr) {
			fename = *rptr;
			*rptr = NULL;
		} else {
			fename = tnode_create (mcsp.tag_LOOPSPACE, NULL);
		}

		*node = map->target->newname (fename, *node, map, 2 * map->target->intsize, 0, 0, 0, map->target->intsize, 0);
		tnode_setchook (fename, map->mapchook, (void *)*node);

		/* transform replicator name into a reference to the space just reserved */
		nameref = map->target->newnameref (*node, map);
		/* *rptr = nameref; */
#if 0
fprintf (stderr, "got nameref for REPLSEQ space:\n");
tnode_dumptree (nameref, 1, stderr);
fprintf (stderr, "*rptr is:\n");
tnode_dumptree (*rptr, 1, stderr);
#endif
		/* *rptr = map->target->newnameref (*node, map); */
		if (*rptr) {
			tnode_free (*rptr);
		}
		*rptr = nameref;

		return 0;
	}
	return 1;
}
/*}}}*/
/*{{{  static int mcsp_codegen_replnode (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	does code-generation for a replnode
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_codegen_replnode (compops_t *cops, tnode_t *node, codegen_t *cgen)
{
	if (node->tag == mcsp.tag_REPLSEQ) {
		int toplab = codegen_new_label (cgen);
		int botlab = codegen_new_label (cgen);
		tnode_t *rref = tnode_nthsubof (node, 1);

		/*{{{  loop-head*/
		codegen_callops (cgen, loadname, tnode_nthsubof (node, 2), 0);		/* start */
		codegen_callops (cgen, storename, rref, 0);				/* => value */
		codegen_callops (cgen, loadname, tnode_nthsubof (node, 3), 0);		/* end */
		codegen_callops (cgen, loadname, tnode_nthsubof (node, 2), 0);		/* start */
		codegen_callops (cgen, tsecondary, I_SUB);
		codegen_callops (cgen, storename, rref, cgen->target->intsize);		/* => count */

		codegen_callops (cgen, setlabel, toplab);

		/*}}}*/
		/*{{{  loop-body*/
		codegen_subcodegen (tnode_nthsubof (node, 0), cgen);

		/*}}}*/
		/*{{{  loop-end*/

		codegen_callops (cgen, loadname, rref, cgen->target->intsize);		/* count */
		codegen_callops (cgen, branch, I_CJ, botlab);
		codegen_callops (cgen, loadname, rref, 0);				/* value */
		codegen_callops (cgen, loadconst, 1);
		codegen_callops (cgen, tsecondary, I_ADD);
		codegen_callops (cgen, storename, rref, 0);				/* value = value - 1*/
		codegen_callops (cgen, loadname, rref, cgen->target->intsize);		/* count */
		codegen_callops (cgen, loadconst, 1);
		codegen_callops (cgen, tsecondary, I_SUB);
		codegen_callops (cgen, storename, rref, cgen->target->intsize);		/* count = count - 1*/
		codegen_callops (cgen, branch, I_J, toplab);

		codegen_callops (cgen, setlabel, botlab);
		/*}}}*/
		return 0;
	}
	return 1;
}
/*}}}*/


/*{{{  static int mcsp_fetrans_guardnode (compops_t *cops, tnode_t **node, fetrans_t *fe)*/
/*
 *	does front-end transformation on a GUARD
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_fetrans_guardnode (compops_t *cops, tnode_t **node, fetrans_t *fe)
{
	mcsp_fetrans_t *mfe = (mcsp_fetrans_t *)fe->langpriv;

	switch (mfe->parse) {
	case 0:
		if ((*node)->tag == mcsp.tag_GUARD) {
			/* don't walk LHS */
			fetrans_subtree (tnode_nthsubaddr (*node, 1), fe);
			return 0;
		}
		break;
	case 1:
		/* nothing in this pass! */
		break;
	case 2:
		if ((*node)->tag == mcsp.tag_GUARD) {
			/*{{{  add event to current alphabet*/
			tnode_t *event = tnode_nthsubof (*node, 0);

			if (mfe->curalpha && (event->tag == mcsp.tag_EVENT)) {
				mcsp_addtoalpha (mfe->curalpha, event);
			}
			/*}}}*/
		}
		break;
	}
	return 1;
}
/*}}}*/


/*{{{  static int mcsp_scopein_spacenode (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	scopes-in a SPACENODE (formal parameters and declarations)
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_scopein_spacenode (compops_t *cops, tnode_t **node, scope_t *ss)
{
	if ((*node)->tag == mcsp.tag_FPARAM) {
		/*{{{  scope in parameter*/
		tnode_t **paramptr = tnode_nthsubaddr (*node, 0);
		char *rawname;
		name_t *name;
		tnode_t *newname;

		rawname = (char *)tnode_nthhookof (*paramptr, 0);
		name = name_addscopename (rawname, *paramptr, NULL, NULL);
		newname = tnode_createfrom (mcsp.tag_EVENT, *paramptr, name);
		SetNameNode (name, newname);
		
		/* free old name, replace with new */
		tnode_free (*paramptr);
		ss->scoped++;
		*paramptr = newname;

		return 0;		/* don't walk beneath */
		/*}}}*/
	}
	return 1;
}
/*}}}*/
/*{{{  static int mcsp_scopeout_spacenode (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	scopes-out a SPACENODE (formal parameters and declarations)
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_scopeout_spacenode (compops_t *cops, tnode_t **node, scope_t *ss)
{
	if ((*node)->tag == mcsp.tag_FPARAM) {
		return 0;
	}
	return 1;
}
/*}}}*/
/*{{{  static int mcsp_typecheck_spacenode (compops_t *cops, tnode_t *node, typecheck_t *tc)*/
/*
 *	does type-checking on a spacenode
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_typecheck_spacenode (compops_t *cops, tnode_t *node, typecheck_t *tc)
{
	return 1;
}
/*}}}*/
/*{{{  static int mcsp_fetrans_spacenode (compops_t *cops, tnode_t **node, fetrans_t *fe)*/
/*
 *	does front-end transformation on an FPARAM
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_fetrans_spacenode (compops_t *cops, tnode_t **node, fetrans_t *fe)
{
	mcsp_fetrans_t *mfe = (mcsp_fetrans_t *)fe->langpriv;

	switch (mfe->parse) {
	case 0:
		if (((*node)->tag == mcsp.tag_FPARAM) || ((*node)->tag == mcsp.tag_UPARAM)) {
			/* nothing to do, don't walk EVENT underneath */
			return 0;
		}
		break;
	}

	return 1;
}
/*}}}*/
/*{{{  static int mcsp_namemap_spacenode (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	does name-mapping on s spacenode (formal params, decls)
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_namemap_spacenode (compops_t *cops, tnode_t **node, map_t *map)
{
	tnode_t **namep = tnode_nthsubaddr (*node, 0);
	tnode_t *type = NULL;
	tnode_t *bename;

	bename = map->target->newname (*namep, NULL, map, map->target->pointersize, 0, 0, 0, map->target->pointersize, 1);		/* always pointers.. (FIXME: tsize)*/

	tnode_setchook (*namep, map->mapchook, (void *)bename);
	*node = bename;
	
	return 0;
}
/*}}}*/
/*{{{  static int mcsp_codegen_spacenode (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	does code-gen on a spacenode (formal params, decls)
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_codegen_spacenode (compops_t *cops, tnode_t *node, codegen_t *cgen)
{
	return 0;
}
/*}}}*/

/*{{{  static int mcsp_fetrans_vardeclnode (compops_t *cops, tnode_t **node, fetrans_t *fe)*/
/*
 *	does front-end transforms for vardeclnodes
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_fetrans_vardeclnode (compops_t *cops, tnode_t **node, fetrans_t *fe)
{
	mcsp_fetrans_t *mfe = (mcsp_fetrans_t *)fe->langpriv;
	tnode_t *t = *node;

	switch (mfe->parse) {
	case 0:
		if (t->tag == mcsp.tag_VARDECL) {
			/* just walk subnodes 1+2 */
			fetrans_subtree (tnode_nthsubaddr (t, 1), fe);
			fetrans_subtree (tnode_nthsubaddr (t, 2), fe);
			return 0;
		}
		break;
	case 1:
		/* nothing in this pass */
		break;
	case 2:
		if (t->tag == mcsp.tag_VARDECL) {
			/*{{{  collect up events in body of declaration, hide the one declared*/
			mcsp_alpha_t *savedalpha = mfe->curalpha;
			mcsp_alpha_t *myalpha;

			mfe->curalpha = mcsp_newalpha ();
			fetrans_subtree (tnode_nthsubaddr (t, 2), fe);		/* walk process in scope of variable */
			myalpha = mfe->curalpha;
			mfe->curalpha = savedalpha;

			parser_rmfromlist (myalpha->elist, tnode_nthsubof (t, 0));
			if (mfe->curalpha) {
				mcsp_mergealpha (mfe->curalpha, myalpha);
			}
			mcsp_freealpha (myalpha);

			return 0;
			/*}}}*/
		}
		break;
	}

	return 1;
}
/*}}}*/
/*{{{  static int mcsp_namemap_vardeclnode (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	does name-mapping for a vardeclnodes
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_namemap_vardeclnode (compops_t *cops, tnode_t **node, map_t *map)
{
	tnode_t **namep = tnode_nthsubaddr (*node, 0);
	tnode_t **bodyp = tnode_nthsubaddr (*node, 2);
	tnode_t *type = NULL;
	tnode_t *bename;

	bename = map->target->newname (*namep, *bodyp, map, map->target->pointersize, 0, 0, 0, map->target->pointersize, 1);		/* always pointers.. (FIXME: tsize)*/

	tnode_setchook (*namep, map->mapchook, (void *)bename);
	*node = bename;

	if ((*namep)->tag == mcsp.tag_EVENT) {
		/* probably need initialisation/finalisation */
		if ((*namep)->tag->ndef->lops && tnode_haslangop_i ((*namep)->tag->ndef->lops, (int)LOPS_INITIALISING_DECL)) {
			tnode_calllangop_i ((*namep)->tag->ndef->lops, (int)LOPS_INITIALISING_DECL, 3, *namep, bename, map);
		}
	}

	bodyp = tnode_nthsubaddr (*node, 1);
	map_submapnames (bodyp, map);			/* map body */
	
	return 0;
}
/*}}}*/


/*{{{  static int mcsp_constprop_const (compops_t *cops, tnode_t **node)*/
/*
 *	does constant propagation on a constant node (post walk)
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_constprop_const (compops_t *cops, tnode_t **node)
{
	mcsp_consthook_t *ch = (mcsp_consthook_t *)tnode_nthhookof (*node, 0);

	if ((*node)->tag == mcsp.tag_INTEGER) {
		*node = constprop_newconst (CONST_INT, *node, NULL, *(int *)(ch->data));
	}
	return 0;
}
/*}}}*/
/*{{{  static int mcsp_namemap_const (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	does name-mapping for a constant
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_namemap_const (compops_t *cops, tnode_t **node, map_t *map)
{
	mcsp_consthook_t *ch = (mcsp_consthook_t *)tnode_nthhookof (*node, 0);

	*node = map->target->newconst (*node, map, ch->data, ch->length);
	return 0;
}
/*}}}*/
/*{{{  static int mcsp_isconst_const (langops_t *lops, tnode_t *node)*/
/*
 *	returns non-zero if the node is a constant (returns width)
 */
static int mcsp_isconst_const (langops_t *lops, tnode_t *node)
{
	mcsp_consthook_t *ch = (mcsp_consthook_t *)tnode_nthhookof (node, 0);

	return ch->length;
}
/*}}}*/
/*{{{  static int mcsp_constvalof_const (langops_t *lops, tnode_t *node, void *ptr)*/
/*
 *	returns the constant value of a constant node (assigns to pointer if non-null)
 */
static int mcsp_constvalof_const (langops_t *lops, tnode_t *node, void *ptr)
{
	mcsp_consthook_t *ch = (mcsp_consthook_t *)tnode_nthhookof (node, 0);
	int r = 0;

	if (node->tag == mcsp.tag_INTEGER) {
		if (ptr) {
			*(int *)ptr = *(int *)(ch->data);
		}
		r = *(int *)(ch->data);
	}
	return r;
}
/*}}}*/
/*{{{  static int mcsp_valbyref_const (langops_t *lops, tnode_t *node)*/
/*
 *	returns non-zero if value-references of this constant get passed by reference
 */
static int mcsp_valbyref_const (langops_t *lops, tnode_t *node)
{
	if (node->tag == mcsp.tag_STRING) {
		return 1;
	}
	return 0;
}
/*}}}*/


/*{{{  static int mcsp_prescope_instancenode (compops_t *cops, tnode_t **node, prescope_t *ps)*/
/*
 *	called to pre-scope an instance node
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_prescope_instancenode (compops_t *cops, tnode_t **node, prescope_t *ps)
{
	tnode_t **paramsptr = tnode_nthsubaddr (*node, 1);

	if (!*paramsptr) {
		/* empty-list */
		*paramsptr = parser_newlistnode (NULL);
	} else if (!parser_islistnode (*paramsptr)) {
		/* singleton */
		tnode_t *list = parser_newlistnode (NULL);

		parser_addtolist (list, *paramsptr);
		*paramsptr = list;
	}
	return 1;
}
/*}}}*/
/*{{{  static int mcsp_scopein_instancenode (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	called to scope-in an instance node
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_scopein_instancenode (compops_t *cops, tnode_t **node, scope_t *ss)
{
	mcsp_scope_t *mss = (mcsp_scope_t *)ss->langpriv;

	mss->inamescope = 1;
	tnode_modprepostwalktree (tnode_nthsubaddr (*node, 0), scope_modprewalktree, scope_modpostwalktree, (void *)ss);
	mss->inamescope = 0;
	tnode_modprepostwalktree (tnode_nthsubaddr (*node, 1), scope_modprewalktree, scope_modpostwalktree, (void *)ss);

	return 0;
}
/*}}}*/
/*{{{  static int mcsp_typecheck_instancenode (compops_t *cops, tnode_t *node, typecheck_t *tc)*/
/*
 *	does type-checking on an instancenode
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_typecheck_instancenode (compops_t *cops, tnode_t *node, typecheck_t *tc)
{
	tnode_t *name = tnode_nthsubof (node, 0);
	tnode_t *aparams = tnode_nthsubof (node, 1);
	name_t *pname;
	tnode_t *fparams;
	tnode_t **fplist, **aplist;
	int nfp, nap;
	int i;

	if (!name || (name->tag->ndef != mcsp.node_NAMENODE)) {
		typecheck_error (node, tc, "instanced object is not a name");
		return 0;
	} else if (name->tag != mcsp.tag_PROCDEF) {
		typecheck_error (node, tc, "called name is not a process");
		return 0;
	}
	pname = tnode_nthnameof (name, 0);
	fparams = NameTypeOf (pname);

#if 0
fprintf (stderr, "mcsp_typecheck_instancenode(): fparams = \n");
tnode_dumptree (fparams, 1, stderr);
fprintf (stderr, "mcsp_typecheck_instancenode(): aparams = \n");
tnode_dumptree (aparams, 1, stderr);
#endif
	fplist = parser_getlistitems (fparams, &nfp);
	aplist = parser_getlistitems (aparams, &nap);

	if (nap > nfp) {
		/* must be wrong */
		typecheck_error (node, tc, "too many actual parameters");
		return 0;
	}

	/* parameters up to a certain point must match */
	for (i = 0; (i < nfp) && (i < nap); i++) {
		if (fplist[i]->tag == mcsp.tag_FPARAM) {
			/* must have an actual event */
			if (aplist[i]->tag != mcsp.tag_EVENT) {
				typecheck_error (node, tc, "parameter %d is not an event", i+1);
				/* keep going.. */
			}
		} else if (fplist[i]->tag == mcsp.tag_UPARAM) {
			/* should not have a matching actual.. */
			typecheck_error (node, tc, "too many actual parameters");
			return 0;
		} else {
			nocc_internal ("mcsp_typecheck_instancenode(): formal parameter (1) is [%s]", fplist[i]->tag->name);
			return 0;
		}
	}
	/* any remaining formals must be UPARAMs */
	for (; i<nfp; i++) {
		if (fplist[i]->tag == mcsp.tag_FPARAM) {
			/* should have had an actual */
			typecheck_error (node, tc, "too few actual parameters");
			return 0;
		} else if (fplist[i]->tag != mcsp.tag_UPARAM) {
			nocc_internal ("mcsp_typecheck_instancenode(): formal parameter (2) is [%s]", fplist[i]->tag->name);
			return 0;
		}
	}

	/* all good :) */

	return 1;
}
/*}}}*/
/*{{{  static int mcsp_fetrans_instancenode (compops_t *cops, tnode_t **node, fetrans_t *fe)*/
/*
 *	does front-end transforms on an instancenode
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_fetrans_instancenode (compops_t *cops, tnode_t **node, fetrans_t *fe)
{
	mcsp_fetrans_t *mfe = (mcsp_fetrans_t *)fe->langpriv;
	tnode_t *t = *node;

	switch (mfe->parse) {
	case 0:
		if (t->tag == mcsp.tag_INSTANCE) {
			/*{{{  add any missing actual parameters to UPARAM formals*/
			tnode_t *aparams = tnode_nthsubof (t, 1);
			tnode_t *iname = tnode_nthsubof (t, 0);
			name_t *pname = tnode_nthnameof (iname, 0);
			tnode_t *fparams = NameTypeOf (pname);
			tnode_t **fplist, **aplist;
			int nfp, nap;

			fplist = parser_getlistitems (fparams, &nfp);
			aplist = parser_getlistitems (aparams, &nap);

#if 0
fprintf (stderr, "mcsp_fetrans_instancenode(): fparams = \n");
tnode_dumptree (fparams, 1, stderr);
fprintf (stderr, "mcsp_fetrans_instancenode(): aparams = \n");
tnode_dumptree (aparams, 1, stderr);
#endif
			if (nap < nfp) {
				/* need to add some missing actuals */
				if (!mfe->uvinsertlist) {
					nocc_internal ("mcsp_fetrans_instancenode(): need to add %d hidden actuals, but no insertlist!", nfp - nap);
					return 0;
				} else {
					int i;

					for (i=nap; i<nfp; i++) {
						/*{{{  add the name manually*/
						tnode_t *decl = tnode_create (mcsp.tag_UPARAM, NULL, NULL);
						tnode_t *newname;
						name_t *sname;
						char *rawname = (char *)smalloc (128);

						sprintf (rawname, "%s.%s", NameNameOf (pname), NameNameOf (tnode_nthnameof (tnode_nthsubof (fplist[i], 0), 0)));
						sname = name_addname (rawname, decl, NULL, NULL);
						sfree (rawname);

						parser_addtolist (mfe->uvinsertlist, decl);
						newname = tnode_createfrom (mcsp.tag_EVENT, decl, sname);
						SetNameNode (sname, newname);
						tnode_setnthsub (decl, 0, newname);

						parser_addtolist (aparams, newname);
						/*}}}*/
					}
				}
			}

			return 0;
			/*}}}*/
		}
		break;
	case 1:
		/* nothing in this pass */
		break;
	case 2:
		if (t->tag == mcsp.tag_INSTANCE) {
			/*{{{  add any events in actual parameters to alphabet*/
			if (mfe->curalpha) {
				tnode_t *aparams = tnode_nthsubof (t, 1);
				tnode_t **aplist;
				int nap, i;

				aplist = parser_getlistitems (aparams, &nap);
				for (i=0; i<nap; i++) {
					if (aplist[i] && (aplist[i]->tag == mcsp.tag_EVENT)) {
						mcsp_addtoalpha (mfe->curalpha, aplist[i]);
					}
				}
			}
			/*}}}*/
		}
		break;
	}

	return 1;
}
/*}}}*/
/*{{{  static int mcsp_namemap_instancenode (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	does name-mapping for an instancenode
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_namemap_instancenode (compops_t *cops, tnode_t **node, map_t *map)
{
	tnode_t *bename, *ibody, *namenode;

	/* map parameters and called name */
	map_submapnames (tnode_nthsubaddr (*node, 0), map);
	map_submapnames (tnode_nthsubaddr (*node, 1), map);

	namenode = tnode_nthsubof (*node, 0);
	if (namenode->tag == mcsp.tag_PROCDEF) {
		tnode_t *instance;
		name_t *name;

		name = tnode_nthnameof (namenode, 0);
		instance = NameDeclOf (name);

		/* body should be a back-end block */
		ibody = tnode_nthsubof (instance, 2);
	} else {
		nocc_internal ("mcsp_namemap_instancenode(): don\'t know how to handle [%s]", namenode->tag->name);
		return 0;
	}

	bename = map->target->newblockref (ibody, *node, map);
	*node = bename;

	return 0;
}
/*}}}*/
/*{{{  static int mcsp_codegen_instancenode (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	does code-generation for an instancenode
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_codegen_instancenode (compops_t *cops, tnode_t *node, codegen_t *cgen)
{
	tnode_t *namenode = tnode_nthsubof (node, 0);
	tnode_t *params = tnode_nthsubof (node, 1);

	if (namenode->tag == mcsp.tag_PROCDEF) {
		int ws_size, ws_offset, vs_size, ms_size, adjust;
		name_t *name = tnode_nthnameof (namenode, 0);
		tnode_t *instance = NameDeclOf (name);
		tnode_t *ibody = tnode_nthsubof (instance, 2);

		codegen_check_beblock (ibody, cgen, 1);

		/* get the size of the block */
		cgen->target->be_getblocksize (ibody, &ws_size, &ws_offset, &vs_size, &ms_size, &adjust, NULL);

		if (!parser_islistnode (params)) {
			nocc_internal ("mcsp_codegen_instancenode(): expected list of parameters, got [%s]", params ? params->tag->name : "(null)");
			return 0;
		} else {
			int nitems, i, wsoff;
			tnode_t **items = parser_getlistitems (params, &nitems);

			for (i=nitems - 1, wsoff = -4; i>=0; i--, wsoff -= 4) {
				codegen_callops (cgen, loadparam, items[i], PARAM_REF);
				codegen_callops (cgen, storelocal, wsoff);
			}
		}
		codegen_callops (cgen, callnamelabel, name, adjust);
	} else {
		nocc_internal ("mcsp_codegen_instancenode(): don\'t know how to handle [%s]", namenode->tag->name);
	}

	return 0;
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
	langops_t *lops;

	/*{{{  mcsp:rawnamenode -- NAME*/
	i = -1;
	tnd = tnode_newnodetype ("mcsp:rawnamenode", &i, 0, 0, 1, TNF_NONE);				/* hooks: raw-name */
	tnd->hook_free = mcsp_rawnamenode_hook_free;
	tnd->hook_copy = mcsp_rawnamenode_hook_copy;
	tnd->hook_dumptree = mcsp_rawnamenode_hook_dumptree;
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (mcsp_scopein_rawname));
	tnd->ops = cops;

	i = -1;
	mcsp.tag_NAME = tnode_newnodetag ("MCSPNAME", &i, tnd, NTF_NONE);

#if 0
fprintf (stderr, "mcsp_process_init_nodes(): tnd->name = [%s], mcsp.tag_NAME->name = [%s], mcsp.tag_NAME->ndef->name = [%s]\n", tnd->name, mcsp.tag_NAME->name, mcsp.tag_NAME->ndef->name);
#endif
	/*}}}*/
	/*{{{  mcsp:scopenode -- HIDE, FIXPOINT*/
	i = -1;
	tnd = mcsp.node_SCOPENODE = tnode_newnodetype ("mcsp:scopenode", &i, 2, 0, 0, TNF_NONE);	/* subnodes: 0 = vars, 1 = process */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "prescope", 2, COMPOPTYPE (mcsp_prescope_scopenode));
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (mcsp_scopein_scopenode));
	tnode_setcompop (cops, "scopeout", 2, COMPOPTYPE (mcsp_scopeout_scopenode));
	tnode_setcompop (cops, "fetrans", 2, COMPOPTYPE (mcsp_fetrans_scopenode));
	tnd->ops = cops;

	i = -1;
	mcsp.tag_HIDE = tnode_newnodetag ("MCSPHIDE", &i, tnd, NTF_NONE);
	i = -1;
	mcsp.tag_FIXPOINT = tnode_newnodetag ("MCSPFIXPOINT", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  mcsp:declnode -- PROCDECL*/
	i = -1;
	tnd = tnode_newnodetype ("mcsp:declnode", &i, 4, 0, 0, TNF_LONGDECL);				/* subnodes: 0 = name, 1 = params, 2 = body, 3 = in-scope-body */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "prescope", 2, COMPOPTYPE (mcsp_prescope_declnode));
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (mcsp_scopein_declnode));
	tnode_setcompop (cops, "scopeout", 2, COMPOPTYPE (mcsp_scopeout_declnode));
	tnode_setcompop (cops, "fetrans", 2, COMPOPTYPE (mcsp_fetrans_declnode));
	tnode_setcompop (cops, "betrans", 2, COMPOPTYPE (mcsp_betrans_declnode));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (mcsp_namemap_declnode));
	tnode_setcompop (cops, "precode", 2, COMPOPTYPE (mcsp_precode_declnode));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (mcsp_codegen_declnode));
	tnd->ops = cops;

	i = -1;
	mcsp.tag_PROCDECL = tnode_newnodetag ("MCSPPROCDECL", &i, tnd, NTF_NONE);
	i = -1;
	mcsp.tag_XPROCDECL = tnode_newnodetag ("MCSPXPROCDECL", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  mcsp:hiddennode -- HIDDENPARAM*/
	i = -1;
	tnd = tnode_newnodetype ("mcsp:hiddennode", &i, 1, 0, 0, TNF_NONE);				/* subnodes: 0 = hidden-param */

	i = -1;
	mcsp.tag_HIDDENPARAM = tnode_newnodetag ("MCSPHIDDENPARAM", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  mcsp:leafnode -- RETURNADDRESS, PARSPACE, LOOPSPACE*/
	i = -1;
	tnd = tnode_newnodetype ("mcsp:leafnode", &i, 0, 0, 0, TNF_NONE);

	i = -1;
	mcsp.tag_RETURNADDRESS = tnode_newnodetag ("MCSPRETURNADDRESS", &i, tnd, NTF_NONE);
	i = -1;
	mcsp.tag_PARSPACE = tnode_newnodetag ("MCSPPARSPACE", &i, tnd, NTF_NONE);
	i = -1;
	mcsp.tag_LOOPSPACE = tnode_newnodetag ("MCSPLOOPSPACE", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  mcsp:leafproc -- SKIP, STOP, DIV, CHAOS*/
	i = -1;
	tnd = mcsp.node_LEAFPROC = tnode_newnodetype ("mcsp:leafproc", &i, 0, 0, 0, TNF_NONE);
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (mcsp_namemap_leafproc));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (mcsp_codegen_leafproc));
	tnd->ops = cops;

	i = -1;
	mcsp.tag_SKIP = tnode_newnodetag ("MCSPSKIP", &i, tnd, NTF_NONE);
	i = -1;
	mcsp.tag_STOP = tnode_newnodetag ("MCSPSTOP", &i, tnd, NTF_NONE);
	i = -1;
	mcsp.tag_DIV = tnode_newnodetag ("MCSPDIV", &i, tnd, NTF_NONE);
	i = -1;
	mcsp.tag_CHAOS = tnode_newnodetag ("MCSPCHAOS", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  mcsp:namenode -- EVENT, PROCDEF, CHAN, VAR*/
	i = -1;
	tnd = mcsp.node_NAMENODE = tnode_newnodetype ("mcsp:namenode", &i, 0, 1, 0, TNF_NONE);		/* subnames: 0 = name */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "fetrans", 2, COMPOPTYPE (mcsp_fetrans_namenode));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (mcsp_namemap_namenode));
/*	cops->gettype = mcsp_gettype_namenode; */
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "initialising_decl", 3, LANGOPTYPE (mcsp_namenode_initialising_decl));
	tnd->lops = lops;

	i = -1;
	mcsp.tag_EVENT = tnode_newnodetag ("MCSPEVENT", &i, tnd, NTF_NONE);
	i = -1;
	mcsp.tag_PROCDEF = tnode_newnodetag ("MCSPPROCDEF", &i, tnd, NTF_NONE);
	i = -1;
	mcsp.tag_CHAN = tnode_newnodetag ("MCSPCHAN", &i, tnd, NTF_NONE);
	i = -1;
	mcsp.tag_VAR = tnode_newnodetag ("MCSPVAR", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  mcsp:actionnode -- SYNC, CHANWRITE*/
	i = -1;
	tnd = tnode_newnodetype ("mcsp:actionnode", &i, 2, 0, 0, TNF_NONE);				/* subnodes: 0 = event(s), 1 = data/null */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "fetrans", 2, COMPOPTYPE (mcsp_fetrans_actionnode));
	tnode_setcompop (cops, "betrans", 2, COMPOPTYPE (mcsp_betrans_actionnode));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (mcsp_namemap_actionnode));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (mcsp_codegen_actionnode));
	tnd->ops = cops;

	i = -1;
	mcsp.tag_SYNC = tnode_newnodetag ("MCSPSYNC", &i, tnd, NTF_NONE);
	i = -1;
	mcsp.tag_CHANWRITE = tnode_newnodetag ("MCSPCHANWRITE", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  mcsp:cnode -- SEQCODE, PARCODE*/
	i = -1;
	tnd = mcsp.node_CNODE = tnode_newnodetype ("mcsp:cnode", &i, 2, 0, 1, TNF_NONE);		/* subnodes: 0 = back-end space reference, 1 = list of processes;  hooks: 0 = mcsp_alpha_t */
	tnd->hook_free = mcsp_alpha_hook_free;
	tnd->hook_copy = mcsp_alpha_hook_copy;
	tnd->hook_dumptree = mcsp_alpha_hook_dumptree;
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "fetrans", 2, COMPOPTYPE (mcsp_fetrans_cnode));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (mcsp_namemap_cnode));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (mcsp_codegen_cnode));
	tnd->ops = cops;

	i = -1;
	mcsp.tag_SEQCODE = tnode_newnodetag ("MCSPSEQNODE", &i, tnd, NTF_NONE);
	i = -1;
	mcsp.tag_PARCODE = tnode_newnodetag ("MCSPPARNODE", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  mcsp:loopnode -- ILOOP, PRIDROP*/
	i = -1;
	tnd = tnode_newnodetype ("mcsp:loopnode", &i, 2, 0, 0, TNF_NONE);				/* subnodes: 0 = body; 1 = condition */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (mcsp_namemap_loopnode));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (mcsp_codegen_loopnode));
	tnd->ops = cops;

	i = -1;
	mcsp.tag_ILOOP = tnode_newnodetag ("MCSPILOOP", &i, tnd, NTF_NONE);
	i = -1;
	mcsp.tag_PRIDROP = tnode_newnodetag ("MCSPPRIDROP", &i, tnd, NTF_NONE);		/* maybe not a loopnode as such, but will do for now */

	/*}}}*/
	/*{{{  mcsp:replnode -- REPLSEQ, REPLPAR, REPLILEAVE*/
	i = -1;
	tnd = mcsp.node_REPLNODE = tnode_newnodetype ("mcsp:replnode", &i, 4, 0, 0, TNF_NONE);		/* subnodes: 0 = body; 1 = repl-var, 2 = start, 3 = end */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (mcsp_scopein_replnode));
	tnode_setcompop (cops, "scopeout", 2, COMPOPTYPE (mcsp_scopeout_replnode));
	tnode_setcompop (cops, "fetrans", 2, COMPOPTYPE (mcsp_fetrans_replnode));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (mcsp_namemap_replnode));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (mcsp_codegen_replnode));
	tnd->ops = cops;

	i = -1;
	mcsp.tag_REPLSEQ = tnode_newnodetag ("MCSPREPLSEQ", &i, tnd, NTF_NONE);
	i = -1;
	mcsp.tag_REPLPAR = tnode_newnodetag ("MCSPREPLSEQ", &i, tnd, NTF_NONE);
	i = -1;
	mcsp.tag_REPLILEAVE = tnode_newnodetag ("MCSPREPLSEQ", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  mcsp:guardnode -- GUARD*/
	i = -1;
	tnd = tnode_newnodetype ("mcsp:guardnode", &i, 2, 0, 0, TNF_NONE);				/* subnodes: 0 = guard, 1 = process */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "fetrans", 2, COMPOPTYPE (mcsp_fetrans_guardnode));
	tnd->ops = cops;

	i = -1;
	mcsp.tag_GUARD = tnode_newnodetag ("MCSPGUARD", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  mcsp:spacenode -- FPARAM, UPARAM, VARDECL*/
	/* this is used in front of formal parameters and local variables*/
	i = -1;
	tnd = mcsp.node_SPACENODE = tnode_newnodetype ("mcsp:spacenode", &i, 1, 0, 0, TNF_NONE);	/* subnodes: 0 = namenode/name */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (mcsp_scopein_spacenode));
	tnode_setcompop (cops, "scopeout", 2, COMPOPTYPE (mcsp_scopeout_spacenode));
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (mcsp_typecheck_spacenode));
	tnode_setcompop (cops, "fetrans", 2, COMPOPTYPE (mcsp_fetrans_spacenode));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (mcsp_namemap_spacenode));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (mcsp_codegen_spacenode));
	tnd->ops = cops;

	i = -1;
	mcsp.tag_FPARAM = tnode_newnodetag ("MCSPFPARAM", &i, tnd, NTF_NONE);
	i = -1;
	mcsp.tag_UPARAM = tnode_newnodetag ("MCSPUPARAM", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  mcsp:vardeclnode -- VARDECL*/
	i = -1;
	tnd = tnode_newnodetype ("mcsp:vardeclnode", &i, 3, 0, 0, TNF_SHORTDECL);			/* subnodes: 0 = name, 1 = initialiser, 2 = body */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (mcsp_namemap_vardeclnode));
	tnode_setcompop (cops, "fetrans", 2, COMPOPTYPE (mcsp_fetrans_vardeclnode));
	tnd->ops = cops;

	i = -1;
	mcsp.tag_VARDECL = tnode_newnodetag ("MCSPVARDECL", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  mcsp:constnode -- STRING, INTEGER*/
	i = -1;
	tnd = tnode_newnodetype ("mcsp:constnode", &i, 0, 0, 1, TNF_NONE);				/* hooks: data */
	tnd->hook_free = mcsp_constnode_hook_free;
	tnd->hook_copy = mcsp_constnode_hook_copy;
	tnd->hook_dumptree = mcsp_constnode_hook_dumptree;
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "constprop", 1, COMPOPTYPE (mcsp_constprop_const));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (mcsp_namemap_const));
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "isconst", 1, LANGOPTYPE (mcsp_isconst_const));
	tnode_setlangop (lops, "constvalof", 2, LANGOPTYPE (mcsp_constvalof_const));
	tnode_setlangop (lops, "valbyref", 1, LANGOPTYPE (mcsp_valbyref_const));
	tnd->ops = cops;

	i = -1;
	mcsp.tag_STRING = tnode_newnodetag ("MCSPSTRING", &i, tnd, NTF_NONE);
	i = -1;
	mcsp.tag_INTEGER = tnode_newnodetag ("MCSPINTEGER", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  mcsp:instancenode -- INSTANCE*/
	i = -1;
	tnd = tnode_newnodetype ("mcsp:instancenode", &i, 2, 0, 0, TNF_NONE);				/* subnodes: 0 = name, 1 = parameters */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "prescope", 2, COMPOPTYPE (mcsp_prescope_instancenode));
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (mcsp_scopein_instancenode));
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (mcsp_typecheck_instancenode));
	tnode_setcompop (cops, "fetrans", 2, COMPOPTYPE (mcsp_fetrans_instancenode));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (mcsp_namemap_instancenode));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (mcsp_codegen_instancenode));
	tnd->ops = cops;

	i = -1;
	mcsp.tag_INSTANCE = tnode_newnodetag ("MCSPINSTANCE", &i, tnd, NTF_NONE);

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
	parser_register_grule ("mcsp:namepush", parser_decode_grule ("T+St0XC1N-", mcsp_nametoken_to_hook, mcsp.tag_NAME));
	parser_register_grule ("mcsp:hidereduce", parser_decode_grule ("ST0T+@tN+N+0C3R-", mcsp.tag_HIDE));
	parser_register_grule ("mcsp:procdeclreduce", parser_decode_grule ("SN2N+N+N+>V0C4R-", mcsp.tag_PROCDECL));
	parser_register_grule ("mcsp:ppreduce", parser_decode_grule ("ST0T+XR-", mcsp_pptoken_to_node));
	parser_register_grule ("mcsp:fixreduce", parser_decode_grule ("SN0N+N+VC2R-", mcsp.tag_FIXPOINT));
	parser_register_grule ("mcsp:subevent", parser_decode_grule ("SN0N+N+V00C4R-", mcsp.tag_SUBEVENT));
	parser_register_grule ("mcsp:integerreduce", parser_decode_grule ("ST0T+XC1R-", mcsp_integertoken_to_hook, mcsp.tag_INTEGER));
	parser_register_grule ("mcsp:stringreduce", parser_decode_grule ("ST0T+XC1R-", mcsp_stringtoken_to_hook, mcsp.tag_STRING));
	parser_register_grule ("mcsp:instancereduce", parser_decode_grule ("SN0N+N+VC2R-", mcsp.tag_INSTANCE));
	parser_register_grule ("mcsp:replseqreduce", parser_decode_grule ("N+N+N+VN+VN-VN+C4R-", mcsp.tag_REPLSEQ));
	parser_register_grule ("mcsp:replseqlreduce", parser_decode_grule ("N+00N+C4R-", mcsp.tag_REPLSEQ));

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
	dynarray_add (transtbl, dfa_transtotbl ("mcsp:string ::= [ 0 +String 1 ] [ 1 {<mcsp:stringreduce>} -* ]"));
	dynarray_add (transtbl, dfa_transtotbl ("mcsp:integer ::= [ 0 +Integer 1 ] [ 1 {<mcsp:integerreduce>} -* ]"));
	dynarray_add (transtbl, dfa_transtotbl ("mcsp:expr ::= [ 0 mcsp:name 1 ] [ 0 mcsp:string 1 ] [ 0 mcsp:integer 1 ] [ 1 {<mcsp:nullreduce>} -* ]"));
	dynarray_add (transtbl, dfa_transtotbl ("mcsp:event ::= [ 0 mcsp:name 1 ] [ 1 @@. 3 ] [ 1 -* 2 ] [ 2 {<mcsp:nullreduce>} -* ] " \
				"[ 3 mcsp:expr 4 ] [ 4 {<mcsp:subevent>} -* ]"));
	dynarray_add (transtbl, dfa_bnftotbl ("mcsp:eventset ::= ( mcsp:event | @@{ { mcsp:event @@, 1 } @@} )"));
	dynarray_add (transtbl, dfa_bnftotbl ("mcsp:fparams ::= { mcsp:name @@, 0 }"));
	dynarray_add (transtbl, dfa_bnftotbl ("mcsp:aparams ::= { mcsp:name @@, 0 }"));
#if 0
	dynarray_add (transtbl, dfa_transtotbl ("mcsp:dop ::= [ 0 +@@-> 1 ] [ 0 +@@; 1 ] [ 0 +@@|| 1 ] [ 0 +@@||| 1 ] [ 0 +@@|~| 1 ] [ 0 +@@[ 3 ] [ 1 Newline 1 ] [ 1 -* 2 ] [ 2 {Rmcsp:op} -* ] "\
				"[ 3 @@] 4 ] [ 4 Newline 4 ] [ 4 -* 5 ] [ 5 {<mcsp:nullechoicereduce>} -* ]"));
#endif
	dynarray_add (transtbl, dfa_transtotbl ("mcsp:leafproc ::= [ 0 +@SKIP 1 ] [ 0 +@STOP 1 ] [ 0 +@DIV 1 ] [ 0 +@CHAOS 1 ] [ 1 {<mcsp:ppreduce>} -* ]"));
	dynarray_add (transtbl, dfa_transtotbl ("mcsp:fixpoint ::= [ 0 @@@ 1 ] [ 1 mcsp:name 2 ] [ 2 @@. 3 ] [ 3 mcsp:process 4 ] [ 4 {<mcsp:fixreduce>} -* ]"));
	dynarray_add (transtbl, dfa_transtotbl ("mcsp:replseq ::= [ 0 @@; 1 ] [ 1 @@[ 2 ] [ 2 mcsp:expr 3 ] [ 3 @@= 4 ] [ 3 @@] 10 ] [ 4 mcsp:expr 5 ] [ 5 @@, 6 ] [ 6 mcsp:expr 7 ] " \
				"[ 7 @@] 8 ] [ 8 mcsp:process 9 ] [ 9 {<mcsp:replseqreduce>} -* ] [ 10 mcsp:process 11 ] [ 11 {<mcsp:replseqlreduce>} -* ]"));
	dynarray_add (transtbl, dfa_transtotbl ("mcsp:hide ::= [ 0 +@@\\ 1 ] [ 1 mcsp:eventset 2 ] [ 2 {<mcsp:hidereduce>} -* ]"));
	dynarray_add (transtbl, dfa_transtotbl ("mcsp:restofprocess ::= [ 0 mcsp:dop 1 ] [ 1 mcsp:process 2 ] [ 2 {Rmcsp:folddop} -* ] " \
				"[ 0 %mcsp:hide <mcsp:hide> ]"));
	dynarray_add (transtbl, dfa_transtotbl ("mcsp:instance ::= [ 0 mcsp:name 1 ] [ 1 @@( 2 ] [ 2 mcsp:aparams 3 ] [ 3 @@) 4 ] [ 4 {<mcsp:instancereduce>} -* ]"));
	dynarray_add (transtbl, dfa_transtotbl ("mcsp:process ::= [ 0 +Name 7 ] [ 0 mcsp:leafproc 2 ] [ 0 mcsp:fixpoint 2 ] [ 0 @@( 3 ] [ 0 -@@; 12 ] " \
				"[ 1 %mcsp:restofprocess <mcsp:restofprocess> ] [ 1 -* 2 ] [ 2 {<mcsp:nullreduce>} -* ] " \
				"[ 3 mcsp:process 4 ] [ 4 @@) 5 ] [ 5 %mcsp:restofprocess <mcsp:restofprocess> ] [ 5 -* 6 ] [ 6 {<mcsp:nullreduce>} -* ] " \
				"[ 7 -@@( 8 ] [ 7 -* 10 ] [ 8 {<parser:rewindtokens>} -* 9 ] [ 9 mcsp:instance 1 ] [ 10 {<parser:rewindtokens>} -* 11 ] [ 11 mcsp:event 1 ] " \
				"[ 12 mcsp:replseq 13 ] [ 13 {<mcsp:nullreduce>} -* ]"));
	/* dynarray_add (transtbl, dfa_transtotbl ("mcsp:traceline ::= [ 0 +Name 7 ] [ 0 mcsp:leafproc 2 ]")); */
	dynarray_add (transtbl, dfa_transtotbl ("mcsp:procdecl ::= [ 0 +Name 1 ] [ 1 {<mcsp:namepush>} ] [ 1 @@::= 2 ] [ 1 @@( 4 ] [ 2 {<mcsp:nullpush>} ] [ 2 mcsp:process 3 ] [ 3 {<mcsp:procdeclreduce>} -* ] " \
				"[ 4 mcsp:fparams 5 ] [ 5 @@) 6 ] [ 6 @@::= 7 ] [ 7 mcsp:process 3 ]"));

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

