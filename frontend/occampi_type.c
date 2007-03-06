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
#include "fcnlib.h"
#include "langdef.h"
#include "dfa.h"
#include "dfaerror.h"
#include "parsepriv.h"
#include "occampi.h"
#include "feunit.h"
#include "names.h"
#include "scope.h"
#include "prescope.h"
#include "typecheck.h"
#include "betrans.h"
#include "langops.h"
#include "target.h"
#include "map.h"
#include "transputer.h"
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
/*{{{  static void occampi_type_initportdecl (tnode_t *node, codegen_t *cgen, void *arg)*/
/*
 *	does initialiser code-gen for a PORT declaration
 */
static void occampi_type_initportdecl (tnode_t *node, codegen_t *cgen, void *arg)
{
	tnode_t *porttype = (tnode_t *)arg;
	int ws_off, vs_off, ms_off, ms_shdw;
	chook_t *pcevhook = tnode_lookupornewchook ("precode:vars");
	tnode_t *pcvars = NULL;

	codegen_callops (cgen, debugline, node);

	/* FIXME: assuming single port for now.. */
	cgen->target->be_getoffsets (node, &ws_off, &vs_off, &ms_off, &ms_shdw);

#if 0
fprintf (stderr, "occampi_initportdecl(): node=[%s], allocated at [%d,%d,%d], type is:\n", node->tag->name, ws_off, vs_off, ms_off);
tnode_dumptree (porttype, 1, stderr);
fprintf (stderr, "occampi_initportdecl(): tnode_nthsubof (node, 0) =\n");
tnode_dumptree (tnode_nthsubof (node, 0), 1, stderr);
#endif
	pcvars = (tnode_t *)tnode_getchook (tnode_nthsubof (node, 0), pcevhook);
	if (!pcvars) {
		codegen_warning (cgen, "occampi_type_initportdecl(): did not find hooked precode:vars on node [%s], zeroing PORT placement", tnode_nthsubof (node, 0)->tag->name);
		codegen_callops (cgen, loadconst, 0);
	} else {
		codegen_callops (cgen, loadname, pcvars, 0);
	}
	/* codegen_subcodegen (tnode_nthsubof (porttype, 1), cgen); */
	/* codegen_callops (cgen, loadconst, 0); */
	codegen_callops (cgen, storelocal, ws_off);
	codegen_callops (cgen, comment, "initportdecl");

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
#if 0
		nocc_message ("occampi_type_prescope(): got typeattr = 0x%8.8x on asinput/asoutput node", (unsigned int)typeattr);
#endif
		typeattr |= (losing->tag == opi.tag_ASINPUT) ? TYPEATTR_MARKED_IN : TYPEATTR_MARKED_OUT;
		tnode_setchook (*nodep, opi.chook_typeattr, (void *)typeattr);

		tnode_free (losing);
	}
	return 1;
}
/*}}}*/
/*{{{  static int occampi_type_namemap (compops_t *cops, tnode_t **nodep, map_t *map)*/
/*
 *	does name-mapping on a type node -- should only be called for PORTs, to map out the placement address
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_type_namemap (compops_t *cops, tnode_t **nodep, map_t *map)
{
	if (((*nodep)->tag == opi.tag_PORT) && tnode_nthsubof (*nodep, 1)) {
		map_submapnames (tnode_nthsubaddr (*nodep, 1), map);
		precode_addtoprecodevars (*nodep, tnode_nthsubof (*nodep, 1));
		return 0;
	}
	tnode_warning (*nodep, "occampi_type_namemap(): called for non-PORT, was [%s]", (*nodep)->tag->name);
	return 1;
}
/*}}}*/
/*{{{  static int occampi_type_precode (compops_t *cops, tnode_t **nodep, codegen_t *cgen)*/
/*
 *	does pre-coding on a type node -- should only be called for PORTs, to pre-code the placement address
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_type_precode (compops_t *cops, tnode_t **nodep, codegen_t *cgen)
{
	if (((*nodep)->tag == opi.tag_PORT) && tnode_nthsubof (*nodep, 1)) {
		codegen_subprecode (tnode_nthsubaddr (*nodep, 1), cgen);
		return 0;
	}
	tnode_warning (*nodep, "occampi_type_precode(): called for non-PORT, was [%s]", (*nodep)->tag->name);
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
	if (formaltype->tag == opi.tag_CHAN) {
		/*{{{  actual type-check for channel*/
		if ((node->tag == opi.tag_INPUT) || (node->tag == opi.tag_OUTPUT)) {
			/* becomes a protocol-check in effect */
			atype = tnode_nthsubof (formaltype, 0);

#if 0
fprintf (stderr, "occampi_type_typeactual(): channel: node->tag = [%s]\n", node->tag->name);
#endif
			atype = typecheck_typeactual (atype, actualtype, node, tc);
		} else {
			/* must be two channels then */
			if (actualtype->tag != opi.tag_CHAN) {
				typecheck_error (node, tc, "expected channel, found [%s]", actualtype->tag->name);
			}
			atype = actualtype;

			if (!typecheck_typeactual (tnode_nthsubof (formaltype, 0), tnode_nthsubof (actualtype, 0), node, tc)) {
				return NULL;
			}
		}
		/*}}}*/
	} else if (formaltype->tag == opi.tag_PORT) {
		/*{{{  actual type-check for port*/
		if ((node->tag == opi.tag_INPUT) || (node->tag == opi.tag_OUTPUT)) {
			/* becomes a protocol-check in effect */
			atype = tnode_nthsubof (formaltype, 0);

#if 0
fprintf (stderr, "occampi_type_typeactual(): port: node->tag = [%s]\n", node->tag->name);
#endif
			atype = typecheck_typeactual (atype, actualtype, node, tc);
		} else {
			/* must be two ports */
			if (actualtype->tag != opi.tag_PORT) {
				typecheck_error (node, tc, "expected port, found [%s]", actualtype->tag->name);
			}
			atype = actualtype;

			if (!typecheck_typeactual (tnode_nthsubof (formaltype, 0), tnode_nthsubof (actualtype, 0), node, tc)) {
				return NULL;
			}
		}
		/*}}}*/
	} else {
		nocc_fatal ("occampi_type_typeactual(): don't know how to handle a non-channel here (yet), got [%s]", formaltype->tag->name);
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
	} else if (t->tag == opi.tag_PORT) {
		/* FIXME! */
		return target->chansize;
	}
	return -1;
}
/*}}}*/
/*{{{  static int occampi_type_initsizes (langops_t *lops, tnode_t *t, tnode_t *declnode, int *wssize, int *vssize, int *mssize, int *indir, map_t *mdata)*/
/*
 *	called during mapping to determine memory requirements in a declaration
 *	returns non-zero if settings were made, zero otherwise
 */
static int occampi_type_initsizes (langops_t *lops, tnode_t *t, tnode_t *declnode, int *wssize, int *vssize, int *mssize, int *indir, map_t *mdata)
{
	if (t->tag == opi.tag_CHAN) {
		/* channel declaration, needs 1 word in workspace */
		*wssize = mdata->target->chansize;
		*vssize = 0;
		*mssize = 0;
		*indir = 0;
		return 1;
	} else if (t->tag == opi.tag_PORT) {
		/* port declaration, actually indirected, so needs a pointer's worth of workspace */
		*wssize = mdata->target->pointersize;
		*vssize = 0;
		*mssize = 0;
		*indir = 1;
		return 1;
	}
	return 0;
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
	} else if ((t->tag == opi.tag_PORT) && tnode_nthsubof (t, 1)) {
		/* PLACED port, need pointer initialisation */
		codegen_setinithook (benode, occampi_type_initportdecl, (void *)t);
		return 1;
	}
	return 0;
}
/*}}}*/
/*{{{  static int occampi_type_codegen_typeaction (langops_t *lops, tnode_t *type, tnode_t *anode, codegen_t *cgen)*/
/*
 *	this handles code-generation for actions involving channels or ports
 *	returns 0 to stop the code-gen walk, 1 to continue, -1 to resort to normal action handling
 */
static int occampi_type_codegen_typeaction (langops_t *lops, tnode_t *type, tnode_t *anode, codegen_t *cgen)
{
	tnode_t *lhs = tnode_nthsubof (anode, 0);		/* some guarantee that action-nodes have these */
	tnode_t *rhs = tnode_nthsubof (anode, 1);

	if (type->tag == opi.tag_CHAN) {
		/*{{{  deal with channel actions*/
		if (anode->tag == opi.tag_ASSIGN) {
			/* doesn't make sense to assign these */
			codegen_warning (cgen, "occampi_type_codegen_typaction(): attempt to assign channel!");
			return -1;
		} else if (anode->tag == opi.tag_INPUT) {
			int bytes = tnode_bytesfor (type, cgen->target);

			codegen_callops (cgen, loadpointer, rhs, 0);
			codegen_callops (cgen, loadpointer, lhs, 0);
			codegen_callops (cgen, loadconst, bytes);
			codegen_callops (cgen, tsecondary, I_IN);
		} else if (anode->tag == opi.tag_OUTPUT) {
			int bytes = tnode_bytesfor (type, cgen->target);

			codegen_callops (cgen, loadpointer, rhs, 0);
			codegen_callops (cgen, loadpointer, lhs, 0);
			codegen_callops (cgen, loadconst, bytes);
			codegen_callops (cgen, tsecondary, I_OUT);
		}

		return 0;
		/*}}}*/
	} else if (type->tag == opi.tag_PORT) {
		/*{{{  deal with port actions*/
		tnode_t *ptype = tnode_nthsubof (type, 0);
		int pcsize = tnode_bytesfor (ptype, cgen->target);

#if 0
fprintf (stderr, "occampi_type_codegen_typeaction(): PORT: %d bytes, ptype =\n", pcsize);
tnode_dumptree (ptype, 1, stderr);
#endif
		if (anode->tag == opi.tag_ASSIGN) {
			/* doesn't make sense to assign these */
			codegen_warning (cgen, "occampi_type_codegen_typaction(): attempt to assign port!");
			return -1;
		} else if (anode->tag == opi.tag_INPUT) {
			codegen_callops (cgen, loadpointer, rhs, 0);
			codegen_callops (cgen, loadpointer, lhs, 0);
			codegen_callops (cgen, loadconst, pcsize);
			codegen_callops (cgen, tsecondary, I_IOR);
		} else if (anode->tag == opi.tag_OUTPUT) {
			codegen_callops (cgen, loadpointer, rhs, 0);
			codegen_callops (cgen, loadpointer, lhs, 0);
			codegen_callops (cgen, loadconst, pcsize);
			codegen_callops (cgen, tsecondary, I_IOW);
		}

		return 0;
		/*}}}*/
	}
	return -1;
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
/*{{{  static int occampi_typespec_occampi_typeattrof (langops_t *lops, tnode_t *node, occampi_typeattr_t *taptr)*/
/*
 *	gets the type attributes of a type-spec node
 *	returns 0 on success, non-zero on failure
 */
static int occampi_typespec_occampi_typeattrof (langops_t *lops, tnode_t *node, occampi_typeattr_t *taptr)
{
	if (!node || !taptr) {
		nocc_internal ("occampi_typespec_occampi_typeattrof(): NULL node or attr-ptr!");
	}
	*taptr = (occampi_typeattr_t)tnode_getchook (node, opi.chook_typeattr);
	return 0;
}
/*}}}*/
/*{{{  static int occampi_typespec_betrans (compops_t *cops, tnode_t **tptr, betrans_t *be)*/
/*
 *	does back-end transformations for a type-spec node
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_typespec_betrans (compops_t *cops, tnode_t **tptr, betrans_t *be)
{
	tnode_t *thisnode = *tptr;

	betrans_subtree (tnode_nthsubaddr (thisnode, 0), be);
	*tptr = tnode_nthsubof (thisnode, 0);
	tnode_setnthsub (thisnode, 0, NULL);

	/* FIXME: may be attached to a namenode's type */
	/* tnode_free (thisnode); */
	return 0;
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
/*{{{  static void occampi_reduce_placedfold (dfastate_t *dfast, parsepriv_t *pp, void *rarg)*/
/*
 *	called to fold an X placement address into a X specification (presumably within an array)
 *	where X is probably PORT
 */
static void occampi_reduce_placedfold (dfastate_t *dfast, parsepriv_t *pp, void *rarg)
{
	tnode_t *expr = dfa_popnode (dfast);
	tnode_t *name = dfa_popnode (dfast);
	tnode_t *aspec = dfa_popnode (dfast);
	tnode_t *walk;

#if 0
	fprintf (stderr, "occampi_reduce_placedfold(): expr =\n");
	tnode_dumptree (expr, 1, stderr);
	fprintf (stderr, "name =\n");
	tnode_dumptree (name, 1, stderr);
	fprintf (stderr, "aspec =\n");
	tnode_dumptree (aspec, 1, stderr);
#endif
	for (walk=aspec; walk && (walk->tag == opi.tag_ARRAY); walk = tnode_nthsubof (walk, 1));
	if (walk->tag == opi.tag_PORT) {
		tnode_setnthsub (walk, 1, expr);
	} else {
		tnode_error (walk, "occampi_reduce_placedfold(): not a placeable thing [%s]", walk->tag->name);
	}
	*(dfast->ptr) = tnode_createfrom (opi.tag_VARDECL, name, name, aspec, NULL);
	// dfa_pushnode (dfast, NULL);
	return;
}
/*}}}*/


/*{{{  occampi_typeattr_t occampi_typeattrof (tnode_t *node)*/
/*
 *	this can be called by other occam-pi parts to get the type-attributes of a node
 *	does it via compiler-hook then langops-call
 *	returns type attribute(s)
 */
occampi_typeattr_t occampi_typeattrof (tnode_t *node)
{
	occampi_typeattr_t attr;

	attr = (occampi_typeattr_t)tnode_getchook (node, opi.chook_typeattr);
	if (attr == TYPEATTR_NONE) {
		if (tnode_haslangop (node->tag->ndef->lops, "occampi_typeattrof")) {
			tnode_calllangop (node->tag->ndef->lops, "occampi_typeattrof", 2, node, &attr);
		}
	}
	return attr;
}
/*}}}*/
/*{{{  void occampi_settypeattr (tnode_t *node, occampi_typeattr_t attr)*/
/*
 *	this sets the type-attributes of a node, does it via compiler-hook only
 */
void occampi_settypeattr (tnode_t *node, occampi_typeattr_t attr)
{
	int aval = (int)attr;

	tnode_setchook (node, opi.chook_typeattr, (void *)aval);
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

	/*{{{  register reduction functions*/
	fcnlib_addfcn ("occampi_reduce_primtype", occampi_reduce_primtype, 0, 3);
	fcnlib_addfcn ("occampi_reduce_placedfold", occampi_reduce_placedfold, 0, 3);

	/*}}}*/
	/*{{{  attributes compiler hook*/
	opi.chook_typeattr = tnode_newchook ("occampi:typeattr");
	opi.chook_typeattr->chook_dumptree = occampi_typeattr_dumpchook;
	opi.chook_typeattr->chook_copy = occampi_typeattr_copychook;
	opi.chook_typeattr->chook_free = occampi_typeattr_freechook;

	/*}}}*/
	/*{{{  occampi_typeattrof -- language op.*/
	tnode_newlangop ("occampi_typeattrof", LOPS_INVALID, 2, (void *)&occampi_parser);

	/*}}}*/
	/*{{{  occampi:typenode -- CHAN, PORT, ASINPUT, ASOUTPUT*/
	i = -1;
	tnd = opi.node_TYPENODE = tnode_newnodetype ("occampi:typenode", &i, 2, 0, 0, TNF_NONE);			/* subnodes: 0 = subtype, 1 = placement address if relevant */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "prescope", 2, COMPOPTYPE (occampi_type_prescope));
	tnode_setcompop (cops, "precode", 2, COMPOPTYPE (occampi_type_precode));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (occampi_type_namemap));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "getdescriptor", 2, LANGOPTYPE (occampi_type_getdescriptor));
	tnode_setlangop (lops, "gettype", 2, LANGOPTYPE (occampi_type_gettype));
	tnode_setlangop (lops, "typeactual", 4, LANGOPTYPE (occampi_type_typeactual));
	tnode_setlangop (lops, "bytesfor", 2, LANGOPTYPE (occampi_type_bytesfor));
	tnode_setlangop (lops, "issigned", 2, LANGOPTYPE (occampi_type_issigned));
	tnode_setlangop (lops, "initsizes", 7, LANGOPTYPE (occampi_type_initsizes));
	tnode_setlangop (lops, "initialising_decl", 3, LANGOPTYPE (occampi_type_initialising_decl));
	tnode_setlangop (lops, "codegen_typeaction", 3, LANGOPTYPE (occampi_type_codegen_typeaction));
	tnd->lops = lops;

	i = -1;
	opi.tag_CHAN = tnode_newnodetag ("CHAN", &i, tnd, NTF_SYNCTYPE);
	i = -1;
	opi.tag_PORT = tnode_newnodetag ("PORT", &i, tnd, NTF_NAMEMAPTYPEINDECL | NTF_PRECODETYPEINDECL);
	i = -1;
	opi.tag_ASINPUT = tnode_newnodetag ("ASINPUT", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_ASOUTPUT = tnode_newnodetag ("ASOUTPUT", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  occampi:typespecnode -- TYPESPEC*/
	/* these appear during scoping */
	i = -1;
	tnd = tnode_newnodetype ("occampi:typespecnode", &i, 1, 0, 0, TNF_TRANSPARENT);
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "betrans", 2, COMPOPTYPE (occampi_typespec_betrans));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "gettype", 2, LANGOPTYPE (occampi_typespec_gettype));
	tnode_setlangop (lops, "typeactual", 4, LANGOPTYPE (occampi_typespec_typeactual));
	tnode_setlangop (lops, "occampi_typeattrof", 2, LANGOPTYPE (occampi_typespec_occampi_typeattrof));
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

	return 0;
}
/*}}}*/


/*{{{  occampi_type_feunit (feunit_t struct)*/
feunit_t occampi_type_feunit = {
	init_nodes: occampi_type_init_nodes,
	reg_reducers: NULL,
	init_dfatrans: NULL,
	post_setup: NULL,
	ident: "occampi-type"
};
/*}}}*/


