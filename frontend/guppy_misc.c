/*
 *	guppy_misc.c -- miscellaneous stuff for guppy
 *	Copyright (C) 2013 Fred Barnes <frmb@kent.ac.uk>
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
#include "fhandle.h"
#include "origin.h"
#include "symbols.h"
#include "keywords.h"
#include "lexer.h"
#include "lexpriv.h"
#include "tnode.h"
#include "parser.h"
#include "dfa.h"
#include "dfaerror.h"
#include "parsepriv.h"
#include "guppy.h"
#include "feunit.h"
#include "fcnlib.h"
#include "names.h"
#include "scope.h"
#include "prescope.h"
#include "library.h"
#include "typecheck.h"
#include "constprop.h"
#include "precheck.h"
#include "usagecheck.h"
#include "map.h"
#include "target.h"
#include "transputer.h"
#include "codegen.h"
#include "langops.h"
#include "fetrans.h"
#include "betrans.h"
#include "metadata.h"
#include "tracescheck.h"
#include "mobilitycheck.h"

/*}}}*/
/*{{{  private types*/


/*}}}*/


/*{{{  static int guppy_namemap_misc (compops_t *cops, tnode_t **nodep, map_t *map)*/
/*
 *	does name-mapping for a misc node
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_namemap_misc (compops_t *cops, tnode_t **nodep, map_t *map)
{
	return 0;
}
/*}}}*/
/*{{{  static int guppy_codegen_misc (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	does code-generation for a misc node
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_codegen_misc (compops_t *cops, tnode_t *node, codegen_t *cgen)
{
	if (node->tag == gup.tag_PPCOMMENT) {
		tnode_t *arg = tnode_nthsubof (node, 0);
		char *str = NULL;

		langops_getctypeof (arg, &str);
#if 0
fhandle_printf (FHAN_STDERR, "guppy_codegen_misc(): arg =\n");
tnode_dumptree (arg, 1, FHAN_STDERR);
#endif
		codegen_ssetindent (cgen);
		codegen_write_fmt (cgen, "/* PPCOMMENT:%s */\n", str ? str : "(invalid)");
		return 0;
	}
	return 1;
}
/*}}}*/


/*{{{  static int guppy_misc_init_nodes (void)*/
/*
 *	sets up misc node-types for Guppy
 *	returns 0 on success, non-zero on error
 */
static int guppy_misc_init_nodes (void)
{
	tndef_t *tnd;
	compops_t *cops;
	langops_t *lops;
	int i;

	/*{{{  guppy:misc -- PPCOMMENT*/
	i = -1;
	tnd = tnode_newnodetype ("guppy:misc", &i, 1, 0, 0, TNF_NONE);					/* subnodes: argument */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (guppy_namemap_misc));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (guppy_codegen_misc));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnd->lops = lops;

	i = -1;
	gup.tag_PPCOMMENT = tnode_newnodetag ("PPCOMMENT", &i, tnd, NTF_NONE);

	/*}}}*/

	return 0;
}
/*}}}*/
/*{{{  static int guppy_misc_post_setup (void)*/
/*
 *	called to do any post-setup on miscellaneous nodes
 *	returns 0 on success, non-zero on error
 */
static int guppy_misc_post_setup (void)
{
	return 0;
}
/*}}}*/


/*{{{  guppy_misc_feunit (feunit_t)*/
feunit_t guppy_misc_feunit = {
	.init_nodes = guppy_misc_init_nodes,
	.reg_reducers = NULL,
	.init_dfatrans = NULL,
	.post_setup = guppy_misc_post_setup,
	.ident = "guppy-misc"
};

/*}}}*/


