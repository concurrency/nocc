/*
 *	occampi_lit.c -- occam-pi literal processing for nocc
 *	Copyright (C) 2005-2007 Fred Barnes <frmb@kent.ac.uk>
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
#include "origin.h"
#include "symbols.h"
#include "keywords.h"
#include "lexer.h"
#include "lexpriv.h"
#include "tnode.h"
#include "constprop.h"
#include "parser.h"
#include "fcnlib.h"
#include "dfa.h"
#include "parsepriv.h"
#include "occampi.h"
#include "feunit.h"
#include "names.h"
#include "scope.h"
#include "prescope.h"
#include "typecheck.h"
#include "fetrans.h"
#include "map.h"
#include "codegen.h"
#include "target.h"
#include "langops.h"


/*}}}*/


/*{{{  static void occampi_litnode_hook_free (void *hook)*/
/*
 *	frees a litnode hook (value-bytes)
 */
static void occampi_litnode_hook_free (void *hook)
{
	occampi_litdata_t *ldata = (occampi_litdata_t *)hook;

	if (ldata) {
		sfree (ldata);
	}
	return;
}
/*}}}*/
/*{{{  static void *occampi_litnode_hook_copy (void *hook)*/
/*
 *	copies a litnode hook (name-bytes)
 */
static void *occampi_litnode_hook_copy (void *hook)
{
	occampi_litdata_t *ldata = (occampi_litdata_t *)hook;

	if (ldata) {
		occampi_litdata_t *tmplit = (occampi_litdata_t *)smalloc (sizeof (occampi_litdata_t));

		tmplit->bytes = ldata->bytes;
		tmplit->data = mem_ndup (ldata->data, ldata->bytes);

		return tmplit;
	}
	return NULL;
}
/*}}}*/
/*{{{  static void occampi_litnode_hook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	dump-tree for litnode hook (name-bytes)
 */
static void occampi_litnode_hook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	occampi_litdata_t *ldata = (occampi_litdata_t *)hook;
	char *hdata;

	if (ldata) {
		hdata = mkhexbuf ((unsigned char *)(ldata->data), ldata->bytes);
	} else {
		hdata = string_dup ("");
	}

	occampi_isetindent (stream, indent);
	fprintf (stream, "<litnode bytes=\"%d\" data=\"%s\" />\n", ldata ? ldata->bytes : 0, hdata);

	sfree (hdata);
	return;
}
/*}}}*/


/*{{{  static tnode_t *occampi_gettype_lit (langops_t *lops, tnode_t *node, tnode_t *default_type)*/
/*
 *	returns the type of a literal.
 *	If the type is not set, the default_type is used to guess it
 */
static tnode_t *occampi_gettype_lit (langops_t *lops, tnode_t *node, tnode_t *default_type)
{
	tnode_t *type = tnode_nthsubof (node, 0);

#if 0
fprintf (stderr, "occampi_gettype_lit(): we are [%s], default type 0x%8.8x, seen type is 0x%8.8x\n",
		node->tag->name, (unsigned int)default_type, (unsigned int)type);
if (default_type) {
	tnode_dumptree (default_type, 1, stderr);
}
#endif
	if (node->tag == opi.tag_LITARRAY) {
		/*{{{  literal array handling*/
		occampi_litdata_t *tmplit = (occampi_litdata_t *)tnode_nthhookof (node, 0);
		tnode_t **dimp;

		if (!type || (type->tag != opi.tag_ARRAY)) {
			nocc_error ("occampi_gettype_lit(): LITARRAY not ARRAY type.. was [%s]", type ? type->tag->name : "(null)");
			return NULL;
		}
		dimp = tnode_nthsubaddr (type, 0);
		if (!*dimp) {
			/* put in dimension.  FIXME: will only do single-level arrays.. */
			tnode_t *subtype = tnode_nthsubof (type, 1);
			int typesize = tnode_bytesfor (subtype, NULL);
			int dimcount;
			
			if (typesize <= 0) {
				nocc_error ("occampi_gettype_lit(): ARRAY sub-type [%s] unsizeable..", subtype->tag->name);
				return NULL;
			}
#if 0
fprintf (stderr, "occampi_gettype_lit(): LITARRAY: mysize is %d, typesize of [%s] is %d, tmplit->bytes=%d\n", tmplit->bytes, subtype->tag->name, typesize, tmplit->bytes);
#endif
			dimcount = tmplit->bytes / typesize;

			*dimp = tnode_create (opi.tag_LITINT, NULL, tnode_create (opi.tag_INT, NULL), occampi_integertoken_to_hook (lexer_newtoken (INTEGER, dimcount)));
		}
		/*}}}*/
	} else if (!type && !default_type) {
		/*{{{  no type and no default type*/
		if (node->tag == opi.tag_LITBOOL) {
			/* easy :) */
			tnode_setnthsub (node, 0, tnode_create (opi.tag_BOOL, NULL));
			type = tnode_nthsubof (node, 0);
		} else {
			/* XXX: not an error? -- may run into this when going through arrays of numbers */
			/* nocc_fatal ("occampi_gettype_lit(): literal not typed, and no default_type!"); */
			return NULL;
		}
		/*}}}*/
	} else if (!type) {
		if (node->tag == opi.tag_LITBOOL) {
			/*{{{  bools*/
			/* ignore default type */
			tnode_setnthsub (node, 0, tnode_create (opi.tag_BOOL, NULL));
			type = tnode_nthsubof (node, 0);
			/*}}}*/
		} else if (node->tag == opi.tag_LITREAL) {
			/* ignore default type */
			occampi_litdata_t *tmplit = (occampi_litdata_t *)tnode_nthhookof (node, 0);
			int typesize = tnode_bytesfor (default_type, NULL);

			/* thing on the literal is either 4 (float) or 8 (double) bytes */
			if ((tmplit->bytes == 4) && (typesize == 4)) {
				/* definitely a REAL32 or equivalent */
				type = tnode_copytree (default_type);
				tnode_setnthsub (node, 0, type);
			} else if ((tmplit->bytes == 8) && (typesize == 4)) {
				/* REAL64 -> REAL32 fixed conversion */
				double dval = *(double *)(tmplit->data);
				float fval = (float)dval;

				sfree (tmplit->data);
				tmplit->bytes = 4;
				tmplit->data = smalloc (tmplit->bytes);
				*(float *)(tmplit->data) = fval;

				type = tnode_copytree (default_type);
				tnode_setnthsub (node, 0, type);
			} else if ((tmplit->bytes == 4) && (typesize == 8)) {
				/* REAL32 -> REAL64 fixed conversion */
				float fval = *(float *)(tmplit->data);
				double dval = (double)fval;

				sfree (tmplit->data);
				tmplit->bytes = 8;
				tmplit->data = smalloc (tmplit->bytes);
				*(double *)(tmplit->data) = dval;

				type = tnode_copytree (default_type);
				tnode_setnthsub (node, 0, type);
			} else if ((tmplit->bytes == 8) && (typesize == 8)) {
				/* definately a REAL64 or equivalent */
				type = tnode_copytree (default_type);
				tnode_setnthsub (node, 0, type);
			} else {
				nocc_internal ("occampi_gettype_lit(): LITREAL error, bytes=%d, typesize=%d", tmplit->bytes, typesize);
			}

#if 0
fprintf (stderr, "occampi_gettype_lit(): LITREAL: typesize=%d, litbytes=%d, default_type=\n", typesize, tmplit->bytes);
tnode_dumptree (default_type, 4, stderr);
#endif
		} else {
			/* no type yet, use default_type */
			occampi_litdata_t *tmplit = (occampi_litdata_t *)tnode_nthhookof (node, 0);
			int typesize = tnode_bytesfor (default_type, NULL);

#if 0
fprintf (stderr, "occampi_gettype_lit(): typesize=%d, default_type=\n", typesize);
tnode_dumptree (default_type, 4, stderr);
#endif
			if ((node->tag == opi.tag_LITINT) && (typesize < tmplit->bytes)) {
				/*{{{  make sure literal fits*/
				int issigned = tnode_issigned (default_type, NULL);

				if (issigned && (tmplit->bytes == 4)) {
					int sint = *(int *)tmplit->data;

					if ((typesize == 1) && ((sint < -128) || (sint > 127))) {
						tnode_error (node, "literal value exceeds range of 1-byte type");
					} else if ((typesize == 2) && ((sint < -32768) || (sint > 32767))) {
						tnode_error (node, "literal value exceeds range of 2-byte type");
					}
				} else if (!issigned && (tmplit->bytes == 4)) {
					unsigned int uint = *(unsigned int *)tmplit->data;

					if ((typesize == 1) && (uint > 255)) {
						tnode_error (node, "literal value exceeds range of 1-byte type");
					} else if ((typesize == 2) && (uint > 65535)) {
						tnode_error (node, "literal value exceeds range of 2-byte type");
					}
				} else if (issigned && (tmplit->bytes == 8)) {
					long long lsint = *(long long *)tmplit->data;

					if ((typesize == 1) && ((lsint < -128LL) || (lsint > 127LL))) {
						tnode_error (node, "literal value exceeds range of 1-byte type");
					} else if ((typesize == 2) && ((lsint < -32768LL) || (lsint > 32767LL))) {
						tnode_error (node, "literal value exceeds range of 2-byte type");
					} else if ((typesize == 4) && ((lsint < -2147483648LL) || (lsint > 2147483647LL))) {
						tnode_error (node, "literal value exceeds range of 4-byte type");
					}
				} else if (!issigned && (tmplit->bytes == 8)) {
					unsigned long long luint = *(unsigned long long *)tmplit->data;

					if ((typesize == 1) && (luint > 255LL)) {
						tnode_error (node, "literal value exceeds range of 1-byte type");
					} else if ((typesize == 2) && (luint > 65535LL)) {
						tnode_error (node, "literal value exceeds range of 2-byte type");
					} else if ((typesize == 4) && (luint > 4294967295LL)) {
						tnode_error (node, "literal value exceeds range of 4-byte type");
					}
				}
				/*}}}*/
				/*{{{  truncate literal*/
#if 0
fprintf (stderr, "occampi_gettype_lit(): adjusting literal size from %d to %d..\n", tmplit->bytes, typesize);
#endif
				tmplit->bytes = typesize;

				/*}}}*/
			}

			type = tnode_copytree (default_type);
			tnode_setnthsub (node, 0, type);
		}
	}
	return type;
}
/*}}}*/
/*{{{  static int occampi_bytesfor_lit (langops_t *lops, tnode_t *node, target_t *target)*/
/*
 *	returns the number of bytes required for this literal, target given if available
 */
static int occampi_bytesfor_lit (langops_t *lops, tnode_t *node, target_t *target)
{
	occampi_litdata_t *tmplit = (occampi_litdata_t *)tnode_nthhookof (node, 0);

	return tmplit->bytes;
}
/*}}}*/
/*{{{  static int occampi_constprop_lit (compops_t *cops, tnode_t **nodep)*/
/*
 *	does constant propagation on a literal node (post walk)
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_constprop_lit (compops_t *cops, tnode_t **nodep)
{
	occampi_litdata_t *tmplit = (occampi_litdata_t *)tnode_nthhookof ((*nodep), 0);

	if (((*nodep)->tag == opi.tag_LITINT) && (tmplit->bytes == 1)) {
		/* BYTE constant expressed as INT */
		*nodep = constprop_newconst (CONST_BYTE, *nodep, NULL, *(unsigned char *)(tmplit->data));
	} else if ((*nodep)->tag == opi.tag_LITBYTE) {
		*nodep = constprop_newconst (CONST_BYTE, *nodep, NULL, *(unsigned char *)(tmplit->data));
	} else if ((*nodep)->tag == opi.tag_LITINT) {
		*nodep = constprop_newconst (CONST_INT, *nodep, NULL, *(int *)(tmplit->data));
	} else if ((*nodep)->tag == opi.tag_LITBOOL) {
		*nodep = constprop_newconst (CONST_BOOL, *nodep, NULL, (int)*(unsigned char *)(tmplit->data));
	}

	return 0;
}
/*}}}*/
/*{{{  static int occampi_namemap_lit (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	name-maps a literal
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_namemap_lit (compops_t *cops, tnode_t **node, map_t *map)
{
	occampi_litdata_t *ldata = (occampi_litdata_t *)tnode_nthhookof (*node, 0);
	tnode_t *ltype = tnode_nthsubof (*node, 0);
	typecat_e ltypecat = typecheck_typetype (ltype);
	tnode_t *cnst;

	cnst = map->target->newconst (*node, map, ldata->data, ldata->bytes, ltypecat);

#if 0
	if ((*node)->tag == opi.tag_LITARRAY) {
		/* map type, may contain constants that we need later on */
		tnode_t *type = ltype;

		while (type->tag == opi.tag_ARRAY) {
			/* map dimension size */
			map_submapnames (tnode_nthsubaddr (type, 0), map);
			type = tnode_nthsubof (type, 1);
		}
#if 0
fprintf (stderr, "occampi_namemap_lit(): ltype is:\n");
tnode_dumptree (ltype, 1, stderr);
#endif
	}
#endif

	*node = cnst;

	return 0;
}
/*}}}*/
/*{{{  static int occampi_precode_lit (compops_t *cops, tnode_t **nodep, codegen_t *cgen)*/
/*
 *	pre-codes a literal
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_precode_lit (compops_t *cops, tnode_t **nodep, codegen_t *cgen)
{
	tnode_t *ltype = tnode_nthsubof (*nodep, 0);

#if 0
	if ((*nodep)->tag == opi.tag_LITARRAY) {
		/* pre-code type, may contain constants that we need later on */
		tnode_t *type = ltype;

		while (type->tag == opi.tag_ARRAY) {
			/* pre-code dimension size */
			codegen_subprecode (tnode_nthsubaddr (type, 0), cgen);
			type = tnode_nthsubof (type, 1);
		}
	}
#endif

	return 1;
}
/*}}}*/
/*{{{  static int occampi_isconst_lit (langops_t *lops, tnode_t *node)*/
/*
 *	returns non-zero if the node is a constant (returns width)
 */
static int occampi_isconst_lit (langops_t *lops, tnode_t *node)
{
	occampi_litdata_t *ldata = (occampi_litdata_t *)tnode_nthhookof (node, 0);

	return ldata->bytes;
}
/*}}}*/
/*{{{  static int occampi_constvalof_lit (langops_t *lops, tnode_t *node, void *ptr)*/
/*
 *	returns constant value of a literal node (assigns to pointer if non-null)
 */
static int occampi_constvalof_lit (langops_t *lops, tnode_t *node, void *ptr)
{
	occampi_litdata_t *ldata = (occampi_litdata_t *)tnode_nthhookof (node, 0);
	int r = 0;

	if ((node->tag == opi.tag_LITBOOL) || (node->tag == opi.tag_LITCHAR) || (node->tag == opi.tag_LITBYTE) || (node->tag == opi.tag_LITINT)) {
		switch (ldata->bytes) {
		case 1:
			if (ptr) {
				*(unsigned char *)ptr = *(unsigned char *)(ldata->data);
			}
			r = (int)(*(unsigned char *)(ldata->data));
			break;
		case 2:
			if (ptr) {
				*(unsigned short int *)ptr = *(unsigned short int *)(ldata->data);
			}
			r = (int)(*(unsigned short int *)(ldata->data));
			break;
		case 4:
			if (ptr) {
				*(unsigned int *)ptr = *(unsigned int *)(ldata->data);
			}
			r = (int)(*(unsigned int *)(ldata->data));
			break;
		case 8:
			if (ptr) {
				*(unsigned long long *)ptr = *(unsigned long long *)(ldata->data);
			}
			r = (int)(*(unsigned long long *)(ldata->data));
			break;
		default:
			tnode_error (node, "occampi_constvalof_lit(): unsupported constant integer width %d!", ldata->bytes);
			break;
		}
	} else if (node->tag == opi.tag_LITREAL) {
		switch (ldata->bytes) {
		case 4:
			if (ptr) {
				*(float *)ptr = *(float *)(ldata->data);
			}
			r = (int)(*(float *)(ldata->data));
			break;
		case 8:
			if (ptr) {
				*(double *)ptr = *(double *)(ldata->data);
			}
			r = (int)(*(double *)(ldata->data));
			break;
		default:
			tnode_error (node, "occampi_constvalof_lit(): unsupported constant floating-point width %d!", ldata->bytes);
			break;
		}
	}

	return r;
}
/*}}}*/
/*{{{  static int occampi_valbyref_lit (langops_t *lops, tnode_t *node)*/
/*
 *	returns non-zero if VALs of this literal are handled as references (LITARRAY, plain types larger than a word)
 */
static int occampi_valbyref_lit (langops_t *lops, tnode_t *node)
{
	if (node->tag == opi.tag_LITARRAY) {
		return 1;
	} else if ((node->tag == opi.tag_LITINT) || (node->tag == opi.tag_LITREAL)) {
		occampi_litdata_t *tmplit = (occampi_litdata_t *)tnode_nthhookof (node, 0);

		/* FIXME: slot-size? */
		if (tmplit->bytes > 4) {
			return 1;
		}
	}
	return 0;
}
/*}}}*/
/*{{{  static tnode_t *occampi_dimtreeof_lit (langops_t *lops, tnode_t *node)*/
/*
 *	returns the dimension tree associated with a literal (LITARRAY)
 */
static tnode_t *occampi_dimtreeof_lit (langops_t *lops, tnode_t *node)
{
	if (node->tag == opi.tag_LITARRAY) {
		tnode_t *type = tnode_nthsubof (node, 0);

#if 0
fprintf (stderr, "occampi_dimtreeof_lit(): LITARRAY type is:\n");
tnode_dumptree (type, 1, stderr);
#endif
		return langops_dimtreeof (type);
	}
	return NULL;
}
/*}}}*/


/*{{{  static void *occampi_bool_hook (int val)*/
/*
 *	creates a constant boolean literal hook
 */
static void *occampi_bool_hook (int val)
{
	occampi_litdata_t *ldata = (occampi_litdata_t *)smalloc (sizeof (occampi_litdata_t));

	ldata->bytes = 1;
	ldata->data = smalloc (1);
	*(unsigned char *)(ldata->data) = val ? 0x01 : 0x00;

	return (void *)ldata;
}
/*}}}*/
/*{{{  static void *occampi_booltrue_hook (void *zero)*/
/*
 *	creates a constant TRUE literal hook
 */
static void *occampi_booltrue_hook (void *zero)
{
	if (zero) {
		nocc_internal ("occampi_booltrue_hook(): got a non-null value!");
	}

	return occampi_bool_hook (1);
}
/*}}}*/
/*{{{  static void *occampi_boolfalse_hook (void *zero)*/
/*
 *	creates a constant FALSE literal hook
 */
static void *occampi_boolfalse_hook (void *zero)
{
	if (zero) {
		nocc_internal ("occampi_boolfalse_hook(): got a non-null value!");
	}

	return occampi_bool_hook (0);
}
/*}}}*/


/*{{{  static int occampi_lit_init_nodes (void)*/
/*
 *	initialises literal-nodes for occam-pi
 *	return 0 on success, non-zero on failure
 */
static int occampi_lit_init_nodes (void)
{
	tndef_t *tnd;
	int i;
	compops_t *cops;
	langops_t *lops;

	/*{{{  register reduction functions*/
	fcnlib_addfcn ("occampi_booltrue_hook", occampi_booltrue_hook, 1, 1);
	fcnlib_addfcn ("occampi_boolfalse_hook", occampi_boolfalse_hook, 1, 1);

	/*}}}*/
	/*{{{  occampi:litnode -- LITBOOL, LITBYTE, LITCHAR, LITINT, LITREAL, LITARRAY*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:litnode", &i, 1, 0, 1, TNF_NONE);
	tnd->hook_free = occampi_litnode_hook_free;
	tnd->hook_copy = occampi_litnode_hook_copy;
	tnd->hook_dumptree = occampi_litnode_hook_dumptree;
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "constprop", 1, COMPOPTYPE (occampi_constprop_lit));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (occampi_namemap_lit));
	tnode_setcompop (cops, "precode", 2, COMPOPTYPE (occampi_precode_lit));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "gettype", 2, LANGOPTYPE (occampi_gettype_lit));
	tnode_setlangop (lops, "bytesfor", 2, LANGOPTYPE (occampi_bytesfor_lit));
	tnode_setlangop (lops, "isconst", 1, LANGOPTYPE (occampi_isconst_lit));
	tnode_setlangop (lops, "constvalof", 2, LANGOPTYPE (occampi_constvalof_lit));
	tnode_setlangop (lops, "valbyref", 1, LANGOPTYPE (occampi_valbyref_lit));
	tnode_setlangop (lops, "dimtreeof", 1, LANGOPTYPE (occampi_dimtreeof_lit));
	tnd->lops = lops;

	i = -1;
	opi.tag_LITBOOL = tnode_newnodetag ("LITBOOL", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_LITBYTE = tnode_newnodetag ("LITBYTE", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_LITCHAR = tnode_newnodetag ("LITCHAR", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_LITINT = tnode_newnodetag ("LITINT", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_LITREAL = tnode_newnodetag ("LITREAL", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_LITARRAY = tnode_newnodetag ("LITARRAY", &i, tnd, NTF_NONE);
	/*}}}*/

	return 0;
}
/*}}}*/


/*{{{  tnode_t *occampi_makelitbool (lexfile_t *lf, const int istrue)*/
/*
 *	creates a boolean constant node
 */
tnode_t *occampi_makelitbool (lexfile_t *lf, const int istrue)
{
	return tnode_create (opi.tag_LITBOOL, lf, NULL, occampi_bool_hook (istrue ? 1 : 0));
}
/*}}}*/
/*{{{  int occampi_islitstring (struct TAG_tnode *node)*/
/*
 *	returns non-zero if the given node is a string literal (array of BYTEs)
 */
int occampi_islitstring (struct TAG_tnode *node)
{
	if (node->tag == opi.tag_LITARRAY) {
		tnode_t *type = tnode_nthsubof (node, 0);

		if (type && (type->tag == opi.tag_ARRAY)) {
			/* some literal array, string? */
			tnode_t *subtype = tnode_nthsubof (type, 1);

			if (subtype && (subtype->tag == opi.tag_BYTE)) {
				/* yes, literal byte array */
				return 1;
			}
		}
	}
	return 0;
}
/*}}}*/
/*{{{  char *occampi_litstringcopy (struct TAG_tnode *node)*/
/*
 *	extracts the data from a literal string node (LITARRAY -> ARRAY -> BYTE)
 *	returns a fresh string on success, NULL on failure
 */
char *occampi_litstringcopy (struct TAG_tnode *node)
{
	if (occampi_islitstring (node)) {
		occampi_litdata_t *tmplit = (occampi_litdata_t *)tnode_nthhookof (node, 0);

		return string_ndup (tmplit->data, tmplit->bytes);
	}
	return NULL;
}
/*}}}*/

/*{{{  occampi_lit_feunit (feunit_t)*/
feunit_t occampi_lit_feunit = {
	init_nodes: occampi_lit_init_nodes,
	reg_reducers: NULL,
	init_dfatrans: NULL,
	post_setup: NULL,
	ident: "occampi-lit"
};
/*}}}*/

