/*
 *	occampi_function.c -- occam-pi FUNCTIONs
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
#include "library.h"
#include "prescope.h"
#include "precheck.h"
#include "typecheck.h"
#include "usagecheck.h"
#include "fetrans.h"
#include "betrans.h"
#include "map.h"
#include "target.h"
#include "transputer.h"
#include "codegen.h"
#include "langops.h"
#include "treeops.h"


/*}}}*/
/*{{{  private types*/
typedef struct TAG_builtinfunction {
	const char *name;
	const char *keymatch;
	const char *symmatch;
	token_t *tok;
	int wsh;
	int wsl;
	void (*betrans)(tnode_t **, struct TAG_builtinfunction *, betrans_t *);
	void (*premap)(tnode_t **, struct TAG_builtinfunction *, map_t *);
	void (*namemap)(tnode_t **, struct TAG_builtinfunction *, map_t *);
	void (*codegen)(tnode_t *, struct TAG_builtinfunction *, codegen_t *);
	const char *descriptor;			/* fed into parser_descparse() */
	tnode_t *decltree;
} builtinfunction_t;

typedef struct TAG_builtinfunctionhook {
	builtinfunction_t *biptr;
} builtinfunctionhook_t;

/*}}}*/
/*{{{  forward decls*/
static void occampi_bifunc_premap_getpri (tnode_t **node, builtinfunction_t *builtin, map_t *map);
static void occampi_bifunc_codegen_getpri (tnode_t *node, builtinfunction_t *builtin, codegen_t *cgen);


/*}}}*/
/*{{{  private data*/
#define BUILTIN_DS_MIN (-1)
#define BUILTIN_DS_IO (-2)
#define BUILTIN_DS_ALTIO (-3)
#define BUILTIN_DS_WAIT (-4)
#define BUILTIN_DS_MAX (-5)

static builtinfunction_t builtins[] = {
	{"GETPRI", "GETPRI", NULL, NULL, 0, BUILTIN_DS_MIN, NULL, occampi_bifunc_premap_getpri, NULL, occampi_bifunc_codegen_getpri, "INT FUNCTION xxGETPRI ()\n", NULL},
	{NULL, NULL, NULL, NULL, 0, 0, NULL, NULL, NULL, NULL, NULL, NULL}
};

/*}}}*/



/*{{{  builtinfunction_t/builtinfunctionhook_t routines*/
/*{{{  static builtinfunctionhook_t *builtinfunctionhook_create (builtinfunction_t *builtin)*/
/*
 *	creates a new builtinfunctionhook_t structure
 */
static builtinfunctionhook_t *builtinfunctionhook_create (builtinfunction_t *builtin)
{
	builtinfunctionhook_t *bfh = (builtinfunctionhook_t *)smalloc (sizeof (builtinfunctionhook_t));

	bfh->biptr = builtin;
	
	return bfh;
}
/*}}}*/
/*{{{  static void builtinfunctionhook_free (void *hook)*/
/*
 *	frees a builtinfunctionhook_t structure
 */
static void builtinfunctionhook_free (void *hook)
{
	builtinfunctionhook_t *bfh = (builtinfunctionhook_t *)hook;

	if (bfh) {
		sfree (bfh);
	}
	return;
}
/*}}}*/
/*{{{  static void builtinfunctionhook_dumphook (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	dumps a builtinfunctionhook_t (debugging)
 */
static void builtinfunctionhook_dumphook (tnode_t *node, void *hook, int indent, FILE *stream)
{
	builtinfunctionhook_t *bfh = (builtinfunctionhook_t *)hook;

	occampi_isetindent (stream, indent);
	if (!hook) {
		fprintf (stream, "<builtinfunctionhook name=\"(null)\" />\n");
	} else {
		builtinfunction_t *builtin = bfh->biptr;

		fprintf (stream, "<builtinfunctionhook name=\"%s\" wsh=\"%d\" wsl=\"%d\" />\n", builtin->name, builtin->wsh, builtin->wsl);
	}
	return;
}
/*}}}*/

/*{{{  static void builtinfunction_fixupmem (builtinfunction_t *bfi, target_t *target)*/
/*
 *	fixes up memory-requirements in a builtinfunction_t once target info is known
 */
static void builtinfunction_fixupmem (builtinfunction_t *bfi, target_t *target)
{
	if (bfi->wsl < 0) {
		switch (bfi->wsl) {
		case BUILTIN_DS_MIN:
			bfi->wsl = target->bws.ds_min;
			break;
		case BUILTIN_DS_IO:
			bfi->wsl = target->bws.ds_io;
			break;
		case BUILTIN_DS_ALTIO:
			bfi->wsl = target->bws.ds_altio;
			break;
		case BUILTIN_DS_WAIT:
			bfi->wsl = target->bws.ds_wait;
			break;
		case BUILTIN_DS_MAX:
			bfi->wsl = target->bws.ds_max;
			break;
		default:
			nocc_internal ("builtinfunction_fixupmem(): unknown size value %d", bfi->wsl);
			break;
		}
	}
	return;
}
/*}}}*/
/*}}}*/


/*{{{  static void occampi_bifunc_premap_getpri (tnode_t **node, builtinfunction_t *builtin, map_t *map)*/
/*
 *	does pre-mapping for a GETPRI() instance.  Get the whole instance.
 */
static void occampi_bifunc_premap_getpri (tnode_t **node, builtinfunction_t *builtin, map_t *map)
{
#if 0
fprintf (stderr, "occampi_bifunc_premap_getpri(): creating result!\n");
#endif
	*node = map->target->newresult (*node, map);
	return;
}
/*}}}*/
/*{{{  static void occampi_bifunc_codegen_getpri (tnode_t *node, builtinfunction_t *builtin, codegen_t *cgen)*/
/*
 *	does code-gen for a GETPRI() instance.  Get the whole instance.
 */
static void occampi_bifunc_codegen_getpri (tnode_t *node, builtinfunction_t *builtin, codegen_t *cgen)
{
	codegen_callops (cgen, tsecondary, I_GETPRI);
	return;
}
/*}}}*/


/*{{{  static int occampi_codegen_valof (tnode_t *node, codegen_t *cgen)*/
/*
 *	generates code for a VALOF
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_codegen_valof (tnode_t *node, codegen_t *cgen)
{
	tnode_t *body = tnode_nthsubof (node, 1);
	tnode_t *results = tnode_nthsubof (node, 0);
	int nresults, resultbytes;

	/* generate body */
	codegen_subcodegen (body, cgen);

	/* and results */
	codegen_subcodegen (results, cgen);

	if (results) {
		if (parser_islistnode (results)) {
			tnode_t **items = parser_getlistitems (results, &nresults);
			int i;

#if 0
fprintf (stderr, "occampi_codegen_valof(): list of results, %d items\n", nresults);
#endif
			for (i=0, resultbytes=0; i<nresults; i++) {
				int thissize, indir;

				if (cgen->target->be_typesize (items[i], &thissize, &indir)) {
					codegen_error (cgen, "occampi_codegen_valof(): cannot get typesize for [%s]", items[i]->tag->name);
					thissize = 0;
				}
#if 0
fprintf (stderr, "occampi_codegen_valof(): result %d, thissize = %d\n", i, thissize);
#endif

				if (thissize > 0) {
					/* round up whole slots */
					if (thissize & (cgen->target->slotsize - 1)) {
						thissize = (thissize & ~(cgen->target->slotsize - 1)) + cgen->target->slotsize;
					}
					resultbytes += thissize;
				} else {
					codegen_warning (cgen, "occampi_codegen_valof(): result %d has size %d\n", i, thissize);
				}
			}
		} else {
			int thissize, indir;

			if (cgen->target->be_typesize (results, &thissize, &indir)) {
				codegen_error (cgen, "occampi_codegen_valof(): cannot get typesize for [%s]", results->tag->name);
				resultbytes = 0;
			} else {
				if (thissize & (cgen->target->slotsize - 1)) {
					thissize = (thissize & ~(cgen->target->slotsize - 1)) + cgen->target->slotsize;
				}
				resultbytes = thissize;
			}

#if 0
fprintf (stderr, "occampi_codegen_valof(): single result, thissize = %d\n", thissize);
#endif
			if (resultbytes <= 0) {
				codegen_warning (cgen, "occampi_codegen_valof(): result has size %d\n", resultbytes);
				resultbytes = 0;
			}
		}
	} else {
		resultbytes = 0;
	}

#if 0
fprintf (stderr, "occampi_codegen_valof(): resultbytes = %d\n", resultbytes);
#endif
	nresults = resultbytes / cgen->target->slotsize;

	codegen_callops (cgen, funcreturn, nresults);

	return 0;
}
/*}}}*/


/*{{{  static int occampi_typecheck_finstance (tnode_t *node, typecheck_t *tc)*/
/*
 *	does type-checking for a function instance-node
 *	returns 1 to continue walk, 0 to stop
 */
static int occampi_typecheck_finstance (tnode_t *node, typecheck_t *tc)
{
	tnode_t *fname = tnode_nthsubof (node, 0);
	tnode_t *functype;
	tnode_t *fparamlist;
	tnode_t *aparamlist;
	tnode_t **fp_items, **ap_items;
	int fp_nitems, ap_nitems;
	int fp_ptr, ap_ptr;
	int paramno;

	if ((fname->tag != opi.tag_NFUNCDEF) && (fname->tag != opi.tag_BUILTINFUNCTION)) {
		typecheck_error (node, tc, "called name is not a function");
	}

	functype = typecheck_gettype (fname, NULL);
	fparamlist = tnode_nthsubof (functype, 1);
	aparamlist = tnode_nthsubof (node, 1);
#if 0
fprintf (stderr, "occampi_typecheck_finstance: fparamlist=\n");
tnode_dumptree (fparamlist, 1, stderr);
fprintf (stderr, "occampi_typecheck_finstance: aparamlist=\n");
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
fprintf (stderr, "occampi_typecheck_finstance: ftype=\n");
tnode_dumptree (ftype, 1, stderr);
fprintf (stderr, "occampi_typecheck_finstance: atype=\n");
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
/*{{{  static tnode_t *occampi_gettype_finstance (tnode_t *node, tnode_t *defaulttype)*/
/*
 *	returns the type of a FUNCTION instance (= return-type from FUNCTIONTYPE)
 */
static tnode_t *occampi_gettype_finstance (tnode_t *node, tnode_t *defaulttype)
{
	tnode_t *fname = tnode_nthsubof (node, 0);

	if (fname->tag->ndef == opi.node_NAMENODE) {
		name_t *finame = tnode_nthnameof (fname, 0);
		tnode_t *ftype = NameTypeOf (finame);

#if 0
fprintf (stderr, "occampi_gettype_finstance(): type = [%s]:\n", ftype->tag->name);
tnode_dumptree (ftype, 1, stderr);
#endif
		return tnode_nthsubof (ftype, 0);
	} else if (fname->tag == opi.tag_BUILTINFUNCTION) {
		tnode_t *type = typecheck_gettype (fname, NULL);

		if (!type) {
			builtinfunctionhook_t *bfh = (builtinfunctionhook_t *)tnode_nthhookof (fname, 0);
			builtinfunction_t *builtin = bfh->biptr;

			nocc_error ("occampi_gettype_finstance(): failed to get type of builtin FUNCTION [%s]", builtin->name ? builtin->name : "<unknown>");
		}
#if 0
fprintf (stderr, "occampi_gettype_finstance(): [BUILTINFUNCTION]: returned type from gettype():\n");
tnode_dumptree (type, 1, stderr);
#endif
		return tnode_nthsubof (type, 0);		/* return-type */
	}
	return NULL;
}
/*}}}*/
/*{{{  static int occampi_fetrans_finstance (tnode_t **node, fetrans_t *fe)*/
/*
 *	does front-end transforms on a FUNCTION instance
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_fetrans_finstance (tnode_t **node, fetrans_t *fe)
{
	tnode_t **aparams = tnode_nthsubaddr (*node, 1);

	if (!parser_islistnode (*aparams)) {
		/* make a singleton or empty list */
		tnode_t *tmp = parser_newlistnode ((*node)->org_file);

		if (*aparams) {
			parser_addtolist (tmp, *aparams);
		}
		*aparams = tmp;
	}

	return 1;
}
/*}}}*/
/*{{{  static int occampi_betrans_finstance (tnode_t **node, betrans_t *be)*/
/*
 *	does back-end transforms on a FUNCTION instance
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_betrans_finstance (tnode_t **node, betrans_t *be)
{
	tnode_t *fnamenode;
	tnode_t *ftype;

	betrans_subtree (tnode_nthsubaddr (*node, 0), be);
	/* do betrans on params after we've messed around with them */

	fnamenode = tnode_nthsubof (*node, 0);		/* name of FUNCTION being instanced */
	ftype = typecheck_gettype (fnamenode, NULL);

	if (!ftype || (ftype->tag != opi.tag_FUNCTIONTYPE)) {
		tnode_error (*node, "type of function not FUNCTIONTYPE");
		return 0;
	}

#if 0
fprintf (stderr, "occampi_betrans_finstance(): function type is:\n");
tnode_dumptree (ftype, 1, stderr);
#endif

	if (fnamenode->tag == opi.tag_BUILTINFUNCTION) {
		/* might have betrans to do on it directly */
		builtinfunctionhook_t *bfh = (builtinfunctionhook_t *)tnode_nthhookof (fnamenode, 0);
		builtinfunction_t *builtin = bfh->biptr;

		/* do fixup here */
		if (builtin) {
			builtinfunction_fixupmem (builtin, be->target);
		}
		if (builtin && builtin->betrans) {
			builtin->betrans (node, builtin, be);
		}
	}

	betrans_subtree (tnode_nthsubaddr (*node, 1), be);

	return 0;
}
/*}}}*/
/*{{{  static int occampi_premap_finstance (tnode_t **node, map_t *map)*/
/*
 *	does pre-mapping for a function instance-node
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_premap_finstance (tnode_t **node, map_t *map)
{
	tnode_t *fnamenode;

	map_subpremap (tnode_nthsubaddr (*node, 0), map);		/* premap on name */
	map_subpremap (tnode_nthsubaddr (*node, 1), map);		/* premap on parameters */

	fnamenode = tnode_nthsubof (*node, 0);

#if 0
fprintf (stderr, "occampi_premap_finstance(): premapping!  fnamenode =\n");
tnode_dumptree (fnamenode, 1, stderr);
#endif
	if (fnamenode->tag == opi.tag_BUILTINFUNCTION) {
		/* might have to premap it */
		builtinfunctionhook_t *bfh = (builtinfunctionhook_t *)tnode_nthhookof (fnamenode, 0);
		builtinfunction_t *builtin = bfh->biptr;

		if (builtin && builtin->premap) {
			builtin->premap (node, builtin, map);
		}
	}

	return 0;
}
/*}}}*/
/*{{{  static int occampi_namemap_finstance (tnode_t **node, map_t *map)*/
/*
 *	does name-mapping for a function instance-node
 *	returns 1 to continue walk, 0 to stop
 */
static int occampi_namemap_finstance (tnode_t **node, map_t *map)
{
	tnode_t *bename, *finstance, *ibody, *namenode;
	name_t *name;

	/* map parameters and called name */
	map_submapnames (tnode_nthsubaddr (*node, 1), map);
	map_submapnames (tnode_nthsubaddr (*node, 0), map);

	namenode = tnode_nthsubof (*node, 0);
#if 0
fprintf (stderr, "occampi_namemap_finstance(): finstance of:\n");
tnode_dumptree (namenode, 1, stderr);
#endif
	if (namenode->tag->ndef == opi.node_NAMENODE) {
		name = tnode_nthnameof (namenode, 0);
		finstance = NameDeclOf (name);

		/* body should be a back-end BLOCK */
		ibody = tnode_nthsubof (finstance, 2);

#if 0
fprintf (stderr, "occampi_namemap_finstance: finstance body is:\n");
tnode_dumptree (ibody, 1, stderr);
#endif

		bename = map->target->newblockref (ibody, *node, map);
#if 0
fprintf (stderr, "occampi_namemap_finstance: created new blockref =\n");
tnode_dumptree (bename, 1, stderr);
#endif

		/* tnode_setnthsub (*node, 0, bename); */
		*node = bename;
	} else if (namenode->tag == opi.tag_BUILTINFUNCTION) {
		builtinfunctionhook_t *bfh = (builtinfunctionhook_t *)tnode_nthhookof (namenode, 0);
		builtinfunction_t *builtin = bfh->biptr;

		if (builtin->namemap) {
			builtin->namemap (node, builtin, map);
		}
	} else {
		nocc_internal ("occampi_namemap_finstance(): don\'t know how to handle [%s]", namenode->tag->name);
		return 0;
	}
	return 0;
}
/*}}}*/
/*{{{  static int occampi_codegen_finstance (tnode_t *node, codegen_t *cgen)*/
/*
 *	generates code for an finstance
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_codegen_finstance (tnode_t *node, codegen_t *cgen)
{
	name_t *name;
	tnode_t *namenode;
	tnode_t *params = tnode_nthsubof (node, 1);
	tnode_t *finstance, *ibody;
	int ws_size, ws_offset, vs_size, ms_size, adjust;
	tnode_t *resulttype;

	namenode = tnode_nthsubof (node, 0);

	if (namenode->tag->ndef == opi.node_NAMENODE) {
		/*{{{  finstance of a name (e.g. FUNCTION definition)*/
		int nresults = 0;

		name = tnode_nthnameof (namenode, 0);
		finstance = NameDeclOf (name);
		ibody = tnode_nthsubof (finstance, 2);

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

		codegen_callops (cgen, callnamedlabel, NameNameOf (name), adjust);

		/* results left on the stack -- indicate appropriately
		 * this is done by bytesfor()ing the formal result types (for which we only have type-information!) */
		resulttype = NameTypeOf (name);

		if (resulttype->tag != opi.tag_FUNCTIONTYPE) {
			codegen_error (cgen, "occampi_codegen_finstance(): type of FUNCTION not FUNCTIONTYPE!");
		} else {
			int resultbytes = 0;

			resulttype = tnode_nthsubof (resulttype, 0);

			if (!resulttype) {
				nresults = 0;
				codegen_warning (cgen, "occampi_codegen_finstance(): FUNCTION has no return types");
			} else if (parser_islistnode (resulttype)) {
				tnode_t **items = parser_getlistitems (resulttype, &nresults);
				int i;

				for (i=0, resultbytes=0; i<nresults; i++) {
					int thissize;

					thissize = tnode_bytesfor (items[i], cgen->target);
					if (thissize > 0) {
						if (thissize & (cgen->target->slotsize - 1)) {
							thissize = (thissize & ~(cgen->target->slotsize - 1)) + cgen->target->slotsize;
						}
						resultbytes += thissize;
					} else {
						codegen_warning (cgen, "occampi_codegen_finstance(): result %d has size %d\n", i, thissize);
					}
				}
			} else {
				int thissize;

				thissize = tnode_bytesfor (resulttype, cgen->target);
				if (thissize > 0) {
					if (thissize & (cgen->target->slotsize - 1)) {
						thissize = (thissize & ~(cgen->target->slotsize - 1)) + cgen->target->slotsize;
					}
					resultbytes += thissize;
				} else {
					codegen_warning (cgen, "occampi_codegen_finstance(): result has size %d\n", thissize);
				}
			}

			nresults = resultbytes / cgen->target->slotsize;
			codegen_callops (cgen, funcresults, nresults);
		}

		/*}}}*/
	} else if (namenode->tag == opi.tag_BUILTINFUNCTION) {
		/*{{{  finstance of a built-in FUNCTION*/
		builtinfunctionhook_t *bfh = (builtinfunctionhook_t *)tnode_nthhookof (namenode, 0);
		builtinfunction_t *builtin = bfh->biptr;

		if (builtin->codegen) {
			builtin->codegen (node, builtin, cgen);
		} else {
			codegen_error (cgen, "occampi_codegen_finstance(): don\'t know how to code for built-in FUNCTION [%s]", builtin->name);
			codegen_callops (cgen, comment, "BUILTINFUNCTION finstance of [%s]", builtin->name);
		}
		/*}}}*/
	} else {
		nocc_internal ("occampi_codegen_finstance(): don\'t know how to handle [%s]", namenode->tag->name);
	}
	return 0;
}
/*}}}*/
/*{{{  static int occampi_iscomplex_finstance (tnode_t *node, int deep)*/
/*
 *	returns non-zero if this function instance is complex
 */
static int occampi_iscomplex_finstance (tnode_t *node, int deep)
{
	return 1;		/* FIXME: for now */
}
/*}}}*/


/*{{{  static int occampi_prescope_funcdecl (tnode_t **node, prescope_t *ps)*/
/*
 *	called to prescope a long FUNCTION definition
 */
static int occampi_prescope_funcdecl (tnode_t **node, prescope_t *ps)
{
	occampi_prescope_t *ops = (occampi_prescope_t *)(ps->hook);
	char *rawname = (char *)tnode_nthhookof (tnode_nthsubof (*node, 0), 0);
	tnode_t **fparamsp = tnode_nthsubaddr (tnode_nthsubof (*node, 1), 1);		/* LHS is return-type */

	if (library_makepublic (node, rawname)) {
		return 1;		/* go round again */
	}
	ops->last_type = NULL;
	if (!*fparamsp) {
		/* no parameters, create empty list */
		*fparamsp = parser_newlistnode (NULL);
	} else if (*fparamsp && !parser_islistnode (*fparamsp)) {
		/* turn single parameter into a list-node */
		tnode_t *list = parser_newlistnode (NULL);

		parser_addtolist (list, *fparamsp);
		*fparamsp = list;
	}
	return 1;
}
/*}}}*/
/*{{{  static int occampi_scopein_funcdecl (tnode_t **node, scope_t *ss)*/
/*
 *	called to scope a FUNCTION definition
 */
static int occampi_scopein_funcdecl (tnode_t **node, scope_t *ss)
{
	tnode_t *name = tnode_nthsubof (*node, 0);
	tnode_t **paramsptr = tnode_nthsubaddr (tnode_nthsubof (*node, 1), 1);		/* LHS is return-type */
	tnode_t **bodyptr = tnode_nthsubaddr (*node, 2);
	void *nsmark;
	char *rawname;
	name_t *funcname;
	tnode_t *newname;

	nsmark = name_markscope ();

	/* walk parameters and body */
	tnode_modprepostwalktree (paramsptr, scope_modprewalktree, scope_modpostwalktree, (void *)ss);
	tnode_modprepostwalktree (bodyptr, scope_modprewalktree, scope_modpostwalktree, (void *)ss);

	name_markdescope (nsmark);

	/* declare and scope FUNCTION name, then check process in the scope of it */
	rawname = tnode_nthhookof (name, 0);
	funcname = name_addscopename (rawname, *node, tnode_nthsubof (*node, 1), NULL);
	newname = tnode_createfrom (opi.tag_NFUNCDEF, name, funcname);
	SetNameNode (funcname, newname);
	tnode_setnthsub (*node, 0, newname);

	/* free old name, check process */
	tnode_free (name);
	tnode_modprepostwalktree (tnode_nthsubaddr (*node, 3), scope_modprewalktree, scope_modpostwalktree, (void *)ss);
	ss->scoped++;

	return 0;		/* already walked child nodes */
}
/*}}}*/
/*{{{  static int occampi_scopeout_funcdecl (tnode_t **node, scope_t *ss)*/
/*
 *	called to scope a FUNCTION definition
 */
static int occampi_scopeout_funcdecl (tnode_t **node, scope_t *ss)
{
	tnode_t *name = tnode_nthsubof (*node, 0);
	name_t *sname;

	if (name->tag != opi.tag_NFUNCDEF) {
		scope_error (name, ss, "not NFUNCDEF!");
		return 0;
	}
	sname = tnode_nthnameof (name, 0);

	name_descopename (sname);

	return 1;
}
/*}}}*/
/*{{{  static tnode_t *occampi_gettype_funcdecl (tnode_t *node, tnode_t *defaulttype)*/
/*
 *	returns the type of a FUNCTION definition (= FUNCTIONTYPE(return-type, parameter-list))
 */
static tnode_t *occampi_gettype_funcdecl (tnode_t *node, tnode_t *defaulttype)
{
	return tnode_nthsubof (node, 1);
}
/*}}}*/
/*{{{  static int occampi_fetrans_funcdecl (tnode_t **node, fetrans_t *fe)*/
/*
 *	does front-end transforms on a FUNCTION definition
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_fetrans_funcdecl (tnode_t **node, fetrans_t *fe)
{
	chook_t *deschook = tnode_lookupchookbyname ("fetrans:descriptor");
	char *dstr = NULL;

	if (!deschook) {
		return 1;
	}
	langops_getdescriptor (*node, &dstr);
	if (dstr) {
		tnode_setchook (*node, deschook, (void *)dstr);
	}

	return 1;
}
/*}}}*/
/*{{{  static int occampi_betrans_funcdecl (tnode_t **node, betrans_t *be)*/
/*
 *	does back-end transforms on a FUNCTION definition
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_betrans_funcdecl (tnode_t **node, betrans_t *be)
{
	tnode_t *fnamenode = tnode_nthsubof (*node, 0);
	name_t *fname = tnode_nthnameof (fnamenode, 0);
	tnode_t *ftype, **rtypep, **ptypep;
	tnode_t **ritemsp;
	tnode_t *fbody = tnode_nthsubof (*node, 2);
	tnode_t **items, **ritems;
	tnode_t *myseqlist = NULL;
	int nitems, nritems, i;
	int resultno;

	ftype = NameTypeOf (fname);
	if (!ftype || (ftype->tag != opi.tag_FUNCTIONTYPE)) {
		tnode_error (*node, "type of function not FUNCTIONTYPE");
		return 0;
	}

	rtypep = tnode_nthsubaddr (ftype, 0);
	ptypep = tnode_nthsubaddr (ftype, 1);

	fbody = treeops_findprocess (fbody);
	if (!fbody || (fbody->tag != opi.tag_VALOF)) {
		tnode_error (*node, "function has non-VALOF body");
		return 0;
	}
	ritemsp = tnode_nthsubaddr (fbody, 0);

	/* if VALOF result is not a list-node, make it one */
	if (!parser_islistnode (*ritemsp)) {
		/* make singleton result a list */
		tnode_t *xitem = *ritemsp;

		*ritemsp = parser_newlistnode ((*node)->org_file);
		parser_addtolist (*ritemsp, xitem);
	}

	/* the following results get moved into parameters:
	 *	- anything bigger than an integer
	 *	- anything that is a complex type
	 *	- anything results beyond "maxfuncreturn" in target (setting this to zero forces all results to parameters..!)
	 */

	if (!parser_islistnode (*rtypep)) {
		/* make singleton result a list */
		tnode_t *xitem = *rtypep;

		*rtypep = parser_newlistnode ((*node)->org_file);
		parser_addtolist (*rtypep, xitem);
	}

	items = parser_getlistitems (*rtypep, &nitems);
	ritems = parser_getlistitems (*ritemsp, &nritems);

#if 0
fprintf (stderr, "occampi_betrans_funcdecl(): items =\n");
tnode_dumptree (*rtypep, 1, stderr);
fprintf (stderr, "occampi_betrans_funcdecl(): results =\n");
tnode_dumptree (*ritemsp, 1, stderr);
#endif
	if (nitems != nritems) {
		tnode_error (*node, "function-type has %d results, but VALOF gives %d", nitems, nritems);
		return 0;
	}

	for (i=0, resultno = 0; i<nitems; i++, resultno++) {
		int bytes = tnode_bytesfor (items[i], be->target);

		if ((bytes < 0) || (bytes > be->target->slotsize) || (i > be->target->maxfuncreturn) || (langops_iscomplex (items[i], 0))) {
			/* don't know, too big, or too many -- make a parameter */
			name_t *tmpname;
			tnode_t *namenode = NULL;
			tnode_t *fparam, *rexpr;
#if 0
fprintf (stderr, "occampi_betrans_funcdecl(): return bytes = %d, want to move out:\n", bytes);
tnode_dumptree (items[i], 1, stderr);
#endif

			tmpname = name_addtempname (NULL, items[i], opi.tag_NPARAM, &namenode);
			fparam = tnode_createfrom (opi.tag_FPARAM, *node, namenode, items[i]);
			SetNameDecl (tmpname, fparam);
			items[i] = fparam;
#if 0
fprintf (stderr, "occampi_betrans_funcdecl(): fudged it into a parameter:\n");
tnode_dumptree (items[i], 1, stderr);
#endif
			if (!myseqlist) {
				tnode_t *reallist = parser_newlistnode ((*node)->org_file);

				myseqlist = tnode_create (opi.tag_SEQ, (*node)->org_file, NULL, reallist);
				parser_addtolist (reallist, tnode_nthsubof (fbody, 1));
				tnode_setnthsub (fbody, 1, myseqlist);
				myseqlist = reallist;
			}
			/* remove corresponding thing from RESULTs of VALOF, make assignment */
			rexpr = parser_delfromlist (*ritemsp, i);
			parser_addtolist (myseqlist, tnode_create (opi.tag_ASSIGN, (*ritemsp)->org_file, namenode, rexpr, NameTypeOf (tmpname)));

			/* remove from results, add to parameters */
			fparam = parser_delfromlist (*rtypep, i);
			parser_addtolist (*ptypep, fparam);
			/* flag the parameter to indicate it got moved from a RESULT */
			betrans_tagnode (fparam, opi.tag_VALOF, resultno, be);

			nitems--, i--;			/* wind back */
			
		}
	}

	return 1;
}
/*}}}*/
/*{{{  static int occampi_precheck_funcdecl (tnode_t *node)*/
/*
 *	does pre-checking on FUNCTION declaration
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_precheck_funcdecl (tnode_t *node)
{
#if 0
fprintf (stderr, "occampi_precheck_funcdecl(): here!\n");
#endif
	precheck_subtree (tnode_nthsubof (node, 2));		/* precheck this body */
	precheck_subtree (tnode_nthsubof (node, 3));		/* precheck in-scope code */
#if 0
fprintf (stderr, "occampi_precheck_funcdecl(): returning 0\n");
#endif
	return 0;
}
/*}}}*/
/*{{{  static int occampi_usagecheck_funcdecl (tnode_t *node, uchk_state_t *ucstate)*/
/*
 *	does usage-checking on a FUNCTION declaration
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_usagecheck_funcdecl (tnode_t *node, uchk_state_t *ucstate)
{
	usagecheck_begin_branches (node, ucstate);
	usagecheck_newbranch (ucstate);
	usagecheck_subtree (tnode_nthsubof (node, 2), ucstate);		/* usage-check this body */
	usagecheck_subtree (tnode_nthsubof (node, 3), ucstate);		/* usage-check in-scope code */
	usagecheck_endbranch (ucstate);
	usagecheck_end_branches (node, ucstate);
	return 0;
}
/*}}}*/
/*{{{  static int occampi_namemap_funcdecl (tnode_t **node, map_t *map)*/
/*
 *	name-maps a FUNCTION definition
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_namemap_funcdecl (tnode_t **node, map_t *map)
{
	tnode_t *blk;
	/* tnode_t *saved_blk = map->thisblock;
	tnode_t **saved_params = map->thisprocparams; */
	tnode_t **paramsptr;
	tnode_t *tmpname;

	blk = map->target->newblock (tnode_nthsubof (*node, 2), map, tnode_nthsubof (*node, 1), map->lexlevel + 1);
	map_pushlexlevel (map, blk, tnode_nthsubaddr (tnode_nthsubof (*node, 1), 1));
	/* map->thisblock = blk;
	 * map->thisprocparams = tnode_nthsubaddr (tnode_nthsubof (*node, 1), 1);
	 * map->lexlevel++; */

	/* map formal params and body */
	paramsptr = map_thisprocparams_cll (map);
	map_submapnames (paramsptr, map);
	map_submapnames (tnode_nthsubaddr (blk, 0), map);		/* do this under the back-end block */

	map_poplexlevel (map);
	/* map->lexlevel--;
	 * map->thisblock = saved_blk;
	 * map->thisprocparams = saved_params; */

	/* insert the BLOCK node before the body of the process */
	tnode_setnthsub (*node, 2, blk);

	/* map scoped body */
	map_submapnames (tnode_nthsubaddr (*node, 3), map);

	/* add static-link, etc. if required and return-address */
	if (!parser_islistnode (*paramsptr)) {
		tnode_t *flist = parser_newlistnode (NULL);

		parser_addtolist (flist, *paramsptr);
		*paramsptr = flist;
	}

	tmpname = map->target->newname (tnode_create (opi.tag_HIDDENPARAM, NULL, tnode_create (opi.tag_RETURNADDRESS, NULL)), NULL, map,
			map->target->pointersize, 0, 0, 0, map->target->pointersize, 0);
	parser_addtolist_front (*paramsptr, tmpname);

	return 0;
}
/*}}}*/
/*{{{  static int occampi_codegen_funcdecl (tnode_t *node, codegen_t *cgen)*/
/*
 *	generates code for a FUNCTION definition
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_codegen_funcdecl (tnode_t *node, codegen_t *cgen)
{
	tnode_t *body = tnode_nthsubof (node, 2);
	tnode_t *name = tnode_nthsubof (node, 0);
	int ws_size, vs_size, ms_size;
	int ws_offset, adjust;
	name_t *pname;

	body = tnode_nthsubof (node, 2);
	cgen->target->be_getblocksize (body, &ws_size, &ws_offset, &vs_size, &ms_size, &adjust, NULL);


	pname = tnode_nthnameof (name, 0);
	codegen_callops (cgen, comment, "FUNCTION %s = %d,%d,%d,%d,%d", pname->me->name, ws_size, ws_offset, vs_size, ms_size, adjust);
	codegen_callops (cgen, setwssize, ws_size, adjust);
	codegen_callops (cgen, setvssize, vs_size);
	codegen_callops (cgen, setmssize, ms_size);
	codegen_callops (cgen, procnameentry, pname);

	/* adjust workspace and generate code for body */

	/* the body is a back-end block, which must be a VALOF/RESULT! -- that does the funcreturn, we just do the real return */

	codegen_subcodegen (body, cgen);

	/* return */
	codegen_callops (cgen, procreturn, adjust);

	/* generate code following declaration */
	codegen_subcodegen (tnode_nthsubof (node, 3), cgen);
#if 0
fprintf (stderr, "occampi_codegen_funcdecl!\n");
#endif
	return 0;
}
/*}}}*/
/*{{{  static int occampi_getdescriptor_funcdecl (tnode_t *node, char **str)*/
/*
 *	generates a descriptor line for a FUNCTION definition
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_getdescriptor_funcdecl (tnode_t *node, char **str)
{
	tnode_t *name = tnode_nthsubof (node, 0);
	char *realname;
	tnode_t *rtypes = tnode_nthsubof (tnode_nthsubof (node, 1), 0);
	tnode_t *params = tnode_nthsubof (tnode_nthsubof (node, 1), 1);

	if (*str) {
		/* shouldn't get this here, but.. */
		nocc_warning ("occampi_getdescriptor_funcdecl(): already had descriptor [%s]", *str);
		sfree (*str);
	}
	realname = NameNameOf (tnode_nthnameof (name, 0));
	*str = (char *)smalloc (10);

	strcpy (*str, "");

	if (parser_islistnode (rtypes)) {
		int nitems, i;
		tnode_t **items = parser_getlistitems (rtypes, &nitems);

		for (i=0; i<nitems; i++) {
			tnode_t *rtype = items[i];

			langops_getdescriptor (rtype, str);
			if (i < (nitems - 1)) {
				char *newstr = (char *)smalloc (strlen (*str) + 5);

				sprintf (newstr, "%s, ", *str);
				sfree (*str);
				*str = newstr;
			}
		}
	} else {
		langops_getdescriptor (rtypes, str);
	}

#if 0
fprintf (stderr, "occampi_getdescriptor_funcdecl(): return type descriptor is [%s]\n", *str);
#endif
	{
		char *newstr = (char *)smalloc (strlen (*str) + strlen (realname) + 15);

		sprintf (newstr, "%s FUNCTION %s (", *str, realname);
		sfree (*str);
		*str = newstr;
	}

	if (parser_islistnode (params)) {
		int nitems, i;
		tnode_t **items = parser_getlistitems (params, &nitems);

		for (i=0; i<nitems; i++) {
			tnode_t *param = items[i];

			langops_getdescriptor (param, str);
			if (i < (nitems - 1)) {
				char *newstr = (char *)smalloc (strlen (*str) + 5);

				sprintf (newstr, "%s, ", *str);
				sfree (*str);
				*str = newstr;
			}
		}
	} else {
		langops_getdescriptor (params, str);
	}

	{
		char *newstr = (char *)smalloc (strlen (*str) + 5);

		sprintf (newstr, "%s)", *str);
		sfree (*str);
		*str = newstr;
	}
	return 0;
}
/*}}}*/


/*{{{  static void occampi_reduce_builtinfunction (dfastate_t *dfast, parsepriv_t *pp, void *rarg)*/
/*
 *	reduces a built-in FUNCTION keyword token to a tree-node
 */
static void occampi_reduce_builtinfunction (dfastate_t *dfast, parsepriv_t *pp, void *rarg)
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

			biname = tnode_create (opi.tag_BUILTINFUNCTION, tok->origin, builtinfunctionhook_create (&(builtins[i])));
			dfa_pushnode (dfast, biname);

			lexer_freetoken (tok);
			return;
			/*}}}*/
		}
	}
	parser_pushtok (pp, tok);
	parser_error (tok->origin, "unknown built-in FUNCTION [%s]", lexer_stokenstr (tok));

	return;
}
/*}}}*/


/*{{{  static tnode_t *occampi_gettype_builtinfunction (tnode_t *node, tnode_t *defaulttype)*/
/*
 *	returns the type of a built-in FUNCTION
 */
static tnode_t *occampi_gettype_builtinfunction (tnode_t *node, tnode_t *defaulttype)
{
	builtinfunctionhook_t *bfh;
	builtinfunction_t *builtin;

	if (node->tag != opi.tag_BUILTINFUNCTION) {
		nocc_internal ("occampi_gettype_builtinfunction(): node not BUILTINFUNCTION");
		return NULL;
	}
	bfh = (builtinfunctionhook_t *)tnode_nthhookof (node, 0);
	builtin = bfh->biptr;

	if (!builtin) {
		nocc_internal ("occampi_gettype_builtinfunction(): builtin missing from hook");
		return NULL;
	}
#if 0
fprintf (stderr, "occampi_gettype_builtinfunction(): [%s] builtin->decltree =\n", builtin->name);
tnode_dumptree (builtin->decltree, 1, stderr);
#endif

	if (!builtin->decltree && builtin->descriptor) {
		/*{{{  parse descriptor and extract declaration-tree*/
		lexfile_t *lexbuf;
		tnode_t *decltree;

		lexbuf = lexer_openbuf (NULL, occampi_parser.langname, (char *)builtin->descriptor);
		if (!lexbuf) {
			nocc_error ("occampi_gettype_builtinfunction(): failed to open descriptor..");
			return NULL;
		}

		decltree = parser_descparse (lexbuf);
		lexer_close (lexbuf);

		if (!decltree) {
			nocc_error ("occampi_gettype_builtinfunction(): failed to parse descriptor..");
			return NULL;
		}

		/* prescope and scope the declaration tree -- to fixup parameters and type */
		if (prescope_tree (&decltree, &occampi_parser)) {
			nocc_error ("occampi_gettype_builtinfunction(): failed to prescope descriptor..");
			return NULL;
		}
		if (scope_tree (decltree, &occampi_parser)) {
			nocc_error ("occampi_gettype_builtinfunction(): failed to scope descriptor..");
			return NULL;
		}
		if (typecheck_tree (decltree, &occampi_parser)) {
			nocc_error ("occampi_gettype_builtinfunction(): failed to typecheck descriptor..");
			return NULL;
		}

		/* okay, attach declaration tree! */
		builtin->decltree = decltree;

#if 0
fprintf (stderr, "occampi_gettype_builtinfunction(): parsed descriptor and got type:\n");
tnode_dumptree (decltype, 1, stderr);
#endif
		/*}}}*/
	}

	/* if we have a declaration, use its type */
	return builtin->decltree ? typecheck_gettype (builtin->decltree, defaulttype) : defaulttype;
}
/*}}}*/


/*{{{  static int occampi_function_init_nodes (void)*/
/*
 *	sets up nodes for occam-pi functionators (monadic, dyadic)
 *	returns 0 on success, non-zero on error
 */
static int occampi_function_init_nodes (void)
{
	tndef_t *tnd;
	compops_t *cops;
	langops_t *lops;
	int i;

	/*{{{  occampi:finstancenode -- FINSTANCE*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:finstancenode", &i, 2, 0, 0, TNF_NONE);	/* subnodes: name; params */
	cops = tnode_newcompops ();
	cops->typecheck = occampi_typecheck_finstance;
	cops->gettype = occampi_gettype_finstance;
	cops->fetrans = occampi_fetrans_finstance;
	cops->betrans = occampi_betrans_finstance;
	cops->premap = occampi_premap_finstance;
	cops->namemap = occampi_namemap_finstance;
	cops->codegen = occampi_codegen_finstance;
	tnd->ops = cops;
	lops = tnode_newlangops ();
	lops->iscomplex = occampi_iscomplex_finstance;
	tnd->lops = lops;

	i = -1;
	opi.tag_FINSTANCE = tnode_newnodetag ("FINSTANCE", &i, tnd, NTF_NONE);
	/*}}}*/
	/*{{{  occampi:valofnode -- VALOF*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:valofnode", &i, 2, 0, 0, TNF_LONGPROC);	/* subnodes: result-exprs; body */
	cops = tnode_newcompops ();
	cops->codegen = occampi_codegen_valof;
	tnd->ops = cops;

	i = -1;
	opi.tag_VALOF = tnode_newnodetag ("VALOF", &i, tnd, NTF_NONE);
	/*}}}*/
	/*{{{  occampi:functiontype -- FUNCTIONTYPE*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:functiontype", &i, 2, 0, 0, TNF_NONE);	/* subnodes: return-type; fparams */
	cops = tnode_newcompops ();
	tnd->ops = cops;

	i = -1;
	opi.tag_FUNCTIONTYPE = tnode_newnodetag ("FUNCTIONTYPE", &i, tnd, NTF_NONE);
	/*}}}*/
	/*{{{  occampi:funcdecl -- FUNCDECL*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:funcdecl", &i, 4, 0, 0, TNF_LONGDECL);	/* subnodes: name; (return-type/fparams); body; in-scope-body */
	cops = tnode_newcompops ();
	cops->prescope = occampi_prescope_funcdecl;
	cops->scopein = occampi_scopein_funcdecl;
	cops->scopeout = occampi_scopeout_funcdecl;
	cops->namemap = occampi_namemap_funcdecl;
	cops->gettype = occampi_gettype_funcdecl;
	cops->precheck = occampi_precheck_funcdecl;
	cops->fetrans = occampi_fetrans_funcdecl;
	cops->betrans = occampi_betrans_funcdecl;
	cops->codegen = occampi_codegen_funcdecl;
	tnd->ops = cops;
	lops = tnode_newlangops ();
	lops->getdescriptor = occampi_getdescriptor_funcdecl;
	lops->do_usagecheck = occampi_usagecheck_funcdecl;
	tnd->lops = lops;

	i = -1;
	opi.tag_FUNCDECL = tnode_newnodetag ("FUNCDECL", &i, tnd, NTF_INDENTED_PROC);
	/*}}}*/
	/*{{{  occampi:shortfuncdecl -- SHORTFUNCDECL*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:shortfuncdecl", &i, 4, 0, 0, TNF_SHORTDECL);		/* subnodes: name; (return-type/fparams); in-scope-body; expr */
	cops = tnode_newcompops ();
#if 0
	cops->prescope = occampi_prescope_shortfuncdecl;
	cops->scopein = occampi_scopein_shortfuncdecl;
	cops->scopeout = occampi_scopeout_shortfuncdecl;
	cops->namemap = occampi_namemap_shortfuncdecl;
	cops->gettype = occampi_gettype_shortfuncdecl;
	cops->precheck = occampi_precheck_shortfuncdecl;
	cops->fetrans = occampi_fetrans_shortfuncdecl;
	cops->precode = occampi_precode_shortfuncdecl;
	cops->codegen = occampi_codegen_shortfuncdecl;
#endif
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnd->lops = lops;

	i = -1;
	opi.tag_SHORTFUNCDECL = tnode_newnodetag ("SHORTFUNCDECL", &i, tnd, NTF_NONE);
	/*}}}*/
	/*{{{  occampi:builtinfunction -- BUILTINFUNCTION*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:builtinfunction", &i, 0, 0, 1, TNF_NONE);		/* hook: builtinfunctionhook_t */
	cops = tnode_newcompops ();
	cops->gettype = occampi_gettype_builtinfunction;
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnd->lops = lops;
	tnd->hook_dumptree = builtinfunctionhook_dumphook;
	tnd->hook_free = builtinfunctionhook_free;

	i = -1;
	opi.tag_BUILTINFUNCTION = tnode_newnodetag ("BUILTINFUNCTION", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  setup builtins*/
	for (i=0; builtins[i].name; i++) {
		if (!builtins[i].tok) {
			if (builtins[i].keymatch) {
				builtins[i].tok = lexer_newtoken (KEYWORD, builtins[i].keymatch);
			} else if (builtins[i].symmatch) {
				builtins[i].tok = lexer_newtoken (SYMBOL, builtins[i].symmatch);
			} else {
				nocc_internal ("occampi_function_init_nodes(): built-in error, name = [%s]", builtins[i].name);
			}
		}
	}
	/*}}}*/

	return 0;
}
/*}}}*/
/*{{{  static int occampi_function_reg_reducers (void)*/
/*
 *	registers reductions for occam-pi functionators
 *	returns 0 on success, non-zero on error
 */
static int occampi_function_reg_reducers (void)
{
	parser_register_grule ("opi:funcdefreduce", parser_decode_grule ("SN1N+N+N+<C200C4R-", opi.tag_FUNCTIONTYPE, opi.tag_FUNCDECL));
	parser_register_grule ("opi:finstancereduce", parser_decode_grule ("SN1N+N+VC2R-", opi.tag_FINSTANCE));
	parser_register_grule ("opi:valofreduce", parser_decode_grule ("ST0T+@t00C2R-", opi.tag_VALOF));

	parser_register_reduce ("Roccampi:builtinfunction", occampi_reduce_builtinfunction, NULL);
	
	return 0;
}
/*}}}*/
/*{{{  static dfattbl_t **occampi_function_init_dfatrans (int *ntrans)*/
/*
 *	initialises and returns DFA transition tables for occam-pi functionators
 */
static dfattbl_t **occampi_function_init_dfatrans (int *ntrans)
{
	DYNARRAY (dfattbl_t *, transtbl);
	int i;
	char *tbuf;

	dynarray_init (transtbl);

	dynarray_add (transtbl, dfa_transtotbl ("occampi:fdeclstarttype ::= [ 0 @FUNCTION 1 ] [ 1 occampi:name 2 ] [ 2 @@( 3 ] [ 3 occampi:fparamlist 4 ] [ 4 @@) 5 ] [ 5 {<opi:funcdefreduce>} -* ]"));
	dynarray_add (transtbl, dfa_transtotbl ("occampi:funcdecl ::= [ 0 occampi:typecommalist <occampi:fdeclstarttype> ]"));
	dynarray_add (transtbl, dfa_transtotbl ("occampi:infinstance ::= [ 0 occampi:exprcommalist 2 ] [ 0 -@@) 1 ] [ 1 {<opi:nullpush>} -* 2 ] [ 2 @@) 3 ] [ 3 {<opi:finstancereduce>} -* ]"));
	dynarray_add (transtbl, dfa_transtotbl ("occampi:valofresult ::= [ 0 @RESULT 1 ] [ 1 occampi:expr 2 ] [ 2 {<opi:nullreduce>} -* ]"));
	dynarray_add (transtbl, dfa_transtotbl ("occampi:valof ::= [ 0 +@VALOF 1 ] [ 1 {<opi:valofreduce>} -* ]"));
	dynarray_add (transtbl, dfa_transtotbl ("occampi:builtinfinstancei ::= [ 0 @@( 1 ] [ 1 {Roccampi:builtinfunction} ] [ 1 occampi:exprcommalist 3 ] [ 1 @@) 2 ] [ 2 {<opi:nullpush>} ] " \
				"[ 2 -* 3 ] [ 3 {<opi:finstancereduce>} -* ]"));

	/* run-through built-in FUNCTIONs generating starting matches (in expressions) */
	tbuf = (char *)smalloc (256);
	for (i=0; builtins[i].name; i++) {
		if (builtins[i].keymatch) {
			sprintf (tbuf, "occampi:expr +:= [ 0 +@%s 1 ] [ 1 -@@( <occampi:builtinfinstancei> ]", builtins[i].keymatch);
		} else if (builtins[i].symmatch) {
			sprintf (tbuf, "occampi:expr +:= [ 0 +@@%s 1 ] [ 1 -@@( <occampi:builtinfinstancei> ]", builtins[i].symmatch);
		}
		dynarray_add (transtbl, dfa_transtotbl (tbuf));
	}
	sfree (tbuf);


	*ntrans = DA_CUR (transtbl);
	return DA_PTR (transtbl);
}
/*}}}*/
/*{{{  static int occampi_function_post_setup (void)*/
/*
 *	does post-setup for occam-pi functionator nodes
 *	returns 0 on success, non-zero on error
 */
static int occampi_function_post_setup (void)
{
	return 0;
}
/*}}}*/


/*{{{  occampi_function_feunit (feunit_t)*/
feunit_t occampi_function_feunit = {
	init_nodes: occampi_function_init_nodes,
	reg_reducers: occampi_function_reg_reducers,
	init_dfatrans: occampi_function_init_dfatrans,
	post_setup: occampi_function_post_setup
};
/*}}}*/


