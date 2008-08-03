/*
 *	occampi_mobiles.c -- occam-pi MOBILE data, channels and processes
 *	Copyright (C) 2005-2008 Fred Barnes <frmb@kent.ac.uk>
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
#include "origin.h"
#include "names.h"
#include "scope.h"
#include "prescope.h"
#include "constprop.h"
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

#include "mobile_types.h"


/*}}}*/


/*{{{  private types*/
typedef struct TAG_mobiletypehook {
	unsigned int val;	/* for mobile types that we can squeeze into a word */
	void *data;		/* data for non-trivial mobile types */
	int dlen;
	int label;		/* label for generated mobile type data */
} mobiletypehook_t;

/*}}}*/
/*{{{  private vars*/
static chook_t *chook_demobiletype = NULL;
static chook_t *chook_actionlhstype = NULL;
static chook_t *chook_mobiletypehook = NULL;
static chook_t *chook_dimtreehook = NULL;

/*}}}*/
/*{{{  static void occampi_condfreedynmobile (tnode_t *node, tnode_t *mtype, codegen_t *cgen, const int clear)*/
/*
 *	conditional-free dynamic mobile
 */
static void occampi_condfreedynmobile (tnode_t *node, tnode_t *mtype, codegen_t *cgen, const int clear)
{
	if (mtype->tag == opi.tag_DYNMOBARRAY) {
		/*{{{  conditional check-and-free for dynamic mobile array*/
		int skiplab;

		skiplab = codegen_new_label (cgen);
		codegen_callops (cgen, loadnthpointer, node, 2, 0);
		codegen_callops (cgen, loadnonlocal, cgen->target->pointersize);			/* load first dimension */
		// codegen_callops (cgen, loadpointer, node, cgen->target->pointersize);
		codegen_callops (cgen, branch, I_CJ, skiplab);
		codegen_callops (cgen, loadpointer, node, 0);						/* load pointer */
		codegen_callops (cgen, tsecondary, I_MTRELEASE);
		if (clear) {
			codegen_callops (cgen, loadconst, 0);
			codegen_callops (cgen, storeatpointer, node, cgen->target->pointersize);	/* zero first dimension */
		}
		codegen_callops (cgen, setlabel, skiplab);
		/*}}}*/
	} else {
		codegen_error (cgen, "occampi_condfreedynmobile(): don\'t know how to free [%s]", mtype->tag->name);
	}
	return;
}
/*}}}*/

/*{{{  static void *occampi_copy_demobilechook (void *chook)*/
/*
 *	copies a demobile-type compiler hook
 */
static void *occampi_copy_demobilechook (void *chook)
{
	tnode_t *type = (tnode_t *)chook;

	if (type) {
		type = tnode_copytree (type);
	}
	return (void *)type;
}
/*}}}*/
/*{{{  static void occampi_free_demobilechook (void *chook)*/
/*
 *	frees a demobile-type compiler hook
 */
static void occampi_free_demobilechook (void *chook)
{
	tnode_t *type = (tnode_t *)chook;

	if (type) {
		tnode_free (type);
	}
	return;
}
/*}}}*/
/*{{{  static void occampi_dumptree_demobilechook (tnode_t *t, void *chook, int indent, FILE *stream)*/
/*
 *	dumps a demobile-type compiler hook (debugging)
 */
static void occampi_dumptree_demobilechook (tnode_t *t, void *chook, int indent, FILE *stream)
{
	tnode_t *type = (tnode_t *)chook;

	if (type) {
		occampi_isetindent (stream, indent);
		fprintf (stream, "<chook:occampi:demobiletype>\n");
		tnode_dumptree (type, indent + 1, stream);
		occampi_isetindent (stream, indent);
		fprintf (stream, "</chook:occampi:demobiletype>\n");
	}
	return;
}
/*}}}*/

/*{{{  static void *occampi_copy_dimtreehook (void *chook)*/
/*
 *	copies a mobile dimension-tree compiler hook
 */
static void *occampi_copy_dimtreehook (void *chook)
{
	tnode_t *dtree = (tnode_t *)chook;

	if (dtree) {
		dtree = tnode_copytree (dtree);
	}
	return (void *)dtree;
}
/*}}}*/
/*{{{  static void occampi_free_dimtreehook (void *chook)*/
/*
 *	frees a mobile dimension-tree compiler hook
 */
static void occampi_free_dimtreehook (void *chook)
{
	tnode_t *dtree = (tnode_t *)chook;

	if (dtree) {
		tnode_free (dtree);
	}
	return;
}
/*}}}*/
/*{{{  static void occampi_dumptree_dimtreehook (tnode_t *t, void *chook, int indent, FILE *stream)*/
/*
 *	dumps a mobile dimension-tree compiler hook (debugging)
 */
static void occampi_dumptree_dimtreehook (tnode_t *t, void *chook, int indent, FILE *stream)
{
	tnode_t *dtree = (tnode_t *)chook;

	if (dtree) {
		occampi_isetindent (stream, indent);
		fprintf (stream, "<chook:occampi:dimtreehook>\n");
		tnode_dumptree (dtree, indent + 1, stream);
		occampi_isetindent (stream, indent);
		fprintf (stream, "</chook:occampi:dimtreehook>\n");
	}
	return;
}
/*}}}*/


/*{{{  static mobiletypehook_t *occampi_newmobiletypehook (void)*/
/*
 *	creates a new mobiletypehook_t structure
 */
static mobiletypehook_t *occampi_newmobiletypehook (void)
{
	mobiletypehook_t *mth = (mobiletypehook_t *)smalloc (sizeof (mobiletypehook_t));

	mth->val = 0;
	mth->data = NULL;
	mth->dlen = 0;
	mth->label = -1;

	return mth;
}
/*}}}*/
/*{{{  static void occampi_freemobiletypehook (mobiletypehook_t *mth)*/
/*
 *	frees a mobiletypehook_t structure
 */
static void occampi_freemobiletypehook (mobiletypehook_t *mth)
{
	if (!mth) {
		nocc_internal ("occampi_freemobiletypehook(): NULL pointer!");
		return;
	}

	if (mth->data) {
		sfree (mth->data);
		mth->data = NULL;
		mth->dlen = 0;
	}
	sfree (mth);
	return;
}
/*}}}*/
/*{{{  static void *occampi_copy_mobiletypehook (void *chook)*/
/*
 *	copies a mobiletypehook compiler hook
 */
static void *occampi_copy_mobiletypehook (void *chook)
{
	mobiletypehook_t *mth = (mobiletypehook_t *)chook;
	mobiletypehook_t *newmth = NULL;

	if (mth) {
		newmth = occampi_newmobiletypehook ();

		newmth->val = mth->val;
		if (mth->data && mth->dlen) {
			newmth->data = mem_ndup (mth->data, mth->dlen);
			newmth->dlen = mth->dlen;
		}
		newmth->label = mth->label;
	}
	
	return newmth;
}
/*}}}*/
/*{{{  static void occampi_free_mobiletypehook (void *chook)*/
/*
 *	frees a mobiletypehook compiler hook
 */
static void occampi_free_mobiletypehook (void *chook)
{
	mobiletypehook_t *mth = (mobiletypehook_t *)chook;

	if (mth) {
		occampi_freemobiletypehook (mth);
	}
	return;
}
/*}}}*/
/*{{{  static void occampi_dumptree_mobiletypehook (tnode_t *t, void *chook, int indent, FILE *stream)*/
/*
 *	dumps a mobile type hook compiler hook
 */
static void occampi_dumptree_mobiletypehook (tnode_t *t, void *chook, int indent, FILE *stream)
{
	mobiletypehook_t *mtd = (mobiletypehook_t *)chook;

	if (mtd) {
		occampi_isetindent (stream, indent);
		fprintf (stream, "<chook:occampi:mobiletypehook label=\"%d\" value=\"0x%8.8x\" dlen=\"%d\" data=\"",
				mtd->label, mtd->val, mtd->dlen);

		if (mtd->data && mtd->dlen) {
			char *str = mkhexbuf (mtd->data, mtd->dlen);

			fprintf (stream, "%s", str);
			sfree (str);
		}
		fprintf (stream, "\" />\n");
	} else {
		occampi_isetindent (stream, indent);
		fprintf (stream, "<chook:occampi:mobiletypehook value=\"null\" />\n");
	}
	return;
}
/*}}}*/


/*{{{  static int occampi_prescope_mobiletypenode (compops_t *cops, tnode_t **node, prescope_t *ps)*/
/*
 *	called to pre-scope a MOBILE type node -- used to clean up partial lists
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_prescope_mobiletypenode (compops_t *cops, tnode_t **node, prescope_t *ps)
{
	tnode_t *type = tnode_nthsubof (*node, 0);

#if 0
fprintf (stderr, "occampi_prescope_mobiletypenode(): here!, type is:\n");
tnode_dumptree (type, 1, stderr);
#endif

	if (parser_islistnode (type)) {
		/* remove any NULL items from the list */
		parser_cleanuplist (type);
	}

	return 1;
}
/*}}}*/
/*{{{  static int occampi_premap_mobiletypenode (compops_t *cops, tnode_t **nodep, map_t *map)*/
/*
 *	called to do back-end mapping on a MOBILE type node -- used to figure out
 *	mobile type-descriptors for run-time allocation
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_premap_mobiletypenode (compops_t *cops, tnode_t **nodep, map_t *map)
{
	mobiletypehook_t *mth = NULL;
	tnode_t *t = *nodep;
	
	if (t->tag->ndef->lops && tnode_haslangop (t->tag->ndef->lops, "mobiletypedescof")) {
		mth = (mobiletypehook_t *)tnode_calllangop (t->tag->ndef->lops, "mobiletypedescof", 1, t);
	}

#if 0
fprintf (stderr, "occampi_premap_mobiletypenode(): here, tag = [%s].  mth = 0x%8.8x\n", (*nodep)->tag->name, (unsigned int)mth);
tnode_dumptree (*nodep, 1, stderr);
#endif

	return 0;
}
/*}}}*/


/*{{{  static mobiletypehook_t *occampi_leaftype_mobiletypedescof (langops_t *lops, tnode_t *t)*/
/*
 *	returns the mobile type descriptor for a leaf-type node -- returns a structure allocated in the node
 */
static mobiletypehook_t *occampi_leaftype_mobiletypedescof (langops_t *lops, tnode_t *t)
{
	mobiletypehook_t *mtd = (mobiletypehook_t *)tnode_getchook (t, chook_mobiletypehook);

	if (!mtd) {
		mtd = occampi_newmobiletypehook ();

		tnode_setchook (t, chook_mobiletypehook, mtd);

		if (t->tag == opi.tag_INT) {
			mtd->val = MT_MAKE_NUM (MT_NUM_INT32);		/* FIXME: default to INT32 for now */
		} else if (t->tag == opi.tag_BYTE) {
			mtd->val = MT_MAKE_NUM (MT_NUM_BYTE);
		} else if (t->tag == opi.tag_CHAR) {
			mtd->val = MT_MAKE_NUM (MT_NUM_INT16);		/* FIXME: default to INT16 for now */
		} else if (t->tag == opi.tag_BOOL) {
			mtd->val = MT_MAKE_NUM (MT_NUM_INT32);
		} else if (t->tag == opi.tag_INT16) {
			mtd->val = MT_MAKE_NUM (MT_NUM_INT16);
		} else if (t->tag == opi.tag_INT32) {
			mtd->val = MT_MAKE_NUM (MT_NUM_INT32);
		} else if (t->tag == opi.tag_INT64) {
			mtd->val = MT_MAKE_NUM (MT_NUM_INT64);
		} else if (t->tag == opi.tag_REAL32) {
			mtd->val = MT_MAKE_NUM (MT_NUM_REAL32);
		} else if (t->tag == opi.tag_REAL64) {
			mtd->val = MT_MAKE_NUM (MT_NUM_REAL64);
		} else {
			nocc_serious ("occampi_leaftype_mobiletypedescof(): don't know how to get mobile type for [%s]", t->tag->name);
		}
	}

	return mtd;
}
/*}}}*/


/*{{{  static void occampi_mobiletypenode_initmobile (tnode_t *node, codegen_t *cgen, void *arg)*/
/*
 *	generates code to initialise a mobile
 */
static void occampi_mobiletypenode_initmobile (tnode_t *node, codegen_t *cgen, void *arg)
{
	tnode_t *mtype = (tnode_t *)arg;
	int ws_off, vs_off, ms_off, ms_shdw;

	cgen->target->be_getoffsets (node, &ws_off, &vs_off, &ms_off, &ms_shdw);

	codegen_callops (cgen, debugline, mtype);
	codegen_callops (cgen, loadmsp, 0);
	codegen_callops (cgen, loadnonlocal, ms_shdw);
	codegen_callops (cgen, storelocal, ws_off);
	codegen_callops (cgen, comment, "initmobile");

	return;
}
/*}}}*/
/*{{{  static void occampi_mobiletypenode_finalmobile (tnode_t *node, codegen_t *cgen, void *arg)*/
/*
 *	generates code to descope a mobile
 */
static void occampi_mobiletypenode_finalmobile (tnode_t *node, codegen_t *cgen, void *arg)
{
	tnode_t *mtype = (tnode_t *)arg;
	int ws_off, vs_off, ms_off, ms_shdw;

	cgen->target->be_getoffsets (node, &ws_off, &vs_off, &ms_off, &ms_shdw);

	codegen_callops (cgen, debugline, mtype);
	codegen_callops (cgen, loadlocal, ws_off);
	codegen_callops (cgen, loadmsp, 0);
	codegen_callops (cgen, storenonlocal, ms_shdw);
	codegen_callops (cgen, comment, "finalmobile");

	return;
}
/*}}}*/
/*{{{  static void occampi_mobiletypenode_initdynmobarray (tnode_t *node, codegen_t *cgen, void *arg)*/
/*
 *	generates code to initialise a dynamic mobile array
 */
static void occampi_mobiletypenode_initdynmobarray (tnode_t *node, codegen_t *cgen, void *arg)
{
	tnode_t *mtype = (tnode_t *)arg;
	int ws_off;

	cgen->target->be_getoffsets (node, &ws_off, NULL, NULL, NULL);

#if 0
fprintf (stderr, "occampi_mobiletypenode_initdynmobarray(): here!, node =\n");
tnode_dumptree (node, 1, stderr);
#endif
	codegen_callops (cgen, debugline, mtype);
	codegen_callops (cgen, tsecondary, I_NULL);
	// codegen_callops (cgen, storepointer, node, 0);

	codegen_callops (cgen, storelocal, ws_off);
#if 0
fprintf (stderr, "occampi_mobiletypenode_initdynmobarray(): mtype =\n");
tnode_dumptree (mtype, 1, stderr);
#endif
	/* FIXME: number of dimensions */
	codegen_callops (cgen, loadconst, 0);
	codegen_callops (cgen, storelocal, ws_off + cgen->target->pointersize);
	codegen_callops (cgen, comment, "initdynmobarray");

	return;
}
/*}}}*/
/*{{{  static void occampi_mobiletypenode_finaldynmobarray (tnode_t *node, codegen_t *cgen, void *arg)*/
/*
 *	generates code to release a dynamic mobile array
 */
static void occampi_mobiletypenode_finaldynmobarray (tnode_t *node, codegen_t *cgen, void *arg)
{
	tnode_t *mtype = (tnode_t *)arg;
	int ws_off, skiplab;

	cgen->target->be_getoffsets (node, &ws_off, NULL, NULL, NULL);

	skiplab = codegen_new_label (cgen);

#if 0
fprintf (stderr, "occampi_mobiletypenode_finaldynmobilearrray(): mtype =\n");
tnode_dumptree (mtype, 1, stderr);
#endif
	codegen_callops (cgen, debugline, mtype);
	codegen_callops (cgen, loadlocal, ws_off + cgen->target->pointersize);			/* load first dimension */
	codegen_callops (cgen, branch, I_CJ, skiplab);
	codegen_callops (cgen, loadlocal, ws_off);						/* load pointer */
	codegen_callops (cgen, tsecondary, I_MTRELEASE);
	codegen_callops (cgen, tsecondary, I_NULL);
	codegen_callops (cgen, storelocal, ws_off);					/* zero pointer */
	codegen_callops (cgen, setlabel, skiplab);
	codegen_callops (cgen, comment, "finaldynmobarray");

	return;
}
/*}}}*/


/*{{{  static int occampi_mobiletypenode_bytesfor (langops_t *lops, tnode_t *t, target_t *target)*/
/*
 *	determines the number of bytes needed for a MOBILE
 */
static int occampi_mobiletypenode_bytesfor (langops_t *lops, tnode_t *t, target_t *target)
{
	if (t->tag == opi.tag_MOBILE) {
		/* static mobile of some variety */
		return tnode_bytesfor (tnode_nthsubof (t, 0), target);
	} else if (t->tag == opi.tag_DYNMOBARRAY) {
		/* don't know */
		return -1;
	}
	return -1;
}
/*}}}*/
/*{{{  static int occampi_mobiletypenode_initsizes (langops_t *lops, tnode_t *t, tnode_t *declnode, int *wssize, int *vssize, int *mssize, int *indir, map_t *mdata)*/
/*
 *	returns special allocation requirements for MOBILEs
 *	return value is non-zero if settings were made
 */
static int occampi_mobiletypenode_initsizes (langops_t *lops, tnode_t *t, tnode_t *declnode, int *wssize, int *vssize, int *mssize, int *indir, map_t *mdata)
{
	if (t->tag == opi.tag_MOBILE) {
		/* static mobile, single pointer in workspace, real sized allocation in mobilespace */
		*wssize = mdata->target->pointersize;
		*vssize = 0;
		*mssize = tnode_bytesfor (tnode_nthsubof (t, 0), mdata->target);
		*indir = 1;		/* pointer left in the workspace */
		return 1;
	} else if (t->tag == opi.tag_DYNMOBARRAY) {
		/* dynamic mobile array, 1 + <ndim> words in workspace, no allocation elsewhere */
		*wssize = mdata->target->pointersize + (1 * mdata->target->slotsize);		/* FIXME: ndim */
		*vssize = 0;
		*mssize = 0;
		*indir = 1;
		return 1;
	}
	return 0;
}
/*}}}*/
/*{{{  static tnode_t *occampi_mobiletypenode_typereduce (langops_t *lops, tnode_t *type)*/
/*
 *	de-mobilises a type and return it (used in type-checking)
 */
static tnode_t *occampi_mobiletypenode_typereduce (langops_t *lops, tnode_t *type)
{
	if (type->tag == opi.tag_MOBILE) {
		return tnode_nthsubof (type, 0);
	} else if (type->tag == opi.tag_DYNMOBARRAY) {
		tnode_t *rtype = (tnode_t *)tnode_getchook (type, chook_demobiletype);

		if (!rtype) {
			/* reducing into an array-type */
			rtype = tnode_createfrom (opi.tag_ARRAY, type, NULL, tnode_nthsubof (type, 0));
			tnode_setchook (type, chook_demobiletype, (void *)rtype);
		}

		return rtype;
	}
	return NULL;
}
/*}}}*/
/*{{{  static tnode_t *occampi_mobiletypenode_typeactual (langops_t *lops, tnode_t *formaltype, tnode_t *actualtype, tnode_t *node, typecheck_t *tc)*/
/*
 *	used to test type compatibility for an operation on a mobile type
 *	returns the actual type used for the operation
 */
static tnode_t *occampi_mobiletypenode_typeactual (langops_t *lops, tnode_t *formaltype, tnode_t *actualtype, tnode_t *node, typecheck_t *tc)
{
	tnode_t *atype = NULL;

	if ((formaltype->tag == opi.tag_DYNMOBARRAY) || (formaltype->tag == opi.tag_MOBILE)) {
		if (actualtype->tag == formaltype->tag) {
			/* two dynamic mobile arrays, or general mobiles, sub-type */
			atype = typecheck_fixedtypeactual (tnode_nthsubof (formaltype, 0), tnode_nthsubof (actualtype, 0), node, tc, 1);

			if (formaltype->tag == opi.tag_DYNMOBARRAY) {
				/* dimension counts must match too */
				/* FIXME! */
			}
		} else {
			/* operation dictates whether this can drop to the non-mobile type */
			if (tnode_ntflagsof (node) & NTF_ACTION_DEMOBILISE) {
				tnode_t *demob = typecheck_typereduce (formaltype);

				if (!demob) {
					typecheck_error (node, tc, "cannot apply [%s] to mobile type [%s] in [%s]",
							actualtype->tag->name, formaltype->tag->name, node->tag->name);
				} else {
					atype = typecheck_typeactual (demob, actualtype, node, tc);
					if (!atype) {
						typecheck_error (node, tc, "incompatible types in [%s]", node->tag->name);
					} else {
						/* yes, can do this operation on the non-mobile type, but real type is still the mobile one */
						atype = formaltype;
					}
				}
			} else {
				typecheck_error (node, tc, "cannot apply [%s] to dynamic mobile array in [%s]",
						actualtype->tag->name, node->tag->name);
			}
		}
	} else {
		typecheck_error (node, tc, "occampi_mobiletypenode_typeactual(): unhandled type [%s]", formaltype->tag->name);
	}

	return atype;
}
/*}}}*/
/*{{{  static int occampi_mobiletypenode_initialising_decl (langops_t *lops, tnode_t *t, tnode_t *benode, map_t *mdata)*/
/*
 *	initialises a mobile declaration node of some form
 */
static int occampi_mobiletypenode_initialising_decl (langops_t *lops, tnode_t *t, tnode_t *benode, map_t *mdata)
{
	if (t->tag == opi.tag_MOBILE) {
		/* static mobile */
		codegen_setinithook (benode, occampi_mobiletypenode_initmobile, (void *)t);
		codegen_setfinalhook (benode, occampi_mobiletypenode_finalmobile, (void *)t);
		return 1;
	} else if (t->tag == opi.tag_DYNMOBARRAY) {
		/* dynamic mobile array */
		codegen_setinithook (benode, occampi_mobiletypenode_initdynmobarray, (void *)t);
		codegen_setfinalhook (benode, occampi_mobiletypenode_finaldynmobarray, (void *)t);
	}
	return 0;
}
/*}}}*/
/*{{{  static int occampi_mobiletypenode_iscomplex (langops_t *lops, tnode_t *t, int deep)*/
/*
 *	returns non-zero if this type is "complex" (e.g. must be moved into FUNCTION params)
 */
static int occampi_mobiletypenode_iscomplex (langops_t *lops, tnode_t *t, int deep)
{
	/* we'll assume they're all complex for now.. */
	return 1;
}
/*}}}*/
/*{{{  static int occampi_mobiletypenode_typeaction (langops_t *lops, tnode_t *type, tnode_t *anode, codegen_t *cgen)*/
/*
 *	this handles code-generation for actions involving mobile types
 *	returns 0 to stop the code-gen walk, 1 to continue, -1 to resort to normal action handling
 */
static int occampi_mobiletypenode_typeaction (langops_t *lops, tnode_t *type, tnode_t *anode, codegen_t *cgen)
{
	if (anode->tag == opi.tag_ASSIGN) {
		if (type->tag == opi.tag_MOBILE) {
			/*{{{  MOBILE assignment -- pointer-swap*/
			codegen_callops (cgen, comment, "FIXME! (mobile assign)");
			/*}}}*/
		} else if (type->tag == opi.tag_DYNMOBARRAY) {
			/*{{{  dynamic mobile array assignment*/
			tnode_t *lhs = tnode_nthsubof (anode, 0);
			tnode_t *rhs = tnode_nthsubof (anode, 1);
			tnode_t *dimlist = NULL;

			tnode_t *lhstype = (tnode_t *)tnode_getchook (anode, chook_actionlhstype);

#if 1
fprintf (stderr, "occampi_mobiletypenode_action(): ASSIGN, lhstype (from hook %p) =\n", chook_actionlhstype);
tnode_dumptree (lhstype, 1, stderr);
fprintf (stderr, "occampi_mobiletypenode_action(): ASSIGN, lhs =\n");
tnode_dumptree (lhs, 1, stderr);
fprintf (stderr, "occampi_mobiletypenode_action(): ASSIGN, rhs =\n");
tnode_dumptree (rhs, 1, stderr);
#endif
			occampi_condfreedynmobile (lhs, type, cgen, 0);

			codegen_subcodegen (rhs, cgen);
			codegen_callops (cgen, storepointer, lhs, 0);

			/* get dimension list from RHS */
			dimlist = langops_dimtreeof (rhs);

#if 1
fprintf (stderr, "occampi_mobiletypenode_action(): ASSIGN, dimlist =\n");
tnode_dumptree (dimlist, 1, stderr);
#endif
			if (!dimlist) {
				nocc_internal ("occampi_mobiletypenode_typeaction(): ASSIGN/DYNMOBARRAY: no dimension(s)!");
			} else if (!parser_islistnode (dimlist)) {
				nocc_internal ("occampi_mobiletypenode_typeaction(): dimension list is not list! [%s]", dimlist->tag->name);
			} else {
				tnode_t **dimitems;
				int ndimitems, i;

				dimitems = parser_getlistitems (dimlist, &ndimitems);
				for (i=0; i<ndimitems; i++) {
					codegen_callops (cgen, loadname, dimitems[i], 0);
					// codegen_subcodegen (dimitems[i], cgen);
					codegen_callops (cgen, storepointer, lhs, (i + 1) * cgen->target->pointersize);
				}
			}

			// codegen_callops (cgen, comment, "FIXME! (dynmobarray assign)");
			/*}}}*/
		} else {
			codegen_warning (cgen, "occampi_mobiletypenode_typeaction(): don\'t know how to assign [%s]", type->tag->name);
			return -1;
		}
	} else if (anode->tag == opi.tag_OUTPUT) {
		if (type->tag == opi.tag_MOBILE) {
			/*{{{  MOBILE output -- pointer-swapping*/
			codegen_callops (cgen, comment, "FIXME! (mobile output)");
			/*}}}*/
		} else if (type->tag == opi.tag_DYNMOBARRAY) {
			/*{{{  dynamic mobile array output*/
			codegen_callops (cgen, comment, "FIXME! (dynmobarray output)");
			/*}}}*/
		} else {
			codegen_warning (cgen, "occampi_mobiletypenode_typeaction(): don\'t know how to output [%s]", type->tag->name);
			return -1;
		}
	} else if (anode->tag == opi.tag_INPUT) {
		if (type->tag == opi.tag_MOBILE) {
			/*{{{  MOBILE input -- pointer-swapping*/
			codegen_callops (cgen, comment, "FIXME! (mobile input)");
			/*}}}*/
		} else if (type->tag == opi.tag_DYNMOBARRAY) {
			/*{{{  dynamic mobile array input*/
			codegen_callops (cgen, comment, "FIXME! (dynmobarray input)");
			/*}}}*/
		} else {
			codegen_warning (cgen, "occampi_mobiletypenode_typeaction(): don\'t know how to input [%s]", type->tag->name);
			return -1;
		}
	}
	return 0;
}
/*}}}*/
/*{{{  static mobiletypehook_t *occampi_mobiletypenode_mobiletypedescof (langops_t *lops, tnode_t *t)*/
/*
 *	gets the mobile type descriptor for a mobile type node, goes down recursively
 *	returns type description on success, NULL on failure
 */
static mobiletypehook_t *occampi_mobiletypenode_mobiletypedescof (langops_t *lops, tnode_t *t)
{
	mobiletypehook_t *mth = (mobiletypehook_t *)tnode_getchook (t, chook_mobiletypehook);

	if (!mth) {
		if (t->tag == opi.tag_DYNMOBARRAY) {
			tnode_t *itype = tnode_nthsubof (t, 0);
			mobiletypehook_t *imth = NULL;

			if (itype && itype->tag->ndef->lops && tnode_haslangop (itype->tag->ndef->lops, "mobiletypedescof")) {
				imth = (mobiletypehook_t *)tnode_calllangop (itype->tag->ndef->lops, "mobiletypedescof", 1, itype);
			}
			if (imth) {
				if (imth->data || imth->dlen) {
					nocc_serious ("occampi_mobiletypenode_mobiletypedescof(): inner type of DYNMOBARRAY gave complex mobile type");
				} else {
					mth = occampi_newmobiletypehook ();
					mth->val = MT_MAKE_ARRAY_TYPE (1, imth->val);
					tnode_setchook (t, chook_mobiletypehook, mth);
				}
			} else {
				nocc_serious ("occampi_mobiletypenode_mobiletypedescof(): inner type of DYNMOBARRAY has no mobile type");
			}
		} else {
			nocc_internal ("occampi_mobiletypenode_mobiletypedescof(): unhandled mobile type [%s]", t->tag->name);
		}
	}

	return mth;
}
/*}}}*/
/*{{{  static tnode_t *occampi_mobiletypenode_dimtreeof_node (langops_t *lops, tnode_t *t, tnode_t *varnode)*/
/*
 *	returns the dimension-tree associated with a mobile, or NULL if none
 */
static tnode_t *occampi_mobiletypenode_dimtreeof_node (langops_t *lops, tnode_t *t, tnode_t *varnode)
{
	if (t->tag == opi.tag_DYNMOBARRAY) {
		/*{{{  return/create dimension tree fo dynamic mobile array type*/
		tnode_t *dimlist = (tnode_t *)tnode_getchook (t, opi.chook_arraydiminfo);

#if 1
fprintf (stderr, "occampi_mobiletypenode_dimtreeof_node(): here!\n");
#endif
		if (!dimlist) {
			/* FIXME: create dimension list for dynamic mobile array type */
			tnode_t *ditem;

			dimlist = parser_newlistnode (NULL);
			ditem = tnode_create (opi.tag_DIMSIZE, NULL, varnode, constprop_newconst (CONST_INT, NULL,
					tnode_create (opi.tag_INT, NULL), 0), tnode_create (opi.tag_INT, NULL));

			parser_addtolist (dimlist, ditem);

			tnode_setchook (t, opi.chook_arraydiminfo, dimlist);
		}

		return dimlist;
		/*}}}*/
	}
	return NULL;
}
/*}}}*/


/*{{{  static int occampi_mobilealloc_typecheck (compops_t *cops, tnode_t *node, typecheck_t *tc)*/
/*
 *	does type-checking for a mobile allocation node, just
 *	figures out what type we're creating and stores it in the subnode
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_mobilealloc_typecheck (compops_t *cops, tnode_t *node, typecheck_t *tc)
{
	tnode_t *subtype = tnode_nthsubof (node, 0);
	tnode_t *rtype = tnode_nthsubof (node, 2);

	if (rtype) {
		/* already got this */
		return 1;
	}

	if (node->tag == opi.tag_NEWDYNMOBARRAY) {
		tnode_t **dimtreep;
		tnode_t **dimitems = NULL;
		int ndimitems, i;
		tnode_t *inttype = tnode_create (opi.tag_INT, NULL);
		tnode_t *sizetree = NULL;

		/* dimension tree should be a list of integer types */
		dimtreep = tnode_nthsubaddr (node, 1);
		if (!parser_islistnode (*dimtreep)) {
			*dimtreep = parser_buildlistnode (NULL, *dimtreep, NULL);
		}

		typecheck_subtree (*dimtreep, tc);
#if 0
fprintf (stderr, "occampi_mobilealloc_typecheck(): here!  typechecked dimension, node now:\n");
tnode_dumptree (node, 1, stderr);
#endif

		rtype = tnode_createfrom (opi.tag_DYNMOBARRAY, node, subtype);
		tnode_setnthsub (node, 2, rtype);

		dimitems = parser_getlistitems (*dimtreep, &ndimitems);
		for (i=0; i<ndimitems; i++) {
			tnode_t *dimtype = typecheck_gettype (dimitems[i], inttype);

			if (!typecheck_fixedtypeactual (inttype, dimtype, node, tc, 0)) {
				typecheck_error (node, tc, "mobile dimension type not integer");
			}

			/* add to complete dimension size (multiplication of dimensions) */
			if (!sizetree) {
				sizetree = tnode_copytree (dimitems[i]);
			} else {
				sizetree = tnode_createfrom (opi.tag_TIMES, node, sizetree, dimitems[i], inttype);
			}
		}

		tnode_setnthsub (node, 3, sizetree);

		return 0;
	}

	return 1;
}
/*}}}*/
/*{{{  static int occampi_mobilealloc_constprop (compops_t *cops, tnode_t **nodep)*/
/*
 *	does constant propagatin for mobile allocation node, figures
 *	out the complete byte size required by certain allocators.  Called in post-order.
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_mobilealloc_constprop (compops_t *cops, tnode_t **nodep)
{
	tnode_t *t = *nodep;

	if (t->tag == opi.tag_NEWDYNMOBARRAY) {
		tnode_t **dimitems;
		int ndimitems, i;
		tnode_t *sizeexpr = NULL;
		
		dimitems = parser_getlistitems (tnode_nthsubof (t, 1), &ndimitems);

#if 0
fprintf (stderr, "occampi_mobilealloc_constprop(): NEWDYNMOBILEARRAY: %d dimension tree:\n", ndimitems);
for (i=0; i<ndimitems; i++) {
tnode_dumptree (dimitems[i], 1, stderr);
}
#endif
	}

	return 1;
}
/*}}}*/
/*{{{  static int occampi_mobilealloc_betrans (compops_t *cops, tnode_t **nodep, betrans_t *be)*/
/*
 *	does back-end transformations for a mobile allocation node
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_mobilealloc_betrans (compops_t *cops, tnode_t **nodep, betrans_t *be)
{
	tnode_t *t = *nodep;

	if (t->tag == opi.tag_NEWDYNMOBARRAY) {
		tnode_t **asizep = tnode_nthsubaddr (t, 3);
		tnode_t *subtype = tnode_nthsubof (t, 0);
		int abytes = tnode_bytesfor (subtype, be->target);

#if 0
fprintf (stderr, "occampi_mobilealloc_betrans(): allocation size expression, abytes = %d:\n", abytes);
tnode_dumptree (*asizep, 1, stderr);
#endif
		if (abytes == 0) {
			nocc_internal ("occampi_mobilealloc_betrans(): got 0 bytes in allocation-size for DYNMOBILEARRAY of [%s]\n", subtype->tag->name);
		} else if (abytes == 1) {
			/* easy! */
		} else {
			*asizep = tnode_createfrom (opi.tag_TIMES, t,
					constprop_newconst (CONST_INT, NULL, tnode_create (opi.tag_INT, NULL), abytes),
					*asizep, tnode_create (opi.tag_INT, NULL));
			/* run const-prop over the result to collapse */
			constprop_tree (asizep);
		}
	}

	return 1;
}
/*}}}*/
/*{{{  static tnode_t *occampi_mobilealloc_gettype (langops_t *lops, tnode_t *node, tnode_t *default_type)*/
/*
 *	gets the type of a mobile allocation node
 */
static tnode_t *occampi_mobilealloc_gettype (langops_t *lops, tnode_t *node, tnode_t *default_type)
{
	tnode_t *rtype = tnode_nthsubof (node, 2);

	/* typecheck should have left us a type in subnode-2 */
	if (!rtype) {
		nocc_internal ("occampi_mobilealloc_gettype(): missing type!");
	}

	return rtype;
}
/*}}}*/
/*{{{  static tnode_t *occampi_mobilealloc_dimtreeof (langops_t *lops, tnode_t *node)*/
/*
 *	gets the dimension tree of a mobile allocation node
 *	returns list on success, NULL on failure
 */
static tnode_t *occampi_mobilealloc_dimtreeof (langops_t *lops, tnode_t *node)
{
	if (node->tag == opi.tag_NEWDYNMOBARRAY) {
		return tnode_nthsubof (node, 1);
	}

	return NULL;
}
/*}}}*/
/*{{{  static int occampi_mobilealloc_premap (compops_t *cops, tnode_t **nodep, map_t *map)*/
/*
 *	does pre-mapping for a mobile allocation node -- inserts back-end result
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_mobilealloc_premap (compops_t *cops, tnode_t **nodep, map_t *map)
{
	mobiletypehook_t *mth = NULL;
	tnode_t *t = *nodep;
	tnode_t *type = tnode_nthsubof (t, 2);

	/* do mobile-typing on type of this node -- descriptor we're going to allocate */
	if (type->tag->ndef->lops && tnode_haslangop (type->tag->ndef->lops, "mobiletypedescof")) {
		mth = (mobiletypehook_t *)tnode_calllangop (type->tag->ndef->lops, "mobiletypedescof", 1, type);
	}

	if (t->tag == opi.tag_NEWDYNMOBARRAY) {
		/* pre-map dimensions and size expressions */
		tnode_t **dimaddr = tnode_nthsubaddr (t, 1);
		tnode_t **sizeaddr = tnode_nthsubaddr (t, 3);

		map_subpremap (dimaddr, map);
		map_subpremap (sizeaddr, map);

		*nodep = map->target->newresult (t, map);
#if 0
fprintf (stderr, "occampi_mobilealloc_premap(): created new result node:\n");
tnode_dumptree (*nodep, 1, stderr);
#endif
	}

	return 0;
}
/*}}}*/
/*{{{  static int occampi_mobilealloc_namemap (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	does name-mapping for a mobile allocation node
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_mobilealloc_namemap (compops_t *cops, tnode_t **node, map_t *map)
{
	if ((*node)->tag == opi.tag_NEWDYNMOBARRAY) {
		tnode_t **sizeaddr = tnode_nthsubaddr (*node, 3);
		tnode_t **dimaddr = tnode_nthsubaddr (*node, 1);

#if 0
fprintf (stderr, "occampi_mobilealloc_namemap(): name-map dynamic mobile array creation:\n");
tnode_dumptree (*node, 1, stderr);
#endif
		/* name-map dimension and size */
		map_submapnames (sizeaddr, map);
		map_submapnames (dimaddr, map);

		/* set in result */
		map_addtoresult (sizeaddr, map);
	}

	return 0;
}
/*}}}*/
/*{{{  static int occampi_mobilealloc_precode (compops_t *cops, tnode_t **nodep, codegen_t *cgen)*/
/*
 *	does pre-coding for a mobile allocation node
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_mobilealloc_precode (compops_t *cops, tnode_t **nodep, codegen_t *cgen)
{
	return 1;
}
/*}}}*/
/*{{{  static int occampi_mobilealloc_codegen (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	does code-generation for a mobile allocation node
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_mobilealloc_codegen (compops_t *cops, tnode_t *node, codegen_t *cgen)
{
	if (node->tag == opi.tag_NEWDYNMOBARRAY) {
		tnode_t *type = tnode_nthsubof (node, 2);
		mobiletypehook_t *mth = NULL;

		if (type->tag->ndef->lops && tnode_haslangop (type->tag->ndef->lops, "mobiletypedescof")) {
			mth = (mobiletypehook_t *)tnode_calllangop (type->tag->ndef->lops, "mobiletypedescof", 1, type);
		}
#if 1
fprintf (stderr, "occampi_mobilealloc_codegen(): here! type is:\n");
tnode_dumptree (type, 1, stderr);
#endif
		if (!mth) {
			nocc_internal ("occampi_mobilealloc_codegen(): missing mobile type hook!\n");
		} else {
			if (!mth->data && !mth->dlen) {
				codegen_callops (cgen, loadconst, mth->val);
			} else {
				codegen_callops (cgen, loadlabaddr, mth->label);
			}
			codegen_callops (cgen, tsecondary, I_MTALLOC);
		}
	} else {
		codegen_callops (cgen, comment, "FIXME: alloc mobile!");
		codegen_callops (cgen, loadconst, 0);
	}
	return 0;
}
/*}}}*/


/*{{{  static int occampi_mobiletypedecl_typecheck (compops_t *cops, tnode_t *node, typecheck_t *tc)*/
/*
 *	does type-checking on a mobile type-declaration node
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_mobiletypedecl_typecheck (compops_t *cops, tnode_t *node, typecheck_t *tc)
{
	int defchk = 1;
	int i = 1;

	if (node->tag == opi.tag_CHANTYPEDECL) {
		/*{{{  channel-type declaration*/
		tnode_t *type = tnode_nthsubof (node, 1);

		if (type && (type->tag == opi.tag_MOBILE)) {
			/* mobile channel-type declaration */
			tnode_t **typep = tnode_nthsubaddr (type, 0);

			defchk = 0;

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
						nocc_warning ("occampi_mobiletypedecl_typecheck(): NULL item in list!");
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
			// nocc_message ("FIXME: occampi_mobiletypedecl_typecheck(): mobile channel-type declaration!");
		}
		/*}}}*/
	} else if (node->tag == opi.tag_DATATYPEDECL) {
		/*{{{  data-type declaration*/
		tnode_t *type = tnode_nthsubof (node, 1);

		if (type && (type->tag == opi.tag_MOBILE)) {
			/* mobile data-type declaration */
			defchk = 0;
			nocc_message ("FIXME: occampi_mobiletypedecl_typecheck(): mobile data-type declaration!");
		}
		/*}}}*/
	}

	if (defchk) {
		if (cops->next && tnode_hascompop_i (cops->next, (int)COPS_TYPECHECK)) {
			i = tnode_callcompop_i (cops->next, (int)COPS_TYPECHECK, 2, node, tc);
		}
	}

	return i;
}
/*}}}*/


/*{{{  static int occampi_mobile_arraydopnode_premap (compops_t *cops, tnode_t **nodep, map_t *map)*/
/*
 *	does pre-mapping for an array DOP node for mobiles
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_mobile_arraydopnode_premap (compops_t *cops, tnode_t **nodep, map_t *map)
{
	tnode_t *t = *nodep;
	int v = 1;

	if (t->tag == opi.tag_DIMSIZE) {
		/* pre-map left */
		map_subpremap (tnode_nthsubaddr (t, 0), map);

		*nodep = map->target->newresult (t, map);

		return 0;
	}

	/* call-through */
	if (tnode_hascompop (cops->next, "premap")) {
		v = tnode_callcompop (cops->next, "premap", 2, nodep, map);
	}

	return v;
}
/*}}}*/
/*{{{  static int occampi_mobile_arraydopnode_namemap (compops_t *cops, tnode_t **nodep, map_t *map)*/
/*
 *	does name-mapping for an array DOP node for mobiles
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_mobile_arraydopnode_namemap (compops_t *cops, tnode_t **nodep, map_t *map)
{
	tnode_t *t = *nodep;
	int v = 1;

	if (t->tag == opi.tag_DIMSIZE) {
		/* name-map left */
		map_submapnames (tnode_nthsubaddr (*nodep, 0), map);

		return 0;
	}

	/* call-through */
	if (tnode_hascompop (cops->next, "namemap")) {
		v = tnode_callcompop (cops->next, "namemap", 2, nodep, map);
	}
	
	return v;
}
/*}}}*/
/*{{{  static int occampi_mobile_arraydopnode_codegen (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	does code-generation for an array DOP node (DIMSIZE) for mobiles
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_mobile_arraydopnode_codegen (compops_t *cops, tnode_t *node, codegen_t *cgen)
{
	int v = 1;

	if (node->tag == opi.tag_DIMSIZE) {
		int dimno;
		tnode_t *base = tnode_nthsubof (node, 0);
		tnode_t *dimt = tnode_nthsubof (node, 1);

		if (!constprop_isconst (dimt)) {
			nocc_internal ("occampi_mobile_arraydopnode_codegen(): dimension number not constant!");
			return 0;
		}
		dimno = constprop_intvalof (dimt);

#if 1
fprintf (stderr, "occampi_mobile_arraydopnode_codegen(): here!\n");
tnode_dumptree (node, 1, stderr);
#endif
		codegen_callops (cgen, loadnthpointer, base, 2, 0);
		codegen_callops (cgen, loadnonlocal, (dimno + 1) * cgen->target->pointersize);

		return 0;
	}

	/* call-through */
	if (tnode_hascompop (cops->next, "codegen")) {
		v = tnode_callcompop (cops->next, "codegen", 2, node, cgen);
	}

	return v;
}
/*}}}*/


/*{{{  static int occampi_mobile_actionnode_fetrans (compops_t *cops, tnode_t **nodep, fetrans_t *fe)*/
/*
 *	does front-end transformations for actions involving mobile types
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_mobile_actionnode_fetrans (compops_t *cops, tnode_t **nodep, fetrans_t *fe)
{
	int v = 1;
	tnode_t **saved_insertpoint = fe->insertpoint;
	tnode_t *t = *nodep;
	tnode_t *acttype = tnode_nthsubof (t, 2);
	int mangled = 0;

	fe->insertpoint = nodep;					/* before action is a good place to insert temporaries */

	if (acttype->tag == opi.tag_DYNMOBARRAY) {
		/*{{{  action involving dynamic mobile array*/
		tnode_t *rhs = tnode_nthsubof (t, 1);
		tnode_t *rhstype = typecheck_gettype (rhs, NULL);		/* get RHS type */
		// tnode_t *rhstype = NULL;

#if 1
fprintf (stderr, "occampi_mobile_actionnode_fetrans(): dynmobile action, rhs is:\n");
tnode_dumptree (rhs, 1, stderr);
#endif
		if (!rhstype) {
			nocc_internal ("occampi_mobile_actionnode_fetrans(): RHS of [%s] has no type!", t->tag->name);
			return 0;
		}

#if 1
fprintf (stderr, "occampi_mobile_actionnode_fetrans(): dynmobile action, rhstype is:\n");
tnode_dumptree (rhstype, 1, stderr);
#endif
		if (rhstype->tag != opi.tag_DYNMOBARRAY) {
			// tnode_t *nmobtype = typecheck_typereduce (acttype);
			tnode_t *nmobtype = tnode_copytree (rhstype);

			if (t->tag == opi.tag_ASSIGN) {
				/*{{{  deal with assignment to mobile array*/
				/* not a mobile array on the RHS, break into separate allocation and assignment */
				tnode_t *seqlist;
				tnode_t *adimlist, *atimesnode, *anode, *assnode;
				int d;
				tnode_t *rwalk;

				adimlist = parser_newlistnode (NULL);
				atimesnode = NULL;
				for (d=0, rwalk=rhstype; rwalk && (rwalk->tag == opi.tag_ARRAY); d++, rwalk = tnode_nthsubof (rwalk, 1)) {
					tnode_t *dimsize = tnode_nthsubof (rwalk, 0);

					if (!dimsize) {
						/* generate DIMSIZE expression */
						dimsize = tnode_create (opi.tag_DIMSIZE, NULL, rhs,
								constprop_newconst (CONST_INT, NULL,
									tnode_create (opi.tag_INT, NULL), d),
								tnode_create (opi.tag_INT, NULL));
					}
#if 0
fprintf (stderr, "occampi_mobile_actionnode_fetrans(): dimension %d has size:\n", d);
tnode_dumptree (dimsize, 1, stderr);
#endif
					if (!atimesnode) {
						atimesnode = tnode_copytree (dimsize);
					} else {
						atimesnode = tnode_create (opi.tag_TIMES, NULL, atimesnode, tnode_copytree (dimsize),
								tnode_create (opi.tag_INT, NULL));
					}

					parser_addtolist (adimlist, dimsize);
				}
#if 0
fprintf (stderr, "occampi_mobile_actionnode_fetrans(): got dimension list:\n");
tnode_dumptree (adimlist, 1, stderr);
fprintf (stderr, "occampi_mobile_actionnode_fetrans(): got dimension multiplication:\n");
tnode_dumptree (atimesnode, 1, stderr);
#endif

				anode = tnode_createfrom (opi.tag_NEWDYNMOBARRAY, t, tnode_copytree (tnode_nthsubof (acttype, 0)),
						adimlist, tnode_copytree (acttype), atimesnode);
				assnode = tnode_createfrom (opi.tag_ASSIGN, t, tnode_nthsubof (t, 0), anode, acttype);
#if 1
fprintf (stderr, "occampi_mobile_actionnode_fetrans(): allocation node:\n");
tnode_dumptree (assnode, 1, stderr);
#endif
#if 1
fprintf (stderr, "occampi_mobile_actionnode_fetrans(): here, non-mobile type to use is:\n");
tnode_dumptree (nmobtype, 1, stderr);
#endif
				/*
				seqlist = fetrans_makeseqassign (tnode_copytree (tnode_nthsubof (t, 0)),
						tnode_create (opi.tag_NEWDYNMOBARRAY, NULL, tnode_copytree (tnode_nthsubof (acttype, 0)),
						parser_buildlistnode (NULL,
							tnode_create (opi.tag_SIZE, NULL, tnode_copytree (rhs), tnode_create (opi.tag_INT, NULL)),
							NULL), tnode_copytree (acttype), NULL),
						tnode_copytree (acttype), fe);
				*/
				seqlist = fetrans_makeseqany (fe);
				parser_insertinlist (seqlist, assnode, 0);
				mangled = 1;

				/* change the type of the action to match the non-mobile type */
				tnode_setnthsub (t, 2, nmobtype);
				/*}}}*/
			} else if (t->tag == opi.tag_OUTPUT) {
				/*{{{  deal with output of mobile array (protocol is mobile)*/
				/*}}}*/
			}
		}

		/*}}}*/
	}

	fe->insertpoint = saved_insertpoint;				/* put back before call-through */

	/* call-through */
	if (mangled) {
		fetrans_subtree (nodep, fe);
		v = 0;
	} else {
		if (tnode_hascompop (cops->next, "fetrans")) {
			v = tnode_callcompop (cops->next, "fetrans", 2, nodep, fe);
		}
	}

	return v;
}
/*}}}*/


/*{{{  static int occampi_mobiles_init_nodes (void)*/
/*
 *	sets up nodes for occam-pi mobiles
 *	returns 0 on success, non-zero on error
 */
static int occampi_mobiles_init_nodes (void)
{
	tndef_t *tnd;
	compops_t *cops;
	langops_t *lops;
	int i;

	/*{{{  mobiletypedescof language operation*/
	if (tnode_newlangop ("mobiletypedescof", LOPS_INVALID, 1, origin_langparser (&occampi_parser)) < 0) {
		nocc_internal ("occampi_mobiles_init_nodes(): failed to create \"mobiletypedescof\" lang-op");
		return -1;
	}

	/*}}}*/
	/*{{{  occampi:mobiletypenode -- MOBILE, DYNMOBARRAY, CTCLI, CTSVR, CTSHCLI, CTSHSVR*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:mobiletypenode", &i, 1, 0, 0, TNF_NONE);		/* subnodes: subtype */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "prescope", 2, COMPOPTYPE (occampi_prescope_mobiletypenode));
	tnode_setcompop (cops, "premap", 2, COMPOPTYPE (occampi_premap_mobiletypenode));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "bytesfor", 2, LANGOPTYPE (occampi_mobiletypenode_bytesfor));
	tnode_setlangop (lops, "typereduce", 1, LANGOPTYPE (occampi_mobiletypenode_typereduce));
	tnode_setlangop (lops, "typeactual", 4, LANGOPTYPE (occampi_mobiletypenode_typeactual));
	tnode_setlangop (lops, "initsizes", 7, LANGOPTYPE (occampi_mobiletypenode_initsizes));
	tnode_setlangop (lops, "initialising_decl", 3, LANGOPTYPE (occampi_mobiletypenode_initialising_decl));
	tnode_setlangop (lops, "iscomplex", 2, LANGOPTYPE (occampi_mobiletypenode_iscomplex));
	tnode_setlangop (lops, "codegen_typeaction", 3, LANGOPTYPE (occampi_mobiletypenode_typeaction));
	tnode_setlangop (lops, "mobiletypedescof", 1, LANGOPTYPE (occampi_mobiletypenode_mobiletypedescof));
	tnode_setlangop (lops, "dimtreeof_node", 2, LANGOPTYPE (occampi_mobiletypenode_dimtreeof_node));
	tnd->lops = lops;

	i = -1;
	opi.tag_MOBILE = tnode_newnodetag ("MOBILE", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_DYNMOBARRAY = tnode_newnodetag ("DYNMOBARRAY", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_DYNMOBCTCLI = tnode_newnodetag ("DYNMOBCTCLI", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_DYNMOBCTSVR = tnode_newnodetag ("DYNMOBCTSVR", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_DYNMOBCTSHCLI = tnode_newnodetag ("DYNMOBCTSHCLI", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_DYNMOBCTSHSVR = tnode_newnodetag ("DYNMOBCTSHSVR", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_DYNMOBPROC = tnode_newnodetag ("DYNMOBPROC", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  occampi:mobilealloc -- NEWDYNMOBARRAY*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:mobilealloc", &i, 4, 0, 0, TNF_NONE);			/* subnodes: subtype, dimtree, type, bytes-for-array-expr */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (occampi_mobilealloc_typecheck));
	tnode_setcompop (cops, "constprop", 1, COMPOPTYPE (occampi_mobilealloc_constprop));
	tnode_setcompop (cops, "betrans", 2, COMPOPTYPE (occampi_mobilealloc_betrans));
	tnode_setcompop (cops, "premap", 2, COMPOPTYPE (occampi_mobilealloc_premap));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (occampi_mobilealloc_namemap));
	tnode_setcompop (cops, "precode", 2, COMPOPTYPE (occampi_mobilealloc_precode));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (occampi_mobilealloc_codegen));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "gettype", 2, LANGOPTYPE (occampi_mobilealloc_gettype));
	tnode_setlangop (lops, "dimtreeof", 1, LANGOPTYPE (occampi_mobilealloc_dimtreeof));
	tnd->lops = lops;

	i = -1;
	opi.tag_NEWDYNMOBARRAY = tnode_newnodetag ("NEWDYNMOBARRAY", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  compiler hooks*/
	if (!chook_demobiletype) {
		chook_demobiletype = tnode_lookupornewchook ("occampi:demobiletype");
		chook_demobiletype->chook_copy = occampi_copy_demobilechook;
		chook_demobiletype->chook_free = occampi_free_demobilechook;
		chook_demobiletype->chook_dumptree = occampi_dumptree_demobilechook;
	}

	if (!chook_mobiletypehook) {
		chook_mobiletypehook = tnode_lookupornewchook ("occampi:mobiletypehook");
		chook_mobiletypehook->chook_copy = occampi_copy_mobiletypehook;
		chook_mobiletypehook->chook_free = occampi_free_mobiletypehook;
		chook_mobiletypehook->chook_dumptree = occampi_dumptree_mobiletypehook;
	}

	if (!chook_dimtreehook) {
		chook_dimtreehook = tnode_lookupornewchook ("occampi:dimtreehook");
		chook_dimtreehook->chook_copy = occampi_copy_dimtreehook;
		chook_dimtreehook->chook_free = occampi_free_dimtreehook;
		chook_dimtreehook->chook_dumptree = occampi_dumptree_dimtreehook;
	}

	/*}}}*/
	/*{{{  change behaviour of (occampi:typedecl,CHANTYPEDECL)*/
	tnd = tnode_lookupnodetype ("occampi:typedecl");
	if (!tnd) {
		nocc_error ("occampi_mobiles_init_node(): failed to find \"occampi:typedecl\" node type");
		return -1;
	}

	cops = tnode_insertcompops (tnd->ops);
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (occampi_mobiletypedecl_typecheck));
	tnd->ops = cops;

	lops = tnode_insertlangops (tnd->lops);
	tnd->lops = lops;

	/*}}}*/

	return 0;
}
/*}}}*/
/*{{{  static int occampi_mobiles_post_setup (void)*/
/*
 *	does post-setup for occam-pi mobile nodes
 *	returns 0 on success, non-zero on error
 */
static int occampi_mobiles_post_setup (void)
{
	tndef_t *tnd;
	compops_t *cops;
	langops_t *lops;

	chook_actionlhstype = tnode_lookupchookbyname ("occampi:action:lhstype");

	/*{{{  interfere with leaf-type nodes for mobile type descriptors (direct)*/
	tnd = tnode_lookupnodetype ("occampi:leaftype");
	if (!tnd) {
		nocc_internal ("occampi_mobiles_post_setup(): no occampi:leaftype node type!");
	}

	tnode_setlangop (tnd->lops, "mobiletypedescof", 1, LANGOPTYPE (occampi_leaftype_mobiletypedescof));

	/*}}}*/
	/*{{{  intefere with arraydopnode for DIMSIZE*/
	tnd = tnode_lookupnodetype ("occampi:arraydopnode");
	if (!tnd) {
		nocc_internal ("occampi_mobiles_post_setup(): no occampi:arraydopnode node type!");
	}

	cops = tnode_insertcompops (tnd->ops);
	tnode_setcompop (cops, "premap", 2, COMPOPTYPE (occampi_mobile_arraydopnode_premap));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (occampi_mobile_arraydopnode_namemap));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (occampi_mobile_arraydopnode_codegen));
	tnd->ops = cops;
	lops = tnode_insertlangops (tnd->lops);
	tnd->lops = lops;

	/*}}}*/
	/*{{{  interfere with action-nodes for mobile/non-mobile I/O*/
	tnd = tnode_lookupnodetype ("occampi:actionnode");
	if (!tnd) {
		nocc_internal ("occampi_mobiles_post_setup(): no occampi:actionnode node type!");
	}

	cops = tnode_insertcompops (tnd->ops);
	tnode_setcompop (cops, "fetrans", 2, COMPOPTYPE (occampi_mobile_actionnode_fetrans));
	tnd->ops = cops;
	lops = tnode_insertlangops (tnd->lops);
	tnd->lops = lops;

	/*}}}*/

	return 0;
}
/*}}}*/


/*{{{  occampi_mobiles_feunit (feunit_t)*/
feunit_t occampi_mobiles_feunit = {
	init_nodes: occampi_mobiles_init_nodes,
	reg_reducers: NULL,
	init_dfatrans: NULL,
	post_setup: occampi_mobiles_post_setup,
	ident: "occampi-mobiles"
};
/*}}}*/


