/*
 *	guppy_lit.c -- literals for Guppy
 *	Copyright (C) 2011-2013 Fred Barnes <frmb@kent.ac.uk>
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


/*{{{  static guppy_litdata_t *guppy_newlitdata (void)*/
/*
 *	creates a new guppy_litdata_t structure
 */
static guppy_litdata_t *guppy_newlitdata (void)
{
	guppy_litdata_t *ldat = (guppy_litdata_t *)smalloc (sizeof (guppy_litdata_t));

	ldat->data = NULL;
	ldat->bytes = 0;
	ldat->littype = NOTOKEN;

	return ldat;
}
/*}}}*/
/*{{{  static void guppy_freelitdata (guppy_litdata_t *ldat)*/
/*
 *	frees a guppy_litdata_t structure
 */
static void guppy_freelitdata (guppy_litdata_t *ldat)
{
	if (!ldat) {
		nocc_internal ("guppy_freelitdata(): NULL pointer!");
		return;
	}
	if (ldat->data) {
		sfree (ldat->data);
		ldat->data = NULL;
		ldat->bytes = 0;
	}
	sfree (ldat);
	return;
}
/*}}}*/


/*{{{  void *guppy_token_to_lithook (void *ntok)*/
/*
 *	used to turn a token into a literal hook (int/real/char/string)
 */
void *guppy_token_to_lithook (void *ntok)
{
	token_t *tok = (token_t *)ntok;
	guppy_litdata_t *ldat = guppy_newlitdata ();

	if (tok->type == INTEGER) {
		ldat->data = smalloc (sizeof (int));
		ldat->bytes = sizeof (int);
		ldat->littype = INTEGER;

		memcpy (ldat->data, &(tok->u.ival), sizeof (int));
	} else if (tok->type == REAL) {
		ldat->data = smalloc (sizeof (double));
		ldat->bytes = sizeof (double);
		ldat->littype = REAL;

		memcpy (ldat->data, &(tok->u.dval), sizeof (double));
	} else if (tok->type == STRING) {
		ldat->data = smalloc (tok->u.str.len + 1);
		ldat->bytes = tok->u.str.len;
		ldat->littype = STRING;

		memcpy (ldat->data, tok->u.str.ptr, tok->u.str.len);
		((char *)ldat->data)[ldat->bytes] = '\0';
	} else if ((tok->type == KEYWORD) && lexer_tokmatchlitstr (tok, "true")) {
		ldat->data = smalloc (sizeof (int));
		ldat->bytes = sizeof (int);
		ldat->littype = INTEGER;

		*(int *)(ldat->data) = 1;
	} else if ((tok->type == KEYWORD) && lexer_tokmatchlitstr (tok, "false")) {
		ldat->data = smalloc (sizeof (int));
		ldat->bytes = sizeof (int);
		ldat->littype = INTEGER;

		*(int *)(ldat->data) = 0;
	} else {
		nocc_serious ("guppy_token_to_lithook(): unsupported token type! %d", (int)tok->type);
		guppy_freelitdata (ldat);
		ldat = NULL;
	}

	return (void *)ldat;
}
/*}}}*/
/*{{{  static char *guppy_esclitstring (guppy_litdata_t *ldat)*/
/*
 *	reproduces a literal string, C-escape style
 */
static char *guppy_esclitstring (guppy_litdata_t *ldat)
{
	char *str;
	unsigned char *xstr;
	int slen, i;

	if (ldat->littype != STRING) {
		nocc_internal ("guppy_esclitstring(): not STRING! (%d)", (int)ldat->littype);
		return NULL;
	}

	slen = 0;
	xstr = (unsigned char *)ldat->data;

	/* FIXME: assuming ASCII strings for the moment.. */
	for (i=0; i<ldat->bytes; i++) {
		if (xstr[i] == '\\') {
			/* special case */
			slen += 2;
		} else if ((xstr[i] < 32) || (xstr[i] > 126)) {
			slen += 4;
		} else {
			slen++;
		}
	}
	str = (char *)smalloc (slen + 2);
	slen = 0;
	for (i=0; i<ldat->bytes; i++) {
		if (xstr[i] == '\\') {
			str[slen++] = '\\';
			str[slen++] = '\\';
		} else if ((xstr[i] < 32) || (xstr[i] > 126)) {
			unsigned int hi = (xstr[i] >> 4) & 0x0f;
			unsigned int lo = xstr[i] & 0x0f;

			str[slen++] = '\\';
			str[slen++] = 'x';
			str[slen++] = (hi < 10) ? ('0' + hi) : ('a' + (hi - 10));
			str[slen++] = (lo < 10) ? ('0' + lo) : ('a' + (lo - 10));
		} else {
			str[slen++] = xstr[i];
		}
	}
	str[slen] = '\0';

	return str;
}
/*}}}*/


/*{{{  tnode_t *guppy_makeintlit (tnode_t *type, tnode_t *org, const int value)*/
/*
 *	creates a new integer literal (as a node)
 */
tnode_t *guppy_makeintlit (tnode_t *type, tnode_t *org, const int value)
{
	guppy_litdata_t *ldat = guppy_newlitdata ();
	tnode_t *lnode;

	ldat->data = smalloc (sizeof (int));
	ldat->bytes = sizeof (int);
	memcpy (ldat->data, &value, sizeof (int));
	ldat->littype = INTEGER;

	if (!org) {
		lnode = tnode_create (gup.tag_LITINT, NULL, type, ldat);
	} else {
		lnode = tnode_createfrom (gup.tag_LITINT, org, type, ldat);
	}

	return lnode;
}
/*}}}*/
/*{{{  tnode_t *guppy_makereallit (tnode_t *type, tnode_t *org, const double value)*/
/*
 *	creates a new real literal (as a node)
 */
tnode_t *guppy_makereallit (tnode_t *type, tnode_t *org, const double value)
{
	guppy_litdata_t *ldat = guppy_newlitdata ();
	tnode_t *lnode;

	ldat->data = smalloc (sizeof (double));
	ldat->bytes = sizeof (double);
	memcpy (ldat->data, &value, sizeof (double));
	ldat->littype = REAL;

	lnode = tnode_createfrom (gup.tag_LITINT, org, type, ldat);

	return lnode;
}
/*}}}*/
/*{{{  tnode_t *guppy_makestringlit (tnode_t *type, tnode_t *org, const char *value)*/
/*
 *	creates a new string literal (as a node)
 */
tnode_t *guppy_makestringlit (tnode_t *type, tnode_t *org, const char *value)
{
	guppy_litdata_t *ldat = guppy_newlitdata ();
	tnode_t *lnode;

	ldat->data = (void *)string_dup (value);
	ldat->bytes = strlen (value);
	ldat->littype = STRING;

	lnode = tnode_createfrom (gup.tag_LITSTRING, org, type, ldat);

	return lnode;
}
/*}}}*/


/*{{{  static void guppy_litnode_hook_free (void *hook)*/
/*
 *	frees a litnode hook
 */
static void guppy_litnode_hook_free (void *hook)
{
	guppy_litdata_t *ldat = (guppy_litdata_t *)hook;

	if (!ldat) {
		return;
	}
	guppy_freelitdata (ldat);
	return;
}
/*}}}*/
/*{{{  static void *guppy_litnode_hook_copy (void *hook)*/
/*
 *	copies a litnode hook
 */
static void *guppy_litnode_hook_copy (void *hook)
{
	guppy_litdata_t *ldat = (guppy_litdata_t *)hook;
	guppy_litdata_t *lcpy = NULL;

	if (!ldat) {
		return NULL;
	}
	lcpy = (guppy_litdata_t *)smalloc (sizeof (guppy_litdata_t));
	switch (ldat->littype) {
	case INTEGER:
		lcpy->data = smalloc (sizeof (int));
		lcpy->bytes = sizeof (int);
		lcpy->littype = INTEGER;
		memcpy (lcpy->data, ldat->data, sizeof (int));
		break;
	case REAL:
		lcpy->data = smalloc (sizeof (double));
		lcpy->bytes = sizeof (double);
		lcpy->littype = REAL;
		memcpy (lcpy->data, ldat->data, sizeof (double));
		break;
	case STRING:
		lcpy->data = smalloc (ldat->bytes + 1);
		lcpy->bytes = ldat->bytes;
		lcpy->littype = STRING;
		memcpy (lcpy->data, ldat->data, ldat->bytes + 1);
		break;
	default:
		nocc_serious ("guppy_litnode_hook_copy(): unsupported literal type! %d", ldat->littype);
		break;
	}

	return (void *)lcpy;
}
/*}}}*/
/*{{{  static void guppy_litnode_hook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)*/
/*
 *	dump-tree for a litnode hook
 */
static void guppy_litnode_hook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)
{
	guppy_litdata_t *ldat = (guppy_litdata_t *)hook;
	char *hdat;

	guppy_isetindent (stream, indent);
	fhandle_printf (stream, "<litnodehook type=\"");
	switch (ldat->littype) {
	case INTEGER:
		fhandle_printf (stream, "integer");
		break;
	case REAL:
		fhandle_printf (stream, "real");
		break;
	case STRING:
		fhandle_printf (stream, "string");
		break;
	default:
		fhandle_printf (stream, "<unknown %d>", ldat->littype);
		break;
	}
	fhandle_printf (stream, "\" bytes=\"%d\" data=\"", ldat->bytes);
	hdat = mkhexbuf ((unsigned char *)ldat->data, ldat->bytes);
	fhandle_printf (stream, "%s\" />\n", hdat);
	sfree (hdat);

	return;
}
/*}}}*/


/*{{{  static tnode_t *guppy_gettype_litnode (langops_t *lops, tnode_t *node, tnode_t *default_type)*/
/*
 *	gets the type of a literal node
 *	if the type is not set, default_type is used to guess it.
 */
static tnode_t *guppy_gettype_litnode (langops_t *lops, tnode_t *node, tnode_t *default_type)
{
	tnode_t *type = tnode_nthsubof (node, 0);

	if (!type && !default_type) {
		/* no type, so return NULL for now */
		return NULL;
	} else if (!type) {
		if (node->tag == gup.tag_LITBOOL) {
			/* ignore default type */
			type = guppy_newprimtype (gup.tag_BOOL, NULL, 1);
			tnode_setnthsub (node, 0, type);
		} else {
			/* no type yet, use default_type */
			guppy_litdata_t *ldat = (guppy_litdata_t *)tnode_nthhookof (node, 0);
			int typesize = tnode_bytesfor (default_type, NULL);
			int issigned = tnode_issigned (default_type, NULL);

#if 1
fprintf (stderr, "guppy_gettype_litnode(): ldat->bytes=%d, issigned=%d, typesize=%d\n", ldat->bytes, issigned, typesize);
#endif
			if ((node->tag == gup.tag_LITINT) && (typesize < ldat->bytes)) {
			}

			type = tnode_copytree (default_type);
			tnode_setnthsub (node, 0, type);
		}
	}
	return type;
}
/*}}}*/
/*{{{  static int guppy_isconst_litnode (langops_t *lops, tnode_t *node)*/
/*
 *	returns non-zero if the node is constant (returns width)
 */
static int guppy_isconst_litnode (langops_t *lops, tnode_t *node)
{
	guppy_litdata_t *ldat = (guppy_litdata_t *)tnode_nthhookof (node, 0);

	return ldat->bytes;
}
/*}}}*/
/*{{{  static int guppy_constvalof_litnode (langops_t *lops, tnode_t *node, void *ptr)*/
/*
 *	gets the constant value of a literal node (assigns to pointer if non-null)
 */
static int guppy_constvalof_litnode (langops_t *lops, tnode_t *node, void *ptr)
{
	guppy_litdata_t *ldat = (guppy_litdata_t *)tnode_nthhookof (node, 0);
	int r = 0;

	if ((node->tag == gup.tag_LITBOOL) || (node->tag == gup.tag_LITCHAR) || (node->tag == gup.tag_LITINT)) {
		switch (ldat->bytes) {
		case 1:
			if (ptr) {
				*(unsigned char *)ptr = *(unsigned char *)(ldat->data);
			}
			r = (int)(*(unsigned char *)(ldat->data));
			break;
		case 2:
			if (ptr) {
				*(unsigned short int *)ptr = *(unsigned short int *)(ldat->data);
			}
			r = (int)(*(unsigned short int *)(ldat->data));
			break;
		case 4:
			if (ptr) {
				*(unsigned int *)ptr = *(unsigned int *)(ldat->data);
			}
			r = (int)(*(unsigned int *)(ldat->data));
			break;
		case 8:
			if (ptr) {
				*(unsigned long long *)ptr = *(unsigned long long *)(ldat->data);
			}
			r = (int)(*(unsigned long long *)(ldat->data));
			break;
		default:
			tnode_error (node, "guppy_constvalof_litnode(): unsupported constant integer width %d!", ldat->bytes);
			break;
		}
	} else if (node->tag == gup.tag_LITREAL) {
		switch (ldat->bytes) {
		case 4:
			if (ptr) {
				*(float *)ptr = *(float *)(ldat->bytes);
			}
			r = (int)(*(float *)(ldat->bytes));
			break;
		case 8:
			if (ptr) {
				*(double *)ptr = *(double *)(ldat->bytes);
			}
			r = (int)(*(double *)(ldat->bytes));
			break;
		default:
			tnode_error (node, "guppy_constvalof_lit(): unsupported constant floating-point width %d!", ldat->bytes);
			break;
		}
	}

	return r;
}
/*}}}*/
/*{{{  static int guppy_getctypeof_litnode (langops_t *lops, tnode_t *node, char **sptr)*/
/*
 *	gets the C type of a literal node -- will produce strings verbatim, but appropriately (re-)escaped
 *	returns 0 on success, non-zero on error
 */
static int guppy_getctypeof_litnode (langops_t *lops, tnode_t *node, char **sptr)
{
	guppy_litdata_t *ldat = (guppy_litdata_t *)tnode_nthhookof (node, 0);

	if (node->tag == gup.tag_LITSTRING) {
		int i, len;
		char *ch;

		if (*sptr) {
			sfree (*sptr);
		}
		for (i=0, len=0; i<ldat->bytes; i++) {
			unsigned char sch = ((unsigned char *)(ldat->data))[i];

			if ((sch < 32) || (sch >= 128)) {
				len += 4;
			} else {
				len++;
			}
		}

		*sptr = (char *)smalloc (len + 1);
		ch = *sptr;
		for (i=0, len=0; i<ldat->bytes; i++) {
			char sch = ((char *)(ldat->data))[i];

			if (sch < 32) {
				int hi = (((unsigned char)sch) >> 4) & 0x0f;
				int lo = sch & 0x0f;

				ch[len++] = '\\';
				ch[len++] = 'x';
				ch[len++] = (hi < 10) ? '0' + hi : 'a' + (hi - 10);
				ch[len++] = (lo < 10) ? '0' + lo : 'a' + (lo - 10);
			} else {
				ch[len++] = sch;
			}
		}
		ch[len] = '\0';

		return 0;
	}
	return -1;
}
/*}}}*/

/*{{{  static int guppy_typecheck_litnode (compops_t *cops, tnode_t *node, typecheck_t *tc)*/
/*
 *	does type-checking on a literal node;  for certain things, populates the type
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_typecheck_litnode (compops_t *cops, tnode_t *node, typecheck_t *tc)
{
	if (node->tag == gup.tag_LITSTRING) {
		tnode_t **tptr = tnode_nthsubaddr (node, 0);
		guppy_litdata_t *ldat = (guppy_litdata_t *)tnode_nthhookof (node, 0);

		if (*tptr) {
			return 0;		/* already got it, oddly.. */
		} else if (!ldat) {
			nocc_internal ("guppy_typecheck_litnode(): no litdata_t attached to LITSTRING..");
			return 0;
		}
		*tptr = guppy_newprimtype (gup.tag_STRING, node, ldat->bytes);
		return 0;
	}
	return 1;
}
/*}}}*/
/*{{{  static int guppy_fetrans1_litnode (compops_t *cops, tnode_t **nodep, guppy_fetrans1_t *fe1)*/
/*
 *	does front-end transforms (1) on a literal node;  pulls string constants out into temporaries
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_fetrans1_litnode (compops_t *cops, tnode_t **nodep, guppy_fetrans1_t *fe1)
{
	if ((*nodep)->tag == gup.tag_LITSTRING) {
		guppy_litdata_t *ldat = (guppy_litdata_t *)tnode_nthhookof (*nodep, 0);
		tnode_t *type = tnode_nthsubof (*nodep, 0);
		tnode_t *constinit = tnode_create (gup.tag_CONSTSTRINGINIT, OrgOf (*nodep), NULL, ldat);
		tnode_t *tname = guppy_fetrans1_maketemp (gup.tag_NDECL, *nodep, type, constinit, fe1);

		/* substitute for new variable */
		*nodep = tname;

		return 0;
	}
	return 1;
}
/*}}}*/
/*{{{  static int guppy_codegen_litnode (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	does code-generation for a literal node
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_codegen_litnode (compops_t *cops, tnode_t *node, codegen_t *cgen)
{
	guppy_litdata_t *ldat = (guppy_litdata_t *)tnode_nthhookof (node, 0);

	switch (ldat->littype) {
	case INTEGER:
		switch (ldat->bytes) {
		case 4:
			codegen_write_fmt (cgen, "%d", *(int *)(ldat->data));
			break;
		case 2:
			codegen_write_fmt (cgen, "%d", *(short int *)(ldat->data));
			break;
		default:
			nocc_internal ("guppy_codegen_litnode(): unhandled INTEGER size %d!", ldat->bytes);
			break;
		}
		break;
	case REAL:
		switch (ldat->bytes) {
		case 4:
			codegen_write_fmt (cgen, "%f", *(float *)(ldat->data));
			break;
		case 8:
			codegen_write_fmt (cgen, "%lf", *(double *)(ldat->data));
			break;
		default:
			nocc_internal ("guppy_codegen_litnode(): unhandled REAL size %d!", ldat->bytes);
			break;
		}
		break;
	case STRING:
		{
			char *ostr = (char *)ldat->data;
			int i, len;
			char *tstr;
			
			for (i=0, len=0; ostr[i] != '\0'; i++) {
				if (ostr[i] < 32) {
					len += 4;	/* '\x42' */
				} else if (ostr[i] >= 128) {
					len += 4;	/* '\xef' */
				} else {
					len++;
				}
			}

			tstr = (char *)smalloc (len + 1);
			for (i=0, len=0; ostr[i] != '\0'; i++) {
				if ((ostr[i] < 32) || (ostr[i] >= 128)) {
					int hi = (ostr[i] >> 4) & 0x0f;
					int lo = ostr[i] & 0x0f;

					tstr[len++] = '\\';
					tstr[len++] = 'x';
					tstr[len++] = (hi < 10) ? '0' + hi : 'a' + (hi - 10);
					tstr[len++] = (lo < 10) ? '0' + lo : 'a' + (lo - 10);
				} else {
					tstr[len++] = ostr[i];
				}
			}

			codegen_write_fmt (cgen, "\"%*s\"", ldat->bytes, tstr);
			sfree (tstr);
		}
		break;
	}
	return 0;
}
/*}}}*/

/*{{{  static int guppy_namemap_litinit (compops_t *cops, tnode_t **nodep, map_t *map)*/
/*
 *	does name-mapping for a literal initialiser
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_namemap_litinit (compops_t *cops, tnode_t **nodep, map_t *map)
{
	cccsp_mapdata_t *cmd = (cccsp_mapdata_t *)map->hook;

	tnode_setnthsub (*nodep, 0, cmd->process_id);

	cmd->target_indir = 0;
	map_submapnames (tnode_nthsubaddr (*nodep, 0), map);

	return 0;
}
/*}}}*/
/*{{{  static int guppy_codegen_litinit (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	does code-generation for a literal initialiser
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_codegen_litinit (compops_t *cops, tnode_t *node, codegen_t *cgen)
{
	guppy_litdata_t *ldat = (guppy_litdata_t *)tnode_nthhookof (node, 0);

	if (node->tag == gup.tag_CONSTSTRINGINIT) {
		char *estr = guppy_esclitstring (ldat);

		codegen_write_fmt (cgen, "GuppyStringConstInitialiser (");
		codegen_subcodegen (tnode_nthsubof (node, 0), cgen);		/* Wptr */
		codegen_write_fmt (cgen, ", \"%s\", %d)", estr, ldat->bytes);
		sfree (estr);

		return 0;
	}
	return 1;
}
/*}}}*/
/*{{{  static int guppy_cccspdcg_litinit (compops_t *cops, tnode_t *node, cccsp_dcg_t *dcg)*/
/*
 *	does direct-call-graph handling for a literal initialiser
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_cccspdcg_litinit (compops_t *cops, tnode_t *node, cccsp_dcg_t *dcg)
{
#if 0
fhandle_printf (FHAN_STDERR, "guppy_cccspdcg_litinit(): here!\n");
#endif
	if (node->tag == gup.tag_CONSTSTRINGINIT) {
		/* this calls one of the API things.. */
		cccsp_sfi_entry_t *sfient = cccsp_sfi_lookupornew ("GuppyStringConstInitialiser");

		if (dcg->thisfcn) {
			cccsp_sfi_addchild (dcg->thisfcn, sfient);
		}
		return 0;
	}
	return 1;
}
/*}}}*/

/*{{{  static int guppy_lit_init_nodes (void)*/
/*
 *	sets up literal nodes for Guppy
 *	returns 0 on success, non-zero on failure
 */
static int guppy_lit_init_nodes (void)
{
	tndef_t *tnd;
	compops_t *cops;
	langops_t *lops;
	int i;

	fcnlib_addfcn ("guppy_token_to_lithook", (void *)guppy_token_to_lithook, 1, 1);

	/*{{{  guppy:litnode -- LITINT, LITREAL, LITCHAR, LITSTRING, LITBOOL*/
	i = -1;
	tnd = tnode_newnodetype ("guppy:litnode", &i, 1, 0, 1, TNF_NONE);		/* subnodes: actual-type; hooks: litdata_t */
	tnd->hook_free = guppy_litnode_hook_free;
	tnd->hook_copy = guppy_litnode_hook_copy;
	tnd->hook_dumptree = guppy_litnode_hook_dumptree;
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (guppy_typecheck_litnode));
	tnode_setcompop (cops, "fetrans1", 2, COMPOPTYPE (guppy_fetrans1_litnode));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (guppy_codegen_litnode));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "gettype", 2, LANGOPTYPE (guppy_gettype_litnode));
	tnode_setlangop (lops, "isconst", 1, LANGOPTYPE (guppy_isconst_litnode));
	tnode_setlangop (lops, "constvalof", 2, LANGOPTYPE (guppy_constvalof_litnode));
	tnode_setlangop (lops, "getctypeof", 2, LANGOPTYPE (guppy_getctypeof_litnode));
	tnd->lops = lops;

	i = -1;
	gup.tag_LITINT = tnode_newnodetag ("LITINT", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_LITBOOL = tnode_newnodetag ("LITBOOL", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_LITREAL = tnode_newnodetag ("LITREAL", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_LITCHAR = tnode_newnodetag ("LITCHAR", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_LITSTRING = tnode_newnodetag ("LITSTRING", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  guppy:litinit -- CONSTSTRINGINIT*/
	i = -1;
	tnd = tnode_newnodetype ("guppy:litinit", &i, 1, 0, 1, TNF_NONE);		/* subnodes: wptr; hooks: litdata_t */
	tnd->hook_free = guppy_litnode_hook_free;
	tnd->hook_copy = guppy_litnode_hook_copy;
	tnd->hook_dumptree = guppy_litnode_hook_dumptree;
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (guppy_namemap_litinit));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (guppy_codegen_litinit));
	tnode_setcompop (cops, "cccsp:dcg", 2, COMPOPTYPE (guppy_cccspdcg_litinit));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnd->lops = lops;

	i = -1;
	gup.tag_CONSTSTRINGINIT = tnode_newnodetag ("CONSTSTRINGINIT", &i, tnd, NTF_NONE);

	/*}}}*/

	return 0;
}
/*}}}*/
/*{{{  static int guppy_lit_post_setup (void)*/
/*
 *	does post-setup for literal nodes
 *	returns 0 on success, non-zero on failure
 */
static int guppy_lit_post_setup (void)
{
	return 0;
}
/*}}}*/


/*{{{  guppy_lit_feunit (feunit_t)*/
feunit_t guppy_lit_feunit = {
	.init_nodes = guppy_lit_init_nodes,
	.reg_reducers = NULL,
	.init_dfatrans = NULL,
	.post_setup = guppy_lit_post_setup,
	.ident = "guppy-lit"
};

/*}}}*/

