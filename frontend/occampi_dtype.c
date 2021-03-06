/*
 *	occampi_dtype.c -- occam-pi data type handling (also named-type handling)
 *	Copyright (C) 2005-2016 Fred Barnes <frmb@kent.ac.uk>
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
#include <stdint.h>
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
#include "fcnlib.h"
#include "dfa.h"
#include "dfaerror.h"
#include "parsepriv.h"
#include "occampi.h"
#include "feunit.h"
#include "names.h"
#include "scope.h"
#include "prescope.h"
#include "typecheck.h"
#include "constprop.h"
#include "tracescheck.h"
#include "fetrans.h"
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
static chook_t *ct_clienttype = NULL;
static chook_t *ct_servertype = NULL;
static chook_t *ct_clienttrace = NULL;
static chook_t *ct_servertrace = NULL;

static chook_t *cttrace_chook = NULL;

static compop_t *intypedecl_scopein_compop = NULL;
static compop_t *intypedecl_scopeout_compop = NULL;

/*}}}*/


/*{{{  static void occampi_typedecl_hook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)*/
/*
 *	dumps a typedeclhook_t hook-node (debugging)
 */
static void occampi_typedecl_hook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)
{
	typedeclhook_t *tdh = (typedeclhook_t *)hook;

	occampi_isetindent (stream, indent);
	if (!hook) {
		fhandle_printf (stream, "<typedeclhook value=\"(null)\" addr=\"0x%8.8x\" />\n", (unsigned int)tdh);
	} else {
		fhandle_printf (stream, "<typedeclhook wssize=\"%d\" addr=\"0x%8.8x\" />\n", tdh->wssize, (unsigned int)tdh);
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
/*{{{  static void occampi_fielddecloffset_chook_dumptree (tnode_t *node, void *chook, int indent, fhandle_t *stream)*/
/*
 *	dumps a fielddecloffset_t chook (debugging)
 */
static void occampi_fielddecloffset_chook_dumptree (tnode_t *node, void *chook, int indent, fhandle_t *stream)
{
	fielddecloffset_t *ofh = (fielddecloffset_t *)chook;

	occampi_isetindent (stream, indent);
	fhandle_printf (stream, "<chook:fielddecloffset offset=\"%d\" />\n", ofh->offset);

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


/*{{{  static void occampi_ctclienttype_chook_dumptree (tnode_t *node, void *chook, int indent, fhandle_t *stream)*/
/*
 *	dumps a ct_clienttype chook (debugging)
 */
static void occampi_ctclienttype_chook_dumptree (tnode_t *node, void *chook, int indent, fhandle_t *stream)
{
	occampi_isetindent (stream, indent);
	fhandle_printf (stream, "<chook:ct_clienttype>\n");
	if (chook) {
		tnode_dumptree ((tnode_t *)chook, indent + 1, stream);
	}
	occampi_isetindent (stream, indent);
	fhandle_printf (stream, "</chook:ct_clienttype>\n");
	return;
}
/*}}}*/
/*{{{  static void occampi_ctservertype_chook_dumptree (tnode_t *node, void *chook, int indent, fhandle_t *stream)*/
/*
 *	dumps a ct_servertype chook (debugging)
 */
static void occampi_ctservertype_chook_dumptree (tnode_t *node, void *chook, int indent, fhandle_t *stream)
{
	occampi_isetindent (stream, indent);
	fhandle_printf (stream, "<chook:ct_servertype>\n");
	if (chook) {
		tnode_dumptree ((tnode_t *)chook, indent + 1, stream);
	}
	occampi_isetindent (stream, indent);
	fhandle_printf (stream, "</chook:ct_servertype>\n");
	return;
}
/*}}}*/

/*{{{  static void occampi_ctclienttrace_chook_dumptree (tnode_t *node, void *chook, int indent, fhandle_t *stream)*/
/*
 *	dumps a ct_clienttrace chook (debugging)
 */
static void occampi_ctclienttrace_chook_dumptree (tnode_t *node, void *chook, int indent, fhandle_t *stream)
{
	occampi_isetindent (stream, indent);
	fhandle_printf (stream, "<chook:ct_clienttrace>\n");
	if (chook) {
		tnode_dumptree ((tnode_t *)chook, indent + 1, stream);
	}
	occampi_isetindent (stream, indent);
	fhandle_printf (stream, "</chook:ct_clienttrace>\n");
	return;
}
/*}}}*/
/*{{{  static void *occampi_ctclienttrace_chook_copy (void *chook)*/
/*
 *	copies a ct_clienttrace chook
 */
static void *occampi_ctclienttrace_chook_copy (void *chook)
{
	return chook;
}
/*}}}*/
/*{{{  static void occampi_ctclienttrace_chook_free (void *chook)*/
/*
 *	frees a ct_clienttrace chook
 */
static void occampi_ctclienttrace_chook_free (void *chook)
{
	return;
}
/*}}}*/
/*{{{  static void occampi_ctservertrace_chook_dumptree (tnode_t *node, void *chook, int indent, fhandle_t *stream)*/
/*
 *	dumps a ct_servertrace chook (debugging)
 */
static void occampi_ctservertrace_chook_dumptree (tnode_t *node, void *chook, int indent, fhandle_t *stream)
{
	occampi_isetindent (stream, indent);
	fhandle_printf (stream, "<chook:ct_servertrace>\n");
	if (chook) {
		tnode_dumptree ((tnode_t *)chook, indent + 1, stream);
	}
	occampi_isetindent (stream, indent);
	fhandle_printf (stream, "</chook:ct_servertrace>\n");
	return;
}
/*}}}*/
/*{{{  static void *occampi_ctservertrace_chook_copy (void *chook)*/
/*
 *	copies a ct_servertrace chook
 */
static void *occampi_ctservertrace_chook_copy (void *chook)
{
	return chook;
}
/*}}}*/
/*{{{  static void occampi_ctservertrace_chook_free (void *chook)*/
/*
 *	frees a ct_servertrace chook
 */
static void occampi_ctservertrace_chook_free (void *chook)
{
	return;
}
/*}}}*/


/*{{{  static void occampi_initchantype_typedecl (tnode_t *node, codegen_t *cgen, void *arg)*/
/*
 *	does initialiser code-gen for a channel-type (variable) declaration (non-MOBILE)
 */
static void occampi_initchantype_typedecl (tnode_t *node, codegen_t *cgen, void *arg)
{
	tnode_t *chantype = (tnode_t *)arg;
	tnode_t *subtypelist = NameTypeOf (tnode_nthnameof (chantype, 0));
	int ws_off, vs_off, ms_off, ms_shdw;
	int i, nfields;
	tnode_t **fields;

	codegen_callops (cgen, debugline, node);

	cgen->target->be_getoffsets (node, &ws_off, &vs_off, &ms_off, &ms_shdw);

#if 0
fprintf (stderr, "occampi_initchantype_typedecl(): node=[%s], allocated at [%d,%d,%d], type is:\n", node->tag->name, ws_off, vs_off, ms_off);
tnode_dumptree (chantype, 1, stderr);
fprintf (stderr, "occampi_initchantype_typedecl(): subtypelist:\n");
tnode_dumptree (subtypelist, 1, stderr);
#endif
	if (!subtypelist || !parser_islistnode (subtypelist)) {
		tnode_error (node, "cannot generate chan-type initialiser for type [%s], no subtypelist\n", chantype->tag->name);
		nocc_internal ("occampi_initchantype_typedecl(): failing");
		return;
	}
	fields = parser_getlistitems (subtypelist, &nfields);
	for (i=0; i<nfields; i++) {
		if (fields[i] && (fields[i]->tag == opi.tag_FIELDDECL)) {
			tnode_t *fname = tnode_nthsubof (fields[i], 0);
			fielddecloffset_t *ofh = tnode_getchook (fname, fielddecloffset);

			codegen_callops (cgen, loadconst, 0);
			codegen_callops (cgen, storelocal, ws_off + ofh->offset);
		} else if (fields[i]) {
			tnode_warning (node, "occampi_initchantype_typedecl(): non FIELDDECL field [%s] in channel type [%s]", fields[i]->tag->name, chantype->tag->name);
		}
	}
	codegen_callops (cgen, comment, "initchantypedecl");

	return;
}
/*}}}*/


/*{{{  static int occampi_prescope_typedecl (compops_t *cops, tnode_t **nodep, prescope_t *ps)*/
/*
 *	called to prescope a type declaration (DATA TYPE ...)
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_prescope_typedecl (compops_t *cops, tnode_t **nodep, prescope_t *ps)
{
	tnode_t *type = tnode_nthsubof (*nodep, 1);

#if 0
fprintf (stderr, "occampi_prescope_typedecl(): here!, type is:\n");
tnode_dumptree (type, 1, stderr);
#endif
	if (parser_islistnode (type)) {
		/* remove any NULL items from the list */
		parser_cleanuplist (type);
	}

	return 1;
}
/*}}}*/
/*{{{  static int occampi_scopein_typedecl (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	called to scope a type declaration (DATA TYPE ...)
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_scopein_typedecl (compops_t *cops, tnode_t **node, scope_t *ss)
{
	tnode_t *name = tnode_nthsubof (*node, 0);
	tnode_t *type;
	char *rawname;
	name_t *sname = NULL;
	tnode_t *newname;
	void *namemark;

	if (name->tag != opi.tag_NAME) {
		scope_error (name, ss, "name not raw-name!");
		return 0;
	}
	rawname = (char*)tnode_nthhookof (name, 0);

#if 0
fprintf (stderr, "occampi_scopein_typedecl: here! rawname = \"%s\".  unscoped type=\n", rawname);
tnode_dumptree (tnode_nthsubof (*node, 1), 1, stderr);
#endif
	namemark = name_markscope ();

	/* scope subtype */
	if (scope_subtree (tnode_nthsubaddr (*node, 1), ss)) {
		return 0;
	}
	type = tnode_nthsubof (*node, 1);
#if 0
fprintf (stderr, "occampi_scopein_typedecl: here! rawname = \"%s\".  scoped type=\n", rawname);
tnode_dumptree (type, 1, stderr);
#endif

	/* if we have an intypedecl_scopein, do that here, followed by any scope-out */
#if 0
fprintf (stderr, "occampi_scopein_typedecl(): intypedecl_scopein cops = 0x%8.8x, compop? = %d\n", (unsigned int)cops,
		tnode_hascompop (cops, "intypedecl_scopein"));
#endif
	if (tnode_hascompop (cops, "intypedecl_scopein")) {
		tnode_callcompop (cops, "intypedecl_scopein", 2, node, ss);
	}

#if 0
fprintf (stderr, "occampi_scopein_typedecl(): here 1, in-scope:\n");
name_dumpnames (stderr);
#endif
	name_markdescope (namemark);

	sname = name_addscopename (rawname, *node, type, NULL);
	if ((*node)->tag == opi.tag_DATATYPEDECL) {
		newname = tnode_createfrom (opi.tag_NDATATYPEDECL, name, sname);
	} else if ((*node)->tag == opi.tag_CHANTYPEDECL) {
		newname = tnode_createfrom (opi.tag_NCHANTYPEDECL, name, sname);
	} else if ((*node)->tag == opi.tag_PROCTYPEDECL) {
		newname = tnode_createfrom (opi.tag_NPROCTYPEDECL, name, sname);
	} else {
		scope_error (name, ss, "unknown node type! [%s]", (*node)->tag->name);
		return 0;
	}
	SetNameNode (sname, newname);
	tnode_setnthsub (*node, 0, newname);
#if 0
fprintf (stderr, "occampi_scopein_typedecl(): here 2, in-scope:\n");
name_dumpnames (stderr);
#endif

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
	void **local = (void **)data;
	typedeclhook_t *tdh = (typedeclhook_t *)local[0];
	target_t *target = (target_t *)local[1];
	int *csizeptr = (int *)local[2];
	int this_ws;

	this_ws = tnode_bytesfor (node, target);
	if (this_ws > 0) {
		tdh->wssize += this_ws;
#if 0
fprintf (stderr, "occampi_prewalk_bytesfor_typedecl(): incrementing tdh->wssize\n");
#endif
		if (target && (tdh->wssize & (target->structalign - 1))) {
			/* pad */
			tdh->wssize += target->structalign;
			tdh->wssize &= ~(target->structalign - 1);
		}
		return 0;
	}
	return 1;
}
/*}}}*/
/*{{{  static int64_t occampi_bytesfor_typedecl (langops_t *lops, tnode_t *node, target_t *target)*/
/*
 *	returns the number of bytes required by a type declaration (DATA TYPE ...)
 */
static int64_t occampi_bytesfor_typedecl (langops_t *lops, tnode_t *node, target_t *target)
{
	typedeclhook_t *tdh = (typedeclhook_t *)tnode_nthhookof (node, 0);
	tnode_t *type = tnode_nthsubof (node, 1);
	int csize;
	void *local[3] = {(void *)tdh, (void *)target, (void *)&csize};

#if 0
fprintf (stderr, "occampi_bytesfor_typedecl(): tdh->wssize = %d\n", tdh->wssize);
#endif
	if (!tdh->wssize) {
		if ((node->tag == opi.tag_DATATYPEDECL) || (node->tag == opi.tag_CHANTYPEDECL)) {
			/*{{{  walk the type to find out its size*/
			tnode_prewalktree (type, occampi_prewalk_bytesfor_typedecl, (void *)local);

			if (!tdh->wssize) {
				nocc_error ("occampi_bytesfor_typedecl(): type has 0 size..  :(");
			}

			/*}}}*/
		} else if (node->tag == opi.tag_PROCTYPEDECL) {
			/*{{{  fixed size (at the moment)*/
			if (target) {
				tdh->wssize = target->pointersize;
			} else {
				tdh->wssize = 4;
			}

			/*}}}*/
		}
	}

#if 0
fprintf (stderr, "occampi_bytesfor_typedecl(): return size = %d.  type =\n", tdh->wssize);
tnode_dumptree (type, 1, stderr);
#endif
	return (int64_t)tdh->wssize;
}
/*}}}*/
/*{{{  static int occampi_typecheck_typedecl (compops_t *cops, tnode_t *node, typecheck_t *tc)*/
/*
 *	does type-checking for type declarations
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_typecheck_typedecl (compops_t *cops, tnode_t *node, typecheck_t *tc)
{
	tnode_t **typep = tnode_nthsubaddr (node, 1);

	if (node->tag == opi.tag_DATATYPEDECL) {
		/*{{{  check data-type declaration (allow anything as long as size known)*/
		if (*typep && !parser_islistnode (*typep)) {
			/* turn it into a list-node */
			*typep = parser_buildlistnode (NULL, *typep, NULL);
		}
		if (*typep) {
			tnode_t **items;
			int nitems, i;

			items = parser_getlistitems (*typep, &nitems);
			for (i=0; i<nitems; i++) {
				int isize;

				if (!items[i]) {
					nocc_warning ("occampi_typecheck_typedecl(): NULL item in list!");
					continue;
				} else if (items[i]->tag != opi.tag_FIELDDECL) {
					typecheck_error (items[i], tc, "field not FIELDDECL");
					continue;
				}

				typecheck_subtree (tnode_nthsubof (items[i], 1), tc);

				isize = tnode_bytesfor (tnode_nthsubof (items[i], 1), NULL);
				if (isize < 0) {
					typecheck_error (items[i], tc, "field has unknown size");
				}
			}
		}
		/*}}}*/
	} else if (node->tag == opi.tag_CHANTYPEDECL) {
		/*{{{  check chan-type declaration (only allow channels)*/
		if (*typep && !parser_islistnode (*typep)) {
			/* turn it into a list-node */
			*typep = parser_buildlistnode (NULL, *typep, NULL);
		}
		if (*typep) {
			tnode_t **items;
			int nitems, i;

			items = parser_getlistitems (*typep, &nitems);
			for (i=0; i<nitems; i++) {
				tnode_t *itype;
				
				if (!items[i]) {
					nocc_warning ("occampi_typecheck_typedecl(): NULL item in list!");
					continue;
				} else if (items[i]->tag != opi.tag_FIELDDECL) {
					typecheck_error (items[i], tc, "field not FIELDDECL");
					continue;
				}

				itype = tnode_nthsubof (items[i], 1);
				typecheck_subtree (itype, tc);

				if (itype->tag != opi.tag_CHAN) {
					typecheck_error (items[i], tc, "channel-type field not a channel");
				} else if (!tnode_getchook (itype, opi.chook_typeattr)) {
					typecheck_error (items[i], tc, "channel must have direction specified");
				}
			}
		}
		/*}}}*/
	} else if (node->tag == opi.tag_PROCTYPEDECL) {
		/*{{{  check proc-type declaration (only allow synchronisation objects)*/
		if (*typep && !parser_islistnode (*typep)) {
			/* turn it into a list-node */
			*typep = parser_buildlistnode (NULL, *typep, NULL);
		}
		if (*typep) {
			tnode_t **items;
			int nitems, i;

			items = parser_getlistitems (*typep, &nitems);
			for (i=0; i<nitems; i++) {
				tnode_t *itype;
				int f_ok = 0;
				
				if (!items[i]) {
					nocc_warning ("occampi_typecheck_typedecl(): NULL item in list!");
					continue;
				} else if (items[i]->tag != opi.tag_FPARAM) {
					typecheck_error (items[i], tc, "parameter not FPARAM");
					continue;
				}

				itype = tnode_nthsubof (items[i], 1);
				typecheck_subtree (itype, tc);

				if (tnode_ntflagsof (itype) & NTF_SYNCTYPE) {
					f_ok = 1;
				}

				if (!f_ok) {
					typecheck_error (items[i], tc, "parameter %d not a synchronisation object", i);
				}
			}
		}
		/*}}}*/
	}
	return 1;
}
/*}}}*/
/*{{{  static int occampi_namemap_typedecl (compops_t *cops, tnode_t **node, map_t *mdata)*/
/*
 *	does name mapping for a type declaration (allocates offsets in structured types)
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_namemap_typedecl (compops_t *cops, tnode_t **node, map_t *mdata)
{
	tnode_t *type = tnode_nthsubof (*node, 1);
	tnode_t **bodyp = tnode_nthsubaddr (*node, 2);

	if (((*node)->tag == opi.tag_DATATYPEDECL) || ((*node)->tag == opi.tag_CHANTYPEDECL)) {
		if (parser_islistnode (type)) {
			tnode_t **items;
			int nitems, i;
			int csize = 0;

			items = parser_getlistitems (type, &nitems);
			for (i=0; i<nitems; i++) {
				if (!items[i]) {
					continue;
				} else if (items[i]->tag != opi.tag_FIELDDECL) {
					nocc_error ("occampi_namemap_typedecl(): item in DATATYPEDECL not FIELDDECL, was [%s]", items[i]->tag->name);
				} else {
					tnode_t *fldname = tnode_nthsubof (items[i], 0);
					tnode_t *fldtype = tnode_nthsubof (items[i], 1);
					int tsize;

					tsize = tnode_bytesfor (fldtype, mdata->target);
					tnode_setchook (fldname, fielddecloffset, occampi_fielddecloffset_chook_create (csize));
					csize += tsize;

					if (csize & (mdata->target->structalign - 1)) {
						/* pad */
						csize += mdata->target->structalign;
						csize &= ~(mdata->target->structalign - 1);
					}
				}
			}
		}
	} else if ((*node)->tag == opi.tag_PROCTYPEDECL) {
		/* don't do anything, yet.. (probably want to add specials here) */
	} else {
		nocc_error ("occampi_namemap_typedecl(): don\'t know how to handle [%s]", (*node)->tag->name);
	}

	map_submapnames (bodyp, mdata);

	return 0;
}
/*}}}*/
/*{{{  static int occampi_precode_typedecl (compops_t *cops, tnode_t **nodep, codegen_t *cgen)*/
/*
 *	does pre-coding for a type declaration (the type declaration itself)
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_precode_typedecl (compops_t *cops, tnode_t **nodep, codegen_t *cgen)
{
	/* FIXME: might actually want to precode things burnt into type declarations at some point ..? [thinking type-descriptors] */

	codegen_subprecode (tnode_nthsubaddr (*nodep, 2), cgen);		/* precode in-scope body */
	return 0;
}
/*}}}*/
/*{{{  static int64_t occampi_usagecheck_typedecl (langops_t *lops, tnode_t *node, uchk_state_t *ucstate)*/
/*
 *	does usage-checking for a type declaration (dummy, because we don't want to check inside..)
 *	returns 0 to stop walk, 1 to continue
 */
static int64_t occampi_usagecheck_typedecl (langops_t *lops, tnode_t *node, uchk_state_t *ucstate)
{
	usagecheck_subtree (tnode_nthsubof (node, 2), ucstate);
	return 0;
}
/*}}}*/


/*{{{  static int occampi_constprop_arraynode (compops_t *cops, tnode_t **nodep)*/
/*
 *	does constant propagation on an array-node (finding constant dimensions)
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_constprop_arraynode (compops_t *cops, tnode_t **nodep)
{
	return 1;
}
/*}}}*/
/*{{{  static int occampi_namemap_arraynode (compops_t *cops, tnode_t **nodep, map_t *mdata)*/
/*
 *	dummy name-map for arraynodes
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_namemap_arraynode (compops_t *cops, tnode_t **nodep, map_t *mdata)
{
	return 0;
}
/*}}}*/
/*{{{  static int occampi_precode_arraynode (compops_t *cops, tnode_t **nodep, codegen_t *cgen)*/
/*
 *	dummy precode for arraynodes
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_precode_arraynode (compops_t *cops, tnode_t **nodep, codegen_t *cgen)
{
	return 0;
}
/*}}}*/
/*{{{  static tnode_t *occampi_typeactual_arraynode (langops_t *lops, tnode_t *formaltype, tnode_t *actualtype, tnode_t *node, typecheck_t *tc)*/
/*
 *	does type-compatibility checking for an ARRAY type
 *	if the formal type is open, actual type can be open or finite
 *	if the formal type is finite, actual type must be finite too (same dimension)
 *	returns the type actually used
 */
static tnode_t *occampi_typeactual_arraynode (langops_t *lops, tnode_t *formaltype, tnode_t *actualtype, tnode_t *node, typecheck_t *tc)
{
	tnode_t *fdim = tnode_nthsubof (formaltype, 0);
	tnode_t *adim = tnode_nthsubof (actualtype, 0);
	tnode_t *rtype = NULL;

	if (fdim && !adim) {
		if (node) {
			typecheck_error (node, tc, "actual type must have dimension specified");
		}
		return NULL;
	} else if (fdim && adim) {
		if (!langops_isconst (fdim) || !langops_isconst (adim)) {
			/* FIXME ? */
			nocc_warning ("occampi_typeactual_arraynode(): non-constant adim or fdim..");
		} else {
			long long fconst = 0LL;
			long long aconst = 0LL;
			int fdimval, adimval;

			fdimval = langops_constvalof (fdim, &fconst);
			adimval = langops_constvalof (adim, &aconst);

			if (fdimval != adimval) {
				typecheck_error (node, tc, "array dimensions are of different sizes");
			}
		}
	}

	/* check sub-types */
	rtype = typecheck_typeactual (tnode_nthsubof (formaltype, 1), tnode_nthsubof (actualtype, 1), node, tc);
	if (!rtype) {
		/* incompatible sub-types */
		return NULL;
	}

	return actualtype;
}
/*}}}*/
/*{{{  static int occampi_bytesfor_arraynode (langops_t *lops, tnode_t *node, target_t *target)*/
/*
 *	returns the number of bytes required by an array-node,
 *	of -1 if not known
 */
static int occampi_bytesfor_arraynode (langops_t *lops, tnode_t *node, target_t *target)
{
	tnode_t *dim = tnode_nthsubof (node, 0);
	tnode_t *subtype = tnode_nthsubof (node, 1);

	if (langops_isconst (dim)) {
		int subbytes = tnode_bytesfor (subtype, target);

		if (subbytes >= 0) {
			int stbytes = langops_constvalof (dim, NULL) * subbytes;

			return stbytes;
		}
	}

	/* don't know -- run-time */
	return -1;
}
/*}}}*/
/*{{{  static int64_t occampi_getdescriptor_arraynode (langops_t *lops, tnode_t *node, char **str)*/
/*
 *	gets the descriptor associated with an ARRAY node (usually producing the type)
 *	returns 0 to stop walk, 1 to continue
 */
static int64_t occampi_getdescriptor_arraynode (langops_t *lops, tnode_t *node, char **str)
{
	char *subtypestr = NULL;
	char *dimstr = NULL;
	tnode_t *dim = tnode_nthsubof (node, 0);

	langops_getdescriptor (tnode_nthsubof (node, 1), &subtypestr);
	if (dim) {
		langops_getdescriptor (dim, &dimstr);
	}

	if (*str) {
		sfree (*str);
	}

	if (!subtypestr) {
		subtypestr = string_dup ("?");
	}
	if (!dimstr) {
		/* could be an open dimension */
		dimstr = string_dup ("");
	}
	*str = (char *)smalloc (5 + strlen (dimstr) + strlen (subtypestr));

	sprintf (*str, "[%s]%s", dimstr, subtypestr);

	sfree (dimstr);
	sfree (subtypestr);

	return 0;
}
/*}}}*/
/*{{{  static tnode_t *occampi_getsubtype_arraynode (langops_t *lops, tnode_t *node, tnode_t *defaulttype)*/
/*
 *	returns the sub-type for an array type
 */
static tnode_t *occampi_getsubtype_arraynode (langops_t *lops, tnode_t *node, tnode_t *defaulttype)
{
	tnode_t *subtype = tnode_nthsubof (node, 1);

	return subtype;
}
/*}}}*/
/*{{{  static int64_t occampi_valbyref_arraynode (langops_t *lops, tnode_t *node)*/
/*
 *	returns non-zero if VALs of this type should be handled by reference (true for arrays)
 */
static int64_t occampi_valbyref_arraynode (langops_t *lops, tnode_t *node)
{
	return 1;
}
/*}}}*/
/*{{{  static tnode_t *occampi_dimtreeof_arraynode (langops_t *lops, tnode_t *node)*/
/*
 *	returns the dimension tree (list) for an array type
 */
static tnode_t *occampi_dimtreeof_arraynode (langops_t *lops, tnode_t *node)
{
	tnode_t *dimlist = (tnode_t *)tnode_getchook (node, opi.chook_arraydiminfo);
	tnode_t *orignode = node;
	
	if (!dimlist) {
		dimlist = parser_newlistnode (node->org);

		while (node->tag == opi.tag_ARRAY) {
			tnode_t *thisdim = tnode_nthsubof (node, 0);

			/* add this dimension and drop down to sub-type */
			parser_addtolist (dimlist, thisdim);
			node = tnode_nthsubof (node, 1);
		}

		tnode_setchook (orignode, opi.chook_arraydiminfo, dimlist);
	}

	return dimlist;
}
/*}}}*/
/*{{{  static tnode_t *occampi_hiddenparamsof_arraynode (langops_t *lops, tnode_t *node)*/
/*
 *	returns the hidden parameters (list) for an array type
 *	will generate a list of HIDDENDIMEN nodes as necessary
 */
static tnode_t *occampi_hiddenparamsof_arraynode (langops_t *lops, tnode_t *node)
{
	tnode_t *dimtree = langops_dimtreeof (node);
	tnode_t *hparams = parser_newlistnode (node->org);
	int i, nditems;
	tnode_t **dlist;

	if (!dimtree) {
		nocc_internal ("occampi_hiddenparamsof_arraynode(): got NULL dimension tree for array!");
		return NULL;
	}
#if 0
fprintf (stderr, "occampi_hiddenparamsof_arraynode(): dimension tree is:\n");
tnode_dumptree (dimtree, 1, stderr);
#endif

	dlist = parser_getlistitems (dimtree, &nditems);
	for (i=0; i<nditems; i++) {
		if (!dlist[i]) {
			/* unknown dimension */
			tnode_t *hparm = tnode_createfrom (opi.tag_HIDDENDIMEN, node,
					tnode_create (opi.tag_DIMSIZE, NULL, NULL, constprop_newconst (CONST_INT, NULL,
						tnode_create (opi.tag_INT, NULL), i), tnode_create (opi.tag_INT, NULL)));

			parser_addtolist (hparams, hparm);
			dlist[i] = hparm;
		}
	}
#if 0
fprintf (stderr, "occampi_hiddenparamsof_arraynode(): dimension tree updated to:\n");
tnode_dumptree (dimtree, 1, stderr);
#endif

	return hparams;
}
/*}}}*/
/*{{{  static int64_t occampi_hiddenslotsof_arraynode (langops_t *lops, tnode_t *node)*/
/*
 *	returns the number of hidden slots required by abbreviations or declarations of array types
 */
static int64_t occampi_hiddenslotsof_arraynode (langops_t *lops, tnode_t *node)
{
	tnode_t *dimtree = langops_dimtreeof (node);
	int i, nditems;
	tnode_t **dlist;
	int c = 0;

	if (!dimtree) {
		nocc_internal ("occampi_hiddenslotsof_arraynode(): got NULL dimension tree for array!");
		return 0;
	}

	dlist = parser_getlistitems (dimtree, &nditems);
	for (i=0; i<nditems; i++) {
		if (!dlist[i]) {
			/* unknown dimension, needs extra slot */
			c++;
		}
	}

	return (int64_t)c;
}
/*}}}*/
/*{{{  static int64_t occampi_istype_arraynode (langops_t *lops, tnode_t *node)*/
/*
 *	returns non-zero if the specified node is a type (always)
 */
static int64_t occampi_istype_arraynode (langops_t *lops, tnode_t *node)
{
	return 1;
}
/*}}}*/
/*{{{  static int64_t occampi_typetype_arraynode (langops_t *lops, tnode_t *node)*/
/*
 *	returns the type-category for an array type
 */
static int64_t occampi_typetype_arraynode (langops_t *lops, tnode_t *node)
{
	return (int64_t)(TYPE_DATA | TYPE_COMM | TYPE_ARRAY);
}
/*}}}*/


/*{{{  static int occampi_typecheck_arraymop (compops_t *cops, tnode_t *node, typecheck_t *tc)*/
/*
 *	does type-checking in an arraymopnode (that the argument is an array usually)
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_typecheck_arraymop (compops_t *cops, tnode_t *node, typecheck_t *tc)
{
	tnode_t *optype;

	/* do typecheck on operator */
	typecheck_subtree (tnode_nthsubof (node, 0), tc);

	optype = typecheck_gettype (tnode_nthsubof (node, 0), NULL);

	if (optype && (optype->tag != opi.tag_ARRAY)) {
		/* see if we can reduce this to an array */
		tnode_t *rdtype = typecheck_typereduce (optype);

		if (rdtype && (rdtype->tag == opi.tag_ARRAY)) {
			optype = rdtype;
		}
	}

	if (!optype || (optype->tag != opi.tag_ARRAY)) {
		typecheck_error (node, tc, "operand to %s is not an array!", node->tag->name);
		return 0;
	}
#if 0
fprintf (stderr, "occampi_typecheck_arraymop(): got optype =\n");
tnode_dumptree (optype, 1, stderr);
#endif

	return 0;
}
/*}}}*/
/*{{{  static tnode_t *occampi_gettype_arraymop (langops_t *lops, tnode_t *node, tnode_t *defaulttype)*/
/*
 *	gets type of an arraymopnode
 */
static tnode_t *occampi_gettype_arraymop (langops_t *lops, tnode_t *node, tnode_t *defaulttype)
{
	tnode_t *mytype;

	mytype = tnode_nthsubof (node, 1);
	if (!mytype) {
		mytype = tnode_create (opi.tag_INT, NULL);
		tnode_setnthsub (node, 1, mytype);
	}

	return mytype;
}
/*}}}*/
/*{{{  static int occampi_constprop_arraymop (compops_t *cops, tnode_t **nodep)*/
/*
 *	does constant propagation on an arraymopnode
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_constprop_arraymop (compops_t *cops, tnode_t **nodep)
{
	tnode_t *op = tnode_nthsubof (*nodep, 0);
	tnode_t *rtype = tnode_nthsubof (*nodep, 1);
	tnode_t *type = typecheck_gettype (op, NULL);

	if (!type) {
		nocc_internal ("occampi_constprop_arraymop(): no type on arraymop!");
		return 0;
	}
#if 0
fprintf (stderr, "occampi_constprop_arraymop(): type is\n");
tnode_dumptree (type, 1, stderr);
fprintf (stderr, "occampi_constprop_arraymop(): rtype is\n");
tnode_dumptree (rtype, 1, stderr);
#endif

	if (type->tag != opi.tag_ARRAY) {
		/* see if we can reduce it (should be able to, got through typecheck) */
		tnode_t *rdtype = typecheck_typereduce (type);

		if (rdtype && (rdtype->tag == opi.tag_ARRAY)) {
			type = rdtype;
		}
	}

	if (type->tag != opi.tag_ARRAY) {
		constprop_error (*nodep, "operand is not an array! [%s]", type->tag->name);
		return 0;
	}
	if (langops_isconst (tnode_nthsubof (type, 0))) {
		/* constant dimension! */
		int dim = langops_constvalof (tnode_nthsubof (type, 0), NULL);

#if 0
fprintf (stderr, "occampi_constprop_arraymop(): constant dimension! = %d\n", dim);
#endif
		*nodep = constprop_newconst (CONST_INT, *nodep, NULL, dim);
	}
	return 1;
}
/*}}}*/
/*{{{  static int occampi_fetrans_arraymop (compops_t *cops, tnode_t **nodep, fetrans_t *fe)*/
/*
 *	does front-end transformations on an arraymopnode
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_fetrans_arraymop (compops_t *cops, tnode_t **nodep, fetrans_t *fe)
{
	tnode_t *optype = typecheck_gettype (tnode_nthsubof (*nodep, 0), NULL);
	tnode_t *dimtree;

	if (!optype) {
		nocc_internal ("occampi_fetrans_arraymop(): NULL operand type!");
		return 0;
	}
#if 0
fprintf (stderr, "occampi_fetrans_arraymop(): operand type:\n");
tnode_dumptree (optype, 1, stderr);
#endif
	dimtree = langops_dimtreeof_node (optype, tnode_nthsubof (*nodep, 0));
	if (!dimtree) {
		nocc_internal ("occampi_fetrans_arraymop(): NULL dimension tree for array operand!");
		return 0;
	}

	if ((*nodep)->tag == opi.tag_SIZE) {
#if 0
fprintf (stderr, "occampi_fetrans_arraymop(): SIZE: dimension-tree of operand:\n");
tnode_dumptree (dimtree, 1, stderr);
#endif
		/* always going to be the first dimension */
		*nodep = parser_getfromlist (dimtree, 0);
	} else {
		nocc_internal ("occampi_fetrans_arraymop(): unexpected operator [%s]", (*nodep)->tag->name);
	}

	return 1;
}
/*}}}*/
/*{{{  static int occampi_premap_arraymop (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	does pre-mapping for an arraymopnode
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_premap_arraymop (compops_t *cops, tnode_t **node, map_t *map)
{
	/* pre-map operand */
	map_subpremap (tnode_nthsubaddr (*node, 0), map);

	/* create a new result for it */
	*node = map->target->newresult (*node, map);

	return 0;
}
/*}}}*/
/*{{{  static int occampi_namemap_arraymop (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	does name-mapping for an arraymopnode
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_namemap_arraymop (compops_t *cops, tnode_t **node, map_t *map)
{
	/* name-map operand */
	map_submapnames (tnode_nthsubaddr (*node, 0), map);

	/* add operand to result */
	map_addtoresult (tnode_nthsubaddr (*node, 0), map);

	return 0;
}
/*}}}*/
/*{{{  static int occampi_codegen_arraymop (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	called to do code-generation for an arraymopnode
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_codegen_arraymop (compops_t *cops, tnode_t *node, codegen_t *cgen)
{
	int i;
	tnode_t *op = tnode_nthsubof (node, 0);

	codegen_callops (cgen, comment, "FIXME: arraymop code");

#if 1
fprintf (stderr, "occampi_codegen_arraymop(): op =\n");
tnode_dumptree (op, 1, FHAN_STDERR);
#endif

	return 0;
}
/*}}}*/
/*{{{  static int occampi_iscomplex_arraymop (langops_t *lops, tnode_t *node, int deep)*/
/*
 *	returns non-zero if the monadic array operator is complex
 */
static int occampi_iscomplex_arraymop (langops_t *lops, tnode_t *node, int deep)
{
	int i = 0;

	if (deep) {
		i = langops_iscomplex (tnode_nthsubof (node, 0), deep);
	}

	return i;
}
/*}}}*/


/*{{{  static int occampi_scopein_fielddecl (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	called to scope in a field declaration (inside a DATA TYPE)
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_scopein_fielddecl (compops_t *cops, tnode_t **node, scope_t *ss)
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

	/*
	 * Changed, frmb (29/03/2008): whatever encloses the field will mark scope and descope afterwards;
	 * needed so that we can capture fieldnames in the whatever declaration for other processing
	 * (e.g. TRACES scoping)
	 */
	/* and descope immediately */
	/* name_descopename (sname); */

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
/*{{{  static int occampi_bytesfor_fielddecl (langops_t *lops, tnode_t *node, target_t *target)*/
/*
 *	returns the number of bytes required by a FIELDDECL
 */
static int occampi_bytesfor_fielddecl (langops_t *lops, tnode_t *node, target_t *target)
{
	tnode_t *type = tnode_nthsubof (node, 1);
	int bytes = tnode_bytesfor (type, target);

#if 0
fprintf (stderr, "occampi_bytesfor_fielddecl(): bytes = %d, type =\n", bytes);
tnode_dumptree (type, 1, stderr);
#endif
	return bytes;
}
/*}}}*/


/*{{{  static int occampi_scopein_subscript (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	called to scope a subscript node -- turns into an ARRAYSUB or RECORDSUB as appropriate
 *	return 0 to stop walk, 1 to continue
 */
static int occampi_scopein_subscript (compops_t *cops, tnode_t **node, scope_t *ss)
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

#if 0
fprintf (stderr, "occampi_scopein_subscript(): got type from NAMENODE, base =\n");
tnode_dumptree (base, 1, stderr);
fprintf (stderr, "type =\n");
tnode_dumptree (type, 1, stderr);
#endif
		if (!type) {
			/* assume arraysub */
			scope_subtree (tnode_nthsubaddr (*node, 1), ss);

			*node = tnode_createfrom (opi.tag_ARRAYSUB, oldnode, tnode_nthsubof (oldnode, 0), tnode_nthsubof (oldnode, 1), tnode_nthsubof (oldnode, 2));
			tnode_setnthsub (oldnode, 0, NULL);
			tnode_setnthsub (oldnode, 1, NULL);
			tnode_free (oldnode);

			return 0;
		}
		if (type->tag == opi.tag_TYPESPEC) {
			/* step over it */
			type = tnode_nthsubof (type, 0);
		}

		if ((type->tag == opi.tag_NCHANTYPEDECL) || (type->tag == opi.tag_NDATATYPEDECL)) {
			void *namemarker;

			namemarker = name_markscope ();
			tnode_prewalktree (NameTypeOf (tnode_nthnameof (type, 0)), occampi_fielddecl_prewalk_scopefields, (void *)ss);

			/* fields should be in scope, try subscript */
			scope_subtree (tnode_nthsubaddr (*node, 1), ss);
#if 0
fprintf (stderr, "occampi_scopein_subscript(): scoped subscript, *node =\n");
tnode_dumptree (*node, 1, stderr);
#endif
			*node = tnode_createfrom (opi.tag_RECORDSUB, oldnode, tnode_nthsubof (oldnode, 0), tnode_nthsubof (oldnode, 1), tnode_nthsubof (oldnode, 2));
			tnode_setnthsub (oldnode, 0, NULL);
			tnode_setnthsub (oldnode, 1, NULL);
			tnode_free (oldnode);

			name_markdescope (namemarker);
		} else {
			/* probably a simple type */
			scope_subtree (tnode_nthsubaddr (*node, 1), ss);

			*node = tnode_createfrom (opi.tag_ARRAYSUB, oldnode, tnode_nthsubof (oldnode, 0), tnode_nthsubof (oldnode, 1), tnode_nthsubof (oldnode, 2));
			tnode_setnthsub (oldnode, 0, NULL);
			tnode_setnthsub (oldnode, 1, NULL);
			tnode_free (oldnode);
		}
	}

	return 0;
}
/*}}}*/
/*{{{  static int occampi_typecheck_subscript (compops_t *cops, tnode_t *node, typecheck_t *tc)*/
/*
 *	does type-checking on a subscript (makes sure ARRAYSUB base is integer)
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_typecheck_subscript (compops_t *cops, tnode_t *node, typecheck_t *tc)
{
	if (node->tag == opi.tag_ARRAYSUB) {
		tnode_t *base = tnode_nthsubof (node, 0);
		tnode_t *atype = typecheck_gettype (base, NULL);
		tnode_t *stype;

		if (atype->tag != opi.tag_ARRAY) {
			/* see if what we've got can be reduced into an array */
			tnode_t *rdtype = typecheck_typereduce (atype);

			if (rdtype && (rdtype->tag == opi.tag_ARRAY)) {
				atype = rdtype;
			}
		}

		if (atype->tag == opi.tag_ARRAY) {
			/* walk through and get-type again */
			atype = tnode_nthsubof (atype, 1);
			stype = typecheck_gettype (atype, NULL);
		} else {
			nocc_internal ("occampi_gettype_subscript(): ARRAYSUB on non-ARRAY not properly implemented yet!");
			stype = NULL;
		}
		tnode_setnthsub (node, 2, stype);
	}
	return 1;
}
/*}}}*/
/*{{{  static int occampi_constprop_subscript (compops_t *cops, tnode_t **nodep)*/
/*
 *	does constant-propagation on a subscript (for constant arrays)
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_constprop_subscript (compops_t *cops, tnode_t **nodep)
{
	return 1;
}
/*}}}*/
/*{{{  static int occampi_namemap_subscript (compops_t *cops, tnode_t **node, map_t *mdata)*/
/*
 *	name-maps a subscript-node, turning it into a back-end INDEXED node
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_namemap_subscript (compops_t *cops, tnode_t **node, map_t *mdata)
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
		int subtypesize = tnode_bytesfor (tnode_nthsubof (*node, 2), mdata->target);

#if 0
fprintf (stderr, "occampi_namemap_subscript(): ARRAYSUB: subtypesize=%d, *node[2] = 0x%8.8x = \n", subtypesize, (unsigned int)tnode_nthsubof (*node, 2));
if (tnode_nthsubof (*node, 2)) {
	tnode_dumptree (tnode_nthsubof (*node, 2), 1, stderr);
} else {
	fprintf (stderr, "    <nullnode />\n");
}
#endif
		*node = mdata->target->newindexed (tnode_nthsubof (*node, 0), tnode_nthsubof (*node, 1), subtypesize, 0);
	} else {
		nocc_error ("occampi_namemap_subscript(): unsupported subscript type [%s]", (*node)->tag->name);
		return 0;
	}
	return 1;
}
/*}}}*/
/*{{{  static tnode_t *occampi_gettype_subscript (langops_t *lops, tnode_t *node, tnode_t *defaulttype)*/
/*
 *	called to get the type of a subscript
 */
static tnode_t *occampi_gettype_subscript (langops_t *lops, tnode_t *node, tnode_t *defaulttype)
{
	if (node->tag == opi.tag_RECORDSUB) {
		/*{{{  type is that of the field*/
		tnode_t *base = tnode_nthsubof (node, 0);
		tnode_t *basetype;
		tnode_t *field = tnode_nthsubof (node, 1);
		name_t *fldname;
		tnode_t *fldtype, *chantype = NULL;
		chook_t *ct_chook = NULL;

		basetype = typecheck_gettype (base, NULL);
		if (basetype && (basetype->tag == opi.tag_TYPESPEC)) {
			occampi_typeattr_t tattr = occampi_typeattrof (basetype);

			chantype = tnode_nthsubof (basetype, 0);
			if (chantype->tag == opi.tag_NCHANTYPEDECL) {
				/* base is a named channel-type, might need to invert direction on fields */
				if (tattr & TYPEATTR_MARKED_IN) {
					ct_chook = ct_servertype;
				} else if (tattr & TYPEATTR_MARKED_OUT) {
					ct_chook = ct_clienttype;
				}
				chantype = NameDeclOf (tnode_nthnameof (chantype, 0));
#if 0
fprintf (stderr, "occampi_gettype_subscript(): of a channel-type type, chantype (decl) =\n");
tnode_dumptree (chantype, 1, stderr);
#endif
			} else {
				/* something else */
				chantype = NULL;
			}
		}
#if 0
fprintf (stderr, "ocacmpi_gettype_subscript(): basetype =\n");
tnode_dumptree (basetype, 1, stderr);
#endif
		if (field->tag != opi.tag_NFIELD) {
			return NULL;
		}
		fldname = tnode_nthnameof (field, 0);

		if (chantype && ct_chook) {
			/*{{{  it's a channel-type field*/
			tnode_t *fdecl = NameDeclOf (fldname);

			if (fdecl->tag != opi.tag_FIELDDECL) {
				nocc_internal ("occampi_gettype_subscript(): expected FIELDDECL for NFIELD, got [%s]", fdecl->tag->name);
			}
			fldtype = (tnode_t *)tnode_getchook (fdecl, ct_chook);
			if (!fldtype) {
				/*{{{  no field type yet, create one*/
				fldtype = tnode_copytree (NameTypeOf (fldname));
#if 0
fprintf (stderr, "occampi_gettype_subscript(): chan-type field, fldtype (copy) =\n");
tnode_dumptree (fldtype, 1, stderr);
#endif
				if (ct_chook == ct_clienttype) {
					/*{{{  invert direction on the field's type*/
					occampi_typeattr_t tattr;

					if (fldtype->tag != opi.tag_CHAN) {
						nocc_internal ("occampi_gettype_subscript(): expected CHAN as type for NFIELD, got [%s]", fldtype->tag->name);
					}
					tattr = occampi_typeattrof (fldtype);
					tattr ^= (TYPEATTR_MARKED_IN | TYPEATTR_MARKED_OUT);
					occampi_settypeattr (fldtype, tattr);
					/*}}}*/
				}
				tnode_setchook (fdecl, ct_chook, (void *)fldtype);
				/*}}}*/
			}
			/*}}}*/
		} else {
			fldtype = NameTypeOf (fldname);
		}

#if 0
fprintf (stderr, "occampi_gettype_subscript(): for [%s], returning:\n", node->tag->name);
tnode_dumptree (fldtype, 1, stderr);
#endif
		return fldtype;
		/*}}}*/
	} else if (node->tag == opi.tag_ARRAYSUB) {
		/*{{{  type is that of the base minus one ARRAY*/
		tnode_t *base = tnode_nthsubof (node, 0);
		tnode_t *atype = typecheck_gettype (base, NULL);
		tnode_t *stype = defaulttype;

		if (atype->tag != opi.tag_ARRAY) {
			/* see if what we've got can be reduced into an array */
			tnode_t *rdtype = typecheck_typereduce (atype);

			if (rdtype && (rdtype->tag == opi.tag_ARRAY)) {
				atype = rdtype;
			}
		}

		if (atype->tag == opi.tag_ARRAY) {
			/* walk through and get-type again */
			atype = tnode_nthsubof (atype, 1);
			stype = typecheck_gettype (atype, defaulttype);
		} else {
			nocc_internal ("occampi_gettype_subscript(): ARRAYSUB on non-ARRAY not properly implemented yet!");
		}
		return stype;
		/*}}}*/
	}
	/* else don't know.. */
	return defaulttype;
}
/*}}}*/
/*{{{  static int occampi_iscomplex_subscript (langops_t *lops, tnode_t *node, int deep)*/
/*
 *	called to determine whether a subscript is "complex"
 */
static int occampi_iscomplex_subscript (langops_t *lops, tnode_t *node, int deep)
{
	if (!langops_isconst (tnode_nthsubof (node, 1))) {
		/* non-constant index, complex */
		return 1;
	}
	if (deep) {
		/* complexity depends on the base */
		return langops_iscomplex (tnode_nthsubof (node, 0), deep);
	}
	/* else trivial */
	return 0;
}
/*}}}*/
/*{{{  static int occampi_isvar_subscript (langops_t *lops, tnode_t *node)*/
/*
 *	returns non-zero if the given subscript is a variable (l-value)
 */
static int occampi_isvar_subscript (langops_t *lops, tnode_t *node)
{
	/* variable if the base is */
	return langops_isvar (tnode_nthsubof (node, 0));
}
/*}}}*/
/*{{{  static tnode_t *occampi_getbasename_subscript (langops_t *lops, tnode_t *node)*/
/*
 *	returns the "base" of a given subscript
 */
static tnode_t *occampi_getbasename_subscript (langops_t *lops, tnode_t *node)
{
	return tnode_nthsubof (node, 0);
}
/*}}}*/
/*{{{  static tnode_t *occampi_getfieldname_subscript (langops_t *lops, tnode_t *node)*/
/*
 *	returns the "field name" of a given subscript, either index or name
 */
static tnode_t *occampi_getfieldname_subscript (langops_t *lops, tnode_t *node)
{
	return tnode_nthsubof (node, 1);
}
/*}}}*/


/*{{{  static int occampi_typecheck_slice (compops_t *cops, tnode_t *node, typecheck_t *tc)*/
/*
 *	does type-checking for an array slice
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_typecheck_slice (compops_t *cops, tnode_t *node, typecheck_t *tc)
{
	tnode_t *definttype = tnode_create (opi.tag_INT, NULL);
	tnode_t *itype, *atype;
	tnode_t *mytype = NULL;

	/* FIXME: needs more I think? */

	/* type-check sub-trees */
	typecheck_subtree (tnode_nthsubof (node, 0), tc);
	typecheck_subtree (tnode_nthsubof (node, 1), tc);
	typecheck_subtree (tnode_nthsubof (node, 2), tc);

	atype = typecheck_gettype (tnode_nthsubof (node, 0), NULL);
#if 0
fprintf (stderr, "occampi_typecheck_slice(): type-checked base, got:\n");
tnode_dumptree (atype, 1, stderr);
#endif
	if (!atype || (atype->tag != opi.tag_ARRAY)) {
		typecheck_error (node, tc, "base of slice must be an array");
	} else {
		mytype = tnode_create (opi.tag_ARRAY, NULL, NULL, tnode_copytree (tnode_nthsubof (atype, 1)));
	}

	itype = typecheck_gettype (tnode_nthsubof (node, 1), definttype);
	if (!typecheck_typeactual (definttype, itype, node, tc)) {
		typecheck_error (node, tc, "start expression in slice must be integer");
	}

	itype = typecheck_gettype (tnode_nthsubof (node, 2), definttype);
	if (!typecheck_typeactual (definttype, itype, node, tc)) {
		typecheck_error (node, tc, "length expression in slice must be integer");
	}

	if (mytype && !tnode_nthsubof (node, 3)) {
		tnode_setnthsub (node, 3, mytype);
		tnode_setnthsub (mytype, 0, tnode_copytree (tnode_nthsubof (node, 2)));
#if 0
fprintf (stderr, "occampi_typecheck_slice(): setting slice type to:\n");
tnode_dumptree (mytype, 1, stderr);
#endif
	}

	tnode_free (definttype);
	return 0;
}
/*}}}*/
/*{{{  static int occampi_namemap_slice (compops_t *cops, tnode_t **nodep, map_t *map)*/
/*
 *	does name-mapping for an array slice
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_namemap_slice (compops_t *cops, tnode_t **nodep, map_t *map)
{
	tnode_t *slicetype = tnode_nthsubof (*nodep, 3);
	int subtypesize = tnode_bytesfor (tnode_nthsubof (slicetype, 1), map->target);

	map_submapnames (tnode_nthsubaddr (*nodep, 0), map);
	map_submapnames (tnode_nthsubaddr (*nodep, 1), map);
	map_submapnames (tnode_nthsubaddr (*nodep, 2), map);

	*nodep = map->target->newindexed (tnode_nthsubof (*nodep, 0), tnode_nthsubof (*nodep, 1), subtypesize, 0);
	return 0;
}
/*}}}*/
/*{{{  static tnode_t *occampi_gettype_slice (langops_t *lops, tnode_t *node, tnode_t *defaulttype)*/
/*
 *	gets the type of an array slice
 */
static tnode_t *occampi_gettype_slice (langops_t *lops, tnode_t *node, tnode_t *defaulttype)
{
	tnode_t *type = tnode_nthsubof (node, 3);

	if (type) {
		return type;
	}
	return defaulttype;
}
/*}}}*/
/*{{{  static int occampi_iscomplex_slice (langops_t *lops, tnode_t *node, int deep)*/
/*
 *	returns non-zero if the array slice is complex
 */
static int occampi_iscomplex_slice (langops_t *lops, tnode_t *node, int deep)
{
	return 1;		/* assume these are complex */
}
/*}}}*/
/*{{{  static tnode_t *occampi_dimtreeof_slice (langops_t *lops, tnode_t *node)*/
/*
 *	returns the dimension tree of an array-slice
 */
static tnode_t *occampi_dimtreeof_slice (langops_t *lops, tnode_t *node)
{
	tnode_t *type = tnode_nthsubof (node, 3);

#if 0
fprintf (stderr, "occampi_dimtreeof_slice(): node is:\n");
tnode_dumptree (node, 1, stderr);
#endif
	if (type) {
		return langops_dimtreeof (type);
	}
	return NULL;
}
/*}}}*/


/*{{{  static tnode_t *occampi_gettype_nametypenode (langops_t *lops, tnode_t *node, tnode_t *default_type)*/
/*
 *	returns the type of a named type-node (trivial)
 */
static tnode_t *occampi_gettype_nametypenode (langops_t *lops, tnode_t *node, tnode_t *default_type)
{
	name_t *name = tnode_nthnameof (node, 0);

	if (!name) {
		nocc_fatal ("occampi_gettype_nametypenode(): NULL name!");
		return NULL;
	}
	if (NameTypeOf (name)) {
#if 0
fprintf (stderr, "occmpi_gettype_nametypenode(): node = [%s], name:\n", node->tag->name);
name_dumpname (name, 1, stderr);
fprintf (stderr, "   \"   name->type:\n");
tnode_dumptree (name->type, 1, stderr);
#endif
		return NameTypeOf (name);
	}
#if 0
nocc_message ("occampi_gettype_nametypenode(): null type on name, node was:");
tnode_dumptree (node, 4, stderr);
#endif
	nocc_fatal ("occampi_gettype_nametypenode(): name has NULL type (FIXME!)");
	return NULL;
}
/*}}}*/
/*{{{  static int occampi_bytesfor_nametypenode (langops_t *lops, tnode_t *node, target_t *target)*/
/*
 *	returns the number of bytes in a named-type-node, associated with its type only
 */
static int occampi_bytesfor_nametypenode (langops_t *lops, tnode_t *node, target_t *target)
{
	if ((node->tag == opi.tag_NDATATYPEDECL) || (node->tag == opi.tag_NCHANTYPEDECL)) {
		name_t *name = tnode_nthnameof (node, 0);
		tnode_t *type, *decl;

		type = NameTypeOf (name);
		decl = NameDeclOf (name);
#if 0
fprintf (stderr, "occampi_bytesfor_nametypenode(): type = ");
tnode_dumptree (type, 1, stderr);
#endif

		return tnode_bytesfor (decl, target);
	} else if (node->tag == opi.tag_NFIELD) {
		name_t *name = tnode_nthnameof (node, 0);
		tnode_t *type = NameTypeOf (name);

#if 0
fprintf (stderr, "occampi_bytesfor_nametypenode(): [N_FIELD], type =\n");
tnode_dumptree (type, 1, stderr);
#endif
		return tnode_bytesfor (type, target);
	}

	nocc_error ("occampi_bytesfor_nametypenode(): no bytes for [%s]", node->tag->name);
	return -1;
}
/*}}}*/
/*{{{  static int occampi_namemap_nametypenode (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	transforms given type-name into a back-end name
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_namemap_nametypenode (compops_t *cops, tnode_t **node, map_t *map)
{
	tnode_t *bename = tnode_getchook (*node, map->mapchook);
	tnode_t *tname;

#if 1
fprintf (stderr, "occampi_namemap_nametypenode(): here 1! bename =\n");
tnode_dumptree (bename, 1, FHAN_STDERR);
#endif
	if (bename) {
		tname = map->target->newnameref (bename, map);
		*node = tname;
	}
	return 0;
}
/*}}}*/
/*{{{  static int occampi_getname_nametypenode (langops_t *lops, tnode_t *node, char **str)*/
/*
 *	gets the name of a named-type-node (data-type name, etc.)
 *	return 0 on success, -ve on failure
 */
static int occampi_getname_nametypenode (langops_t *lops, tnode_t *node, char **str)
{
	char *pname = NameNameOf (tnode_nthnameof (node, 0));

	if (*str) {
		sfree (*str);
	}
	*str = (char *)smalloc (strlen (pname) + 2);
	strcpy (*str, pname);

	return 0;
}
/*}}}*/
/*{{{  static int occampi_initialising_decl_nametypenode (langops_t *lops, tnode_t *t, tnode_t *benode, map_t *mdata)*/
/*
 *	does initialising declarations for user-defined types (DATA TYPE, CHAN TYPE, ...)
 *	returns 0 if nothing needed, non-zero otherwise
 */
static int occampi_initialising_decl_nametypenode (langops_t *lops, tnode_t *t, tnode_t *benode, map_t *mdata)
{
#if 0
name_t *name = tnode_nthnameof (t, 0);
tnode_t *realtype = NameDeclOf (name);

fprintf (stderr, "occampi_initialising_decl_nametypenode(): t = [%s], realtype = [%s], back-end-node is:\n", t->tag->name, realtype ? realtype->tag->name : "null");
tnode_dumptree (benode, 4, stderr);
#endif
	if (t->tag == opi.tag_NCHANTYPEDECL) {
		codegen_setinithook (benode, occampi_initchantype_typedecl, (void *)t);
		return 1;
	}
	return 0;
}
/*}}}*/
/*{{{  static int occampi_istype_nametypenode (langops_t *lops, tnode_t *node)*/
/*
 *	returns non-zero if the specified node is a type
 */
static int occampi_istype_nametypenode (langops_t *lops, tnode_t *node)
{
	if (node->tag == opi.tag_NDATATYPEDECL) {
		return 1;
	} else if (node->tag == opi.tag_NFIELD) {
		return 0;
	} else if (node->tag == opi.tag_NCHANTYPEDECL) {
		return 1;
	} else if (node->tag == opi.tag_NPROCTYPEDECL) {
		return 1;
	}
	return 0;
}
/*}}}*/
/*{{{  static int occampi_typehash_nametypenode (langops_t *lops, tnode_t *node, int hsize, void *ptr)*/
/*
 *	generates a type-hash for a named type
 *	returns 0 on success, non-zero on failure
 */
static int occampi_typehash_nametypenode (langops_t *lops, tnode_t *node, int hsize, void *ptr)
{
	char *pname = NameNameOf (tnode_nthnameof (node, 0));
	unsigned int myhash = 0;
	tnode_t *subtype = NameTypeOf (tnode_nthnameof (node, 0));

	langops_typehash_blend (hsize, ptr, strlen (pname), (void *)pname);

	if (node->tag == opi.tag_NDATATYPEDECL) {
		myhash = 0x5363;
	} else if (node->tag == opi.tag_NFIELD) {
		myhash = 0x693;
	} else if (node->tag == opi.tag_NCHANTYPEDECL) {
		myhash = 0x42503;
	} else if (node->tag == opi.tag_NPROCTYPEDECL) {
		myhash = 0xa3608d;
	} else {
		nocc_serious ("occampi_typehash_nametypenode(): unknown node (%s,%s)", node->tag->name, node->tag->ndef->name);
		return 1;
	}
#if 0
fprintf (stderr, "occampi_typehash_nametypenode(): FIXME: subtype needs including, got:\n");
tnode_dumptree (subtype, 1, stderr);
#endif

	return 0;
}
/*}}}*/
/*{{{  static tnode_t *occampi_gettags_nametypenode (langops_t *lops, tnode_t *node)*/
/*
 *	returns the list of tags associated with a named-type node, or NULL if none
 */
static tnode_t *occampi_gettags_nametypenode (langops_t *lops, tnode_t *node)
{
	if (node->tag == opi.tag_NFIELD) {
		name_t *name = tnode_nthnameof (node, 0);
		tnode_t *type = NameTypeOf (name);

		return langops_gettags (type);
	}
	return NULL;
}
/*}}}*/
/*{{{  static tnode_t *occampi_tracespecof_nametypenode (langops_t *lops, tnode_t *node)*/
/*
 *	returns the traces-specification associated with a named channel-type
 *	returns NULL if none
 */
static tnode_t *occampi_tracespecof_nametypenode (langops_t *lops, tnode_t *node)
{
	name_t *name = tnode_nthnameof (node, 0);
	tnode_t *decl = NameDeclOf (name);
	tnode_t *trs = (tnode_t *)tnode_getchook (decl, cttrace_chook);

#if 0
fprintf (stderr, "occampi_tracespecof_nametypenode(): declaration of CHANTYPEDECL name is:\n");
tnode_dumptree (decl, 1, stderr);
#endif

	return trs;
}
/*}}}*/


/*{{{  static tnode_t *occampi_tracespecof_cttypespecnode (langops_t *lops, tnode_t *node)*/
/*
 *	returns the traces associated with a type-spec node -- used to modify traces on a CHAN TYPE (client and server ends),
 *	or NULL if none
 */
static tnode_t *occampi_tracespecof_cttypespecnode (langops_t *lops, tnode_t *node)
{
	occampi_typeattr_t tattr = occampi_typeattrof (node);
	tnode_t *sub = tnode_nthsubof (node, 0);
	tnode_t *trs = NULL;

#if 0
fprintf (stderr, "occampi_tracespecof_cttypespecnode(): here!, sub is [%s]\n", sub->tag->name);
#endif
	if (sub->tag == opi.tag_NCHANTYPEDECL) {
		/*{{{  possibly have some traces attached to the CHAN TYPE declaration*/
		trs = tracescheck_tracespecof (sub);

		if (trs) {
#if 1
fprintf (stderr, "occampi_tracespecof_cttypespecnode(): got plain traces on CHAN TYPE decl\n");
// tnode_dumptree (trs, 1, stderr);
#endif
		}
		/*}}}*/
	}
	return trs;
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
/*{{{  static void occampi_reduce_arrayfold (dfastate_t *dfast, parsepriv_t *pp, void *rarg)*/
/*
 *	takes a declaration of some kind and an ARRAY on the node-stack, and folds
 *	the ARRAY into the declaration's type
 */
static void occampi_reduce_arrayfold (dfastate_t *dfast, parsepriv_t *pp, void *rarg)
{
	tnode_t *decl, *array;

	decl = dfa_popnode (dfast);
	array = dfa_popnode (dfast);

#if 0
fprintf (stderr, "occampi_reduce_arrayfold(): decl =\n");
tnode_dumptree (decl, 4, stderr);
fprintf (stderr, "occampi_reduce_arrayfold(): array =\n");
tnode_dumptree (array, 4, stderr);
#endif
	if (!array) {
		parser_error (SLOCN (pp->lf), "broken array specification");
	} else {
		tnode_t **typep = tnode_nthsubaddr (decl, 1);

		if ((*typep)->tag == opi.tag_FUNCTIONTYPE) {
			/* put array inside FUNCTIONTYPE results */
			typep = tnode_nthsubaddr (*typep, 0);
		}

		tnode_setnthsub (array, 1, *typep);
		*typep = array;
	}
	*(dfast->ptr) = decl;

	return;
}
/*}}}*/
/*{{{  static void occampi_reduce_valarrayfold (dfastate_t *dfast, parsepriv_t *pp, void *rarg)*/
/*
 *	takes a non-VAL declaration of some kind and an ARRAY on the node-stack, and folds
 *	the ARRAY into the VALFPARAM's type
 */
static void occampi_reduce_valarrayfold (dfastate_t *dfast, parsepriv_t *pp, void *rarg)
{
	tnode_t *decl, *array;

	decl = dfa_popnode (dfast);
	array = dfa_popnode (dfast);

	if (!array) {
		parser_error (SLOCN (pp->lf), "broken array specification");
	} else {
		tnode_t **typep = tnode_nthsubaddr (decl, 1);

		tnode_setnthsub (array, 1, *typep);
		*typep = array;
	}
	*(dfast->ptr) = decl;

	if (decl->tag == opi.tag_FPARAM) {
		decl->tag = opi.tag_VALFPARAM;
	} else if (decl->tag == opi.tag_ABBREV) {
		decl->tag = opi.tag_VALABBREV;
	}
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

	/*{{{  register reduction functions*/
	fcnlib_addfcn ("occampi_typedeclhook_blankhook", (void *)occampi_typedeclhook_blankhook, 1, 1);
	fcnlib_addfcn ("occampi_reduce_resetnewline", (void *)occampi_reduce_resetnewline, 0, 3);
	fcnlib_addfcn ("occampi_reduce_arrayfold", (void *)occampi_reduce_arrayfold, 0, 3);
	fcnlib_addfcn ("occampi_reduce_valarrayfold", (void *)occampi_reduce_valarrayfold, 0, 3);

	/*}}}*/
	/*{{{  compiler operations for handling scoping inside type declarations*/
	if (tnode_newcompop ("intypedecl_scopein", COPS_INVALID, 2, INTERNAL_ORIGIN) < 0) {
		nocc_error ("occampi_dtype_init_nodes(): failed to create intypedecl_scopein compiler operation");
		return -1;
	}
	intypedecl_scopein_compop = tnode_findcompop ("intypedecl_scopein");

	if (tnode_newcompop ("intypedecl_scopeout", COPS_INVALID, 2, INTERNAL_ORIGIN) < 0) {
		nocc_error ("occampi_dtype_init_nodes(): failed to create intypedecl_scopeout compiler operation");
		return -1;
	}
	intypedecl_scopeout_compop = tnode_findcompop ("intypedecl_scopeout");

	if (!intypedecl_scopein_compop || !intypedecl_scopeout_compop) {
		nocc_error ("occampi_dtype_init_nodes(): failed to find intypedecl scoping compiler operations");
		return -1;
	}
	/*}}}*/

	/*{{{  occampi:typedecl -- DATATYPEDECL, CHANTYPEDECL, PROCTYPEDECL*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:typedecl", &i, 3, 0, 1, TNF_SHORTDECL);		/* subnodes: 0 = name; 1 = type; 2 = body; hooks: 0 = typedeclhook_t */
	tnd->hook_dumptree = occampi_typedecl_hook_dumptree;
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "prescope", 2, COMPOPTYPE (occampi_prescope_typedecl));
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (occampi_scopein_typedecl));
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (occampi_typecheck_typedecl));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (occampi_namemap_typedecl));
	tnode_setcompop (cops, "precode", 2, COMPOPTYPE (occampi_precode_typedecl));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "bytesfor", 2, LANGOPTYPE (occampi_bytesfor_typedecl));
	tnode_setlangop (lops, "do_usagecheck", 2, LANGOPTYPE (occampi_usagecheck_typedecl));
	tnd->lops = lops;

	i = -1;
	opi.tag_DATATYPEDECL = tnode_newnodetag ("DATATYPEDECL", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_CHANTYPEDECL = tnode_newnodetag ("CHANTYPEDECL", &i, tnd, NTF_SYNCTYPE);
	i = -1;
	opi.tag_PROCTYPEDECL = tnode_newnodetag ("PROCTYPEDECL", &i, tnd, NTF_NONE);
	/*}}}*/
	/*{{{  occampi:arraynode -- ARRAY*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:arraynode", &i, 2, 0, 0, TNF_NONE);			/* subnodes: 0 = dim, 1 = sub-type */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "constprop", 1, COMPOPTYPE (occampi_constprop_arraynode));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (occampi_namemap_arraynode));
	tnode_setcompop (cops, "precode", 2, COMPOPTYPE (occampi_precode_arraynode));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "getdescriptor", 2, LANGOPTYPE (occampi_getdescriptor_arraynode));
	tnode_setlangop (lops, "typeactual", 4, LANGOPTYPE (occampi_typeactual_arraynode));
	tnode_setlangop (lops, "bytesfor", 2, LANGOPTYPE (occampi_bytesfor_arraynode));
	tnode_setlangop (lops, "getsubtype", 2, LANGOPTYPE (occampi_getsubtype_arraynode));
	tnode_setlangop (lops, "valbyref", 1, LANGOPTYPE (occampi_valbyref_arraynode));
	tnode_setlangop (lops, "dimtreeof", 1, LANGOPTYPE (occampi_dimtreeof_arraynode));
	tnode_setlangop (lops, "hiddenparamsof", 1, LANGOPTYPE (occampi_hiddenparamsof_arraynode));
	tnode_setlangop (lops, "hiddenslotsof", 1, LANGOPTYPE (occampi_hiddenslotsof_arraynode));
	tnode_setlangop (lops, "istype", 1, LANGOPTYPE (occampi_istype_arraynode));
	tnode_setlangop (lops, "typetype", 1, LANGOPTYPE (occampi_typetype_arraynode));
	tnd->lops = lops;

	i = -1;
	opi.tag_ARRAY = tnode_newnodetag ("ARRAY", &i, tnd, NTF_NAMEMAPTYPEINDECL | NTF_PRECODETYPEINDECL);

	/*}}}*/
	/*{{{  occampi:arraymopnode -- SIZE*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:arraymopnode", &i, 2, 0, 0, TNF_NONE);		/* subnodes: 0 = operand, 1 = type */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (occampi_typecheck_arraymop));
	tnode_setcompop (cops, "constprop", 1, COMPOPTYPE (occampi_constprop_arraymop));
	tnode_setcompop (cops, "fetrans", 2, COMPOPTYPE (occampi_fetrans_arraymop));
	tnode_setcompop (cops, "premap", 2, COMPOPTYPE (occampi_premap_arraymop));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (occampi_namemap_arraymop));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (occampi_codegen_arraymop));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "gettype", 2, LANGOPTYPE (occampi_gettype_arraymop));
	tnode_setlangop (lops, "iscomplex", 2, LANGOPTYPE (occampi_iscomplex_arraymop));
	tnd->lops = lops;

	i = -1;
	opi.tag_SIZE = tnode_newnodetag ("SIZE", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  occampi:arraydopnode -- DIMSIZE*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:arraydopnode", &i, 3, 0, 0, TNF_NONE);		/* subnodes: 0 = lhs, 1 = rhs, 2 = type */
	cops = tnode_newcompops ();
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnd->lops = lops;

	i = -1;
	opi.tag_DIMSIZE = tnode_newnodetag ("DIMSIZE", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  occampi:fielddecl -- FIELDDECL*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:fielddecl", &i, 2, 0, 0, TNF_NONE);			/* subnodes: 0 = name, 1 = type */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (occampi_scopein_fielddecl));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "bytesfor", 2, LANGOPTYPE (occampi_bytesfor_fielddecl));
	tnd->lops = lops;

	i = -1;
	opi.tag_FIELDDECL = tnode_newnodetag ("FIELDDECL", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  occampi:subscript -- SUBSCRIPT, RECORDSUB, ARRAYSUB*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:subscript", &i, 3, 0, 0, TNF_NONE);			/* subnodes: 0 = base, 1 = field/index, 2 = subscript-type */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (occampi_scopein_subscript));
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (occampi_typecheck_subscript));
	tnode_setcompop (cops, "constprop", 1, COMPOPTYPE (occampi_constprop_subscript));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (occampi_namemap_subscript));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "gettype", 2, LANGOPTYPE (occampi_gettype_subscript));
	tnode_setlangop (lops, "iscomplex", 2, LANGOPTYPE (occampi_iscomplex_subscript));
	tnode_setlangop (lops, "isvar", 1, LANGOPTYPE (occampi_isvar_subscript));
	tnode_setlangop (lops, "getbasename", 1, LANGOPTYPE (occampi_getbasename_subscript));
	tnode_setlangop (lops, "getfieldname", 1, LANGOPTYPE (occampi_getfieldname_subscript));
	tnd->lops = lops;

	i = -1;
	opi.tag_SUBSCRIPT = tnode_newnodetag ("SUBSCRIPT", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_RECORDSUB = tnode_newnodetag ("RECORDSUB", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_ARRAYSUB = tnode_newnodetag ("ARRAYSUB", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  occampi:slice -- ARRAYSLICE*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:slice", &i, 4, 0, 0, TNF_NONE);			/* subnodes: 0 = base, 1 = start, 2 = length, 3 = type */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (occampi_typecheck_slice));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (occampi_namemap_slice));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "gettype", 2, LANGOPTYPE (occampi_gettype_slice));
	tnode_setlangop (lops, "iscomplex", 2, LANGOPTYPE (occampi_iscomplex_slice));
	tnode_setlangop (lops, "dimtreeof", 1, LANGOPTYPE (occampi_dimtreeof_slice));
	tnd->lops = lops;

	i = -1;
	opi.tag_ARRAYSLICE = tnode_newnodetag ("ARRAYSLICE", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  fielddecloffset compiler hook*/
	fielddecloffset = tnode_lookupornewchook ("occampi:fielddecloffset");
	fielddecloffset->chook_dumptree = occampi_fielddecloffset_chook_dumptree;

	/*}}}*/
	/*{{{  ct_clienttype, ct_servertype compiler hooks*/
	ct_clienttype = tnode_lookupornewchook ("occampi:chantype:clienttype");
	ct_clienttype->chook_dumptree = occampi_ctclienttype_chook_dumptree;
	ct_servertype = tnode_lookupornewchook ("occampi:chantype:servertype");
	ct_servertype->chook_dumptree = occampi_ctservertype_chook_dumptree;

	/*}}}*/
	/*{{{  ct_clienttrace, ct_servertrace compiler hooks*/
	ct_clienttrace = tnode_lookupornewchook ("occampi:chantype:clienttrace");
	ct_clienttrace->chook_dumptree = occampi_ctclienttrace_chook_dumptree;
	ct_clienttrace->chook_copy = occampi_ctclienttrace_chook_copy;
	ct_clienttrace->chook_free = occampi_ctclienttrace_chook_free;
	ct_servertrace = tnode_lookupornewchook ("occampi:chantype:servertrace");
	ct_servertrace->chook_dumptree = occampi_ctservertrace_chook_dumptree;
	ct_servertrace->chook_copy = occampi_ctservertrace_chook_copy;
	ct_servertrace->chook_free = occampi_ctservertrace_chook_free;

	/*}}}*/

	/*{{{  occampi:nametypenode -- N_DATATYPEDECL, N_FIELD, N_CHANTYPEDECL, N_PROCTYPEDECL*/
	i = -1;
	tnd = opi.node_NAMETYPENODE = tnode_newnodetype ("occampi:nametypenode", &i, 0, 1, 0, TNF_NONE);	/* subnames: name */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (occampi_namemap_nametypenode));
	tnd->ops = cops;

	lops = tnode_newlangops ();
	tnode_setlangop (lops, "gettype", 2, LANGOPTYPE (occampi_gettype_nametypenode));
	tnode_setlangop (lops, "bytesfor", 2, LANGOPTYPE (occampi_bytesfor_nametypenode));
	tnode_setlangop (lops, "getname", 2, LANGOPTYPE (occampi_getname_nametypenode));
	tnode_setlangop (lops, "initialising_decl", 3, LANGOPTYPE (occampi_initialising_decl_nametypenode));
	tnode_setlangop (lops, "istype", 1, LANGOPTYPE (occampi_istype_nametypenode));
	tnode_setlangop (lops, "typehash", 3, LANGOPTYPE (occampi_typehash_nametypenode));
	tnode_setlangop (lops, "gettags", 1, LANGOPTYPE (occampi_gettags_nametypenode));
	tnode_setlangop (lops, "tracespecof", 1, LANGOPTYPE (occampi_tracespecof_nametypenode));
	tnd->lops = lops;

	i = -1;
	opi.tag_NDATATYPEDECL = tnode_newnodetag ("N_DATATYPEDECL", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_NCHANTYPEDECL = tnode_newnodetag ("N_CHANTYPEDECL", &i, tnd, NTF_SYNCTYPE);
	i = -1;
	opi.tag_NPROCTYPEDECL = tnode_newnodetag ("N_PROCTYPEDECL", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_NFIELD = tnode_newnodetag ("N_FIELD", &i, tnd, NTF_NONE);

	/*}}}*/

	return 0;
}
/*}}}*/
/*{{{  static int occampi_dtype_post_setup (void)*/
/*
 *	does post-setup for data type nodes in occampi
 *	returns 0 on success, non-zero on failure
 */
static int occampi_dtype_post_setup (void)
{
	tndef_t *tnd;
	compops_t *cops;
	langops_t *lops;

	/*{{{  intefere with occampi:typespecnode to handle channel-type ends*/
	tnd = tnode_lookupnodetype ("occampi:typespecnode");
	if (!tnd) {
		nocc_internal ("occampi_dtype_post_setup(): failed to find \"occampi:typespecnode\" node-type!");
		return -1;
	}
	cops = tnode_insertcompops (tnd->ops);
	tnd->ops = cops;
	lops = tnode_insertlangops (tnd->lops);
	tnode_setlangop (lops, "tracespecof", 1, LANGOPTYPE (occampi_tracespecof_cttypespecnode));
	tnd->lops = lops;

	/*}}}*/
	/*{{{  grab occampi:cttrace compiler-hook (attaches trace definitions to CHAN TYPE declarations)*/
	cttrace_chook = tnode_lookupornewchook ("occampi:cttrace");

	/*}}}*/
	return 0;
}
/*}}}*/


/*{{{  occampi_dtype_feunit (feunit_t)*/
feunit_t occampi_dtype_feunit = {
	.init_nodes = occampi_dtype_init_nodes,
	.reg_reducers = NULL,
	.init_dfatrans = NULL,
	.post_setup = occampi_dtype_post_setup,
	.ident = "occampi-dtype"
};
/*}}}*/

