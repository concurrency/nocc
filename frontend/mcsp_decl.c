/*
 *	mcsp_decl.c -- declarations for MCSP
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
#include "fetrans.h"
#include "betrans.h"
#include "map.h"
#include "codegen.h"
#include "target.h"
#include "transputer.h"


/*}}}*/



/*{{{  */
/*
 *	initialises MCSP declaration nodes
 *	returns 0 on success, non-zero on failure
 */
static int mcsp_decl_init_nodes (void)
{
	tndef_t *tnd;
	int i;
	compops_t *cops;
	langops_t *lops;

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
	/*{{{  mcsp:leafnode -- RETURNADDRESS*/
	i = -1;
	tnd = tnode_lookupnodetype ("mcsp:leafnode");

	i = -1;
	mcsp.tag_RETURNADDRESS = tnode_newnodetag ("MCSPRETURNADDRESS", &i, tnd, NTF_NONE);

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

	return 0;
}
/*}}}*/
/*{{{  */
/*
 *	registers reducers for MCSP declaration nodes
 *	returns 0 on success, non-zero on failure
 */
static int mcsp_decl_reg_reducers (void)
{
	parser_register_grule ("mcsp:hidereduce", parser_decode_grule ("ST0T+@tN+N+0C3R-", mcsp.tag_HIDE));
	parser_register_grule ("mcsp:procdeclreduce", parser_decode_grule ("SN2N+N+N+>V0C4R-", mcsp.tag_PROCDECL));
	parser_register_grule ("mcsp:fixreduce", parser_decode_grule ("SN0N+N+VC2R-", mcsp.tag_FIXPOINT));

	return 0;
}
/*}}}*/
/*{{{  */
/*
 *	creates and returns DFA transition tables for MCSP declaration nodes
 */
static dfattbl_t **mcsp_decl_init_dfatrans (int *ntrans)
{
	DYNARRAY (dfattbl_t *, transtbl);

	dynarray_init (transtbl);
	dynarray_add (transtbl, dfa_bnftotbl ("mcsp:fparams ::= { mcsp:name @@, 0 }"));
	dynarray_add (transtbl, dfa_transtotbl ("mcsp:fixpoint ::= [ 0 @@@ 1 ] [ 1 mcsp:name 2 ] [ 2 @@. 3 ] [ 3 mcsp:process 4 ] [ 4 {<mcsp:fixreduce>} -* ]"));
	dynarray_add (transtbl, dfa_transtotbl ("mcsp:hide ::= [ 0 +@@\\ 1 ] [ 1 mcsp:eventset 2 ] [ 2 {<mcsp:hidereduce>} -* ]"));
	dynarray_add (transtbl, dfa_transtotbl ("mcsp:procdecl ::= [ 0 +Name 1 ] [ 1 {<mcsp:namepush>} ] [ 1 @@::= 2 ] [ 1 @@( 4 ] [ 2 {<mcsp:nullpush>} ] [ 2 mcsp:process 3 ] [ 3 {<mcsp:procdeclreduce>} -* ] " \
				"[ 4 mcsp:fparams 5 ] [ 5 @@) 6 ] [ 6 @@::= 7 ] [ 7 mcsp:process 3 ]"));

	*ntrans = DA_CUR (transtbl);
	return DA_PTR (transtbl);
}
/*}}}*/



/*{{{  mcsp_decl_feunit (feunit_t)*/
feunit_t mcsp_decl_feunit = {
	init_nodes: mcsp_decl_init_nodes,
	reg_reducers: mcsp_decl_reg_reducers,
	init_dfatrans: mcsp_decl_init_dfatrans,
	post_setup: NULL
};

/*}}}*/

