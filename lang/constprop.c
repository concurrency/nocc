/*
 *	constprop.c -- constant propagator for NOCC
 *	Copyright (C) 2005 Fred Barnes <frmb@kent.ac.uk>
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
#include "constprop.h"


/*}}}*/
/*{{{  private types*/
typedef struct TAG_consthook {
	union {
		unsigned char bval;
		int ival;
		double dval;
		unsigned long long ullval;
	} u;
	consttype_e type;
	tnode_t *orig;
} consthook_t;


/*}}}*/
/*{{{  private data*/
static ntdef_t *tag_CONST;		/* constant */


/*}}}*/


/*{{{  static void cprop_isetindent (FILE *stream, int indent)*/
/*
 *	prints indentation
 */
static void cprop_isetindent (FILE *stream, int indent)
{
	int i;

	for (i=0; i<indent; i++) {
		fprintf (stream, "    ");
	}
	return;
}
/*}}}*/


/*{{{  static consthook_t *cprop_newconsthook (void)*/
/*
 *	creates a new consthook_t structure (empty)
 */
static consthook_t *cprop_newconsthook (void)
{
	consthook_t *ch = (consthook_t *)smalloc (sizeof (consthook_t));

	ch->u.ival = 0;
	ch->u.ullval = 0ULL;
	ch->type = CONST_INVALID;
	ch->orig = NULL;

	return ch;
}
/*}}}*/
/*{{{  static void *cprop_consthook_hook_copy (void *hook)*/
/*
 *	copies a consthook_t structure
 */
static void *cprop_consthook_hook_copy (void *hook)
{
	consthook_t *ch = (consthook_t *)hook;
	consthook_t *newch;

	if (!ch) {
		return NULL;
	}
	newch = cprop_newconsthook ();
	newch->type = ch->type;
	memcpy (&(newch->u), &(ch->u), sizeof (newch->u));
	newch->orig = ch->orig ? tnode_copytree (ch->orig) : NULL;

	return (void *)newch;
}
/*}}}*/
/*{{{  static void cprop_consthook_hook_free (void *hook)*/
/*
 *	frees a consthook_t structure
 */
static void cprop_consthook_hook_free (void *hook)
{
	consthook_t *ch = (consthook_t *)hook;

	if (!ch) {
		return;
	}
	if (ch->orig) {
		tnode_free (ch->orig);
	}
	sfree (ch);

	return;
}
/*}}}*/
/*{{{  static void cprop_consthook_hook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	dumps a consthook_t structure (debugging)
 */
static void cprop_consthook_hook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	consthook_t *ch = (consthook_t *)hook;

	cprop_isetindent (stream, indent);
	fprintf (stream, "<consthook addr=\"0x%8.8x\" ", (unsigned int)hook);
	if (!ch) {
		fprintf (stream, "/>\n");
	} else {
		fprintf (stream, "type=\"");
		switch (ch->type) {
		case CONST_INVALID:
			fprintf (stream, "invalid\"");
			break;
		case CONST_BYTE:
			fprintf (stream, "byte\" value=\"0x%2.2x\"", ch->u.bval);
			break;
		case CONST_INT:
			fprintf (stream, "int\" value=\"0x%8.8x\"", (unsigned int)ch->u.ival);
			break;
		case CONST_DOUBLE:
			fprintf (stream, "double\" value=\"0x%8.8x\"", *(unsigned int *)(&(ch->u.dval)));
			break;
		case CONST_ULL:
			fprintf (stream, "ull\" value=\"0x%16.16Lx\"", ch->u.ullval);
			break;
		}
		if (ch->orig) {
			fprintf (stream, ">\n");
			tnode_dumptree (ch->orig, indent+1, stream);
			cprop_isetindent (stream, indent);
			fprintf (stream, "</consthook>\n");
		} else {
			fprintf (stream, " />\n");
		}
	}
	return;
}
/*}}}*/


/*{{{  static tnode_t *cprop_gettype_const (tnode_t *node, tnode_t *default_type)*/
/*
 *	gets the type of a constant node
 */
static tnode_t *cprop_gettype_const (tnode_t *node, tnode_t *default_type)
{
	if (!tnode_nthsubof (node, 0) && default_type) {
		/* FIXME: check what we already have fits the default type if given */
		tnode_setnthsub (node, 0, tnode_copytree (default_type));
	}

	return tnode_nthsubof (node, 0);
}
/*}}}*/


/*{{{  static int cprop_modprewalktree (tnode_t **tptr, void *arg)*/
/*
 *	does mod-pre-tree walk for constant propagation
 *	returns 0 to stop walk, 1 to continue
 */
static int cprop_modprewalktree (tnode_t **tptr, void *arg)
{
	return 1;
}
/*}}}*/
/*{{{  static int cprop_modpostwalktree (tnode_t **tptr, void *arg)*/
/*
 *	does mod-post-tree walk for constant propatation
 *	returns 0 to stop walk, 1 to continue (not really relevant)
 */
static int cprop_modpostwalktree (tnode_t **tptr, void *arg)
{
	int i = 0;

	if (tptr && *tptr && (*tptr)->tag->ndef->ops && (*tptr)->tag->ndef->ops->constprop) {
		i = (*tptr)->tag->ndef->ops->constprop (tptr);
	}
	return i;
}
/*}}}*/


/*{{{  int constprop_init (void)*/
/*
 *	initialises the constant propagator
 *	returns 0 on success, non-zero on failure
 */
int constprop_init (void)
{
	tndef_t *tnd;
	int i;
	compops_t *cops;

	i = -1;
	tnd = tnode_newnodetype ("nocc:const", &i, 1, 0, 1, TNF_NONE);		/* subnodes: 0=type */
	tnd->hook_copy = cprop_consthook_hook_copy;
	tnd->hook_free = cprop_consthook_hook_free;
	tnd->hook_dumptree = cprop_consthook_hook_dumptree;
	cops = tnode_newcompops ();
	cops->gettype = cprop_gettype_const;
	tnd->ops = cops;

	i = -1;
	tag_CONST = tnode_newnodetag ("CONST", &i, tnd, NTF_NONE);

	return 0;
}
/*}}}*/
/*{{{  int constprop_shutdown (void)*/
/*
 *	shuts-down the constant propagator
 *	returns 0 on success, non-zero on failure
 */
int constprop_shutdown (void)
{
	return 0;
}
/*}}}*/


/*{{{  tnode_t *constprop_newconst (consttype_e ctype, tnode_t *orig, tnode_t *type, ...)*/
/*
 *	creates a new constant-node
 */
tnode_t *constprop_newconst (consttype_e ctype, tnode_t *orig, tnode_t *type, ...)
{
	va_list ap;
	tnode_t *cnode;
	consthook_t *ch = cprop_newconsthook ();

	va_start (ap, type);
	ch->type = ctype;
	ch->orig = orig;
	switch (ctype) {
	case CONST_INVALID:
		break;
	case CONST_BYTE:
		ch->u.bval = (unsigned char)va_arg (ap, int);
		break;
	case CONST_INT:
		ch->u.ival = va_arg (ap, int);
		break;
	case CONST_DOUBLE:
		ch->u.dval = va_arg (ap, double);
		break;
	case CONST_ULL:
		ch->u.ullval = va_arg (ap, unsigned long long);
		break;
	}
	va_end (ap);

	cnode = tnode_create (tag_CONST, NULL, type, ch);

	return cnode;
}
/*}}}*/
/*{{{  int constprop_isconst (tnode_t *node)*/
/*
 *	returns non-zero if this node is a CONST
 */
int constprop_isconst (tnode_t *node)
{
	return (node && (node->tag == tag_CONST));
}
/*}}}*/
/*{{{  consttype_e constprop_consttype (tnode_t *tptr)*/
/*
 *	returns the constant type of the given node
 */
consttype_e constprop_consttype (tnode_t *tptr)
{
	consthook_t *ch;

	if (!tptr || (tptr->tag != tag_CONST)) {
		return CONST_INVALID;
	}
	ch = (consthook_t *)tnode_nthhookof (tptr, 0);

	return ch->type;
}
/*}}}*/
/*{{{  int constprop_sametype (tnode_t *tptr1, tnode_t *tptr2)*/
/*
 *	returns non-zero if the two constants are the same type
 */
int constprop_sametype (tnode_t *tptr1, tnode_t *tptr2)
{
	consthook_t *ch1, *ch2;

	if (!tptr1 || !tptr2 || (tptr1->tag != tag_CONST) || (tptr2->tag != tag_CONST)) {
		return 0;
	}
	ch1 = (consthook_t *)tnode_nthhookof (tptr1, 0);
	ch2 = (consthook_t *)tnode_nthhookof (tptr2, 0);

	if (ch1->type == CONST_INVALID) {
		return 0;
	}
	return (ch1->type == ch2->type);
}
/*}}}*/
/*{{{  int constprop_intvalof (tnode_t *tptr)*/
/*
 *	returns the integer value of a constant node
 */
int constprop_intvalof (tnode_t *tptr)
{
	consthook_t *ch;

	if (!tptr || (tptr->tag != tag_CONST)) {
		return 0;
	}
	ch = (consthook_t *)tnode_nthhookof (tptr, 0);

	switch (ch->type) {
	case CONST_INVALID:
		return 0;
	case CONST_BYTE:
		return (int)ch->u.bval;
	case CONST_INT:
		return ch->u.ival;
	case CONST_DOUBLE:
		return 0;
	case CONST_ULL:
		return (int)ch->u.ullval;
	}
	return -1;
}
/*}}}*/
/*{{{  int constprop_tree (tnode_t **tptr)*/
/*
 *	does constant propagation on the parse-tree
 *	returns 0 on success, non-zero on failure
 */
int constprop_tree (tnode_t **tptr)
{
	tnode_modprepostwalktree (tptr, cprop_modprewalktree, cprop_modpostwalktree, NULL);
	return 0;
}
/*}}}*/



