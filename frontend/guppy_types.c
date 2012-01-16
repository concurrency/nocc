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
	int sign;						/* whether or not this is a signed type */
} primtypehook_t;

typedef struct TAG_chantypehook {
	int marked_svr, marked_cli;				/* whether marked as client or server (or neither) */
} chantypehook_t;

/*}}}*/


/*{{{  static primtypehook_t *guppy_newprimtypehook (void)*/
/*
 *	creates a new primtypehook_t structure
 */
static primtypehook_t *guppy_newprimtypehook (void)
{
	primtypehook_t *pth = (primtypehook_t *)smalloc (sizeof (primtypehook_t));

	pth->size = 0;
	pth->sign = 1;
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
/*{{{  static chantypehook_t *guppy_newchantypehook (void)*/
/*
 *	creates a new chantypehook_t
 */
static chantypehook_t *guppy_newchantypehook (void)
{
	chantypehook_t *cth = (chantypehook_t *)smalloc (sizeof (chantypehook_t));

	cth->marked_svr = 0;
	cth->marked_cli = 1;
	return cth;
}
/*}}}*/
/*{{{  static void guppy_freechantypehook (chantypehook_t *cth)*/
/*
 *	frees a chantypehook_t structure
 */
static void guppy_freechantypehook (chantypehook_t *cth)
{
	if (!cth) {
		nocc_serious ("guppy_freechantypehook(): NULL argument!");
		return;
	}
	sfree (cth);
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
/*{{{  static void guppy_reduce_chantype (dfastate_t *dfast, parsepriv_t *pp, void *rarg)*/
/*
 *	reduces a channel type.  Note: expects to be called with the local stack containing the sub-protocol and a token or NULL
 */
static void guppy_reduce_chantype (dfastate_t *dfast, parsepriv_t *pp, void *rarg)
{
	tnode_t *proto = dfa_popnode (dfast);
	token_t *tok = parser_gettok (pp);
	token_t *cttok = NULL;
	chantypehook_t *cth = guppy_newchantypehook ();

	if (lexer_tokmatchlitstr (tok, "?")) {
		cth->marked_svr = 1;
		cttok = tok;
		tok = parser_gettok (pp);
	} else if (lexer_tokmatchlitstr (tok, "!")) {
		cth->marked_cli = 1;
		cttok = tok;
		tok = parser_gettok (pp);
	} else {
		cttok = NULL;
	}

	*(dfast->ptr) = tnode_create (gup.tag_CHAN, tok->origin, proto, cth);

	if (cttok) {
		lexer_freetoken (cttok);
		cttok = NULL;
	}
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
	memcpy (pth, opth, sizeof (primtypehook_t));

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
	fprintf (stream, "<primtypehook size=\"%d\" sign=\"%d\" />\n", pth->size, pth->sign);
	return;
}
/*}}}*/

/*{{{  static void guppy_chantype_hook_free (void *hook)*/
/*
 *	frees a channel type hook
 */
static void guppy_chantype_hook_free (void *hook)
{
	if (!hook) {
		return;
	}
	guppy_freechantypehook ((chantypehook_t *)hook);
	return;
}
/*}}}*/
/*{{{  static void *guppy_chantype_hook_copy (void *hook)*/
/*
 *	copies a channel type hook
 */
static void *guppy_chantype_hook_copy (void *hook)
{
	chantypehook_t *cth, *octh;

	if (!hook) {
		return hook;
	}
	octh = (chantypehook_t *)hook;
	cth = guppy_newchantypehook ();
	memcpy (cth, octh, sizeof (chantypehook_t));

	return (void *)cth;
}
/*}}}*/
/*{{{  static void guppy_chantype_hook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	dump-tree for channel type hook
 */
static void guppy_chantype_hook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	chantypehook_t *cth = (chantypehook_t *)hook;

	if (!cth) {
		return;
	}
	guppy_isetindent (stream, indent);
	fprintf (stream, "<chantypehook marked_svr=\"%d\" marked_cli=\"%d\" />\n", cth->marked_svr, cth->marked_cli);
	return;
}
/*}}}*/


/*{{{  tnode_t *guppy_newprimtype (ntdef_t *tag, tnode_t *org, const int size)*/
/*
 *	creates a new primitive type node (used internally to type things)
 */
tnode_t *guppy_newprimtype (ntdef_t *tag, tnode_t *org, const int size)
{
	tnode_t *ptype;
	primtypehook_t *pth = guppy_newprimtypehook ();

	pth->size = size;
	if (org) {
		ptype = tnode_createfrom (tag, org, pth);
	} else {
		ptype = tnode_createfrom (tag, NULL, pth);
	}

	return ptype;
}
/*}}}*/
/*{{{  static int guppy_bytesfor_primtype (langops_t *lops, tnode_t *t, target_t *target)*/
/*
 *	returns the number of bytes required to hold a particular type (primitive or pointer).
 */
static int guppy_bytesfor_primtype (langops_t *lops, tnode_t *t, target_t *target)
{
	primtypehook_t *pth = (primtypehook_t *)tnode_nthhookof (t, 0);

	if (t->tag == gup.tag_BOOL) {
		return target ? target->intsize : 4;
	} else if (t->tag == gup.tag_INT) {
		if (pth->size) {
			/* number of bits rounded up into bytes */
			return (pth->size >> 3) + ((pth->size & 0x07) ? 1 : 0);
		} else {
			return target ? target->intsize : 4;
		}
	} else if (t->tag == gup.tag_REAL) {
		if (pth->size) {
			/* number of bits rounded up into bytes */
			return (pth->size >> 3) + ((pth->size & 0x07) ? 1 : 0);
		} else {
			return target ? target->intsize : 4;
		}
	} else if (t->tag == gup.tag_BYTE) {
		/* byte will always be 1 byte :) */
		return 1;
	} else if (t->tag == gup.tag_CHAR) {
		return target ? target->charsize : 4;			/* allow enough for unicode */
	} else if (t->tag == gup.tag_STRING) {
		return target ? target->pointersize : 4;		/* strings always pointers */
	}
	return -1;
}
/*}}}*/
/*{{{  static int guppy_getctypeof_primtype (langops_t *lops, tnode_t *t, char **str)*/
/*
 *	generates a C string for a particular type.
 */
static int guppy_getctypeof_primtype (langops_t *lops, tnode_t *t, char **str)
{
	primtypehook_t *pth = (primtypehook_t *)tnode_nthhookof (t, 0);
	char *lstr = NULL;

	if (t->tag == gup.tag_BOOL) {
		lstr = string_dup ("int");
	} else if (t->tag == gup.tag_INT) {
		int bytes = (pth->size >> 3) + ((pth->size & 0x07) ? 1 : 0);

		switch (pth->size) {
		case 8:
			if (pth->sign) {
				lstr = string_dup ("int8_t");
			} else {
				lstr = string_dup ("uint8_t");
			}
			break;
		case 16:
			if (pth->sign) {
				lstr = string_dup ("int16_t");
			} else {
				lstr = string_dup ("uint16_t");
			}
			break;
		case 32:
			if (pth->sign) {
				lstr = string_dup ("int32_t");
			} else {
				lstr = string_dup ("uint32_t");
			}
			break;
		case 64:
			if (pth->sign) {
				lstr = string_dup ("int64_t");
			} else {
				lstr = string_dup ("uint64_t");
			}
			break;
		default:
			/* FIXME: breaks for non-standard stuff.. */
			lstr = string_dup ("void");
			break;
		}
	} else if (t->tag == gup.tag_REAL) {
		switch (pth->size) {
		case 32:
			lstr = string_dup ("float");
			break;
		case 64:
			lstr = string_dup ("double");
			break;
		default:
			/* FIXME: breaks for non-standard stuff.. */
			lstr = string_dup ("void");
			break;
		}
	} else if (t->tag == gup.tag_BYTE) {
		lstr = string_dup ("unsigned char");
	} else if (t->tag == gup.tag_CHAR) {
		lstr = string_dup ("int");
	} else if (t->tag == gup.tag_STRING) {
		lstr = string_dup ("char *");
	} else {
		lstr = NULL;
	}

	if (*str) {
		sfree (*str);
	}
	*str = lstr;

	return 0;
}
/*}}}*/
/*{{{  static int guppy_getctypeof_chantype (langops_t *lops, tnode_t *t, char **str)*/
/*
 *	generates a C string for a particular channel-type.
 */
static int guppy_getctypeof_chantype (langops_t *lops, tnode_t *t, char **str)
{
	chantypehook_t *cth = (chantypehook_t *)tnode_nthhookof (t, 0);
	char *lstr = NULL;

	lstr = string_dup ("Channel *");
	if (*str) {
		sfree (*str);
	}
	*str = lstr;

	return 0;
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
	fcnlib_addfcn ("guppy_reduce_chantype", guppy_reduce_chantype, 0, 3);

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
	tnode_setlangop (lops, "bytesfor", 2, LANGOPTYPE (guppy_bytesfor_primtype));
	tnode_setlangop (lops, "getctypeof", 2, LANGOPTYPE (guppy_getctypeof_primtype));
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
	/*{{{  guppy:chantype -- CHAN*/
	i = -1;
	tnd = tnode_newnodetype ("guppy:chantype", &i, 1, 0, 1, TNF_NONE);		/* subnotes: 0 = type; hooks: 0 = chantypehook_t */
	tnd->hook_free = guppy_chantype_hook_free;
	tnd->hook_copy = guppy_chantype_hook_copy;
	tnd->hook_dumptree = guppy_chantype_hook_dumptree;
	cops = tnode_newcompops ();
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "getctypeof", 2, LANGOPTYPE (guppy_getctypeof_chantype));
	tnd->lops = lops;

	i = -1;
	gup.tag_CHAN = tnode_newnodetag ("CHAN", &i, tnd, NTF_NONE);

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

