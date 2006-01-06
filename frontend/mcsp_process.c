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

	/*{{{  mcsp:process -- EVENT*/
	i = -1;
	tnd = tnode_newnodetype ("mcsp:process", &i, 1, 0, 0, TNF_NONE);		/* subnodes: 0 = event */
	cops = tnode_newcompops ();
	tnd->ops = cops;

	i = -1;
	mcsp.tag_EVENT = tnode_newnodetag ("EVENT", &i, tnd, NTF_NONE);

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
	dynarray_add (transtbl, dfa_transtotbl ("mcsp:process ::= [ 0 +Name 1 ] [ 1 {<mcsp:nullset>} -* ]"));

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

