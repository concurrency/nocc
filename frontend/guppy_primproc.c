/*
 *	guppy_primproc.c -- Guppy primitive processes
 *	Copyright (C) 2010-2015 Fred Barnes, University of Kent <frmb@kent.ac.uk>
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
#include "guppy.h"
#include "feunit.h"
#include "names.h"
#include "scope.h"
#include "prescope.h"
#include "typecheck.h"
#include "map.h"
#include "codegen.h"
#include "target.h"
#include "cccsp.h"


/*}}}*/


/*{{{  static void guppy_reduce_primproc (dfastate_t *dfast, parsepriv_t *pp, void *rarg)*/
/*
 *	reduces a primitive process
 */
static void guppy_reduce_primproc (dfastate_t *dfast, parsepriv_t *pp, void *rarg)
{
	token_t *tok;
	ntdef_t *tag;

	tok = parser_gettok (pp);
	if (tok->type == KEYWORD) {
		char *ustr = string_upper (tok->u.kw->name);

		tag = tnode_lookupnodetag (ustr);
		sfree (ustr);
		if (!tag) {
			parser_error (SLOCN (pp->lf), "unknown primitive process [%s] in guppy_reduce_primproc()", tok->u.kw->name);
			goto out_error;
		}
	} else {
		parser_error (SLOCN (pp->lf), "unknown primitive process in guppy_reduce_primproc()");
		goto out_error;
	}

	*(dfast->ptr) = tnode_create (tag, SLOCN (tok->origin));
out_error:
	lexer_freetoken (tok);

	return;
}
/*}}}*/


/*{{{  static int guppy_namemap_leafnode (compops_t *cops, tnode_t **nodep, map_t *mapdata)*/
/*
 *	called to do name-mapping on a primitive process
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_namemap_leafnode (compops_t *cops, tnode_t **nodep, map_t *mapdata)
{
	cccsp_mapdata_t *cmd = (cccsp_mapdata_t *)mapdata->hook;

	if ((*nodep)->tag == gup.tag_STOP) {
		/* turn into API call */
		tnode_t *newinst, *newparms, *callnum;

		newparms = parser_newlistnode (SLOCI);
		parser_addtolist (newparms, cmd->process_id);
		map_submapnames (&newparms, mapdata);
		callnum = cccsp_create_apicallname (STOP_PROC);
		newinst = tnode_createfrom (gup.tag_APICALL, *nodep, callnum, newparms);

		*nodep = newinst;
	} else if ((*nodep)->tag == gup.tag_SHUTDOWN) {
		/* turn into API call */
		tnode_t *newinst, *newparms, *callnum;

		newparms = parser_newlistnode (SLOCI);
		parser_addtolist (newparms, cmd->process_id);
		map_submapnames (&newparms, mapdata);
		callnum = cccsp_create_apicallname (SHUTDOWN);
		newinst = tnode_createfrom (gup.tag_APICALL, *nodep, callnum, newparms);

		*nodep = newinst;
	}
	return 0;
}
/*}}}*/
/*{{{  static int guppy_codegen_leafnode (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	called to do code-generation for a primitive process
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_codegen_leafnode (compops_t *cops, tnode_t *node, codegen_t *cgen)
{
	codegen_callops (cgen, debugline, node);
	return 0;
}
/*}}}*/


/*{{{  static int guppy_primproc_init_nodes (void)*/
/*
 *	initialises literal-nodes for occam-pi
 *	return 0 on success, non-zero on failure
 */
static int guppy_primproc_init_nodes (void)
{
	tndef_t *tnd;
	compops_t *cops;
	int i;

	/*{{{  register reduction functions*/
	fcnlib_addfcn ("guppy_reduce_primproc", guppy_reduce_primproc, 0, 3);

	/*}}}*/
	/*{{{  guppy:leafnode -- SKIP, STOP*/
	i = -1;
	tnd = gup.node_LEAFNODE = tnode_newnodetype ("guppy:leafnode", &i, 0, 0, 0, TNF_NONE);
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (guppy_namemap_leafnode));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (guppy_codegen_leafnode));
	tnd->ops = cops;


	i = -1;
	gup.tag_SKIP = tnode_newnodetag ("SKIP", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_STOP = tnode_newnodetag ("STOP", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_SHUTDOWN = tnode_newnodetag ("SHUTDOWN", &i, tnd, NTF_NONE);
	/*}}}*/

	return 0;
}
/*}}}*/


/*{{{  guppy_primproc_feunit (feunit_t)*/
feunit_t guppy_primproc_feunit = {
	.init_nodes = guppy_primproc_init_nodes,
	.reg_reducers = NULL,
	.init_dfatrans = NULL,
	.post_setup = NULL,
	.ident = "guppy-primproc"
};
/*}}}*/

