/*
 *	aliascheck.c -- alias checker for NOCC
 *	Copyright (C) 2005-2016 Fred Barnes <frmb@kent.ac.uk>
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
#include "parsepriv.h"
#include "dfa.h"
#include "names.h"
#include "aliascheck.h"


/*}}}*/
/*{{{  private things*/
static chook_t *aliascheckhook = NULL;

/*}}}*/


/*{{{  int aliascheck_init (void)*/
/*
 *	initialises alias checker
 *	returns 0 on success, non-zero on error
 */
int aliascheck_init (void)
{
	return 0;
}
/*}}}*/
/*{{{  int aliascheck_shutdown (void)*/
/*
 *	shuts-down alias checker
 *	returns 0 on success, non-zero on error
 */
int aliascheck_shutdown (void)
{
	return 0;
}
/*}}}*/


/*{{{  int aliascheck_tree (tnode_t *tree, langparser_t *lang)*/
/*
 *	does alias checking on the parse tree
 *	returns 0 on success, non-zero on error
 */
int aliascheck_tree (tnode_t *tree, langparser_t *lang)
{
	if (!aliascheckhook) {
		aliascheckhook = tnode_newchook ("aliascheck");
	}

	return 0;
}
/*}}}*/

