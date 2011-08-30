/*
 *	eac_code.c -- EAC for NOCC
 *	Copyright (C) 2011 Fred Barnes <frmb@kent.ac.uk>
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
#include "fcnlib.h"
#include "symbols.h"
#include "keywords.h"
#include "lexer.h"
#include "lexpriv.h"
#include "tnode.h"
#include "parser.h"
#include "dfa.h"
#include "parsepriv.h"
#include "eac.h"
#include "feunit.h"
#include "names.h"
#include "scope.h"
#include "prescope.h"
#include "constprop.h"
#include "typecheck.h"
#include "usagecheck.h"
#include "postcheck.h"
#include "fetrans.h"
#include "mwsync.h"
#include "betrans.h"
#include "map.h"
#include "codegen.h"
#include "target.h"
#include "transputer.h"
#include "eacpriv.h"


/*}}}*/
/*{{{  private data*/

static int eac_ignore_unresolved = 0;


/*}}}*/


/*{{{  static void eac_rawnamenode_hook_free (void *hook)*/
/*
 *	frees a rawnamenode hook (name-bytes)
 */
static void eac_rawnamenode_hook_free (void *hook)
{
	if (hook) {
		sfree (hook);
	}
	return;
}
/*}}}*/
/*{{{  static void *eac_rawnamenode_hook_copy (void *hook)*/
/*
 *	copies a rawnamenode hook (name-bytes)
 */
static void *eac_rawnamenode_hook_copy (void *hook)
{
	char *rawname = (char *)hook;

	if (rawname) {
		return string_dup (rawname);
	}
	return NULL;
}
/*}}}*/
/*{{{  static void eac_rawnamenode_hook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	dump-tree for rawnamenode hook (name-bytes)
 */
static void eac_rawnamenode_hook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	eac_isetindent (stream, indent);
	fprintf (stream, "<eacrawnamenode value=\"%s\" />\n", hook ? (char *)hook : "(null)");
	return;
}
/*}}}*/


/*{{{  static int eac_format_instr (char *str, int *sleft, const char *fmt, ...)*/
/*
 *	formats into a string.
 *	returns number of bytes written.
 */
static int eac_format_instr (char *str, int *sleft, const char *fmt, ...)
{
	int w;
	va_list ap;

	va_start (ap, fmt);
	w = vsnprintf (str, *sleft, fmt, ap);
	va_end (ap);

	if (w > 0) {
		*sleft -= w;
		return w;
	}
	return 0;
}
/*}}}*/
/*{{{  static int eac_format_inexpr (char *ptr, int sleft, tnode_t *expr)*/
/*
 *	formats an escape analysis expression into a string.
 *	returns number of bytes added.
 */
static int eac_format_inexpr (char *str, int *sleft, tnode_t *expr)
{
	int this = 0;
	int tleft = *sleft;

	if (!expr) {
		return 0;
	}

	if (parser_islistnode (expr)) {
		tnode_t **items;
		int nitems, i;

		items = parser_getlistitems (expr, &nitems);

		for (i=0; i<nitems; i++) {
			if (i) {
				this += eac_format_instr (str + this, sleft, ", ");
			}
			this += eac_format_inexpr (str + this, sleft, items[i]);
		}
	} else if (expr->tag == eac.tag_DECL) {
		this = eac_format_inexpr (str, sleft, tnode_nthsubof (expr, 0));
		this += eac_format_instr (str + this, sleft, " (");
		this += eac_format_inexpr (str + this, sleft, tnode_nthsubof (expr, 1));
		this += eac_format_instr (str + this, sleft, ") = \n\t");
		this += eac_format_inexpr (str + this, sleft, tnode_nthsubof (expr, 2));
	} else if (expr->tag->ndef == eac.node_NAMENODE) {
		this = eac_format_instr (str, sleft, "%s", NameNameOf (tnode_nthnameof (expr, 0))); 
	} else if (expr->tag == eac.tag_VARDECL) {
		/* free-var or parameter */
		this = eac_format_inexpr (str, sleft, tnode_nthsubof (expr, 0));
	} else if (expr->tag == eac.tag_ESET) {
		this = eac_format_instr (str, sleft, "{");
		this += eac_format_inexpr (str + this, sleft, tnode_nthsubof (expr, 0));
		this += eac_format_instr (str + this, sleft, "}");
	} else if (expr->tag == eac.tag_ESEQ) {
		this = eac_format_instr (str, sleft, "<");
		this += eac_format_inexpr (str + this, sleft, tnode_nthsubof (expr, 0));
		this += eac_format_instr (str + this, sleft, ">");
	} else if (expr->tag == eac.tag_INPUT) {
		this = eac_format_inexpr (str, sleft, tnode_nthsubof (expr, 0));
		this += eac_format_instr (str + this, sleft, "?");
		this += eac_format_inexpr (str + this, sleft, tnode_nthsubof (expr, 1));
	} else if (expr->tag == eac.tag_OUTPUT) {
		this = eac_format_inexpr (str, sleft, tnode_nthsubof (expr, 0));
		this += eac_format_instr (str + this, sleft, "!");
		this += eac_format_inexpr (str + this, sleft, tnode_nthsubof (expr, 1));
	} else if (expr->tag == eac.tag_VARCOMP) {
		this = eac_format_inexpr (str, sleft, tnode_nthsubof (expr, 0));
		this += eac_format_instr (str + this, sleft, "<-");
		if (parser_islistnode (tnode_nthsubof (expr, 1))) {
			this += eac_format_instr (str + this, sleft, "{");
			this += eac_format_inexpr (str + this, sleft, tnode_nthsubof (expr, 1));
			this += eac_format_instr (str + this, sleft, "}");
		} else {
			this += eac_format_inexpr (str + this, sleft, tnode_nthsubof (expr, 1));
		}
	} else if (expr->tag == eac.tag_SVREND) {
		this = eac_format_instr (str, sleft, "~");
		this += eac_format_inexpr (str + this, sleft, tnode_nthsubof (expr, 0));
	} else if (expr->tag == eac.tag_CLIEND) {
		this = eac_format_instr (str, sleft, "^");
		this += eac_format_inexpr (str + this, sleft, tnode_nthsubof (expr, 0));
	} else if (expr->tag == eac.tag_PAR) {
		this = eac_format_inexpr (str, sleft, tnode_nthsubof (expr, 0));
		this += eac_format_instr (str + this, sleft, " || ");
		this += eac_format_inexpr (str + this, sleft, tnode_nthsubof (expr, 1));
	} else if (expr->tag == eac.tag_HIDE) {
		this = eac_format_inexpr (str, sleft, tnode_nthsubof (expr, 0));
		this += eac_format_instr (str + this, sleft, " \\ {");
		this += eac_format_inexpr (str + this, sleft, tnode_nthsubof (expr, 1));
		this += eac_format_instr (str + this, sleft, "}");
	} else if (expr->tag == eac.tag_INSTANCE) {
		this = eac_format_inexpr (str, sleft, tnode_nthsubof (expr, 0));
		this += eac_format_instr (str + this, sleft, " (");
		this += eac_format_inexpr (str + this, sleft, tnode_nthsubof (expr, 1));
		this += eac_format_instr (str + this, sleft, ")");
	}

	return this;
}
/*}}}*/
/*{{{  char *eac_format_expr (tnode_t *expr)*/
/*
 *	formats an escape analysis expression for human-readable display.
 */
char *eac_format_expr (tnode_t *expr)
{
	int slen = 1024;
	int sleft = slen - 1;
	char *str = (char *)smalloc (slen * sizeof (char));

	*str = '\0';
	eac_format_inexpr (str, &sleft, expr);

	return str;
}
/*}}}*/


/*{{{  static int eac_scopein_paramlist (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	scopes in parameters for a procedure definition;
 *	called directly as no specific fparam type exists.
 *	returns 0 on success, non-zero on failure.
 */
static int eac_scopein_paramlist (compops_t *cops, tnode_t **node, scope_t *ss)
{
	tnode_t **items;
	int nitems, i;
	
	if (!*node) {
		return 0;
	}
	if (!parser_islistnode (*node)) {
		/* not a list, probably singleton */
		*node = parser_buildlistnode (OrgFileOf (*node), *node, NULL);
	}

	items = parser_getlistitems (*node, &nitems);
	for (i=0; i<nitems; i++) {
		char *rawname;
		name_t *varname;
		tnode_t *namenode, *olditem = items[i];

		if (items[i]->tag != eac.tag_NAME) {
			scope_error (items[i], ss, "parameter not a name!");
			return 1;
		}

		rawname = tnode_nthhookof (items[i], 0);
		varname = name_addscopenamess (rawname, items[i], NULL, NULL, ss);
		namenode = tnode_createfrom (eac.tag_NCHANVAR, items[i], varname);
		SetNameNode (varname, namenode);
		items[i] = tnode_createfrom (eac.tag_VARDECL, olditem, namenode);

		/* free old param */
		tnode_free (olditem);
		ss->scoped++;
	}

	return 0;
}
/*}}}*/
/*{{{  static int eac_scopein_freevars (compops_t *cops, tnode_t *fvlist, scope_t *ss)*/
/*
 *	scopes in free-variables for a procedure definition -- these are free in escape sets or process compositions.
 *	returns 0 on success, non-zero on failure.
 */
static int eac_scopein_freevars (compops_t *cops, tnode_t *fvlist, scope_t *ss)
{
	tnode_t **items;
	int nitems, i;

	if (!fvlist) {
		return 0;
	}
	if (!parser_islistnode (fvlist)) {
		nocc_internal ("eac_scopein_freevars(): fvlist not list! (%s,%s)", fvlist->tag->name, fvlist->tag->ndef->name);
		return -1;
	}

	items = parser_getlistitems (fvlist, &nitems);
	for (i=0; i<nitems; i++) {
		char *rawname;
		name_t *varname;
		tnode_t *namenode, *olditem = items[i];

		if (items[i]->tag != eac.tag_NAME) {
			scope_error (items[i], ss, "parameter not a name!");
			return 1;
		}

		rawname = tnode_nthhookof (items[i], 0);
		varname = name_addscopenamess (rawname, items[i], NULL, NULL, ss);
		namenode = tnode_createfrom (eac.tag_NVAR, items[i], varname);
		SetNameNode (varname, namenode);
		items[i] = tnode_createfrom (eac.tag_VARDECL, olditem, namenode);

		/* free old name -- copy anyway */
		tnode_free (olditem);
		ss->scoped++;
	}

	return 0;
}
/*}}}*/
/*{{{  static int eac_scope_scanfreevars (tnode_t *node, void *arg)*/
/*
 *	looks for free name references inside processes, adds them to a list.
 *	returns 0 to stop walk, 1 to continue
 */
static int eac_scope_scanfreevars (tnode_t *node, void *arg)
{
	tnode_t *fvlist = (tnode_t *)arg;

#if 0
nocc_message ("eac_scope_scanfreevars(): looking at (%s)", node->tag->name);
#endif
	if (node->tag == eac.tag_NAME) {
		char *rawname = tnode_nthhookof (node, 0);
		tnode_t **xitems;
		int nxitems, i;

		xitems = parser_getlistitems (fvlist, &nxitems);
		for (i=0; i<nxitems; i++) {
			/* assert: fvlist is a list of eac_NAMEs */
			char *thisname = tnode_nthhookof (xitems[i], 0);

			if (!strcmp (thisname, rawname)) {
				/* got this name already */
				break;		/* for() */
			}
		}
		if (i == nxitems) {
			parser_addtolist (fvlist, tnode_copytree (node));
		}
	}
	return 1;
}
/*}}}*/
/*{{{  static int eac_scopein_declnode (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	scopes in a process definition
 *	returns 0 to stop walk, 1 to continue
 */
static int eac_scopein_declnode (compops_t *cops, tnode_t **node, scope_t *ss)
{
	tnode_t *name = tnode_nthsubof (*node, 0);
	void *nsmark;
	char *rawname;
	name_t *procname;
	tnode_t *newname;
	tnode_t *fvlist;
	int eac_lastunresolved = eac_ignore_unresolved;

	if (name->tag != eac.tag_NAME) {
		scope_error (name, ss, "eac_scopein_declnode(): declaration name not name! (%s,%s)", name->tag->name, name->tag->ndef->name);
		return 0;
	}
	rawname = tnode_nthhookof (name, 0);
	procname = name_addscopenamess (rawname, *node, NULL, NULL, ss);
	newname = tnode_createfrom (eac.tag_NPROCDEF, name, procname);
	SetNameNode (procname, newname);
	tnode_setnthsub (*node, 0, newname);

	/* free old name */
	tnode_free (name);
	ss->scoped++;

	nsmark = name_markscope ();
	/* scope parameters */
	eac_scopein_paramlist (cops, tnode_nthsubaddr (*node, 1), ss);

	/* scope body, primarily to pick out parameters */
	eac_ignore_unresolved = 1;
	scope_subtree (tnode_nthsubaddr (*node, 2), ss);
	eac_ignore_unresolved = eac_lastunresolved;

	/* scan body looking for leftover free variables */
	fvlist = parser_newlistnode (OrgFileOf (*node));
	tnode_prewalktree (tnode_nthsubof (*node, 2), eac_scope_scanfreevars, fvlist);

	/* scope in free variables and attach to tree */
	eac_scopein_freevars (cops, fvlist, ss);
	tnode_setnthsub (*node, 3, fvlist);

	/* scope body again */
	scope_subtree (tnode_nthsubaddr (*node, 2), ss);

	/* remove params and free-vars from visible scope */
	name_markdescope (nsmark);

	return 0;
}
/*}}}*/
/*{{{  static int eac_scopein_rawname (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	scopes in a free-floating name
 */
static int eac_scopein_rawname (compops_t *cops, tnode_t **node, scope_t *ss)
{
	tnode_t *name = *node;
	char *rawname;
	name_t *sname = NULL;

	if (name->tag != eac.tag_NAME) {
		scope_error (name, ss, "name not raw-name!");
		return 0;
	}
	rawname = tnode_nthhookof (name, 0);

#if 0
fprintf (stderr, "eac_scopein_rawname: here! rawname = \"%s\"\n", rawname);
#endif
	sname = name_lookupss (rawname, ss);
	if (sname) {
		/* resolved */
		*node = NameNodeOf (sname);

		tnode_free (name);
	} else {
		if (!eac_ignore_unresolved) {
			scope_error (name, ss, "unresolved name \"%s\"", rawname);
		}
		/* else we ignore this fact */
	}

	return 1;
}
/*}}}*/


/*{{{  static int eac_fetrans_declnode (compops_t *cops, tnode_t **tptr, fetrans_t *fe)*/
/*
 *	front-end transformations for procedure declarations
 *	returns 0 to stop walk, non-zero to continue
 */
static int eac_fetrans_declnode (compops_t *cops, tnode_t **tptr, fetrans_t *fe)
{
	tnode_t *name = tnode_nthsubof (*tptr, 0);
	name_t *nname = tnode_nthnameof (name, 0);
	eac_istate_t *istate = eac_getistate ();
	int i;

	for (i=0; i<DA_CUR (istate->procs); i++) {
		if (DA_NTHITEM (istate->procs, i) == nname) {
			/* already got this one */
			return 1;
		}
	}
	dynarray_add (istate->procs, nname);

	return 1;
}
/*}}}*/


/*{{{  static int eac_code_init_nodes (void)*/
/*
 *	initialises EAC declaration nodes
 *	returns 0 on success, non-zero on failure
 */
static int eac_code_init_nodes (void)
{
	tndef_t *tnd;
	int i;
	compops_t *cops;
	langops_t *lops;

	/*{{{  register named functions*/
	fcnlib_addfcn ("eac_nametoken_to_hook", (void *)eac_nametoken_to_hook, 1, 1);

	/*}}}*/
	/*{{{  eac:rawnamenode -- EACNAME*/
	i = -1;
	tnd = tnode_newnodetype ("eac:rawnamenode", &i, 0, 0, 1, TNF_NONE);			/* hooks: raw-name */
	tnd->hook_free = eac_rawnamenode_hook_free;
	tnd->hook_copy = eac_rawnamenode_hook_copy;
	tnd->hook_dumptree = eac_rawnamenode_hook_dumptree;
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (eac_scopein_rawname));
	tnd->ops = cops;

	i = -1;
	eac.tag_NAME = tnode_newnodetag ("EACNAME", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  eac:namenode -- EACNPROCDEF, EACNCHANVAR, EACNVAR*/
	i = -1;
	tnd = tnode_newnodetype ("eac:namenode", &i, 0, 1, 0, TNF_NONE);			/* names: name */
	cops = tnode_newcompops ();
	tnd->ops = cops;
	eac.node_NAMENODE = tnd;

	i = -1;
	eac.tag_NPROCDEF = tnode_newnodetag ("EACNPROCDEF", &i, tnd, NTF_NONE);
	i = -1;
	eac.tag_NCHANVAR = tnode_newnodetag ("EACNCHANVAR", &i, tnd, NTF_NONE);
	i = -1;
	eac.tag_NVAR = tnode_newnodetag ("EACNVAR", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  eac:actionnode -- EACINPUT, EACOUTPUT*/
	i = -1;
	tnd = tnode_newnodetype ("eac:actionnode", &i, 2, 0, 0, TNF_NONE);			/* subnodes: left, right */
	cops = tnode_newcompops ();
	tnd->ops = cops;

	i = -1;
	eac.tag_INPUT = tnode_newnodetag ("EACINPUT", &i, tnd, NTF_NONE);
	i = -1;
	eac.tag_OUTPUT = tnode_newnodetag ("EACOUTPUT", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  eac:varcomp -- EACVARCOMP*/
	i = -1;
	tnd = tnode_newnodetype ("eac:varcomp", &i, 2, 0, 0, TNF_NONE);				/* subnodes: left, right */
	cops = tnode_newcompops ();
	tnd->ops = cops;

	i = -1;
	eac.tag_VARCOMP = tnode_newnodetag ("EACVARCOMP", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  eac:declnode -- EACDECL*/
	i = -1;
	tnd = tnode_newnodetype ("eac:declnode", &i, 4, 0, 0, TNF_NONE);			/* subnodes: name, params, body, freevars */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (eac_scopein_declnode));
	tnode_setcompop (cops, "fetrans", 2, COMPOPTYPE (eac_fetrans_declnode));
	tnd->ops = cops;

	i = -1;
	eac.tag_DECL = tnode_newnodetag ("EACDECL", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  eac:eseqnode -- EACESEQ*/
	i = -1;
	tnd = tnode_newnodetype ("eac:eseqnode", &i, 1, 0, 0, TNF_NONE);			/* subnodes: escape-sequence-list */
	cops = tnode_newcompops ();
	tnd->ops = cops;

	i = -1;
	eac.tag_ESEQ = tnode_newnodetag ("EACESEQ", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  eac:esetnode -- EACESET*/
	i = -1;
	tnd = tnode_newnodetype ("eac:esetnode", &i, 2, 0, 0, TNF_NONE);			/* subnodes: free-var-decls, escape-sequences */
	cops = tnode_newcompops ();
	tnd->ops = cops;

	i = -1;
	eac.tag_ESET = tnode_newnodetag ("EACESET", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  eac:chanmark -- EACSVREND, EACCLIEND*/
	i = -1;
	tnd = tnode_newnodetype ("eac:chanmark", &i, 1, 0, 0, TNF_NONE);			/* subnodes: channel */
	cops = tnode_newcompops ();
	tnd->ops = cops;

	i = -1;
	eac.tag_SVREND = tnode_newnodetag ("EACSVREND", &i, tnd, NTF_NONE);
	i = -1;
	eac.tag_CLIEND = tnode_newnodetag ("EACCLIEND", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  eac:pcompnode -- EACPAR, EACHIDE*/
	i = -1;
	tnd = tnode_newnodetype ("eac:pcompnode", &i, 2, 0, 0, TNF_NONE);			/* subnodes: left, right */
	cops = tnode_newcompops ();
	tnd->ops = cops;

	i = -1;
	eac.tag_PAR = tnode_newnodetag ("EACPAR", &i, tnd, NTF_NONE);
	i = -1;
	eac.tag_HIDE = tnode_newnodetag ("EACHIDE", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  eac:instancenode -- EACINSTANCE*/
	i = -1;
	tnd = tnode_newnodetype ("eac:instancenode", &i, 2, 0, 0, TNF_NONE);			/* subnodes: name, params */
	cops = tnode_newcompops ();
	tnd->ops = cops;

	i = -1;
	eac.tag_INSTANCE = tnode_newnodetag ("EACINSTANCE", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  eac:typenode -- EACPROC, EACCHANVAR, EACVAR*/
	i = -1;
	tnd = tnode_newnodetype ("eac:typenode", &i, 0, 0, 0, TNF_NONE);
	cops = tnode_newcompops ();
	tnd->ops = cops;

	i = -1;
	eac.tag_PROC = tnode_newnodetag ("EACPROC", &i, tnd, NTF_NONE);
	i = -1;
	eac.tag_CHANVAR = tnode_newnodetag ("EACCHANVAR", &i, tnd, NTF_NONE);
	i = -1;
	eac.tag_VAR = tnode_newnodetag ("EACVAR", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  eac:vardeclnode -- EACVARDECL*/
	i = -1;
	tnd = tnode_newnodetype ("eac:vardeclnode", &i, 1, 0, 0, TNF_NONE);			/* subnodes: name */
	cops = tnode_newcompops ();
	tnd->ops = cops;

	i = -1;
	eac.tag_VARDECL = tnode_newnodetag ("EACVARDECL", &i, tnd, NTF_NONE);

	/*}}}*/

	return 0;
}
/*}}}*/


/*{{{  eac_code_feunit (feunit_t)*/
feunit_t eac_code_feunit = {
	.init_nodes = eac_code_init_nodes,
	.reg_reducers = NULL,
	.init_dfatrans = NULL,
	.post_setup = NULL,
	.ident = "eac-code"
};
/*}}}*/


