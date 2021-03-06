/*
 *	occampi_instance.c -- instance (PROC calls, etc.) handling for occampi
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
#include "dfa.h"
#include "parsepriv.h"
#include "occampi.h"
#include "feunit.h"
#include "names.h"
#include "scope.h"
#include "prescope.h"
#include "typecheck.h"
#include "constprop.h"
#include "usagecheck.h"
#include "tracescheck.h"
#include "fetrans.h"
#include "betrans.h"
#include "map.h"
#include "target.h"
#include "transputer.h"
#include "codegen.h"
#include "langops.h"

/*}}}*/
/*{{{  private types*/

typedef enum ENUM_builtinprocname {
	BI_INVALID,
	BI_SETPRI,
	BI_RESCHEDULE,
	BI_ASSERT
} builtinprocname_e;

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
	builtinprocname_e val;
} builtinproc_t;

typedef struct TAG_builtinprochook {
	builtinproc_t *biptr;
} builtinprochook_t;


/*}}}*/
/*{{{  forward decls*/
static void builtin_codegen_reschedule (tnode_t *node, builtinproc_t *builtin, codegen_t *cgen);
static void builtin_codegen_setpri (tnode_t *node, builtinproc_t *builtin, codegen_t *cgen);
static void builtin_codegen_assert (tnode_t *node, builtinproc_t *builtin, codegen_t *cgen);


/*}}}*/
/*{{{  private data*/
/* these are used to fill in workspace-sizes once target info is known */
#define BUILTIN_DS_MIN (-1)
#define BUILTIN_DS_IO (-2)
#define BUILTIN_DS_ALTIO (-3)
#define BUILTIN_DS_WAIT (-4)
#define BUILTIN_DS_MAX (-5)

static builtinproc_t builtins[] = {
	{"SETPRI", "SETPRI", NULL, NULL, 4, BUILTIN_DS_MIN, builtin_codegen_setpri, "PROC xxSETPRI (VAL INT newpri)\n", NULL, BI_SETPRI},
	{"RESCHEDULE", "RESCHEDULE", NULL, NULL, 0, BUILTIN_DS_MIN, builtin_codegen_reschedule, NULL, NULL, BI_RESCHEDULE},
	{"ASSERT", "ASSERT", NULL, NULL, 4, BUILTIN_DS_MIN, builtin_codegen_assert, "PROC xxASSERT (VAL BOOL val)\n", NULL, BI_ASSERT},
	{NULL, NULL, NULL, NULL, 0, 0, NULL, NULL, NULL, BI_INVALID}
};

static chook_t *chook_matchedformal = NULL;

static chook_t *trimplchook = NULL;
static chook_t *trtracechook = NULL;
static chook_t *trbvarschook = NULL;

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
/*{{{  static void builtinprochook_dumphook (tnode_t *node, void *hook, int indent, fhandle_t *stream)*/
/*
 *	dumps a builtinprochook_t (debugging)
 */
static void builtinprochook_dumphook (tnode_t *node, void *hook, int indent, fhandle_t *stream)
{
	builtinprochook_t *bph = (builtinprochook_t *)hook;

	occampi_isetindent (stream, indent);
	if (!hook) {
		fhandle_printf (stream, "<builtinprochook name=\"(null)\" />\n");
	} else {
		builtinproc_t *builtin = bph->biptr;

		fhandle_printf (stream, "<builtinprochook name=\"%s\" wsh=\"%d\" wsl=\"%d\" />\n", builtin->name, builtin->wsh, builtin->wsl);
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


/*{{{  static void *occampi_matchedformal_chook_copy (void *chook)*/
/*
 *	copies a matchedformal compiler hook
 */
static void *occampi_matchedformal_chook_copy (void *chook)
{
	return chook;		/* alias */
}
/*}}}*/
/*{{{  static void occampi_matchedformal_chook_free (void *chook)*/
/*
 *	frees a matchedformal compiler hook
 */
static void occampi_matchedformal_chook_free (void *chook)
{
	/* alias, do do nothing */
	return;
}
/*}}}*/
/*{{{  static void occampi_matchedformal_chook_dumptree (tnode_t *node, void *chook, int indent, fhandle_t *stream)*/
/*
 *	dumps a matchedformal compiler hook (debugging)
 */
static void occampi_matchedformal_chook_dumptree (tnode_t *node, void *chook, int indent, fhandle_t *stream)
{
	occampi_isetindent (stream, indent);
	fhandle_printf (stream, "<chook:matchedformal addr=\"0x%8.8x\" />\n", (unsigned int)chook);
	return;
}
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

	/*{{{  get formal and actual parameter lists*/
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
		/* make it a list */
		aparamlist = parser_buildlistnode (NULL, aparamlist, NULL);
		tnode_setnthsub (node, 1, aparamlist);			/* change parameters */
		ap_items = parser_getlistitems (aparamlist, &ap_nitems);
	}

	/*}}}*/
	/*{{{  type-check actual parameters*/
	typecheck_subtree (aparamlist, tc);

	/*}}}*/

	for (paramno = 1, fp_ptr = 0, ap_ptr = 0; (fp_ptr < fp_nitems) && (ap_ptr < ap_nitems);) {
		/*{{{  type-check/type-actual parameter*/
		tnode_t *ftype, *atype;
		occampi_typeattr_t fattr = TYPEATTR_NONE;
		occampi_typeattr_t aattr = TYPEATTR_NONE;

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

		fattr = occampi_typeattrof (ftype);
		aattr = occampi_typeattrof (ap_items[ap_ptr]);

#if 0
fprintf (stderr, "occampi_typecheck_instance: fattr=0x%8.8x, ftype=\n", (unsigned int)fattr);
tnode_dumptree (ftype, 1, stderr);
fprintf (stderr, "occampi_typecheck_instance: aattr=0x%8.8x, atype=\n", (unsigned int)aattr);
tnode_dumptree (atype, 1, stderr);
fprintf (stderr, "occampi_typecheck_instance: ap_items[ap_ptr]=\n");
tnode_dumptree (ap_items[ap_ptr], 1, stderr);
#endif
		if (fp_items[fp_ptr]->tag == opi.tag_FPARAM) {
			if (!langops_isvar (ap_items[ap_ptr])) {
				typecheck_error (node, tc, "parameter %d must be a variable", paramno);
			}
		}

		/* set parameter info in type-check, slightly special, but needed for some things */
		tc->this_ftype = ftype;
		tc->this_aparam = ap_items[ap_ptr];

		if (!typecheck_typeactual (ftype, atype, node, tc)) {
			typecheck_error (node, tc, "incompatible types for parameter %d", paramno);
		}

		tc->this_ftype = NULL;
		tc->this_aparam = NULL;

		if (fattr && aattr) {
			if ((fattr ^ aattr) & (TYPEATTR_MARKED_IN | TYPEATTR_MARKED_OUT)) {
				typecheck_error (node, tc, "incompatible type attributes for parameter %d", paramno);
			}
		}

		fp_ptr++;
		ap_ptr++;
		paramno++;
		/*}}}*/
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
/*{{{  static int occampi_constprop_instance (compops_t *cops, tnode_t **nodep)*/
/*
 *	does constant propagation on an instance node
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_constprop_instance (compops_t *cops, tnode_t **nodep)
{
	tnode_t *nname = tnode_nthsubof (*nodep, 0);

	if (nname->tag == opi.tag_BUILTINPROC) {
		/*{{{  do constant propagation for built-in procedure, parameters first (normal)*/
		builtinprochook_t *bph = (builtinprochook_t *)tnode_nthhookof (nname, 0);
		builtinproc_t *builtin = bph->biptr;

		constprop_tree (tnode_nthsubaddr (*nodep, 1));

		switch (builtin->val) {
		case BI_INVALID:
			nocc_internal ("occampi_constprop_instance(): invalid builtin");
			break;
		case BI_RESCHEDULE:
		case BI_SETPRI:
			/* nothing special */
			break;
		case BI_ASSERT:
			/*{{{  constant asserts reducible to SKIP or STOP*/
			{
				int nparams, i;
				tnode_t **params = parser_getlistitems (tnode_nthsubof (*nodep, 1), &nparams);

				if (nparams != 1) {
					constprop_error (*nodep, "ASSERT has %d parameters!", nparams);
				} else {
					tnode_t *param = params[0];

					if (constprop_isconst (param)) {
						tnode_t *newnode = NULL;
						int v = constprop_intvalof (param);

#if 0
fprintf (stderr, "occampi_constprop_instance(): constant ASSERT parameter:\n");
tnode_dumptree (param, 1, stderr);
#endif
						if (!v) {
							/* constant FALSE, reduce to STOP */
							newnode = tnode_createfrom (opi.tag_STOP, *nodep);
						} else {
							/* constant TRUE, reduce to SKIP */
							newnode = tnode_createfrom (opi.tag_STOP, *nodep);
						}

						tnode_free (*nodep);
						*nodep = newnode;
					}
				}
			}
			/*}}}*/
			break;
		}
		/*}}}*/

		return 0;
	}
	return 1;
}
/*}}}*/
/*{{{  static int occampi_tracescheck_instance (compops_t *cops, tnode_t *node, tchk_state_t *tc)*/
/*
 *	does traces checking on an instance node
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_tracescheck_instance (compops_t *cops, tnode_t *node, tchk_state_t *tc)
{
	tnode_t *nname;

	nname = tnode_nthsubof (node, 0);
#if 0
fprintf (stderr, "occampi_tracescheck_instance(): here!, nname =\n");
tnode_dumptree (nname, 1, stderr);
#endif

	if (nname->tag == opi.tag_BUILTINPROC) {
		/*{{{  handle traces for built-in procedures*/
		builtinprochook_t *bph = (builtinprochook_t *)tnode_nthhookof (nname, 0);
		builtinproc_t *builtin = bph->biptr;

#if 0
fprintf (stderr, "occampi_tracescheck_instance(): BUILTINPROC, nname =\n");
tnode_dumptree (nname, 1, stderr);
#endif
		switch (builtin->val) {
		case BI_INVALID:
			nocc_internal ("occampi_tracescheck_instance(): invalid builtin");
			break;
		case BI_RESCHEDULE:
		case BI_ASSERT:
			/* no traces for these currently */
			break;
		case BI_SETPRI:
			/*{{{  traces for setpri*/
			/* FIXME: maybe want to capure this in traces (communication with the scheduler) */
			/*}}}*/
			break;
		}
		/*}}}*/
	} else {
		/*{{{  handle traces for regular procedure instance*/
		name_t *iname;
		tnode_t *decl;

		iname = tnode_nthnameof (nname, 0);
		decl = NameDeclOf (iname);

		if (decl) {
			/*{{{  do traces checks for instance (based on traces attached to declaration)*/
			tchk_traces_t *trc = (tchk_traces_t *)tnode_getchook (decl, trtracechook);

			if (trc && DA_CUR (trc->items)) {
				/*
				 * traces are in terms of PROC formal parameters, need to substitute in actual parameters
				 */
				tnode_t *fparamlist = typecheck_gettype (tnode_nthsubof (node, 0), NULL);
				tnode_t *aparamlist = tnode_nthsubof (node, 1);
				tchknode_t *rtraces;
				tnode_t **apset, **fpset;
				int nfp, nap, i;

	#if 0
	fprintf (stderr, "occampi_tracescheck_instance(): here, got traces:\n");
	tracescheck_dumptraces (trc, 1, stderr);
	#endif
				rtraces = tracescheck_tracestondet (trc);
				if (rtraces) {
					tnode_t **fpsetcopy = NULL;
					tnode_t **apsetcopy = NULL;

					fpset = parser_getlistitems (fparamlist, &nfp);
					apset = parser_getlistitems (aparamlist, &nap);

					if (nfp != nap) {
						nocc_internal ("occampi_tracescheck_instance(): expected %d parameters on PROC, got %d",
								nfp, nap);
						return 0;
					}
					if (nfp > 0) {
						fpsetcopy = (tnode_t **)smalloc (nfp * sizeof (tnode_t *));

						for (i=0; i<nfp; i++) {
							if (fpset[i]->tag == opi.tag_FPARAM) {
								/* formal parameter (expected) */
								fpsetcopy[i] = tnode_nthsubof (fpset[i], 0);
							} else {
								fpsetcopy[i] = fpset[i];
							}
						}
					}
					if (nap > 0) {
						apsetcopy = (tnode_t **)smalloc (nap * sizeof (tnode_t *));

						for (i=0; i<nap; i++) {
							apsetcopy[i] = apset[i];

							/* skip through transparent nodes in actual parameters -- usually decorations */
							while (apsetcopy[i] && (tnode_tnflagsof (apsetcopy[i]) & TNF_TRANSPARENT)) {
								apsetcopy[i] = tnode_nthsubof (apsetcopy[i], 0);
							}
						}
					}

					tracescheck_substitutenodes (rtraces, fpsetcopy, apsetcopy, nfp);
	#if 0
	fprintf (stderr, "occampi_tracescheck_instance(): here!  reduced substituted %d non-determinstic traces of PROC [%s] are:\n", nfp, NameNameOf (iname));
	tracescheck_dumpnode (rtraces, 1, stderr);
	#endif
					tracescheck_addtobucket (tc, rtraces);

					if (fpsetcopy) {
						sfree (fpsetcopy);
					}
					if (apsetcopy) {
						sfree (apsetcopy);
					}
				}
			}
			/*}}}*/
		}
		/*}}}*/
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

		fattr = occampi_typeattrof (ftype);

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
/*{{{  static int occampi_fetrans_instance (compops_t *cops, tnode_t **node, fetrans_t *fe)*/
/*
 *	does front-end transforms on an instance-node -- includes dropping in array-dimensions
 *	returns 1 to continue walk, 0 to stop
 */
static int occampi_fetrans_instance (compops_t *cops, tnode_t **node, fetrans_t *fe)
{
	tnode_t *fparamlist = typecheck_gettype (tnode_nthsubof (*node, 0), NULL);
	tnode_t *aparamlist = tnode_nthsubof (*node, 1);
	tnode_t **fp_items, **ap_items;
	int fp_nitems, ap_nitems;
	int fp_ptr, ap_ptr;
	int paramno;
	tnode_t *lastaparam = NULL;

#if 0
fprintf (stderr, "occampi_fetrans_instance: fparamlist=\n");
tnode_dumptree (fparamlist, 1, stderr);
fprintf (stderr, "occampi_fetrans_instance: aparamlist=\n");
tnode_dumptree (aparamlist, 1, stderr);
#endif

	/*{{{  get formal and actual parameter lists*/
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
		nocc_internal ("occampi_fetrans_instance(): actual parameters not a list [%s]", aparamlist->tag->name);
		ap_items = NULL;
	}
	/*}}}*/

	for (paramno = 1, fp_ptr = 0, ap_ptr = 0; fp_ptr < fp_nitems; fp_ptr++) {
		tnode_t *fparam = fp_items[fp_ptr];

		if ((fparam->tag == opi.tag_FPARAM) || (fparam->tag == opi.tag_VALFPARAM) || (fparam->tag == opi.tag_RESFPARAM)) {
			/*{{{  skip to next parameter*/
			lastaparam = ap_items[ap_ptr];
			tnode_setchook (ap_items[ap_ptr], chook_matchedformal, fp_items[fp_ptr]);
			ap_ptr++;
			paramno++;
			/*}}}*/
		} else if (fparam->tag == opi.tag_HIDDENDIMEN) {
			/*{{{  hidden dimension*/
			tnode_t *atype = typecheck_gettype (lastaparam, NULL);
			tnode_t *adimtree = langops_dimtreeof (lastaparam);
			tnode_t *fhparm = tnode_nthsubof (fparam, 0);

			if (!adimtree) {
				nocc_internal ("occampi_fetrans_instance(): hidden formal dimension, but last actual [%s] has no dimension tree", lastaparam->tag->name);
			}

			if (fhparm->tag == opi.tag_DIMSIZE) {
				int vdim;
				tnode_t *dnode = tnode_nthsubof (fhparm, 1);		/* dimension number */
				tnode_t *dimparam = NULL;

				if (!constprop_isconst (dnode)) {
					nocc_internal ("occampi_fetrans_instance(): hidden formal dimension with DIMSIZE, but bad dimension tag [%s]", dnode->tag->name);
				}
				vdim = constprop_intvalof (dnode);

#if 0
fprintf (stderr, "occampi_fetrans_instance: got hidden formal dimension with DIMSIZE for dimension %d, last actual was:\n", vdim);
tnode_dumptree (lastaparam, 1, stderr);
fprintf (stderr, "occampi_fetrans_instance: last actual dimension tree:\n");
tnode_dumptree (adimtree, 1, stderr);
#endif
				dimparam = parser_getfromlist (adimtree, vdim);
				if (!dimparam) {
					tnode_error (*node, "occampi_fetrans_instance(): no dimension %d on actual-parameter [%s]", vdim, lastaparam->tag->name);
				} else {
					/* insert this into the list of actual parameters */
					// dimparam = tnode_copytree (dimparam);
					/* XXX: don't copy dimension parameter -- will fail name-map if we do that */

					if (!aparamlist) {
						aparamlist = parser_buildlistnode (NULL, dimparam, NULL);
						tnode_setnthsub (*node, 1, aparamlist);
					} else {
						parser_insertinlist (aparamlist, dimparam, ap_ptr);
					}
					tnode_setchook (dimparam, chook_matchedformal, fparam);

					ap_items = parser_getlistitems (aparamlist, &ap_nitems);

					ap_ptr++;
				}
			} else {
				tnode_error (*node, "occampi_fetrans_instance(): unknown HIDDENDIMEN type [%s]", fhparm->tag->name);
			}
			/*}}}*/
		}
	}

	return 1;
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
	if (!tnode_nthsubof (*node, 1)) {
		/* no parameters! */
	} else {
		int i, nparams;
		tnode_t **params = parser_getlistitems (tnode_nthsubof (*node, 1), &nparams);

		for (i=0; i<nparams; i++) {
			void *matchedformal = tnode_getchook (params[i], chook_matchedformal);
			
#if 1
fprintf (stderr, "occampi_namemap_instance(): setting parameter %d matchedformal to hook in:\n", i);
tnode_dumptree (params[i], 1, FHAN_STDERR);
#endif
			tnode_setchook (params[i], chook_matchedformal, NULL);
			map_submapnames (params + i, map);
			tnode_setchook (params[i], chook_matchedformal, matchedformal);
		}
	}
	map_submapnames (tnode_nthsubaddr (*node, 0), map);

	namenode = tnode_nthsubof (*node, 0);
#if 1
fprintf (stderr, "occampi_namemap_instance(): instance of:\n");
tnode_dumptree (namenode, 1, FHAN_STDERR);
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

#if 0
fprintf (stderr, "occampi_codegen_instance(): params are:\n");
tnode_dumptree (params, 1, stderr);
#endif

		if (!params) {
			/* no parameters! */
		} else if (parser_islistnode (params)) {
			int nitems, i, wsoff;
			tnode_t **items = parser_getlistitems (params, &nitems);

			wsoff = -(cgen->target->slotsize);
			for (i=nitems - 1; i>=0; i--) {
				tnode_t *formal = (tnode_t *)tnode_getchook (items[i], chook_matchedformal);
				codegen_parammode_e pmode;

				if (!formal) {
					tnode_warning (node, "occampi_codegen_instance(): no matched formal in parameter [%s] %d, assuming reference", items[i]->tag->name, i);
					pmode = PARAM_REF;
				} else {
#if 0
fprintf (stderr, "occampi_codegen_instance(): matched formal for this parameter is:\n");
tnode_dumptree (formal, 1, stderr);
#endif
					if (formal->tag == opi.tag_VALFPARAM) {
						tnode_t *ftype = tnode_nthsubof (formal, 1);

						pmode = langops_valbyref (ftype) ? PARAM_REF : PARAM_VAL;
					} else if ((formal->tag == opi.tag_HIDDENDIMEN) || (formal->tag == opi.tag_HIDDENPARAM)) {
						pmode = PARAM_VAL;
					} else {
						pmode = PARAM_REF;
					}
				}
				codegen_callops (cgen, loadparam, items[i], pmode);
				codegen_callops (cgen, storelocal, wsoff);

				wsoff -= (cgen->target->slotsize);
			}
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
/*{{{  static void builtin_codegen_assert (tnode_t *node, builtinproc_t *builtin, codegen_t *cgen)*/
/*
 *	generates code for an ASSERT(VAL BOOL)
 */
static void builtin_codegen_assert (tnode_t *node, builtinproc_t *builtin, codegen_t *cgen)
{
	tnode_t *param = tnode_nthsubof (node, 1);
	int skiplab = codegen_new_label (cgen);

#if 0
fprintf (stderr, "builtin_codegen_assert(): param =\n");
tnode_dumptree (param, 1, stderr);
#endif
	if (parser_islistnode (param)) {
		int nparams;
		tnode_t **items = parser_getlistitems (param, &nparams);

		if (nparams != 1) {
			nocc_internal ("builtin_codegen_assert(): %d parameters!", nparams);
		}
		param = items[0];
	}

	codegen_callops (cgen, loadname, param, 0);
	codegen_callops (cgen, tsecondary, I_BOOLINVERT);
	codegen_callops (cgen, branch, I_CJ, skiplab);
	codegen_callops (cgen, tsecondary, I_SETERR);
	codegen_callops (cgen, setlabel, skiplab);

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

			biname = tnode_create (opi.tag_BUILTINPROC, SLOCN (tok->origin), builtinprochook_create (&(builtins[i])));
			dfa_pushnode (dfast, biname);

			lexer_freetoken (tok);
			return;
			/*}}}*/
		}
	}
	parser_pushtok (pp, tok);
	parser_error (SLOCN (tok->origin), "unknown built-in PROC [%s]", lexer_stokenstr (tok));

	return;
}
/*}}}*/

/*{{{  tnode_t *occampi_makeassertion (lexfile_t *lf, tnode_t *param)*/
/*
 *	creates an assertion instance node, used by occam-pi front-end code elsewhere
 *	returns ASSERT proc-instance on success, NULL on failure
 */
tnode_t *occampi_makeassertion (lexfile_t *lf, tnode_t *param)
{
	tnode_t *asnode = NULL;
	builtinproc_t *biproc = NULL;
	int i;

	for (i=0; builtins[i].name; i++) {
		if (builtins[i].val == BI_ASSERT) {
			biproc = &(builtins[i]);
		}
	}

	if (!biproc) {
		nocc_internal ("occampi_makeassertion(): failed to find ASSERT built-in!");
		return NULL;
	}

	asnode = tnode_create (opi.tag_PINSTANCE, SLOCN (lf),
			tnode_create (opi.tag_BUILTINPROC, SLOCN (lf), builtinprochook_create (biproc)),
			parser_buildlistnode (SLOCN (lf), param, NULL),
			NULL);

	return asnode;
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
	tnode_setcompop (cops, "constprop", 1, COMPOPTYPE (occampi_constprop_instance));
	tnode_setcompop (cops, "tracescheck", 2, COMPOPTYPE (occampi_tracescheck_instance));
	tnode_setcompop (cops, "fetrans", 2, COMPOPTYPE (occampi_fetrans_instance));
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
	/*{{{  chook:matchedformal compiler hook*/
	chook_matchedformal = tnode_lookupornewchook ("chook:matchedformal");
	chook_matchedformal->flags |= CHOOK_AUTOPROMOTE;
	chook_matchedformal->chook_copy = occampi_matchedformal_chook_copy;
	chook_matchedformal->chook_free = occampi_matchedformal_chook_free;
	chook_matchedformal->chook_dumptree = occampi_matchedformal_chook_dumptree;

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
/*{{{  static int occampi_instance_post_setup (void)*/
/*
 *	does post-setup for instance nodes
 *	returns 0 on success, non-zero on failure
 */
static int occampi_instance_post_setup (void)
{
	trimplchook = tracescheck_getimplchook ();
	trtracechook = tracescheck_gettraceschook ();
	trbvarschook = tracescheck_getbvarschook ();

	if (!trimplchook || !trtracechook || !trbvarschook) {
		nocc_internal ("occampi_instance_post_setup(): failed to find traces compiler hooks");
		return -1;
	}

	return 0;
}
/*}}}*/


/*{{{  occampi_instance_feunit (feunit_t)*/
feunit_t occampi_instance_feunit = {
	.init_nodes = occampi_instance_init_nodes,
	.reg_reducers = occampi_instance_reg_reducers,
	.init_dfatrans = occampi_instance_init_dfatrans,
	.post_setup = occampi_instance_post_setup,
	.ident = "occampi-instance"
};
/*}}}*/

