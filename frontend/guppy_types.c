/*
 *	guppy_types.c -- types for Guppy
 *	Copyright (C) 2010 Fred Barnes <frmb@kent.ac.uk>
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
#include "origin.h"
#include "symbols.h"
#include "keywords.h"
#include "lexer.h"
#include "lexpriv.h"
#include "tnode.h"
#include "parser.h"
#include "dfa.h"
#include "dfaerror.h"
#include "parsepriv.h"
#include "guppy.h"
#include "feunit.h"
#include "fcnlib.h"
#include "names.h"
#include "scope.h"
#include "prescope.h"
#include "library.h"
#include "typecheck.h"
#include "constprop.h"
#include "precheck.h"
#include "usagecheck.h"
#include "map.h"
#include "target.h"
#include "transputer.h"
#include "codegen.h"
#include "langops.h"
#include "fetrans.h"
#include "betrans.h"
#include "metadata.h"
#include "tracescheck.h"
#include "mobilitycheck.h"


/*}}}*/
/*{{{  private types/data*/

typedef struct TAG_primtypehook {
	int size;						/* bit-size for some primitive types */
} primtypehook_t;

/*}}}*/


/*{{{  static primtypehook_t *guppy_newprimtypehook (void)*/
/*
 *	creates a new primtypehook_t structure
 */
static primtypehook_t *guppy_newprimtypehook (void)
{
	primtypehook_t *pth = (primtypehook_t *)smalloc (sizeof (primtypehook_t));

	pth->size = 0;
	return pth;
}
/*}}}*/
/*{{{  static void guppy_freeprimtypehook (primtypehook_t *pth)*/
/*
 *	frees a primtypehook_t structure
 */
static void guppy_freeprimtypehook (primtypehook_t *pth)
{
	if (!pth) {
		nocc_serious ("guppy_freeprimtypehook(): NULL argument!");
		return;
	}
	sfree (pth);
}
/*}}}*/


/*{{{  static void guppy_reduce_primtype (dfastate_t *dfast, parsepriv_t *pp, void *rarg)*/
/*
 *	reduces a primitive type -- some of these may be sized
 */
static void guppy_reduce_primtype (dfastate_t *dfast, parsepriv_t *pp, void *rarg)
{
	token_t *tok;
	ntdef_t *tag;

	tok = parser_gettok (pp);
	if (tok->type == KEYWORD) {
		char *ustr = string_upper (tok->u.kw->name);

		tag = tnode_lookupnodetag (ustr);
		sfree (ustr);
		if (!tag) {
			parser_error (pp->lf, "unknown primitive type [%s] in guppy_reduce_primtype()", tok->u.kw->name);
			goto out_error;
		}
	} else {
		parser_error (pp->lf, "unknown primitive type in guppy_reduce_primtype()");
		goto out_error;
	}

	*(dfast->ptr) = tnode_create (tag, tok->origin);
	
	/* may have a size for a primitive type hook */
	if ((tag == gup.tag_INT) || (tag == gup.tag_REAL)) {
		primtypehook_t *pth = guppy_newprimtypehook ();

		pth->size = (int)tok->iptr;
		tnode_setnthhook (*(dfast->ptr), 0, pth);
	}
out_error:
	lexer_freetoken (tok);

	return;
}
/*}}}*/


/*{{{  static void guppy_primtype_hook_free (void *hook)*/
/*
 *	frees a primitive type hook
 */
static void guppy_primtype_hook_free (void *hook)
{
	if (!hook) {
		return;
	}
	guppy_freeprimtypehook ((primtypehook_t *)hook);
	return;
}
/*}}}*/
/*{{{  static void *guppy_primtype_hook_copy (void *hook)*/
/*
 *	copies a primitive type hook
 */
static void *guppy_primtype_hook_copy (void *hook)
{
	primtypehook_t *pth, *opth;

	if (!hook) {
		return hook;
	}
	opth = (primtypehook_t *)hook;
	pth = guppy_newprimtypehook ();
	pth->size = opth->size;

	return (void *)pth;
}
/*}}}*/
/*{{{  static void guppy_primtype_hook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	dump-tree for primitive type hook
 */
static void guppy_primtype_hook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	primtypehook_t *pth = (primtypehook_t *)hook;

	if (!pth) {
		return;
	}
	guppy_isetindent (stream, indent);
	fprintf (stream, "<primtypehook size=\"%d\" />\n", pth->size);
	return;
}
/*}}}*/


/*{{{  static int guppy_types_init_nodes (void)*/
/*
 *	sets up type nodes for guppy
 *	returns 0 on success, non-zero on failure
 */
static int guppy_types_init_nodes (void)
{
	tndef_t *tnd;
	compops_t *cops;
	langops_t *lops;
	int i;

	/*{{{  register reduction functions*/
	fcnlib_addfcn ("guppy_reduce_primtype", guppy_reduce_primtype, 0, 3);

	/*}}}*/
	/*{{{  guppy:primtype -- INT, REAL, BOOL, BYTE, CHAR, STRING*/
	i = -1;
	tnd = tnode_newnodetype ("guppy:primtype", &i, 0, 0, 1, TNF_NONE);		/* hooks: primtypehook_t */
	tnd->hook_free = guppy_primtype_hook_free;
	tnd->hook_copy = guppy_primtype_hook_copy;
	tnd->hook_dumptree = guppy_primtype_hook_dumptree;
	cops = tnode_newcompops ();
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnd->lops = lops;

	i = -1;
	gup.tag_INT = tnode_newnodetag ("INT", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_REAL = tnode_newnodetag ("REAL", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_BOOL = tnode_newnodetag ("BOOL", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_BYTE = tnode_newnodetag ("BYTE", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_CHAR = tnode_newnodetag ("CHAR", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_STRING = tnode_newnodetag ("STRING", &i, tnd, NTF_NONE);

	/*}}}*/

	return 0;
}
/*}}}*/
/*{{{  static int guppy_types_post_setup (void)*/
/*
 *	does post-setup for type nodes
 *	returns 0 on success, non-zero on failure
 */
static int guppy_types_post_setup (void)
{
	return 0;
}
/*}}}*/


/*{{{  guppy_types_feunit (feunit_t)*/
feunit_t guppy_types_feunit = {
	.init_nodes = guppy_types_init_nodes,
	.reg_reducers = NULL,
	.init_dfatrans = NULL,
	.post_setup = guppy_types_post_setup,
	.ident = "guppy-types"
};
/*}}}*/

