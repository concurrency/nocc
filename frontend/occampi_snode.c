/*
 *	occampi_snode.c -- occam-pi structured processes for NOCC (IF, ALT, etc.)
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
#include "typecheck.h"
#include "usagecheck.h"
#include "map.h"
#include "codegen.h"
#include "target.h"
#include "transputer.h"


/*}}}*/


/*{{{  static int occampi_codegen_snode (tnode_t *node, codegen_t *cgen)*/
/*
 *	does code-generation for structured process nodes
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_codegen_snode (tnode_t *node, codegen_t *cgen)
{
	if (node->tag == opi.tag_IF) {
		tnode_t *body = tnode_nthsubof (node, 1);
		tnode_t **bodies;
		int nbodies, i;
		int joinlab = codegen_new_label (cgen);

		if (!parser_islistnode (body)) {
			nocc_error ("occampi_codegen_snode(): body of IF not list!");
			return 0;
		}
		bodies = parser_getlistitems (body, &nbodies);

		for (i=0; i<nbodies; i++) {
			tnode_t *ifcond = tnode_nthsubof (bodies[i], 0);
			tnode_t *ifbody = tnode_nthsubof (bodies[i], 1);
			int skiplab = codegen_new_label (cgen);

			codegen_callops (cgen, loadname, ifcond, 0);
			codegen_callops (cgen, branch, I_CJ, skiplab);
			codegen_subcodegen (ifbody, cgen);
			codegen_callops (cgen, branch, I_J, joinlab);
			codegen_callops (cgen, setlabel, skiplab);
		}

		codegen_callops (cgen, tsecondary, I_SETERR);
		codegen_callops (cgen, setlabel, joinlab);
	}
	return 0;
}
/*}}}*/


/*{{{  static int occampi_snode_init_nodes (void)*/
/*
 *	initailises structured process nodes for occam-pi
 *	returns 0 on success, non-zero on failure
 */
static int occampi_snode_init_nodes (void)
{
	tndef_t *tnd;
	int i;
	compops_t *cops;

	/*{{{  occampi:snode -- IF, ALT, CASE*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:snode", &i, 3, 0, 0, TNF_LONGPROC);		/* subnodes: 0 = expr; 1 = body; 2 = replicator */
	cops = tnode_newcompops ();
	cops->codegen = occampi_codegen_snode;
	tnd->ops = cops;

	i = -1;
	opi.tag_ALT = tnode_newnodetag ("ALT", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_IF = tnode_newnodetag ("IF", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_CASE = tnode_newnodetag ("CASE", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  occampi:condnode -- CONDITIONAL*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:condnode", &i, 2, 0, 0, TNF_LONGPROC);	/* subnodes: 0 = expr; 1 = body */
	cops = tnode_newcompops ();
	tnd->ops = cops;

	i = -1;
	opi.tag_CONDITIONAL = tnode_newnodetag ("CONDITIONAL", &i, tnd, NTF_NONE);
	/*}}}*/

	return 0;
}
/*}}}*/
/*{{{  static int occampi_snode_reg_reducers (void)*/
/*
 *	registers reducers for structured process nodes
 */
static int occampi_snode_reg_reducers (void)
{
	parser_register_grule ("opi:altsnode", parser_decode_grule ("ST0T+@t000C3R-", opi.tag_ALT));
	parser_register_grule ("opi:ifcond", parser_decode_grule ("SN0N+0C2R-", opi.tag_CONDITIONAL));

	return 0;
}
/*}}}*/
/*{{{  static dfattbl_t **occampi_snode_init_dfatrans (int *ntrans)*/
/*
 *	creates and returns DFA transition tables for structured process nodes
 *	returns 0 on success, non-zero on failure
 */
static dfattbl_t **occampi_snode_init_dfatrans (int *ntrans)
{
	DYNARRAY (dfattbl_t *, transtbl);

	dynarray_init (transtbl);
	dynarray_add (transtbl, dfa_transtotbl ("occampi:snode +:= [ 0 +@ALT 1 ] [ 1 -Newline 2 ] [ 2 {<opi:altsnode>} -* ]"));
	dynarray_add (transtbl, dfa_transtotbl ("occampi:ifcond ::= [ 0 occampi:expr 1 ] [ 1 -Newline 2 ] [ 2 {<opi:ifcond>} -* ]"));

	*ntrans = DA_CUR (transtbl);
	return DA_PTR (transtbl);
}
/*}}}*/


/*{{{  cocampi_snode_feunit (feunit_t)*/
feunit_t occampi_snode_feunit = {
	init_nodes: occampi_snode_init_nodes,
	reg_reducers: occampi_snode_reg_reducers,
	init_dfatrans: occampi_snode_init_dfatrans,
	post_setup: NULL
};
/*}}}*/

