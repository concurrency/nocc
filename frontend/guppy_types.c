/*
 *	guppy_types.c -- types for Guppy
 *	Copyright (C) 2010-2013 Fred Barnes <frmb@kent.ac.uk>
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
#include "fhandle.h"
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
#include "cccsp.h"


/*}}}*/
/*{{{  private types/data*/

typedef struct TAG_primtypehook {
	int size;						/* bit-size for some primitive types */
	int sign;						/* whether or not this is a signed type */
	int strlen;						/* for strings, length in characters (or -1) */
} primtypehook_t;

typedef struct TAG_chantypehook {
	int marked_svr, marked_cli;				/* whether marked as client or server (or neither) */
} chantypehook_t;

typedef struct TAG_arraytypehook {
	int ndim;						/* number of dimensions */
	int nelem;						/* total number of elements if constant or -1 */
	int *known_sizes;					/* constant known sizes (of all dimensions) or -1 */
	int constprop;						/* non-zero if constant propagation has been done (known sizes filled in) */
} arraytypehook_t;

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
	pth->strlen = -1;
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
	cth->marked_cli = 0;
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
/*{{{  static arraytypehook_t *guppy_newarraytypehook (void)*/
/*
 *	creates a new arraytypehook_t
 */
static arraytypehook_t *guppy_newarraytypehook (void)
{
	arraytypehook_t *ath = (arraytypehook_t *)smalloc (sizeof (arraytypehook_t));

	ath->ndim = 0;
	ath->nelem = -1;
	ath->known_sizes = NULL;
	ath->constprop = 0;
	return ath;
}
/*}}}*/
/*{{{  static arraytypehook_t *guppy_newarraytypehook_nd (const int ndim)*/
/*
 *	creates a new arraytypehook_t (known number of dimensions)
 */
static arraytypehook_t *guppy_newarraytypehook_nd (const int ndim)
{
	arraytypehook_t *ath = (arraytypehook_t *)smalloc (sizeof (arraytypehook_t));
	int i;

	ath->ndim = ndim;
	ath->nelem = -1;
	ath->known_sizes = (int *)smalloc (ndim * sizeof (int));
	for (i=0; i<ndim; i++) {
		ath->known_sizes[i] = -1;
	}
	ath->constprop = 0;

	return ath;
}
/*}}}*/
/*{{{  static void guppy_freearraytypehook (arraytypehook_t *ath)*/
/*
 *	frees an arraytypehook_t structure
 */
static void guppy_freearraytypehook (arraytypehook_t *ath)
{
	if (!ath) {
		nocc_serious ("guppy_freearraytypehook(): NULL argument!");
		return;
	}
	if (ath->known_sizes) {
		sfree (ath->known_sizes);
		ath->known_sizes = NULL;
	}
	ath->ndim = -1;
	ath->nelem = -1;

	sfree (ath);
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
			parser_error (SLOCN (pp->lf), "unknown primitive type [%s] in guppy_reduce_primtype()", tok->u.kw->name);
			goto out_error;
		}
	} else {
		parser_error (SLOCN (pp->lf), "unknown primitive type in guppy_reduce_primtype()");
		goto out_error;
	}

	*(dfast->ptr) = tnode_create (tag, SLOCN (tok->origin), NULL);
	
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

#if 0
fprintf (stderr, "guppy_reduce_chantype(): here! (tok = %s)\n", lexer_stokenstr (tok));
#endif
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
#if 0
fprintf (stderr, "guppy_reduce_chantype(): here2! (tok = %s)\n", lexer_stokenstr (tok));
#endif

	*(dfast->ptr) = tnode_create (gup.tag_CHAN, SLOCN (tok->origin), proto, cth);

	if (cttok) {
		lexer_freetoken (cttok);
		cttok = NULL;
	}
	lexer_freetoken (tok);
	return;
}
/*}}}*/
/*{{{  static void guppy_reduce_arraytype (dfastate_t *dfast, parsepriv_t *pp, void *rarg)*/
/*
 *	reduces an array type.  Note: expects to be called with the local stack containing the sub-type and the dimension.
 */
static void guppy_reduce_arraytype (dfastate_t *dfast, parsepriv_t *pp, void *rarg)
{
	tnode_t *subtype = dfa_popnode (dfast);
	tnode_t *dim = dfa_popnode (dfast);
	arraytypehook_t *ath = guppy_newarraytypehook ();

	*(dfast->ptr) = tnode_createfrom (gup.tag_ARRAY, subtype, dim, subtype, ath);

	return;
}
/*}}}*/

/*{{{  static void guppy_setarraytypedims (arraytypehook_t *ath, int ndims, ...)*/
/*
 *	sets array type dimensions.
 */
static void guppy_setarraytypedims (arraytypehook_t *ath, int ndims, ...)
{
	int i;
	va_list ap;
	int all_known = 1;

	ath->ndim = ndims;
	if (ath->known_sizes) {
		sfree (ath->known_sizes);
		ath->known_sizes = NULL;
	}
	if (ndims > 0) {
		ath->known_sizes = (int *)smalloc (ndims * sizeof (int));
	}
	va_start (ap, ndims);
	ath->nelem = 1;
	for (i=0; i<ndims; i++) {
		int arg = va_arg (ap, int);

		ath->known_sizes[i] = arg;
		if (arg == -1) {
			all_known = 0;
		} else {
			ath->nelem *= arg;
		}
	}
	va_end (ap);
	if (!all_known) {
		ath->nelem = -1;
	}
	return;
}
/*}}}*/
/*{{{  static void guppy_setarraytypedims_undef (arraytypehook_t *ath, int ndims)*/
/*
 *	sets array type dimensions to unknown/undefined.
 */
static void guppy_setarraytypedims_undef (arraytypehook_t *ath, int ndims)
{
	int i;

	ath->ndim = ndims;
	ath->nelem = -1;
	if (ath->known_sizes) {
		sfree (ath->known_sizes);
		ath->known_sizes = NULL;
	}
	if (ndims > 0) {
		ath->known_sizes = (int *)smalloc (ndims * sizeof (int));
	}
	for (i=0; i<ndims; i++) {
		ath->known_sizes[i] = -1;
	}
	return;
}
/*}}}*/

/*{{{  static void guppy_primtype_hook_free (void *hook)*/
/*
 *	frees a primitive type hook
 */
static void guppy_primtype_hook_free (void *hook)
{
#if 0
fprintf (stderr, "guppy_primtype_hook_free(): here! hook = 0x%8.8x\n", (unsigned int)hook);
#endif
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
/*{{{  static void guppy_primtype_hook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)*/
/*
 *	dump-tree for primitive type hook
 */
static void guppy_primtype_hook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)
{
	primtypehook_t *pth = (primtypehook_t *)hook;

	if (!pth) {
		return;
	}
	guppy_isetindent (stream, indent);
	fhandle_printf (stream, "<primtypehook size=\"%d\" sign=\"%d\" strlen=\"%d\" />\n", pth->size, pth->sign, pth->strlen);
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
/*{{{  static void guppy_chantype_hook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)*/
/*
 *	dump-tree for channel type hook
 */
static void guppy_chantype_hook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)
{
	chantypehook_t *cth = (chantypehook_t *)hook;

	if (!cth) {
		return;
	}
	guppy_isetindent (stream, indent);
	fhandle_printf (stream, "<chantypehook marked_svr=\"%d\" marked_cli=\"%d\" />\n", cth->marked_svr, cth->marked_cli);
	return;
}
/*}}}*/

/*{{{  static void guppy_arraytype_hook_free (void *hook)*/
/*
 *	frees an array-type hook
 */
static void guppy_arraytype_hook_free (void *hook)
{
	arraytypehook_t *ath = (arraytypehook_t *)hook;

	if (!ath) {
		return;
	}
	guppy_freearraytypehook (ath);
	return;
}
/*}}}*/
/*{{{  static void *guppy_arraytype_hook_copy (void *hook)*/
/*
 *	copies an array type hook
 */
static void *guppy_arraytype_hook_copy (void *hook)
{
	arraytypehook_t *ath, *oath;

	oath = (arraytypehook_t *)hook;
	if (!oath) {
		return NULL;
	}
	ath = guppy_newarraytypehook ();
	memcpy (ath, oath, sizeof (arraytypehook_t));

	return (void *)ath;
}
/*}}}*/
/*{{{  static void guppy_arraytype_hook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)*/
/*
 *	dump-tree for array type hook
 */
static void guppy_arraytype_hook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)
{
	arraytypehook_t *ath = (arraytypehook_t *)hook;
	int i;

	if (!ath) {
		return;
	}
	guppy_isetindent (stream, indent);
	fhandle_printf (stream, "<arraytypehook ndim=\"%d\" nelem=\"%d\" known_sizes=\"", ath->ndim, ath->nelem);
	for (i=0; i<ath->ndim; i++) {
		fhandle_printf (stream, "%s%d", i ? "," : "", ath->known_sizes[i]);
	}
	fhandle_printf (stream, "\" />\n");
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

	if (tag == gup.tag_STRING) {
		pth->strlen = size;
	} else {
		pth->size = size;
	}
	ptype = tnode_createfrom (tag, org, pth);

	return ptype;
}
/*}}}*/
/*{{{  tnode_t *guppy_newchantype (ntdef_t *tag, tnode_t *org, tnode_t *protocol)*/
/*
 *	creates a new channel type node (used internally to generate channels)
 */
tnode_t *guppy_newchantype (ntdef_t *tag, tnode_t *org, tnode_t *protocol)
{
	tnode_t *ctype;
	chantypehook_t *cth = guppy_newchantypehook ();

	ctype = tnode_createfrom (tag, org, protocol, cth);

	return ctype;
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
/*{{{  static int guppy_getdescriptor_primtype (langops_t *lops, tnode_t *node, char **sptr)*/
/*
 *	generates a descriptor string for a particular type.
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_getdescriptor_primtype (langops_t *lops, tnode_t *node, char **sptr)
{
	primtypehook_t *pth = (primtypehook_t *)tnode_nthhookof (node, 0);
	char *lstr = NULL;

	if (node->tag == gup.tag_BOOL) {
		lstr = string_dup ("bool");
	} else if (node->tag == gup.tag_INT) {
		if (pth->sign) {
			if (pth->size == 0) {
				lstr = string_dup ("int");
			} else {
				lstr = string_fmt ("int%d", pth->size);
			}
		} else {
			if (pth->size == 0) {
				lstr = string_dup ("uint");
			} else {
				lstr = string_fmt ("uint%d", pth->size);
			}
		}
	} else if (node->tag == gup.tag_REAL) {
		lstr = string_fmt ("real%d", pth->size);
	} else if (node->tag == gup.tag_BYTE) {
		lstr = string_dup ("byte");
	} else if (node->tag == gup.tag_CHAR) {
		lstr = string_dup ("char");
	} else if (node->tag == gup.tag_STRING) {
		lstr = string_dup ("string");
	} else {
		nocc_internal ("guppy_getdescriptor_primtype(): unhandled [%s]", node->tag->name);
		return 0;
	}

	if (*sptr) {
		char *tmpstr = string_fmt ("%s%s", *sptr, lstr);

		sfree (*sptr);
		sfree (lstr);
		*sptr = tmpstr;
	} else {
		*sptr = lstr;
	}
	return 0;
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
		case 0:
			if (pth->sign) {
				lstr = string_dup ("int");
			} else {
				lstr = string_dup ("unsigned int");
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
		lstr = string_dup ("gtString_t");
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
/*{{{  static tnode_t *guppy_gettype_primtype (langops_t *lops, tnode_t *node, tnode_t *default_type)*/
/*
 *	returns the type of a primitive type (id function).
 */
static tnode_t *guppy_gettype_primtype (langops_t *lops, tnode_t *node, tnode_t *default_type)
{
	primtypehook_t *pth = (primtypehook_t *)tnode_nthhookof (node, 0);

#if 1
fprintf (stderr, "guppy_gettype_primtype(): default_type is 0x%8.8x\n", (int)default_type);
#endif
	return node;
}
/*}}}*/
/*{{{  static tnode_t *guppy_typeactual_primtype (langops_t *lops, tnode_t *formaltype, tnode_t *actualtype, tnode_t *node, typecheck_t *tc)*/
/*
 *	does type-compatibility check on a primitive type, returns actual type used
 */
static tnode_t *guppy_typeactual_primtype (langops_t *lops, tnode_t *formaltype, tnode_t *actualtype, tnode_t *node, typecheck_t *tc)
{
	tnode_t *atype = NULL;

#if 0
fprintf (stderr, "guppy_typeactual_primtype(): formaltype=[%s], actualtype=[%s]\n", formaltype->tag->name, actualtype->tag->name);
#endif
	if (formaltype->tag == actualtype->tag) {
		atype = actualtype;
	} else {
		typecheck_error (node, tc, "incompatible types in operation [%s,%s]", formaltype->tag->name, actualtype->tag->name);
	}

	return atype;
}
/*}}}*/
/*{{{  static int guppy_knownsizeof_primtype (langops_t *lops, tnode_t *t)*/
/*
 *	returns the known-size of some primitive type (number of chars in string usually), -1 if unknown
 */
static int guppy_knownsizeof_primtype (langops_t *lops, tnode_t *t)
{
	primtypehook_t *pth;

	if (!t || (t->tag != gup.tag_STRING)) {
		return -1;
	}
	pth = (primtypehook_t *)tnode_nthhookof (t, 0);

	return pth->strlen;
}
/*}}}*/
/*{{{  static int guppy_typehash_primtype (langops_t *lops, tnode_t *t, int hsize, void *ptr)*/
/*
 *	gets the type-hash for a primitive type
 *	returns 0 on success, non-zero on failure
 */
static int guppy_typehash_primtype (langops_t *lops, tnode_t *t, int hsize, void *ptr)
{
	primtypehook_t *pth = (primtypehook_t *)tnode_nthhookof (t, 0);
	unsigned int myhash = 0;

	if (t->tag == gup.tag_BOOL) {
		myhash = 0x36136930;
	} else if (t->tag == gup.tag_INT) {
		if (pth->sign) {
			myhash = 0xa36cfe00 + pth->size;
		} else {
			myhash = 0xc30ae600 + pth->size;
		}
	} else if (t->tag == gup.tag_REAL) {
		myhash = 0x048caee1 + pth->size;
	} else if (t->tag == gup.tag_BYTE) {
		myhash = 0x1046ca0b;
	} else if (t->tag == gup.tag_CHAR) {
		myhash = 0xfeedf00d;
	} else if (t->tag == gup.tag_STRING) {
		myhash = 0xbeef0011;
	} else {
		nocc_internal ("guppy_typehash_primtype(): unhandled [%s]", t->tag->name);
		return 0;
	}

	langops_typehash_blend (hsize, ptr, sizeof (myhash), (void *)&myhash);
	return 0;
}
/*}}}*/
/*{{{  static int guppy_isdefpointer_primtype (langops_t *lops, tnode_t *node)*/
/*
 *	returns default indirection level for primitive type
 */
static int guppy_isdefpointer_primtype (langops_t *lops, tnode_t *node)
{
	if (node->tag == gup.tag_STRING) {
		return 1;
	}
	return 0;
}
/*}}}*/
/*{{{  static tnode_t *guppy_initcall_primtype (langops_t *lops, tnode_t *typenode, tnode_t *name)*/
/*
 *	generates initialiser for primitive types (string only)
 */
static tnode_t *guppy_initcall_primtype (langops_t *lops, tnode_t *typenode, tnode_t *name)
{
	if (typenode->tag == gup.tag_STRING) {
		tnode_t *inode;

		inode = tnode_create (gup.tag_STRINIT, SLOCI, NULL, typenode, name);
		return inode;
	}
	return NULL;
}
/*}}}*/
/*{{{  static tnode_t *guppy_freecall_primtype (langops_t *lops, tnode_t *typenode, tnode_t *name)*/
/*
 *	generates finaliser for primitive types (string only)
 */
static tnode_t *guppy_freecall_primtype (langops_t *lops, tnode_t *typenode, tnode_t *name)
{
	if (typenode->tag == gup.tag_STRING) {
		tnode_t *inode;

		inode = tnode_create (gup.tag_STRFREE, SLOCI, NULL, typenode, name);
		return inode;
	}
	return NULL;
}
/*}}}*/
/*{{{  static int guppy_namemap_typeaction_primtype (langops_t *lops, tnode_t *typenode, tnode_t **nodep, map_t *map)*/
/*
 *	handles mapping for channel IO on strings
 *	returns 0 to stop walk, 1 to continue (in map), -1 to revert to default behaviour
 */
static int guppy_namemap_typeaction_primtype (langops_t *lops, tnode_t *typenode, tnode_t **nodep, map_t *map)
{
	if (typenode->tag == gup.tag_STRING) {
		int bytes = tnode_bytesfor (typenode, map->target);
		tnode_t *newinst, *newparms;
		tnode_t *newslist, *newseq;
		tnode_t *sizeexp = constprop_newconst (CONST_INT, NULL, NULL, bytes);
		tnode_t *newarg;
		tnode_t *callnum, *wptr;
		cccsp_mapdata_t *cmd = (cccsp_mapdata_t *)map->hook;
		int saved_indir = cmd->target_indir;
		tnode_t *cargs, **cargp;

		cargs = parser_newlistnode (SLOCI);
		cargp = parser_addtolist (cargs, cmd->process_id);
		cmd->target_indir = 0;
		map_submapnames (cargp, map);
		cargp = parser_addtolist (cargs, tnode_nthsubof (*nodep, 1));
		if ((*nodep)->tag == gup.tag_INPUT) {
			cmd->target_indir = 1;
		} else {
			cmd->target_indir = 2;
		}
		map_submapnames (cargp, map);

		wptr = cmd->process_id;
		map_submapnames (&wptr, map);

		/* map LHS and RHS: LHS channel must be a pointer */
		cmd->target_indir = 1;
		map_submapnames (tnode_nthsubaddr (*nodep, 0), map);
		cmd->target_indir = 2;
		map_submapnames (tnode_nthsubaddr (*nodep, 1), map);
		cmd->target_indir = 0;
		map_submapnames (&sizeexp, map);

		/* transform into CCSP API call */
		newparms = parser_newlistnode (SLOCI);
		parser_addtolist (newparms, wptr);
		parser_addtolist (newparms, tnode_nthsubof (*nodep, 0));	/* channel */
		parser_addtolist (newparms, tnode_nthsubof (*nodep, 1));	/* data */
		parser_addtolist (newparms, sizeexp);

		newslist = parser_newlistnode (SLOCI);
		newseq = tnode_createfrom (gup.tag_SEQ, *nodep, NULL, newslist);

		if ((*nodep)->tag == gup.tag_INPUT) {
			tnode_t *ccall;

			callnum = cccsp_create_apicallname (CHAN_IN);
			newinst = tnode_createfrom (gup.tag_APICALL, *nodep, callnum, newparms);

			ccall = tnode_createfrom (gup.tag_APICALL, *nodep, cccsp_create_apicallname (STR_FREE), cargs);

			parser_addtolist (newslist, ccall);
			parser_addtolist (newslist, newinst);
		} else if ((*nodep)->tag == gup.tag_OUTPUT) {
			tnode_t *ccall;

			callnum = cccsp_create_apicallname (CHAN_OUT);
			newinst = tnode_createfrom (gup.tag_APICALL, *nodep, callnum, newparms);

			ccall = tnode_createfrom (gup.tag_APICALL, *nodep, cccsp_create_apicallname (STR_CLEAR), cargs);

			parser_addtolist (newslist, newinst);
			parser_addtolist (newslist, ccall);
		} else {
			nocc_internal ("guppy_namemap_typeaction_primtype(): unknown node tag [%s]", (*nodep)->tag->name);
			return 0;
		}

		cmd->target_indir = saved_indir;
		*nodep = newseq;

		return 0;
	}
	return -1;
}
/*}}}*/

/*{{{  static int guppy_getdescriptor_chantype (langops_t *lops, tnode_t *node, char **sptr)*/
/*
 *	gets the descriptor for a channel-type
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_getdescriptor_chantype (langops_t *lops, tnode_t *node, char **sptr)
{
	chantypehook_t *cth = (chantypehook_t *)tnode_nthhookof (node, 0);
	char *lstr = NULL;
	char *ststr = NULL;

	langops_getdescriptor (tnode_nthsubof (node, 0), &ststr);			/* subtype string */
	lstr = string_fmt ("chan%s(%s)", (cth->marked_cli ? "!" : (cth->marked_svr ? "?" : "")), ststr);

	if (*sptr) {
		char *tmpstr = string_fmt ("%s%s", *sptr, lstr);

		sfree (*sptr);
		sfree (lstr);
		*sptr = tmpstr;
	} else {
		*sptr = lstr;
	}

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

	lstr = string_dup ("Channel");
	if (*str) {
		sfree (*str);
	}
	*str = lstr;

	return 0;
}
/*}}}*/
/*{{{  static tnode_t *guppy_gettype_chantype (langops_t *lops, tnode_t *node, tnode_t *default_type)*/
/*
 *	returns the type of a channel (id function)
 */
static tnode_t *guppy_gettype_chantype (langops_t *lops, tnode_t *node, tnode_t *default_type)
{
	return node;
}
/*}}}*/
/*{{{  static tnode_t *guppy_getsubtype_chantype (langops_t *lops, tnode_t *node, tnode_t *default_type)*/
/*
 *	returns the sub-type of a channel (channel protocol)
 */
static tnode_t *guppy_getsubtype_chantype (langops_t *lops, tnode_t *node, tnode_t *default_type)
{
	tnode_t *type = tnode_nthsubof (node, 0);

	if (!type) {
		nocc_internal ("guppy_getsubtype_chantype(): no subtype?");
		return NULL;
	}
	return type;
}
/*}}}*/
/*{{{  static tnode_t *guppy_typeactual_chantype (langops_t *lops, tnode_t *formaltype, tnode_t *actualtype, tnode_t *node, typecheck_t *tc)*/
/*
 *	does type compatibility on a channel, returns actual type used
 */
static tnode_t *guppy_typeactual_chantype (langops_t *lops, tnode_t *formaltype, tnode_t *actualtype, tnode_t *node, typecheck_t *tc)
{
	tnode_t *atype = NULL;

#if 0
fprintf (stderr, "guppy_typeactual_chantype(): formaltype=[%s], actualtype=[%s]\n", formaltype->tag->name, actualtype->tag->name);
#endif

	if (!actualtype) {
		return NULL;
	}
	if (formaltype->tag == gup.tag_CHAN) {
		/*{{{  actual type-check for channel*/
		if ((node->tag == gup.tag_INPUT) || (node->tag == gup.tag_OUTPUT)) {
			/* becomes a protocol-check */
			atype = tnode_nthsubof (formaltype, 0);

			atype = typecheck_typeactual (atype, actualtype, node, tc);
		} else {
			/* must be two channels then */
			if (actualtype->tag != gup.tag_CHAN) {
				typecheck_error (node, tc, "expected channel, found [%s]", actualtype->tag->name);
			}
			atype = actualtype;

			if (!typecheck_typeactual (tnode_nthsubof (formaltype, 0), tnode_nthsubof (actualtype, 0), node, tc)) {
				return NULL;
			}
		}
		/*}}}*/
	} else {
		nocc_fatal ("guppy_typeactual_chantype(): don\'t know how to handle a non-channel here (yet), got [%s]", formaltype->tag->name);
	}
	return atype;
}
/*}}}*/
/*{{{  static int guppy_guesstlp_chantype (langops_t *lops, tnode_t *node)*/
/*
 *	guesses the top-level usage of a channel
 *	returns 1=input, 2=output, 0=unknown
 */
static int guppy_guesstlp_chantype (langops_t *lops, tnode_t *node)
{
	chantypehook_t *cth = (chantypehook_t *)tnode_nthhookof (node, 0);

	if (cth && cth->marked_svr) {
		return 1;
	} else if (cth && cth->marked_cli) {
		return 2;
	}
	return 0;
}
/*}}}*/
/*{{{  static int guppy_typehash_chantype (langops_t *lops, tnode_t *t, int hsize, void *ptr)*/
/*
 *	gets the type-hash for a channel type
 *	returns 0 on success, non-zero on failure
 */
static int guppy_typehash_chantype (langops_t *lops, tnode_t *t, int hsize, void *ptr)
{
	chantypehook_t *cth = (chantypehook_t *)tnode_nthhookof (t, 0);
	unsigned int myhash;

	langops_typehash (tnode_nthsubof (t, 0), sizeof (myhash), (void *)&myhash);
	myhash = ((myhash << 3) ^ (myhash >> 28)) ^ 0x50030001;

	langops_typehash_blend (hsize, ptr, sizeof (myhash), (void *)&myhash);
	return 0;
}
/*}}}*/
/*{{{  static int guppy_isdefpointer_chantype (langops_t *lops, tnode_t *node)*/
/*
 *	returns default indirection level for channel type (trivial)
 */
static int guppy_isdefpointer_chantype (langops_t *lops, tnode_t *node)
{
	/* FIXME: if moving channel-ends around, this needs to be true */
	return 0;
}
/*}}}*/
/*{{{  static tnode_t *guppy_initcall_chantype (langops_t *lops, tnode_t *typenode, tnode_t *name)*/
/*
 *	generates initialiser for channel types
 */
static tnode_t *guppy_initcall_chantype (langops_t *lops, tnode_t *typenode, tnode_t *name)
{
	if (typenode->tag == gup.tag_CHAN) {
		tnode_t *inode;

		inode = tnode_create (gup.tag_CHANINIT, SLOCI, NULL, typenode, name);
		return inode;
	}
	return NULL;
}
/*}}}*/
/*{{{  static int guppy_setinout_chantype (langops_t *lops, tnode_t *node, int marked_in, int marked_out)*/
/*
 *	sets the input+output specifiers for a channel-type node
 *	returns 0 on success, non-zero on failure
 */
static int guppy_setinout_chantype (langops_t *lops, tnode_t *node, int marked_in, int marked_out)
{
	chantypehook_t *cth = (chantypehook_t *)tnode_nthhookof (node, 0);

	cth->marked_svr = marked_in;
	cth->marked_cli = marked_out;

	return 0;
}
/*}}}*/
/*{{{  static int guppy_getinout_chantype (langops_t *lops, tnode_t *node, int *marked_in, int *marked_out)*/
/*
 *	gets the input+output specifiers for a channel-type node
 *	returns 0 on success, non-zero on failure
 */
static int guppy_getinout_chantype (langops_t *lops, tnode_t *node, int *marked_in, int *marked_out)
{
	chantypehook_t *cth = (chantypehook_t *)tnode_nthhookof (node, 0);

	if (marked_in) {
		*marked_in = cth->marked_svr;
	}
	if (marked_out) {
		*marked_out = cth->marked_cli;
	}

	return 0;
}
/*}}}*/

/*{{{  static int guppy_prescope_arraytype (compops_t *cops, tnode_t **nodep, prescope_t *ps)*/
/*
 *	does pre-scoping on an ARRAY type node -- makes sure dimension-tree is a list.
 *	returns 0 to stop walk, 1 to continue.
 */
static int guppy_prescope_arraytype (compops_t *cops, tnode_t **nodep, prescope_t *ps)
{
	tnode_t **dimp = tnode_nthsubaddr (*nodep, 0);
	arraytypehook_t *ath = (arraytypehook_t *)tnode_nthhookof (*nodep, 0);

	if (!*dimp) {
		/* outright NULL means no dimensions specified at all, assume unsized 1 dimensional */
		*dimp = parser_newlistnode (OrgOf (*nodep));
		parser_addtolist (*dimp, NULL);
		guppy_setarraytypedims_undef (ath, 1);
	} else {
		int ndims;

		parser_ensurelist (dimp, *nodep);
		ndims = parser_countlist (*dimp);
		guppy_setarraytypedims_undef (ath, ndims);
	}
	return 1;
}
/*}}}*/
/*{{{  static int guppy_typecheck_arraytype (compops_t *cops, tnode_t *node, typecheck_t *tc)*/
/*
 *	does type-checking on an ARRAY type node.
 *	returns 0 to stop walk, 1 to continue.
 */
static int guppy_typecheck_arraytype (compops_t *cops, tnode_t *node, typecheck_t *tc)
{
	tnode_t *subtype;

	return 1;
}
/*}}}*/
/*{{{  static int guppy_constprop_arraytype (compops_t *cops, tnode_t **nodep)*/
/*
 *	does constant-propagation on an ARRAY type node.
 *	returns 0 to stop walk, 1 to continue.
 */
static int guppy_constprop_arraytype (compops_t *cops, tnode_t **nodep)
{
	return 1;
}
/*}}}*/
/*{{{  static tnode_t *guppy_gettype_arraytype (langops_t *lops, tnode_t *node, tnode_t *default_type)*/
/*
 *	gets the type of an ARRAY node (already set by typecheck)
 */
static tnode_t *guppy_gettype_arraytype (langops_t *lops, tnode_t *node, tnode_t *default_type)
{
	tnode_t *type = tnode_nthsubof (node, 0);

	return type;
}
/*}}}*/

/*{{{  static int guppy_getdescriptor_anytype (langops_t *lops, tnode_t *node, char **sptr)*/
/*
 *	generates a descriptor string for the any-type (trivial)
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_getdescriptor_anytype (langops_t *lops, tnode_t *node, char **sptr)
{
	char *lstr = NULL;

	lstr = string_dup ("*");

	if (*sptr) {
		char *tmpstr = string_fmt ("%s%s", *sptr, lstr);

		sfree (*sptr);
		sfree (lstr);
		*sptr = tmpstr;
	} else {
		*sptr = lstr;
	}
	return 0;
}
/*}}}*/
/*{{{  static tnode_t *guppy_gettype_anytype (langops_t *lops, tnode_t *node, tnode_t *default_type)*/
/*
 *	returns the type of the any-type (id function).
 */
static tnode_t *guppy_gettype_anytype (langops_t *lops, tnode_t *node, tnode_t *default_type)
{
#if 0
fprintf (stderr, "guppy_gettype_anytype(): default_type is 0x%8.8x\n", (int)default_type);
#endif
	return node;
}
/*}}}*/
/*{{{  static tnode_t *guppy_typeactual_anytype (langops_t *lops, tnode_t *formaltype, tnode_t *actualtype, tnode_t *node, typecheck_t *tc)*/
/*
 *	does type-compatibility check on the any-type, returns actual type used
 */
static tnode_t *guppy_typeactual_anytype (langops_t *lops, tnode_t *formaltype, tnode_t *actualtype, tnode_t *node, typecheck_t *tc)
{
	tnode_t *atype = NULL;

	/* special case: anything is permissable on an ANY formal */
	atype = actualtype;
#if 1
fprintf (stderr, "guppy_typeactual_anytype(): formaltype=[%s], actualtype=[%s]\n", formaltype->tag->name, actualtype->tag->name);
#endif

	return atype;
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

	/*{{{  register reduction functions and new langops*/

	fcnlib_addfcn ("guppy_reduce_primtype", guppy_reduce_primtype, 0, 3);
	fcnlib_addfcn ("guppy_reduce_chantype", guppy_reduce_chantype, 0, 3);
	fcnlib_addfcn ("guppy_reduce_arraytype", guppy_reduce_arraytype, 0, 3);

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
	tnode_setlangop (lops, "getdescriptor", 2, LANGOPTYPE (guppy_getdescriptor_primtype));
	tnode_setlangop (lops, "getctypeof", 2, LANGOPTYPE (guppy_getctypeof_primtype));
	tnode_setlangop (lops, "gettype", 2, LANGOPTYPE (guppy_gettype_primtype));
	tnode_setlangop (lops, "typeactual", 4, LANGOPTYPE (guppy_typeactual_primtype));
	tnode_setlangop (lops, "knownsizeof", 1, LANGOPTYPE (guppy_knownsizeof_primtype));
	tnode_setlangop (lops, "typehash", 3, LANGOPTYPE (guppy_typehash_primtype));
	tnode_setlangop (lops, "isdefpointer", 1, LANGOPTYPE (guppy_isdefpointer_primtype));
	tnode_setlangop (lops, "initcall", 2, LANGOPTYPE (guppy_initcall_primtype));
	tnode_setlangop (lops, "freecall", 2, LANGOPTYPE (guppy_freecall_primtype));
	tnode_setlangop (lops, "namemap_typeaction", 3, LANGOPTYPE (guppy_namemap_typeaction_primtype));
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
	tnd = tnode_newnodetype ("guppy:chantype", &i, 1, 0, 1, TNF_NONE);		/* subnodes: 0 = type; hooks: 0 = chantypehook_t */
	tnd->hook_free = guppy_chantype_hook_free;
	tnd->hook_copy = guppy_chantype_hook_copy;
	tnd->hook_dumptree = guppy_chantype_hook_dumptree;
	cops = tnode_newcompops ();
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "getdescriptor", 2, LANGOPTYPE (guppy_getdescriptor_chantype));
	tnode_setlangop (lops, "getctypeof", 2, LANGOPTYPE (guppy_getctypeof_chantype));
	tnode_setlangop (lops, "gettype", 2, LANGOPTYPE (guppy_gettype_chantype));
	tnode_setlangop (lops, "getsubtype", 2, LANGOPTYPE (guppy_getsubtype_chantype));
	tnode_setlangop (lops, "typeactual", 4, LANGOPTYPE (guppy_typeactual_chantype));
	tnode_setlangop (lops, "guesstlp", 1, LANGOPTYPE (guppy_guesstlp_chantype));
	tnode_setlangop (lops, "typehash", 3, LANGOPTYPE (guppy_typehash_chantype));
	tnode_setlangop (lops, "isdefpointer", 1, LANGOPTYPE (guppy_isdefpointer_chantype));
	tnode_setlangop (lops, "initcall", 2, LANGOPTYPE (guppy_initcall_chantype));
	tnode_setlangop (lops, "chantype_setinout", 3, LANGOPTYPE (guppy_setinout_chantype));
	tnode_setlangop (lops, "chantype_getinout", 3, LANGOPTYPE (guppy_getinout_chantype));
	tnd->lops = lops;

	i = -1;
	gup.tag_CHAN = tnode_newnodetag ("CHAN", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  guppy:arraytype -- ARRAY*/
	i = -1;
	tnd = tnode_newnodetype ("guppy:arraytype", &i, 2, 0, 1, TNF_NONE);		/* subnodes: 0 = dimension-tree (at declaration); 1 = sub-type; hooks: 0 = arraytypehook_t */
	tnd->hook_free = guppy_arraytype_hook_free;
	tnd->hook_copy = guppy_arraytype_hook_copy;
	tnd->hook_dumptree = guppy_arraytype_hook_dumptree;
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "prescope", 2, COMPOPTYPE (guppy_prescope_arraytype));
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (guppy_typecheck_arraytype));
	tnode_setcompop (cops, "constprop", 1, COMPOPTYPE (guppy_constprop_arraytype));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "gettype", 2, LANGOPTYPE (guppy_gettype_arraytype));
	tnd->lops = lops;

	i = -1;
	gup.tag_ARRAY = tnode_newnodetag ("ARRAY", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  guppy:fcntype -- FCNTYPE*/
	i = -1;
	tnd = tnode_newnodetype ("guppy:fcntype", &i, 2, 0, 0, TNF_NONE);		/* subnodes: 0 = params; 1 = results */
	cops = tnode_newcompops ();
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnd->lops = lops;

	i = -1;
	gup.tag_FCNTYPE = tnode_newnodetag ("FCNTYPE", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  guppy:valtype -- VALTYPE*/
	/* Note: this only exists before scope */
	i = -1;
	tnd = tnode_newnodetype ("guppy:valtype", &i, 1, 0, 0, TNF_NONE);		/* subnodes: 0 = type */
	cops = tnode_newcompops ();
	tnd->ops = cops;

	i = -1;
	gup.tag_VALTYPE = tnode_newnodetag ("VALTYPE", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  guppy:anytype -- ANY*/
	i = -1;
	tnd = tnode_newnodetype ("guppy:anytype", &i, 0, 0, 0, TNF_NONE);
	cops = tnode_newcompops ();
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "getdescriptor", 2, LANGOPTYPE (guppy_getdescriptor_anytype));
	tnode_setlangop (lops, "gettype", 2, LANGOPTYPE (guppy_gettype_anytype));
	tnode_setlangop (lops, "typeactual", 4, LANGOPTYPE (guppy_typeactual_anytype));
	tnd->lops = lops;

	i = -1;
	gup.tag_ANY = tnode_newnodetag ("ANY", &i, tnd, NTF_NONE);

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

