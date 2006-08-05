/*
 *	occampi_mwsync.c -- occam-pi multi-way synchronisations
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
#include "occampi.h"
#include "feunit.h"
#include "names.h"
#include "scope.h"
#include "prescope.h"
#include "precheck.h"
#include "typecheck.h"
#include "usagecheck.h"
#include "map.h"
#include "target.h"
#include "transputer.h"
#include "codegen.h"
#include "langops.h"


/*}}}*/


/*{{{  static tnode_t *occampi_mwsync_leaftype_gettype (langops_t *lops, tnode_t *t, tnode_t *defaulttype)*/
/*
 *	gets the type for a mwsync leaftype -- do nothing really
 */
static tnode_t *occampi_mwsync_leaftype_gettype (langops_t *lops, tnode_t *t, tnode_t *defaulttype)
{
	if (t->tag == opi.tag_BARRIER) {
		return t;
	}
	if (lops->next && lops->next->gettype) {
		return lops->next->gettype (lops->next, t, defaulttype);
	}
	nocc_error ("occampi_mwsync_leaftype_gettype(): no next function!");
	return defaulttype;
}
/*}}}*/
/*{{{  static int occampi_mwsync_leaftype_bytesfor (langops_t *lops, tnode_t *t, target_t *target)*/
/*
 *	returns the number of bytes required by a basic type
 */
static int occampi_mwsync_leaftype_bytesfor (langops_t *lops, tnode_t *t, target_t *target)
{
	if (t->tag == opi.tag_BARRIER) {
		return target->intsize * 5;
	}
	if (lops->next && lops->next->bytesfor) {
		return lops->next->bytesfor (lops->next, t, target);
	}
	nocc_error ("occampi_mwsync_leaftype_bytesfor(): no next function!");
	return -1;
}
/*}}}*/
/*{{{  static int occampi_mwsync_leaftype_issigned (langops_t *lops, tnode_t *t, target_t *target)*/
/*
 *	returns 0 if the given basic type is unsigned
 */
static int occampi_mwsync_leaftype_issigned (langops_t *lops, tnode_t *t, target_t *target)
{
	if (t->tag == opi.tag_BARRIER) {
		return 0;
	}
	if (lops->next && lops->next->issigned) {
		return lops->next->issigned (lops->next, t, target);
	}
	nocc_error ("occampi_mwsync_leaftype_issigned(): no next function!");
	return 0;
}
/*}}}*/
/*{{{  static int occampi_mwsync_leaftype_getdescriptor (langops_t *lops, tnode_t *node, char **str)*/
/*
 *	gets descriptor information for a leaf-type
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_mwsync_leaftype_getdescriptor (langops_t *lops, tnode_t *node, char **str)
{
	char *sptr;

	if (node->tag == opi.tag_BARRIER) {
		if (*str) {
			char *newstr = (char *)smalloc (strlen (*str) + 16);

			sptr = newstr;
			sptr += sprintf (newstr, "%s", *str);
			sfree (*str);
			*str = newstr;
		} else {
			*str = (char *)smalloc (16);
			sptr = *str;
		}
		sprintf (sptr, "BARRIER");
		return 0;
	}
	if (lops->next && lops->next->getdescriptor) {
		lops->next->getdescriptor (lops->next, node, str);
	}
	nocc_error ("occampi_mwsync_leaftype_getdescriptor(): no next function!");

	return 0;
}
/*}}}*/


/*{{{  static int occampi_mwsync_init_nodes (void)*/
/*
 *	sets up nodes for occam-pi multi-way synchronisations
 *	returns 0 on success, non-zero on error
 */
static int occampi_mwsync_init_nodes (void)
{
	tndef_t *tnd;
	compops_t *cops;
	langops_t *lops;
	int i;

	/*{{{  occampi:leaftype -- BARRIER*/
	tnd = tnode_lookupnodetype ("occampi:leaftype");
	if (!tnd) {
		nocc_error ("occampi_mwsync_init_nodes(): failed to find occampi:leaftype");
		return -1;
	}
	cops = tnode_insertcompops (tnd->ops);
	tnd->ops = cops;
	lops = tnode_insertlangops (tnd->lops);
	lops->getdescriptor = occampi_mwsync_leaftype_getdescriptor;
	lops->gettype = occampi_mwsync_leaftype_gettype;
	lops->bytesfor = occampi_mwsync_leaftype_bytesfor;
	lops->issigned = occampi_mwsync_leaftype_issigned;
	tnd->lops = lops;

	i = -1;
	opi.tag_BARRIER = tnode_newnodetag ("BARRIER", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  occampi:leafnode -- PARBARRIER, PROCBARRIER*/
	tnd = tnode_lookupnodetype ("occampi:leafnode");

	i = -1;
	opi.tag_PARBARRIER = tnode_newnodetag ("PARBARRIER", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_PROCBARRIER = tnode_newnodetag ("PROCBARRIER", &i, tnd, NTF_NONE);

	/*}}}*/

	return 0;
}
/*}}}*/
/*{{{  static int occampi_mwsync_reg_reducers (void)*/
/*
 *	registers reductions for occam-pi multi-way synchronisation reductions
 *	returns 0 on success, non-zero on error
 */
static int occampi_mwsync_reg_reducers (void)
{
	parser_register_grule ("opi:barrierreduce", parser_decode_grule ("ST0T+@tC0R-", opi.tag_BARRIER));

	return 0;
}
/*}}}*/
/*{{{  static dfattbl_t **occampi_mwsync_init_dfatrans (int *ntrans)*/
/*
 *	initialises and returns DFA transition tables for occam-pi multi-way synchronisations
 */
static dfattbl_t **occampi_mwsync_init_dfatrans (int *ntrans)
{
	DYNARRAY (dfattbl_t *, transtbl);

	dynarray_init (transtbl);

	dynarray_add (transtbl, dfa_transtotbl ("occampi:primtype +:= [ 0 +@BARRIER 1 ] [ 1 {<opi:barrierreduce>} -* ]"));
	/* FIXME! */
	/* dynarray_add (transtbl, dfa_transtotbl ("occampi:mobileprocdecl ::= [ 0 @MOBILE 1 ] [ 1 @PROC 2 ] [ 2 occampi:name 3 ] [ 3 {<opi:nullreduce>} -* ]")); */

#if 0
	dynarray_add (transtbl, dfa_transtotbl ("occampi:type +:= [ 0 -@MOBILE 1 ] [ 1 occampi:mobiletype 2 ] [ 2 {<opi:nullreduce>} -* ]"));
	dynarray_add (transtbl, dfa_transtotbl ("occampi:expr +:= [ 0 -@MOBILE 1 ] [ 1 occampi:mobileallocexpr 2 ] [ 2 {<opi:nullreduce>} -* ]"));
#endif

	*ntrans = DA_CUR (transtbl);
	return DA_PTR (transtbl);
}
/*}}}*/
/*{{{  static int occampi_mwsync_post_setup (void)*/
/*
 *	does post-setup for occam-pi multi-way synchronisation nodes
 *	returns 0 on success, non-zero on error
 */
static int occampi_mwsync_post_setup (void)
{
	return 0;
}
/*}}}*/



/*{{{  occampi_mwsync_feunit (feunit_t)*/
feunit_t occampi_mwsync_feunit = {
	init_nodes: occampi_mwsync_init_nodes,
	reg_reducers: occampi_mwsync_reg_reducers,
	init_dfatrans: occampi_mwsync_init_dfatrans,
	post_setup: occampi_mwsync_post_setup
};
/*}}}*/

