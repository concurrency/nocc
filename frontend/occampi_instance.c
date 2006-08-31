/*
 *	occampi_instance.c -- instance (PROC calls, etc.) handling for occampi
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
#include "usagecheck.h"
#include "fetrans.h"
#include "betrans.h"
#include "map.h"
#include "target.h"
#include "transputer.h"
#include "codegen.h"

/*}}}*/

/*{{{  private types*/
typedef struct TAG_builtinproc {
	const char *name;
	const char *keymatch;
	const char *symmatch;
	token_t *tok;
	int wsh;
	int wsl;
	void (*codegen)(tnode_t *, struct TAG_builtinproc *, codegen_t *);
	const char *descriptor;			/* fed into parser_descparse() */
	tnode_t *decltree;
} builtinproc_t;

typedef struct TAG_builtinprochook {
	builtinproc_t *biptr;
} builtinprochook_t;


/*}}}*/
/*{{{  forward decls*/
static void builtin_codegen_reschedule (tnode_t *node, builtinproc_t *builtin, codegen_t *cgen);
static void builtin_codegen_setpri (tnode_t *node, builtinproc_t *builtin, codegen_t *cgen);


/*}}}*/
/*{{{  private data*/
/* these are used to fill in workspace-sizes once target info is known */
#define BUILTIN_DS_MIN (-1)
#define BUILTIN_DS_IO (-2)
#define BUILTIN_DS_ALTIO (-3)
#define BUILTIN_DS_WAIT (-4)
#define BUILTIN_DS_MAX (-5)

static builtinproc_t builtins[] = {
	{"SETPRI", "SETPRI", NULL, NULL, 4, BUILTIN_DS_MIN, builtin_codegen_setpri, "PROC xxSETPRI (VAL INT newpri)\n", NULL},
	{"RESCHEDULE", "RESCHEDULE", NULL, NULL, 0, BUILTIN_DS_MIN, builtin_codegen_reschedule, NULL, NULL},
	{NULL, NULL, NULL, NULL, 0, 0, NULL, NULL, NULL}
};


/*}}}*/


/*{{{  builtinproc_t/builtinprochook_t routines*/
/*{{{  static builtinprochook_t *builtinprochook_create (builtinproc_t *builtin)*/
/*
 *	creates a new builtinprochook_t structure
 */
static builtinprochook_t *builtinprochook_create (builtinproc_t *builtin)
{
	builtinprochook_t *bph = (builtinprochook_t *)smalloc (sizeof (builtinprochook_t));

	bph->biptr = builtin;
	
	return bph;
}
/*}}}*/
/*{{{  static void builtinprochook_free (void *hook)*/
/*
 *	frees a builtinprochook_t structure
 */
static void builtinprochook_free (void *hook)
{
	builtinprochook_t *bph = (builtinprochook_t *)hook;

	if (bph) {
		sfree (bph);
	}
	return;
}
/*}}}*/
/*{{{  static void builtinprochook_dumphook (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	dumps a builtinprochook_t (debugging)
 */
static void builtinprochook_dumphook (tnode_t *node, void *hook, int indent, FILE *stream)
{
	builtinprochook_t *bph = (builtinprochook_t *)hook;

	occampi_isetindent (stream, indent);
	if (!hook) {
		fprintf (stream, "<builtinprochook name=\"(null)\" />\n");
	} else {
		builtinproc_t *builtin = bph->biptr;

		fprintf (stream, "<builtinprochook name=\"%s\" wsh=\"%d\" wsl=\"%d\" />\n", builtin->name, builtin->wsh, builtin->wsl);
	}
	return;
}
/*}}}*/

/*{{{  static void builtinproc_fixupmem (builtinproc_t *bpi, target_t *target)*/
/*
 *	fixes up memory-requirements in a builtinproc_t once target info is known
 */
static void builtinproc_fixupmem (builtinproc_t *bpi, target_t *target)
{
	if (bpi->wsl < 0) {
		switch (bpi->wsl) {
		case BUILTIN_DS_MIN:
			bpi->wsl = target->bws.ds_min;
			break;
		case BUILTIN_DS_IO:
			bpi->wsl = target->bws.ds_io;
			break;
		case BUILTIN_DS_ALTIO:
			bpi->wsl = target->bws.ds_altio;
			break;
		case BUILTIN_DS_WAIT:
			bpi->wsl = target->bws.ds_wait;
			break;
		case BUILTIN_DS_MAX:
			bpi->wsl = target->bws.ds_max;
			break;
		default:
			nocc_internal ("builtinproc_fixupmem(): unknown size value %d", bpi->wsl);
			break;
		}
	}
	return;
}
/*}}}*/
/*}}}*/


/*{{{  static int occampi_typecheck_instance (compops_t *cops, tnode_t *node, typecheck_t *tc)*/
/*
 *	does type-checking for an instance-node
 *	returns 1 to continue walk, 0 to stop
 */
static int occampi_typecheck_instance (compops_t *cops, tnode_t *node, typecheck_t *tc)
{
	tnode_t *fparamlist = typecheck_gettype (tnode_nthsubof (node, 0), NULL);
	tnode_t *aparamlist = tnode_nthsubof (node, 1);
	tnode_t **fp_items, **ap_items;
	int fp_nitems, ap_nitems;
	int fp_ptr, ap_ptr;
	int paramno;

#if 0
fprintf (stderr, "occampi_typecheck_instance: fparamlist=\n");
tnode_dumptree (fparamlist, 1, stderr);
fprintf (stderr, "occampi_typecheck_instance: aparamlist=\n");
tnode_dumptree (aparamlist, 1, stderr);
#endif

	if (!fparamlist) {
		fp_items = NULL;
		fp_nitems = 0;
	} else if (parser_islistnode (fparamlist)) {
		fp_items = parser_getlistitems (fparamlist, &fp_nitems);
	} else {
		fp_items = &fparamlist;
		fp_nitems = 1;
	}
	if (!aparamlist) {
		ap_items = NULL;
		ap_nitems = 0;
	} else if (parser_islistnode (aparamlist)) {
		ap_items = parser_getlistitems (aparamlist, &ap_nitems);
	} else {
		ap_items = &aparamlist;
		ap_nitems = 1;
	}

	for (paramno = 1, fp_ptr = 0, ap_ptr = 0; (fp_ptr < fp_nitems) && (ap_ptr < ap_nitems);) {
		tnode_t *ftype, *atype;

		/* skip over hidden parameters */
		if (fp_items[fp_ptr]->tag == opi.tag_HIDDENPARAM) {
			fp_ptr++;
			continue;
		}
		if (ap_items[ap_ptr]->tag == opi.tag_HIDDENPARAM) {
			ap_ptr++;
			continue;
		}
		ftype = typecheck_gettype (fp_items[fp_ptr], NULL);
		atype = typecheck_gettype (ap_items[ap_ptr], ftype);
#if 0
fprintf (stderr, "occampi_typecheck_instance: ftype=\n");
tnode_dumptree (ftype, 1, stderr);
fprintf (stderr, "occampi_typecheck_instance: atype=\n");
tnode_dumptree (atype, 1, stderr);
#endif

		if (!typecheck_typeactual (ftype, atype, node, tc)) {
			typecheck_error (node, tc, "incompatible types for parameter %d", paramno);
		}
		fp_ptr++;
		ap_ptr++;
		paramno++;
	}

	/* skip over any left-over hidden params */
	for (; (fp_ptr < fp_nitems) && (fp_items[fp_ptr]->tag == opi.tag_HIDDENPARAM); fp_ptr++);
	for (; (ap_ptr < ap_nitems) && (ap_items[ap_ptr]->tag == opi.tag_HIDDENPARAM); ap_ptr++);
	if (fp_ptr < fp_nitems) {
		typecheck_error (node, tc, "too few actual parameters");
	} else if (ap_ptr < ap_nitems) {
		typecheck_error (node, tc, "too many actual parameters");
	}

	return 0;
}
/*}}}*/
/*{{{  static int occampi_usagecheck_instance (langops_t *lops, tnode_t *node, uchk_state_t *uc)*/
/*
 *	does usage-checking for an instance node
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_usagecheck_instance (langops_t *lops, tnode_t *node, uchk_state_t *uc)
{
	tnode_t *fparamlist = typecheck_gettype (tnode_nthsubof (node, 0), NULL);
	tnode_t *aparamlist = tnode_nthsubof (node, 1);
	tnode_t **fp_items, **ap_items;
	int fp_nitems, ap_nitems;
	int fp_ptr, ap_ptr;
	int paramno;

	if (!fparamlist) {
		fp_items = NULL;
		fp_nitems = 0;
	} else if (parser_islistnode (fparamlist)) {
		fp_items = parser_getlistitems (fparamlist, &fp_nitems);
	} else {
		fp_items = &fparamlist;
		fp_nitems = 1;
	}
	if (!aparamlist) {
		ap_items = NULL;
		ap_nitems = 0;
	} else if (parser_islistnode (aparamlist)) {
		ap_items = parser_getlistitems (aparamlist, &ap_nitems);
	} else {
		ap_items = &aparamlist;
		ap_nitems = 1;
	}

	for (paramno = 1, fp_ptr = 0, ap_ptr = 0; (fp_ptr < fp_nitems) && (ap_ptr < ap_nitems);) {
		tnode_t *ftype;
		occampi_typeattr_t fattr = TYPEATTR_NONE;
		uchk_mode_t savedmode = uc->defmode;

		/* skip over hidden parameters */
		if (fp_items[fp_ptr]->tag == opi.tag_HIDDENPARAM) {
			fp_ptr++;
			continue;
		}
		if (ap_items[ap_ptr]->tag == opi.tag_HIDDENPARAM) {
			ap_ptr++;
			continue;
		}
		ftype = typecheck_gettype (fp_items[fp_ptr], NULL);
		if (!ftype) {
			tnode_error (node, "occampi_usagecheck_instance(): no type on parameter %d!", paramno);
			return 0;
		}

		fattr = (occampi_typeattr_t)tnode_getchook (ftype, opi.chook_typeattr);

#if 0
fprintf (stderr, "occampi_usagecheck_instance: fattr = 0x%8.8x, ftype =\n", (unsigned int)fattr);
tnode_dumptree (ftype, 1, stderr);
fprintf (stderr, "occampi_usagecheck_instance: aparam =\n");
tnode_dumptree (ap_items[ap_ptr], 1, stderr);
#endif

		if (fattr & TYPEATTR_MARKED_IN) {
			uc->defmode = USAGE_INPUT;
		} else if (fattr & TYPEATTR_MARKED_OUT) {
			uc->defmode = USAGE_OUTPUT;
		} else if (fp_items[fp_ptr]->tag == opi.tag_NPARAM) {
			uc->defmode = USAGE_WRITE;
		} else if (fp_items[fp_ptr]->tag == opi.tag_NVALPARAM) {
			uc->defmode = USAGE_READ;
		}

		/* usagecheck the actual parameter */
		usagecheck_subtree (ap_items[ap_ptr], uc);

		uc->defmode = savedmode;

		fp_ptr++;
		ap_ptr++;
		paramno++;
	}

	return 0;
}
/*}}}*/
/*{{{  static int occampi_namemap_instance (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	does name-mapping for an instance-node
 *	returns 1 to continue walk, 0 to stop
 */
static int occampi_namemap_instance (compops_t *cops, tnode_t **node, map_t *map)
{
	tnode_t *bename, *instance, *ibody, *namenode;
	name_t *name;

	/* map parameters and called name */
	map_submapnames (tnode_nthsubaddr (*node, 1), map);
	map_submapnames (tnode_nthsubaddr (*node, 0), map);

	namenode = tnode_nthsubof (*node, 0);
#if 0
fprintf (stderr, "occampi_namemap_instance(): instance of:\n");
tnode_dumptree (namenode, 1, stderr);
#endif
	if (namenode->tag->ndef == opi.node_NAMENODE) {
		name = tnode_nthnameof (namenode, 0);
		instance = NameDeclOf (name);

		/* body should be a back-end BLOCK */
		ibody = tnode_nthsubof (instance, 2);
	} else if (namenode->tag == opi.tag_BUILTINPROC) {
		/* do nothing else */
		builtinprochook_t *bph = (builtinprochook_t *)tnode_nthhookof (namenode, 0);
		builtinproc_t *builtin = bph->biptr;

		builtinproc_fixupmem (builtin, map->target);
		bename = map->target->newname (*node, NULL, map, builtin->wsh, builtin->wsl, 0, 0, 0, 0);
		*node = bename;
		return 0;
	} else {
		nocc_internal ("occampi_namemap_instance(): don\'t know how to handle [%s]", namenode->tag->name);
		return 0;
	}
#if 0
fprintf (stderr, "occampi_namemap_instance: instance body is:\n");
tnode_dumptree (ibody, 1, stderr);
#endif

	bename = map->target->newblockref (ibody, *node, map);
#if 0
fprintf (stderr, "occampi_namemap_instance: created new blockref =\n");
tnode_dumptree (bename, 1, stderr);
#endif

	*node = bename;
	return 0;
}
/*}}}*/
/*{{{  static int occampi_codegen_instance (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	generates code for an instance
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_codegen_instance (compops_t *cops, tnode_t *node, codegen_t *cgen)
{
	name_t *name;
	tnode_t *namenode;
	tnode_t *params = tnode_nthsubof (node, 1);
	tnode_t *instance, *ibody;
	int ws_size, ws_offset, vs_size, ms_size, adjust;

	namenode = tnode_nthsubof (node, 0);

	if (namenode->tag->ndef == opi.node_NAMENODE) {
		/*{{{  instance of a name (e.g. PROC definition)*/
		name = tnode_nthnameof (namenode, 0);
		instance = NameDeclOf (name);
		ibody = tnode_nthsubof (instance, 2);

		codegen_check_beblock (ibody, cgen, 1);

		/* get size of this block */
		cgen->target->be_getblocksize (ibody, &ws_size, &ws_offset, &vs_size, &ms_size, &adjust, NULL);

		/* FIXME: load parameters in reverse order, into -4, -8, ... */
		if (!params) {
			/* no parameters! */
		} else if (parser_islistnode (params)) {
			int nitems, i, wsoff;
			tnode_t **items = parser_getlistitems (params, &nitems);

			for (i=nitems - 1, wsoff = -4; i>=0; i--, wsoff -= 4) {
				codegen_callops (cgen, loadparam, items[i], PARAM_REF);
				codegen_callops (cgen, storelocal, wsoff);
			}
		} else {
			/* single parameter */
			codegen_callops (cgen, loadparam, params, PARAM_REF);
			codegen_callops (cgen, storelocal, -4);
		}

		codegen_callops (cgen, callnamelabel, name, adjust);
		/*}}}*/
	} else if (namenode->tag == opi.tag_BUILTINPROC) {
		/*{{{  instance of a built-in PROC*/
		builtinprochook_t *bph = (builtinprochook_t *)tnode_nthhookof (namenode, 0);
		builtinproc_t *builtin = bph->biptr;

		if (builtin->codegen) {
			builtin->codegen (node, builtin, cgen);
		} else {
			nocc_warning ("occampi_codegen_instance(): don\'t know how to code for built-in PROC [%s]", builtin->name);
			codegen_callops (cgen, comment, "BUILTINPROC instance of [%s]", builtin->name);
		}
		/*}}}*/
	} else {
		nocc_internal ("occampi_codegen_instance(): don\'t know how to handle [%s]", namenode->tag->name);
	}

	return 0;
}
/*}}}*/


/*{{{  static void builtin_codegen_reschedule (tnode_t *node, builtinproc_t *builtin, codegen_t *cgen)*/
/*
 *	generates code for a RESCHEDULE()
 */
static void builtin_codegen_reschedule (tnode_t *node, builtinproc_t *builtin, codegen_t *cgen)
{
	codegen_callops (cgen, tsecondary, I_RESCHEDULE);
	return;
}
/*}}}*/
/*{{{  static void builtin_codegen_setpri (tnode_t *node, builtinproc_t *builtin, codegen_t *cgen)*/
/*
 *	generates code for a SETPRI(VAL INT)
 */
static void builtin_codegen_setpri (tnode_t *node, builtinproc_t *builtin, codegen_t *cgen)
{
	tnode_t *param = tnode_nthsubof (node, 1);

#if 0
fprintf (stderr, "builtin_codegen_setpri(): param =\n");
tnode_dumptree (param, 1, stderr);
#endif
	codegen_callops (cgen, loadname, param, 0);
	codegen_callops (cgen, tsecondary, I_SETPRI);
	return;
}
/*}}}*/


/*{{{  static tnode_t *occampi_gettype_builtinproc (langops_t *lops, tnode_t *node, tnode_t *defaulttype)*/
/*
 *	returns the type of a built-in PROC
 */
static tnode_t *occampi_gettype_builtinproc (langops_t *lops, tnode_t *node, tnode_t *defaulttype)
{
	builtinprochook_t *bph;
	builtinproc_t *builtin;

	if (node->tag != opi.tag_BUILTINPROC) {
		nocc_internal ("occampi_gettype_builtinproc(): node not BUILTINPROC");
		return NULL;
	}
	bph = (builtinprochook_t *)tnode_nthhookof (node, 0);
	builtin = bph->biptr;

	if (!builtin) {
		nocc_internal ("occampi_gettype_builtinproc(): builtin missing from hook");
		return NULL;
	}
#if 0
fprintf (stderr, "occampi_gettype_builtinproc(): [%s] builtin->decltree =\n", builtin->name);
tnode_dumptree (builtin->decltree, 1, stderr);
#endif

	if (!builtin->decltree && builtin->descriptor) {
		/*{{{  parse descriptor and extract declaration-tree*/
		lexfile_t *lexbuf;
		tnode_t *decltree;

		lexbuf = lexer_openbuf (NULL, occampi_parser.langname, (char *)builtin->descriptor);
		if (!lexbuf) {
			nocc_error ("occampi_gettype_builtinproc(): failed to open descriptor..");
			return NULL;
		}

		decltree = parser_descparse (lexbuf);
		lexer_close (lexbuf);

		if (!decltree) {
			nocc_error ("occampi_gettype_builtinproc(): failed to parse descriptor..");
			return NULL;
		}

		/* prescope and scope the declaration tree -- to fixup parameters and type */
		if (prescope_tree (&decltree, &occampi_parser)) {
			nocc_error ("occampi_gettype_builtinproc(): failed to prescope descriptor..");
			return NULL;
		}
		if (scope_tree (decltree, &occampi_parser)) {
			nocc_error ("occampi_gettype_builtinproc(): failed to scope descriptor..");
			return NULL;
		}
		if (typecheck_tree (decltree, &occampi_parser)) {
			nocc_error ("occampi_gettype_builtinproc(): failed to typecheck descriptor..");
			return NULL;
		}

		/* okay, attach declaration tree! */
		builtin->decltree = decltree;

#if 0
fprintf (stderr, "occampi_gettype_builtinproc(): parsed descriptor and got type:\n");
tnode_dumptree (decltype, 1, stderr);
#endif
		/*}}}*/
	}

	/* if we have a declaration, use its type */
	return builtin->decltree ? typecheck_gettype (builtin->decltree, defaulttype) : defaulttype;
}
/*}}}*/

/*{{{  static void occampi_reduce_builtinproc (dfastate_t *dfast, parsepriv_t *pp, void *rarg)*/
/*
 *	reduces a built-in PROC keyword token to a tree-node
 */
static void occampi_reduce_builtinproc (dfastate_t *dfast, parsepriv_t *pp, void *rarg)
{
	token_t *tok = parser_gettok (pp);
	int i;

#if 0
fprintf (stderr, "occampi_reduce_builtinproc(): ..\n");
#endif
	for (i=0; builtins[i].name; i++) {
		if (lexer_tokmatch (builtins[i].tok, tok)) {
			/*{{{  got a match*/
			tnode_t *biname;

			biname = tnode_create (opi.tag_BUILTINPROC, tok->origin, builtinprochook_create (&(builtins[i])));
			dfa_pushnode (dfast, biname);

			lexer_freetoken (tok);
			return;
			/*}}}*/
		}
	}
	parser_pushtok (pp, tok);
	parser_error (tok->origin, "unknown built-in PROC [%s]", lexer_stokenstr (tok));

	return;
}
/*}}}*/


/*{{{  static int occampi_instance_init_nodes (void)*/
/*
 *	initialises nodes for occam-pi instances
 */
static int occampi_instance_init_nodes (void)
{
	tndef_t *tnd;
	int i;
	compops_t *cops;
	langops_t *lops;

	/*{{{  occampi:instancenode -- PINSTANCE*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:instancenode", &i, 2, 0, 1, TNF_NONE);		/* subnodes: names; params */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (occampi_typecheck_instance));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (occampi_namemap_instance));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (occampi_codegen_instance));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "do_usagecheck", 2, LANGOPTYPE (occampi_usagecheck_instance));
	tnd->lops = lops;

	i = -1;
	opi.tag_PINSTANCE = tnode_newnodetag ("PINSTANCE", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  occampi:builtinproc -- BUILTINPROC*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:builtinproc", &i, 0, 0, 1, TNF_NONE);			/* hook: builtinprochook_t */
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "gettype", 2, LANGOPTYPE (occampi_gettype_builtinproc));
	tnd->lops = lops;
	tnd->hook_dumptree = builtinprochook_dumphook;
	tnd->hook_free = builtinprochook_free;

	i = -1;
	opi.tag_BUILTINPROC = tnode_newnodetag ("BUILTINPROC", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  setup builtins*/
	for (i=0; builtins[i].name; i++) {
		if (!builtins[i].tok) {
			if (builtins[i].keymatch) {
				builtins[i].tok = lexer_newtoken (KEYWORD, builtins[i].keymatch);
			} else if (builtins[i].symmatch) {
				builtins[i].tok = lexer_newtoken (SYMBOL, builtins[i].symmatch);
			} else {
				nocc_internal ("occampi_instance_init_nodes(): built-in error, name = [%s]", builtins[i].name);
			}
		}
	}

	/*}}}*/

	return 0;
}
/*}}}*/
/*{{{  static int occampi_instance_reg_reducers (void)*/
/*
 *	registers reducers for instance nodes
 */
static int occampi_instance_reg_reducers (void)
{
	parser_register_grule ("opi:pinstancereduce", parser_decode_grule ("SN1N+N+V0C3R-", opi.tag_PINSTANCE));

	parser_register_reduce ("Roccampi:builtinproc", occampi_reduce_builtinproc, NULL);

	return 0;
}
/*}}}*/
/*{{{  static dfattbl_t **occampi_instance_init_dfatrans (int *ntrans)*/
/*
 *	creates and returns DFA transition tables for instance nodes
 */
static dfattbl_t **occampi_instance_init_dfatrans (int *ntrans)
{
	DYNARRAY (dfattbl_t *, transtbl);
	int i;
	char *tbuf;

	dynarray_init (transtbl);
	dynarray_add (transtbl, dfa_bnftotbl ("occampi:aparamlist ::= ( -@@) {<opi:nullset>} | { occampi:expr @@, 1 } )"));
	dynarray_add (transtbl, dfa_transtotbl ("occampi:namestartname +:= [ 0 @@( 1 ] [ 1 occampi:aparamlist 2 ] " \
				"[ 2 @@) 3 ] [ 3 {<opi:pinstancereduce>} -* ]"));

	dynarray_add (transtbl, dfa_transtotbl ("occampi:builtinprocinstancei ::= [ 0 @@( 1 ] [ 1 {Roccampi:builtinproc} ] [ 1 occampi:aparamlist 2 ] [ 2 @@) 3 ] [ 3 {<opi:pinstancereduce>} -* ]"));

	/* run through built-in PROCs generating starting matches */
	tbuf = (char *)smalloc (256);
	for (i=0; builtins[i].name; i++) {
		if (builtins[i].keymatch) {
			sprintf (tbuf, "occampi:builtinprocinstance +:= [ 0 +@%s 1 ] [ 1 -@@( <occampi:builtinprocinstancei> ]", builtins[i].keymatch);
		} else if (builtins[i].symmatch) {
			sprintf (tbuf, "occampi:builtinprocinstance +:= [ 0 +@@%s 1 ] [ 1 -@@( <occampi:builtinprocinstancei> ]", builtins[i].symmatch);
		}
		dynarray_add (transtbl, dfa_transtotbl (tbuf));
	}
	sfree (tbuf);


	*ntrans = DA_CUR (transtbl);
	return DA_PTR (transtbl);
}
/*}}}*/


/*{{{  occampi_instance_feunit (feunit_t)*/
feunit_t occampi_instance_feunit = {
	init_nodes: occampi_instance_init_nodes,
	reg_reducers: occampi_instance_reg_reducers,
	init_dfatrans: occampi_instance_init_dfatrans,
	post_setup: NULL
};
/*}}}*/

