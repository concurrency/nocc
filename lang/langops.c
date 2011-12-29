/*
 *	langops.c -- langage-level operations for nocc
 *	Copyright (C) 2005-2008 Fred Barnes <frmb@kent.ac.uk>
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
#include "parsepriv.h"
#include "names.h"
#include "tracescheck.h"
#include "langops.h"

/*}}}*/


/*{{{  static int langops_getdescriptor_walk (tnode_t *node, void *ptr)*/
/*
 *	tree-walk routine for getting descriptors
 *	returns 0 to stop walk, 1 to continue
 */
static int langops_getdescriptor_walk (tnode_t *node, void *ptr)
{
	int r;

	if (!node) {
		return 0;
	}
	if (node->tag->ndef->lops && tnode_haslangop_i (node->tag->ndef->lops, (int)LOPS_GETDESCRIPTOR)) {
		r = tnode_calllangop_i (node->tag->ndef->lops, (int)LOPS_GETDESCRIPTOR, 2, node, (char **)ptr);
	} else {
		r = 1;
	}
	return r;
}
/*}}}*/
/*{{{  void langops_getdescriptor (tnode_t *node, char **str)*/
/*
 *	walks a section of the tree to get descriptor-line information
 */
void langops_getdescriptor (tnode_t *node, char **str)
{
	tnode_prewalktree (node, langops_getdescriptor_walk, (void *)str);
	return;
}
/*}}}*/
/*{{{  static int langops_getname_walk (tnode_t *node, void *ptr)*/
/*
 *	tree-walk routine for getting names
 *	returns 0 to stop walk, 1 to continue
 */
static int langops_getname_walk (tnode_t *node, void *ptr)
{
	int r = 1;

	if (!node) {
		return 0;
	}
	if (node->tag->ndef->lops && tnode_haslangop_i (node->tag->ndef->lops, (int)LOPS_GETNAME)) {
		r = tnode_calllangop_i (node->tag->ndef->lops, (int)LOPS_GETNAME, 2, node, (char **)ptr);
		if (r < 0) {
			r = 1;		/* try again down the tree */
		}
	}

	return r;
}
/*}}}*/
/*{{{  void langops_getname (tnode_t *node, char **str)*/
/*
 *	walks a section of the tree to extract a name
 */
void langops_getname (tnode_t *node, char **str)
{
	tnode_prewalktree (node, langops_getname_walk, (void *)str);
	return;
}
/*}}}*/
/*{{{  int langops_isconst (tnode_t *node)*/
/*
 *	determines whether or not the given node is a known constant
 */
int langops_isconst (tnode_t *node)
{
	int r = 0;

	while (node && node->tag->ndef->lops && node->tag->ndef->lops->passthrough) {
		/* skip through this node */
		node = tnode_nthsubof (node, 0);
	}

	if (node && node->tag->ndef->lops && tnode_haslangop_i (node->tag->ndef->lops, (int)LOPS_ISCONST)) {
		r = tnode_calllangop_i (node->tag->ndef->lops, (int)LOPS_ISCONST, 1, node);
	}
	if (compopts.traceconstprop) {
		nocc_message ("langops: isconst? (%s,%s) = %d", node->tag->ndef->name, node->tag->name, r);
	}
	return r;
}
/*}}}*/
/*{{{  int langops_constvalof (tnode_t *node, void *ptr)*/
/*
 *	extracts the constant value of a given node.  if "ptr" is non-null,
 *	value is assigned there (to the appropriate width).  Anything integer
 *	or less is returned as well.
 */
int langops_constvalof (tnode_t *node, void *ptr)
{
	int r = 0;

	while (node && node->tag->ndef->lops && node->tag->ndef->lops->passthrough) {
		/* skip through this node */
		node = tnode_nthsubof (node, 0);
	}

	if (node && node->tag->ndef->lops && tnode_haslangop_i (node->tag->ndef->lops, (int)LOPS_CONSTVALOF)) {
		r = tnode_calllangop_i (node->tag->ndef->lops, (int)LOPS_CONSTVALOF, 2, node, ptr);
	} else {
		tnode_warning (node, "extracting non-existant constant value!");
	}

	return r;
}
/*}}}*/
/*{{{  int langops_constsizeof (tnode_t *node)*/
/*
 *	determines the number of bytes required for a known constant
 */
int langops_constsizeof (tnode_t *node)
{
	int r = 0;

	while (node && node->tag->ndef->lops && node->tag->ndef->lops->passthrough) {
		/* skip through this node */
		node = tnode_nthsubof (node, 0);
	}

	if (node && node->tag->ndef->lops && tnode_haslangop_i (node->tag->ndef->lops, (int)LOPS_CONSTSIZEOF)) {
		r = tnode_calllangop_i (node->tag->ndef->lops, (int)LOPS_CONSTSIZEOF, 1, node);
	}
	if (compopts.traceconstprop) {
		nocc_message ("langops: constsizeof? (%s,%s) = %d", node->tag->ndef->name, node->tag->name, r);
	}
	return r;
}
/*}}}*/
/*{{{  int langops_valbyref (tnode_t *node)*/
/*
 *	returns non-zero if VALs of this node (type/constant) are treated as references
 */
int langops_valbyref (tnode_t *node)
{
	int r = 0;

	while (node && node->tag->ndef->lops && node->tag->ndef->lops->passthrough) {
		/* skip through this node */
		node = tnode_nthsubof (node, 0);
	}

	if (node && node->tag->ndef->lops && tnode_haslangop_i (node->tag->ndef->lops, (int)LOPS_VALBYREF)) {
		r = tnode_calllangop_i (node->tag->ndef->lops, (int)LOPS_VALBYREF, 1, node);
	}

	return r;
}
/*}}}*/
/*{{{  static int langops_iscomplex_prewalk (tnode_t *t, void *arg)*/
/*
 *	helper for langops_iscomplex()
 *	returns 0 to stop walk, 1 to continue
 */
static int langops_iscomplex_prewalk (tnode_t *t, void *arg)
{
	int *r = (int *)arg;

	if (t && t->tag->ndef->lops && tnode_haslangop_i (t->tag->ndef->lops, (int)LOPS_ISCOMPLEX)) {
		*r = tnode_calllangop_i (t->tag->ndef->lops, (int)LOPS_ISCOMPLEX, 2, t, 1);
		return 0;
	}
	return 1;
}
/*}}}*/
/*{{{  int langops_iscomplex (tnode_t *node, int deep)*/
/*
 *	returns non-zero if a node is complex (used largely by fetrans for
 *	simplifying expressions).
 */
int langops_iscomplex (tnode_t *node, int deep)
{
	int r = -1;

	while (node && node->tag->ndef->lops && node->tag->ndef->lops->passthrough) {
		/* skip through this node */
		node = tnode_nthsubof (node, 0);
	}

	if (node && node->tag->ndef->lops && tnode_haslangop_i (node->tag->ndef->lops, (int)LOPS_ISCOMPLEX)) {
		r = tnode_calllangop_i (node->tag->ndef->lops, (int)LOPS_ISCOMPLEX, 2, node, deep);
	} else if (node && deep) {
		r = 0;
		tnode_prewalktree (node, langops_iscomplex_prewalk, &r);
	}
	return r;
}
/*}}}*/
/*{{{  int langops_isvar (tnode_t *node)*/
/*
 *	returns non-zero if a node is a variable (l-value), used during usage checks
 */
int langops_isvar (tnode_t *node)
{
	int r = 0;

	while (node && node->tag->ndef->lops && node->tag->ndef->lops->passthrough) {
		/* skip through this node */
		node = tnode_nthsubof (node, 0);
	}

	if (parser_islistnode (node)) {
		/* special case: list is l-value if all items are l-values */
		int i, nitems;
		tnode_t **items = parser_getlistitems (node, &nitems);
		int nvar = 0;

		for (i=0; i<nitems; i++) {
			if (langops_isvar (items[i])) {
				nvar++;
			}
		}

		r = (nvar == nitems);
	} else if (node && node->tag->ndef->lops && tnode_haslangop_i (node->tag->ndef->lops, (int)LOPS_ISVAR)) {
		r = tnode_calllangop_i (node->tag->ndef->lops, (int)LOPS_ISVAR, 1, node);
	}
	return r;
}
/*}}}*/
/*{{{  tnode_t *langops_retypeconst (tnode_t *node, tnode_t *type)*/
/*
 *	re-types a constant (during constant-propagation)
 *	returns new constant node on success, NULL on failure
 */
tnode_t *langops_retypeconst (tnode_t *node, tnode_t *type)
{
	tnode_t *nc = NULL;

	/* does the operation on the type, rather than the operand */
	if (type && type->tag->ndef->lops && tnode_haslangop_i (type->tag->ndef->lops, (int)LOPS_RETYPECONST)) {
		nc = (tnode_t *)tnode_calllangop_i (type->tag->ndef->lops, (int)LOPS_RETYPECONST, 2, node, type);
	}
	return nc;
}
/*}}}*/
/*{{{  tnode_t *langops_dimtreeof (tnode_t *node)*/
/*
 *	returns a dimension-tree for array types (as a list of dimensions, NULL indicates unknown dimension)
 *	returns NULL if not relevant (scalar types)
 */
tnode_t *langops_dimtreeof (tnode_t *node)
{
	tnode_t *dt = NULL;

	while (node && node->tag->ndef->lops && node->tag->ndef->lops->passthrough) {
		/* skip through this node */
		node = tnode_nthsubof (node, 0);
	}

	if (node && node->tag->ndef->lops && tnode_haslangop_i (node->tag->ndef->lops, (int)LOPS_DIMTREEOF)) {
		dt = (tnode_t *)tnode_calllangop_i (node->tag->ndef->lops, (int)LOPS_DIMTREEOF, 1, node);
	}
	return dt;
}
/*}}}*/
/*{{{  tnode_t *langops_dimtreeof_node (tnode_t *node, tnode_t *varnode)*/
/*
 *	returns a dimension-tree fo array types (as a list of dimensions, NULL indicates unknown dimension)
 *	the source node is also provided for cases where the size is part of the name itself (e.g. mobile arrays)
 *	returns NULL if not relevant (scalar types)
 */
tnode_t *langops_dimtreeof_node (tnode_t *node, tnode_t *varnode)
{
	tnode_t *dt = NULL;

	while (node && node->tag->ndef->lops && node->tag->ndef->lops->passthrough) {
		/* skip through this node */
		node = tnode_nthsubof (node, 0);
	}

	if (node && node->tag->ndef->lops && tnode_haslangop_i (node->tag->ndef->lops, (int)LOPS_DIMTREEOF_NODE)) {
		dt = (tnode_t *)tnode_calllangop_i (node->tag->ndef->lops, (int)LOPS_DIMTREEOF_NODE, 2, node, varnode);
	} else if (node && node->tag->ndef->lops && tnode_haslangop_i (node->tag->ndef->lops, (int)LOPS_DIMTREEOF)) {
		dt = (tnode_t *)tnode_calllangop_i (node->tag->ndef->lops, (int)LOPS_DIMTREEOF, 1, node);
	}

	return dt;
}
/*}}}*/
/*{{{  tnode_t *langops_hiddenparamsof (tnode_t *node)*/
/*
 *	returns the hidden parameters associated with a formal parameter (as a list)
 *	returns NULL if not relevant
 */
tnode_t *langops_hiddenparamsof (tnode_t *node)
{
	tnode_t *hp = NULL;

	if (node && node->tag->ndef->lops && tnode_haslangop_i (node->tag->ndef->lops, (int)LOPS_HIDDENPARAMSOF)) {
		hp = (tnode_t *)tnode_calllangop_i (node->tag->ndef->lops, (int)LOPS_HIDDENPARAMSOF, 1, node);
	}
	return hp;
}
/*}}}*/
/*{{{  int langops_hiddenslotsof (tnode_t *node)*/
/*
 *	returns the number of hidden slots required for abbreviations or declarations (called on the type)
 *	returns 0 if not relevant
 */
int langops_hiddenslotsof (tnode_t *node)
{
	int n = 0;

	if (node && node->tag->ndef->lops && tnode_haslangop_i (node->tag->ndef->lops, (int)LOPS_HIDDENSLOTSOF)) {
		n = tnode_calllangop_i (node->tag->ndef->lops, (int)LOPS_HIDDENSLOTSOF, 1, node);
	}
	return n;
}
/*}}}*/
/*{{{  int langops_typehash (tnode_t *node, const int hsize, void *ptr)*/
/*
 *	generates a type-hash for the specified node (width in bytes and a pointer to suitable buffer are given)
 *	returns 0 on success, non-zero on failure
 */
int langops_typehash (tnode_t *node, const int hsize, void *ptr)
{
	if (node && node->tag->ndef->lops && tnode_haslangop_i (node->tag->ndef->lops, (int)LOPS_TYPEHASH)) {
		return tnode_calllangop_i (node->tag->ndef->lops, (int)LOPS_TYPEHASH, 3, node, hsize, ptr);
	}
	return 1;
}
/*}}}*/
/*{{{  int langops_typehash_blend (const int dsize, void *dptr, const int ssize, void *sptr)*/
/*
 *	blends together two type-hashes (widths in bytes and pointers to suitable buffers are given)
 *	returns 0 on success, non-zero on failure
 */
int langops_typehash_blend (const int dsize, void *dptr, const int ssize, void *sptr)
{
	unsigned char *dbytes = (unsigned char *)dptr;
	unsigned char *sbytes = (unsigned char *)sptr;
	int i;
	unsigned char tmpch;

	/* shuffle destination round */
	tmpch = dbytes[0];
	for (i=1; i<dsize; i++) {
		dbytes[i-1] = dbytes[i];
	}
	dbytes[dsize-1] = tmpch;

	/* exclusive or add source */
	for (i=0; i<ssize; i++) {
		dbytes[i % dsize] ^= sbytes[i];
	}

	return 0;
}
/*}}}*/
/*{{{  tnode_t *langops_getbasename (tnode_t *node)*/
/*
 *	gets the base-name for a specified node (walks through SUBSCRIPTs and the like)
 *	returns base node on success, NULL on failure
 */
tnode_t *langops_getbasename (tnode_t *node)
{
	while (node && node->tag->ndef->lops && node->tag->ndef->lops->passthrough) {
		/* skip through this node */
		node = tnode_nthsubof (node, 0);
	}

	if (!node || !node->tag->ndef->lops || !tnode_haslangop_i (node->tag->ndef->lops, (int)LOPS_GETBASENAME)) {
		return NULL;
	}
	while (node && node->tag->ndef->lops && tnode_haslangop_i (node->tag->ndef->lops, (int)LOPS_GETBASENAME)) {
		node = (tnode_t *)tnode_calllangop_i (node->tag->ndef->lops, (int)LOPS_GETBASENAME, 1, node);
	}
	return node;
}
/*}}}*/
/*{{{  tnode_t *langops_getfieldname (tnode_t *node)*/
/*
 *	gets the field-name for a specified node (on a SUBSCRIPT)
 *	returns field name on success, NULL on failure
 */
tnode_t *langops_getfieldname (tnode_t *node)
{
	while (node && node->tag->ndef->lops && node->tag->ndef->lops->passthrough) {
		/* skip through this node */
		node = tnode_nthsubof (node, 0);
	}

	if (node && node->tag->ndef->lops && tnode_haslangop_i (node->tag->ndef->lops, (int)LOPS_GETFIELDNAME)) {
		return (tnode_t *)tnode_calllangop_i (node->tag->ndef->lops, (int)LOPS_GETFIELDNAME, 1, node);
	}
	return NULL;
}
/*}}}*/
/*{{{  tnode_t *langops_getfieldnamelist (tnode_t *node)*/
/*
 *	gets a field-name list for a specified node (walks SUBSCRIPTS collecting up the indices)
 *	returns list on success, NULL on failure
 */
tnode_t *langops_getfieldnamelist (tnode_t *node)
{
	tnode_t *xlist = parser_newlistnode (NULL);

	while (node && node->tag->ndef->lops && node->tag->ndef->lops->passthrough) {
		/* skip through this node */
		node = tnode_nthsubof (node, 0);
	}

	while (node && node->tag->ndef->lops && tnode_haslangop_i (node->tag->ndef->lops, (int)LOPS_GETFIELDNAME)) {
		tnode_t *field = (tnode_t *)tnode_calllangop_i (node->tag->ndef->lops, (int)LOPS_GETFIELDNAME, 1, node);

		if (field) {
#if 0
fprintf (stderr, "langops_getfieldnamelist(): got field at 0x%8.8x\n", (unsigned int)field);
#endif
			parser_addtolist (xlist, field);
		}
		node = langops_getbasename (node);
	}

	if (!parser_countlist (xlist)) {
		parser_trashlist (xlist);
		xlist = NULL;
	}
	return xlist;
}
/*}}}*/
/*{{{  int langops_iscommunicable (tnode_t *node)*/
/*
 *	determines whether or not something can be communicated down a channel (determined ultimately by type)
 *	returns 1 if so, 0 if not
 */
int langops_iscommunicable (tnode_t *node)
{
	while (node && node->tag->ndef->lops && node->tag->ndef->lops->passthrough) {
		/* skip through this node */
		node = tnode_nthsubof (node, 0);
	}

	if (node && node->tag->ndef->lops && tnode_haslangop_i (node->tag->ndef->lops, (int)LOPS_ISCOMMUNICABLE)) {
		return tnode_calllangop_i (node->tag->ndef->lops, (int)LOPS_ISCOMMUNICABLE, 1, node);
	}
	return 0;
}
/*}}}*/
/*{{{  tnode_t *langops_gettags (tnode_t *node)*/
/*
 *	returns a list of tags associated with a particular node (determined ultimately by type)
 *	returns list of tags or NULL
 */
tnode_t *langops_gettags (tnode_t *node)
{
	while (node && node->tag->ndef->lops && node->tag->ndef->lops->passthrough) {
		/* skip through this node */
		node = tnode_nthsubof (node, 0);
	}

	if (node && node->tag->ndef->lops && tnode_haslangop_i (node->tag->ndef->lops, (int)LOPS_GETTAGS)) {
		return (tnode_t *)tnode_calllangop_i (node->tag->ndef->lops, (int)LOPS_GETTAGS, 1, node);
	}
	return NULL;
}
/*}}}*/
/*{{{  name_t *langops_nameof (tnode_t *node)*/
/*
 *	returns the name_t associated with a particular node (usually the name of something)
 *	returns name on success, NULL on failure
 */
name_t *langops_nameof (tnode_t *node)
{
	while (node && node->tag->ndef->lops && node->tag->ndef->lops->passthrough) {
		/* skip through this node */
		node = tnode_nthsubof (node, 0);
	}

	if (node && node->tag->ndef->lops && tnode_haslangop_i (node->tag->ndef->lops, (int)LOPS_NAMEOF)) {
		return (name_t *)tnode_calllangop_i (node->tag->ndef->lops, (int)LOPS_NAMEOF, 1, node);
	}
	return NULL;
}
/*}}}*/
/*{{{  tnode_t *langops_tracespecof (tnode_t *node)*/
/*
 *	returns the traces specification of a particular node (usually a type or name)
 *	returns traces-specification on success, NULL if none
 */
tnode_t *langops_tracespecof (tnode_t *node)
{
	while (node && node->tag->ndef->lops && node->tag->ndef->lops->passthrough) {
		/* skip through this node */
		node = tnode_nthsubof (node, 0);
	}

	if (node && node->tag->ndef->lops && tnode_haslangop_i (node->tag->ndef->lops, (int)LOPS_TRACESPECOF)) {
		return (tnode_t *)tnode_calllangop_i (node->tag->ndef->lops, (int)LOPS_TRACESPECOF, 1, node);
	}
	return NULL;
}
/*}}}*/
/*{{{  static int langops_getctypeof_walk (tnode_t *node, void *ptr)*/
/*
 *	tree-walk routine for getting C type strings (used by C-CCSP back-end)
 * 	return 0 to stop walk, 1 to continue.
 */
static int langops_getctypeof_walk (tnode_t *node, void *ptr)
{
	int r = 1;

	if (!node) {
		return 0;
	}
	if (node->tag->ndef->lops && tnode_haslangop_i (node->tag->ndef->lops, (int)LOPS_GETCTYPEOF)) {
		r = tnode_calllangop_i (node->tag->ndef->lops, (int)LOPS_GETCTYPEOF, 2, node, (char **)ptr);
		if (r < 0) {
			r = 1;		/* try again down the tree */
		}
	}

	return r;
}
/*}}}*/
/*{{{  void langops_getctypeof (tnode_t *node, char **str)*/
/*
 *	walks a section of a type-tree to extract the C type name.
 */
void langops_getctypeof (tnode_t *node, char **str)
{
	tnode_prewalktree (node, langops_getctypeof_walk, (void *)str);
}
/*}}}*/


/*{{{  int langops_init (void)*/
/*
 *	initialises tree-operations
 *	returns 0 on success, non-zero on failure
 */
int langops_init (void)
{
	return 0;
}
/*}}}*/
/*{{{  int langops_shutdown (void)*/
/*
 *	shuts-down tree-operations
 *	returns 0 on success, non-zero on failure
 */
int langops_shutdown (void)
{
	return 0;
}
/*}}}*/


