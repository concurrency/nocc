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
#include "target.h"
#include "map.h"
#include "codegen.h"


/*}}}*/


/*{{{  static void occampi_type_initchandecl (tnode_t *node, codegen_t *cgen, void *arg)*/
/*
 *	does initialiser code-gen for a channel declaration
 */
static void occampi_type_initchandecl (tnode_t *node, codegen_t *cgen, void *arg)
{
	tnode_t *chantype = (tnode_t *)arg;
	int ws_off, vs_off, ms_off, ms_shdw;

	codegen_callops (cgen, debugline, node);

	/* FIXME: assuming single channel for now.. */
	cgen->target->be_getoffsets (node, &ws_off, &vs_off, &ms_off, &ms_shdw);

#if 0
fprintf (stderr, "occampi_initchandecl(): node=[%s], allocated at [%d,%d,%d], type is:\n", node->tag->name, ws_off, vs_off, ms_off);
tnode_dumptree (chantype, 1, stderr);
#endif
	codegen_callops (cgen, loadconst, 0);
	codegen_callops (cgen, storelocal, ws_off);
	codegen_callops (cgen, comment, "initchandecl");

	return;
}
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
/*{{{  static void *occampi_typeattr_copychook (void *hook)*/
/*
 *	copies a type-attribute hook
 */
static void *occampi_typeattr_copychook (void *hook)
{
	return hook;
}
/*}}}*/
/*{{{  static void occampi_typeattr_freechook (void *hook)*/
/*
 *	frees a type-attribute hook
 */
static void occampi_typeattr_freechook (void *hook)
{
	return;
}
/*}}}*/


/*{{{  static int occampi_type_prescope (compops_t *cops, tnode_t **nodep, prescope_t *ps)*/
/*
 *	pre-scopes a type-node;  fixes ASINPUT/ASOUTPUT nodes
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_type_prescope (compops_t *cops, tnode_t **nodep, prescope_t *ps)
{
	if (((*nodep)->tag == opi.tag_ASINPUT) || ((*nodep)->tag == opi.tag_ASOUTPUT)) {
		tnode_t *losing = *nodep;
		occampi_typeattr_t typeattr;

		*nodep = tnode_nthsubof (losing, 0);
		tnode_setnthsub (losing, 0, NULL);

		typeattr = (occampi_typeattr_t)tnode_getchook (*nodep, opi.chook_typeattr);
		typeattr |= (losing->tag == opi.tag_ASINPUT) ? TYPEATTR_MARKED_IN : TYPEATTR_MARKED_OUT;
		tnode_setchook (*nodep, opi.chook_typeattr, (void *)typeattr);

		tnode_free (losing);
	}
	return 1;
}
/*}}}*/
/*{{{  static tnode_t *occampi_type_gettype (langops_t *lops, tnode_t *node, tnode_t *default_type)*/
/*
 *	returns the type of a type-node (typically the sub-type)
 */
static tnode_t *occampi_type_gettype (langops_t *lops, tnode_t *node, tnode_t *default_type)
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
/*{{{  static tnode_t *occampi_type_typeactual (langops_t *lops, tnode_t *formaltype, tnode_t *actualtype, tnode_t *node, typecheck_t *tc)*/
/*
 *	does type compatibility on a type-node, returns the actual type used by the operation
 */
static tnode_t *occampi_type_typeactual (langops_t *lops, tnode_t *formaltype, tnode_t *actualtype, tnode_t *node, typecheck_t *tc)
{
	tnode_t *atype;

#if 0
fprintf (stderr, "occampi_type_typeactual(): formaltype=[%s], actualtype=[%s]\n", formaltype->tag->name, actualtype->tag->name);
#endif
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

		if (!typecheck_typeactual (tnode_nthsubof (formaltype, 0), tnode_nthsubof (actualtype, 0), node, tc)) {
			return NULL;
		}
	} else {
		nocc_fatal ("occampi_type_typeactual(): don't know how to handle a non-channel here (yet)");
		atype = NULL;
	}

	return atype;
}
/*}}}*/
/*{{{  static int occampi_type_bytesfor (langops_t *lops, tnode_t *t, target_t *target)*/
/*
 *	returns the number of bytes required by this type (or -1 if not known)
 */
static int occampi_type_bytesfor (langops_t *lops, tnode_t *t, target_t *target)
{
	if (t->tag == opi.tag_CHAN) {
		return target->chansize;
	}
	return -1;
}
/*}}}*/
/*{{{  static int occampi_type_issigned (langops_t *lops, tnode_t *t, target_t *target)*/
/*
 *	returns the signedness of a type (or -1 if not known)
 */
static int occampi_type_issigned (langops_t *lops, tnode_t *t, target_t *target)
{
	return -1;
}
/*}}}*/
/*{{{  static int occampi_type_getdescriptor (langops_t *lops, tnode_t *node, char **str)*/
/*
 *	gets descriptor information for a type
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_type_getdescriptor (langops_t *lops, tnode_t *node, char **str)
{
	if (node->tag == opi.tag_CHAN) {
		occampi_typeattr_t typeattr = (occampi_typeattr_t)tnode_getchook (node, opi.chook_typeattr);

		if (*str) {
			char *newstr = (char *)smalloc (strlen (*str) + 7);

			sprintf (newstr, "%sCHAN%s ", *str, (typeattr & TYPEATTR_MARKED_IN) ? "?" : ((typeattr & TYPEATTR_MARKED_OUT) ? "!" : ""));
			sfree (*str);
			*str = newstr;
		} else {
			*str = (char *)smalloc (8);
			sprintf (*str, "CHAN%s ", (typeattr & TYPEATTR_MARKED_IN) ? "?" : ((typeattr & TYPEATTR_MARKED_OUT) ? "!" : ""));
		}
	}
	return 1;
}
/*}}}*/
/*{{{  static int occampi_type_initialising_decl (langops_t *lops, tnode_t *t, tnode_t *benode, map_t *mdata)*/
/*
 *	called for declarations to handle initialisation if needed
 *	returns 0 if nothing needed, non-zero otherwise
 */
static int occampi_type_initialising_decl (langops_t *lops, tnode_t *t, tnode_t *benode, map_t *mdata)
{
	if (t->tag == opi.tag_CHAN) {
		codegen_setinithook (benode, occampi_type_initchandecl, (void *)t);
		return 1;
	}
	return 0;
}
/*}}}*/


/*{{{  static tnode_t *occampi_typespec_gettype (langops_t *lops, tnode_t *node, tnode_t *default_type)*/
/*
 *	gets the type of a type-spec node (largely transparent)
 */
static tnode_t *occampi_typespec_gettype (langops_t *lops, tnode_t *node, tnode_t *default_type)
{
	return typecheck_gettype (tnode_nthsubof (node, 0), default_type);
}
/*}}}*/
/*{{{  static tnode_t *occampi_typespec_typeactual (langops_t *lops, tnode_t *formaltype, tnode_t *actualtype, tnode_t *node, typecheck_t *tc)*/
/*
 *	does type compatability on a type-spec node,
 *	returns the actual type used by the operation
 */
static tnode_t *occampi_typespec_typeactual (langops_t *lops, tnode_t *formaltype, tnode_t *actualtype, tnode_t *node, typecheck_t *tc)
{
	return typecheck_typeactual (tnode_nthsubof (formaltype, 0), actualtype, node, tc);
}
/*}}}*/


/*{{{  static tnode_t *occampi_leaftype_gettype (langops_t *lops, tnode_t *t, tnode_t *defaulttype)*/
/*
 *	gets the type for a leaftype -- do nothing really
 */
static tnode_t *occampi_leaftype_gettype (langops_t *lops, tnode_t *t, tnode_t *defaulttype)
{
	return t;
}
/*}}}*/
/*{{{  static int occampi_leaftype_bytesfor (langops_t *lops, tnode_t *t, target_t *target)*/
/*
 *	returns the number of bytes required by a basic type
 */
static int occampi_leaftype_bytesfor (langops_t *lops, tnode_t *t, target_t *target)
{
	if (t->tag == opi.tag_BOOL) {
		return target ? target->intsize : 4;
	} else if (t->tag == opi.tag_BYTE) {
		return 1;
	} else if (t->tag == opi.tag_INT) {
		return target ? target->intsize : 4;
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
		return target ? target->charsize : 1;
	}
	return -1;
}
/*}}}*/
/*{{{  static int occampi_leaftype_issigned (langops_t *lops, tnode_t *t, target_t *target)*/
/*
 *	returns 0 if the given basic type is unsigned
 */
static int occampi_leaftype_issigned (langops_t *lops, tnode_t *t, target_t *target)
{
	if (t->tag == opi.tag_BYTE) {
		return 0;
	} else if (t->tag == opi.tag_BOOL) {
		return 0;
	}
	/* everything else is signed */
	return 1;
}
/*}}}*/
/*{{{  static int occampi_leaftype_getdescriptor (langops_t *lops, tnode_t *node, char **str)*/
/*
 *	gets descriptor information for a leaf-type
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_leaftype_getdescriptor (langops_t *lops, tnode_t *node, char **str)
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
	if (node->tag == opi.tag_BOOL) {
		sprintf (sptr, "BOOL");
	} else if (node->tag == opi.tag_BYTE) {
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


/*{{{  static void occampi_protocol_dfaeh_stuck (dfanode_t *dfanode, token_t *tok)*/
/*
 *	called by parser when it gets stuck in an occampi:protocol DFA node
 */
static void occampi_protocol_dfaeh_stuck (dfanode_t *dfanode, token_t *tok)
{
	char *msg;

	msg = dfa_expectedmatchstr (dfanode, tok, "in protocol specification");
	parser_error (tok->origin, msg);

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
	tnode_setcompop (cops, "prescope", 2, COMPOPTYPE (occampi_type_prescope));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "getdescriptor", 2, LANGOPTYPE (occampi_type_getdescriptor));
	tnode_setlangop (lops, "gettype", 2, LANGOPTYPE (occampi_type_gettype));
	tnode_setlangop (lops, "typeactual", 4, LANGOPTYPE (occampi_type_typeactual));
	tnode_setlangop (lops, "bytesfor", 2, LANGOPTYPE (occampi_type_bytesfor));
	tnode_setlangop (lops, "issigned", 2, LANGOPTYPE (occampi_type_issigned));
	tnode_setlangop (lops, "initialising_decl", 3, LANGOPTYPE (occampi_type_initialising_decl));
	tnd->lops = lops;

	i = -1;
	opi.tag_CHAN = tnode_newnodetag ("CHAN", &i, tnd, NTF_SYNCTYPE);
	i = -1;
	opi.tag_ASINPUT = tnode_newnodetag ("ASINPUT", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_ASOUTPUT = tnode_newnodetag ("ASOUTPUT", &i, tnd, NTF_NONE);
	/*}}}*/
	/*{{{  occampi:typespecnode -- TYPESPEC*/
	/* these appear during scoping */
	i = -1;
	tnd = tnode_newnodetype ("occampi:typespecnode", &i, 1, 0, 0, TNF_TRANSPARENT);
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "gettype", 2, LANGOPTYPE (occampi_typespec_gettype));
	tnode_setlangop (lops, "typeactual", 4, LANGOPTYPE (occampi_typespec_typeactual));
	tnd->lops = lops;

	i = -1;
	opi.tag_TYPESPEC = tnode_newnodetag ("TYPESPEC", &i, tnd, NTF_NONE);
	/*}}}*/
	/*{{{  occampi:leaftype -- INT, BYTE, INT16, INT32, INT64, REAL32, REAL64, CHAR*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:leaftype", &i, 0, 0, 0, TNF_NONE);
	cops = tnode_newcompops ();
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "getdescriptor", 2, LANGOPTYPE (occampi_leaftype_getdescriptor));
	tnode_setlangop (lops, "gettype", 2, LANGOPTYPE (occampi_leaftype_gettype));
	tnode_setlangop (lops, "bytesfor", 2, LANGOPTYPE (occampi_leaftype_bytesfor));
	tnode_setlangop (lops, "issigned", 2, LANGOPTYPE (occampi_leaftype_issigned));
	tnd->lops = lops;

	i = -1;
	opi.tag_INT = tnode_newnodetag ("INT", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_BYTE = tnode_newnodetag ("BYTE", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_BOOL = tnode_newnodetag ("BOOL", &i, tnd, NTF_NONE);
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
	opi.chook_typeattr->chook_copy = occampi_typeattr_copychook;
	opi.chook_typeattr->chook_free = occampi_typeattr_freechook;

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
	dynarray_add (transtbl, dfa_bnftotbl ("occampi:primtype ::= ( +@INT | +@BYTE | +@BOOL | +@INT16 | +@INT32 | +@INT64 | +@REAL32 | +@REAL64 | +@CHAR ) {Roccampi:primtype}"));
	dynarray_add (transtbl, dfa_transtotbl ("occampi:protocol ::= [ 0 occampi:primtype 3 ] [ 0 +Name 1 ] [ 0 -@@[ 2 ] [ 1 {<opi:namepush>} -* 3 ] " \
				"[ 2 occampi:arraytypetype 3 ] " \
				"[ 3 {<opi:nullreduce>} -* ]"));
	dynarray_add (transtbl, dfa_transtotbl ("occampi:type ::= [ 0 occampi:primtype 1 ] [ 1 {<opi:nullreduce>} -* ]"));
	dynarray_add (transtbl, dfa_bnftotbl ("occampi:typecommalist ::= { occampi:type @@, 1 }"));

	*ntrans = DA_CUR (transtbl);
	return DA_PTR (transtbl);
}
/*}}}*/
/*{{{  static int occampi_type_post_setup (void)*/
/*
 *	does post-setup for type nodes
 *	returns 0 on success, non-zero on failure
 */
static int occampi_type_post_setup (void)
{
	static dfaerrorhandler_t protocol_eh = { occampi_protocol_dfaeh_stuck };

	dfa_seterrorhandler ("occampi:protocol", &protocol_eh);

	return 0;
}
/*}}}*/


/*{{{  occampi_type_feunit (feunit_t struct)*/
feunit_t occampi_type_feunit = {
	init_nodes: occampi_type_init_nodes,
	reg_reducers: occampi_type_reg_reducers,
	init_dfatrans: occampi_type_init_dfatrans,
	post_setup: occampi_type_post_setup
};
/*}}}*/


