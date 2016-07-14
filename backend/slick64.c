/*
 *	slick64.c -- top-level interface to the x86-64 target and "slick" scheduler
 *	Copyright (C) 2016 Fred Barnes, University of Kent <frmb@kent.ac.uk>
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
#include <sys/wait.h>
#ifdef HAVE_TIME_H
#include <time.h>
#endif 
#include <errno.h>

#include "nocc.h"
#include "support.h"
#include "version.h"
#include "origin.h"
#include "fhandle.h"
#include "tnode.h"
#include "opts.h"
#include "lexer.h"
#include "parser.h"
#include "constprop.h"
#include "treeops.h"
#include "langops.h"
#include "names.h"
#include "typecheck.h"
#include "target.h"
#include "betrans.h"
#include "map.h"
#include "transputer.h"
#include "codegen.h"
#include "allocate.h"
#include "slick64.h"
#include "parsepriv.h"

/*}}}*/
/*{{{  target_t for this target*/
target_t slick64_target = {
	.initialised =		0,
	.name =			"slick64",
	.tarch = 		"x86-64",
	.tvendor =		"slick",
	.tos =			"linux",
	.desc =			"x86-64 assembler / slick",
	.extn =			"s",
	.tcap = {
		.can_do_fp = 1,
		.can_do_dmem = 1,
	},
	.bws = {
		.ds_min = 32,
		.ds_io = 40,
		.ds_altio = 40,
		.ds_wait = 56,
		.ds_max = 56
	},
	.aws = {
		.as_alt = 8,
		.as_par = 24,
	},

	.chansize =		8,
	.charsize =		1,
	.intsize =		8,
	.pointersize =		8,
	.slotsize =		8,
	.structalign =		0,
	.maxfuncreturn =	0,
	.skipallocate =		0,

	.tag_NAME =		NULL,
	.tag_NAMEREF =		NULL,
	.tag_BLOCK =		NULL,
	.tag_CONST =		NULL,
	.tag_INDEXED =		NULL,
	.tag_BLOCKREF =		NULL,
	.tag_STATICLINK =	NULL,
	.tag_RESULT =		NULL,

	.init =			slick64_target_init,
	.newname =		slick64_name_create,
	.newnameref =		slick64_nameref_create,
	.newblock =		slick64_block_create,
	.newconst =		slick64_const_create,
	.newindexed =		NULL,
	.newblockref =		slick64_blockref_create,
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
	.be_codegen_init =	slick64_be_codegen_init,
	.be_codegen_final =	slick64_be_codegen_final,

	.be_precode_seenproc =	NULL,

	.be_do_betrans =	slick64_do_betrans,
	.be_do_premap =		slick64_do_premap,
	.be_do_namemap =	slick64_do_namemap,
	.be_do_bemap =		slick64_do_bemap,
	.be_do_preallocate =	slick64_do_preallocate,
	.be_do_precode =	slick64_do_precode,
	.be_do_codegen =	slick64_do_codegen,

	.priv =		NULL
};

/*}}}*/
/*{{{  private types*/
typedef struct TAG_slick64_namehook {
	int lexlevel;		/* lexical level */
	int alloc_wsh;		/* bytes of high-workspace */
	int alloc_wsl;		/* bytes of low-workspace */
	int typesize;		/* size of actual type (if known) */
	int indir;		/* indirection count (0 = real-thing, 1 = pointer, 2 = pointer-pointer, etc.) */
	int ws_offset;		/* workspace offset */
	typecat_e typecat;	/* type category */
} slick64_namehook_t;

typedef struct TAG_slick64_blockhook {
	int lexlevel;		/* lexical level */
	int alloc_ws;		/* workspace requirements (in bytes) */
	int static_adjust;	/* adjustment from initial WS [top of] to where we run (skipping params/return-addr/etc.) */
	int ws_offset;		/* workspace offset for the block (covering static_adjust plus locals and/or [temp]) */
	int entrylab;		/* entry-point label */
	int addstaticlink;	/* whether it needs an explicit staticlink (PROC calls only) */
	int addfbp;		/* whether it needs a "fork" barrier */
} slick64_blockhook_t;

typedef struct TAG_slick64_blockrefhook {
	tnode_t *block;
} slick64_blockrefhook_t;

typedef struct TAG_slick64_consthook {
	void *byteptr;
	int size;		/* constant size (bytes) */
	int label;
	int labrefs;		/* number of references to this */
	tnode_t *orgnode;
	typecat_e typecat;	/* type category for constant */
} slick64_consthook_t;

typedef struct TAG_slick64_priv {
	ntdef_t *tag_PRECODE;
	ntdef_t *tag_CONSTREF;
	ntdef_t *tag_PROGENTRY;
	ntdef_t *tag_FBP;	/* fork barrier pointer */
	tnode_t *precodelist;
	name_t *toplevelname;

	chook_t *mapchook;
	chook_t *resultsubhook;

	lexfile_t *lastfile;

	struct {
		unsigned int stoperrornode:1;
	} options;
} slick64_priv_t;

typedef struct TAG_slick64_resultsubhook {
	int eval_regs;
	int eval_fregs;
	int result_regs;
	int result_fregs;
	DYNARRAY (tnode_t **, sublist);
} slick64_resultsubhook_t;


/*}}}*/


/*{{{  void slick64_isetindent (fhandle_t *stream, int indent)*/
/*
 *	set-indent for debugging output
 */
void slick64_isetindent (fhandle_t *stream, int indent)
{
	int i;

	for (i=0; i<indent; i++) {
		fhandle_printf (stream, "    ");
	}
	return;
}
/*}}}*/


/*{{{  static int slick64_opthandler_flag (cmd_option_t *opt, char ***argwalk, int *argleft)*/
/*
 *	option handler for this target's options
 *	returns 0 on success, non-zero on failure
 */
static int slick64_opthandler_flag (cmd_option_t *opt, char ***argwalk, int *argleft)
{
	int optv = (int)((uint64_t)opt->arg);
	int flagval = 1;
	slick64_priv_t *spriv = (slick64_priv_t *)slick64_target.priv;

	if (optv < 0) {
		flagval = 0;
		optv = -optv;
	}
	switch (optv) {
	case 1:
		spriv->options.stoperrormode = flagval;
		break;
	default:
		return -1;
	}
	return 0;
}
/*}}}*/
/*{{{  static int slick64_init_options (slick64_priv_t *spriv)*/
/*
 *	initialises options for the slick64 (x86-64 with 'slick' scheduler) back-end
 *	returns 0 on success, non-zero on failure
 */
static int slick64_init_options (slick64_priv_t *spriv)
{
	opts_add ("stoperrormode", '\0', slick64_opthandler_flag, (void *)1, "1use stop error-mode");
	opts_add ("halterrormode", '\0', slick64_opthandler_flag, (void *)-1, "1use halt error-mode");

	return 0;
}
/*}}}*/


/*{{{  int slick64_init (void)*/
/*
 *	initialises the slick64 back-end
 *	returns 0 on success, non-zero on error
 */
int slick64_init (void)
{
	if (target_register (&slick64_target)) {
		nocc_error ("slick64_init(): failed to register target!");
		return 1;
	}
	return 0;
}
/*}}}*/
/*{{{  int slick64_shutdown (void)*/
/*
 *	shuts-down the slick64 back-end
 *	returns 0 on success, non-zero on error
 */
int slick64_shutdown (void)
{
	if (target_unregister (&slick64_target)) {
		nocc_error ("slick64_init(): failed to unregister target!");
		return 1;
	}
	return 0;
}
/*}}}*/


/*{{{  */
/*
 *	initialises the slick64 target
 *	returns 0 on success, non-zero on error
 */
static int slick64_target_init (target_t *target)
{
	tndef_t *tnd;
	compops_t *cops;
	langops_t *lops;
	slick64_priv_t *spriv;
	int i;

	if (target->initialised) {
		nocc_internal ("slick64_target_init(): already initialised!");
		return 1;
	}

	spriv = (slick64_priv_t *)smalloc (sizeof (slick64_priv_t));

	spriv->precodelist = NULL;
	spriv->toplevelname = NULL;

	spriv->mapchook = tnode_lookupornewchook ("map:mapnames");
	spriv->resultsubhook = tnode_lookupornewchook ("slick64:resultsubhook");
	spriv->resultsubhook->chook_dumptree = NULL;			/* FIXME! */
	spriv->resultsubhook->chook_free = NULL;			/* FIXME! */

	spriv->lastfile = NULL;
	spriv->options.stoperrormode = 0;			/* halt error-mode by default */

	target->priv = (void *)spriv;

	slick64_init_options (spriv);


	/* setup back-end nodes */
	/*{{{  slick64:name -- SLICK64NAME*/
	i = -1;
	tnd = tnode_newnodetype ("slick64:name", &i, 2, 0, 1, TNF_NONE);		/* subnodes: original name, in-scope body; hooks: slick64_namehook_t */

	/*}}}*/

}
/*}}}*/


