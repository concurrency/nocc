/*
 *	avrasm_hll.c -- handling for AVR assembler high-level constructs
 *	Copyright (C) 2012 Fred Barnes <frmb@kent.ac.uk>
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
#include "avrasm.h"
#include "feunit.h"
#include "names.h"
#include "scope.h"
#include "prescope.h"
#include "typecheck.h"
#include "constprop.h"
#include "usagecheck.h"
#include "map.h"
#include "codegen.h"
#include "target.h"
#include "avrinstr.h"

/*}}}*/
/*{{{  private types/data*/
/*}}}*/


/*{{{  static int avrasm_prescope_fcndefnode (compops_t *cops, tnode_t **tptr, prescope_t *ps)*/
/*
 *	does pre-scope for a function definition (fixes parameters)
 *	returns 0 to stop walk, 1 to continue
 */
static int avrasm_prescope_fcndefnode (compops_t *cops, tnode_t **tptr, prescope_t *ps)
{
	tnode_t **paramptr = tnode_nthsubaddr (*tptr, 1);

	if (!*paramptr) {
		/* no parameters, create empty list */
		*paramptr = parser_newlistnode ((*tptr)->org_file);
	}
	return 1;
}
/*}}}*/
/*{{{  static int avrasm_scopein_fcndefnode (compops_t *cops, tnode_t **tptr, scope_t *ss)*/
/*
 *	does scope-in for a function definition
 *	returns 0 to stop walk, 1 to continue
 */
static int avrasm_scopein_fcndefnode (compops_t *cops, tnode_t **tptr, scope_t *ss)
{
	tnode_t **namep = tnode_nthsubaddr (*tptr, 0);

	if ((*namep)->tag != avrasm.tag_NAME) {
		scope_error (*tptr, ss, "function name is not a name (got [%s])", (*namep)->tag->name);
	} else {
		char *rawname = (char *)tnode_nthhookof (*namep, 0);
		name_t *fname;
		tnode_t *namenode;
		void *nsmark = name_markscope ();

		/* scope parameters and body */
		tnode_modprepostwalktree (tnode_nthsubaddr (*tptr, 1), scope_modprewalktree, scope_modpostwalktree, (void *)ss);
		tnode_modprepostwalktree (tnode_nthsubaddr (*tptr, 2), scope_modprewalktree, scope_modpostwalktree, (void *)ss);

		name_markdescope (nsmark);

		fname = name_addscopenamess (rawname, *tptr, NULL, NULL, ss);
		namenode = tnode_createfrom (avrasm.tag_FCNNAME, *tptr, fname);
		SetNameNode (fname, namenode);

		tnode_free (*namep);
		*namep = namenode;

		ss->scoped++;
	}
	return 0;
}
/*}}}*/
/*{{{  static int avrasm_hlltypecheck_fcndefnode (compops_t *cops, tnode_t **tptr, hlltypecheck_t *hltc)*/
/*
 *	does high-level type check for a function definition
 *	returns 0 to stop walk, 1 to continue
 */
static int avrasm_hlltypecheck_fcndefnode (compops_t *cops, tnode_t **tptr, hlltypecheck_t *hltc)
{
	/* nothing to do here.. */
	return 1;
}
/*}}}*/
/*{{{  static int avrasm_scopein_fcnparamnode (compops_t *cops, tnode_t **tptr, scope_t *ss)*/
/*
 *	does scope-in for a function parameter (during function scope-in)
 *	returns 0 to stop walk, 1 to continue
 */
static int avrasm_scopein_fcnparamnode (compops_t *cops, tnode_t **tptr, scope_t *ss)
{
	tnode_t **namep = tnode_nthsubaddr (*tptr, 0);

	if ((*namep)->tag != avrasm.tag_NAME) {
		scope_error (*tptr, ss, "function parameter name is not a name (got [%s])", (*namep)->tag->name);
	} else {
		char *rawname = (char *)tnode_nthhookof (*namep, 0);
		name_t *fpname;
		tnode_t *namenode;

		fpname = name_addscopenamess (rawname, *tptr, NULL, NULL, ss);
		namenode = tnode_createfrom (avrasm.tag_FCNPARAMNAME, *tptr, fpname);
		SetNameNode (fpname, namenode);

		tnode_free (*namep);
		*namep = namenode;

		ss->scoped++;
	}
	return 1;
}
/*}}}*/
/*{{{  static int avrasm_hlltypecheck_fcnparamnode (compops_t *cops, tnode_t **tptr, hlltypecheck_t *hltc)*/
/*
 *	does high-level type check for a function parameter
 *	returns 0 to stop walk, 1 to continue
 */
static int avrasm_hlltypecheck_fcnparamnode (compops_t *cops, tnode_t **tptr, hlltypecheck_t *hltc)
{
	tnode_t *name = tnode_nthsubof (*tptr, 0);
	name_t *pname = NULL;
	tnode_t *expr = tnode_nthsubof (*tptr, 2);
	tnode_t **typep = tnode_nthsubaddr (*tptr, 1);

	if (name->tag != avrasm.tag_FCNPARAMNAME) {
		tnode_error (*tptr, "function parameter name not name, got [%s]", name->tag->name);
		hltc->errcount++;
		return 0;
	}
	pname = tnode_nthnameof (name, 0);
	if (!expr) {
		tnode_error (*tptr, "function parameter \"%s\" has no expression", NameNameOf (pname));
		hltc->errcount++;
		return 0;
	}
	if (*typep && ((*typep)->tag != avrasm.tag_SIGNED) && ((*typep)->tag != avrasm.tag_UNSIGNED)) {
		tnode_error (*tptr, "function parameter \"%s\" has badly specified type (got [%s])",
				NameNameOf (pname), (*typep)->tag->name);
		hltc->errcount++;
		return 0;
	}
	if (expr->tag == avrasm.tag_REGPAIR) {
		/*{{{  16-bit thing in two registers*/
		tnode_t *hreg = tnode_nthsubof (expr, 0);
		tnode_t *lreg = tnode_nthsubof (expr, 1);

		if (hreg->tag != avrasm.tag_LITREG) {
			tnode_error (*tptr, "bad register in expression for parameter \"%s\" (got [%s])",
					NameNameOf (pname), hreg->tag->name);
			hltc->errcount++;
			return 0;
		}
		if (lreg->tag != avrasm.tag_LITREG) {
			tnode_error (*tptr, "bad register in expression for parameter \"%s\" (got [%s])",
					NameNameOf (pname), lreg->tag->name);
			hltc->errcount++;
			return 0;
		}
		/* good so far, create 16-bit type for it */
		if (*typep) {
			if ((*typep)->tag == avrasm.tag_SIGNED) {
				tnode_free (*typep);
				*typep = tnode_createfrom (avrasm.tag_INT16, *tptr);
			} else {
				tnode_free (*typep);
				*typep = tnode_createfrom (avrasm.tag_UINT16, *tptr);
			}
		} else {
			/* default to signed */
			*typep = tnode_createfrom (avrasm.tag_INT16, *tptr);
		}
		/*}}}*/
	} else {
		/*{{{  else should be a single register*/
		if (expr->tag != avrasm.tag_LITREG) {
			tnode_error (*tptr, "bad register in expression for parameter \"%s\" (got [%s])",
					NameNameOf (pname), expr->tag->name);
			hltc->errcount++;
			return 0;
		}
		/* good so far, create 8-bit type for it */
		if (*typep) {
			if ((*typep)->tag == avrasm.tag_SIGNED) {
				tnode_free (*typep);
				*typep = tnode_createfrom (avrasm.tag_INT8, *tptr);
			} else {
				tnode_free (*typep);
				*typep = tnode_createfrom (avrasm.tag_UINT8, *tptr);
			}
		} else {
			/* default to signed */
			*typep = tnode_createfrom (avrasm.tag_INT8, *tptr);
		}
		/*}}}*/
	}
	SetNameType (pname, *typep);
	return 1;
}
/*}}}*/
/*{{{  static int avrasm_scopein_letdefnode (compops_t *cops, tnode_t **tptr, scope_t *ss)*/
/*
 *	does scope-in for a "let" definition
 *	returns 0 to stop walk, 1 to continue
 */
static int avrasm_scopein_letdefnode (compops_t *cops, tnode_t **tptr, scope_t *ss)
{
	tnode_t **namep = tnode_nthsubaddr (*tptr, 0);

	if ((*namep)->tag != avrasm.tag_NAME) {
		scope_error (*tptr, ss, "\"let\" name is not a name (got [%s])", (*namep)->tag->name);
	} else {
		char *rawname = (char *)tnode_nthhookof (*namep, 0);
		name_t *fpname;
		tnode_t *namenode;

		fpname = name_addscopenamess (rawname, *tptr, NULL, NULL, ss);
		namenode = tnode_createfrom (avrasm.tag_LETNAME, *tptr, fpname);
		SetNameNode (fpname, namenode);

		tnode_free (*namep);
		*namep = namenode;

		ss->scoped++;
	}
	return 1;
}
/*}}}*/
/*{{{  static int avrasm_hlltypecheck_letdefnode (compops_t *cops, tnode_t **tptr, hlltypecheck_t *hltc)*/
/*
 *	does high-level type check for a 'let' definition
 *	returns 0 to stop walk, 1 to continue
 */
static int avrasm_hlltypecheck_letdefnode (compops_t *cops, tnode_t **tptr, hlltypecheck_t *hltc)
{
	tnode_t *name = tnode_nthsubof (*tptr, 0);
	name_t *lname = NULL;
	tnode_t *expr = tnode_nthsubof (*tptr, 2);
	tnode_t **typep = tnode_nthsubaddr (*tptr, 1);

	if (name->tag != avrasm.tag_LETNAME) {
		tnode_error (*tptr, "let definition name not name, got [%s]", name->tag->name);
		hltc->errcount++;
		return 0;
	}
	lname = tnode_nthnameof (name, 0);
	if (!expr) {
		tnode_error (*tptr, "let definition \"%s\" has no expression", NameNameOf (lname));
		hltc->errcount++;
		return 0;
	}
	if (*typep && ((*typep)->tag != avrasm.tag_SIGNED) && ((*typep)->tag != avrasm.tag_UNSIGNED)) {
		tnode_error (*tptr, "let definition \"%s\" has badly specified type (got [%s])",
				NameNameOf (lname), (*typep)->tag->name);
		hltc->errcount++;
		return 0;
	}
	if (expr->tag == avrasm.tag_REGPAIR) {
		/*{{{  16-bit thing in two registers*/
		tnode_t *hreg = tnode_nthsubof (expr, 0);
		tnode_t *lreg = tnode_nthsubof (expr, 1);

		if (hreg->tag != avrasm.tag_LITREG) {
			tnode_error (*tptr, "bad register in expression for definition of \"%s\" (got [%s])",
					NameNameOf (lname), hreg->tag->name);
			hltc->errcount++;
			return 0;
		}
		if (lreg->tag != avrasm.tag_LITREG) {
			tnode_error (*tptr, "bad register in expression for definition of \"%s\" (got [%s])",
					NameNameOf (lname), lreg->tag->name);
			hltc->errcount++;
			return 0;
		}
		/* good so far, create 16-bit type for it */
		if (*typep) {
			if ((*typep)->tag == avrasm.tag_SIGNED) {
				tnode_free (*typep);
				*typep = tnode_createfrom (avrasm.tag_INT16, *tptr);
			} else {
				tnode_free (*typep);
				*typep = tnode_createfrom (avrasm.tag_UINT16, *tptr);
			}
		} else {
			/* default to signed */
			*typep = tnode_createfrom (avrasm.tag_INT16, *tptr);
		}
		/*}}}*/
	} else {
		/*{{{  else should be a single register*/
		if (expr->tag != avrasm.tag_LITREG) {
			tnode_error (*tptr, "bad register in expression for definition of \"%s\" (got [%s])",
					NameNameOf (lname), expr->tag->name);
			hltc->errcount++;
			return 0;
		}
		/* good so far, create 8-bit type for it */
		if (*typep) {
			if ((*typep)->tag == avrasm.tag_SIGNED) {
				tnode_free (*typep);
				*typep = tnode_createfrom (avrasm.tag_INT8, *tptr);
			} else {
				tnode_free (*typep);
				*typep = tnode_createfrom (avrasm.tag_UINT8, *tptr);
			}
		} else {
			/* default to signed */
			*typep = tnode_createfrom (avrasm.tag_INT8, *tptr);
		}
		/*}}}*/
	}
	SetNameType (lname, *typep);
	return 1;
}
/*}}}*/
/*{{{  static int avrasm_hllsimplify_hllinstr (compops_t *cops, tnode_t **tptr, hllsimplify_t *hls)*/
/*
 *	does simplifications for high-level instructions, replaces with a simplified list
 *	returns 0 to stop walk, 1 to continue
 */
static int avrasm_hllsimplify_hllinstr (compops_t *cops, tnode_t **tptr, hllsimplify_t *hls)
{
#if 0
fprintf (stderr, "avrasm_hllsimplify_hllinstr(): *tptr =\n");
tnode_dumptree (*tptr, 1, stderr);
#endif
	return 0;
}
/*}}}*/


/*{{{  static int avrasm_hll_init_nodes (void)*/
/*
 *	initialises nodes for high-level AVR assembler
 *	returns 0 on success, non-zero on failure
 */
static int avrasm_hll_init_nodes (void)
{
	tndef_t *tnd;
	int i;
	compops_t *cops;
	langops_t *lops;

	/*{{{  register reduction functions*/

	/*}}}*/
	/*{{{  avrasm:fcndefnode -- FCNDEF*/
	i = -1;
	tnd = tnode_newnodetype ("avrasm:fcndefnode", &i, 3, 0, 0, TNF_NONE);		/* subnodes: 0 = name, 1 = params, 2 = body */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "prescope", 2, COMPOPTYPE (avrasm_prescope_fcndefnode));
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (avrasm_scopein_fcndefnode));
	tnode_setcompop (cops, "hlltypecheck", 2, COMPOPTYPE (avrasm_hlltypecheck_fcndefnode));
	tnd->ops = cops;

	i = -1;
	avrasm.tag_FCNDEF = tnode_newnodetag ("AVRASMFCNDEF", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  avrasm:fcnparamnode -- FCNPARAM*/
	i = -1;
	tnd = tnode_newnodetype ("avrasm:fcnparamnode", &i, 3, 0, 0, TNF_NONE);		/* subnodes: 0 = name, 1 = type, 2 = expr */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (avrasm_scopein_fcnparamnode));
	tnode_setcompop (cops, "hlltypecheck", 2, COMPOPTYPE (avrasm_hlltypecheck_fcnparamnode));
	tnd->ops = cops;

	i = -1;
	avrasm.tag_FCNPARAM = tnode_newnodetag ("AVRASMFCNPARAM", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  avrasm:regpairnode -- REGPAIR*/
	i = -1;
	tnd = tnode_newnodetype ("avrasm:regpairnode", &i, 2, 0, 0, TNF_NONE);		/* subnodes: 0 = high-reg, 1 = low-reg */
	cops = tnode_newcompops ();
	tnd->ops = cops;

	i = -1;
	avrasm.tag_REGPAIR = tnode_newnodetag ("AVRASMREGPAIR", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  avrasm:letdefnode -- LETDEF*/
	i = -1;
	tnd = tnode_newnodetype ("avrasm:letdefnode", &i, 3, 0, 0, TNF_NONE);		/* subnodes: 0 = name, 1 = type, 2 = expr */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (avrasm_scopein_letdefnode));
	tnode_setcompop (cops, "hlltypecheck", 2, COMPOPTYPE (avrasm_hlltypecheck_letdefnode));
	tnd->ops = cops;

	i = -1;
	avrasm.tag_LETDEF = tnode_newnodetag ("AVRASMLETDEF", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  avrasm:hllinstr -- DLOAD, DSTORE*/
	i = -1;
	tnd = tnode_newnodetype ("avrasm:hllinstr", &i, 2, 0, 0, TNF_NONE);		/* subnodes: 0 = arg0, 1 = arg1 */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "hllsimplify", 2, COMPOPTYPE (avrasm_hllsimplify_hllinstr));
	tnd->ops = cops;

	i = -1;
	avrasm.tag_DLOAD = tnode_newnodetag ("AVRASMDLOAD", &i, tnd, NTF_NONE);
	i = -1;
	avrasm.tag_DSTORE = tnode_newnodetag ("AVRASMDSTORE", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  avrasm:hllnamenode -- FCNNAME, FCNPARAMNAME, LETNAME*/
	i = -1;
	tnd = tnode_newnodetype ("avrasm:hllnamenode", &i, 0, 1, 0, TNF_NONE);		/* namenodes: 0 = name */
	cops = tnode_newcompops ();
	tnd->ops = cops;

	i = -1;
	avrasm.tag_FCNNAME = tnode_newnodetag ("AVRASMFCNNAME", &i, tnd, NTF_NONE);
	i = -1;
	avrasm.tag_FCNPARAMNAME = tnode_newnodetag ("AVRASMFCNPARAMNAME", &i, tnd, NTF_NONE);
	i = -1;
	avrasm.tag_LETNAME = tnode_newnodetag ("AVRASMLETNAME", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  avrasm:hllleafnode -- INT8, UINT8, INT16, UINT16, SIGNED, UNSIGNED*/
	i = -1;
	tnd = tnode_newnodetype ("avrasm:hllleafnode", &i, 0, 0, 0, TNF_NONE);
	cops = tnode_newcompops ();
	tnd->ops = cops;

	i = -1;
	avrasm.tag_INT8 = tnode_newnodetag ("AVRASMINT8", &i, tnd, NTF_NONE);
	i = -1;
	avrasm.tag_UINT8 = tnode_newnodetag ("AVRASMUINT8", &i, tnd, NTF_NONE);
	i = -1;
	avrasm.tag_INT16 = tnode_newnodetag ("AVRASMINT16", &i, tnd, NTF_NONE);
	i = -1;
	avrasm.tag_UINT16 = tnode_newnodetag ("AVRASMUINT16", &i, tnd, NTF_NONE);
	i = -1;
	avrasm.tag_SIGNED = tnode_newnodetag ("AVRASMSIGNED", &i, tnd, NTF_NONE);
	i = -1;
	avrasm.tag_UNSIGNED = tnode_newnodetag ("AVRASMUNSIGNED", &i, tnd, NTF_NONE);

	/*}}}*/

	return 0;
}
/*}}}*/
/*{{{  static int avrasm_hll_post_setup (void)*/
/*
 *	does post-setup for high-level AVR assembler
 *	returns 0 on success, non-zero on failure
 */
static int avrasm_hll_post_setup (void)
{
	return 0;
}
/*}}}*/

/*{{{  avrasm_hll_feunit (feunit_t)*/

feunit_t avrasm_hll_feunit = {
	.init_nodes = avrasm_hll_init_nodes,
	.reg_reducers = NULL,
	.init_dfatrans = NULL,
	.post_setup = avrasm_hll_post_setup,
	.ident = "avrasm-hll"
};

/*}}}*/

