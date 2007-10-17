/*
 *	traceslang_expr.c -- traces language expression handling
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
#include "parsepriv.h"
#include "traceslang.h"
#include "traceslang_fe.h"
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


/*{{{  static int traceslang_expr_init_nodes (void)*/
/*
 *	initialises nodes for traces language
 *	returns 0 on success, non-zero on failure
 */
static int traceslang_expr_init_nodes (void)
{
	return 0;
}
/*}}}*/


/*{{{  traceslang_expr_feinit (feunit_t)*/
feunit_t traceslang_expr_feunit = {
	init_nodes: traceslang_expr_init_nodes,
	reg_reducers: NULL,
	init_dfatrans: NULL,
	post_setup: NULL,
	ident: "traceslang-expr"
};

/*}}}*/


