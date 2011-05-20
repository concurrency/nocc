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


/*{{{  static int eac_scopein_rawname (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	scopes in a free-floating name
 */
static int eac_scopein_rawname (compops_t *cops, tnode_t **node, scope_t *ss)
{
	tnode_t *name = *node;
	char *rawname;
	name_t *sname = NULL;
#if 0
	eac_lex_t *lmp;
	eac_scope_t *mss = (eac_scope_t *)ss->langpriv;

	if ((*node)->org_file && (*node)->org_file->priv) {
		lexpriv_t *lp = (lexpriv_t *)(*node)->org_file->priv;
		
		lmp = (eac_lex_t *)lp->langpriv;
	} else {
		lmp = NULL;
	}
#endif

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

#if 0
		/* if it looks like a PROCDEF, turn into an INSTANCE -- if we're not already in an instance!*/
		if (!mss->inamescope && ((*node)->tag == eac.tag_PROCDEF)) {
			*node = tnode_createfrom (eac.tag_INSTANCE, name, *node, parser_newlistnode (NULL));
		}
#endif

		tnode_free (name);
	} else {
#if 0
fprintf (stderr, "eac_scopein_rawname(): unresolved name \"%s\", unbound-events = %d, mss->uvinsertlist = 0x%8.8x\n", rawname, lmp ? lmp->unboundvars : -1, (unsigned int)mss->uvinsertlist);
#endif
#if 0
		if (lmp && lmp->unboundvars) {
			if (mss && mss->uvinsertlist) {
				/*{{{  add the name manually*/
				tnode_t *decl = tnode_create (eac.tag_UPARAM, NULL, NULL, tnode_create (eac.tag_EVENTTYPE, NULL));
				tnode_t *newname;

				sname = name_addsubscopenamess (rawname, mss->uvscopemark, decl, NULL, name, ss);
				parser_addtolist (mss->uvinsertlist, decl);
				newname = tnode_createfrom (eac.tag_EVENT, decl, sname);
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
#endif
		scope_error (name, ss, "unresolved name \"%s\"", rawname);
	}

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
	/*{{{  mcsp:rawnamenode -- NAME*/
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


	return 0;
}
/*}}}*/


/*{{{  eac_code_feunit (feunit_t)*/
feunit_t eac_code_feunit = {
	init_nodes: eac_code_init_nodes,
	reg_reducers: NULL,
	init_dfatrans: NULL,
	post_setup: NULL,
	ident: "eac-code"
};
/*}}}*/


