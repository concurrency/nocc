/*
 *	atmelavr.c -- Atmel AVR back-end
 *	Copyright (C) 2012 Fred Barnes <frmb@kent.ac.uk>
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
	tnode_t *instr;					/* instruction node (presumably involving the label) */
	int offset;					/* byte offset in image where this should be assembled */
} aavr_labelfixup_t;

typedef struct TAG_aavr_labelinfo {
	atmelavr_image_t *img;				/* in which image this lives, NULL if none */
	int baddr;					/* byte-address, -1 if not defined yet */
	tnode_t *labins;				/* label defining instruction */
	DYNARRAY (aavr_labelfixup_t *, fixups);		/* instructions that require more assembly */
} aavr_labelinfo_t;

/*}}}*/
/*{{{  private data*/

static chook_t *labelinfo_chook = NULL;

/*}}}*/


/*{{{  void atmelavr_isetindent (FILE *stream, int indent)*/
/*
 *	set-indent for debugging output
 */
void atmelavr_isetindent (FILE *stream, int indent)
{
	int i;

	for (i=0; i<indent; i++) {
		fprintf (stream, "    ");
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

/*{{{  static aavr_labelfixup_t *atmelavr_newaavrlabelfixup (void)*/
/*
 *	creates a new aavr_labelfixup_t structure
 */
static aavr_labelfixup_t *atmelavr_newaavrlabelfixup (void)
{
	aavr_labelfixup_t *aalf = (aavr_labelfixup_t *)smalloc (sizeof (aavr_labelfixup_t));

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

/*{{{  static void atmelavr_labelinfohook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	dumps an aavr:labelinfo compiler hook
 */
static void atmelavr_labelinfohook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	aavr_labelinfo_t *aali = (aavr_labelinfo_t *)hook;
	int i;

	atmelavr_isetindent (stream, indent);
	fprintf (stream, "<chook:aavr:labelinfo img=\"%p\" baddr=\"%d\" labins=\"%p\">\n", aali->img, aali->baddr, aali->labins);
	for (i=0; i<DA_CUR (aali->fixups); i++) {
		aavr_labelfixup_t *aalf = DA_NTHITEM (aali->fixups, i);

		atmelavr_isetindent (stream, indent + 1);
		fprintf (stream, "<labelfixup instr=\"%s\" offset=\"%d\" />\n", aalf->instr->tag->name, aalf->offset);
	}
	atmelavr_isetindent (stream, indent);
	fprintf (stream, "</chook:aavr:labelinfo>\n");

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

/*{{{  static int insarg_to_constreg (tnode_t *arg, const int min, const int max, codegen_t *cgen)*/
/*
 *	extracts a constant register number (within limits) from an instruction operand
 */
static int insarg_to_constreg (tnode_t *arg, const int min, const int max, codegen_t *cgen)
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
/*{{{  static int insarg_to_constval (tnode_t *arg, const int min, const int max, codegen_t *cgen)*/
/*
 *	extracts a constant (immediate) value (within limits) from an instruction operand
 */
static int insarg_to_constval (tnode_t *arg, const int min, const int max, codegen_t *cgen)
{
	int val;

	if (!constprop_isconst (arg)) {
		codegen_node_error (cgen, arg, "expected constant value, found [%s]", arg->tag->name);
		return min;
	}
	val = constprop_intvalof (arg);

	if ((val < min) || (val > max)) {
		codegen_node_error (cgen, arg, "value %d out of range for instruction (expected [%d - %d])", val, min, max);
		return min;
	}
	return val;
}
/*}}}*/
/*{{{  static int insarg_to_constaddrdiff (tnode_t *arg, tnode_t *instr, const int myoffs, const int ibytes, const int min, const int max, codegen_t *cgen)*/
/*
 *	extracts a label address difference (in words)
 */
static int insarg_to_constaddrdiff (tnode_t *arg, tnode_t *instr, const int myoffs, const int ibytes, const int min, const int max, codegen_t *cgen)
{
#if 0
fprintf (stderr, "insarg_to_constaddrdiff(): arg=[%s], myoffs=%d\n", arg->tag->name, myoffs);
#endif
	if (arg->tag == avrasm.tag_GLABEL) {
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

			aalf->instr = instr;
			aalf->offset = myoffs;
			dynarray_add (aali->fixups, aalf);
			return 0;
		}
	}
	return 0;
}
/*}}}*/
/*{{{  static int insarg_to_constaddr (tnode_t *arg, tnode_t *instr, const int myoffs, const int min, const int max, codegen_t *cgen)*/
/*
 *	extracts a label address (in words)
 */
static int insarg_to_constaddr (tnode_t *arg, tnode_t *instr, const int myoffs, const int min, const int max, codegen_t *cgen)
{
	if (arg->tag == avrasm.tag_GLABEL) {
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

			aalf->instr = instr;
			aalf->offset = myoffs;
			dynarray_add (aali->fixups, aalf);
			return 0;
		}
	}
	return 0;
}
/*}}}*/
/*{{{  static int atmelavr_assemble_instr (atmelavr_image_t *img, int *offset, tnode_t *instr, avrinstr_tbl_t *inst, codegen_t *cgen)*/
/*
 *	called to assemble a single instruction at a specific place in the image
 *	returns number of bytes assembled, or that should be assembled
 */
static int atmelavr_assemble_instr (atmelavr_image_t *img, int *offset, tnode_t *instr, avrinstr_tbl_t *inst, codegen_t *cgen)
{
	int width = 0;
	int offs = *offset;
	int rd, rr, rs;
	int val, val2;

	switch (inst->ins) {
	case INS_ADD: /*{{{*/
		rd = insarg_to_constreg (tnode_nthsubof (instr, 1), 0, 31, cgen);	
		rr = insarg_to_constreg (tnode_nthsubof (instr, 2), 0, 31, cgen);
		img->image[offs++] = 0x0c | ((rr >> 3) & 0x02) | ((rd >> 4) & 0x01);
		img->image[offs++] = ((rd & 0x0f) << 4) | (rr & 0x0f);
		width = 2;
		break;
		/*}}}*/
	case INS_BRNE: /*{{{*/
		val = insarg_to_constaddrdiff (tnode_nthsubof (instr, 1), instr, offs, 2, -64, 63, cgen);
#if 0
fprintf (stderr, "atmelavr_assemble_instr(): INS_BRNE: offset is %d\n", val);
#endif
		img->image[offs++] = 0xf4 | ((val >> 5) & 0x03);
		img->image[offs++] = ((val << 3) & 0xf8) | 0x01;
		width = 2;
		break;
		/*}}}*/
	case INS_CALL: /*{{{*/
		val = insarg_to_constaddr (tnode_nthsubof (instr, 1), instr, offs, 0, (1 << 22) - 1, cgen);
		img->image[offs++] = 0x94 | ((val >> 21) & 0x01);
		img->image[offs++] = ((val >> 13) & 0xf0) | 0x0e | ((val >> 16) & 0x01);
		img->image[offs++] = (val >> 8) & 0xff;
		img->image[offs++] = val & 0xff;
		width = 4;
		break;
		/*}}}*/
	case INS_CBI: /*{{{*/
		val = insarg_to_constval (tnode_nthsubof (instr, 1), 0, 31, cgen);
		val2 = insarg_to_constval (tnode_nthsubof (instr, 2), 0, 7, cgen);
		img->image[offs++] = 0x98;
		img->image[offs++] = ((val & 0x1f) << 3) | (val2 & 0x07);
		width = 2;
		break;
		/*}}}*/
	case INS_CLI: /*{{{*/
		img->image[offs++] = 0x94;
		img->image[offs++] = 0xf8;
		width = 2;
		break;
		/*}}}*/
	case INS_DEC: /*{{{*/
		rd = insarg_to_constreg (tnode_nthsubof (instr, 1), 0, 31, cgen);
		img->image[offs++] = 0x94 | ((rd >> 4) & 0x01);
		img->image[offs++] = ((rd & 0x0f) << 4) | 0x0a;
		width = 2;
		break;
		/*}}}*/
	case INS_JMP: /*{{{*/
		val = insarg_to_constaddr (tnode_nthsubof (instr, 1), instr, offs, 0, (1 << 22) - 1, cgen);
		img->image[offs++] = 0x94 | ((val >> 21) & 0x01);
		img->image[offs++] = ((val >> 13) & 0xf0) | 0x0c | ((val >> 16) & 0x01);
		img->image[offs++] = (val >> 8) & 0xff;
		img->image[offs++] = val & 0xff;
		width = 4;
		break;
		/*}}}*/
	case INS_LDI: /*{{{*/
		rd = insarg_to_constreg (tnode_nthsubof (instr, 1), 16, 31, cgen);	
		val = insarg_to_constval (tnode_nthsubof (instr, 2), 0, 255, cgen);
		img->image[offs++] = 0xe0 | ((val >> 4) & 0x0f);
		img->image[offs++] = ((rd & 0x0f) << 4) | (val & 0x0f);
		width = 2;
		break;
		/*}}}*/
	case INS_NOP: /*{{{*/
		img->image[offs++] = 0x00;
		img->image[offs++] = 0x00;
		width = 2;
		break;
		/*}}}*/
	case INS_OUT: /*{{{*/
		val = insarg_to_constval (tnode_nthsubof (instr, 1), 0, 63, cgen);
		rr = insarg_to_constreg (tnode_nthsubof (instr, 2), 0, 31, cgen);
		img->image[offs++] = 0xb8 | ((val >> 3) & 0x06) | ((rr >> 4) & 0x01);
		img->image[offs++] = ((rr & 0x0f) << 4) | (val & 0x0f);
		width = 2;
		break;
		/*}}}*/
	case INS_POP: /*{{{*/
		rd = insarg_to_constreg (tnode_nthsubof (instr, 1), 0, 31, cgen);
		img->image[offs++] = 0x90 | ((rd >> 4) & 0x01);
		img->image[offs++] = ((rd & 0x0f) << 4) | 0x0f;
		width = 2;
		break;
		/*}}}*/
	case INS_PUSH: /*{{{*/
		rd = insarg_to_constreg (tnode_nthsubof (instr, 1), 0, 31, cgen);
		img->image[offs++] = 0x92 | ((rd >> 4) & 0x01);
		img->image[offs++] = ((rd & 0x0f) << 4) | 0x0f;
		width = 2;
		break;
		/*}}}*/
	case INS_RET: /*{{{*/
		img->image[offs++] = 0x95;
		img->image[offs++] = 0x08;
		width = 2;
		break;
		/*}}}*/
	case INS_RETI: /*{{{*/
		img->image[offs++] = 0x95;
		img->image[offs++] = 0x18;
		width = 2;
		break;
		/*}}}*/
	case INS_RJMP: /*{{{*/
		val = insarg_to_constaddrdiff (tnode_nthsubof (instr, 1), instr, offs, 2, -2048, 2047, cgen);
		img->image[offs++] = 0xc0 | ((val >> 8) & 0x0f);
		img->image[offs++] = val & 0xff;
		width = 2;
		break;
		/*}}}*/
	case INS_SBI: /*{{{*/
		val = insarg_to_constval (tnode_nthsubof (instr, 1), 0, 31, cgen);
		val2 = insarg_to_constval (tnode_nthsubof (instr, 2), 0, 7, cgen);
		img->image[offs++] = 0x9a;
		img->image[offs++] = ((val & 0x1f) << 3) | (val2 & 0x07);
		width = 2;
		break;
		/*}}}*/
	case INS_SEI: /*{{{*/
		img->image[offs++] = 0x94;
		img->image[offs++] = 0x78;
		width = 2;
		break;
		/*}}}*/
	case INS_SLEEP: /*{{{*/
		img->image[offs++] = 0x95;
		img->image[offs++] = 0x88;
		width = 2;
		break;
		/*}}}*/
	default:
		codegen_node_error (cgen, instr, "atmelavr_assemble_instr(): unhandled instruction \"%s\"", inst->str);
		return 0;
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
/*{{{  static int img_write_hexfile (atmelavr_image_t *img, FILE *fp, codegen_t *cgen)*/
/*
 *	writes out an image to a .hex (Intel style) file
 *	returns 0 on success, non-zero on failure (errors emitted)
 */
static int img_write_hexfile (atmelavr_image_t *img, FILE *fp, codegen_t *cgen)
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
		fprintf (fp, ":%2.2X:%4.4X00", left, addr);
		csum = (unsigned char)left;
		csum += (unsigned char)((addr >> 8) & 0xff);
		csum += (unsigned char)(addr & 0xff);
		csum += 0x00;

		for (k=0; k<left; k++) {
			int idx = (addr + k) ^ 0x01;				/* byte-swap in words */

			fprintf (fp, "%2.2X", img->image[idx]);
			csum += img->image[idx];
		}

		/* checksum */
		csum = (~csum) + 1;
		fprintf (fp, "%2.2X\n", csum);

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

		fprintf (fp, ":%2.2X%4.4X00", cs, addr);
		csum = (unsigned char)cs;
		csum += (unsigned char)((addr >> 8) & 0xff);
		csum += (unsigned char)(addr & 0xff);
		csum += 0x00;

		for (k=0; k<cs; k++) {
			int idx = (addr + k) ^ 0x01;				/* byte-swap in words */

			fprintf (fp, "%2.2X", img->image[idx]);
			csum += img->image[idx];
		}

		/* checksum */
		csum = (~csum) + 1;
		fprintf (fp, "%2.2X\n", csum);

		addr += cs;
	}

	/* finally, emit end-of-file record */
	fprintf (fp, ":00000001FF\n");

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

	if (apriv->mcu) {
		codegen_write_fmt (cgen, "# using MCU \"%s\"\n", apriv->mcu->name);
	}

	return;
}
/*}}}*/
/*{{{  static int atmelavr_do_assemble (codegen_t *cgen, atmelavr_image_t *img, tnode_t *contents)*/
/*
 *	called to assemble stuff into a memory image;  codegen to listing file
 *	returns 0 on success, non-zero on failure (will emit errors/warnings)
 */
static int atmelavr_do_assemble (codegen_t *cgen, atmelavr_image_t *img, tnode_t *contents)
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
			codegen_node_warning (cgen, items[i], "origin not specified for [%s], assuming 0", items[i]->tag->name);
			imr = atmelavr_newimgrange ();
			imr->start = 0;
			imr->size = 0;
			genoffset = 0;
		}

		if (items[i]->tag == avrasm.tag_GLABELDEF) {
			/*{{{  planting label definition*/
			aavr_labelinfo_t *aali;
			int j;

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

#if 0
fprintf (stderr, "atmelavr_do_assemble(): planted label definition at address %d, got %d fixups..\n", aali->baddr, DA_CUR (aali->fixups));
#endif

			for (j=0; j<DA_CUR (aali->fixups); j++) {
				aavr_labelfixup_t *aalf = DA_NTHITEM (aali->fixups, j);

				if (aalf->instr->tag == avrasm.tag_INSTR) {
					avrinstr_tbl_t *inst = (avrinstr_tbl_t *)tnode_nthhookof (aalf->instr, 0);
					int bytesin, offset = aalf->offset;

					bytesin = atmelavr_assemble_instr (img, &offset, aalf->instr, inst, cgen);
				} else {
					codegen_node_error (cgen, aalf->instr, "cannot fixup for label, unknown node type [%s]", aalf->instr->tag->name);
				}

				atmelavr_freeaavrlabelfixup (aalf);
			}
			dynarray_trash (aali->fixups);
			dynarray_init (aali->fixups);


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

		if (items[i]->tag == avrasm.tag_CONST) {
			/*{{{  constant data, into eeprom or text (must be writable)*/

#if 1
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
			bytesin = atmelavr_assemble_instr (img, &genoffset, items[i], inst, cgen);
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
			res = atmelavr_do_assemble (cgen, img, tnode_nthsubof (thisnode, 1));
			if (res) {
				codegen_error (cgen, "failed to assemble, giving up");
				return;
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
			/* FIXME: really need a proper file I/O abstraction in the compiler.. */
			{
				FILE *fp;

				fp = fopen (outfname, "wt");
				if (!fp) {
					codegen_error (cgen, "failed to open [%s] for writing: %s", outfname, strerror (errno));
				} else {
					img_write_hexfile (img, fp, cgen);
					fclose (fp);
				}
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

#if 1
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


