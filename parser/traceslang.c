/*
 *	traceslang.c -- traces language for NOCC
 *	Copyright (C) 2007-2016 Fred Barnes <frmb@kent.ac.uk>
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


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/*{{{  includes*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/types.h>
#include <errno.h>

#include "nocc.h"
#include "support.h"
#include "version.h"
#include "symbols.h"
#include "keywords.h"
#include "lexer.h"
#include "tnode.h"
#include "treecheck.h"
#include "parser.h"
#include "typecheck.h"
#include "fcnlib.h"
#include "extn.h"
#include "dfa.h"
#include "dfaerror.h"
#include "langdef.h"
#include "parsepriv.h"
#include "lexpriv.h"
#include "names.h"
#include "target.h"
#include "opts.h"
#include "traceslang.h"


/*}}}*/


/*{{{  int traceslang_init (void)*/
/*
 *	initialises the traces language parts of the compiler
 *	returns 0 on success, non-zero on failure
 */
int traceslang_init (void)
{
	return 0;
}
/*}}}*/
/*{{{  int traceslang_shutdown (void)*/
/*
 *	shuts-down traces language bits
 *	returns 0 on success, non-zero on failure
 */
int traceslang_shutdown (void)
{
	return 0;
}
/*}}}*/


/*{{{  int traceslang_initialise (void)*/
/*
 *	initialises traces handling for actual use (triggers traceslang parser initialisation)
 *	returns 0 on success, non-zero on failure
 */
int traceslang_initialise (void)
{
	lexfile_t *lf;
	tnode_t *tree;

	lf = lexer_openbuf ("traceslang_initialise.ttl", "traceslang", "\n");
	if (!lf) {
		nocc_error ("traceslang: failed to open buffer");
		return -1;
	}

	tree = parser_parse (lf);
	if (tree) {
#if 0
fprintf (stderr, "traceslang_initialise(): got tree:\n");
tnode_dumptree (tree, 1, stderr);
#endif
		tnode_free (tree);
	}

	lexer_close (lf);
	return 0;
}
/*}}}*/


