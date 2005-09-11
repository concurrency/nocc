/*
 *	occampi_dtype.c -- occam-pi data type handling
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
#include "precheck.h"
#include "usagecheck.h"
#include "map.h"
#include "target.h"
#include "transputer.h"
#include "codegen.h"
#include "langops.h"


/*}}}*/

/*
 *	this file contains the compiler front-end routines for occam-pi
 *	declarations, parameters and names.
 */

/*{{{  private types*/
typedef struct TAG_typedeclhook {
	int wssize;
} typedeclhook_t;

typedef struct TAG_fielddecloffset {
	int offset;
} fielddecloffset_t;


/*}}}*/
/*{{{  private data*/
static chook_t *fielddecloffset = NULL;

/*}}}*/


/*{{{  static void occampi_typedecl_hook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	dumps a typedeclhook_t hook-node (debugging)
 */
static void occampi_typedecl_hook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	typedeclhook_t *tdh = (typedeclhook_t *)hook;

	occampi_isetindent (stream, indent);
	if (!hook) {
		fprintf (stream, "<typedeclhook value=\"(null)\" addr=\"0x%8.8x\" />\n", (unsigned int)tdh);
	} else {
		fprintf (stream, "<typedeclhook wssize=\"%d\" addr=\"0x%8.8x\" />\n", tdh->wssize, (unsigned int)tdh);
	}

	return;
}
/*}}}*/
/*{{{  static void *occampi_typedeclhook_blankhook (void *tos)*/
/*
 *	creates a new typedeclhook_t and returns it as void * for DFA processing
 */
static void *occampi_typedeclhook_blankhook (void *tos)
{
	typedeclhook_t *tdh;

	if (tos) {
		nocc_internal ("occampi_typedeclhook_blankhook(): tos was not NULL (0x%8.8x)", (unsigned int)tos);
		return NULL;
	}
	tdh = (typedeclhook_t *)smalloc (sizeof (typedeclhook_t));

	tdh->wssize = 0;

	return (void *)tdh;
}
/*}}}*/
/*{{{  static void occampi_fielddecloffset_chook_dumptree (tnode_t *node, void *chook, int indent, FILE *stream)*/
/*
 *	dumps a fielddecloffset_t chook (debugging)
 */
static void occampi_fielddecloffset_chook_dumptree (tnode_t *node, void *chook, int indent, FILE *stream)
{
	fielddecloffset_t *ofh = (fielddecloffset_t *)chook;

	occampi_isetindent (stream, indent);
	fprintf (stream, "<chook:fielddecloffset offset=\"%d\" />\n", ofh->offset);

	return;
}
/*}}}*/
/*{{{  static void *occampi_fielddecloffset_chook_create (int offset)*/
/*
 *	creates a new fielddecloffset chook
 */
static void *occampi_fielddecloffset_chook_create (int offset)
{
	fielddecloffset_t *ofh = (fielddecloffset_t *)smalloc (sizeof (fielddecloffset_t));

	ofh->offset = offset;

	return (void *)ofh;
}
/*}}}*/


/*{{{  static int occampi_scopein_typedecl (tnode_t **node, scope_t *ss)*/
/*
 *	called to scope a type declaration (DATA TYPE ...)
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_scopein_typedecl (tnode_t **node, scope_t *ss)
{
	tnode_t *name = tnode_nthsubof (*node, 0);
	tnode_t *type;
	char *rawname;
	name_t *sname = NULL;
	tnode_t *newname;

	if (name->tag != opi.tag_NAME) {
		scope_error (name, ss, "name not raw-name!");
		return 0;
	}
	rawname = tnode_nthhookof (name, 0);

#if 0
fprintf (stderr, "occampi_scopein_typedecl: here! rawname = \"%s\".  unscoped type=\n", rawname);
tnode_dumptree (tnode_nthsubof (*node, 1), 1, stderr);
#endif
	if (scope_subtree (tnode_nthsubaddr (*node, 1), ss)) {
		return 0;
	}
	type = tnode_nthsubof (*node, 1);
#if 0
fprintf (stderr, "occampi_scopein_typedecl: here! rawname = \"%s\".  scoped type=\n", rawname);
tnode_dumptree (type, 1, stderr);
#endif

	sname = name_addscopename (rawname, *node, type, NULL);
	newname = tnode_createfrom (opi.tag_NTYPEDECL, name, sname);
	SetNameNode (sname, newname);
	tnode_setnthsub (*node, 0, newname);

	/* free the old name */
	tnode_free (name);
	ss->scoped++;

	/* scope body */
	if (scope_subtree (tnode_nthsubaddr (*node, 2), ss)) {
		return 0;
	}

	name_descopename (sname);

	return 0;
}
/*}}}*/
/*{{{  static int occampi_prewalk_bytesfor_typedecl (tnode_t *node, void *data)*/
/*
 *	walks a tree to collect the cumulative size of a type (record types)
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_prewalk_bytesfor_typedecl (tnode_t *node, void *data)
{
	typedeclhook_t *tdh = (typedeclhook_t *)data;
	int this_ws;

	this_ws = tnode_bytesfor (node);
	if (this_ws > 0) {
		tdh->wssize += this_ws;
		return 0;
	}
	return 1;
}
/*}}}*/
/*{{{  static int occampi_bytesfor_typedecl (tnode_t *node)*/
/*
 *	returns the number of bytes required by a type declaration (DATA TYPE ...)
 */
static int occampi_bytesfor_typedecl (tnode_t *node)
{
	typedeclhook_t *tdh = (typedeclhook_t *)tnode_nthhookof (node, 0);
	tnode_t *type = tnode_nthsubof (node, 1);

	if (!tdh->wssize) {
		/*{{{  walk the type to find out its size*/
		tnode_prewalktree (type, occampi_prewalk_bytesfor_typedecl, (void *)tdh);

		if (!tdh->wssize) {
			nocc_error ("occampi_bytesfor_typedecl(): type has 0 size..  :(");
		}

		/*}}}*/
	}

#if 0
fprintf (stderr, "occampi_bytesfor_typedecl(): return size = %d.  type =\n", tdh->wssize);
tnode_dumptree (type, 1, stderr);
#endif
	return tdh->wssize;
}
/*}}}*/
/*{{{  static int occampi_namemap_typedecl (tnode_t **node, map_t *mdata)*/
/*
 *	does name mapping for a type declaration (allocates offsets in structured types)
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_namemap_typedecl (tnode_t **node, map_t *mdata)
{
	tnode_t *type = tnode_nthsubof (*node, 1);
	tnode_t **bodyp = tnode_nthsubaddr (*node, 2);

	if (parser_islistnode (type)) {
		tnode_t **items;
		int nitems, i;
		int csize = 0;

		items = parser_getlistitems (type, &nitems);
		for (i=0; i<nitems; i++) {
			if (!items[i]) {
				continue;
			} else if (items[i]->tag != opi.tag_FIELDDECL) {
				nocc_error ("occampi_namemap_typedecl(): item in TYPEDECL not FIELDDECL, was [%s]", items[i]->tag->name);
			} else {
				tnode_t *fldname = tnode_nthsubof (items[i], 0);
				tnode_t *fldtype = tnode_nthsubof (items[i], 1);
				int tsize;

				tsize = tnode_bytesfor (fldtype);
				tnode_setchook (fldname, fielddecloffset, occampi_fielddecloffset_chook_create (csize));
				csize += tsize;
			}
		}
	}

	map_submapnames (bodyp, mdata);

	return 0;
}
/*}}}*/


/*{{{  static int occampi_scopein_fielddecl (tnode_t **node, scope_t *ss)*/
/*
 *	called to scope in a field declaration (inside a DATA TYPE)
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_scopein_fielddecl (tnode_t **node, scope_t *ss)
{
	tnode_t *name = tnode_nthsubof (*node, 0);
	tnode_t *type;
	char *rawname;
	name_t *sname = NULL;
	tnode_t *newname;

#if 0
fprintf (stderr, "occampi_scopein_fielddecl(): *node =\n");
tnode_dumptree (*node, 1, stderr);
#endif
	if (name->tag != opi.tag_NAME) {
		scope_error (name, ss, "name not raw-name!");
		return 0;
	}
	rawname = tnode_nthhookof (name, 0);

	scope_subtree (tnode_nthsubaddr (*node, 1), ss);		/* scope type */
	type = tnode_nthsubof (*node, 1);

#if 0
fprintf (stderr, "occampi_scopein_fielddecl(): scoping field [%s], scoped type:\n", rawname);
tnode_dumptree (type, 1, stderr);
#endif
	sname = name_addscopename (rawname, *node, type, NULL);
	newname = tnode_createfrom (opi.tag_NFIELD, name, sname);
	SetNameNode (sname, newname);
	tnode_setnthsub (*node, 0, newname);

	/* free old name */
	tnode_free (name);
	ss->scoped++;

	/* and descope immediately */
	name_descopename (sname);

	return 0;
}
/*}}}*/
/*{{{  static int occampi_fielddecl_prewalk_scopefields (tnode_t *node, void *data)*/
/*
 *	called to scope in fields in a record type -- already NAMENODEs
 */
static int occampi_fielddecl_prewalk_scopefields (tnode_t *node, void *data)
{
	scope_t *ss = (scope_t *)data;

#if 0
fprintf (stderr, "occampi_fielddecl_prewalk_scopefields(): node = [%s]\n", node->tag->name);
#endif
	if (node->tag == opi.tag_FIELDDECL) {
		tnode_t *fldname = tnode_nthsubof (node, 0);

		if (fldname->tag == opi.tag_NFIELD) {
#if 0
fprintf (stderr, "occampi_fielddecl_prewalk_scopefields(): adding name [%s]\n", NameNameOf (tnode_nthnameof (fldname, 0)));
#endif
			name_scopename (tnode_nthnameof (fldname, 0));
		} else {
			scope_warning (fldname, ss, "FIELDDECL does not have NFIELD name");
		}
	}
	return 1;
}
/*}}}*/
/*{{{  static int occampi_bytesfor_fielddecl (tnode_t *node)*/
/*
 *	returns the number of bytes required by a FIELDDECL
 */
static int occampi_bytesfor_fielddecl (tnode_t *node)
{
	tnode_t *type = tnode_nthsubof (node, 1);
	int bytes = tnode_bytesfor (type);

#if 0
fprintf (stderr, "occampi_bytesfor_fielddecl(): bytes = %d, type =\n", bytes);
tnode_dumptree (type, 1, stderr);
#endif
	return bytes;
}
/*}}}*/


/*{{{  static int occampi_scopein_subscript (tnode_t **node, scope_t *ss)*/
/*
 *	called to scope a subscript node -- turns into an ARRAYSUB or RECORDSUB as appropriate
 *	return 0 to stop walk, 1 to continue
 */
static int occampi_scopein_subscript (tnode_t **node, scope_t *ss)
{
	tnode_t *base;
	tnode_t *oldnode = *node;

	if (oldnode->tag != opi.tag_SUBSCRIPT) {
		/* already done this */
		return 0;
	}
	if (scope_subtree (tnode_nthsubaddr (*node, 0), ss)) {		/* scope base */
		return 0;
	}
	base = tnode_nthsubof (*node, 0);

#if 0
fprintf (stderr, "occampi_scopein_subscript(): scoped base, *node =\n");
tnode_dumptree (*node, 1, stderr);
#endif
	if (base->tag->ndef == opi.node_NAMENODE) {
		name_t *name = tnode_nthnameof (base, 0);
		tnode_t *type = NameTypeOf (name);

		if (type->tag == opi.tag_NTYPEDECL) {
			void *namemarker;

			namemarker = name_markscope ();
			tnode_prewalktree (NameTypeOf (tnode_nthnameof (type, 0)), occampi_fielddecl_prewalk_scopefields, (void *)ss);

			/* fields should be in scope, try subscript */
			scope_subtree (tnode_nthsubaddr (*node, 1), ss);
#if 0
fprintf (stderr, "occampi_scopein_subscript(): scoped subscript, *node =\n");
tnode_dumptree (*node, 1, stderr);
#endif
			*node = tnode_createfrom (opi.tag_RECORDSUB, oldnode, tnode_nthsubof (oldnode, 0), tnode_nthsubof (oldnode, 1));
			tnode_setnthsub (oldnode, 0, NULL);
			tnode_setnthsub (oldnode, 1, NULL);
			tnode_free (oldnode);

			name_markdescope (namemarker);
		} else {
			/* probably a simple type */
			scope_subtree (tnode_nthsubaddr (*node, 1), ss);

			*node = tnode_createfrom (opi.tag_ARRAYSUB, oldnode, tnode_nthsubof (oldnode, 0), tnode_nthsubof (oldnode, 1));
			tnode_setnthsub (oldnode, 0, NULL);
			tnode_setnthsub (oldnode, 1, NULL);
			tnode_free (oldnode);
		}
	}

	return 0;
}
/*}}}*/
/*{{{  static tnode_t *occampi_gettype_subscript (tnode_t *node, tnode_t *defaulttype)*/
/*
 *	called to get the type of a subscript
 */
static tnode_t *occampi_gettype_subscript (tnode_t *node, tnode_t *defaulttype)
{
	if (node->tag == opi.tag_RECORDSUB) {
		/* type is that of the field */
		tnode_t *field = tnode_nthsubof (node, 1);
		name_t *fldname;
		tnode_t *fldtype;

		if (field->tag != opi.tag_NFIELD) {
			return NULL;
		}
		fldname = tnode_nthnameof (field, 0);
		fldtype = NameTypeOf (fldname);

#if 0
fprintf (stderr, "occampi_gettype_subscript(): for [%s], returning:\n", node->tag->name);
tnode_dumptree (fldtype, 1, stderr);
#endif
		return fldtype;
	} else if (node->tag == opi.tag_ARRAYSUB) {
		/* type is that of the base minus one ARRAY */
		nocc_internal ("occampi_gettype_subscript(): ARRAYSUB not properly implemented yet!");
		return NULL;
	}
	/* else don't know.. */
	return defaulttype;
}
/*}}}*/
/*{{{  static int occampi_namemap_subscript (tnode_t **node, map_t *mdata)*/
/*
 *	name-maps a subscript-node, turning it into a back-end INDEXED node
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_namemap_subscript (tnode_t **node, map_t *mdata)
{
	if ((*node)->tag == opi.tag_RECORDSUB) {
		fielddecloffset_t *fdh;
		tnode_t *index = tnode_nthsubof (*node, 1);

		/* "index" should be an N_FIELD */
		if (index->tag != opi.tag_NFIELD) {
			return 0;
		}
		fdh = (fielddecloffset_t *)tnode_getchook (index, fielddecloffset);

		*node = mdata->target->newindexed (tnode_nthsubof (*node, 0), NULL, 0, fdh->offset);

	} else if ((*node)->tag == opi.tag_ARRAYSUB) {
		nocc_error ("occampi_namemap_subscript(): ARRAYSUB not supported yet!");
		return 0;
	} else {
		nocc_error ("occampi_namemap_subscript(): unsupported subscript type [%s]", (*node)->tag->name);
		return 0;
	}
	return 1;
}
/*}}}*/


/*{{{  static void occampi_typedecl_dfaeh_stuck (dfanode_t *dfanode, token_t *tok)*/
/*
 *	called by parser when it gets stuck in an occampi:typedecl DFA node
 */
static void occampi_typedecl_dfaeh_stuck (dfanode_t *dfanode, token_t *tok)
{
	char msgbuf[1024];
	int gone = 0;
	int max = 1023;

	gone += snprintf (msgbuf + gone, max - gone, "parse error at %s in DATA TYPE declaration", lexer_stokenstr (tok));
	if (DA_CUR (dfanode->match)) {
		int n;

		gone += snprintf (msgbuf + gone, max - gone, ", expected ");
		for (n=0; n<DA_CUR (dfanode->match); n++) {
			token_t *match = DA_NTHITEM (dfanode->match, n);

			gone += snprintf (msgbuf + gone, max - gone, "%s%s", !n ? "" : ((n == DA_CUR (dfanode->match) - 1) ? " or " : ", "), lexer_stokenstr (match));
		}
	}
	parser_error (tok->origin, msgbuf);
	return;
}
/*}}}*/


/*{{{  static int occampi_dtype_init_nodes (void)*/
/*
 *	sets up data type nodes for occampi
 *	returns 0 on success, non-zero on error
 */
static int occampi_dtype_init_nodes (void)
{
	tndef_t *tnd;
	compops_t *cops;
	langops_t *lops;
	int i;

	/*{{{  occampi:typedecl -- TYPEDECL*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:typedecl", &i, 3, 0, 1, TNF_SHORTDECL);
	tnd->hook_dumptree = occampi_typedecl_hook_dumptree;
	cops = tnode_newcompops ();
	cops->scopein = occampi_scopein_typedecl;
	cops->bytesfor = occampi_bytesfor_typedecl;
	cops->namemap = occampi_namemap_typedecl;
	tnd->ops = cops;
	i = -1;
	opi.tag_TYPEDECL = tnode_newnodetag ("TYPEDECL", &i, tnd, NTF_NONE);
	i = -1;
	/*}}}*/
	/*{{{  occampi:fielddecl -- FIELDDECL*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:fielddecl", &i, 2, 0, 0, TNF_NONE);
	cops = tnode_newcompops ();
	cops->scopein = occampi_scopein_fielddecl;
	cops->bytesfor = occampi_bytesfor_fielddecl;
	tnd->ops = cops;
	i = -1;
	opi.tag_FIELDDECL = tnode_newnodetag ("FIELDDECL", &i, tnd, NTF_NONE);
	/*}}}*/
	/*{{{  occampi:subscript -- SUBSCRIPT*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:subscript", &i, 2, 0, 0, TNF_NONE);
	cops = tnode_newcompops ();
	cops->scopein = occampi_scopein_subscript;
	cops->gettype = occampi_gettype_subscript;
	cops->namemap = occampi_namemap_subscript;
	tnd->ops = cops;
	i = -1;
	opi.tag_SUBSCRIPT = tnode_newnodetag ("SUBSCRIPT", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_RECORDSUB = tnode_newnodetag ("RECORDSUB", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_ARRAYSUB = tnode_newnodetag ("ARRAYSUB", &i, tnd, NTF_NONE);
	/*}}}*/
	/*{{{  fielddecloffset compiler hook*/
	fielddecloffset = tnode_lookupornewchook ("occampi:fielddecloffset");
	fielddecloffset->chook_dumptree = occampi_fielddecloffset_chook_dumptree;

	/*}}}*/

	return 0;
}
/*}}}*/
/*{{{  static void occampi_reduce_resetnewline (dfastate_t *dfast, parsepriv_t *pp, void *rarg)*/
/*
 *	creates a newline token and pushes it back into the lexer
 */
static void occampi_reduce_resetnewline (dfastate_t *dfast, parsepriv_t *pp, void *rarg)
{
	token_t *tok = lexer_newtoken (NEWLINE);

#if 0
fprintf (stderr, "occampi_reduce_resetnewline(): pp->lf = 0x%8.8x, DA_CUR (pp->tokstack) = %d, DA_CUR (dfast->nodestack) = %d\n", (unsigned int)pp->lf, DA_CUR (pp->tokstack), DA_CUR (dfast->nodestack));
#endif

	tok->origin = pp->lf;
	lexer_pushback (pp->lf, tok);
	return;
}
/*}}}*/
/*{{{  static int occampi_dtype_reg_reducers (void)*/
/*
 *	registers reductions for declaration nodes
 */
static int occampi_dtype_reg_reducers (void)
{
	parser_register_grule ("opi:datatypedeclreduce", parser_decode_grule ("SN1N+N+V00XC4R-", occampi_typedeclhook_blankhook, opi.tag_TYPEDECL));
	parser_register_grule ("opi:fieldreduce", parser_decode_grule ("SN1N+N+C2R-", opi.tag_FIELDDECL));
	parser_register_grule ("opi:resultpush", parser_decode_grule ("R+N-"));

	parser_register_reduce ("Roccampi:resetnewline", occampi_reduce_resetnewline, NULL);

	return 0;
}
/*}}}*/
/*{{{  static dfattbl_t **occampi_dtype_init_dfatrans (int *ntrans)*/
/*
 *	initialises DFA transition tables for data type nodes
 */
static dfattbl_t **occampi_dtype_init_dfatrans (int *ntrans)
{
	DYNARRAY (dfattbl_t *, transtbl);

	dynarray_init (transtbl);

	dynarray_add (transtbl, dfa_transtotbl ("occampi:subtspec ::= [ 0 occampi:primtype 1 ] [ 1 occampi:namelist 2 ] [ 2 @@: 3 ] [ 3 {<opi:fieldreduce>} -* ]"));
	dynarray_add (transtbl, dfa_transtotbl ("occampi:subtspeclist ::= [ 0 occampi:subtspec 1 ] [ 1 {<parser:nullreduce>} ] [ 1 Newline 2 ] [ 1 -* 4 ] [ 2 {Rinlist} ] [ 2 occampi:subtspec 1 ] " \
				"[ 2 -* 3 ] [ 3 {Roccampi:resetnewline} ] [ 3 -* 4 ] [ 4 -* ]"));

	dynarray_add (transtbl, dfa_transtotbl ("occampi:typedecl ::= [ 0 @DATA 1 ] [ 1 @TYPE 2 ] [ 2 +Name 3 ] [ 3 {<opi:namepush>} ] [ 3 @IS 4 ] [ 3 Newline 7 ] " \
				"[ 4 occampi:type 5 ] [ 5 @@: 6 ] [ 6 {<opi:datatypedeclreduce>} -* ] " \
				"[ 7 Indent 8 ] [ 8 @RECORD 9 ] [ 9 Newline 10 ] [ 10 Indent 11 ] [ 11 occampi:subtspeclist 12 ] [ 12 Newline 13 ] " \
				"[ 13 Outdent 14 ] [ 14 Outdent 15 ] [ 15 @@: 16 ] [ 16 {<opi:datatypedeclreduce>} -* ] "));

	dynarray_add (transtbl, dfa_transtotbl ("occampi:namestartname +:= [ 0 @@[ 1 ] [ 1 occampi:expr 2 ] [ 2 @@] 3 ] [ 3 {<opi:xsubscriptreduce>} -* 4 ] [ 4 {<opi:resultpush>} -* 0 ]"));

	*ntrans = DA_CUR (transtbl);
	return DA_PTR (transtbl);
}
/*}}}*/
/*{{{  static int occampi_dtype_post_setup (void)*/
/*
 *	does post-setup for initialisation
 */
static int occampi_dtype_post_setup (void)
{
	static dfaerrorhandler_t typedecl_eh = { occampi_typedecl_dfaeh_stuck };

	dfa_seterrorhandler ("occampi:typedecl", &typedecl_eh);

	return 0;
}
/*}}}*/


/*{{{  occampi_dtype_feunit (feunit_t)*/
feunit_t occampi_dtype_feunit = {
	init_nodes: occampi_dtype_init_nodes,
	reg_reducers: occampi_dtype_reg_reducers,
	init_dfatrans: occampi_dtype_init_dfatrans,
	post_setup: occampi_dtype_post_setup
};
/*}}}*/

