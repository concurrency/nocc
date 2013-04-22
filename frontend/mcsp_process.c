/*
 *	mcsp_process.c -- handling for MCSP processes
 *	Copyright (C) 2006-2013 Fred Barnes <frmb@kent.ac.uk>
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
#include "fhandle.h"
#include "fcnlib.h"
#include "symbols.h"
#include "keywords.h"
#include "lexer.h"
#include "lexpriv.h"
#include "tnode.h"
#include "parser.h"
#include "dfa.h"
#include "parsepriv.h"
#include "mcsp.h"
#include "feunit.h"
#include "names.h"
#include "scope.h"
#include "prescope.h"
#include "constprop.h"
#include "typecheck.h"
#include "usagecheck.h"
#include "postcheck.h"
#include "mwsync.h"
#include "fetrans.h"
#include "betrans.h"
#include "map.h"
#include "codegen.h"
#include "target.h"
#include "transputer.h"


/*}}}*/

/*{{{  private types/data*/
typedef struct TAG_mcsp_consthook {
	char *data;
	int length;
	int valtype;		/* whether 'data' should be interpreted as a number */
} mcsp_consthook_t;


/*}}}*/

/*{{{  static void *mcsp_nametoken_to_hook (void *ntok)*/
/*
 *	turns a name token into a hooknode for a tag_NAME
 */
static void *mcsp_nametoken_to_hook (void *ntok)
{
	token_t *tok = (token_t *)ntok;
	char *rawname;

	rawname = tok->u.name;
	tok->u.name = NULL;

	lexer_freetoken (tok);

	return (void *)rawname;
}
/*}}}*/
/*{{{  static void *mcsp_stringtoken_to_hook (void *ntok)*/
/*
 *	turns a string token into a hooknode for a tag_STRING
 */
static void *mcsp_stringtoken_to_hook (void *ntok)
{
	token_t *tok = (token_t *)ntok;
	mcsp_consthook_t *ch;

	ch = (mcsp_consthook_t *)smalloc (sizeof (mcsp_consthook_t));
	ch->data = string_ndup (tok->u.str.ptr, tok->u.str.len);
	ch->length = tok->u.str.len;
	ch->valtype = 0;

	lexer_freetoken (tok);
	return (void *)ch;
}
/*}}}*/
/*{{{  static void *mcsp_integertoken_to_hook (void *ntok)*/
/*
 *	turns an integer token into a hooknode for a tag_INTEGER
 */
static void *mcsp_integertoken_to_hook (void *ntok)
{
	token_t *tok = (token_t *)ntok;
	mcsp_consthook_t *ch;

	ch = (mcsp_consthook_t *)smalloc (sizeof (mcsp_consthook_t));
	ch->data = mem_ndup (&(tok->u.ival), sizeof (int));
	ch->length = sizeof (int);
	ch->valtype = 1;

	lexer_freetoken (tok);
	return (void *)ch;
}
/*}}}*/
/*{{{  static void *mcsp_pptoken_to_node (void *ntok)*/
/*
 *	turns a keyword token for a primitive process into a mcsp:leafproc node
 */
static void *mcsp_pptoken_to_node (void *ntok)
{
	token_t *tok = (token_t *)ntok;
	char *sbuf;
	tnode_t *node;
	ntdef_t *tag;

	if (tok->type != KEYWORD) {
		nocc_error ("mcsp_pptoken_to_node(): token not keyword: [%s]", lexer_stokenstr (tok));
		lexer_freetoken (tok);
		return NULL;
	}
	sbuf = (char *)smalloc (128);
	snprintf (sbuf, 127, "MCSP%s", tok->u.kw->name);

	tag = tnode_lookupnodetag (sbuf);
	if (!tag) {
		nocc_error ("mcsp_pptoken_to_node(): keyword not node-tag: [%s]", sbuf);
		sfree (sbuf);
		lexer_freetoken (tok);
		return NULL;
	}
	sfree (sbuf);

	node = tnode_create (tag, tok->origin);
	lexer_freetoken (tok);

	return node;
}
/*}}}*/


/*{{{  static void mcsp_rawnamenode_hook_free (void *hook)*/
/*
 *	frees a rawnamenode hook (name-bytes)
 */
static void mcsp_rawnamenode_hook_free (void *hook)
{
	if (hook) {
		sfree (hook);
	}
	return;
}
/*}}}*/
/*{{{  static void *mcsp_rawnamenode_hook_copy (void *hook)*/
/*
 *	copies a rawnamenode hook (name-bytes)
 */
static void *mcsp_rawnamenode_hook_copy (void *hook)
{
	char *rawname = (char *)hook;

	if (rawname) {
		return string_dup (rawname);
	}
	return NULL;
}
/*}}}*/
/*{{{  static void mcsp_rawnamenode_hook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)*/
/*
 *	dump-tree for rawnamenode hook (name-bytes)
 */
static void mcsp_rawnamenode_hook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)
{
	mcsp_isetindent (stream, indent);
	fhandle_printf (stream, "<mcsprawnamenode value=\"%s\" />\n", hook ? (char *)hook : "(null)");
	return;
}
/*}}}*/


/*{{{  static void mcsp_constnode_hook_free (void *hook)*/
/*
 *	frees a constnode hook (bytes)
 */
static void mcsp_constnode_hook_free (void *hook)
{
	mcsp_consthook_t *ch = (mcsp_consthook_t *)hook;

	if (ch) {
		if (ch->data) {
			sfree (ch->data);
		}
		sfree (ch);
	}

	return;
}
/*}}}*/
/*{{{  static void *mcsp_constnode_hook_copy (void *hook)*/
/*
 *	copies a constnode hook (name-bytes)
 */
static void *mcsp_constnode_hook_copy (void *hook)
{
	mcsp_consthook_t *ch = (mcsp_consthook_t *)hook;

	if (ch) {
		mcsp_consthook_t *newch = (mcsp_consthook_t *)smalloc (sizeof (mcsp_consthook_t));

		if (ch->valtype) {
			newch->data = ch->data ? mem_ndup (ch->data, ch->length) : NULL;
		} else {
			newch->data = ch->data ? string_dup (ch->data) : NULL;
		}
		newch->length = ch->length;
		newch->valtype = ch->valtype;

		return (void *)newch;
	}
	return NULL;
}
/*}}}*/
/*{{{  static void mcsp_constnode_hook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)*/
/*
 *	dump-tree for constnode hook (name-bytes)
 */
static void mcsp_constnode_hook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)
{
	mcsp_consthook_t *ch = (mcsp_consthook_t *)hook;

	mcsp_isetindent (stream, indent);
	if (!ch) {
		fhandle_printf (stream, "<mcspconsthook length=\"0\" value=\"(null)\" />\n");
	} else if (!ch->valtype) {
		fhandle_printf (stream, "<mcspconsthook length=\"%d\" value=\"%s\" />\n", ch->length, ch->data ? ch->data : "(null)");
	} else if (ch->length == 4) {
		fhandle_printf (stream, "<mcspconsthook length=\"%d\" value=\"%d\" />\n", ch->length, ch->data ? *(int *)ch->data : 0);
	} else {
		char *vstr = mkhexbuf ((unsigned char *)ch->data, ch->length);

		fhandle_printf (stream, "<mcspconsthook length=\"%d\" hexvalue=\"%s\" />\n", ch->length, vstr);
		sfree (vstr);
	}
	return;
}
/*}}}*/


/*{{{  static int mcsp_scopein_rawname (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	scopes in a free-floating name
 */
static int mcsp_scopein_rawname (compops_t *cops, tnode_t **node, scope_t *ss)
{
	tnode_t *name = *node;
	char *rawname;
	name_t *sname = NULL;
	mcsp_lex_t *lmp;
	mcsp_scope_t *mss = (mcsp_scope_t *)ss->langpriv;

	if ((*node)->org_file && (*node)->org_file->priv) {
		lexpriv_t *lp = (lexpriv_t *)(*node)->org_file->priv;
		
		lmp = (mcsp_lex_t *)lp->langpriv;
	} else {
		lmp = NULL;
	}

	if (name->tag != mcsp.tag_NAME) {
		scope_error (name, ss, "name not raw-name!");
		return 0;
	}
	rawname = tnode_nthhookof (name, 0);

#if 0
fprintf (stderr, "mcsp_scopein_rawname: here! rawname = \"%s\"\n", rawname);
#endif
	sname = name_lookupss (rawname, ss);
	if (sname) {
		/* resolved */
		*node = NameNodeOf (sname);

		/* if it looks like a PROCDEF, turn into an INSTANCE -- if we're not already in an instance!*/
		if (!mss->inamescope && ((*node)->tag == mcsp.tag_PROCDEF)) {
			*node = tnode_createfrom (mcsp.tag_INSTANCE, name, *node, parser_newlistnode (NULL));
		}

		tnode_free (name);
	} else {
#if 0
fprintf (stderr, "mcsp_scopein_rawname(): unresolved name \"%s\", unbound-events = %d, mss->uvinsertlist = 0x%8.8x\n", rawname, lmp ? lmp->unboundvars : -1, (unsigned int)mss->uvinsertlist);
#endif
		if (lmp && lmp->unboundvars) {
			if (mss && mss->uvinsertlist) {
				/*{{{  add the name manually*/
				tnode_t *decl = tnode_create (mcsp.tag_UPARAM, NULL, NULL, tnode_create (mcsp.tag_EVENTTYPE, NULL));
				tnode_t *newname;

				sname = name_addsubscopenamess (rawname, mss->uvscopemark, decl, NULL, name, ss);
				parser_addtolist (mss->uvinsertlist, decl);
				newname = tnode_createfrom (mcsp.tag_EVENT, decl, sname);
				SetNameNode (sname, newname);
				tnode_setnthsub (decl, 0, newname);

				/* and replace local node! */
				*node = newname;
				tnode_free (name);

				ss->scoped++;
				/*}}}*/
			} else {
				scope_error (name, ss, "unresolved name \"%s\" cannot be captured", rawname);
			}
		} else {
			scope_error (name, ss, "unresolved name \"%s\"", rawname);
		}
	}

	return 1;
}
/*}}}*/


/*{{{  static int mcsp_namemap_leafproc (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	does name-mapping for a leaf-process
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_namemap_leafproc (compops_t *cops, tnode_t **node, map_t *map)
{
	return 0;
}
/*}}}*/
/*{{{  static int mcsp_codegen_leafproc (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	does code-generation for a leaf-process
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_codegen_leafproc (compops_t *cops, tnode_t *node, codegen_t *cgen)
{
	if (node->tag == mcsp.tag_SKIP) {
		/* nothing :) */
		return 0;
	} else if (node->tag == mcsp.tag_STOP) {
		codegen_callops (cgen, tsecondary, I_SETERR);
		return 0;
	} else if ((node->tag == mcsp.tag_DIV) || (node->tag == mcsp.tag_CHAOS)) {
		/* just stop for now -- maybe get creative later ;) */
		codegen_callops (cgen, tsecondary, I_SETERR);
		return 0;
	}
	codegen_warning (cgen, "don\'t know how to generate code for [%s]", node->tag->name);
	return 1;
}
/*}}}*/


/*{{{  static int mcsp_postcheck_namenode (compops_t *cops, tnode_t **node, postcheck_t *pc)*/
/*
 *	does post-check transformation on an EVENT
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_postcheck_namenode (compops_t *cops, tnode_t **node, postcheck_t *pc)
{
	tnode_t *t = *node;

#if 0
fprintf (stderr, "mcsp_postcheck_namenode(): t =\n");
tnode_dumptree (t, 1, stderr);
#endif
	if (t->tag == mcsp.tag_EVENT) {
		/*{{{  turn event into SYNC -- not a declaration occurence*/
		*node = tnode_create (mcsp.tag_SYNC, NULL, *node, NULL, NameTypeOf (tnode_nthnameof (t, 0)));
		return 0;
		/*}}}*/
	}
	return 1;
}
/*}}}*/
/*{{{  static int mcsp_fetrans_namenode (compops_t *cops, tnode_t **node, fetrans_t *fe)*/
/*
 *	does front-end transformation on an EVENT
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_fetrans_namenode (compops_t *cops, tnode_t **node, fetrans_t *fe)
{
	mcsp_fetrans_t *mfe = (mcsp_fetrans_t *)fe->langpriv;
	tnode_t *t = *node;

	switch (mfe->parse) {
	case 0:
		if (t->tag == mcsp.tag_EVENT) {
			tnode_warning (*node, "mcsp_fetrans_namenode(): unexpected EVENT");
		}
		break;
	}

	return 1;
}
/*}}}*/
/*{{{  static int mcsp_mwsynctrans_namenode (compops_t *cops, tnode_t **node, mwsynctrans_t *mwi)*/
/*
 *	does multiway sync transform for an EVENT
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_mwsynctrans_namenode (compops_t *cops, tnode_t **node, mwsynctrans_t *mwi)
{
	if ((*node)->tag == mcsp.tag_EVENT) {
		name_t *name = tnode_nthnameof (*node, 0);

		mwsync_mwsynctrans_nameref (node, name, mcsp.tag_EVENT, mwi);
	}
	return 0;
}
/*}}}*/
/*{{{  static int mcsp_namemap_namenode (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	does name-mapping for namenodes (unbound references)
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_namemap_namenode (compops_t *cops, tnode_t **node, map_t *map)
{
	tnode_t *bename = (tnode_t *)tnode_getchook (*node, map->mapchook);
	tnode_t *tname;

	if (bename) {
		tname = map->target->newnameref (bename, map);
		*node = tname;
	}

	return 0;
}
/*}}}*/
#if 0
/*{{{  static void mcsp_namenode_initevent (tnode_t *node, codegen_t *cgen, void *arg)*/
/*
 *	generates code to initialise an EVENT
 */
static void mcsp_namenode_initevent (tnode_t *node, codegen_t *cgen, void *arg)
{
	int ws_offs;

	cgen->target->be_getoffsets (node, &ws_offs, NULL, NULL, NULL);
	/* FIXME: this needs tidying */
	codegen_callops (cgen, loadconst, 5 * cgen->target->slotsize);
	codegen_callops (cgen, tsecondary, I_MALLOC);
	codegen_callops (cgen, storelocal, ws_offs);

	/* initialise */
	codegen_callops (cgen, loadconst, 1);
	codegen_callops (cgen, loadlocal, ws_offs);
	codegen_callops (cgen, storenonlocal, 0);		/* ref-count */

	codegen_callops (cgen, loadconst, 1);
	codegen_callops (cgen, loadlocal, ws_offs);
	codegen_callops (cgen, storenonlocal, 4);		/* enrolled-count */

	codegen_callops (cgen, loadconst, 1);
	codegen_callops (cgen, loadlocal, ws_offs);
	codegen_callops (cgen, storenonlocal, 8);		/* count-down */

	codegen_callops (cgen, tsecondary, I_NULL);
	codegen_callops (cgen, loadlocal, ws_offs);
	codegen_callops (cgen, storenonlocal, 12);		/* fptr */

	codegen_callops (cgen, tsecondary, I_NULL);
	codegen_callops (cgen, loadlocal, ws_offs);
	codegen_callops (cgen, storenonlocal, 16);		/* bptr */

	return;
}
/*}}}*/
/*{{{  static void mcsp_namenode_finalevent (tnode_t *node, codegen_t *cgen, void *arg)*/
/*
 *	generates code to destroy an EVENT
 */
static void mcsp_namenode_finalevent (tnode_t *node, codegen_t *cgen, void *arg)
{
	int ws_offs;

	cgen->target->be_getoffsets (node, &ws_offs, NULL, NULL, NULL);

	codegen_callops (cgen, loadlocal, ws_offs);
	codegen_callops (cgen, tsecondary, I_MRELEASE);
	codegen_callops (cgen, tsecondary, I_NULL);
	codegen_callops (cgen, storelocal, ws_offs);

	return;
}
/*}}}*/
#endif
/*{{{  static int mcsp_namenode_initialising_decl (langops_t *lops, tnode_t *node, tnode_t *bename, map_t *map)*/
/*
 *	used to initialise EVENTs
 */
static int mcsp_namenode_initialising_decl (langops_t *lops, tnode_t *node, tnode_t *bename, map_t *map)
{
	if (node->tag == mcsp.tag_EVENT) {
#if 0
		codegen_setinithook (bename, mcsp_namenode_initevent, (void *)node);
		codegen_setfinalhook (bename, mcsp_namenode_finalevent, (void *)node);
#endif
	}
	return 0;
}
/*}}}*/
/*{{{  static tnode_t *mcsp_namenode_gettype (langops_t *lops, tnode_t *node, tnode_t *default_type)*/
/*
 *	returns the type of a name-node (trivial)
 */
static tnode_t *mcsp_namenode_gettype (langops_t *lops, tnode_t *node, tnode_t *default_type)
{
	name_t *name = tnode_nthnameof (node, 0);

	if (!name) {
		nocc_fatal ("mcsp_namenode_gettype(): NULL name!");
		return NULL;
	}
	if (name->type) {
#if 0
fprintf (stderr, "occmpi_gettype_namenode(): node = [%s], name:\n", node->tag->name);
name_dumpname (name, 1, stderr);
fprintf (stderr, "   \"   name->type:\n");
tnode_dumptree (name->type, 1, stderr);
#endif
		return name->type;
	}
#if 1
nocc_message ("mcsp_namenode_gettype(): null type on name, node was:");
tnode_dumptree (node, 4, FHAN_STDERR);
#endif
	nocc_fatal ("mcsp_namenode_gettype(): name has NULL type (FIXME!)");
	return NULL;
}
/*}}}*/
/*{{{  static int mcsp_namenode_bytesfor (langops_t *lops, tnode_t *node, target_t *target)*/
/*
 *	returns the number of bytes in a name-node, associated with its type only
 */
static int mcsp_namenode_bytesfor (langops_t *lops, tnode_t *node, target_t *target)
{
	nocc_error ("mcsp_namenode_bytesfor(): no bytes for [%s]", node->tag->name);
	return -1;
}
/*}}}*/
/*{{{  static int mcsp_namenode_getname (langops_t *lops, tnode_t *node, char **str)*/
/*
 *	gets the name of a namenode (var/etc. name)
 *	return 0 on success, -ve on failure
 */
static int mcsp_namenode_getname (langops_t *lops, tnode_t *node, char **str)
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



/*{{{  static int mcsp_typecheck_actionnode (compops_t *cops, tnode_t *node, typecheck_t *tc)*/
/*
 *	does type-checkking on an action-node
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_typecheck_actionnode (compops_t *cops, tnode_t *node, typecheck_t *tc)
{
	int i = 1;

	if (node->tag == mcsp.tag_SYNC) {
		return 0;
	}
	return i;
}
/*}}}*/
/*{{{  static int mcsp_fetrans_actionnode (compops_t *cops, tnode_t **node, fetrans_t *fe)*/
/*
 *	does front-end transforms for an action-node
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_fetrans_actionnode (compops_t *cops, tnode_t **node, fetrans_t *fe)
{
	mcsp_fetrans_t *mfe = (mcsp_fetrans_t *)fe->langpriv;
	tnode_t *t = *node;

	switch (mfe->parse) {
	case 0:
		if (t->tag == mcsp.tag_SYNC) {
			/* ignore on this pass */
			return 0;
		}
		/* fall through */
	case 1:
		/* nothing in these passes! */
		break;
	case 2:
		if (t->tag == mcsp.tag_SYNC) {
			/*{{{  add event to list*/
			if (mfe->curalpha) {
				mcsp_addtoalpha (mfe->curalpha, tnode_nthsubof (t, 0));
			}
			/*}}}*/
		}
		break;
	}
	return 1;
}
/*}}}*/
/*{{{  static int mcsp_betrans_actionnode (compops_t *cops, tnode_t **node, betrans_t *be)*/
/*
 *	does back-end transforms for an action-node
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_betrans_actionnode (compops_t *cops, tnode_t **node, betrans_t *be)
{
	tnode_t *t = *node;

#if 0
	if (t->tag == mcsp.tag_SYNC) {
		/* we need to turn this into a single-guard ALT */
		tnode_t *altnode, *guard, *glist, *event;

		event = tnode_nthsubof (t, 0);
		guard = tnode_createfrom (mcsp.tag_GUARD, t, event, tnode_create (mcsp.tag_SKIP, NULL));
		glist = parser_newlistnode (NULL);
		parser_addtolist (glist, guard);
		altnode = tnode_createfrom (mcsp.tag_ALT, t, glist, NULL);

		tnode_setnthsub (t, 0, NULL);
		tnode_free (t);
		*node = altnode;

		betrans_subtree (node, be);
		return 0;
	}
#endif
	return 1;
}
/*}}}*/
/*{{{  static int mcsp_namemap_actionnode (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	does name-mapping for an action-node
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_namemap_actionnode (compops_t *cops, tnode_t **node, map_t *map)
{
	tnode_t *t = *node;

	if (t->tag == mcsp.tag_SYNC) {
		tnode_t *bename;

		map_submapnames (tnode_nthsubaddr (*node, 0), map);		/* map event operand */
		bename = map->target->newname (*node, NULL, map, 0, map->target->bws.ds_min, 0, 0, 0, 0);
		*node = bename;

		return 0;
	} else if (t->tag == mcsp.tag_CHANWRITE) {
		tnode_t *bename;
		tnode_t **opp;
		int aslots = 0;		/* above workspace slots that we might need */

		/* do channel (and argument if not an event) */
		map_submapnames (tnode_nthsubaddr (*node, 0), map);

		opp = tnode_nthsubaddr (*node, 1);
		if (*opp && ((*opp)->tag != mcsp.tag_EVENT)) {
			map_submapnames (opp, map);
		} else if (*opp) {
			/* slightly crafty -- turn operand into a back-end constant containing the EVENT's name */
			name_t *evname = tnode_nthnameof (*opp, 0);
			char *thename;
			char *localname;

			/* may need to hunt back through modified barrier tree */
			evname = mwsync_basenameof (evname, map);
			thename = NameNameOf (evname);
			localname = (char *)smalloc (strlen (thename) + 3);

			sprintf (localname, "%s\n", thename);
			*opp = map->target->newconst (*opp, map, (void *)localname, strlen (localname) + 1, TYPE_NOTTYPE);	/* null terminator for free */
			sfree (localname);
			aslots = 1;											/* need a counter for output */
		}

		/* make space for output */
		bename = map->target->newname (*node, NULL, map, aslots * map->target->slotsize, map->target->bws.ds_io, 0, 0, 0, 0);
		*node = bename;

		return 0;
	}

	return 1;
}
/*}}}*/
/*{{{  static int mcsp_codegen_actionnode (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	does code-generation for an action-node
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_codegen_actionnode (compops_t *cops, tnode_t *node, codegen_t *cgen)
{
	if (node->tag == mcsp.tag_SYNC) {
		tnode_t *bar = tnode_nthsubof (node, 0);

		codegen_callops (cgen, debugline, node);
		codegen_callops (cgen, tsecondary, I_MWS_ALTLOCK);
		codegen_callops (cgen, loadpointer, bar, 0);
		codegen_callops (cgen, tsecondary, I_MWS_SYNC);
		//codegen_error (cgen, "mcsp_codegen_actionnode(): cannot generate code for SYNC!");
		return 0;
	} else if (node->tag == mcsp.tag_CHANWRITE) {
		tnode_t *chan = tnode_nthsubof (node, 0);
		tnode_t *data = tnode_nthsubof (node, 1);
		int tempslot = 0;				/* reserved a slot -- should really be_getoffsets for it */
		int looplab, skiplab;

		/* FIXME: this is crude.. */
		looplab = codegen_new_label (cgen);
		skiplab = codegen_new_label (cgen);

		codegen_callops (cgen, loadconst, 0);
		codegen_callops (cgen, storelocal, tempslot);

		codegen_callops (cgen, setlabel, looplab);
		codegen_callops (cgen, loadpointer, data, 0);
		codegen_callops (cgen, loadlocal, tempslot);
		codegen_callops (cgen, tsecondary, I_SUM);
		codegen_callops (cgen, tsecondary, I_LB);
		codegen_callops (cgen, branch, I_CJ, skiplab);		/* if we saw the NULL terminator */
		codegen_callops (cgen, trashistack);

		codegen_callops (cgen, loadpointer, data, 0);
		codegen_callops (cgen, loadlocal, tempslot);
		codegen_callops (cgen, tsecondary, I_SUM);

		codegen_callops (cgen, loadpointer, chan, 0);
		codegen_callops (cgen, loadconst, 1);			/* BYTE output only! */
		codegen_callops (cgen, tsecondary, I_OUT);

		codegen_callops (cgen, loadlocal, tempslot);
		codegen_callops (cgen, loadconst, 1);
		codegen_callops (cgen, tsecondary, I_SUM);
		codegen_callops (cgen, storelocal, tempslot);

		codegen_callops (cgen, branch, I_J, looplab);

		codegen_callops (cgen, setlabel, skiplab);

		return 0;
	}
	codegen_warning (cgen, "don\'t know how to generate code for [%s]", node->tag->name);
	return 1;
}
/*}}}*/


/*{{{  static int mcsp_constprop_const (compops_t *cops, tnode_t **node)*/
/*
 *	does constant propagation on a constant node (post walk)
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_constprop_const (compops_t *cops, tnode_t **node)
{
	mcsp_consthook_t *ch = (mcsp_consthook_t *)tnode_nthhookof (*node, 0);

	if ((*node)->tag == mcsp.tag_INTEGER) {
		*node = constprop_newconst (CONST_INT, *node, NULL, *(int *)(ch->data));
	}
	return 0;
}
/*}}}*/
/*{{{  static int mcsp_namemap_const (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	does name-mapping for a constant
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_namemap_const (compops_t *cops, tnode_t **node, map_t *map)
{
	mcsp_consthook_t *ch = (mcsp_consthook_t *)tnode_nthhookof (*node, 0);
	tnode_t *ltype = tnode_nthsubof (*node, 0);

	*node = map->target->newconst (*node, map, ch->data, ch->length, typecheck_typetype (ltype));
	return 0;
}
/*}}}*/
/*{{{  static int mcsp_isconst_const (langops_t *lops, tnode_t *node)*/
/*
 *	returns non-zero if the node is a constant (returns width)
 */
static int mcsp_isconst_const (langops_t *lops, tnode_t *node)
{
	mcsp_consthook_t *ch = (mcsp_consthook_t *)tnode_nthhookof (node, 0);

	return ch->length;
}
/*}}}*/
/*{{{  static int mcsp_constvalof_const (langops_t *lops, tnode_t *node, void *ptr)*/
/*
 *	returns the constant value of a constant node (assigns to pointer if non-null)
 */
static int mcsp_constvalof_const (langops_t *lops, tnode_t *node, void *ptr)
{
	mcsp_consthook_t *ch = (mcsp_consthook_t *)tnode_nthhookof (node, 0);
	int r = 0;

	if (node->tag == mcsp.tag_INTEGER) {
		if (ptr) {
			*(int *)ptr = *(int *)(ch->data);
		}
		r = *(int *)(ch->data);
	}
	return r;
}
/*}}}*/
/*{{{  static int mcsp_valbyref_const (langops_t *lops, tnode_t *node)*/
/*
 *	returns non-zero if value-references of this constant get passed by reference
 */
static int mcsp_valbyref_const (langops_t *lops, tnode_t *node)
{
	if (node->tag == mcsp.tag_STRING) {
		return 1;
	}
	return 0;
}
/*}}}*/


/*{{{  static int mcsp_mwsynctrans_leaftype (compops_t *cops, tnode_t **node, mwsynctrans_t *mwi)*/
/*
 *	does multiway synchronisation transforms on a leaftype
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_mwsynctrans_leaftype (compops_t *cops, tnode_t **node, mwsynctrans_t *mwi)
{
	if ((*node)->tag == mcsp.tag_EVENTTYPE) {
		mwsync_mwsynctrans_makebarriertype (node, mwi);
		return 0;
	}
	return 1;
}
/*}}}*/
/*{{{  static tnode_t *mcsp_gettype_leaftype (langops_t *lops, tnode_t *t, tnode_t *defaulttype)*/
/*
 *	gets the type for a mwsync leaftype -- do nothing really
 */
static tnode_t *mcsp_gettype_leaftype (langops_t *lops, tnode_t *t, tnode_t *defaulttype)
{
	if (t->tag == mcsp.tag_EVENTTYPE) {
		return t;
	}

	if (lops->next && tnode_haslangop_i (lops->next, (int)LOPS_GETTYPE)) {
		return (tnode_t *)tnode_calllangop_i (lops->next, (int)LOPS_GETTYPE, 2, t, defaulttype);
	}
	nocc_error ("mcsp_gettype_leaftype(): no next function!");
	return defaulttype;
}
/*}}}*/
/*{{{  static int mcsp_bytesfor_leaftype (langops_t *lops, tnode_t *t, target_t *target)*/
/*
 *	returns the number of bytes required by a basic type
 */
static int mcsp_bytesfor_leaftype (langops_t *lops, tnode_t *t, target_t *target)
{
	if (t->tag == mcsp.tag_EVENTTYPE) {
		nocc_warning ("mcsp_bytesfor_leaftype(): unreplaced EVENTTYPE type probably!");
		return 0;
	}

	if (lops->next && tnode_haslangop_i (lops->next, (int)LOPS_BYTESFOR)) {
		return tnode_calllangop_i (lops->next, (int)LOPS_BYTESFOR, 2, t, target);
	}
	nocc_error ("mcsp_bytesfor_leaftype(): no next function!");
	return -1;
}
/*}}}*/
/*{{{  static int mcsp_issigned_leaftype (langops_t *lops, tnode_t *t, target_t *target)*/
/*
 *	returns 0 if the given basic type is unsigned
 */
static int mcsp_issigned_leaftype (langops_t *lops, tnode_t *t, target_t *target)
{
	if (t->tag == mcsp.tag_EVENTTYPE) {
		return 0;
	}

	if (lops->next && tnode_haslangop_i (lops->next, (int)LOPS_ISSIGNED)) {
		return tnode_calllangop_i (lops->next, (int)LOPS_ISSIGNED, 2, t, target);
	}
	nocc_error ("mcsp_issigned_leaftype(): no next function!");
	return 0;
}
/*}}}*/
/*{{{  static int mcsp_getdescriptor_leaftype (langops_t *lops, tnode_t *node, char **str)*/
/*
 *	gets descriptor information for a leaf-type
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_getdescriptor_leaftype (langops_t *lops, tnode_t *node, char **str)
{
	char *sptr;

	if (node->tag == mcsp.tag_EVENTTYPE) {
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
		sprintf (sptr, "EVENT");
		return 0;
	}
	if (lops->next && tnode_haslangop_i (lops->next, (int)LOPS_GETDESCRIPTOR)) {
		return tnode_calllangop_i (lops->next, (int)LOPS_GETDESCRIPTOR, 2, node, str);
	}
	nocc_error ("mcsp_getdescriptor_leaftype(): no next function!");

	return 0;
}
/*}}}*/
/*{{{  static int mcsp_initialising_decl_leaftype (langops_t *lops, tnode_t *t, tnode_t *benode, map_t *mdata)*/
/*
 *	called for declarations to handle initialisation if needed
 *	returns 0 if nothing needed, 1 otherwise
 */
static int mcsp_initialising_decl_leaftype (langops_t *lops, tnode_t *t, tnode_t *benode, map_t *mdata)
{
	if (t->tag == mcsp.tag_EVENTTYPE) {
		nocc_warning ("mcsp_initialising_decl_leaftype(): not expecting an EVENTTYPE here..");
		return 0;
	}
	if (lops->next && tnode_haslangop_i (lops->next, (int)LOPS_INITIALISING_DECL)) {
		return tnode_calllangop_i (lops->next, (int)LOPS_INITIALISING_DECL, 3, t, benode, mdata);
	}
	return 0;
}
/*}}}*/




/*{{{  static int mcsp_process_init_nodes (void)*/
/*
 *	initialises MCSP process nodes
 *	returns 0 on success, non-zero on failure
 */
static int mcsp_process_init_nodes (void)
{
	tndef_t *tnd;
	int i;
	compops_t *cops;
	langops_t *lops;

	/*{{{  register named functions*/
	fcnlib_addfcn ("mcsp_nametoken_to_hook", (void *)mcsp_nametoken_to_hook, 1, 1);
	fcnlib_addfcn ("mcsp_pptoken_to_node", (void *)mcsp_pptoken_to_node, 1, 1);
	fcnlib_addfcn ("mcsp_integertoken_to_hook", (void *)mcsp_integertoken_to_hook, 1, 1);
	fcnlib_addfcn ("mcsp_stringtoken_to_hook", (void *)mcsp_stringtoken_to_hook, 1, 1);

	/*}}}*/
	/*{{{  mcsp:rawnamenode -- NAME*/
	i = -1;
	tnd = tnode_newnodetype ("mcsp:rawnamenode", &i, 0, 0, 1, TNF_NONE);				/* hooks: raw-name */
	tnd->hook_free = mcsp_rawnamenode_hook_free;
	tnd->hook_copy = mcsp_rawnamenode_hook_copy;
	tnd->hook_dumptree = mcsp_rawnamenode_hook_dumptree;
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (mcsp_scopein_rawname));
	tnd->ops = cops;

	i = -1;
	mcsp.tag_NAME = tnode_newnodetag ("MCSPNAME", &i, tnd, NTF_NONE);

#if 0
fprintf (stderr, "mcsp_process_init_nodes(): tnd->name = [%s], mcsp.tag_NAME->name = [%s], mcsp.tag_NAME->ndef->name = [%s]\n", tnd->name, mcsp.tag_NAME->name, mcsp.tag_NAME->ndef->name);
#endif
	/*}}}*/
	/*{{{  mcsp:leafproc -- SKIP, STOP, DIV, CHAOS*/
	i = -1;
	tnd = mcsp.node_LEAFPROC = tnode_newnodetype ("mcsp:leafproc", &i, 0, 0, 0, TNF_NONE);
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (mcsp_namemap_leafproc));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (mcsp_codegen_leafproc));
	tnd->ops = cops;

	i = -1;
	mcsp.tag_SKIP = tnode_newnodetag ("MCSPSKIP", &i, tnd, NTF_NONE);
	i = -1;
	mcsp.tag_STOP = tnode_newnodetag ("MCSPSTOP", &i, tnd, NTF_NONE);
	i = -1;
	mcsp.tag_DIV = tnode_newnodetag ("MCSPDIV", &i, tnd, NTF_NONE);
	i = -1;
	mcsp.tag_CHAOS = tnode_newnodetag ("MCSPCHAOS", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  mcsp:leaftype -- EVENTTYPE*/
	i = -1;
	tnd = mcsp.node_LEAFTYPE = tnode_newnodetype ("mcsp:leaftype", &i, 0, 0, 0, TNF_NONE);
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "mwsynctrans", 2, COMPOPTYPE (mcsp_mwsynctrans_leaftype));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "getdescriptor", 2, LANGOPTYPE (mcsp_getdescriptor_leaftype));
	tnode_setlangop (lops, "gettype", 2, LANGOPTYPE (mcsp_gettype_leaftype));
	tnode_setlangop (lops, "bytesfor", 2, LANGOPTYPE (mcsp_bytesfor_leaftype));
	tnode_setlangop (lops, "issigned", 2, LANGOPTYPE (mcsp_issigned_leaftype));
	tnode_setlangop (lops, "initialising_decl", 3, LANGOPTYPE (mcsp_initialising_decl_leaftype));
	tnd->lops = lops;

	i = -1;
	mcsp.tag_EVENTTYPE = tnode_newnodetag ("MCSPEVENTTYPE", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  mcsp:namenode -- EVENT, PROCDEF, CHAN, VAR*/
	i = -1;
	tnd = mcsp.node_NAMENODE = tnode_newnodetype ("mcsp:namenode", &i, 0, 1, 0, TNF_NONE);		/* subnames: 0 = name */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "postcheck", 2, COMPOPTYPE (mcsp_postcheck_namenode));
	tnode_setcompop (cops, "fetrans", 2, COMPOPTYPE (mcsp_fetrans_namenode));
	tnode_setcompop (cops, "mwsynctrans", 2, COMPOPTYPE (mcsp_mwsynctrans_namenode));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (mcsp_namemap_namenode));
/*	cops->gettype = mcsp_gettype_namenode; */
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "gettype", 2, LANGOPTYPE (mcsp_namenode_gettype));
	tnode_setlangop (lops, "bytesfor", 2, LANGOPTYPE (mcsp_namenode_bytesfor));
	tnode_setlangop (lops, "getname", 2, LANGOPTYPE (mcsp_namenode_getname));
	tnd->lops = lops;

	i = -1;
	mcsp.tag_EVENT = tnode_newnodetag ("MCSPEVENT", &i, tnd, NTF_NONE);
	i = -1;
	mcsp.tag_PROCDEF = tnode_newnodetag ("MCSPPROCDEF", &i, tnd, NTF_NONE);
	i = -1;
	mcsp.tag_CHAN = tnode_newnodetag ("MCSPCHAN", &i, tnd, NTF_NONE);
	i = -1;
	mcsp.tag_VAR = tnode_newnodetag ("MCSPVAR", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  mcsp:actionnode -- SYNC, CHANWRITE*/
	i = -1;
	tnd = tnode_newnodetype ("mcsp:actionnode", &i, 3, 0, 0, TNF_NONE);				/* subnodes: 0 = event(s), 1 = data/null, 2 = action-type */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "fetrans", 2, COMPOPTYPE (mcsp_fetrans_actionnode));
	tnode_setcompop (cops, "betrans", 2, COMPOPTYPE (mcsp_betrans_actionnode));
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (mcsp_typecheck_actionnode));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (mcsp_namemap_actionnode));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (mcsp_codegen_actionnode));
	tnd->ops = cops;

	i = -1;
	mcsp.tag_SYNC = tnode_newnodetag ("MCSPSYNC", &i, tnd, NTF_NONE);
	i = -1;
	mcsp.tag_CHANWRITE = tnode_newnodetag ("MCSPCHANWRITE", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  mcsp:constnode -- STRING, INTEGER*/
	i = -1;
	tnd = tnode_newnodetype ("mcsp:constnode", &i, 0, 0, 1, TNF_NONE);				/* hooks: data */
	tnd->hook_free = mcsp_constnode_hook_free;
	tnd->hook_copy = mcsp_constnode_hook_copy;
	tnd->hook_dumptree = mcsp_constnode_hook_dumptree;
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "constprop", 1, COMPOPTYPE (mcsp_constprop_const));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (mcsp_namemap_const));
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "isconst", 1, LANGOPTYPE (mcsp_isconst_const));
	tnode_setlangop (lops, "constvalof", 2, LANGOPTYPE (mcsp_constvalof_const));
	tnode_setlangop (lops, "valbyref", 1, LANGOPTYPE (mcsp_valbyref_const));
	tnd->ops = cops;

	i = -1;
	mcsp.tag_STRING = tnode_newnodetag ("MCSPSTRING", &i, tnd, NTF_NONE);
	i = -1;
	mcsp.tag_INTEGER = tnode_newnodetag ("MCSPINTEGER", &i, tnd, NTF_NONE);

	/*}}}*/

	return 0;
}
/*}}}*/


/*{{{  mcsp_process_feunit (feunit_t)*/
feunit_t mcsp_process_feunit = {
	.init_nodes = mcsp_process_init_nodes,
	.reg_reducers = NULL,
	.init_dfatrans = NULL,
	.post_setup = NULL,
	.ident = "mcsp-process"
};
/*}}}*/

