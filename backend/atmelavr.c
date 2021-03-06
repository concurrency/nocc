/*
 *	atmelavr.c -- Atmel AVR back-end
 *	Copyright (C) 2012-2016 Fred Barnes <frmb@kent.ac.uk>
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
 *	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
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
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#include <errno.h>

#include "nocc.h"
#include "support.h"
#include "version.h"
#include "fhandle.h"
#include "tnode.h"
#include "opts.h"
#include "lexer.h"
#include "parser.h"
#include "treeops.h"
#include "langops.h"
#include "names.h"
#include "typecheck.h"
#include "constprop.h"
#include "target.h"
#include "betrans.h"
#include "map.h"
#include "avrasm.h"
#include "atmelavr.h"
#include "codegen.h"
#include "allocate.h"
#include "avrinstr.h"

/*}}}*/
/*{{{  forward decls*/
static int atmelavr_target_init (target_t *target);

static int atmelavr_be_codegen_init (codegen_t *cgen, lexfile_t *srcfile);
static int atmelavr_be_codegen_final (codegen_t *cgen, lexfile_t *srcfile);

static void atmelavr_be_do_precode (tnode_t **ptptr, codegen_t *cgen);
static void atmelavr_be_do_codegen (tnode_t *tptr, codegen_t *cgen);

/*}}}*/

/*{{{  target_t for this target*/

target_t atmelavr_target = {
	.initialised =		0,
	.name =			"atmelavr",
	.tarch =		"avr",
	.tvendor =		"atmel",
	.tos =			NULL,
	.desc =			"Atmel AVR code",
	.extn =			"lst",
	.tcap = {
		.can_do_fp = 0,
		.can_do_dmem = 0,
	},
	.bws = {			/* below and above workspace meaningless for this */
		.ds_min = 0,
		.ds_io = 0,
		.ds_altio = 0,
		.ds_wait = 0,
		.ds_max = 0,
	},
	.aws = {
		.as_alt = 0,
		.as_par = 0,
	},

	.chansize =		0,
	.charsize =		1,
	.intsize =		1,
	.pointersize =		2,
	.slotsize =		0,
	.structalign =		0,
	.maxfuncreturn =	0,
	.skipallocate =		1,

	.tag_NAME =		NULL,
	.tag_NAMEREF =		NULL,
	.tag_BLOCK =		NULL,
	.tag_CONST =		NULL,
	.tag_INDEXED =		NULL,
	.tag_BLOCKREF =		NULL,
	.tag_STATICLINK =	NULL,
	.tag_RESULT =		NULL,

	.init =			atmelavr_target_init,
	.newname =		NULL,
	.newnameref =		NULL,
	.newblock =		NULL,
	.newindexed =		NULL,
	.newblockref =		NULL,
	.newresult =		NULL,
	.inresult =		NULL,

	.be_getorgnode =	NULL,
	.be_blockbodyaddr =	NULL,
	.be_allocsize =		NULL,
	.be_typesize =		NULL,
	.be_settypecat =	NULL,
	.be_gettypecat =	NULL,
	.be_setoffsets =	NULL,
	.be_getoffsets =	NULL,
	.be_blocklexlevel =	NULL,
	.be_setblocksize =	NULL,
	.be_getblocksize =	NULL,
	.be_codegen_init =	atmelavr_be_codegen_init,
	.be_codegen_final =	atmelavr_be_codegen_final,

	.be_precode_seenproc =	NULL,

	.be_do_betrans =	NULL,
	.be_do_premap =		NULL,
	.be_do_namemap =	NULL,
	.be_do_preallocate =	NULL,
	.be_do_precode =	atmelavr_be_do_precode,
	.be_do_codegen =	atmelavr_be_do_codegen,

	.priv =			NULL
};

/*}}}*/
/*{{{  private types*/

/* used to define ranges inside regions */
typedef struct TAG_imgrange {
	int start;
	int size;
} imgrange_t;

/* image for a specific region */
typedef struct TAG_atmelavr_image {
	tnode_t *zone;			/* particular segment */
	unsigned char *image;		/* raw image */
	int isize;			/* image size (as allocated) */
	int canwrite;			/* whether we can actually write stuff here */
	DYNARRAY (imgrange_t *, ranges);
} atmelavr_image_t;


typedef struct TAG_atmelavr_priv {
	lexfile_t *lastfile;
	avrtarget_t *mcu;
	DYNARRAY (atmelavr_image_t *, images);
} atmelavr_priv_t;


/* used for labels definitions, includes fixup */
typedef struct TAG_aavr_labelfixup {
	atmelavr_image_t *img;				/* image where the assemblage goes */
	tnode_t *instr;					/* instruction node (presumably involving the label) */
	int offset;					/* byte offset in image where this should be assembled */
} aavr_labelfixup_t;

typedef struct TAG_aavr_labelinfo {
	atmelavr_image_t *img;				/* in which image this lives, NULL if none */
	int baddr;					/* byte-address, -1 if not defined yet */
	tnode_t *labins;				/* label defining instruction */
	DYNARRAY (aavr_labelfixup_t *, fixups);		/* instructions that require more assembly */
} aavr_labelinfo_t;


/* used during late constant propagation to get label addresses */
typedef struct TAG_aavr_constpropstate {
	atmelavr_image_t *img;
	tnode_t *instr;
	int offset;
	int didfix;
} aavr_constpropstate_t;

/*}}}*/
/*{{{  private data*/

static chook_t *labelinfo_chook = NULL;

static aavr_constpropstate_t *atmelavr_constpropstate = NULL;

/*}}}*/


/*{{{  void atmelavr_isetindent (fhandle_t *stream, int indent)*/
/*
 *	set-indent for debugging output
 */
void atmelavr_isetindent (fhandle_t *stream, int indent)
{
	int i;

	for (i=0; i<indent; i++) {
		fhandle_printf (stream, "    ");
	}
	return;
}
/*}}}*/
/*{{{  static int atmelavr_opthandler_flag (cmd_option_t *opt, char ***argwalk, int *argleft)*/
/*
 *	option handler for this target's options
 *	returns 0 on success, non-zero on failure
 */
static int atmelavr_opthandler_flag (cmd_option_t *opt, char ***argwalk, int *argleft)
{
	return 0;
}
/*}}}*/
/*{{{  static int atmelavr_init_options (atmelavr_priv_t *apriv)*/
/*
 *	initialises options for atmel-avr back-end
 *	returns 0 on success, non-zero on failure
 */
static int atmelavr_init_options (atmelavr_priv_t *apriv)
{
	return 0;
}
/*}}}*/


/*{{{  static imgrange_t *atmelavr_newimgrange (void)*/
/*
 *	creates a new imgrange_t structure
 */
static imgrange_t *atmelavr_newimgrange (void)
{
	imgrange_t *imr = (imgrange_t *)smalloc (sizeof (imgrange_t));

	imr->start = 0;
	imr->size = 0;

	return imr;
}
/*}}}*/
/*{{{  static void atmelavr_freeimgrange (imgrange_t *imr)*/
/*
 *	frees an imgrange_t structure
 */
static void atmelavr_freeimgrange (imgrange_t *imr)
{
	if (!imr) {
		nocc_serious ("atmelavr_freeimgrange(): NULL pointer!");
		return;
	}
	sfree (imr);
	return;
}
/*}}}*/
/*{{{  static atmelavr_image_t *atmelavr_newatmelavrimage (void)*/
/*
 *	creates a new atmelavr_image_t structure, used for images of memory (eeprom,flash,etc.)
 */
static atmelavr_image_t *atmelavr_newatmelavrimage (void)
{
	atmelavr_image_t *aai = (atmelavr_image_t *)smalloc (sizeof (atmelavr_image_t));

	aai->zone = NULL;
	aai->image = NULL;
	aai->isize = 0;
	aai->canwrite = 0;
	dynarray_init (aai->ranges);

	return aai;
}
/*}}}*/
/*{{{  static void atmelavr_freeatmelavrimage (atmelavr_image_t *aai)*/
/*
 *	frees an atmelavr_image_t structure
 */
static void atmelavr_freeatmelavrimage (atmelavr_image_t *aai)
{
	int i;

	if (!aai) {
		nocc_serious ("atmelavr_freeatmelavrimage(): NULL pointer!");
		return;
	}
	aai->zone = NULL;
	if (aai->image) {
		sfree (aai->image);
		aai->image = NULL;
	}
	aai->isize = 0;
	for (i=0; i<DA_CUR (aai->ranges); i++) {
		imgrange_t *imr = DA_NTHITEM (aai->ranges, i);

		if (imr) {
			atmelavr_freeimgrange (imr);
		}
	}
	dynarray_trash (aai->ranges);

	sfree (aai);
	return;
}
/*}}}*/
/*{{{  static atmelavr_priv_t *atmelavr_newatmelavrpriv (void)*/
/*
 *	creates a new atmelavr_priv_t structure
 */
static atmelavr_priv_t *atmelavr_newatmelavrpriv (void)
{
	atmelavr_priv_t *aap = (atmelavr_priv_t *)smalloc (sizeof (atmelavr_priv_t));

	aap->lastfile = NULL;
	aap->mcu = NULL;
	dynarray_init (aap->images);

	return aap;
}
/*}}}*/
/*{{{  static void atmelavr_freeatmelavrpriv (atmelavr_priv_t *aap)*/
/*
 *	frees an atmelavr_priv_t structure
 */
static void atmelavr_freeatmelavrpriv (atmelavr_priv_t *aap)
{
	int i;

	if (!aap) {
		nocc_serious ("atmelavr_freeatmelavrpriv(): NULL pointer!");
		return;
	}
	aap->lastfile = NULL;
	for (i=0; i<DA_CUR (aap->images); i++) {
		atmelavr_image_t *aai = DA_NTHITEM (aap->images, i);

		if (aai) {
			atmelavr_freeatmelavrimage (aai);
		}
	}
	dynarray_trash (aap->images);

	sfree (aap);
	return;
}
/*}}}*/
/*{{{  static void atmelavr_setconstpropstate (atmelavr_image_t *img, tnode_t *instr, int offset)*/
/*
 *	creates and initialises the 'constpropstate'
 */
static void atmelavr_setconstpropstate (atmelavr_image_t *img, tnode_t *instr, int offset)
{
	if (atmelavr_constpropstate) {
		nocc_internal ("atmelavr_setconstpropstate(): already here..");
		return;
	}
	atmelavr_constpropstate = (aavr_constpropstate_t *)smalloc (sizeof (aavr_constpropstate_t));
	atmelavr_constpropstate->img = img;
	atmelavr_constpropstate->instr = instr;
	atmelavr_constpropstate->offset = offset;
	atmelavr_constpropstate->didfix = 0;
	return;
}
/*}}}*/
/*{{{  static void atmelavr_clearconstpropstate (void)*/
/*
 *	trashes the 'constpropstate'
 */
static int atmelavr_clearconstpropstate (void)
{
	int r;

	if (!atmelavr_constpropstate) {
		nocc_internal ("atmelavr_clearconstpropstate(): no state..");
		return 0;
	}

	atmelavr_constpropstate->img = NULL;
	atmelavr_constpropstate->instr = NULL;
	atmelavr_constpropstate->offset = -1;
	r = atmelavr_constpropstate->didfix;
	atmelavr_constpropstate->didfix = 0;

	sfree (atmelavr_constpropstate);
	atmelavr_constpropstate = NULL;
	return r;
}
/*}}}*/


/*{{{  static aavr_labelfixup_t *atmelavr_newaavrlabelfixup (void)*/
/*
 *	creates a new aavr_labelfixup_t structure
 */
static aavr_labelfixup_t *atmelavr_newaavrlabelfixup (void)
{
	aavr_labelfixup_t *aalf = (aavr_labelfixup_t *)smalloc (sizeof (aavr_labelfixup_t));

	aalf->img = NULL;
	aalf->instr = NULL;
	aalf->offset = -1;

	return aalf;
}
/*}}}*/
/*{{{  static void atmelavr_freeaavrlabelfixup (aavr_labelfixup_t *aalf)*/
/*
 *	frees an aavr_labelfixup_t structure
 */
static void atmelavr_freeaavrlabelfixup (aavr_labelfixup_t *aalf)
{
	if (!aalf) {
		nocc_serious ("atmelavr_freeaavrlabelfixup(): NULL pointer!");
		return;
	}
	aalf->img = NULL;
	aalf->instr = NULL;
	sfree (aalf);
	return;
}
/*}}}*/
/*{{{  static aavr_labelinfo_t *atmelavr_newaavrlabelinfo (void)*/
/*
 *	creates a new aavr_labelinfo_t structure
 */
static aavr_labelinfo_t *atmelavr_newaavrlabelinfo (void)
{
	aavr_labelinfo_t *aali = (aavr_labelinfo_t *)smalloc (sizeof (aavr_labelinfo_t));

	aali->img = NULL;
	aali->baddr = -1;
	aali->labins = NULL;
	dynarray_init (aali->fixups);

	return aali;
}
/*}}}*/
/*{{{  static void atmelavr_freeaavrlabelinfo (aavr_labelinfo_t *aali)*/
/*
 *	frees an aavr_labelinfo_t structure
 */
static void atmelavr_freeaavrlabelinfo (aavr_labelinfo_t *aali)
{
	int i;

	if (!aali) {
		nocc_serious ("atmelavr_freeaavrlabelinfo(): NULL pointer!");
		return;
	}
	aali->img = NULL;
	aali->labins = NULL;
	for (i=0; i<DA_CUR (aali->fixups); i++) {
		aavr_labelfixup_t *aalf = DA_NTHITEM (aali->fixups, i);

		if (aalf) {
			atmelavr_freeaavrlabelfixup (aalf);
		}
	}
	dynarray_trash (aali->fixups);
	sfree (aali);
	return;
}
/*}}}*/

/*{{{  static void atmelavr_labelinfohook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)*/
/*
 *	dumps an aavr:labelinfo compiler hook
 */
static void atmelavr_labelinfohook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)
{
	aavr_labelinfo_t *aali = (aavr_labelinfo_t *)hook;
	int i;

	atmelavr_isetindent (stream, indent);
	fhandle_printf (stream, "<chook:aavr:labelinfo img=\"%p\" baddr=\"%d\" labins=\"%p\">\n", aali->img, aali->baddr, aali->labins);
	for (i=0; i<DA_CUR (aali->fixups); i++) {
		aavr_labelfixup_t *aalf = DA_NTHITEM (aali->fixups, i);

		atmelavr_isetindent (stream, indent + 1);
		fhandle_printf (stream, "<labelfixup instr=\"%s\" offset=\"%d\" />\n", aalf->instr->tag->name, aalf->offset);
	}
	atmelavr_isetindent (stream, indent);
	fhandle_printf (stream, "</chook:aavr:labelinfo>\n");

	return;
}
/*}}}*/
/*{{{  static void atmelavr_labelinfohook_free (void *hook)*/
/*
 *	frees an aavr:labelinfo compiler hook
 */
static void atmelavr_labelinfohook_free (void *hook)
{
	aavr_labelinfo_t *aali = (aavr_labelinfo_t *)hook;

	if (!aali) {
		return;
	}
	atmelavr_freeaavrlabelinfo (aali);
	return;
}
/*}}}*/

/*{{{  static int insarg_to_constreg (atmelavr_image_t *img, tnode_t *arg, const int min, const int max, codegen_t *cgen)*/
/*
 *	extracts a constant register number (within limits) from an instruction operand
 */
static int insarg_to_constreg (atmelavr_image_t *img, tnode_t *arg, const int min, const int max, codegen_t *cgen)
{
	int reg;

	if (!constprop_isconst (arg)) {
		codegen_node_error (cgen, arg, "expected constant register number, found [%s]", arg->tag->name);
		return min;
	}
	reg = constprop_intvalof (arg);

	if ((reg < min) || (reg > max)) {
		codegen_node_error (cgen, arg, "register %d out of range for instruction (expected [%d - %d])", reg, min, max);
		return min;
	}
	return reg;
}
/*}}}*/
/*{{{  static int insarg_to_constval (atmelavr_image_t *img, tnode_t *arg, tnode_t *instr, const int myoffs, const int min, const int max, codegen_t *cgen)*/
/*
 *	extracts a constant (immediate) value (within limits) from an instruction operand
 */
static int insarg_to_constval (atmelavr_image_t *img, tnode_t **argp, tnode_t *instr, const int myoffs, const int min, const int max, codegen_t *cgen)
{
	int val;
	tnode_t *arg = *argp;

	/* when we encounter labels used as constants, and expressions thereof, may find unresolveable things here -- be prepared to fixup! */
	if (!constprop_isconst (arg)) {
		int fixed;

		atmelavr_setconstpropstate (img, instr, myoffs);
		constprop_tree (argp);
		fixed = atmelavr_clearconstpropstate ();
		arg = *argp;

		/* if still not constant, and no fixups added, abandon */
		if (!constprop_isconst (arg)) {
			if (!fixed) {
				codegen_node_error (cgen, arg, "expected constant value, found [%s]", arg->tag->name);
			}
			/* else, okay, because we'll fixup later */
			return min;
		}
	}

	val = constprop_intvalof (arg);

	/* if range is unsigned, consider only unsigned version if -ve */
	if ((min == 0) && (max >= min) && (val < 0)) {
		/* Note: requires that 'max+1' is a power of 2.. */
		val = (val & max);
	}
	if ((val < min) || (val > max)) {
		codegen_node_error (cgen, arg, "value %d out of range for instruction (expected [%d - %d])", val, min, max);
		return min;
	}
	return val;
}
/*}}}*/
/*{{{  static int insarg_to_constaddrdiff (atmelavr_image_t *img, tnode_t *arg, tnode_t *instr, const int myoffs, const int ibytes, const int min, const int max, codegen_t *cgen)*/
/*
 *	extracts a label address difference (in words)
 */
static int insarg_to_constaddrdiff (atmelavr_image_t *img, tnode_t *arg, tnode_t *instr, const int myoffs, const int ibytes, const int min, const int max, codegen_t *cgen)
{
#if 0
fprintf (stderr, "insarg_to_constaddrdiff(): arg=[%s], myoffs=%d\n", arg->tag->name, myoffs);
#endif
	if ((arg->tag == avrasm.tag_GLABEL) || (arg->tag == avrasm.tag_LLABEL))  {
		name_t *labname = tnode_nthnameof (arg, 0);
		tnode_t *ndecl = NameDeclOf (labname);
		aavr_labelinfo_t *aali;

		if (tnode_haschook (ndecl, labelinfo_chook)) {
			aali = (aavr_labelinfo_t *)tnode_getchook (ndecl, labelinfo_chook);
		} else {
			/* create new label info */
			aali = atmelavr_newaavrlabelinfo ();
			tnode_setchook (ndecl, labelinfo_chook, aali);
		}
		if (aali->baddr >= 0) {
			/* got address for this label already */
			int wdiff = (aali->baddr - (myoffs + ibytes)) >> 1;

			if ((wdiff < min) || (wdiff > max)) {
				codegen_node_error (cgen, instr, "address difference too far (%d), range is [%d - %d]", wdiff, min, max);
				return 0;
			}

#if 0
fprintf (stderr, "insarg_to_constaddrdiff(): arg=[%s], myoffs=%d, aali->baddr=%d, wdiff=%d\n", arg->tag->name, myoffs, aali->baddr, wdiff);
#endif
			return wdiff;
		} else {
			/* add fixup */
			aavr_labelfixup_t *aalf = atmelavr_newaavrlabelfixup ();

			aalf->img = img;
			aalf->instr = instr;
			aalf->offset = myoffs;
			dynarray_add (aali->fixups, aalf);
			return 0;
		}
	}
	return 0;
}
/*}}}*/
/*{{{  static int insarg_to_constaddr (atmelavr_image_t *img, tnode_t *arg, tnode_t *instr, const int myoffs, const int min, const int max, codegen_t *cgen)*/
/*
 *	extracts a label address (in words)
 */
static int insarg_to_constaddr (atmelavr_image_t *img, tnode_t *arg, tnode_t *instr, const int myoffs, const int min, const int max, codegen_t *cgen)
{
	if ((arg->tag == avrasm.tag_GLABEL) || (arg->tag == avrasm.tag_LLABEL)) {
		name_t *labname = tnode_nthnameof (arg, 0);
		tnode_t *ndecl = NameDeclOf (labname);
		aavr_labelinfo_t *aali;

		if (tnode_haschook (ndecl, labelinfo_chook)) {
			aali = (aavr_labelinfo_t *)tnode_getchook (ndecl, labelinfo_chook);
		} else {
			/* create new label info */
			aali = atmelavr_newaavrlabelinfo ();
			tnode_setchook (ndecl, labelinfo_chook, aali);
		}
		if (aali->baddr >= 0) {
			/* got address for this label already */
			int addr = aali->baddr >> 1;

			if ((addr < min) || (addr > max)) {
				codegen_node_error (cgen, instr, "address (%d) out of range [%d - %d]", addr, min, max);
				return 0;
			}
			return addr;
		} else {
			/* add fixup */
			aavr_labelfixup_t *aalf = atmelavr_newaavrlabelfixup ();

			aalf->img = img;
			aalf->instr = instr;
			aalf->offset = myoffs;
			dynarray_add (aali->fixups, aalf);
			return 0;
		}
	}
	return 0;
}
/*}}}*/
/*{{{  static int atmelavr_assemble_const (atmelavr_image_t *img, int *offset, tnode_t *cinstr, codegen_t *cgen)*/
/*
 *	called to assemble a block of constant stuff at a specific place in the image
 *	returns number of bytes assembled, or that should be assembled
 */
static int atmelavr_assemble_const (atmelavr_image_t *img, int *offset, tnode_t *cinstr, codegen_t *cgen, avrtarget_t *target)
{
	int width = 0;
	int iw = 0;
	int offs = *offset;
	int i, nitems;
	tnode_t **items;

	if (cinstr->tag == avrasm.tag_CONST) {
		iw = 1;
	} else if (cinstr->tag == avrasm.tag_CONST16) {
		iw = 2;
	} else {
		nocc_internal ("atmelavr_assemble_const(): unknown constant type [%s]", cinstr->tag->name);
		return 0;
	}

	items = parser_getlistitems (tnode_nthsubof (cinstr, 0), &nitems);
	for (i=0; i<nitems; i++) {
		int val;

		/* everything must be constant by the time we get here, but labels may need fixups */
		if (!constprop_isconst (items[i])) {
			int fixed;

			atmelavr_setconstpropstate (img, cinstr, offs);
			constprop_tree (&(items[i]));
			fixed = atmelavr_clearconstpropstate ();

			/* if still not constant, and no fixups added, abandon */
			if (!constprop_isconst (items[i])) {
				if (!fixed) {
					codegen_node_error (cgen, items[i], "expected constant value, found [%s]", items[i]->tag->name);
					return 0;
				}
				val = 0;
			} else {
				val = constprop_intvalof (items[i]);
			}
		} else {
			val = constprop_intvalof (items[i]);
		}

		/* check value-in-range */
		if ((iw == 1) && ((val < -128) || (val > 256))) {
			codegen_node_error (cgen, items[i], "8-bit constant (%d) out of range", val);
			return 0;
		} else if ((iw == 2) && ((val < -32768) || (val > 65536))) {
			codegen_node_error (cgen, items[i], "16-bit constant (%d) out of range", val);
			return 0;
		}

		/* good here, assemble! */
		if (iw == 1) {
			img->image[offs++] = (unsigned char)(val & 0xff);
		} else if (iw == 2) {
			if (target->bswap_code && (img->zone->tag == avrasm.tag_TEXTSEG)) {
				/* byte-swap these */
				img->image[offs++] = (unsigned char)(val & 0xff);
				img->image[offs++] = (unsigned char)((val >> 8) & 0xff);
			} else {
				img->image[offs++] = (unsigned char)((val >> 8) & 0xff);
				img->image[offs++] = (unsigned char)(val & 0xff);
			}
		}
		width += iw;
	}

	/* extra check: if assembling into .text, must pad out to 16-bits */
	if (img->zone->tag == avrasm.tag_TEXTSEG) {
		if (width & 0x01) {
			codegen_node_warning (cgen, cinstr, "8-bit data in text segment being zero-appended to 16-bits");
			img->image[offs++] = 0x00;
			width++;
		}
	}

	*offset = offs;
	return width;
}
/*}}}*/
/*{{{  static int atmelavr_assemble_instr (atmelavr_image_t *img, int *offset, tnode_t *instr, avrinstr_tbl_t *inst, codegen_t *cgen)*/
/*
 *	called to assemble a single instruction at a specific place in the image
 *	returns number of bytes assembled, or that should be assembled
 */
static int atmelavr_assemble_instr (atmelavr_image_t *img, int *offset, tnode_t *instr, avrinstr_tbl_t *inst, codegen_t *cgen, avrtarget_t *target)
{
	int width = 0;
	int offs = *offset;
	int rd, rr, rs;
	int val, val2, prepost;

	switch (inst->ins) {
	case INS_ADC: /*{{{  add with carry*/
		rd = insarg_to_constreg (img, tnode_nthsubof (instr, 1), 0, 31, cgen);
		rr = insarg_to_constreg (img, tnode_nthsubof (instr, 2), 0, 31, cgen);
		img->image[offs++] = 0x1c | ((rr >> 3) & 0x02) | ((rd >> 4) & 0x01);
		img->image[offs++] = ((rd & 0x0f) << 4) | (rr & 0x0f);
		width = 2;
		break;
		/*}}}*/
	case INS_ADD: /*{{{  add without carry*/
		rd = insarg_to_constreg (img, tnode_nthsubof (instr, 1), 0, 31, cgen);	
		rr = insarg_to_constreg (img, tnode_nthsubof (instr, 2), 0, 31, cgen);
		img->image[offs++] = 0x0c | ((rr >> 3) & 0x02) | ((rd >> 4) & 0x01);
		img->image[offs++] = ((rd & 0x0f) << 4) | (rr & 0x0f);
		width = 2;
		break;
		/*}}}*/
	case INS_ADIW: /*{{{  add immediate to word*/
		if (tnode_nthsubof (instr, 1)->tag == avrasm.tag_XYZREG) {
			/* directly specified X,Y,Z (==r26,r28,r30) */
			avrasm_getxyzreginfo (tnode_nthsubof (instr, 1), &rd, &prepost, &val2);

			if (val2 || prepost) {
				codegen_node_error (cgen, instr, "invalid modifier for X,Y,Z registers for \"adiw\"");
				rd = 0;
			} else {
				rd = 26 + (rd * 2);	/* X,Y,Z -> 26,28,30 */
			}
		} else {
			rd = insarg_to_constreg (img, tnode_nthsubof (instr, 1), 24, 30, cgen);
		}
		val = insarg_to_constval (img, tnode_nthsubaddr (instr, 2), instr, offs, 0, 63, cgen);
		if (rd & 0x01) {
			/* cannot have odd-numbered register here, must be r25:r24, ..., r31:r30 */
			codegen_node_error (cgen, instr, "invalid register %d for \"adiw\" (24,26,28,30)", rd);
			rd = 0;
		}
		rd = (rd - 24) / 2;		/* -> [0..3] */
		img->image[offs++] = 0x96;
		img->image[offs++] = ((val & 0x30) << 2) | (rd << 4) | (val & 0x0f);
		width = 2;
		break;
		/*}}}*/
	case INS_AND: /*{{{  logical-and*/
		rd = insarg_to_constreg (img, tnode_nthsubof (instr, 1), 0, 31, cgen);	
		rr = insarg_to_constreg (img, tnode_nthsubof (instr, 2), 0, 31, cgen);
		img->image[offs++] = 0x20 | ((rr >> 3) & 0x02) | ((rd >> 4) & 0x01);
		img->image[offs++] = ((rd & 0x0f) << 4) | (rr & 0x0f);
		width = 2;
		break;
		/*}}}*/
	case INS_ANDI: /*{{{  and with immediate*/
		rd = insarg_to_constreg (img, tnode_nthsubof (instr, 1), 16, 31, cgen);	
		val = insarg_to_constval (img, tnode_nthsubaddr (instr, 2), instr, offs, 0, 255, cgen);
		img->image[offs++] = 0x70 | ((val >> 4) & 0x0f);
		img->image[offs++] = ((rd & 0x0f) << 4) | (val & 0x0f);
		width = 2;
		break;
		/*}}}*/
	case INS_ASR: /*{{{  arithmetic shift right*/
		rd = insarg_to_constreg (img, tnode_nthsubof (instr, 1), 0, 31, cgen);	
		img->image[offs++] = 0x94 | ((rd >> 4) & 0x01);
		img->image[offs++] = ((rd & 0x0f) << 4) | 0x05;
		width = 2;
		break;
		/*}}}*/
	case INS_BCLR: /*{{{  bit clear in SREG*/
		val = insarg_to_constval (img, tnode_nthsubaddr (instr, 1), instr, offs, 0, 7, cgen);
		img->image[offs++] = 0x94;
		img->image[offs++] = 0x88 | (val << 4);
		width = 2;
		break;
		/*}}}*/
	case INS_BLD: /*{{{  bit load from T to register*/
		rd = insarg_to_constreg (img, tnode_nthsubof (instr, 1), 0, 31, cgen);
		val = insarg_to_constval (img, tnode_nthsubaddr (instr, 2), instr, offs, 0, 7, cgen);
		img->image[offs++] = 0xf8 | ((rd >> 4) & 0x01);
		img->image[offs++] = ((rd & 0x0f) << 4) | (val & 0x07);
		width = 2;
		break;
		/*}}}*/
	case INS_BRBC: /*{{{  branch if SREG bit clear*/
		val = insarg_to_constval (img, tnode_nthsubaddr (instr, 1), instr, offs, 0, 7, cgen);
		val2 = insarg_to_constaddrdiff (img, tnode_nthsubof (instr, 2), instr, offs, 2, -64, 63, cgen);
		img->image[offs++] = 0xf4 | ((val2 >> 5) & 0x03);
		img->image[offs++] = ((val2 << 3) & 0xf8) | (val & 0x07);
		width = 2;
		break;
		/*}}}*/
	case INS_BRBS: /*{{{  branch if SREG bit set*/
		val = insarg_to_constval (img, tnode_nthsubaddr (instr, 1), instr, offs, 0, 7, cgen);
		val2 = insarg_to_constaddrdiff (img, tnode_nthsubof (instr, 2), instr, offs, 2, -64, 63, cgen);
		img->image[offs++] = 0xf0 | ((val2 >> 5) & 0x03);
		img->image[offs++] = ((val2 << 3) & 0xf8) | (val & 0x07);
		width = 2;
		break;
		/*}}}*/
	case INS_BRCC: /*{{{  branch if carry clear*/
		val = insarg_to_constaddrdiff (img, tnode_nthsubof (instr, 1), instr, offs, 2, -64, 63, cgen);
		img->image[offs++] = 0xf4 | ((val >> 5) & 0x03);
		img->image[offs++] = ((val << 3) & 0xf8);
		width = 2;
		break;
		/*}}}*/
	case INS_BRCS: /*{{{  branch if carry set*/
		val = insarg_to_constaddrdiff (img, tnode_nthsubof (instr, 1), instr, offs, 2, -64, 63, cgen);
		img->image[offs++] = 0xf0 | ((val >> 5) & 0x03);
		img->image[offs++] = ((val << 3) & 0xf8);
		width = 2;
		break;
		/*}}}*/
	case INS_BREAK: /*{{{  break [on supported devices only!]*/
		img->image[offs++] = 0x95;
		img->image[offs++] = 0x98;
		width = 2;
		break;
		/*}}}*/
	case INS_BREQ: /*{{{  branch if equal*/
		val = insarg_to_constaddrdiff (img, tnode_nthsubof (instr, 1), instr, offs, 2, -64, 63, cgen);
		img->image[offs++] = 0xf0 | ((val >> 5) & 0x03);
		img->image[offs++] = ((val << 3) & 0xf8) | 0x01;
		width = 2;
		break;
		/*}}}*/
	case INS_BRGE: /*{{{  branch if greater or equal (signed)*/
		val = insarg_to_constaddrdiff (img, tnode_nthsubof (instr, 1), instr, offs, 2, -64, 63, cgen);
		img->image[offs++] = 0xf4 | ((val >> 5) & 0x03);
		img->image[offs++] = ((val << 3) & 0xf8) | 0x04;
		width = 2;
		break;
		/*}}}*/
	case INS_BRHC: /*{{{  branch if half-carry clear*/
		val = insarg_to_constaddrdiff (img, tnode_nthsubof (instr, 1), instr, offs, 2, -64, 63, cgen);
		img->image[offs++] = 0xf4 | ((val >> 5) & 0x03);
		img->image[offs++] = ((val << 3) & 0xf8) | 0x05;
		width = 2;
		break;
		/*}}}*/
	case INS_BRHS: /*{{{  branch if half-carry set*/
		val = insarg_to_constaddrdiff (img, tnode_nthsubof (instr, 1), instr, offs, 2, -64, 63, cgen);
		img->image[offs++] = 0xf0 | ((val >> 5) & 0x03);
		img->image[offs++] = ((val << 3) & 0xf8) | 0x05;
		width = 2;
		break;
		/*}}}*/
	case INS_BRID: /*{{{  branch if interrupts disabled*/
		val = insarg_to_constaddrdiff (img, tnode_nthsubof (instr, 1), instr, offs, 2, -64, 63, cgen);
		img->image[offs++] = 0xf4 | ((val >> 5) & 0x03);
		img->image[offs++] = ((val << 3) & 0xf8) | 0x07;
		width = 2;
		break;
		/*}}}*/
	case INS_BRIE: /*{{{  branch if interrupts enabled*/
		val = insarg_to_constaddrdiff (img, tnode_nthsubof (instr, 1), instr, offs, 2, -64, 63, cgen);
		img->image[offs++] = 0xf0 | ((val >> 5) & 0x03);
		img->image[offs++] = ((val << 3) & 0xf8) | 0x07;
		width = 2;
		break;
		/*}}}*/
	case INS_BRLO: /*{{{  branch if lower (unsigned)*/
		val = insarg_to_constaddrdiff (img, tnode_nthsubof (instr, 1), instr, offs, 2, -64, 63, cgen);
		img->image[offs++] = 0xf0 | ((val >> 5) & 0x03);
		img->image[offs++] = ((val << 3) & 0xf8);
		width = 2;
		break;
		/*}}}*/
	case INS_BRLT: /*{{{  branch if less-than (signed)*/
		val = insarg_to_constaddrdiff (img, tnode_nthsubof (instr, 1), instr, offs, 2, -64, 63, cgen);
		img->image[offs++] = 0xf0 | ((val >> 5) & 0x03);
		img->image[offs++] = ((val << 3) & 0xf8) | 0x04;
		width = 2;
		break;
		/*}}}*/
	case INS_BRMI: /*{{{  branch if minus (negative flag set)*/
		val = insarg_to_constaddrdiff (img, tnode_nthsubof (instr, 1), instr, offs, 2, -64, 63, cgen);
		img->image[offs++] = 0xf0 | ((val >> 5) & 0x03);
		img->image[offs++] = ((val << 3) & 0xf8) | 0x02;
		width = 2;
		break;
		/*}}}*/
	case INS_BRNE: /*{{{  branch if not-equal*/
		val = insarg_to_constaddrdiff (img, tnode_nthsubof (instr, 1), instr, offs, 2, -64, 63, cgen);
		img->image[offs++] = 0xf4 | ((val >> 5) & 0x03);
		img->image[offs++] = ((val << 3) & 0xf8) | 0x01;
		width = 2;
		break;
		/*}}}*/
	case INS_BRPL: /*{{{  branch if plus (negative flag not set)*/
		val = insarg_to_constaddrdiff (img, tnode_nthsubof (instr, 1), instr, offs, 2, -64, 63, cgen);
		img->image[offs++] = 0xf4 | ((val >> 5) & 0x03);
		img->image[offs++] = ((val << 3) & 0xf8) | 0x02;
		width = 2;
		break;
		/*}}}*/
	case INS_BRSH: /*{{{  branch if same or higher (unsigned)*/
		val = insarg_to_constaddrdiff (img, tnode_nthsubof (instr, 1), instr, offs, 2, -64, 63, cgen);
		img->image[offs++] = 0xf4 | ((val >> 5) & 0x03);
		img->image[offs++] = ((val << 3) & 0xf8);
		width = 2;
		break;
		/*}}}*/
	case INS_BRTC: /*{{{  branch if T flag clear*/
		val = insarg_to_constaddrdiff (img, tnode_nthsubof (instr, 1), instr, offs, 2, -64, 63, cgen);
		img->image[offs++] = 0xf4 | ((val >> 5) & 0x03);
		img->image[offs++] = ((val << 3) & 0xf8) | 0x06;
		width = 2;
		break;
		/*}}}*/
	case INS_BRTS: /*{{{  branch if T flag set*/
		val = insarg_to_constaddrdiff (img, tnode_nthsubof (instr, 1), instr, offs, 2, -64, 63, cgen);
		img->image[offs++] = 0xf0 | ((val >> 5) & 0x03);
		img->image[offs++] = ((val << 3) & 0xf8) | 0x06;
		width = 2;
		break;
		/*}}}*/
	case INS_BRVC: /*{{{  branch if overflow clear*/
		val = insarg_to_constaddrdiff (img, tnode_nthsubof (instr, 1), instr, offs, 2, -64, 63, cgen);
		img->image[offs++] = 0xf4 | ((val >> 5) & 0x03);
		img->image[offs++] = ((val << 3) & 0xf8) | 0x03;
		width = 2;
		break;
		/*}}}*/
	case INS_BRVS: /*{{{  branch if overflow set*/
		val = insarg_to_constaddrdiff (img, tnode_nthsubof (instr, 1), instr, offs, 2, -64, 63, cgen);
		img->image[offs++] = 0xf0 | ((val >> 5) & 0x03);
		img->image[offs++] = ((val << 3) & 0xf8) | 0x03;
		width = 2;
		break;
		/*}}}*/
	case INS_BSET: /*{{{  set bit in SREG*/
		val = insarg_to_constval (img, tnode_nthsubaddr (instr, 1), instr, offs, 0, 7, cgen);
		img->image[offs++] = 0x94;
		img->image[offs++] = ((val << 4) & 0x70) | 0x08;
		width = 2;
		break;
		/*}}}*/
	case INS_BST: /*{{{  bit store into T flag*/
		rd = insarg_to_constreg (img, tnode_nthsubof (instr, 1), 0, 31, cgen);
		val = insarg_to_constval (img, tnode_nthsubaddr (instr, 2), instr, offs, 0, 7, cgen);
		img->image[offs++] = 0xfa | ((rd >> 4) & 0x01);
		img->image[offs++] = ((rd << 4) & 0xf0) | (val & 0x07);
		width = 2;
		break;
		/*}}}*/
	case INS_CALL: /*{{{  call subroutine (immediate address)*/
		val = insarg_to_constaddr (img, tnode_nthsubof (instr, 1), instr, offs, 0, (1 << 22) - 1, cgen);
		img->image[offs++] = 0x94 | ((val >> 21) & 0x01);
		img->image[offs++] = ((val >> 13) & 0xf0) | 0x0e | ((val >> 16) & 0x01);
		img->image[offs++] = (val >> 8) & 0xff;
		img->image[offs++] = val & 0xff;
		width = 4;
		break;
		/*}}}*/
	case INS_CBI: /*{{{  clear bit in I/O register*/
		val = insarg_to_constval (img, tnode_nthsubaddr (instr, 1), instr, offs, 0, 31, cgen);
		val2 = insarg_to_constval (img, tnode_nthsubaddr (instr, 2), instr, offs, 0, 7, cgen);
		img->image[offs++] = 0x98;
		img->image[offs++] = ((val & 0x1f) << 3) | (val2 & 0x07);
		width = 2;
		break;
		/*}}}*/
	case INS_CBR: /*{{{  clear bits in register*/
		rd = insarg_to_constreg (img, tnode_nthsubof (instr, 1), 16, 31, cgen);
		val = insarg_to_constval (img, tnode_nthsubaddr (instr, 2), instr, offs, 0, 255, cgen);
		img->image[offs++] = 0x70 | ((~val >> 4) & 0x0f);
		img->image[offs++] = ((rd << 4) & 0xf0) | (~val & 0x0f);
		width = 2;
		break;
		/*}}}*/
	case INS_CLC: /*{{{  clear carry*/
		img->image[offs++] = 0x94;
		img->image[offs++] = 0x88;
		width = 2;
		break;
		/*}}}*/
	case INS_CLH: /*{{{  clear half-carry*/
		img->image[offs++] = 0x94;
		img->image[offs++] = 0xd8;
		width = 2;
		break;
		/*}}}*/
	case INS_CLI: /*{{{  clear interrupt (interrupt disable)*/
		img->image[offs++] = 0x94;
		img->image[offs++] = 0xf8;
		width = 2;
		break;
		/*}}}*/
	case INS_CLN: /*{{{  clear negative flag*/
		img->image[offs++] = 0x94;
		img->image[offs++] = 0xa8;
		width = 2;
		break;
		/*}}}*/
	case INS_CLR: /*{{{  clear register*/
		rd = insarg_to_constreg (img, tnode_nthsubof (instr, 1), 0, 31, cgen);
		img->image[offs++] = 0x24 | ((rd >> 3) & 0x02) | ((rd >> 4) & 0x01);
		img->image[offs++] = ((rd & 0x0f) << 4) | (rd & 0x0f);
		width = 2;
		break;
		/*}}}*/
	case INS_CLS: /*{{{  clear signed flag*/
		img->image[offs++] = 0x94;
		img->image[offs++] = 0xc8;
		width = 2;
		break;
		/*}}}*/
	case INS_CLT: /*{{{  clear T flag*/
		img->image[offs++] = 0x94;
		img->image[offs++] = 0xe8;
		width = 2;
		break;
		/*}}}*/
	case INS_CLV: /*{{{  clear overflow flag*/
		img->image[offs++] = 0x94;
		img->image[offs++] = 0xb8;
		width = 2;
		break;
		/*}}}*/
	case INS_CLZ: /*{{{  clear zero flag*/
		img->image[offs++] = 0x94;
		img->image[offs++] = 0x98;
		width = 2;
		break;
		/*}}}*/
	case INS_COM: /*{{{  one's complement*/
		rd = insarg_to_constreg (img, tnode_nthsubof (instr, 1), 0, 31, cgen);
		img->image[offs++] = 0x94 | ((rd >> 4) & 0x01);
		img->image[offs++] = ((rd << 4) & 0xf0);
		width = 2;
		break;
		/*}}}*/
	case INS_CP: /*{{{  compare*/
		rd = insarg_to_constreg (img, tnode_nthsubof (instr, 1), 0, 31, cgen);
		rr = insarg_to_constreg (img, tnode_nthsubof (instr, 2), 0, 31, cgen);
		img->image[offs++] = 0x14 | ((rr >> 3) & 0x02) | ((rd >> 4) & 0x01);
		img->image[offs++] = ((rd << 4) & 0xf0) | (rr & 0x0f);
		width = 2;
		break;
		/*}}}*/
	case INS_CPC: /*{{{  compare with carry*/
		rd = insarg_to_constreg (img, tnode_nthsubof (instr, 1), 0, 31, cgen);
		rr = insarg_to_constreg (img, tnode_nthsubof (instr, 2), 0, 31, cgen);
		img->image[offs++] = 0x04 | ((rr >> 3) & 0x02) | ((rd >> 4) & 0x01);
		img->image[offs++] = ((rd << 4) & 0xf0) | (rr & 0x0f);
		width = 2;
		break;
		/*}}}*/
	case INS_CPI: /*{{{  compare with immediate*/
		rd = insarg_to_constreg (img, tnode_nthsubof (instr, 1), 16, 31, cgen);
		val = insarg_to_constval (img, tnode_nthsubaddr (instr, 2), instr, offs, 0, 255, cgen);
		img->image[offs++] = 0x30 | ((val >> 4) & 0x0f);
		img->image[offs++] = ((rd << 4) & 0xf0) | (val & 0x0f);
		width = 2;
		break;
		/*}}}*/
	case INS_CPSE: /*{{{  compare and skip if equal*/
		rd = insarg_to_constreg (img, tnode_nthsubof (instr, 1), 0, 31, cgen);
		rr = insarg_to_constreg (img, tnode_nthsubof (instr, 2), 0, 31, cgen);
		img->image[offs++] = 0x10 | ((rr >> 3) & 0x02) | ((rd >> 4) & 0x01);
		img->image[offs++] = ((rd << 4) & 0xf0) | (rr & 0x0f);
		width = 2;
		break;
		/*}}}*/
	case INS_DEC: /*{{{  decrement*/
		rd = insarg_to_constreg (img, tnode_nthsubof (instr, 1), 0, 31, cgen);
		img->image[offs++] = 0x94 | ((rd >> 4) & 0x01);
		img->image[offs++] = ((rd & 0x0f) << 4) | 0x0a;
		width = 2;
		break;
		/*}}}*/
	case INS_EICALL: /*{{{  extended indirect call (to EIND:Z)*/
		img->image[offs++] = 0x95;
		img->image[offs++] = 0x19;
		width = 2;
		break;
		/*}}}*/
	case INS_EIJMP: /*{{{  extended indirect jump (to EIND:Z)*/
		img->image[offs++] = 0x94;
		img->image[offs++] = 0x19;
		width = 2;
		break;
		/*}}}*/
	case INS_ELPM: /*{{{  extended load program memory*/
		rd = insarg_to_constreg (img, tnode_nthsubof (instr, 1), 0, 31, cgen);
		avrasm_getxyzreginfo (tnode_nthsubof (instr, 2), &rr, &prepost, &val);

		/* only two encodings for this */
		if (rr != 2) {
			codegen_node_error (cgen, instr, "can only use Z with ELPM");
			return 0;
		}
		if (prepost < 0) {
			codegen_node_error (cgen, instr, "cannot use predecrement with ELPM");
			return 0;
		}
		if (val) {
			codegen_node_error (cgen, instr, "cannot use displacement with ELPM");
			return 0;
		}
		img->image[offs++] = 0x90 | ((rd >> 4) & 0x01);
		img->image[offs++] = ((rd << 4) & 0xf0) | (prepost ? 0x07 : 0x06);
		width = 2;
		break;
		/*}}}*/
	case INS_EOR: /*{{{  exclusive-or*/
		rd = insarg_to_constreg (img, tnode_nthsubof (instr, 1), 0, 31, cgen);	
		rr = insarg_to_constreg (img, tnode_nthsubof (instr, 2), 0, 31, cgen);	
		img->image[offs++] = 0x24 | ((rr >> 3) & 0x02) | ((rd >> 4) & 0x01);
		img->image[offs++] = ((rd << 4) & 0xf0) | (rr & 0x0f);
		width = 2;
		break;
		/*}}}*/
#if 0
	case INS_ESPM: /*{{{  extended store program memory [not available?]*/
		img->image[offs++] = 0x95;
		img->image[offs++] = 0xf8;
		width = 2;
		break;
		/*}}}*/
#endif
	case INS_FMUL: /*{{{  fractional multiply unsigned*/
		rd = insarg_to_constreg (img, tnode_nthsubof (instr, 1), 16, 23, cgen);	
		rr = insarg_to_constreg (img, tnode_nthsubof (instr, 2), 16, 23, cgen);	
		img->image[offs++] = 0x03;
		img->image[offs++] = 0x08 | ((rd << 4) & 0x70) | (rr & 0x07);
		width = 2;
		break;
		/*}}}*/
	case INS_FMULS: /*{{{  fractional multiply signed*/
		rd = insarg_to_constreg (img, tnode_nthsubof (instr, 1), 16, 23, cgen);	
		rr = insarg_to_constreg (img, tnode_nthsubof (instr, 2), 16, 23, cgen);	
		img->image[offs++] = 0x03;
		img->image[offs++] = 0x80 | ((rd << 4) & 0x70) | (rr & 0x07);
		width = 2;
		break;
		/*}}}*/
	case INS_FMULSU: /*{{{  fractional multiply signed with unsigned*/
		rd = insarg_to_constreg (img, tnode_nthsubof (instr, 1), 16, 23, cgen);	
		rr = insarg_to_constreg (img, tnode_nthsubof (instr, 2), 16, 23, cgen);	
		img->image[offs++] = 0x03;
		img->image[offs++] = 0x88 | ((rd << 4) & 0x70) | (rr & 0x07);
		width = 2;
		break;
		/*}}}*/
	case INS_ICALL: /*{{{  indirect call (to Z)*/
		img->image[offs++] = 0x95;
		img->image[offs++] = 0x09;
		width = 2;
		break;
		/*}}}*/
	case INS_IJMP: /*{{{  indirect jump (to Z)*/
		img->image[offs++] = 0x94;
		img->image[offs++] = 0x09;
		width = 2;
		break;
		/*}}}*/
	case INS_IN: /*{{{  input from I/O port*/
		rd = insarg_to_constreg (img, tnode_nthsubof (instr, 1), 0, 31, cgen);
		val = insarg_to_constval (img, tnode_nthsubaddr (instr, 2), instr, offs, 0, 63, cgen);
		img->image[offs++] = 0xb0 | ((val >> 3) & 0x06) | ((rd >> 4) & 0x01);
		img->image[offs++] = ((rd << 4) & 0xf0) | (val & 0x0f);
		width = 2;
		break;
		/*}}}*/
	case INS_INC: /*{{{  increment register*/
		rd = insarg_to_constreg (img, tnode_nthsubof (instr, 1), 0, 31, cgen);
		img->image[offs++] = 0x94 | ((rd >> 4) & 0x01);
		img->image[offs++] = ((rd << 4) & 0xf0) | 0x03;
		width = 2;
		break;
		/*}}}*/
	case INS_JMP: /*{{{  unconditional jump*/
		val = insarg_to_constaddr (img, tnode_nthsubof (instr, 1), instr, offs, 0, (1 << 22) - 1, cgen);
		img->image[offs++] = 0x94 | ((val >> 21) & 0x01);
		img->image[offs++] = ((val >> 13) & 0xf0) | 0x0c | ((val >> 16) & 0x01);
		img->image[offs++] = (val >> 8) & 0xff;
		img->image[offs++] = val & 0xff;
		width = 4;
		break;
		/*}}}*/
	case INS_LD: /*{{{  load data (and with displacement)*/
	case INS_LDD:
		rd = insarg_to_constreg (img, tnode_nthsubof (instr, 1), 0, 31, cgen);
		avrasm_getxyzreginfo (tnode_nthsubof (instr, 2), &rr, &prepost, &val);

		/* various encodings for this */
		switch (rr) {
		case 0:		/* X */
			if (val) {
				codegen_node_error (cgen, instr, "cannot use offset with X register");
				return 0;
			}
			if (prepost > 0) {
				img->image[offs++] = 0x90 | ((rd >> 4) & 0x01);
				img->image[offs++] = ((rd << 4) & 0xf0) | 0x0d;
			} else if (prepost < 0) {
				img->image[offs++] = 0x90 | ((rd >> 4) & 0x01);
				img->image[offs++] = ((rd << 4) & 0xf0) | 0x0e;
			} else {
				img->image[offs++] = 0x90 | ((rd >> 4) & 0x01);
				img->image[offs++] = ((rd << 4) & 0xf0) | 0x0c;
			}
			break;
		case 1:		/* Y */
			if (val) {
				img->image[offs++] = 0x80 | (val & 0x20) | ((val >> 1) & 0x0c) | ((rd >> 4) & 0x01);
				img->image[offs++] = ((rd << 4) & 0xf0) | 0x08 | (val & 0x07);
			} else if (prepost > 0) {
				img->image[offs++] = 0x90 | ((rd >> 4) & 0x01);
				img->image[offs++] = ((rd << 4) & 0xf0) | 0x09;
			} else if (prepost < 0) {
				img->image[offs++] = 0x90 | ((rd >> 4) & 0x01);
				img->image[offs++] = ((rd << 4) & 0xf0) | 0x0a;
			} else {
				img->image[offs++] = 0x80 | ((rd >> 4) & 0x01);
				img->image[offs++] = ((rd << 4) & 0xf0) | 0x08;
			}
			break;
		case 2:		/* Z */
			if (val) {
				img->image[offs++] = 0x80 | (val & 0x20) | ((val >> 1) & 0x0c) | ((rd >> 4) & 0x01);
				img->image[offs++] = ((rd << 4) & 0xf0) | (val & 0x07);
			} else if (prepost > 0) {
				img->image[offs++] = 0x90 | ((rd >> 4) & 0x01);
				img->image[offs++] = ((rd << 4) & 0xf0) | 0x01;
			} else if (prepost < 0) {
				img->image[offs++] = 0x90 | ((rd >> 4) & 0x01);
				img->image[offs++] = ((rd << 4) & 0xf0) | 0x02;
			} else {
				img->image[offs++] = 0x80 | ((rd >> 4) & 0x01);
				img->image[offs++] = ((rd << 4) & 0xf0);
			}
			break;
		}
		width = 2;
		break;
		/*}}}*/
	case INS_LDI: /*{{{  load immediate*/
		rd = insarg_to_constreg (img, tnode_nthsubof (instr, 1), 16, 31, cgen);	
		val = insarg_to_constval (img, tnode_nthsubaddr (instr, 2), instr, offs, 0, 255, cgen);
		img->image[offs++] = 0xe0 | ((val >> 4) & 0x0f);
		img->image[offs++] = ((rd & 0x0f) << 4) | (val & 0x0f);
		width = 2;
		break;
		/*}}}*/
	case INS_LDS: /*{{{  load direct (from immediate address)*/
		rd = insarg_to_constreg (img, tnode_nthsubof (instr, 1), 0, 31, cgen);
		// val = insarg_to_constaddr (img, tnode_nthsubof (instr, 2), instr, offs, 0, (1 << 16) - 1, cgen);
		val = insarg_to_constval (img, tnode_nthsubaddr (instr, 2), instr, offs, 0, (1 << 16) - 1, cgen);
		img->image[offs++] = 0x90 | ((rd >> 4) & 0x01);
		img->image[offs++] = ((rd << 4) & 0xf0);
		img->image[offs++] = (val >> 8) & 0xff;
		img->image[offs++] = val & 0xff;
		width = 4;
		break;
		/*}}}*/
	case INS_LPM: /*{{{  load program memory*/
		rd = insarg_to_constreg (img, tnode_nthsubof (instr, 1), 0, 31, cgen);
		avrasm_getxyzreginfo (tnode_nthsubof (instr, 2), &rr, &prepost, &val);

		/* only two encodings for this */
		if (rr != 2) {
			codegen_node_error (cgen, instr, "can only use Z with LPM");
			return 0;
		}
		if (prepost < 0) {
			codegen_node_error (cgen, instr, "cannot use predecrement with LPM");
			return 0;
		}
		if (val) {
			codegen_node_error (cgen, instr, "cannot use displacement with LPM");
			return 0;
		}
		img->image[offs++] = 0x90 | ((rd >> 4) & 0x01);
		img->image[offs++] = ((rd << 4) & 0xf0) | (prepost ? 0x05 : 0x04);
		width = 2;
		break;
		/*}}}*/
	case INS_LSL: /*{{{  logical shift left*/
		rd = insarg_to_constreg (img, tnode_nthsubof (instr, 1), 0, 31, cgen);
		img->image[offs++] = 0x0c | ((rd >> 3) & 0x02) | ((rd >> 4) & 0x01);
		img->image[offs++] = ((rd << 4) & 0xf0) | (rd & 0x0f);
		width = 2;
		break;
		/*}}}*/
	case INS_LSR: /*{{{  logical shift right*/
		rd = insarg_to_constreg (img, tnode_nthsubof (instr, 1), 0, 31, cgen);
		img->image[offs++] = 0x94 | ((rd >> 4) & 0x01);
		img->image[offs++] = ((rd << 4) & 0xf0) | 0x06;
		width = 2;
		break;
		/*}}}*/
	case INS_MOV: /*{{{  move*/
		rd = insarg_to_constreg (img, tnode_nthsubof (instr, 1), 0, 31, cgen);	
		rr = insarg_to_constreg (img, tnode_nthsubof (instr, 2), 0, 31, cgen);	
		img->image[offs++] = 0x2c | ((rr >> 3) & 0x02) | ((rd >> 4) & 0x01);
		img->image[offs++] = ((rd << 4) & 0xf0) | (rr & 0x0f);
		width = 2;
		break;
		/*}}}*/
	case INS_MOVW: /*{{{  move word*/
		rd = insarg_to_constreg (img, tnode_nthsubof (instr, 1), 0, 31, cgen);	
		rr = insarg_to_constreg (img, tnode_nthsubof (instr, 2), 0, 31, cgen);	
		if ((rd & 0x01) || (rr & 0x01)) {
			codegen_node_error (cgen, instr, "arguments to MOVW must be even register numbers only (got %d,%d)", rd, rr);
			return 0;
		}
		rd >>= 1;
		rr >>= 1;
		img->image[offs++] = 0x01;
		img->image[offs++] = ((rd << 4) & 0xf0) | (rr & 0x0f);
		width = 2;
		break;
		/*}}}*/
	case INS_MUL: /*{{{  multiply (unsigned) -- result dumped in r1:r0*/
		rd = insarg_to_constreg (img, tnode_nthsubof (instr, 1), 0, 31, cgen);	
		rr = insarg_to_constreg (img, tnode_nthsubof (instr, 2), 0, 31, cgen);	
		img->image[offs++] = 0x9c | ((rr >> 3) & 0x02) | ((rd >> 4) & 0x01);
		img->image[offs++] = ((rd << 4) & 0xf0) | (rr & 0x0f);
		width = 2;
		break;
		/*}}}*/
	case INS_MULS: /*{{{  multiply (signed) -- result dumped in r1:r0*/
		rd = insarg_to_constreg (img, tnode_nthsubof (instr, 1), 16, 31, cgen);
		rr = insarg_to_constreg (img, tnode_nthsubof (instr, 2), 16, 31, cgen);
		img->image[offs++] = 0x02;
		img->image[offs++] = ((rd << 4) & 0xf0) | (rr & 0x0f);
		width = 2;
		break;
		/*}}}*/
	case INS_MULSU: /*{{{  multiply (signed [rd] with unsigned [rr]) -- result dumped in r1:r0*/
		rd = insarg_to_constreg (img, tnode_nthsubof (instr, 1), 16, 23, cgen);
		rr = insarg_to_constreg (img, tnode_nthsubof (instr, 2), 16, 23, cgen);
		img->image[offs++] = 0x03;
		img->image[offs++] = ((rd << 4) & 0x70) | (rr & 0x07);
		width = 2;
		break;
		/*}}}*/
	case INS_NEG: /*{{{  two's complement*/
		rd = insarg_to_constreg (img, tnode_nthsubof (instr, 1), 0, 31, cgen);	
		img->image[offs++] = 0x94 | ((rd >> 4) & 0x01);
		img->image[offs++] = ((rd << 4) & 0xf0) | 0x01;
		width = 2;
		break;
		/*}}}*/
	case INS_NOP: /*{{{  no-operation*/
		img->image[offs++] = 0x00;
		img->image[offs++] = 0x00;
		width = 2;
		break;
		/*}}}*/
	case INS_OR: /*{{{  logical-or*/
		rd = insarg_to_constreg (img, tnode_nthsubof (instr, 1), 0, 31, cgen);	
		rr = insarg_to_constreg (img, tnode_nthsubof (instr, 2), 0, 31, cgen);
		img->image[offs++] = 0x28 | ((rr >> 3) & 0x02) | ((rd >> 4) & 0x01);
		img->image[offs++] = ((rd & 0x0f) << 4) | (rr & 0x0f);
		width = 2;
		break;
		/*}}}*/
	case INS_ORI: /*{{{  logical-or with immediate*/
		rd = insarg_to_constreg (img, tnode_nthsubof (instr, 1), 16, 31, cgen);	
		val = insarg_to_constval (img, tnode_nthsubaddr (instr, 2), instr, offs, 0, 255, cgen);
		img->image[offs++] = 0x60 | ((val >> 4) & 0x0f);
		img->image[offs++] = ((rd & 0x0f) << 4) | (val & 0x0f);
		width = 2;
		break;
		/*}}}*/
	case INS_OUT: /*{{{  output to I/O port*/
		val = insarg_to_constval (img, tnode_nthsubaddr (instr, 1), instr, offs, 0, 63, cgen);
		rr = insarg_to_constreg (img, tnode_nthsubof (instr, 2), 0, 31, cgen);
		img->image[offs++] = 0xb8 | ((val >> 3) & 0x06) | ((rr >> 4) & 0x01);
		img->image[offs++] = ((rr & 0x0f) << 4) | (val & 0x0f);
		width = 2;
		break;
		/*}}}*/
	case INS_POP: /*{{{  pop from stack*/
		rd = insarg_to_constreg (img, tnode_nthsubof (instr, 1), 0, 31, cgen);
		img->image[offs++] = 0x90 | ((rd >> 4) & 0x01);
		img->image[offs++] = ((rd & 0x0f) << 4) | 0x0f;
		width = 2;
		break;
		/*}}}*/
	case INS_PUSH: /*{{{  push onto stack*/
		rd = insarg_to_constreg (img, tnode_nthsubof (instr, 1), 0, 31, cgen);
		img->image[offs++] = 0x92 | ((rd >> 4) & 0x01);
		img->image[offs++] = ((rd & 0x0f) << 4) | 0x0f;
		width = 2;
		break;
		/*}}}*/
	case INS_RCALL: /*{{{  relative call*/
		val = insarg_to_constaddrdiff (img, tnode_nthsubof (instr, 1), instr, offs, 2, -2048, 2047, cgen);
		img->image[offs++] = 0xd0 | ((val >> 8) & 0x0f);
		img->image[offs++] = (val & 0xff);
		width = 2;
		break;
		/*}}}*/
	case INS_RET: /*{{{  return from subroutine*/
		img->image[offs++] = 0x95;
		img->image[offs++] = 0x08;
		width = 2;
		break;
		/*}}}*/
	case INS_RETI: /*{{{  return from interrupt*/
		img->image[offs++] = 0x95;
		img->image[offs++] = 0x18;
		width = 2;
		break;
		/*}}}*/
	case INS_RJMP: /*{{{  relative jump*/
		val = insarg_to_constaddrdiff (img, tnode_nthsubof (instr, 1), instr, offs, 2, -2048, 2047, cgen);
		img->image[offs++] = 0xc0 | ((val >> 8) & 0x0f);
		img->image[offs++] = val & 0xff;
		width = 2;
		break;
		/*}}}*/
	case INS_ROL: /*{{{  rotate left through carry*/
		rd = insarg_to_constreg (img, tnode_nthsubof (instr, 1), 0, 31, cgen);
		img->image[offs++] = 0x1c | ((rd >> 3) & 0x02) | ((rd >> 4) & 0x01);
		img->image[offs++] = ((rd & 0x0f) << 4) | (rd & 0x0f);
		width = 2;
		break;
		/*}}}*/
	case INS_ROR: /*{{{  rotate right through carry*/
		rd = insarg_to_constreg (img, tnode_nthsubof (instr, 1), 0, 31, cgen);
		img->image[offs++] = 0x94 | ((rd >> 4) & 0x01);
		img->image[offs++] = ((rd & 0x0f) << 4) | 0x07;
		width = 2;
		break;
		/*}}}*/
	case INS_SBC: /*{{{  subtract with carry*/
		rd = insarg_to_constreg (img, tnode_nthsubof (instr, 1), 0, 31, cgen);
		rr = insarg_to_constreg (img, tnode_nthsubof (instr, 2), 0, 31, cgen);
		img->image[offs++] = 0x08 | ((rr >> 3) & 0x02) | ((rd >> 4) & 0x01);
		img->image[offs++] = ((rd & 0x0f) << 4) | (rr & 0x0f);
		width = 2;
		break;
		/*}}}*/
	case INS_SBCI: /*{{{  subtract immediate with carry*/
		rd = insarg_to_constreg (img, tnode_nthsubof (instr, 1), 16, 31, cgen);
		val = insarg_to_constval (img, tnode_nthsubaddr (instr, 2), instr, offs, 0, 255, cgen);
		img->image[offs++] = 0x40 | ((val >> 4) & 0x0f);
		img->image[offs++] = ((rd & 0x0f) << 4) | (val & 0x0f);
		width = 2;
		break;
		/*}}}*/
	case INS_SBI: /*{{{  set bit in I/O register*/
		val = insarg_to_constval (img, tnode_nthsubaddr (instr, 1), instr, offs, 0, 31, cgen);
		val2 = insarg_to_constval (img, tnode_nthsubaddr (instr, 2), instr, offs, 0, 7, cgen);
		img->image[offs++] = 0x9a;
		img->image[offs++] = ((val & 0x1f) << 3) | (val2 & 0x07);
		width = 2;
		break;
		/*}}}*/
	case INS_SBIC: /*{{{  skip if bit in I/O register clear*/
		val = insarg_to_constval (img, tnode_nthsubaddr (instr, 1), instr, offs, 0, 31, cgen);
		val2 = insarg_to_constval (img, tnode_nthsubaddr (instr, 2), instr, offs, 0, 7, cgen);
		img->image[offs++] = 0x99;
		img->image[offs++] = ((val & 0x1f) << 3) | (val2 & 0x07);
		width = 2;
		break;
		/*}}}*/
	case INS_SBIS: /*{{{  skip if bit in I/O register set*/
		val = insarg_to_constval (img, tnode_nthsubaddr (instr, 1), instr, offs, 0, 31, cgen);
		val2 = insarg_to_constval (img, tnode_nthsubaddr (instr, 2), instr, offs, 0, 7, cgen);
		img->image[offs++] = 0x9b;
		img->image[offs++] = ((val & 0x1f) << 3) | (val2 & 0x07);
		width = 2;
		break;
		/*}}}*/
	case INS_SBIW: /*{{{  subtract immediate from word*/
		if (tnode_nthsubof (instr, 1)->tag == avrasm.tag_XYZREG) {
			/* directly specified X,Y,Z (==r26,r28,r30) */
			avrasm_getxyzreginfo (tnode_nthsubof (instr, 1), &rd, &prepost, &val2);

			if (val2 || prepost) {
				codegen_node_error (cgen, instr, "invalid modifier for X,Y,Z registers for \"sbiw\"");
				rd = 0;
			} else {
				rd = 26 + (rd * 2);	/* X<Y<Z -> 26,28,30 */
			}
		} else {
			rd = insarg_to_constreg (img, tnode_nthsubof (instr, 1), 24, 30, cgen);
		}
		val = insarg_to_constval (img, tnode_nthsubaddr (instr, 2), instr, offs, 0, 63, cgen);
		if (rd & 0x01) {
			/* cannot have odd-numbered register here, must be r25:r24, ..., r31:r30 */
			codegen_node_error (cgen, instr, "invalid register %d for \"sbiw\" (24,26,28,30)", rd);
			rd = 0;
		}
		rd = (rd - 24) / 2;		/* -> [0..3] */
		img->image[offs++] = 0x97;
		img->image[offs++] = ((val & 0x30) << 2) | (rd << 4) | (val & 0x0f);
		width = 2;
		break;
		/*}}}*/
	case INS_SBR: /*{{{  set bits in register*/
		rd = insarg_to_constreg (img, tnode_nthsubof (instr, 1), 16, 31, cgen);
		val = insarg_to_constval (img, tnode_nthsubaddr (instr, 2), instr, offs, 0, 255, cgen);
		img->image[offs++] = 0x60 | ((val >> 4) & 0x0f);
		img->image[offs++] = ((rd << 4) & 0xf0) | (val & 0x0f);
		width = 2;
		break;
		/*}}}*/
	case INS_SBRC: /*{{{  skip if bit in register clear*/
		rr = insarg_to_constreg (img, tnode_nthsubof (instr, 1), 0, 31, cgen);
		val = insarg_to_constval (img, tnode_nthsubaddr (instr, 2), instr, offs, 0, 7, cgen);
		img->image[offs++] = 0xfc | ((rr >> 4) & 0x01);
		img->image[offs++] = ((rr << 4) & 0xf0) | (val & 0x07);
		width = 2;
		break;
		/*}}}*/
	case INS_SBRS: /*{{{  skip if bit in register set*/
		rr = insarg_to_constreg (img, tnode_nthsubof (instr, 1), 0, 31, cgen);
		val = insarg_to_constval (img, tnode_nthsubaddr (instr, 2), instr, offs, 0, 7, cgen);
		img->image[offs++] = 0xfe | ((rr >> 4) & 0x01);
		img->image[offs++] = ((rr << 4) & 0xf0) | (val & 0x07);
		width = 2;
		break;
		/*}}}*/
	case INS_SEC: /*{{{  set carry flag*/
		img->image[offs++] = 0x94;
		img->image[offs++] = 0x08;
		width = 2;
		break;
		/*}}}*/
	case INS_SEH: /*{{{  set half-carry flag*/
		img->image[offs++] = 0x94;
		img->image[offs++] = 0x58;
		width = 2;
		break;
		/*}}}*/
	case INS_SEI: /*{{{  set interrupt flag (interrupt enable)*/
		img->image[offs++] = 0x94;
		img->image[offs++] = 0x78;
		width = 2;
		break;
		/*}}}*/
	case INS_SEN: /*{{{  set negative flag*/
		img->image[offs++] = 0x94;
		img->image[offs++] = 0x28;
		width = 2;
		break;
		/*}}}*/
	case INS_SER: /*{{{  set bits in register*/
		rd = insarg_to_constreg (img, tnode_nthsubof (instr, 1), 16, 31, cgen);
		img->image[offs++] = 0xef;
		img->image[offs++] = ((rd << 4) & 0xf0) | 0x0f;
		width = 2;
		break;
		/*}}}*/
	case INS_SES: /*{{{  set signed flag*/
		img->image[offs++] = 0x94;
		img->image[offs++] = 0x48;
		width = 2;
		break;
		/*}}}*/
	case INS_SET: /*{{{  set T flag*/
		img->image[offs++] = 0x94;
		img->image[offs++] = 0x68;
		width = 2;
		break;
		/*}}}*/
	case INS_SEV: /*{{{  set overflow flag*/
		img->image[offs++] = 0x94;
		img->image[offs++] = 0x38;
		width = 2;
		break;
		/*}}}*/
	case INS_SEZ: /*{{{  set zero flag*/
		img->image[offs++] = 0x94;
		img->image[offs++] = 0x18;
		width = 2;
		break;
		/*}}}*/
	case INS_SLEEP: /*{{{  sleep*/
		img->image[offs++] = 0x95;
		img->image[offs++] = 0x88;
		width = 2;
		break;
		/*}}}*/
	case INS_SPM: /*{{{  store program memory (implied operands)*/
		img->image[offs++] = 0x95;
		img->image[offs++] = 0xe8;
		width = 2;
		break;
		/*}}}*/
	case INS_ST: /*{{{  store / store with displacement*/
	case INS_STD:
		avrasm_getxyzreginfo (tnode_nthsubof (instr, 1), &rd, &prepost, &val);
		rr = insarg_to_constreg (img, tnode_nthsubof (instr, 2), 0, 31, cgen);

		/* various encodings for this */
		switch (rd) {
		case 0:		/* X */
			if (val) {
				codegen_node_error (cgen, instr, "cannot use offset with X register");
				return 0;
			}
			if (prepost > 0) {
				img->image[offs++] = 0x92 | ((rr >> 4) & 0x01);
				img->image[offs++] = ((rr << 4) & 0xf0) | 0x0d;
			} else if (prepost < 0) {
				img->image[offs++] = 0x92 | ((rr >> 4) & 0x01);
				img->image[offs++] = ((rr << 4) & 0xf0) | 0x0e;
			} else {
				img->image[offs++] = 0x92 | ((rr >> 4) & 0x01);
				img->image[offs++] = ((rr << 4) & 0xf0) | 0x0c;
			}
			break;
		case 1:		/* Y */
			if (val) {
				img->image[offs++] = 0x82 | (val & 0x20) | ((val >> 1) & 0x0c) | ((rr >> 4) & 0x01);
				img->image[offs++] = ((rr << 4) & 0xf0) | 0x08 | (val & 0x07);
			} else if (prepost > 0) {
				img->image[offs++] = 0x92 | ((rr >> 4) & 0x01);
				img->image[offs++] = ((rr << 4) & 0xf0) | 0x09;
			} else if (prepost < 0) {
				img->image[offs++] = 0x92 | ((rr >> 4) & 0x01);
				img->image[offs++] = ((rr << 4) & 0xf0) | 0x0a;
			} else {
				img->image[offs++] = 0x82 | ((rr >> 4) & 0x01);
				img->image[offs++] = ((rr << 4) & 0xf0) | 0x08;
			}
			break;
		case 2:		/* Z */
			if (val) {
				img->image[offs++] = 0x82 | (val & 0x20) | ((val >> 1) & 0x0c) | ((rr >> 4) & 0x01);
				img->image[offs++] = ((rr << 4) & 0xf0) | (val & 0x07);
			} else if (prepost > 0) {
				img->image[offs++] = 0x92 | ((rr >> 4) & 0x01);
				img->image[offs++] = ((rr << 4) & 0xf0) | 0x01;
			} else if (prepost < 0) {
				img->image[offs++] = 0x92 | ((rr >> 4) & 0x01);
				img->image[offs++] = ((rr << 4) & 0xf0) | 0x02;
			} else {
				img->image[offs++] = 0x82 | ((rr >> 4) & 0x01);
				img->image[offs++] = ((rr << 4) & 0xf0);
			}
			break;
		}
		width = 2;
		break;
		/*}}}*/
	case INS_STS: /*{{{  store direct (to immediate address)*/
		val = insarg_to_constval (img, tnode_nthsubaddr (instr, 1), instr, offs, 0, (1 << 16) - 1, cgen);
		// val = insarg_to_constaddr (img, tnode_nthsubof (instr, 1), instr, offs, 0, (1 << 16) - 1, cgen);
		rr = insarg_to_constreg (img, tnode_nthsubof (instr, 2), 0, 31, cgen);
		img->image[offs++] = 0x92 | ((rr >> 4) & 0x01);
		img->image[offs++] = ((rr << 4) & 0xf0);
		img->image[offs++] = (val >> 8) & 0xff;
		img->image[offs++] = val & 0xff;
		width = 4;
		break;
		/*}}}*/
	case INS_SUB: /*{{{  subtract without carry*/
		rd = insarg_to_constreg (img, tnode_nthsubof (instr, 1), 0, 31, cgen);	
		rr = insarg_to_constreg (img, tnode_nthsubof (instr, 2), 0, 31, cgen);
		img->image[offs++] = 0x18 | ((rr >> 3) & 0x02) | ((rd >> 4) & 0x01);
		img->image[offs++] = ((rd & 0x0f) << 4) | (rr & 0x0f);
		width = 2;
		break;
		/*}}}*/
	case INS_SUBI: /*{{{  subtract immediate*/
		rd = insarg_to_constreg (img, tnode_nthsubof (instr, 1), 16, 31, cgen);
		val = insarg_to_constval (img, tnode_nthsubaddr (instr, 2), instr, offs, 0, 255, cgen);
		img->image[offs++] = 0x50 | ((val >> 4) & 0x0f);
		img->image[offs++] = ((rd & 0x0f) << 4) | (val & 0x0f);
		width = 2;
		break;
		/*}}}*/
	case INS_SWAP: /*{{{  swap nibbles in byte*/
		rd = insarg_to_constreg (img, tnode_nthsubof (instr, 1), 0, 31, cgen);
		img->image[offs++] = 0x94 | ((rd >> 4) & 0x01);
		img->image[offs++] = ((rd << 4) & 0xf0) | 0x02;
		width = 2;
		break;
		/*}}}*/
	case INS_TST: /*{{{  test for zero or minus*/
		rd = insarg_to_constreg (img, tnode_nthsubof (instr, 1), 0, 31, cgen);
		img->image[offs++] = 0x20 | ((rd >> 3) & 0x02) | ((rd >> 4) & 0x01);
		img->image[offs++] = ((rd << 4) & 0xf0) | (rd & 0x0f);
		width = 2;
		break;
		/*}}}*/
	case INS_WDR: /*{{{  watchdog reset*/
		img->image[offs++] = 0x95;
		img->image[offs++] = 0xa8;
		width = 2;
		break;
		/*}}}*/
	default:
		codegen_node_error (cgen, instr, "atmelavr_assemble_instr(): unhandled instruction \"%s\"", inst->str);
		return 0;
	}

	if (target->bswap_code && (img->zone->tag == avrasm.tag_TEXTSEG)) {
		/* better byte-swap what we just generated */
		unsigned char tmp;
		int i;

		for (i=(offs - width); i<offs; i+=2) {
			tmp = img->image[i];
			img->image[i] = img->image[i+1];
			img->image[i+1] = tmp;
		}
	}

	*offset = offs;
	return width;
}
/*}}}*/

/*{{{  static int sub_compare_ranges (imgrange_t *r1, imgrange_t *r2)*/
/*
 *	comparison function for ranges
 */
static int sub_compare_ranges (imgrange_t *r1, imgrange_t *r2)
{
	if (r1->start == r2->start) {
		/* bigger issues than this! */
		return r1->size - r2->size;
	}
	return r1->start - r2->start;
}
/*}}}*/
/*{{{  static int img_sort_ranges (atmelavr_image_t *img)*/
/*
 *	sorts ranges in a single image into start order
 *	returns 0 on success, non-zero on failure
 */
static int img_sort_ranges (atmelavr_image_t *img)
{
	dynarray_qsort (img->ranges, sub_compare_ranges);
	return 0;
}
/*}}}*/
/*{{{  static int img_check_ranges (atmelavr_image_t *img, codegen_t *cgen)*/
/*
 *	checks for overlaps in ranges (malformed .org directives probably)
 *	returns 0 on success, non-zero on failure (error emitted)
 */
static int img_check_ranges (atmelavr_image_t *img, codegen_t *cgen)
{
	int i;
	imgrange_t *last = NULL;

	for (i=0; i<DA_CUR (img->ranges); i++) {
		if (!last) {
			last = DA_NTHITEM (img->ranges, i);
		} else {
			imgrange_t *cur = DA_NTHITEM (img->ranges, i);

			if ((last->start + last->size) > cur->start) {
				codegen_error (cgen, "overlapping regions in image for [%s], ranges [%d - %d], [%d - %d]",
						img->zone->tag->name, last->start, (last->start + last->size), cur->start, (cur->start + cur->size));
				return -1;
			}
		}
	}
	return 0;
}
/*}}}*/
/*{{{  static int img_combine_ranges (atmelavr_image_t *img)*/
/*
 *	combines multiple small ranges into a single (big) one
 *	returns 0 on success, non-zero on failure
 */
static int img_combine_ranges (atmelavr_image_t *img)
{
	int i;
	imgrange_t *imr, *final;

	if (DA_CUR (img->ranges) <= 1) {
		return 0;			/* nothing to do */
	}
	imr = DA_NTHITEM (img->ranges, 0);
	final = DA_NTHITEM (img->ranges, DA_CUR (img->ranges) - 1);

	imr->size = (final->start + final->size) - imr->start;

	/* kill off the rest, including 'final' */
	while (DA_CUR (img->ranges) > 1) {
		int lidx = DA_CUR (img->ranges) - 1;
		imgrange_t *last = DA_NTHITEM (img->ranges, lidx);

		atmelavr_freeimgrange (last);
		dynarray_delitem (img->ranges, lidx);
	}
	return 0;
}
/*}}}*/
/*{{{  static int img_write_hexfile (atmelavr_image_t *img, fhandle_t *fhan, codegen_t *cgen)*/
/*
 *	writes out an image to a .hex (Intel style) file
 *	returns 0 on success, non-zero on failure (errors emitted)
 */
static int img_write_hexfile (atmelavr_image_t *img, fhandle_t *fhan, codegen_t *cgen)
{
	imgrange_t *imr;
	int addr, end;

	if (!DA_CUR (img->ranges)) {
		codegen_warning (cgen, "nothing to write in image for [%s]!", img->zone->tag->name);
		return 0;
	}
	imr = DA_NTHITEM (img->ranges, 0);
	addr = imr->start;
	end = imr->start + imr->size;

	/* if not 16-byte aligned at the start, emit a partial row */
	if (addr & 0x0f) {
		int left = 16 - (addr & 0x0f);
		unsigned char csum = 0x00;
		int k;
		
		if (left > imr->size) {
			left = imr->size;	/* tiny */
		}
		fhandle_printf (fhan, ":%2.2X:%4.4X00", left, addr);
		csum = (unsigned char)left;
		csum += (unsigned char)((addr >> 8) & 0xff);
		csum += (unsigned char)(addr & 0xff);
		csum += 0x00;

		for (k=0; k<left; k++) {
			int idx = (addr + k);

			fhandle_printf (fhan, "%2.2X", img->image[idx]);
			csum += img->image[idx];
		}

		/* checksum */
		csum = (~csum) + 1;
		fhandle_printf (fhan, "%2.2X\n", csum);

		addr += left;
	}

	/* and the rest */
	while (addr < end) {
		int cs, k;
		unsigned char csum = 0x00;

		cs = end - addr;
		if (cs > 16) {
			cs = 16;		/* at a time */
		}

		fhandle_printf (fhan, ":%2.2X%4.4X00", cs, addr);
		csum = (unsigned char)cs;
		csum += (unsigned char)((addr >> 8) & 0xff);
		csum += (unsigned char)(addr & 0xff);
		csum += 0x00;

		for (k=0; k<cs; k++) {
			int idx = (addr + k);

			fhandle_printf (fhan, "%2.2X", img->image[idx]);
			csum += img->image[idx];
		}

		/* checksum */
		csum = (~csum) + 1;
		fhandle_printf (fhan, "%2.2X\n", csum);

		addr += cs;
	}

	/* finally, emit end-of-file record */
	fhandle_printf (fhan, ":00000001FF\n");

	return 0;
}
/*}}}*/


/*{{{  static int atmelavr_be_codegen_init (codegen_t *cgen, lexfile_t *srcfile)*/
/*
 *	initialises back-end code generation for Atmel AVR target
 *	returns 0 on success, non-zero on failure
 */
static int atmelavr_be_codegen_init (codegen_t *cgen, lexfile_t *srcfile)
{
	atmelavr_priv_t *apriv = (atmelavr_priv_t *)cgen->target->priv;
	char hostnamebuf[128];
	char timebuf[128];
	int i;

#if 0
fprintf (stderr, "atmelavr_be_codegen_init(): here!\n");
#endif
	/* output is a listing file, .hex output happens separately */
	codegen_write_fmt (cgen, "#\n#\t%s\n", cgen->fname);
	codegen_write_fmt (cgen, "#\tassembled from %s\n", srcfile->filename ?: "(unknown)");
	if (gethostname (hostnamebuf, sizeof (hostnamebuf) - 1)) {
		strcpy (hostnamebuf, "(unknown)");
	}
#ifdef HAVE_TIME_H
	{
		time_t now;
		char *ch;

		time (&now);
		ctime_r (&now, timebuf);

		if ((ch = strchr (timebuf, '\n'))) {
			*ch = '\0';
		}
	}
#else
	strcpy (timebuf, "(unknown)");
#endif
	codegen_write_fmt (cgen, "#\ton host %s at %s\n", hostnamebuf, timebuf);
	codegen_write_fmt (cgen, "#\tsource language: %s, target: %s\n", parser_langname (srcfile) ?: "(unknown)", compopts.target_str);
	codegen_write_string (cgen, "#\n\n");

	return 0;
}
/*}}}*/
/*{{{  static int atmelavr_be_codegen_final (codegen_t *cgen, lexfile_t *srcfile)*/
/*
 *	shutdown back-end code generation for Atmel AVR target
 *	returns 0 on success, non-zero on failure
 */
static int atmelavr_be_codegen_final (codegen_t *cgen, lexfile_t *srcfile)
{
	return 0;
}
/*}}}*/
/*{{{  static void atmelavr_be_do_precode (tnode_t **ptptr, codegen_t *cgen)*/
/*
 *	called before code-generation starts
 */
static void atmelavr_be_do_precode (tnode_t **ptptr, codegen_t *cgen)
{
	tnode_t *tptr = *ptptr;
	atmelavr_priv_t *apriv = (atmelavr_priv_t *)cgen->target->priv;
	int i, nitems;
	tnode_t **items;

#if 0
fprintf (stderr, "atmelavr_be_do_precode(): here!\n");
#endif
	if (!parser_islistnode (tptr)) {
		nocc_internal ("atmelavr_be_do_precode(): parse-tree is not a list!  was [%s]", tptr->tag->name);
		return;
	}
	items = parser_getlistitems (tptr, &nitems);

	/* look for MCU setting */
	for (i=0; i<nitems; i++) {
		tnode_t *thisnode = items[i];

		if (thisnode->tag == avrasm.tag_MCUMARK) {
			if (apriv->mcu) {
				tnode_warning (thisnode, "MCU already set to [%s]", apriv->mcu->name);
				break;		/* for() */
			}
			apriv->mcu = avrasm_findtargetbymark (tnode_nthsubof (thisnode, 0));
			if (!apriv->mcu) {
				tnode_error (thisnode, "invalid MCU specified");
				codegen_error (cgen, "invalid MCU");
				break;		/* for() */
			}
		}
	}

	if (!apriv->mcu) {
		codegen_warning (cgen, "No MCU specified, assuming ATMEGA328");
		apriv->mcu = avrasm_findtargetbyname ("ATMEGA328");
	}
	if (apriv->mcu) {
		codegen_write_fmt (cgen, "# using MCU \"%s\"\n", apriv->mcu->name);
	}

	return;
}
/*}}}*/
/*{{{  static int atmelavr_do_assemble (codegen_t *cgen, atmelavr_priv_t *apriv, atmelavr_image_t *img, tnode_t *contents)*/
/*
 *	called to assemble stuff into a memory image;  codegen to listing file
 *	returns 0 on success, non-zero on failure (will emit errors/warnings)
 */
static int atmelavr_do_assemble (codegen_t *cgen, atmelavr_priv_t *apriv, atmelavr_image_t *img, tnode_t *contents)
{
	int i, nitems;
	tnode_t **items;
	int genoffset = 0;
	imgrange_t *imr = NULL;

	if (!parser_islistnode (contents)) {
		codegen_error (cgen, "segment contents not a list, got [%s]", contents->tag->name);
		return -1;
	}

	items = parser_getlistitems (contents, &nitems);
	for (i=0; i<nitems; i++) {
#if 0
fprintf (stderr, "atmelavr_do_assemble(): want to assemble [%s] into image (%d/%d bytes)\n", items[i]->tag->name, genoffset, img->isize);
#endif
		/* things we don't expect to see */
		if (items[i]->tag == avrasm.tag_SEGMENTMARK) {
			codegen_node_error (cgen, items[i], "unexpected segment mark here!");
			return -1;
		}

		/* things that are ignorable */
		if ((items[i]->tag == avrasm.tag_TARGETMARK) || (items[i]->tag == avrasm.tag_MCUMARK)) {
			continue;
		}
		if ((items[i]->tag == avrasm.tag_MACRODEF) || (items[i]->tag == avrasm.tag_EQU) || (items[i]->tag == avrasm.tag_DEF)) {
			continue;
		}

		if (items[i]->tag == avrasm.tag_ORG) {
			/*{{{  .org to start range*/
			tnode_t *addr = tnode_nthsubof (items[i], 0);
			int aval;

			if (!constprop_isconst (addr)) {
				codegen_node_error (cgen, items[i], "non-constant origin");
				return -1;
			}
			aval = constprop_intvalof (addr);

			if (imr) {
				/* clean this one up */
				imr->size = genoffset - imr->start;
				dynarray_add (img->ranges, imr);
			}
			imr = atmelavr_newimgrange ();
			imr->start = aval;
			genoffset = aval;
			imr->size = 0;

			/*}}}*/
			continue;
		}

		/* anything else must be in a viable range! */
		if (!imr) {
			int j, addr = 0;

			/* look at image for greatest offset */
			for (j=0; j<DA_CUR (img->ranges); j++) {
				imgrange_t *timr = DA_NTHITEM (img->ranges, j);

				if ((timr->start + timr->size) > addr) {
					addr = (((timr->start + timr->size) + 1) & ~1);		/* round up */
				}
			}

			// codegen_node_warning (cgen, items[i], "origin not specified for [%s], assuming 0", items[i]->tag->name);
			imr = atmelavr_newimgrange ();
			imr->start = addr;
			imr->size = 0;
			genoffset = addr;
		}

		if ((items[i]->tag == avrasm.tag_GLABELDEF) || (items[i]->tag == avrasm.tag_LLABELDEF)) {
			/*{{{  planting label definition*/
			aavr_labelinfo_t *aali;
			int j;
			tnode_t *rname = tnode_nthsubof (items[i], 0);
			char *srname = NULL;

			if (rname) {
				langops_getname (rname, &srname);
			}

			if (!tnode_haschook (items[i], labelinfo_chook)) {
				/* create new one */
				aali = atmelavr_newaavrlabelinfo ();
				tnode_setchook (items[i], labelinfo_chook, aali);
			} else {
				aali = (aavr_labelinfo_t *)tnode_getchook (items[i], labelinfo_chook);
			}

			aali->img = img;
			aali->baddr = genoffset;
			aali->labins = items[i];

			if (compopts.verbose > 2) {
				nocc_message ("%s:%-5d label [%s]", img->zone->tag->name, aali->baddr, srname ?: "(anon)");
			}
#if 0
fhandle_printf (FHAN_STDERR, "atmelavr_do_assemble(): planted label definition [%s] at address %d, got %d fixups..\n", srname ?: "", aali->baddr, DA_CUR (aali->fixups));
#endif
			if (srname) {
				sfree (srname);
				srname = NULL;
			}

			for (j=0; j<DA_CUR (aali->fixups); j++) {
				aavr_labelfixup_t *aalf = DA_NTHITEM (aali->fixups, j);

				if (aalf->instr->tag == avrasm.tag_INSTR) {
					avrinstr_tbl_t *inst = (avrinstr_tbl_t *)tnode_nthhookof (aalf->instr, 0);
					int bytesin, offset = aalf->offset;

					bytesin = atmelavr_assemble_instr (aalf->img, &offset, aalf->instr, inst, cgen, apriv->mcu);
				} else if ((aalf->instr->tag == avrasm.tag_CONST) || (aalf->instr->tag == avrasm.tag_CONST16)) {
					int bytesin, offset = aalf->offset;

					bytesin = atmelavr_assemble_const (aalf->img, &offset, aalf->instr, cgen, apriv->mcu);
				} else {
					codegen_node_error (cgen, aalf->instr, "cannot fixup thing, unknown node type [%s]", aalf->instr->tag->name);
				}

				atmelavr_freeaavrlabelfixup (aalf);
			}
			dynarray_trash (aali->fixups);
			dynarray_init (aali->fixups);


			/*}}}*/
			continue;
		}

		if ((items[i]->tag == avrasm.tag_SPACE) || (items[i]->tag == avrasm.tag_SPACE16)) {
			/*{{{  reserving space, doesn't need writeable image*/
			int bytes = constprop_intvalof (tnode_nthsubof (items[i], 0));

			if (items[i]->tag == avrasm.tag_SPACE16) {
				bytes *= 2;
			}

			genoffset += bytes;
			/*}}}*/
			continue;
		}

		/* anything else must be in a writable range */
		if (!img->canwrite) {
			codegen_node_error (cgen, items[i], "cannot assemble here! (unwritable segment)");
			return -1;
		}
		if (genoffset >= img->isize) {
			codegen_node_error (cgen, items[i], "reached end-of-segment, cannot fit anymore in!");
			return 0;
		}

		if ((items[i]->tag == avrasm.tag_CONST) || (items[i]->tag == avrasm.tag_CONST16)) {
			/*{{{  constant data, into eeprom or text (must be writable)*/
			int bytesin;
			
			bytesin = atmelavr_assemble_const (img, &genoffset, items[i], cgen, apriv->mcu);

#if 0
fprintf (stderr, "atmelavr_do_assemble(): FIXME! constant..\n");
#endif
			/*}}}*/
			continue;
		}
		if (items[i]->tag == avrasm.tag_INSTR) {
			/*{{{  instruction, into text presumably*/
			avrinstr_tbl_t *inst = (avrinstr_tbl_t *)tnode_nthhookof (items[i], 0);
			int bytesin;

#if 0
fprintf (stderr, "atmelavr_do_assemble(): at %d, instruction [%s]\n", genoffset, inst->str);
#endif
			bytesin = atmelavr_assemble_instr (img, &genoffset, items[i], inst, cgen, apriv->mcu);
			/*}}}*/
			continue;
		}
	}

	/* if we have some range left, store it */
	if (imr) {
		/* clean this one up */
		imr->size = genoffset - imr->start;
		dynarray_add (img->ranges, imr);
	}

	return 0;
}
/*}}}*/
/*{{{  static int atmelavr_constprop_glabel (compops_t *cops, tnode_t **tptr)*/
/*
 *	called during code-gen to do constant propagation on labels -- if we have the address, use that.
 */
static int atmelavr_constprop_glabel (compops_t *cops, tnode_t **tptr)
{
	int i = 1;

	if (((*tptr)->tag == avrasm.tag_GLABEL) || ((*tptr)->tag == avrasm.tag_LLABEL)) {
		name_t *labname = tnode_nthnameof (*tptr, 0);
		tnode_t *ndecl = NameDeclOf (labname);
		aavr_labelinfo_t *aali;

		if (tnode_haschook (ndecl, labelinfo_chook)) {
			aali = (aavr_labelinfo_t *)tnode_getchook (ndecl, labelinfo_chook);
		} else {
			/* create new label info */
			aali = atmelavr_newaavrlabelinfo ();
			tnode_setchook (ndecl, labelinfo_chook, aali);
		}
		if (aali->baddr >= 0) {
			/* got address for this label, use it! */
			*tptr = constprop_newconst (CONST_INT, *tptr, NULL, aali->baddr);
		} else {
			/* need to add fixup */
			aavr_labelfixup_t *aalf = atmelavr_newaavrlabelfixup ();

			if (!atmelavr_constpropstate) {
				nocc_internal ("atmelavr_constprop_glabel(): atmelavr_constpropstate is NULL!");
				return 1;
			}
			aalf->img = atmelavr_constpropstate->img;
			aalf->instr = atmelavr_constpropstate->instr;
			aalf->offset = atmelavr_constpropstate->offset;
			atmelavr_constpropstate->didfix++;

			dynarray_add (aali->fixups, aalf);
		}
	} else {
		/* down-stream constprop */
		if (cops->next && tnode_hascompop_i (cops->next, (int)COPS_CONSTPROP)) {
			i = tnode_callcompop_i (cops->next, (int)COPS_CONSTPROP, 1, tptr);
		}
	}
	return i;
}
/*}}}*/
/*{{{  static void atmelavr_be_do_codegen (tnode_t *tptr, codegen_t *cgen)*/
/*
 *	called to do the actual code-generation, given the parse-tree at this stage
 */
static void atmelavr_be_do_codegen (tnode_t *tptr, codegen_t *cgen)
{
	atmelavr_priv_t *apriv = (atmelavr_priv_t *)cgen->target->priv;
	int i, nitems;
	tnode_t **items;

#if 0
fprintf (stderr, "atmelavr_be_do_codegen(): here!\n");
#endif
	if (!parser_islistnode (tptr)) {
		nocc_internal ("atmelavr_be_do_codegen(): parse-tree is not a list!  was [%s]", tptr->tag->name);
		return;
	}

	/* alter constant-propagation for labels, needed to fixup addresses used as constants */
	{
		compops_t *cops = tnode_insertcompops (avrasm.tag_GLABEL->ndef->ops);

		tnode_setcompop (cops, "constprop", 1, COMPOPTYPE (atmelavr_constprop_glabel));
		avrasm.tag_GLABEL->ndef->ops = cops;
		/* Note: also gets tag_LLABEL doing this! */
	}

	items = parser_getlistitems (tptr, &nitems);

	for (i=0; i<nitems; i++) {
		tnode_t *thisnode = items[i];

		if (thisnode->tag == avrasm.tag_SEGMENTMARK) {
			tnode_t *zone = tnode_nthsubof (thisnode, 0);
			atmelavr_image_t *img;
			int j;
			int res;
			
			/* find compatible image, or create one */
			for (j=0; j<DA_CUR (apriv->images); j++) {
				img = DA_NTHITEM (apriv->images, j);

				if (img->zone->tag == zone->tag) {
					/* this one */
					break;		/* for() */
				}
			}
			if (j == DA_CUR (apriv->images)) {
				/* create new */
				img = atmelavr_newatmelavrimage ();
				img->zone = zone;

				if (zone->tag == avrasm.tag_TEXTSEG) {
					img->isize = apriv->mcu->code_size;
					img->canwrite = 1;
				} else if (zone->tag == avrasm.tag_EEPROMSEG) {
					img->isize = apriv->mcu->eeprom_size;
					img->canwrite = 1;
				} else if (zone->tag == avrasm.tag_DATASEG) {
					/* we cannot assemble into this, but size kept */
					img->isize = apriv->mcu->ram_start + apriv->mcu->ram_size;
					img->canwrite = 0;
				} else {
					codegen_error (cgen, "unknown segment [%s] for target image", zone->tag->name);
					img->isize = 0;
					img->canwrite = 0;
				}
				if (img->isize) {
					img->image = (unsigned char *)smalloc (img->isize);
					memset (img->image, 0x00, img->isize);
				} else {
					img->image = NULL;
				}

				dynarray_add (apriv->images, img);
			}

			/* and then assemble the contents! */
			res = atmelavr_do_assemble (cgen, apriv, img, tnode_nthsubof (thisnode, 1));
			if (res) {
				codegen_error (cgen, "failed to assemble, giving up");
				goto outlab;
			}
		}
	}

	/* should have assembled into all the segment images now */
	for (i=0; i<DA_CUR (apriv->images); i++) {
		atmelavr_image_t *img = DA_NTHITEM (apriv->images, i);

		img_sort_ranges (img);
		if (img->canwrite) {
			/* this is one we can write out */
			int is_flash = (img->zone->tag == avrasm.tag_TEXTSEG);
			int is_eeprom = (img->zone->tag == avrasm.tag_EEPROMSEG);
			int fnlen = strlen (cgen->fname) - strlen (cgen->target->extn);
			char *outfname = (char *)smalloc (fnlen + 20);
			fhandle_t *fhan;

			strncpy (outfname, cgen->fname, fnlen);
			if (is_flash) {
				sprintf (outfname + fnlen, "flash.hex");
			} else if (is_eeprom) {
				sprintf (outfname + fnlen, "eeprom.hex");
			} else {
				sprintf (outfname + fnlen, "hex");
			}

			img_check_ranges (img, cgen);
			img_combine_ranges (img);
#if 0
fprintf (stderr, "atmelavr_be_do_codegen(): want to write to [%s]\n", outfname);
#endif
			fhan = fhandle_fopen (outfname, "wt");
			if (!fhan) {
				codegen_error (cgen, "failed to open [%s] for writing: %s", outfname, strerror (fhandle_lasterr (NULL)));
			} else {
				img_write_hexfile (img, fhan, cgen);
				fhandle_close (fhan);
			}
		}

		if (compopts.verbose > 1) {
			int z;

			nocc_message ("atmelavr: image zone=[%s], isize=%d (0x%x), canwrite=%d, %d range(s):", img->zone->tag->name,
					img->isize, img->isize, img->canwrite, DA_CUR (img->ranges));
			for (z=0; z<DA_CUR (img->ranges); z++) {
				imgrange_t *rng = DA_NTHITEM (img->ranges, z);

				nocc_message ("atmelavr:     [0x%-8.8x - 0x%8.8x]", rng->start, (rng->start + rng->size) - 1);
			}
		}
#if 0
fprintf (stderr, "atmelavr_be_do_codegen(): image zone=[%s], isize=%d, canwrite=%d, %d range(s):\n", img->zone->tag->name,
		img->isize, img->canwrite, DA_CUR (img->ranges));
{ int z; for (z=0; z<DA_CUR (img->ranges); z++) {
	imgrange_t *rng = DA_NTHITEM (img->ranges, z);
	fprintf (stderr, "    [0x%-8.8x - 0x%-8.8x]\n", rng->start, rng->start + rng->size);
}}
#endif
	}

outlab:
	/* remove extra stuff for constant propagation in labels */
	{
		compops_t *cops = tnode_removecompops (avrasm.tag_GLABEL->ndef->ops);

		avrasm.tag_GLABEL->ndef->ops = cops;
		/* Note: also removed from LLABEL */
	}


	return;
}
/*}}}*/


/*{{{  int atmelavr_init (void)*/
/*
 *	initialises the Atmel AVR back-end
 *	returns 0 on success, non-zero on error
 */
int atmelavr_init (void)
{
	/* register target */
	if (target_register (&atmelavr_target)) {
		nocc_error ("atmelavr_init(): failed to register target!");
		return 1;
	}

	return 0;
}
/*}}}*/
/*{{{  int atmelavr_shutdown (void)*/
/*
 *	shuts-down the Atmel AVR back-end
 *	returns 0 on success, non-zero on error
 */
int atmelavr_shutdown (void)
{
	/* unregister the target */
	if (target_unregister (&atmelavr_target)) {
		nocc_error ("atmelavr_shutdown(): failed to unregister target!");
		return 1;
	}

	return 0;
}
/*}}}*/


/*{{{  static int atmelavr_target_init (target_t *target)*/
/*
 *	initialises the Atmel AVR target
 *	returns 0 on success, non-zero on failure
 */
static int atmelavr_target_init (target_t *target)
{
	atmelavr_priv_t *apriv = atmelavr_newatmelavrpriv ();

#if 0
fprintf (stderr, "atmelavr_target_init(): here!\n");
#endif
	target->priv = (void *)apriv;

	/* setup compiler hooks, etc. */
	labelinfo_chook = tnode_lookupornewchook ("aavr:labelinfo");
	labelinfo_chook->chook_dumptree = atmelavr_labelinfohook_dumptree;
	labelinfo_chook->chook_free = atmelavr_labelinfohook_free;

	atmelavr_init_options (apriv);

	target->initialised = 1;
	return 0;
}
/*}}}*/


