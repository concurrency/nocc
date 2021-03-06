/*
 *	avrasm_hll.c -- handling for AVR assembler high-level constructs
 *	Copyright (C) 2012-2016 Fred Barnes <frmb@kent.ac.uk>
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
#include <stdint.h>
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
#include "langops.h"
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
		*paramptr = parser_newlistnode ((*tptr)->org);
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
	/* Note: the function name itself will already be scoped, just need to do parameters and body here */
	void *nsmark = name_markscope ();

	/* scope parameters and body */
	tnode_modprepostwalktree (tnode_nthsubaddr (*tptr, 1), scope_modprewalktree, scope_modpostwalktree, (void *)ss);
	tnode_modprepostwalktree (tnode_nthsubaddr (*tptr, 2), scope_modprewalktree, scope_modpostwalktree, (void *)ss);

	name_markdescope (nsmark);

	return 0;
}
/*}}}*/
/*{{{  static int avrasm_llscope_fcndefnode (compops_t *cops, tnode_t **tptr, void *lls)*/
/*
 *	does local-label scoping for a function definition
 *	returns 0 to stop walk, 1 to continue
 */
static int avrasm_llscope_fcndefnode (compops_t *cops, tnode_t **tptr, void *lls)
{
	avrasm_ext_llscope_subtree (tnode_nthsubaddr (*tptr, 2), lls);
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
/*{{{  static int avrasm_hllsimplify_fcndefnode (compops_t *cops, tnode_t **tptr, hllsimplify_t *hls)*/
/*
 *	does simplifications for high-level function definition
 *	returns 0 to stop walk, 1 to continue
 */
static int avrasm_hllsimplify_fcndefnode (compops_t *cops, tnode_t **tptr, hllsimplify_t *hls)
{
	tnode_t *fcnname = tnode_nthsubof (*tptr, 0);
	name_t *curname;
	tnode_t *label, *lname, *rlist;

	/* mangle the name first, so if it occurs recursively, correct thing is done */
#if 0
fprintf (stderr, "avrasm_hllsimplify_fcndefnode(): did body, fcnname is:\n");
tnode_dumptree (fcnname, 1, stderr);
#endif
	curname = tnode_nthnameof (fcnname, 0);
	lname = tnode_createfrom (avrasm.tag_GLABEL, fcnname, curname);
	label = tnode_createfrom (avrasm.tag_GLABELDEF, *tptr, lname);
	SetNameDecl (curname, label);						/* switch to other declaration */
	SetNameNode (curname, lname);						/* and other name */

	/* just run over the body */
	avrasm_hllsimplify_subtree (tnode_nthsubaddr (*tptr, 2), hls);

	/* replace the function definition with its body, peppered with some labelling */
	rlist = parser_newlistnode (NULL);
	parser_addtolist (rlist, label);
	parser_addtolist (rlist, tnode_nthsubof (*tptr, 2));
	parser_addtolist (rlist, tnode_createfrom (avrasm.tag_INSTR, *tptr, avrasm_newlitins (*tptr, INS_RET), NULL, NULL, NULL));

	*tptr = rlist;

	return 0;
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
	tnode_t *newlist = parser_newlistnode (NULL);

	/* call simplify on both sides to flatten names to registers or pairs or registers */
	avrasm_hllsimplify_subtree (tnode_nthsubaddr (*tptr, 0), hls);
	avrasm_hllsimplify_subtree (tnode_nthsubaddr (*tptr, 1), hls);

#if 0
fprintf (stderr, "avrasm_hllsimplify_hllinstr(): *tptr =\n");
tnode_dumptree (*tptr, 1, stderr);
#endif
	if ((*tptr)->tag == avrasm.tag_DSTORE) {
		tnode_t *memref = tnode_nthsubof (*tptr, 0);
		tnode_t *srcref = tnode_nthsubof (*tptr, 1);

		if (srcref->tag == avrasm.tag_REGPAIR) {
			/* 16-bit store, little-endian style */
			tnode_t *hreg = tnode_nthsubof (srcref, 0);
			tnode_t *lreg = tnode_nthsubof (srcref, 1);

			if (memref->tag == avrasm.tag_ADD) {
				/*{{{  memory reference with offset */
				tnode_t *base = tnode_nthsubof (memref, 0);
				tnode_t *offset = tnode_nthsubof (memref, 1);
				int offsval;

				if (!langops_isconst (offset)) {
					tnode_error (*tptr, "non-constant offset in memory reference for \"dstore\" (got [%s])", offset->tag->name);
					hls->errcount++;
					return 0;
				}
				offsval = langops_constvalof (offset, NULL);

				/* base must be a 16-bit quantity (X,Y,Z, equiv pair or outright address) */
				if (base->tag == avrasm.tag_XYZREG) {
					// tnode_t *hstore = tnode_createfrom (avrasm.tag_INSTR, *tptr, avrasm_newlitins (*tptr, INS_ST),
					tnode_error (*tptr, "avrasm_hllsimplify_hllinstr(): FIXME! (shouldn't happen..)");
				} else if (base->tag == avrasm.tag_REGPAIR) {
					tnode_t *hareg = tnode_nthsubof (base, 0);
					tnode_t *lareg = tnode_nthsubof (base, 1);
					tnode_t *reghexpr = NULL;
					tnode_t *reglexpr = NULL;
					int xyzreg = 0;

					/* storing to memory specified by a register pair -- must be X, Y or Z */
					if ((avrasm_getlitregval (hareg) == 27) && (avrasm_getlitregval (lareg) == 26)) {
						/* X register */
						xyzreg = 0;
					} else if ((avrasm_getlitregval (hareg) == 29) && (avrasm_getlitregval (lareg) == 28)) {
						/* Y register */
						xyzreg = 1;
					} else if ((avrasm_getlitregval (hareg) == 31) && (avrasm_getlitregval (lareg) == 30)) {
						/* Z register */
						xyzreg = 2;
					} else {
						tnode_error (*tptr, "cannot \"dstore\" into non X/Y/Z referenced memory");
						hls->errcount++;
						return 0;
					}
					reghexpr = avrasm_newxyzreginfo (base, xyzreg, 0, offsval);
					reglexpr = avrasm_newxyzreginfo (base, xyzreg, 0, offsval + 1);

					parser_addtolist (newlist, tnode_createfrom (avrasm.tag_INSTR, *tptr, avrasm_newlitins (*tptr, INS_ST),
						reghexpr, tnode_copytree (hreg), NULL));
					parser_addtolist (newlist, tnode_createfrom (avrasm.tag_INSTR, *tptr, avrasm_newlitins (*tptr, INS_ST),
						reglexpr, tnode_copytree (lreg), NULL));
				} else if ((base->tag == avrasm.tag_GLABEL) || (base->tag == avrasm.tag_LLABEL)) {
					/* storing to memory specified directly (with an offset) */
					tnode_t *hdst = tnode_createfrom (avrasm.tag_ADD, memref, tnode_copytree (base), avrasm_newlitint (*tptr, offsval));
					tnode_t *ldst = tnode_createfrom (avrasm.tag_ADD, memref, tnode_copytree (base), avrasm_newlitint (*tptr, offsval + 1));

					parser_addtolist (newlist, tnode_createfrom (avrasm.tag_INSTR, *tptr, avrasm_newlitins (*tptr, INS_STS),
						hdst, tnode_copytree (hreg), NULL));
					parser_addtolist (newlist, tnode_createfrom (avrasm.tag_INSTR, *tptr, avrasm_newlitins (*tptr, INS_STS),
						ldst, tnode_copytree (lreg), NULL));
				} else {
					tnode_error (*tptr, "invalid memory reference for \"dstore\" (got [%s])", base->tag->name);
					hls->errcount++;
					return 0;
				}

				/*}}}*/
			} else if (memref->tag == avrasm.tag_REGPAIR) {
				/*{{{  memory reference as register pair (must be X,Y,Z or equivalent)*/
				tnode_t *hareg = tnode_nthsubof (memref, 0);
				tnode_t *lareg = tnode_nthsubof (memref, 1);
				tnode_t *reghexpr = NULL;
				tnode_t *reglexpr = NULL;
				int xyzreg = 0;

				/* storing to memory specified by a register pair -- must be X, Y or Z */
				if ((avrasm_getlitregval (hareg) == 27) && (avrasm_getlitregval (lareg) == 26)) {
					/* X register */
					xyzreg = 0;
				} else if ((avrasm_getlitregval (hareg) == 29) && (avrasm_getlitregval (lareg) == 28)) {
					/* Y register */
					xyzreg = 1;
				} else if ((avrasm_getlitregval (hareg) == 31) && (avrasm_getlitregval (lareg) == 30)) {
					/* Z register */
					xyzreg = 2;
				} else {
					tnode_error (*tptr, "cannot \"dstore\" into non X/Y/Z referenced memory");
					hls->errcount++;
					return 0;
				}
				reghexpr = avrasm_newxyzreginfo (memref, xyzreg, 0, 0);
				reglexpr = avrasm_newxyzreginfo (memref, xyzreg, 0, 1);

				parser_addtolist (newlist, tnode_createfrom (avrasm.tag_INSTR, *tptr, avrasm_newlitins (*tptr, INS_ST),
					reghexpr, tnode_copytree (hreg), NULL));
				parser_addtolist (newlist, tnode_createfrom (avrasm.tag_INSTR, *tptr, avrasm_newlitins (*tptr, INS_ST),
					reglexpr, tnode_copytree (lreg), NULL));
				/*}}}*/
			} else if ((memref->tag == avrasm.tag_GLABEL) || (memref->tag == avrasm.tag_LLABEL)) {
				/*{{{  memory reference as a label*/
				tnode_t *hdst = tnode_createfrom (avrasm.tag_ADD, memref, tnode_copytree (memref), avrasm_newlitint (*tptr, 0));
				tnode_t *ldst = tnode_createfrom (avrasm.tag_ADD, memref, tnode_copytree (memref), avrasm_newlitint (*tptr, 1));

				parser_addtolist (newlist, tnode_createfrom (avrasm.tag_INSTR, *tptr, avrasm_newlitins (*tptr, INS_STS),
					hdst, tnode_copytree (hreg), NULL));
				parser_addtolist (newlist, tnode_createfrom (avrasm.tag_INSTR, *tptr, avrasm_newlitins (*tptr, INS_STS),
					ldst, tnode_copytree (lreg), NULL));
				/*}}}*/
			} else {
				tnode_error (*tptr, "invalid memory reference for 16-bit \"dstore\", got [%s]", memref->tag->name);
				hls->errcount++;
				return 0;
			}
		} else if (srcref->tag == avrasm.tag_LITREG) {
			/* 8-bit store */
			if (memref->tag == avrasm.tag_ADD) {
				/*{{{  memory reference with offset */
				tnode_t *base = tnode_nthsubof (memref, 0);
				tnode_t *offset = tnode_nthsubof (memref, 1);
				int offsval;

				if (!langops_isconst (offset)) {
					tnode_error (*tptr, "non-constant offset in memory reference for \"dstore\" (got [%s])", offset->tag->name);
					hls->errcount++;
					return 0;
				}
				offsval = langops_constvalof (offset, NULL);

				/* base must be a 16-bit quantity (X,Y,Z, equiv pair or outright address) */
				if (base->tag == avrasm.tag_XYZREG) {
					tnode_error (*tptr, "avrasm_hllsimplify_hllinstr(): FIXME! (shouldn't happen..)");
				} else if (base->tag == avrasm.tag_REGPAIR) {
					tnode_t *hareg = tnode_nthsubof (base, 0);
					tnode_t *lareg = tnode_nthsubof (base, 1);
					tnode_t *regexpr = NULL;
					int xyzreg = 0;

					/* storing to memory specified by a register pair -- must be X, Y or Z */
					if ((avrasm_getlitregval (hareg) == 27) && (avrasm_getlitregval (lareg) == 26)) {
						/* X register */
						xyzreg = 0;
					} else if ((avrasm_getlitregval (hareg) == 29) && (avrasm_getlitregval (lareg) == 28)) {
						/* Y register */
						xyzreg = 1;
					} else if ((avrasm_getlitregval (hareg) == 31) && (avrasm_getlitregval (lareg) == 30)) {
						/* Z register */
						xyzreg = 2;
					} else {
						tnode_error (*tptr, "cannot \"dstore\" into non X/Y/Z referenced memory");
						hls->errcount++;
						return 0;
					}
					regexpr = avrasm_newxyzreginfo (base, xyzreg, 0, offsval);

					parser_addtolist (newlist, tnode_createfrom (avrasm.tag_INSTR, *tptr, avrasm_newlitins (*tptr, INS_ST),
						regexpr, tnode_copytree (srcref), NULL));
				} else if ((base->tag == avrasm.tag_GLABEL) || (base->tag == avrasm.tag_LLABEL)) {
					/* storing to memory specified directly (with an offset) */
					tnode_t *dst = tnode_createfrom (avrasm.tag_ADD, memref, tnode_copytree (base), avrasm_newlitint (*tptr, offsval));

					parser_addtolist (newlist, tnode_createfrom (avrasm.tag_INSTR, *tptr, avrasm_newlitins (*tptr, INS_STS),
						dst, tnode_copytree (srcref), NULL));
				} else {
					tnode_error (*tptr, "invalid memory reference for \"dstore\" (got [%s])", base->tag->name);
					hls->errcount++;
					return 0;
				}

				/*}}}*/
			} else if (memref->tag == avrasm.tag_REGPAIR) {
				/*{{{  memory reference as register pair (must be X,Y,Z or equivalent)*/
				tnode_t *hareg = tnode_nthsubof (memref, 0);
				tnode_t *lareg = tnode_nthsubof (memref, 1);
				tnode_t *regexpr = NULL;
				int xyzreg = 0;

				/* storing to memory specified by a register pair -- must be X, Y or Z */
				if ((avrasm_getlitregval (hareg) == 27) && (avrasm_getlitregval (lareg) == 26)) {
					/* X register */
					xyzreg = 0;
				} else if ((avrasm_getlitregval (hareg) == 29) && (avrasm_getlitregval (lareg) == 28)) {
					/* Y register */
					xyzreg = 1;
				} else if ((avrasm_getlitregval (hareg) == 31) && (avrasm_getlitregval (lareg) == 30)) {
					/* Z register */
					xyzreg = 2;
				} else {
					tnode_error (*tptr, "cannot \"dstore\" into non X/Y/Z referenced memory");
					hls->errcount++;
					return 0;
				}
				regexpr = avrasm_newxyzreginfo (memref, xyzreg, 0, 0);

				parser_addtolist (newlist, tnode_createfrom (avrasm.tag_INSTR, *tptr, avrasm_newlitins (*tptr, INS_ST),
					regexpr, tnode_copytree (srcref), NULL));
				/*}}}*/
			} else if ((memref->tag == avrasm.tag_GLABEL) || (memref->tag == avrasm.tag_LLABEL)) {
				/*{{{  memory reference as a label*/
				parser_addtolist (newlist, tnode_createfrom (avrasm.tag_INSTR, *tptr, avrasm_newlitins (*tptr, INS_STS),
					tnode_copytree (memref), tnode_copytree (srcref), NULL));

				/*}}}*/
			} else {
				tnode_error (*tptr, "invalid memory reference for 8-bit \"dstore\", got [%s]", memref->tag->name);
				hls->errcount++;
				return 0;
			}
		}
	} else if ((*tptr)->tag == avrasm.tag_DLOAD) {
		tnode_t *dstref = tnode_nthsubof (*tptr, 0);
		tnode_t *memref = tnode_nthsubof (*tptr, 1);

		if (dstref->tag == avrasm.tag_REGPAIR) {
			/* 16-bit load, little endian style */
			tnode_t *hreg = tnode_nthsubof (dstref, 0);
			tnode_t *lreg = tnode_nthsubof (dstref, 1);

			if (memref->tag == avrasm.tag_ADD) {
				/*{{{  memory reference with offset */
				tnode_t *base = tnode_nthsubof (memref, 0);
				tnode_t *offset = tnode_nthsubof (memref, 1);
				int offsval;

				if (!langops_isconst (offset)) {
					tnode_error (*tptr, "non-constant offset in memory reference for \"dload\" (got [%s])", offset->tag->name);
					hls->errcount++;
					return 0;
				}
				offsval = langops_constvalof (offset, NULL);

				/* base must be a 16-bit quantity (X,Y,Z, equiv pair or outright address) */
				if (base->tag == avrasm.tag_XYZREG) {
					tnode_error (*tptr, "avrasm_hllsimplify_hllinstr(): FIXME! (shouldn't happen..)");
				} else if (base->tag == avrasm.tag_REGPAIR) {
					tnode_t *hareg = tnode_nthsubof (base, 0);
					tnode_t *lareg = tnode_nthsubof (base, 1);
					tnode_t *reghexpr = NULL;
					tnode_t *reglexpr = NULL;
					int xyzreg = 0;

					/* loading from memory specified by a register pair -- must be X, Y or Z */
					if ((avrasm_getlitregval (hareg) == 27) && (avrasm_getlitregval (lareg) == 26)) {
						/* X register */
						xyzreg = 0;
					} else if ((avrasm_getlitregval (hareg) == 29) && (avrasm_getlitregval (lareg) == 28)) {
						/* Y register */
						xyzreg = 1;
					} else if ((avrasm_getlitregval (hareg) == 31) && (avrasm_getlitregval (lareg) == 30)) {
						/* Z register */
						xyzreg = 2;
					} else {
						tnode_error (*tptr, "cannot \"dload\" from non X/Y/Z referenced memory");
						hls->errcount++;
						return 0;
					}
					reghexpr = avrasm_newxyzreginfo (base, xyzreg, 0, offsval);
					reglexpr = avrasm_newxyzreginfo (base, xyzreg, 0, offsval + 1);

					parser_addtolist (newlist, tnode_createfrom (avrasm.tag_INSTR, *tptr, avrasm_newlitins (*tptr, INS_LD),
						tnode_copytree (hreg), reghexpr, NULL));
					parser_addtolist (newlist, tnode_createfrom (avrasm.tag_INSTR, *tptr, avrasm_newlitins (*tptr, INS_LD),
						tnode_copytree (lreg), reglexpr, NULL));
				} else if ((base->tag == avrasm.tag_GLABEL) || (base->tag == avrasm.tag_LLABEL)) {
					/* loading from memory specified directly (with an offset) */
					tnode_t *hsrc = tnode_createfrom (avrasm.tag_ADD, memref, tnode_copytree (base), avrasm_newlitint (*tptr, offsval));
					tnode_t *lsrc = tnode_createfrom (avrasm.tag_ADD, memref, tnode_copytree (base), avrasm_newlitint (*tptr, offsval + 1));

					parser_addtolist (newlist, tnode_createfrom (avrasm.tag_INSTR, *tptr, avrasm_newlitins (*tptr, INS_LDS),
						tnode_copytree (hreg), hsrc, NULL));
					parser_addtolist (newlist, tnode_createfrom (avrasm.tag_INSTR, *tptr, avrasm_newlitins (*tptr, INS_LDS),
						tnode_copytree (lreg), lsrc, NULL));
				} else {
					tnode_error (*tptr, "invalid memory reference for \"dload\" (got [%s])", base->tag->name);
					hls->errcount++;
					return 0;
				}

				/*}}}*/
			} else if (memref->tag == avrasm.tag_REGPAIR) {
				/*{{{  memory reference as register pair (must be X,Y,Z or equivalent)*/
				tnode_t *hareg = tnode_nthsubof (memref, 0);
				tnode_t *lareg = tnode_nthsubof (memref, 1);
				tnode_t *reghexpr = NULL;
				tnode_t *reglexpr = NULL;
				int xyzreg = 0;

				/* loading from memory specified by a register pair -- must be X, Y or Z */
				if ((avrasm_getlitregval (hareg) == 27) && (avrasm_getlitregval (lareg) == 26)) {
					/* X register */
					xyzreg = 0;
				} else if ((avrasm_getlitregval (hareg) == 29) && (avrasm_getlitregval (lareg) == 28)) {
					/* Y register */
					xyzreg = 1;
				} else if ((avrasm_getlitregval (hareg) == 31) && (avrasm_getlitregval (lareg) == 30)) {
					/* Z register */
					xyzreg = 2;
				} else {
					tnode_error (*tptr, "cannot \"dload\" from non X/Y/Z referenced memory");
					hls->errcount++;
					return 0;
				}
				reghexpr = avrasm_newxyzreginfo (memref, xyzreg, 0, 0);
				reglexpr = avrasm_newxyzreginfo (memref, xyzreg, 0, 1);

				parser_addtolist (newlist, tnode_createfrom (avrasm.tag_INSTR, *tptr, avrasm_newlitins (*tptr, INS_LD),
					tnode_copytree (hreg), reghexpr, NULL));
				parser_addtolist (newlist, tnode_createfrom (avrasm.tag_INSTR, *tptr, avrasm_newlitins (*tptr, INS_LD),
					tnode_copytree (lreg), reglexpr, NULL));
				/*}}}*/
			} else if ((memref->tag == avrasm.tag_GLABEL) || (memref->tag == avrasm.tag_LLABEL)) {
				/*{{{  memory reference as a label*/
				tnode_t *hsrc = tnode_createfrom (avrasm.tag_ADD, memref, tnode_copytree (memref), avrasm_newlitint (*tptr, 0));
				tnode_t *lsrc = tnode_createfrom (avrasm.tag_ADD, memref, tnode_copytree (memref), avrasm_newlitint (*tptr, 1));

				parser_addtolist (newlist, tnode_createfrom (avrasm.tag_INSTR, *tptr, avrasm_newlitins (*tptr, INS_LDS),
					tnode_copytree (hreg), hsrc, NULL));
				parser_addtolist (newlist, tnode_createfrom (avrasm.tag_INSTR, *tptr, avrasm_newlitins (*tptr, INS_LDS),
					tnode_copytree (lreg), lsrc, NULL));
				/*}}}*/
			} else {
				tnode_error (*tptr, "invalid memory reference for 16-bit \"dload\", got [%s]", memref->tag->name);
				hls->errcount++;
				return 0;
			}
		} else if (dstref->tag == avrasm.tag_LITREG) {
			/* 8-bit load */
			if (memref->tag == avrasm.tag_ADD) {
				/*{{{  memory reference with offset */
				tnode_t *base = tnode_nthsubof (memref, 0);
				tnode_t *offset = tnode_nthsubof (memref, 1);
				int offsval;

				if (!langops_isconst (offset)) {
					tnode_error (*tptr, "non-constant offset in memory reference for \"dload\" (got [%s])", offset->tag->name);
					hls->errcount++;
					return 0;
				}
				offsval = langops_constvalof (offset, NULL);

				/* base must be a 16-bit quantity (X,Y,Z, equiv pair or outright address) */
				if (base->tag == avrasm.tag_XYZREG) {
					tnode_error (*tptr, "avrasm_hllsimplify_hllinstr(): FIXME! (shouldn't happen..)");
				} else if (base->tag == avrasm.tag_REGPAIR) {
					tnode_t *hareg = tnode_nthsubof (base, 0);
					tnode_t *lareg = tnode_nthsubof (base, 1);
					tnode_t *regexpr = NULL;
					int xyzreg = 0;

					/* storing to memory specified by a register pair -- must be X, Y or Z */
					if ((avrasm_getlitregval (hareg) == 27) && (avrasm_getlitregval (lareg) == 26)) {
						/* X register */
						xyzreg = 0;
					} else if ((avrasm_getlitregval (hareg) == 29) && (avrasm_getlitregval (lareg) == 28)) {
						/* Y register */
						xyzreg = 1;
					} else if ((avrasm_getlitregval (hareg) == 31) && (avrasm_getlitregval (lareg) == 30)) {
						/* Z register */
						xyzreg = 2;
					} else {
						tnode_error (*tptr, "cannot \"dload\" into non X/Y/Z referenced memory");
						hls->errcount++;
						return 0;
					}
					regexpr = avrasm_newxyzreginfo (base, xyzreg, 0, offsval);

					parser_addtolist (newlist, tnode_createfrom (avrasm.tag_INSTR, *tptr, avrasm_newlitins (*tptr, INS_LD),
						tnode_copytree (dstref), regexpr, NULL));
				} else if ((base->tag == avrasm.tag_GLABEL) || (base->tag == avrasm.tag_LLABEL)) {
					/* loading from memory specified directly (with an offset) */
					tnode_t *src = tnode_createfrom (avrasm.tag_ADD, memref, tnode_copytree (base), avrasm_newlitint (*tptr, offsval));

					parser_addtolist (newlist, tnode_createfrom (avrasm.tag_INSTR, *tptr, avrasm_newlitins (*tptr, INS_LDS),
						tnode_copytree (dstref), src, NULL));
				} else {
					tnode_error (*tptr, "invalid memory reference for \"dload\" (got [%s])", base->tag->name);
					hls->errcount++;
					return 0;
				}

				/*}}}*/
			} else if (memref->tag == avrasm.tag_REGPAIR) {
				/*{{{  memory reference as register pair (must be X,Y,Z or equivalent)*/
				tnode_t *hareg = tnode_nthsubof (memref, 0);
				tnode_t *lareg = tnode_nthsubof (memref, 1);
				tnode_t *regexpr = NULL;
				int xyzreg = 0;

				/* loading from memory specified by a register pair -- must be X, Y or Z */
				if ((avrasm_getlitregval (hareg) == 27) && (avrasm_getlitregval (lareg) == 26)) {
					/* X register */
					xyzreg = 0;
				} else if ((avrasm_getlitregval (hareg) == 29) && (avrasm_getlitregval (lareg) == 28)) {
					/* Y register */
					xyzreg = 1;
				} else if ((avrasm_getlitregval (hareg) == 31) && (avrasm_getlitregval (lareg) == 30)) {
					/* Z register */
					xyzreg = 2;
				} else {
					tnode_error (*tptr, "cannot \"dload\" from non X/Y/Z referenced memory");
					hls->errcount++;
					return 0;
				}
				regexpr = avrasm_newxyzreginfo (memref, xyzreg, 0, 0);

				parser_addtolist (newlist, tnode_createfrom (avrasm.tag_INSTR, *tptr, avrasm_newlitins (*tptr, INS_LD),
					tnode_copytree (dstref), regexpr, NULL));
				/*}}}*/
			} else if ((memref->tag == avrasm.tag_GLABEL) || (memref->tag == avrasm.tag_LLABEL)) {
				/*{{{  memory reference as a label*/
				parser_addtolist (newlist, tnode_createfrom (avrasm.tag_INSTR, *tptr, avrasm_newlitins (*tptr, INS_LDS),
					tnode_copytree (dstref), tnode_copytree (memref), NULL));

				/*}}}*/
			} else {
				tnode_error (*tptr, "invalid memory reference for 8-bit \"dload\", got [%s]", memref->tag->name);
				hls->errcount++;
				return 0;
			}
		}
	}

	tnode_free (*tptr);
	*tptr = newlist;

	return 0;
}
/*}}}*/
/*{{{  static int avrasm_hllsimplify_hllnamenode (compops_t *cops, tnode_t **tptr, hllsimplify_t *hls)*/
/*
 *	does simplifications on a high-level name, replaces with registers or pairs thereof
 *	returns 0 to stop walk, 1 to continue
 */
static int avrasm_hllsimplify_hllnamenode (compops_t *cops, tnode_t **tptr, hllsimplify_t *hls)
{
	tnode_t *node = *tptr;

	if ((node->tag == avrasm.tag_FCNPARAMNAME) || (node->tag == avrasm.tag_LETNAME)) {
		name_t *name = tnode_nthnameof (node, 0);
		tnode_t *decl = NameDeclOf (name);
		tnode_t *expr = NULL;

		if (!decl) {
			tnode_error (*tptr, "missing declaration for \"%s\"!", NameNameOf (name));
			hls->errcount++;
			return 0;
		}
		/* Note: rely on the fact that LETDEF and FCNPARAM nodes have expression as subnode 2 */
		expr = tnode_nthsubof (decl, 2);
		if (!expr) {
			tnode_error (*tptr, "missing expression for \"%s\"!", NameNameOf (name));
			hls->errcount++;
			return 0;
		}
		*tptr = tnode_copytree (expr);
	}
	return 0;
}
/*}}}*/
/*{{{  static tnode_t *avrasm_gettype_hllnamenode (langops_t *lops, tnode_t *node, tnode_t *default_type)*/
/*
 *	gets the type of a high-level name
 */
static tnode_t *avrasm_gettype_hllnamenode (langops_t *lops, tnode_t *node, tnode_t *default_type)
{
	name_t *name = tnode_nthnameof (node, 0);
	tnode_t *type = NameTypeOf (name);

	if (!type) {
		return default_type;
	}
	return type;
}
/*}}}*/
/*{{{  static int avrasm_hllsimplify_insnode (compops_t *cops, tnode_t **tptr, hllsimplify_t *hls)*/
/*
 *	does simplifications on a regular instruction node, flattening out certain things (done fairly blindly!)
 *	returns 0 to stop walk, 1 to continue
 */
static int avrasm_hllsimplify_insnode (compops_t *cops, tnode_t **tptr, hllsimplify_t *hls)
{
	tnode_t *ins = tnode_nthsubof (*tptr, 0);
	int inscode = avrasm_getlitinsval (ins);
	tnode_t **arg0p = tnode_nthsubaddr (*tptr, 1);
	tnode_t **arg1p = tnode_nthsubaddr (*tptr, 2);
	tnode_t *newlist = NULL;

	/* do simplifications on arguments if valid -- reduces named things to pairs/etc. */
	if (*arg0p) {
		avrasm_hllsimplify_subtree (arg0p, hls);
	}
	if (*arg1p) {
		avrasm_hllsimplify_subtree (arg1p, hls);
	}

	/* see if one of the arguments looks like a register pair or function name */
	if (*arg1p && ((*arg1p)->tag == avrasm.tag_REGPAIR)) {
		tnode_t **hrptr, **lrptr;

		if (inscode != INS_MOVW) {
			tnode_error (*tptr, "avrasm_hllsimplify_insnode(): unsupported instruction %d for regpair in second argument", inscode);
			hls->errcount++;
			return 0;
		}

		hrptr = tnode_nthsubaddr (*arg1p, 0);
		lrptr = tnode_nthsubaddr (*arg1p, 1);

		if (((*hrptr)->tag == avrasm.tag_LITREG) && ((*lrptr)->tag == avrasm.tag_LITREG)) {
			int hreg = avrasm_getlitregval (*hrptr);
			int lreg = avrasm_getlitregval (*lrptr);

			if ((hreg != (lreg + 1)) || (lreg & 1)) {
				tnode_error (*tptr, "avrasm_hllsimplify_insnode(): mismatched pair for MOVW source\n");
				hls->errcount++;
			} else {
				/* swap REGPAIR for new single (low) register */
				tnode_free (*arg1p);
				*arg1p = avrasm_newlitreg (*tptr, lreg);
			}
		} else {
			tnode_error (*tptr, "avrasm_hllsimplify_insnode(): not registers in second argument to MOVW\n");
			hls->errcount++;
		}
	}

	if (*arg0p && ((*arg0p)->tag == avrasm.tag_REGPAIR)) {
		switch (inscode) {
		case INS_SBIW:
		case INS_ADIW:
			/* special case, only allow r25:r24, r27:r26(X), r29:r28(Y), r31:r30(Z) */
			{
				tnode_t **hrptr, **lrptr;

#if 0
fprintf (stderr, "avrasm_hllsimplify_insnode(): SBIW/ADIW with regpair:\n");
tnode_dumptree (*arg0p, 1, stderr);
#endif
				hrptr = tnode_nthsubaddr (*arg0p, 0);
				lrptr = tnode_nthsubaddr (*arg0p, 1);
				if (((*hrptr)->tag == avrasm.tag_LITREG) && ((*lrptr)->tag == avrasm.tag_LITREG)) {
					int hreg = avrasm_getlitregval (*hrptr);
					int lreg = avrasm_getlitregval (*lrptr);

					if ((hreg != (lreg + 1)) || (lreg & 1)) {
						tnode_error (*tptr, "avrasm_hllsimplify_insnode(): mismatched pair for SBIW/ADIW\n");
						hls->errcount++;
					} else {
						/* swap REGPAIR for new single (low) register */
						tnode_free (*arg0p);
						*arg0p = avrasm_newlitreg (*tptr, lreg);
					}
				} else {
					tnode_error (*tptr, "avrasm_hllsimplify_insnode(): not registers in first argument to SBIW/ADIW\n");
					hls->errcount++;
				}
			}
			break;
		case INS_MOVW:
			/* allow any combination of R+1:R registers, reduce to even number (low) */
			{
				tnode_t **hrptr, **lrptr;

				hrptr = tnode_nthsubaddr (*arg0p, 0);
				lrptr = tnode_nthsubaddr (*arg0p, 1);

				if (((*hrptr)->tag == avrasm.tag_LITREG) && ((*lrptr)->tag == avrasm.tag_LITREG)) {
					int hreg = avrasm_getlitregval (*hrptr);
					int lreg = avrasm_getlitregval (*lrptr);

					if ((hreg != (lreg + 1)) || (lreg & 1)) {
						tnode_error (*tptr, "avrasm_hllsimplify_insnode(): mismatched pair for MOVW\n");
						hls->errcount++;
					} else {
						/* swap REGPAIR for new single (low) register */
						tnode_free (*arg0p);
						*arg0p = avrasm_newlitreg (*tptr, lreg);
					}
				} else {
					tnode_error (*tptr, "avrasm_hllsimplify_insnode(): not registers in first argument to MOVW\n");
					hls->errcount++;
				}
			}
			break;
		case INS_PUSH:
			/* push individually onto the stack, high first */
			newlist = parser_newlistnode (NULL);
			parser_addtolist (newlist, tnode_createfrom (avrasm.tag_INSTR, *tptr, avrasm_newlitins (*tptr, INS_PUSH),
					tnode_copytree (tnode_nthsubof (*arg0p, 0)), NULL, NULL));
			parser_addtolist (newlist, tnode_createfrom (avrasm.tag_INSTR, *tptr, avrasm_newlitins (*tptr, INS_PUSH),
					tnode_copytree (tnode_nthsubof (*arg0p, 1)), NULL, NULL));
			break;
		case INS_POP:
			/* pop individually from the stack, low first */
			newlist = parser_newlistnode (NULL);
			parser_addtolist (newlist, tnode_createfrom (avrasm.tag_INSTR, *tptr, avrasm_newlitins (*tptr, INS_POP),
					tnode_copytree (tnode_nthsubof (*arg0p, 1)), NULL, NULL));
			parser_addtolist (newlist, tnode_createfrom (avrasm.tag_INSTR, *tptr, avrasm_newlitins (*tptr, INS_POP),
					tnode_copytree (tnode_nthsubof (*arg0p, 0)), NULL, NULL));
			break;
		case INS_LDI:
			/* loading 16-bits of immediate data, high and low */
			if (!(*arg1p)) {
				tnode_error (*tptr, "avrasm_hllsimplify_insnode(): broken LDI instruction");
				hls->errcount++;
				return 0;
			} else {
				tnode_t *a1copy, *a2copy;

				if (((*arg1p)->tag->ndef == avrasm.node_NAMENODE) || ((*arg1p)->tag->ndef == avrasm.node_HLLNAMENODE)) {
					a1copy = *arg1p;
					a2copy = *arg1p;
					*arg1p = NULL;
				} else {
					a1copy = tnode_copytree (*arg1p);
					a2copy = tnode_copytree (*arg1p);
				}

				newlist = parser_newlistnode (NULL);
				parser_addtolist (newlist, tnode_createfrom (avrasm.tag_INSTR, *tptr, avrasm_newlitins (*tptr, INS_LDI),
						tnode_copytree (tnode_nthsubof (*arg0p, 0)),
						tnode_createfrom (avrasm.tag_HI, *tptr, a1copy),
						NULL));
				parser_addtolist (newlist, tnode_createfrom (avrasm.tag_INSTR, *tptr, avrasm_newlitins (*tptr, INS_LDI),
						tnode_copytree (tnode_nthsubof (*arg0p, 1)),
						tnode_createfrom (avrasm.tag_LO, *tptr, a2copy),
						NULL));
			}
			break;
		default:
			tnode_error (*tptr, "avrasm_hllsimplify_insnode(): unsupported instruction %d", inscode);
			hls->errcount++;
			return 0;
		}
	} else if (*arg0p && ((*arg0p)->tag == avrasm.tag_FCNNAME)) {
		/* blindly turn into GLABEL */
		*arg0p = tnode_createfrom (avrasm.tag_GLABEL, *arg0p, tnode_nthnameof (*arg0p, 0));
	}

	if (newlist) {
#if 0
fprintf (stderr, "avrasm_hllsimplify_insnode(): replacing:\n");
tnode_dumptree (*tptr, 1, stderr);
fprintf (stderr, "with:\n");
tnode_dumptree (newlist, 1, stderr);
#endif
		tnode_free (*tptr);
		*tptr = newlist;
	}
	/* these get called individually, so no need to walk further */
	return 0;
}
/*}}}*/
/*{{{  static tnode_t *avrasm_gettype_hllleafnode (langops_t *lops, tnode_t *node, tnode_t *default_type)*/
/*
 *	called to get the type of a high-level leaf node
 */
static tnode_t *avrasm_gettype_hllleafnode (langops_t *lops, tnode_t *node, tnode_t *default_type)
{
	return node;
}
/*}}}*/
/*{{{  static int avrasm_hlltypecheck_hllexpnode (compops_t *cops, tnode_t **tptr, hlltypecheck_t *hltc)*/
/*
 *	called to do high-level type checking on expression nodes
 *	returns 0 to stop walk, 1 to continue
 */
static int avrasm_hlltypecheck_hllexpnode (compops_t *cops, tnode_t **tptr, hlltypecheck_t *hltc)
{
	tnode_t **typep = tnode_nthsubaddr (*tptr, 2);
	tnode_t **lhsp = tnode_nthsubaddr (*tptr, 0);
	tnode_t **rhsp = tnode_nthsubaddr (*tptr, 1);
	tnode_t *lhstype, *rhstype;
	ntdef_t *ttag;

	if (*typep) {
		/* already got a type */
		return 0;
	}
	/* sub type-check left and right arguments */
	avrasm_hlltypecheck_subtree (lhsp, hltc);
	avrasm_hlltypecheck_subtree (rhsp, hltc);

	lhstype = typecheck_gettype (*lhsp, NULL);
	if (!lhstype) {
		/* see if we can get a type from the RHS */
		rhstype = typecheck_gettype (*rhsp, NULL);

		if (!rhstype) {
			tnode_error (*tptr, "failed to determine type of expression [%s]", (*tptr)->tag->name);
			hltc->errcount++;
			return 0;
		}

		lhstype = typecheck_gettype (*lhsp, rhstype);
		if (!lhstype) {
			tnode_error (*tptr, "failed to resolve type for expression [%s]", (*lhsp)->tag->name);
			hltc->errcount++;
			return 0;
		}
	} else {
		rhstype = typecheck_gettype (*rhsp, lhstype);

		if (!rhstype) {
			tnode_error (*tptr, "failed to resolve type for expression [%s]", (*rhsp)->tag->name);
			hltc->errcount++;
			return 0;
		}
	}

	/* requirements depend on particular expression operation */
	ttag = (*tptr)->tag;
	if ((ttag == avrasm.tag_EXPADD) || (ttag == avrasm.tag_EXPSUB) ||
			(ttag == avrasm.tag_EXPMUL) || (ttag == avrasm.tag_EXPDIV) || (ttag == avrasm.tag_EXPREM) ||
			(ttag == avrasm.tag_EXPBITOR) || (ttag == avrasm.tag_EXPBITXOR) || (ttag == avrasm.tag_EXPBITAND)) {
		if (lhstype->tag == rhstype->tag) {
			/* same type :) */
			*typep = tnode_copytree (lhstype);
		} else {
			tnode_error (*tptr, "incompatible types for expression [%s]", ttag->name);
			hltc->errcount++;
			return 0;
		}
	} else if ((ttag == avrasm.tag_EXPEQ) || (ttag == avrasm.tag_EXPNEQ) ||
			(ttag == avrasm.tag_EXPLT) || (ttag == avrasm.tag_EXPGT) ||
			(ttag == avrasm.tag_EXPLE) || (ttag == avrasm.tag_EXPGE)) {
		/* boolean operation, types must match and CC results */
		if (lhstype->tag == rhstype->tag) {
			*typep = tnode_copytree (lhstype);			/* leave as operation type -- CC is obvious */
			// *typep = tnode_createfrom (avrasm.tag_CC, *tptr);
		} else {
			tnode_error (*tptr, "incompatible types for expression [%s]", ttag->name);
			hltc->errcount++;
			return 0;
		}
	}

	return 0;
}
/*}}}*/
/*{{{  static int avrasm_hllsimplify_hllexpnode (compops_t *cops, tnode_t **tptr, hllsimplify_t *hls)*/
/*
 *	called to do simplifications on an expression node -- expands out
 *	returns 0 to stop walk, 1 to continue
 */
static int avrasm_hllsimplify_hllexpnode (compops_t *cops, tnode_t **tptr, hllsimplify_t *hls)
{
	ntdef_t *ttag;
	tnode_t *ilist = parser_newlistnode ((*tptr)->org);
	tnode_t *type;

	ttag = (*tptr)->tag;

	avrasm_hllsimplify_subtree (tnode_nthsubaddr (*tptr, 0), hls);
	avrasm_hllsimplify_subtree (tnode_nthsubaddr (*tptr, 1), hls);

	type = tnode_nthsubof (*tptr, 2);
#if 0
fprintf (stderr, "avrasm_hllsimplify_hllexpnode(): eocond_label=%p, expr_target=%p, *tptr=\n", hls->eocond_label, hls->expr_target);
tnode_dumptree (*tptr, 1, stderr);
#endif
	if ((ttag == avrasm.tag_EXPEQ) || (ttag == avrasm.tag_EXPNEQ) || (ttag == avrasm.tag_EXPLT) || (ttag == avrasm.tag_EXPGT) ||
			(ttag == avrasm.tag_EXPLE) || (ttag == avrasm.tag_EXPGE)) {
		int invjump = INS_INVALID;

		if (ttag == avrasm.tag_EXPEQ) {
			invjump = INS_BRNE;
		} else if (ttag == avrasm.tag_EXPNEQ) {
			invjump = INS_BREQ;
		} else if (ttag == avrasm.tag_EXPLT) {
			invjump = INS_BRGE;
		}
		/* assume left+right are registers */
		parser_addtolist (ilist, tnode_createfrom (avrasm.tag_INSTR, *tptr, avrasm_newlitins (*tptr, INS_CP),
				tnode_nthsubof (*tptr, 0), tnode_nthsubof (*tptr, 1), NULL));
		if (hls->eocond_label) {
			parser_addtolist (ilist, tnode_createfrom (avrasm.tag_INSTR, *tptr, avrasm_newlitins (*tptr, INS_BRNE),
					tnode_copytree (hls->eocond_label), NULL, NULL));
		} else {
			/* FIXME ... */
		}
	}

	*tptr = ilist;

	return 0;
}
/*}}}*/
/*{{{  static tnode_t *avrasm_gettype_hllexpnode (langops_t *lops, tnode_t *node, tnode_t *default_type)*/
/*
 *	gets the type of a high-level expression node
 *	returns type on success, NULL on failure
 */
static tnode_t *avrasm_gettype_hllexpnode (langops_t *lops, tnode_t *node, tnode_t *default_type)
{
	tnode_t *type = tnode_nthsubof (node, 2);

	if (!type) {
		return default_type;
	}
	return type;
}
/*}}}*/
/*{{{  static int avrasm_hlltypecheck_hllifnode (compops_t *cops, tnode_t **tptr, hlltypecheck_t *hltc)*/
/*
 *	called to do high-level type checking on structured 'if' nodes
 *	returns 0 to stop walk, 1 to continue
 */
static int avrasm_hlltypecheck_hllifnode (compops_t *cops, tnode_t **tptr, hlltypecheck_t *hltc)
{
	tnode_t *clist = tnode_nthsubof (*tptr, 0);
	tnode_t **items;
	int nitems, i;

	if (!parser_islistnode (clist)) {
		tnode_error (*tptr, "deformed condition list, got [%s]", clist->tag->name);
		hltc->errcount++;
		return 0;
	}
	/* check each condition is a HLLCONDNODE */
	items = parser_getlistitems (clist, &nitems);
	for (i=0; i<nitems; i++) {
		if (items[i]->tag != avrasm.tag_HLLCOND) {
			tnode_error (items[i], "item in \'if\' is not a condition, got [%s]", items[i]->tag->name);
			hltc->errcount++;
			return 0;
		}
	}

	return 1;
}
/*}}}*/
/*{{{  static int avrasm_hllsimplify_hllifnode (compops_t *cops, tnode_t **tptr, hllsimplify_t *hls)*/
/*
 *	called to simplify an 'if' node (flattens into local things)
 *	returns 0 to stop walk, 1 to continue
 */
static int avrasm_hllsimplify_hllifnode (compops_t *cops, tnode_t **tptr, hllsimplify_t *hls)
{
	tnode_t *clist = tnode_nthsubof (*tptr, 0);
	tnode_t **items;
	int nitems, i;
	tnode_t *ilist = parser_newlistnode (clist->org);
	tnode_t *eoif_decl = NULL;
	tnode_t *eoif_lab = NULL;
	tnode_t *hls_eoif_label = hls->eoif_label;

	avrasm_newtemplabel (*tptr, &eoif_decl, &eoif_lab);
	hls->eoif_label = eoif_lab;

	items = parser_getlistitems (clist, &nitems);
	for (i=0; i<nitems; i++) {
		/* call simplify on conditional */
		avrasm_hllsimplify_subtree (&(items[i]), hls);

		parser_addtolist (ilist, items[i]);
		items[i] = NULL;
	}

	parser_addtolist (ilist, eoif_decl);
	tnode_free (*tptr);
	*tptr = ilist;

	hls->eoif_label = hls_eoif_label;

	return 0;
}
/*}}}*/
/*{{{  static int avrasm_hlltypecheck_hllcondnode (compops_t *cops, tnode_t **tptr, hlltypecheck_t *hltc)*/
/*
 *	called to do high-level type checking on conditional nodes
 *	returns 0 to stop walk, 1 to continue
 */
static int avrasm_hlltypecheck_hllcondnode (compops_t *cops, tnode_t **tptr, hlltypecheck_t *hltc)
{
	tnode_t **exprp = tnode_nthsubaddr (*tptr, 0);
	tnode_t *etype = NULL;
	tnode_t *dfltype = tnode_create (avrasm.tag_CC, NULL);

	if (!*exprp) {
		/* must be "else" case */
		return 1;
	}
	avrasm_hlltypecheck_subtree (exprp, hltc);

	etype = typecheck_gettype (*exprp, dfltype);

#if 0
	if (etype->tag != avrasm.tag_CC) {
		tnode_error (*tptr, "invalid type for condition, got [%s]", etype->tag->name);
		hltc->errcount++;
		return 0;
	}
#endif
#if 0
fprintf (stderr, "avrasm_hlltypecheck_hllcondnode(): etype is:\n");
tnode_dumptree (etype, 1, stderr);
#endif
	
	tnode_free (dfltype);

	return 1;
}
/*}}}*/
/*{{{  static int avrasm_hllsimplify_hllcondnode (compops_t *cops, tnode_t **tptr, hllsimplify_t *hls)*/
/*
 *	called to simplify a conditional node (flatten out)
 *	returns 0 to stop walk, 1 to continue
 */
static int avrasm_hllsimplify_hllcondnode (compops_t *cops, tnode_t **tptr, hllsimplify_t *hls)
{
	tnode_t *expr = tnode_nthsubof (*tptr, 0);
	tnode_t *ilist;

	if (!hls->eoif_label) {
		tnode_error (*tptr, "here");
		nocc_internal ("avrasm_hllsimplify_hllcondnode(): outside 'if' context!");
		return 0;
	}

	ilist = parser_newlistnode ((*tptr)->org);

	if (!expr) {
		/* empty expression -- must be 'else' case */
		tnode_t **bodyp = tnode_nthsubaddr (*tptr, 1);

		avrasm_hllsimplify_subtree (bodyp, hls);

		parser_addtolist (ilist, *bodyp);
		*bodyp = NULL;
	} else {
		tnode_t **exprp = tnode_nthsubaddr (*tptr, 0);
		tnode_t **bodyp = tnode_nthsubaddr (*tptr, 1);
		tnode_t *eocond_decl = NULL;
		tnode_t *eocond_lab = NULL;
		tnode_t *hls_eocond_label = hls->eocond_label;

		avrasm_newtemplabel (*tptr, &eocond_decl, &eocond_lab);
		hls->eocond_label = eocond_lab;

		/* simplify expression -- will drop code to branch if *not* true */
		avrasm_hllsimplify_subtree (exprp, hls);

		parser_addtolist (ilist, *exprp);
		*exprp = NULL;
		hls->eocond_label = hls_eocond_label;

		/* then this body */
		avrasm_hllsimplify_subtree (bodyp, hls);
		parser_addtolist (ilist, *bodyp);
		*bodyp = NULL;

		/* then end-of-condition */
		parser_addtolist (ilist, eocond_decl);
	}

	tnode_free (*tptr);
	*tptr = ilist;

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
	tnode_setcompop (cops, "llscope", 2, COMPOPTYPE (avrasm_llscope_fcndefnode));
	tnode_setcompop (cops, "hlltypecheck", 2, COMPOPTYPE (avrasm_hlltypecheck_fcndefnode));
	tnode_setcompop (cops, "hllsimplify", 2, COMPOPTYPE (avrasm_hllsimplify_fcndefnode));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "avrasm_inseg", 1, LANGOPTYPE (avrasm_inseg_true));
	tnd->lops = lops;

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
	/*{{{  avrasm:setnode -- EXPRSET*/
	i = -1;
	tnd = tnode_newnodetype ("avrasm:setnode", &i, 1, 0, 0, TNF_NONE);		/* subnodes: 0 = list-of-items */
	cops = tnode_newcompops ();
	tnd->ops = cops;

	i = -1;
	avrasm.tag_EXPRSET = tnode_newnodetag ("AVRASMEXPRSET", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  avrasm:letdefnode -- LETDEF*/
	i = -1;
	tnd = tnode_newnodetype ("avrasm:letdefnode", &i, 3, 0, 0, TNF_NONE);		/* subnodes: 0 = name, 1 = type, 2 = expr */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (avrasm_scopein_letdefnode));
	tnode_setcompop (cops, "hlltypecheck", 2, COMPOPTYPE (avrasm_hlltypecheck_letdefnode));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "avrasm_inseg", 1, LANGOPTYPE (avrasm_inseg_true));
	tnd->lops = lops;

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
	avrasm.node_HLLNAMENODE = tnd;
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "hllsimplify", 2, COMPOPTYPE (avrasm_hllsimplify_hllnamenode));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "gettype", 2, LANGOPTYPE (avrasm_gettype_hllnamenode));
	tnd->lops = lops;

	i = -1;
	avrasm.tag_FCNNAME = tnode_newnodetag ("AVRASMFCNNAME", &i, tnd, NTF_NONE);
	i = -1;
	avrasm.tag_FCNPARAMNAME = tnode_newnodetag ("AVRASMFCNPARAMNAME", &i, tnd, NTF_NONE);
	i = -1;
	avrasm.tag_LETNAME = tnode_newnodetag ("AVRASMLETNAME", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  avrasm:hllleafnode -- CC, INT8, UINT8, INT16, UINT16, SIGNED, UNSIGNED*/
	i = -1;
	tnd = tnode_newnodetype ("avrasm:hllleafnode", &i, 0, 0, 0, TNF_NONE);
	cops = tnode_newcompops ();
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "gettype", 2, LANGOPTYPE (avrasm_gettype_hllleafnode));
	tnd->lops = lops;

	i = -1;
	avrasm.tag_CC = tnode_newnodetag ("AVRASMCC", &i, tnd, NTF_NONE);
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
	/*{{{  avrasm:hllexpnode -- EXPADD, EXPSUB, EXPMUL, EXPDIV, EXPREM, EXPOR, EXPAND, EXPXOR, EXPBITOR, EXPBITAND, EXPBITXOR, EXPEQ, EXPNEQ, EXPLT, EXPGT, EXPLE, EXPGE*/
	i = -1;
	tnd = tnode_newnodetype ("avrasm:hllexpnode", &i, 3, 0, 0, TNF_NONE);		/* subnodes: 0 = left, 1 = right, 2 = type */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "hlltypecheck", 2, COMPOPTYPE (avrasm_hlltypecheck_hllexpnode));
	tnode_setcompop (cops, "hllsimplify", 2, COMPOPTYPE (avrasm_hllsimplify_hllexpnode));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "gettype", 2, LANGOPTYPE (avrasm_gettype_hllexpnode));
	tnd->lops = lops;

	i = -1;
	avrasm.tag_EXPADD = tnode_newnodetag ("AVRASMEXPADD", &i, tnd, NTF_NONE);
	i = -1;
	avrasm.tag_EXPSUB = tnode_newnodetag ("AVRASMEXPSUB", &i, tnd, NTF_NONE);

	i = -1;
	avrasm.tag_EXPEQ = tnode_newnodetag ("AVRASMEXPEQ", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  avrasm:hllifnode -- HLLIF*/
	i = -1;
	tnd = tnode_newnodetype ("avrasm:hllifnode", &i, 1, 0, 0, TNF_NONE);		/* subnodes: 0 = list of HLLCONDs */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "hlltypecheck", 2, COMPOPTYPE (avrasm_hlltypecheck_hllifnode));
	tnode_setcompop (cops, "hllsimplify", 2, COMPOPTYPE (avrasm_hllsimplify_hllifnode));
	tnd->ops = cops;

	i = -1;
	avrasm.tag_HLLIF = tnode_newnodetag ("AVRASMHLLIF", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  avrasm:hllcondnode -- HLLCOND*/
	i = -1;
	tnd = tnode_newnodetype ("avrasm:hllcondnode", &i, 2, 0, 0, TNF_NONE);		/* subnodes: 0 = expression (or null if "else"); 1 = code */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "hlltypecheck", 2, COMPOPTYPE (avrasm_hlltypecheck_hllcondnode));
	tnode_setcompop (cops, "hllsimplify", 2, COMPOPTYPE (avrasm_hllsimplify_hllcondnode));
	tnd->ops = cops;

	i = -1;
	avrasm.tag_HLLCOND = tnode_newnodetag ("AVRASMHLLCOND", &i, tnd, NTF_NONE);

	/*}}}*/

	/*{{{  interfere with avrasm:insnode in hllsimplify pass*/
	tnd = tnode_lookupnodetype ("avrasm:insnode");
	if (!tnd) {
		return -1;
	}
	if (!tnd->ops) {
		cops = tnode_newcompops ();
		tnd->ops = cops;
	}
	tnode_setcompop (tnd->ops, "hllsimplify", 2, COMPOPTYPE (avrasm_hllsimplify_insnode));

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

