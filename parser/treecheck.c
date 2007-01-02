/*
 *	treecheck.c -- tree checking routines for NOCC
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/*{{{  includes*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
#include "fcnlib.h"
#include "extn.h"
#include "dfa.h"
#include "dfaerror.h"
#include "langdef.h"
#include "parsepriv.h"
#include "lexpriv.h"
#include "names.h"
#include "target.h"


/*}}}*/


/*{{{  int treecheck_init (void)*/
/*
 *	initialises the tree-checking routines
 *	returns 0 on success, non-zero on failure
 */
int treecheck_init (void)
{
	return 0;
}
/*}}}*/
/*{{{  int treecheck_shutdown (void)*/
/*
 *	shuts-down the tree-checking routines
 *	returns 0 on success, non-zero on failure
 */
int treecheck_shutdown (void)
{
	return 0;
}
/*}}}*/


/*{{{  int treecheck_prepass (tnode_t *tree, const char *pname, const int penabled)*/
/*
 *	called to do pre-pass checks on a parse-tree
 *	returns 0 on success, non-zero on failure
 */
int treecheck_prepass (tnode_t *tree, const char *pname, const int penabled)
{
	/* FIXME! */
	/* nocc_message ("treecheck_prepass() on [%s] enabled = %d", pname, penabled); */
	return 0;
}
/*}}}*/
/*{{{  int treecheck_postpass (tnode_t *tree, const char *pname, const int penabled)*/
/*
 *	called to do post-pass checks on a parse-tree
 *	returns 0 on success, non-zero on failure
 */
int treecheck_postpass (tnode_t *tree, const char *pname, const int penabled)
{
	/* FIXME! */
	return 0;
}
/*}}}*/



