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
#include "precheck.h"
#include "typecheck.h"
#include "usagecheck.h"
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

		// cgen->target->be_getoffsets (node, &ws_off, NULL, NULL, NULL);

		skiplab = codegen_new_label (cgen);
		codegen_callops (cgen, loadatpointer, node, cgen->target->pointersize);		/* load first dimension */
		codegen_callops (cgen, branch, I_CJ, skiplab);
		codegen_callops (cgen, loadatpointer, node, 0);					/* load pointer */
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

#if 1
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

	codegen_callops (cgen, debugline, mtype);
	codegen_callops (cgen, tsecondary, I_NULL);
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

#if 1
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

			tnode_t *lhstype = (tnode_t *)tnode_getchook (anode, chook_actionlhstype);

#if 1
fprintf (stderr, "occampi_mobiletypenode_action(): ASSIGN, lhstype (from hook %p) =\n", chook_actionlhstype);
tnode_dumptree (lhstype, 1, stderr);
fprintf (stderr, "occampi_mobiletypenode_action(): ASSIGN, lhs =\n");
tnode_dumptree (lhs, 1, stderr);
fprintf (stderr, "occampi_mobiletypenode_action(): ASSIGN, rhs =\n");
tnode_dumptree (rhs, 1, stderr);
#endif
			/* FIXME: we need to get hold of the LHS type before this, lhs is now a back-end node */
			occampi_condfreedynmobile (lhs, type, cgen, 0);

			codegen_callops (cgen, comment, "FIXME! (dynmobarray assign)");
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
					nocc_serious ("occampi_premap_mobiletypenode(): inner type of DYNMOBARRAY gave complex mobile type");
				} else {
					mth = occampi_newmobiletypehook ();
					mth->val = MT_MAKE_ARRAY_TYPE (1, imth->val);
					tnode_setchook (t, chook_mobiletypehook, mth);
				}
			} else {
				nocc_serious ("occampi_premap_mobiletypenode(): inner type of DYNMOBARRAY has no mobile type");
			}
		} else {
			nocc_internal ("occampi_mobiletypenode_mobiletypedescof(): unhandled mobile type [%s]", t->tag->name);
		}
	}

	return mth;
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
		rtype = tnode_createfrom (opi.tag_DYNMOBARRAY, node, subtype);

		tnode_setnthsub (node, 2, rtype);
	}
#if 0
fprintf (stderr, "occampi_mobilealloc_typecheck(): here!\n");
#endif

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
/*{{{  static int occampi_mobilealloc_premap (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	does pre-mapping for a mobile allocation node -- inserts back-end result
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_mobilealloc_premap (compops_t *cops, tnode_t **node, map_t *map)
{
	if ((*node)->tag == opi.tag_NEWDYNMOBARRAY) {
		/* pre-map dimension */
		map_subpremap (tnode_nthsubaddr (*node, 1), map);

		*node = map->target->newresult (*node, map);
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
		/* name-map dimension */
		map_submapnames (tnode_nthsubaddr (*node, 1), map);

		/* set in result */
		map_addtoresult (tnode_nthsubaddr (*node, 1), map);
	}

	return 0;
}
/*}}}*/
/*{{{  static int occampi_mobilealloc_codegen (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	does code-generation for a mobile allocation node
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_mobilealloc_codegen (compops_t *cops, tnode_t *node, codegen_t *cgen)
{
	codegen_callops (cgen, comment, "FIXME: alloc mobile!");
	codegen_callops (cgen, loadconst, 0);
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
	tnode_setlangop (lops, "initsizes", 7, LANGOPTYPE (occampi_mobiletypenode_initsizes));
	tnode_setlangop (lops, "initialising_decl", 3, LANGOPTYPE (occampi_mobiletypenode_initialising_decl));
	tnode_setlangop (lops, "iscomplex", 2, LANGOPTYPE (occampi_mobiletypenode_iscomplex));
	tnode_setlangop (lops, "codegen_typeaction", 3, LANGOPTYPE (occampi_mobiletypenode_typeaction));
	tnode_setlangop (lops, "mobiletypedescof", 1, LANGOPTYPE (occampi_mobiletypenode_mobiletypedescof));
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
	tnd = tnode_newnodetype ("occampi:mobilealloc", &i, 3, 0, 0, TNF_NONE);			/* subnodes: subtype, dimtree, type */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (occampi_mobilealloc_typecheck));
	tnode_setcompop (cops, "premap", 2, COMPOPTYPE (occampi_mobilealloc_premap));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (occampi_mobilealloc_namemap));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (occampi_mobilealloc_codegen));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "gettype", 2, LANGOPTYPE (occampi_mobilealloc_gettype));
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

	chook_actionlhstype = tnode_lookupchookbyname ("occampi:action:lhstype");

	/*{{{  interfere with leaf-type nodes for mobile type descriptors (direct)*/
	tnd = tnode_lookupnodetype ("occampi:leaftype");
	if (!tnd) {
		nocc_internal ("occampi_mobiles_post_setup(): no occampi:leaftype node type!");
	}

	tnode_setlangop (tnd->lops, "mobiletypedescof", 1, LANGOPTYPE (occampi_leaftype_mobiletypedescof));

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


