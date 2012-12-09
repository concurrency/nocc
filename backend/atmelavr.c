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
#include "target.h"
#include "betrans.h"
#include "map.h"
#include "avrasm.h"
#include "atmelavr.h"
#include "codegen.h"
#include "allocate.h"

/*}}}*/
/*{{{  forward decls*/
static int atmelavr_target_init (target_t *target);

static int atmelavr_be_codegen_init (codegen_t *cgen, lexfile_t *srcfile);
static int atmelavr_be_codegen_final (codegen_t *cgen, lexfile_t *srcfile);

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
	.be_do_precode =	NULL,
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
	DYNARRAY (imgrange_t *, ranges);
} atmelavr_image_t;


typedef struct TAG_atmelavr_priv {
	lexfile_t *lastfile;
	DYNARRAY (atmelavr_image_t *, images);
} atmelavr_priv_t;

/*}}}*/
/*{{{  private data*/

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
/*{{{  static void atmelavr_be_do_codegen (tnode_t *tptr, codegen_t *cgen)*/
/*
 *	called to do the actual code-generation, given the parse-tree at this stage
 */
static void atmelavr_be_do_codegen (tnode_t *tptr, codegen_t *cgen)
{
#if 1
fprintf (stderr, "atmelavr_be_do_codegen(): here!\n");
#endif
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

	target->initialised = 1;
	return 0;
}
/*}}}*/


