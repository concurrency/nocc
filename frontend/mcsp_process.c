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
} mcsp_consthook_t;

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

		newch->data = ch->data ? string_dup (ch->data) : NULL;
		newch->length = ch->length;

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
	fprintf (stream, "<mcspconsthook length=\"%d\" value=\"%s\" />\n", ch ? ch->length : 0, (ch && ch->data) ? ch->data : "(null)");
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
		tnode_free (name);
	} else {
#if 0
fprintf (stderr, "mcsp_scopein_rawname(): unresolved name \"%s\", unbound-events = %d, mss->uvinsertlist = 0x%8.8x\n", rawname, lmp ? lmp->unboundvars : -1, (unsigned int)mss->uvinsertlist);
#endif
		if (lmp && lmp->unboundvars) {
			if (mss && mss->uvinsertlist) {
				/*{{{  add the name manually*/
				scope_error (name, ss, "FIXME: unresolved name \"%s\"", rawname);
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

/*{{{  static int mcsp_checkisevent (tnode_t *node)*/
/*
 *	checks to see if the given tree is an event
 */
static int mcsp_checkisevent (tnode_t *node)
{
	if (node->tag == mcsp.tag_EVENT) {
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
	}
	return 0;
}
/*}}}*/
/*{{{  static int mcsp_typecheck_dopnode (tnode_t *node, typecheck_t *tc)*/
/*
 *	does type-checking on a dop-node
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_typecheck_dopnode (tnode_t *node, typecheck_t *tc)
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
	return 1;
}
/*}}}*/
/*{{{  static int mcsp_fetrans_dopnode (tnode_t **node, fetrans_t *fe)*/
/*
 *	does front-end transformations on a DOPNODE (quite a lot done here)
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_fetrans_dopnode (tnode_t **node, fetrans_t *fe)
{
	tnode_t *t = *node;

	/* these have to be done in the right order..! */
	if (t->tag == mcsp.tag_ECHOICE) {
		/*{{{  external-choice: scoop up events and build ALT*/
		tnode_t **lhsp = tnode_nthsubaddr (t, 0);
		tnode_t **rhsp = tnode_nthsubaddr (t, 1);
		tnode_t *list, *altnode;

		if ((*lhsp)->tag == mcsp.tag_THEN) {
			tnode_t *event = tnode_nthsubof (*lhsp, 0);
			tnode_t *guard;

			if (event->tag == mcsp.tag_SUBEVENT) {
				/* sub-event, just pick LHS */
				event = tnode_nthsubof (*lhsp, 0);
			}
			*lhsp = tnode_create (mcsp.tag_GUARD, NULL, event, *lhsp);
		}
		if ((*rhsp)->tag == mcsp.tag_THEN) {
			tnode_t *event = tnode_nthsubof (*rhsp, 0);
			tnode_t *guard;

			if (event->tag == mcsp.tag_SUBEVENT) {
				/* sub-event, just pick LHS */
				event = tnode_nthsubof (*rhsp, 0);
			}
			*rhsp = tnode_create (mcsp.tag_GUARD, NULL, event, *rhsp);
		}

		list = parser_buildlistnode (NULL, *lhsp, *rhsp, NULL);
		altnode = tnode_create (mcsp.tag_ALT, NULL, list);

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
		list = tnode_create (mcsp.tag_SEQCODE, NULL, list);

#if 0
fprintf (stderr, "mcsp_fetrans_dopnode(): list is now:\n");
tnode_dumptree (list, 1, stderr);
#endif
		*node = list;
		/*}}}*/
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
	ntdef_t *xtag;

	if ((*node)->tag == mcsp.tag_HIDE) {
		xtag = mcsp.tag_EVENT;
	} else if ((*node)->tag == mcsp.tag_FIXPOINT) {
		xtag = mcsp.tag_PROCDEF;
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

		sname = name_addscopename (rawname, *node, NULL, NULL);
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

/*{{{  static int mcsp_prescope_declnode (tnode_t **node, prescope_t *ps)*/
/*
 *	pre-scopes a process definition
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_prescope_declnode (tnode_t **node, prescope_t *ps)
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
/*{{{  static int mcsp_scopein_declnode (tnode_t **node, scope_t *ss)*/
/*
 *	called to scope-in a process definition
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_scopein_declnode (tnode_t **node, scope_t *ss)
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

	nsmark = name_markscope ();
	/* scope-in any parameters and walk body */
	tnode_modprepostwalktree (paramptr, scope_modprewalktree, scope_modpostwalktree, (void *)ss);
	mss->uvinsertlist = *paramptr;										/* prescope made sure it's a list */
	tnode_modprepostwalktree (bodyptr, scope_modprewalktree, scope_modpostwalktree, (void *)ss);

	name_markdescope (nsmark);
	mss->uvinsertlist = saved_uvil;

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
/*{{{  static int mcsp_scopeout_declnode (tnode_t **node, scope_t *ss)*/
/*
 *	called to scope-out a process definition
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_scopeout_declnode (tnode_t **node, scope_t *ss)
{
	return 1;
}
/*}}}*/
/*{{{  static int mcsp_fetrans_declnode (tnode_t **node, fetrans_t *fe)*/
/*
 *	does front-end transforms on a process declaration node
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_fetrans_declnode (tnode_t **node, fetrans_t *fe)
{
	return 1;
}
/*}}}*/
/*{{{  static int mcsp_betrans_declnode (tnode_t **node, betrans_t *be)*/
/*
 *	does back-end transforms on a process declaration node
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_betrans_declnode (tnode_t **node, betrans_t *be)
{
	return 1;
}
/*}}}*/
/*{{{  static int mcsp_namemap_declnode (tnode_t **node, map_t *map)*/
/*
 *	does name-mapping for a process declaration node
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_namemap_declnode (tnode_t **node, map_t *map)
{
	tnode_t *blk;
	tnode_t *saved_blk = map->thisblock;
	tnode_t **saved_params = map->thisprocparams;
	tnode_t **paramsptr;
	tnode_t *tmpname;

	blk = map->target->newblock (tnode_nthsubof (*node, 2), map, tnode_nthsubof (*node, 1), map->lexlevel + 1);
	map->thisblock = blk;
	map->thisprocparams = tnode_nthsubaddr (*node, 1);
	map->lexlevel++;

	/* map formal params and body */
	paramsptr = tnode_nthsubaddr (*node, 1);
	map_submapnames (paramsptr, map);
	map_submapnames (tnode_nthsubaddr (blk, 0), map);		/* do this under the back-end block */

	map->lexlevel--;
	map->thisblock = saved_blk;
	map->thisprocparams = saved_params;

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
/*{{{  static int mcsp_codegen_declnode (tnode_t *node, codegen_t *cgen)*/
/*
 *	does code-generation for a process definition
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_codegen_declnode (tnode_t *node, codegen_t *cgen)
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
	codegen_callops (cgen, debugline, node);

	/* generate body */
	codegen_subcodegen (body, cgen);
	codegen_callops (cgen, procreturn, adjust);

	/* generate code following declaration */
	codegen_subcodegen (tnode_nthsubof (node, 3), cgen);

	return 0;
}
/*}}}*/

/*{{{  static int mcsp_fetrans_namenode (tnode_t **node, fetrans_t *fe)*/
/*
 *	does front-end transformation on an EVENT
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_fetrans_namenode (tnode_t **node, fetrans_t *fe)
{
	tnode_t *t = *node;

	if (t->tag == mcsp.tag_EVENT) {
		/*{{{  turn event into SYNC -- not a declaration occurence*/
		*node = tnode_create (mcsp.tag_SYNC, NULL, *node, NULL);
		return 0;
		/*}}}*/
	}
	return 1;
}
/*}}}*/
/*{{{  static int mcsp_namemap_namenode (tnode_t **node, map_t *map)*/
/*
 *	does name-mapping for namenodes (unbound references)
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_namemap_namenode (tnode_t **node, map_t *map)
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

/*{{{  static int mcsp_fetrans_guardnode (tnode_t **node, fetrans_t *fe)*/
/*
 *	does front-end transformation on a GUARD
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_fetrans_guardnode (tnode_t **node, fetrans_t *fe)
{
	if ((*node)->tag == mcsp.tag_GUARD) {
		/* don't walk LHS */
		fetrans_subtree (tnode_nthsubaddr (*node, 1), fe);
		return 0;
	}
	return 1;
}
/*}}}*/

/*{{{  static int mcsp_scopein_spacenode (tnode_t **node, scope_t *ss)*/
/*
 *	scopes-in a SPACENODE (formal parameters and declarations)
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_scopein_spacenode (tnode_t **node, scope_t *ss)
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
/*{{{  static int mcsp_scopeout_spacenode (tnode_t **node, scope_t *ss)*/
/*
 *	scopes-out a SPACENODE (formal parameters and declarations)
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_scopeout_spacenode (tnode_t **node, scope_t *ss)
{
	if ((*node)->tag == mcsp.tag_FPARAM) {
		return 0;
	}
	return 1;
}
/*}}}*/
/*{{{  static int mcsp_typecheck_spacenode (tnode_t *node, typecheck_t *tc)*/
/*
 *	does type-checking on a spacenode
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_typecheck_spacenode (tnode_t *node, typecheck_t *tc)
{
	return 1;
}
/*}}}*/
/*{{{  static int mcsp_fetrans_spacenode (tnode_t **node, fetrans_t *fe)*/
/*
 *	does front-end transformation on an FPARAM
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_fetrans_spacenode (tnode_t **node, fetrans_t *fe)
{
	if ((*node)->tag == mcsp.tag_FPARAM) {
		/* nothing to do, don't walk EVENT underneath */
		return 0;
	}
	return 1;
}
/*}}}*/
/*{{{  static int mcsp_namemap_spacenode (tnode_t **node, map_t *map)*/
/*
 *	does name-mapping on s spacenode (formal params, decls)
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_namemap_spacenode (tnode_t **node, map_t *map)
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
/*{{{  static int mcsp_codegen_spacenode (tnode_t *node, codegen_t *cgen)*/
/*
 *	does code-gen on a spacenode (formal params, decls)
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_codegen_spacenode (tnode_t *node, codegen_t *cgen)
{
	return 0;
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

	dopnode = tnode_create (tag, pp->lf, NULL, NULL, NULL);
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
	/*{{{  mcsp:dopnode -- SUBEVENT, THEN, SEQ, PAR, ILEAVE, ICHOICE, ECHOICE*/
	i = -1;
	tnd = mcsp.node_DOPNODE = tnode_newnodetype ("mcsp:dopnode", &i, 3, 0, 0, TNF_NONE);		/* subnodes: 0 = LHS, 1 = RHS, 2 = type */
	cops = tnode_newcompops ();
	cops->typecheck = mcsp_typecheck_dopnode;
	cops->fetrans = mcsp_fetrans_dopnode;
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
	mcsp.tag_ILEAVE = tnode_newnodetag ("MCSPILEAVE", &i, tnd, NTF_NONE);
	i = -1;
	mcsp.tag_ICHOICE = tnode_newnodetag ("MCSPICHOICE", &i, tnd, NTF_NONE);
	i = -1;
	mcsp.tag_ECHOICE = tnode_newnodetag ("MCSPECHOICE", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  mcsp:scopenode -- HIDE, FIXPOINT*/
	i = -1;
	tnd = mcsp.node_SCOPENODE = tnode_newnodetype ("mcsp:scopenode", &i, 2, 0, 0, TNF_NONE);	/* subnodes: 0 = vars, 1 = process */
	cops = tnode_newcompops ();
	cops->prescope = mcsp_prescope_scopenode;
	cops->scopein = mcsp_scopein_scopenode;
	cops->scopeout = mcsp_scopeout_scopenode;
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
	cops->prescope = mcsp_prescope_declnode;
	cops->scopein = mcsp_scopein_declnode;
	cops->scopeout = mcsp_scopeout_declnode;
	cops->fetrans = mcsp_fetrans_declnode;
	cops->betrans = mcsp_betrans_declnode;
	cops->namemap = mcsp_namemap_declnode;
	cops->codegen = mcsp_codegen_declnode;
	tnd->ops = cops;

	i = -1;
	mcsp.tag_PROCDECL = tnode_newnodetag ("MCSPPROCDECL", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  mcsp:hiddennode -- HIDDENPARAM*/
	i = -1;
	tnd = tnode_newnodetype ("mcsp:hiddennode", &i, 1, 0, 0, TNF_NONE);				/* subnodes: 0 = hidden-param */

	i = -1;
	mcsp.tag_HIDDENPARAM = tnode_newnodetag ("MCSPHIDDENPARAM", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  mcsp:leafnode -- RETURNADDRESS*/
	i = -1;
	tnd = tnode_newnodetype ("mcsp:leafnode", &i, 0, 0, 0, TNF_NONE);

	i = -1;
	mcsp.tag_RETURNADDRESS = tnode_newnodetag ("MCSPRETURNADDRESS", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  mcsp:leafproc -- SKIP, STOP, DIV, CHAOS*/
	i = -1;
	tnd = mcsp.node_LEAFPROC = tnode_newnodetype ("mcsp:leafproc", &i, 0, 0, 0, TNF_NONE);
	cops = tnode_newcompops ();
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
	/*{{{  mcsp:namenode -- EVENT, PROCDEF*/
	i = -1;
	tnd = mcsp.node_NAMENODE = tnode_newnodetype ("mcsp:namenode", &i, 0, 1, 0, TNF_NONE);		/* subnames: 0 = name */
	cops = tnode_newcompops ();
	cops->fetrans = mcsp_fetrans_namenode;
	cops->namemap = mcsp_namemap_namenode;
/*	cops->gettype = mcsp_gettype_namenode; */
	tnd->ops = cops;

	i = -1;
	mcsp.tag_EVENT = tnode_newnodetag ("MCSPEVENT", &i, tnd, NTF_NONE);
	i = -1;
	mcsp.tag_PROCDEF = tnode_newnodetag ("MCSPPROCDEF", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  mcsp:actionnode -- SYNC*/
	i = -1;
	tnd = tnode_newnodetype ("mcsp:actionnode", &i, 2, 0, 0, TNF_NONE);				/* subnodes: 0 = event(s), 1 = data/null */
	cops = tnode_newcompops ();
	tnd->ops = cops;

	i = -1;
	mcsp.tag_SYNC = tnode_newnodetag ("MCSPSYNC", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  mcsp:cnode -- SEQCODE, PARCODE*/
	i = -1;
	tnd = tnode_newnodetype ("mcsp:cnode", &i, 1, 0, 0, TNF_NONE);					/* subnodes: 0 = list of processes */
	cops = tnode_newcompops ();
	tnd->ops = cops;

	i = -1;
	mcsp.tag_SEQCODE = tnode_newnodetag ("MCSPSEQNODE", &i, tnd, NTF_NONE);
	i = -1;
	mcsp.tag_PARCODE = tnode_newnodetag ("MCSPPARNODE", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  mcsp:snode -- ALT*/
	i = -1;
	tnd = tnode_newnodetype ("mcsp:snode", &i, 1, 0, 0, TNF_NONE);					/* subnodes: 0 = list of guards/nested ALTs */
	cops = tnode_newcompops ();
	tnd->ops = cops;

	i = -1;
	mcsp.tag_ALT = tnode_newnodetag ("MCSPALT", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  mcsp:guardnode -- GUARD*/
	i = -1;
	tnd = tnode_newnodetype ("mcsp:guardnode", &i, 2, 0, 0, TNF_NONE);				/* subnodes: 0 = guard, 1 = process */
	cops = tnode_newcompops ();
	cops->fetrans = mcsp_fetrans_guardnode;
	tnd->ops = cops;

	i = -1;
	mcsp.tag_GUARD = tnode_newnodetag ("MCSPGUARD", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  mcsp:spacenode -- FPARAM, UPARAM*/
	/* this is used in front of formal parameters */
	i = -1;
	tnd = mcsp.node_SPACENODE = tnode_newnodetype ("mcsp:spacenode", &i, 1, 0, 0, TNF_NONE);	/* subnodes: 1 = namenode/name */
	cops = tnode_newcompops ();
	cops->scopein = mcsp_scopein_spacenode;
	cops->scopeout = mcsp_scopeout_spacenode;
	cops->typecheck = mcsp_typecheck_spacenode;
	cops->fetrans = mcsp_fetrans_spacenode;
	cops->namemap = mcsp_namemap_spacenode;
	cops->codegen = mcsp_codegen_spacenode;
	tnd->ops = cops;

	i = -1;
	mcsp.tag_FPARAM = tnode_newnodetag ("MCSPFPARAM", &i, tnd, NTF_NONE);
	i = -1;
	mcsp.tag_UPARAM = tnode_newnodetag ("MCSPUPARAM", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  mcsp:constnode -- STRING*/
	i = -1;
	tnd = tnode_newnodetype ("mcsp:constnode", &i, 0, 0, 1, TNF_NONE);				/* hooks: data */
	tnd->hook_free = mcsp_constnode_hook_free;
	tnd->hook_copy = mcsp_constnode_hook_copy;
	tnd->hook_dumptree = mcsp_constnode_hook_dumptree;
	cops = tnode_newcompops ();
	tnd->ops = cops;

	i = -1;
	mcsp.tag_STRING = tnode_newnodetag ("MCSPSTRING", &i, tnd, NTF_NONE);

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
	parser_register_grule ("mcsp:namepush", parser_decode_grule ("T+St0XC1N-", mcsp_nametoken_to_hook, mcsp.tag_NAME));
	parser_register_grule ("mcsp:hidereduce", parser_decode_grule ("ST0T+@tN+N+0C3R-", mcsp.tag_HIDE));
	parser_register_grule ("mcsp:procdeclreduce", parser_decode_grule ("SN2N+N+N+>V0C4R-", mcsp.tag_PROCDECL));
	parser_register_grule ("mcsp:nullechoicereduce", parser_decode_grule ("ST0T+@t000C3R-", mcsp.tag_ECHOICE));
	parser_register_grule ("mcsp:ppreduce", parser_decode_grule ("ST0T+XR-", mcsp_pptoken_to_node));
	parser_register_grule ("mcsp:fixreduce", parser_decode_grule ("SN0N+N+VC2R-", mcsp.tag_FIXPOINT));
	parser_register_grule ("mcsp:subevent", parser_decode_grule ("SN0N+N+V0C3R-", mcsp.tag_SUBEVENT));
	parser_register_grule ("mcsp:stringreduce", parser_decode_grule ("ST0T+XC1R-", mcsp_stringtoken_to_hook, mcsp.tag_STRING));

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
	dynarray_add (transtbl, dfa_transtotbl ("mcsp:string ::= [ 0 +String 1 ] [ 1 {<mcsp:stringreduce>} -* ]"));
	dynarray_add (transtbl, dfa_transtotbl ("mcsp:expr ::= [ 0 mcsp:name 1 ] [ 0 mcsp:string 1 ] [ 1 {<mcsp:nullreduce>} -* ]"));
	dynarray_add (transtbl, dfa_transtotbl ("mcsp:event ::= [ 0 mcsp:name 1 ] [ 1 @@. 3 ] [ 1 -* 2 ] [ 2 {<mcsp:nullreduce>} -* ] " \
				"[ 3 mcsp:expr 4 ] [ 4 {<mcsp:subevent>} -* ]"));
	dynarray_add (transtbl, dfa_bnftotbl ("mcsp:eventset ::= ( mcsp:event | @@{ { mcsp:event @@, 1 } @@} )"));
	dynarray_add (transtbl, dfa_bnftotbl ("mcsp:fparams ::= { mcsp:name @@, 0 }"));
	dynarray_add (transtbl, dfa_transtotbl ("mcsp:dop ::= [ 0 +@@-> 1 ] [ 0 +@@; 1 ] [ 0 +@@|| 1 ] [ 0 +@@||| 1 ] [ 0 +@@|~| 1 ] [ 0 +@@[ 2 ] [ 1 {Rmcsp:op} -* ] "\
				"[ 2 @@] 3 ] [ 3 {<mcsp:nullechoicereduce>} -* ]"));
	dynarray_add (transtbl, dfa_transtotbl ("mcsp:leafproc ::= [ 0 +@SKIP 1 ] [ 0 +@STOP 1 ] [ 0 +@DIV 1 ] [ 0 +@CHAOS 1 ] [ 1 {<mcsp:ppreduce>} -* ]"));
	dynarray_add (transtbl, dfa_transtotbl ("mcsp:fixpoint ::= [ 0 @@@ 1 ] [ 1 mcsp:name 2 ] [ 2 @@. 3 ] [ 3 mcsp:process 4 ] [ 4 {<mcsp:fixreduce>} -* ]"));
	dynarray_add (transtbl, dfa_transtotbl ("mcsp:hide ::= [ 0 +@@\\ 1 ] [ 1 mcsp:eventset 2 ] [ 2 {<mcsp:hidereduce>} -* ]"));
	dynarray_add (transtbl, dfa_transtotbl ("mcsp:restofprocess ::= [ 0 mcsp:dop 1 ] [ 1 mcsp:process 2 ] [ 2 {Rmcsp:folddop} -* ] " \
				"[ 0 %mcsp:hide <mcsp:hide> ]"));
	dynarray_add (transtbl, dfa_transtotbl ("mcsp:process ::= [ 0 mcsp:event 1 ] [ 0 mcsp:leafproc 2 ] [ 0 mcsp:fixpoint 2 ] [ 0 @@( 3 ] [ 1 %mcsp:restofprocess <mcsp:restofprocess> ] [ 1 -* 2 ] [ 2 {<mcsp:nullreduce>} -* ] " \
				"[ 3 mcsp:process 4 ] [ 4 @@) 5 ] [ 5 %mcsp:restofprocess <mcsp:restofprocess> ] [ 5 -* 6 ] [ 6 {<mcsp:nullreduce>} -* ]"));
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

