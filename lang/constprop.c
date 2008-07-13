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
#include "typecheck.h"
#include "parsepriv.h"
#include "names.h"
#include "constprop.h"
#include "map.h"
#include "target.h"


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
		case CONST_BOOL:
			fprintf (stream, "bool\" value=\"0x%x\"", ch->u.ival);
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


/*{{{  static tnode_t *cprop_gettype_const (langops_t *lops, tnode_t *node, tnode_t *default_type)*/
/*
 *	gets the type of a constant node
 */
static tnode_t *cprop_gettype_const (langops_t *lops, tnode_t *node, tnode_t *default_type)
{
	if (!tnode_nthsubof (node, 0) && default_type) {
		/* FIXME: check what we already have fits the default type if given */
		tnode_setnthsub (node, 0, tnode_copytree (default_type));
	}

	return tnode_nthsubof (node, 0);
}
/*}}}*/
/*{{{  static int cprop_namemap_const (compops_t *cops, tnode_t **nodep, map_t *map)*/
/*
 *	maps a constant node, generating back-end constant
 *	returns 0 to stop walk, 1 to continue
 */
static int cprop_namemap_const (compops_t *cops, tnode_t **nodep, map_t *map)
{
	consthook_t *ch = (consthook_t *)tnode_nthhookof ((*nodep), 0);
	void *dptr = NULL;
	int dlen = 0;

	switch (ch->type) {
	case CONST_INVALID:
		dptr = NULL;
		dlen = 0;
		break;
	case CONST_BYTE:
		dptr = &(ch->u.bval);
		dlen = 1;
		break;
	case CONST_BOOL:
	case CONST_INT:
		dptr = &(ch->u.ival);
		dlen = map->target->intsize;
		break;
	case CONST_DOUBLE:
		dptr = &(ch->u.dval);
		dlen = sizeof (ch->u.dval);
		break;
	case CONST_ULL:
		dptr = &(ch->u.ullval);
		dlen = sizeof (ch->u.ullval);
		break;
	}

	if (dptr) {
		tnode_t *cnst;

		cnst = map->target->newconst (*nodep, map, dptr, dlen);
		*nodep = cnst;
	}
	return 0;
}
/*}}}*/
/*{{{  static int cprop_isconst_const (langops_t *lops, tnode_t *node)*/
/*
 *	returns the width of a constant (in bytes)
 */
static int cprop_isconst_const (langops_t *lops, tnode_t *node)
{
	consthook_t *ch = (consthook_t *)tnode_nthhookof (node, 0);

	switch (ch->type) {
	case CONST_INVALID:
		return 0;
	case CONST_BYTE:
		return 1;
	case CONST_BOOL:
	case CONST_INT:
		return 4;
	case CONST_DOUBLE:
	case CONST_ULL:
		return 8;
	}
	return 0;
}
/*}}}*/
/*{{{  static int cprop_iscomplex_const (langops_t *lops, tnode_t *node, int deep)*/
/*
 *	returns non-zero if the constant is complex (they're not)
 */
static int cprop_iscomplex_const (langops_t *lops, tnode_t *node, int deep)
{
	return 0;
}
/*}}}*/
/*{{{  static int cprop_constvalof_const (langops_t *lops, tnode_t *node, void *ptr)*/
/*
 *	returns the constant value of a constant node
 */
static int cprop_constvalof_const (langops_t *lops, tnode_t *node, void *ptr)
{
	consthook_t *ch = (consthook_t *)tnode_nthhookof (node, 0);

	switch (ch->type) {
	case CONST_INVALID:
		return 0;
	case CONST_BYTE:
		if (ptr) {
			*(unsigned char *)ptr = ch->u.bval;
		}
		return (int)ch->u.bval;
	case CONST_INT:
	case CONST_BOOL:
		if (ptr) {
			*(int *)ptr = ch->u.ival;
		}
		return ch->u.ival;
	case CONST_DOUBLE:
		if (ptr) {
			*(double *)ptr = ch->u.dval;
		}
		return 0;
	case CONST_ULL:
		if (ptr) {
			*(unsigned long long *)ptr = ch->u.ullval;
		}
		return (int)ch->u.ullval;
	}
	return 0;
}
/*}}}*/
/*{{{  static int cprop_getdescriptor_const (langops_t *lops, tnode_t *node, char **str)*/
/*
 *	gets a descriptor string for a constant
 *	returns 0 to stop walk, 1 to continue
 */
static int cprop_getdescriptor_const (langops_t *lops, tnode_t *node, char **str)
{
	if (*str) {
		char *newstr = (char *)smalloc (strlen (*str) + 12);

		sprintf (newstr, "%s%d", *str, constprop_intvalof (node));
		sfree (*str);
		*str = newstr;
	} else {
		*str = (char *)smalloc (12);

		sprintf (*str, "%d", constprop_intvalof (node));
	}
	return 0;
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

	if (tptr && *tptr && (*tptr)->tag->ndef->ops && tnode_hascompop_i ((*tptr)->tag->ndef->ops, (int)COPS_CONSTPROP)) {
		if (compopts.traceconstprop) {
			nocc_message ("constprop: checking (%s,%s)", (*tptr)->tag->ndef->name, (*tptr)->tag->name);
		}
		i = tnode_callcompop_i ((*tptr)->tag->ndef->ops, (int)COPS_CONSTPROP, 1, tptr);
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
	langops_t *lops;

	/*{{{  nocc:const -- CONST*/
	i = -1;
	tnd = tnode_newnodetype ("nocc:const", &i, 1, 0, 1, TNF_NONE);		/* subnodes: 0=type */
	tnd->hook_copy = cprop_consthook_hook_copy;
	tnd->hook_free = cprop_consthook_hook_free;
	tnd->hook_dumptree = cprop_consthook_hook_dumptree;
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (cprop_namemap_const));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "gettype", 2, LANGOPTYPE (cprop_gettype_const));
	tnode_setlangop (lops, "isconst", 1, LANGOPTYPE (cprop_isconst_const));
	tnode_setlangop (lops, "iscomplex", 2, LANGOPTYPE (cprop_iscomplex_const));
	tnode_setlangop (lops, "constvalof", 2, LANGOPTYPE (cprop_constvalof_const));
	tnode_setlangop (lops, "getdescriptor", 2, LANGOPTYPE (cprop_getdescriptor_const));
	tnd->lops = lops;

	i = -1;
	tag_CONST = tnode_newnodetag ("CONST", &i, tnd, NTF_NONE);

	/*}}}*/

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
	case CONST_BOOL:
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
	case CONST_BOOL:
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
/*{{{  int constprop_checkintrange (tnode_t *node, const int issigned, const int bits)*/
/*
 *	checks to see if a constant is within a given range (by signed-ness and bit-width)
 *	returns non-zero on success, zero on failure
 */
int constprop_checkintrange (tnode_t *node, const int issigned, const int bits)
{
	if (!node) {
		nocc_warning ("constprop_checkintrange(): NULL node!");
		return 0;
	}
	switch (constprop_consttype (node)) {
	case CONST_BYTE:
		{
			unsigned long long max = (1ULL << (bits - (issigned ? 1 : 0))) - 1;
			unsigned long long val = 0ULL;
			unsigned char ch = 0;

			cprop_constvalof_const (NULL, node, (void *)&ch);
			val = (unsigned long long)ch;
			if (val > max) {
				return 0;
			}
		}
		break;
	case CONST_ULL:
		{
			unsigned long long max = (1ULL << (bits - (issigned ? 1 : 0))) - 1;
			unsigned long long val = 0ULL;

			cprop_constvalof_const (NULL, node, (void *)&val);
			if (val > max) {
				return 0;
			}
		}
		break;
	case CONST_INT:
		{
			long long min = issigned ? -(1LL << (bits - (issigned ? 1 : 0))) : 0LL;
			long long max = (1LL << (bits - (issigned ? 1 : 0))) - 1;
			long long val = 0ULL;
			int ival;

			cprop_constvalof_const (NULL, node, (void *)&ival);
			val = (long long)ival;
			if ((val < min) || (val > max)) {
				return 0;
			}
		}
		break;
	case CONST_BOOL:
		/* always fits */
		break;
	case CONST_DOUBLE:
		{
			long long min = issigned ? -(1LL << (bits - (issigned ? 1 : 0))) : 0LL;
			long long max = (1LL << (bits - (issigned ? 1 : 0))) - 1;
			long long val = 0ULL;
			double dval;

			cprop_constvalof_const (NULL, node, (void *)&dval);
			val = (long long)dval;
			if ((val < min) || (val > max)) {
				return 0;
			}
		}
		break;
	default:
		nocc_warning ("constprop_checkintrange(): unknown constant type %d!", (int)constprop_consttype (node));
		return 0;
	}
	return 1;
}
/*}}}*/


/*{{{  void constprop_warning (tnode_t *t, const char *fmt, ...)*/
/*
 *	generates a generic warning message
 */
void constprop_warning (tnode_t *t, const char *fmt, ...)
{
	va_list ap;
	static char warnbuf[512];
	int n;
	lexfile_t *lf = t->org_file;

	va_start (ap, fmt);
	n = sprintf (warnbuf, "%s:%d (warning) ", lf ? lf->fnptr : "(unknown)", t->org_line);
	vsnprintf (warnbuf + n, 512 - n, fmt, ap);
	va_end (ap);

	if (lf) {
		lf->warncount++;
	}

	nocc_outerrmsg (warnbuf);

	return;
}
/*}}}*/
/*{{{  void constprop_error (tnode_t *t, const char *fmt, ...)*/
/*
 *	generates an error message
 */
void constprop_error (tnode_t *t, const char *fmt, ...)
{
	va_list ap;
	static char errbuf[512];
	int n;
	lexfile_t *lf = t->org_file;

	va_start (ap, fmt);
	n = sprintf (errbuf, "%s:%d (error) ", lf ? lf->fnptr : "(unknown)", t->org_line);
	vsnprintf (errbuf + n, 512 - n, fmt, ap);
	va_end (ap);

	if (lf) {
		lf->errcount++;
	}

	nocc_outerrmsg (errbuf);

	return;
}
/*}}}*/



