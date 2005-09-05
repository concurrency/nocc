/*
 *	occampi_type.c -- occam-pi type handling for nocc
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
#include "dfa.h"
#include "parsepriv.h"
#include "occampi.h"
#include "feunit.h"
#include "names.h"
#include "scope.h"
#include "prescope.h"
#include "typecheck.h"
#include "langops.h"
/*}}}*/



/*{{{  static void occampi_typeattr_dumpchook (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	dumps a typeattr compiler hook
 */
static void occampi_typeattr_dumpchook (tnode_t *node, void *hook, int indent, FILE *stream)
{
	occampi_typeattr_t attr = (occampi_typeattr_t)hook;
	char buf[256];
	int x = 0;

	occampi_isetindent (stream, indent);
	if (attr & TYPEATTR_MARKED_IN) {
		x += sprintf (buf + x, "marked-in ");
	}
	if (attr & TYPEATTR_MARKED_OUT) {
		x += sprintf (buf + x, "marked-out ");
	}
	if (x) {
		buf[x-1] = '\0';
	}
	fprintf (stream, "<chook id=\"occampi:typeattr\" flags=\"%s\" />\n", buf);

	return;
}
/*}}}*/
/*{{{  static tnode_t *occampi_type_gettype (tnode_t *node, tnode_t *default_type)*/
/*
 *	returns the type of a type-node (typically the sub-type)
 */
static tnode_t *occampi_type_gettype (tnode_t *node, tnode_t *default_type)
{
	tnode_t *type;

	type = tnode_nthsubof (node, 0);
	if (!type) {
		nocc_internal ("occampi_type_gettype(): no subtype ?");
		return NULL;
	}
	return type;
}
/*}}}*/
/*{{{  static tnode_t *occampi_type_typeactual (tnode_t *formaltype, tnode_t *actualtype, tnode_t *node, typecheck_t *tc)*/
/*
 *	does type compatibility on a type-node, returns the actual type used by the operation
 */
static tnode_t *occampi_type_typeactual (tnode_t *formaltype, tnode_t *actualtype, tnode_t *node, typecheck_t *tc)
{
	tnode_t *atype;

	if ((formaltype->tag == opi.tag_CHAN) && ((node->tag == opi.tag_INPUT) || (node->tag == opi.tag_OUTPUT))) {
		/* becomes a protocol-check in effect */
		atype = tnode_nthsubof (formaltype, 0);

#if 0
fprintf (stderr, "occampi_type_typeactual(): channel: node->tag = [%s]\n", node->tag->name);
#endif
		atype = typecheck_typeactual (atype, actualtype, node, tc);
	} else if (formaltype->tag == opi.tag_CHAN) {
		/* must be two channels then */
		if (actualtype->tag != opi.tag_CHAN) {
			typecheck_error (node, tc, "expected channel, found [%s]", actualtype->tag->name);
		}
		atype = actualtype;

		typecheck_typeactual (tnode_nthsubof (formaltype, 0), tnode_nthsubof (actualtype, 0), node, tc);
	} else {
		nocc_fatal ("occampi_type_typeactual(): don't know how to handle a non-channel here (yet)");
		atype = NULL;
	}

	return atype;
}
/*}}}*/
/*{{{  static int occampi_type_bytesfor (tnode_t *t)*/
/*
 *	returns the number of bytes required by this type (or -1 if not known)
 */
static int occampi_type_bytesfor (tnode_t *t)
{
	return -1;
}
/*}}}*/
/*{{{  static int occampi_type_getdescriptor (tnode_t *node, char **str)*/
/*
 *	gets descriptor information for a type
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_type_getdescriptor (tnode_t *node, char **str)
{
	if (node->tag == opi.tag_CHAN) {
		if (*str) {
			char *newstr = (char *)smalloc (strlen (*str) + 6);

			sprintf (newstr, "%sCHAN ", *str);
			sfree (*str);
			*str = newstr;
		} else {
			*str = (char *)smalloc (8);
			sprintf (*str, "CHAN ");
		}
	} else if ((node->tag == opi.tag_ASINPUT) || (node->tag == opi.tag_ASOUTPUT)) {
		langops_getdescriptor (tnode_nthsubof (node, 0), str);

		if (*str) {
			char *newstr = (char *)smalloc (strlen (*str) + 3);

			sprintf (newstr, "%s%c", *str, (node->tag == opi.tag_ASINPUT) ? '?' : '!');
			sfree (*str);
			*str = newstr;
		} else {
			*str = (char *)smalloc (8);
			sprintf (*str, "%s ", (node->tag == opi.tag_ASINPUT) ? "ASINPUT" : "ASOUTPUT");
		}
		return 0;
	}
	return 1;
}
/*}}}*/


/*{{{  static int occampi_leaftype_bytesfor (tnode_t *t)*/
/*
 *	returns the number of bytes required by a basic type
 */
static int occampi_leaftype_bytesfor (tnode_t *t)
{
	if (t->tag == opi.tag_BYTE) {
		return 1;
	} else if (t->tag == opi.tag_INT) {
		return 4;
	} else if (t->tag == opi.tag_INT16) {
		return 2;
	} else if (t->tag == opi.tag_INT32) {
		return 4;
	} else if (t->tag == opi.tag_INT64) {
		return 8;
	} else if (t->tag == opi.tag_REAL32) {
		return 4;
	} else if (t->tag == opi.tag_REAL64) {
		return 8;
	} else if (t->tag == opi.tag_CHAR) {
		return 1;
	}
	return -1;
}
/*}}}*/
/*{{{  static int occampi_leaftype_getdescriptor (tnode_t *node, char **str)*/
/*
 *	gets descriptor information for a leaf-type
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_leaftype_getdescriptor (tnode_t *node, char **str)
{
	char *sptr;

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
	if (node->tag == opi.tag_BYTE) {
		sprintf (sptr, "BYTE");
	} else if (node->tag == opi.tag_INT) {
		sprintf (sptr, "INT");
	} else if (node->tag == opi.tag_INT16) {
		sprintf (sptr, "INT16");
	} else if (node->tag == opi.tag_INT32) {
		sprintf (sptr, "INT32");
	} else if (node->tag == opi.tag_INT64) {
		sprintf (sptr, "INT64");
	} else if (node->tag == opi.tag_REAL32) {
		sprintf (sptr, "REAL32");
	} else if (node->tag == opi.tag_REAL64) {
		sprintf (sptr, "REAL64");
	} else if (node->tag == opi.tag_CHAR) {
		sprintf (sptr, "CHAR");
	}

	return 0;
}
/*}}}*/


/*{{{  static void occampi_reduce_primtype (dfastate_t *dfast, parsepriv_t *pp, void *rarg)*/
/*
 *	reduces a primitive type
 */
static void occampi_reduce_primtype (dfastate_t *dfast, parsepriv_t *pp, void *rarg)
{
	token_t *tok;
	ntdef_t *tag;

	tok = parser_gettok (pp);
	tag = tnode_lookupnodetag (tok->u.kw->name);
	*(dfast->ptr) = tnode_create (tag, tok->origin);
	lexer_freetoken (tok);

	return;
}
/*}}}*/


/*{{{  static int occampi_type_init_nodes (void)*/
/*
 *	initialises type nodes for occam-pi
 *	return 0 on success, non-zero on error
 */
static int occampi_type_init_nodes (void)
{
	int i;
	tndef_t *tnd;
	compops_t *cops;
	langops_t *lops;

	/*{{{  occampi:typenode -- CHAN, ASINPUT, ASOUTPUT*/
	i = -1;
	tnd = opi.node_TYPENODE = tnode_newnodetype ("occampi:typenode", &i, 1, 0, 0, TNF_NONE);
	cops = tnode_newcompops ();
	cops->gettype = occampi_type_gettype;
	cops->typeactual = occampi_type_typeactual;
	cops->bytesfor = occampi_type_bytesfor;
	tnd->ops = cops;
	lops = tnode_newlangops ();
	lops->getdescriptor = occampi_type_getdescriptor;
	tnd->lops = lops;

	i = -1;
	opi.tag_CHAN = tnode_newnodetag ("CHAN", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_ASINPUT = tnode_newnodetag ("ASINPUT", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_ASOUTPUT = tnode_newnodetag ("ASOUTPUT", &i, tnd, NTF_NONE);
	/*}}}*/
	/*{{{  occampi:leaftype -- INT, BYTE, INT16, INT32, INT64, REAL32, REAL64, CHAR*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:leaftype", &i, 0, 0, 0, TNF_NONE);
	cops = tnode_newcompops ();
	cops->bytesfor = occampi_leaftype_bytesfor;
	tnd->ops = cops;
	lops = tnode_newlangops ();
	lops->getdescriptor = occampi_leaftype_getdescriptor;
	tnd->lops = lops;

	i = -1;
	opi.tag_INT = tnode_newnodetag ("INT", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_BYTE = tnode_newnodetag ("BYTE", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_INT16 = tnode_newnodetag ("INT16", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_INT32 = tnode_newnodetag ("INT32", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_INT64 = tnode_newnodetag ("INT64", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_REAL32 = tnode_newnodetag ("REAL32", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_REAL64 = tnode_newnodetag ("REAL64", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_CHAR = tnode_newnodetag ("CHAR", &i, tnd, NTF_NONE);
	/*}}}*/
	/*{{{  input/output tokens*/
	opi.tok_INPUT = lexer_newtoken (SYMBOL, "?");
	opi.tok_OUTPUT = lexer_newtoken (SYMBOL, "!");
	/*}}}*/
	/*{{{  attributes compiler hook*/
	opi.chook_typeattr = tnode_newchook ("occampi:typeattr");
	opi.chook_typeattr->chook_dumptree = occampi_typeattr_dumpchook;

	/*}}}*/

	return 0;
}
/*}}}*/
/*{{{  static int occampi_type_reg_reducers (void)*/
/*
 *	registers reducers for occam-pi types
 *	returns 0 on success, non-zero on error
 */
static int occampi_type_reg_reducers (void)
{
	parser_register_reduce ("Roccampi:primtype", occampi_reduce_primtype, NULL);
	parser_register_grule ("opi:chanpush", parser_decode_grule ("N+Sn0C1N-", opi.tag_CHAN));

	return 0;
}
/*}}}*/
/*{{{  static dfattbl_t **occampi_type_init_dfatrans (int *ntrans)*/
/*
 *	creates and returns DFA transition tables for occam-pi type nodes
 */
static dfattbl_t **occampi_type_init_dfatrans (int *ntrans)
{
	DYNARRAY (dfattbl_t *, transtbl);

	dynarray_init (transtbl);
	dynarray_add (transtbl, dfa_bnftotbl ("occampi:primtype ::= ( +@INT | +@BYTE ) {Roccampi:primtype}"));
	dynarray_add (transtbl, dfa_bnftotbl ("occampi:protocol ::= ( occampi:primtype | +Name {<opi:namepush>} ) {<opi:nullreduce>}"));

	*ntrans = DA_CUR (transtbl);
	return DA_PTR (transtbl);
}
/*}}}*/


/*{{{  occampi_type_feunit (feunit_t struct)*/
feunit_t occampi_type_feunit = {
	init_nodes: occampi_type_init_nodes,
	reg_reducers: occampi_type_reg_reducers,
	init_dfatrans: occampi_type_init_dfatrans,
	post_setup: NULL
};
/*}}}*/


