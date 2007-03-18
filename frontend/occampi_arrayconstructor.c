/*
 *	occampi_arrayconstructor.c -- array constructors (including constant and variable constructors)
 *	Copyright (C) 2007 Fred Barnes <frmb@kent.ac.uk>
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
#include "dfaerror.h"
#include "parsepriv.h"
#include "occampi.h"
#include "feunit.h"
#include "names.h"
#include "scope.h"
#include "prescope.h"
#include "typecheck.h"
#include "constprop.h"
#include "precheck.h"
#include "usagecheck.h"
#include "map.h"
#include "target.h"
#include "transputer.h"
#include "codegen.h"
#include "langops.h"


/*}}}*/



/*{{{  static int occampi_ac_init_nodes (void)*/
/*
 *	sets up array-constructor nodes for occampi
 *	returns 0 on success, non-zero on error
 */
static int occampi_ac_init_nodes (void)
{
	tndef_t *tnd;
	compops_t *cops;
	langops_t *lops;
	int i;

	/*{{{  occampi:ac -- CONSTCONSTRUCTOR*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:ac", &i, 2, 0, 0, TNF_NONE);				/* subnodes: 0 = items, 1 = type */
	cops = tnode_newcompops ();
//	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (occampi_namemap_arraynode));
//	tnode_setcompop (cops, "precode", 2, COMPOPTYPE (occampi_precode_arraynode));
	tnd->ops = cops;
	lops = tnode_newlangops ();
//	tnode_setlangop (lops, "getdescriptor", 2, LANGOPTYPE (occampi_getdescriptor_arraynode));
//	tnode_setlangop (lops, "typeactual", 4, LANGOPTYPE (occampi_typeactual_arraynode));
//	tnode_setlangop (lops, "bytesfor", 2, LANGOPTYPE (occampi_bytesfor_arraynode));
	tnd->lops = lops;

	i = -1;
	opi.tag_CONSTCONSTRUCTOR = tnode_newnodetag ("CONSTCONSTRUCTOR", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_ARRAYCONSTRUCTOR = tnode_newnodetag ("ARRAYCONSTRUCTOR", &i, tnd, NTF_NONE);

	/*}}}*/

	return 0;
}
/*}}}*/


/*{{{  occampi_arrayconstructor_feunit (feunit_t)*/
feunit_t occampi_arrayconstructor_feunit = {
	init_nodes: occampi_ac_init_nodes,
	reg_reducers: NULL,
	init_dfatrans: NULL,
	post_setup: NULL,
	ident: "occampi-arrayconstructor"
};

/*}}}*/


