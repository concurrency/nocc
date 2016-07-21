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
/*{{{  forward declarations*/

static int slick64_target_init (target_t *target);
static tnode_t *slick64_name_create (tnode_t *fename, tnode_t *body, map_t *mdata, int asize_wsh, int asize_wsl, int asize_vs, int asize_ms, int tsize, int ind);
static tnode_t *slick64_nameref_create (tnode_t *bename, map_t *mdata);
static tnode_t *slick64_block_create (tnode_t *body, map_t *mdata, tnode_t *slist, int lexlevel);
static tnode_t *slick64_const_create (tnode_t *val, map_t *mdata, void *data, int size, typecat_e typecat);
static tnode_t *slick64_blockref_create (tnode_t *block, tnode_t *body, map_t *mdata);

static int slick64_be_codegen_init (codegen_t *cgen, lexfile_t *srcfile);
static int slick64_be_codegen_final (codegen_t *cgen, lexfile_t *srcfile);

static void slick64_coder_setlabel (codegen_t *cgen, int lbl);
static void slick64_coder_constblock (codegen_t *cgen, void *ptr, int size);


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

	.be_do_betrans =	NULL,
	.be_do_premap =		NULL,
	.be_do_namemap =	NULL,
	.be_do_bemap =		NULL,
	.be_do_preallocate =	NULL,
	.be_do_precode =	NULL,
	.be_do_codegen =	NULL,

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
		unsigned int stoperrormode:1;
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

/*{{{  slick64_namehook_t routines*/
/*{{{  static void slick64_namehook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)*/
/*
 *	dumps hook data for debugging
 */
static void slick64_namehook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)
{
	slick64_namehook_t *nh = (slick64_namehook_t *)hook;

	slick64_isetindent (stream, indent);
	fhandle_printf (stream, "<namehook addr=\"%p\" lexlevel=\"%d\", allocwsh=\"%d\" allocwsl=\"%d\" typesize=\"%d\" indir=\"%d\" wsoffset=\"%d\" typecat=\"0x%16.16lx\" />\n",
			nh, nh->lexlevel, nh->alloc_wsh, nh->alloc_wsl, nh->typesize, nh->indir, nh->ws_offset, (uint64_t)nh->typecat);
	return;
}
/*}}}*/
/*{{{  static slick64_namehook_t *slick64_namehook_create (int ll, int asize_wsh, int asize_wsl, int tsize, int ind)*/
/*
 *	creates a name-hook
 */
static slick64_namehook_t *slick64_namehook_create (int ll, int asize_wsh, int asize_wsl, int tsize, int ind)
{
	slick64_namehook_t *nh = (slick64_namehook_t *)smalloc (sizeof (slick64_namehook_t));

	nh->lexlevel = ll;
	nh->alloc_wsh = asize_wsh;
	nh->alloc_wsl = asize_wsl;
	nh->typesize = tsize;
	nh->indir = ind;
	nh->ws_offset = -1;
	nh->typecat = TYPE_NOTTYPE;

	return nh;
}
/*}}}*/

/*}}}*/
/*{{{  slick64_blockhook_t routines*/
/*{{{  static void slick64_blockhook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)*/
/*
 *	dumps hook for debugging
 */
static void slick64_blockhook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)
{
	slick64_blockhook_t *bh = (slick64_blockhook_t *)hook;

	slick64_isetindent (stream, indent);
	fhandle_printf (stream, "<blockhook addr=\"%p\" lexlevel=\"%d\" allocws=\"%d\" adjust=\"%d\" wsoffset=\"%d\" entrylab=\"%d\" addstaticlink=\"%d\" addfbp=\"%d\" />\n",
			bh, bh->lexlevel, bh->alloc_ws, bh->static_adjust, bh->ws_offset, bh->entrylab, bh->addstaticlink, bh->addfbp);
	return;
}
/*}}}*/
/*{{{  static slick64_blockhook_t *slick64_blockhook_create (int ll)*/
/*
 *	creates a block-hook
 */
static slick64_blockhook_t *slick64_blockhook_create (int ll)
{
	slick64_blockhook_t *bh = (slick64_blockhook_t *)smalloc (sizeof (slick64_blockhook_t));

	bh->lexlevel = ll;
	bh->alloc_ws = 0;
	bh->static_adjust = 0;
	bh->ws_offset = 0;
	bh->entrylab = 0;
	bh->addstaticlink = 0;
	bh->addfbp = 0;

	return bh;
}
/*}}}*/

/*}}}*/
/*{{{  slick64_blockrefhook_t routines*/
/*{{{  static void slick64_blockrefhook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)*/
/*
 *	dumps hook (debugging)
 */
static void slick64_blockrefhook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)
{
	slick64_blockrefhook_t *brh = (slick64_blockrefhook_t *)hook;
	tnode_t *blk = brh->block;

	if (blk && parser_islistnode (blk)) {
		int nitems, i;
		tnode_t **blks = parser_getlistitems (blk, &nitems);

		slick64_isetindent (stream, indent);
		fhandle_printf (stream, "<blockrefhook addr=\"%p\" block=\"%p\" nblocks=\"%d\" blocks=\"", brh, blk, nitems);
		for (i=0; i<nitems; i++ ) {
			if (i) {
				fhandle_printf (stream, ",");
			}
			fhandle_printf (stream, "%p", blks[i]);
		}
		fhandle_printf (stream, "\" />\n");
	} else {
		slick64_isetindent (stream, indent);
		fhandle_printf (stream, "<blockrefhook addr=\"%p\" block=\"%p\" />\n", brh, blk);
	}

	return;
}
/*}}}*/
/*{{{  static slick64_blockrefhook_t *slick64_blockrefhook_create (tnode_t *block)*/
/*
 *	creates a new hook (populated)
 */
static slick64_blockrefhook_t *slick64_blockrefhook_create (tnode_t *block)
{
	slick64_blockrefhook_t *brh = (slick64_blockrefhook_t *)smalloc (sizeof (slick64_blockrefhook_t));

	brh->block = block;

	return brh;
}
/*}}}*/

/*}}}*/
/*{{{  slick64_consthook_t routines*/
/*{{{  static void slick64_consthook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)*/
/*
 *	dumps hook for debugging
 */
static void slick64_consthook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)
{
	slick64_consthook_t *ch = (slick64_consthook_t *)hook;

	slick64_isetindent (stream, indent);
	fhandle_printf (stream, "<consthook addr=\"%p\" data=\"%p\" size=\"%d\" label=\"%d\" labrefs=\"%d\" orgnode=\"%p\" orgnodetag=\"%s\" typecat=\"0x%16.16lx\" />\n",
			ch, ch->byteptr, ch->size, ch->label, ch->labrefs,
			ch->orgnode, ch->orgnode ? ch->orgnode->tag->name : "", (uint64_t)ch->typecat);
	return;
}
/*}}}*/
/*{{{  static slick64_consthook_t *slick64_consthook_create (void *ptr, int size)*/
/*
 *	creates a constant-hook
 */
static slick64_consthook_t *slick64_consthook_create (void *ptr, int size)
{
	slick64_consthook_t *ch = (slick64_consthook_t *)smalloc (sizeof (slick64_consthook_t));

	ch->byteptr = mem_ndup (ptr, size);
	ch->size = size;
	ch->label = -1;
	ch->labrefs = 0;
	ch->orgnode = NULL;
	ch->typecat = TYPE_NOTTYPE;

	return ch;
}
/*}}}*/

/*}}}*/

/*{{{  static tnode_t *slick64_name_create (tnode_t *fename, tnode_t *body, map_t *mdata, int asize_wsh, int asize_wsl, int asize_vs, int asize_ms, int tsize, int ind)*/
/*
 *	allocates a new back-end name-node
 */
static tnode_t *slick64_name_create (tnode_t *fename, tnode_t *body, map_t *mdata, int asize_wsh, int asize_wsl, int asize_vs, int asize_ms, int tsize, int ind)
{
	target_t *xt = mdata->target;		/* must be us! */
	tnode_t *name;
	slick64_namehook_t *nh;
	tnode_t *blk = map_thisblock_cll (mdata);

	nh = slick64_namehook_create (mdata->lexlevel, asize_wsh, asize_wsl, tsize, ind);
	name = tnode_create (xt->tag_NAME, NULL, fename, body, (void *)nh);

	if (blk) {
		slick64_blockhook_t *bh = (slick64_blockhook_t *)tnode_nthhookof (blk, 0);

		/* FIXME: might need to add static-link..? */
	}

	return name;
}
/*}}}*/
/*{{{  static tnode_t *slick64_nameref_create (tnode_t *bename, map_t *mdata)*/
/*
 *	allocates a new back-end name-ref-node
 */
static tnode_t *slick64_nameref_create (tnode_t *bename, map_t *mdata)
{
	slick64_namehook_t *nh, *be_nh;
	slick64_blockhook_t *bh;
	tnode_t *name, *fename;
	tnode_t *blk = map_thisblock_cll (mdata);

	if (!blk) {
		nocc_internal ("slick64_nameref_create(): reference to name outside of block");
		return NULL;
	}
	bh = (slick64_blockhook_t *)tnode_nthhookof (blk, 0);
	be_nh = (slick64_namehook_t *)tnode_nthhookof (bename, 0);
#if 0
fprintf (stderr, "slick64_nameref_create (): referenced lexlevel=%d, map lexlevel=%d, enclosing block lexlevel=%d\n", be_nh->lexlevel, mdata->lexlevel, bh->lexlevel);
#endif
	if (be_nh->lexlevel < bh->lexlevel) {
		/*{{{  need a static-link to get at this one*/
		int i;

		for (i=bh->lexlevel; i>be_nh->lexlevel; i--) {
			tnode_t *llblk = map_thisblock_ll (mdata, i);
			slick64_blockhook_t *llbh = (slick64_blockhook_t *)tnode_nthhookof (llblk, 0);

			if (llbh) {
				llbh->addstaticlink = 1;
			} else {
				nocc_warning ("slick64_nameref_create(): no block at lexlevel %d", i);
			}
		}
		/*}}}*/
	}
	nh = slick64_namehook_create (mdata->lexlevel, 0, 0, be_nh->typesize, be_nh->indir);
	nh->typecat = be_nh->typecat;				/* copy over type-category */

	fename = tnode_nthsubof (bename, 0);
	name = tnode_create (mdata->target->tag_NAMEREF, NULL, fename, (void *)nh);

	return name;
}
/*}}}*/
/*{{{  static tnode_t *slick64_block_create (tnode_t *body, map_t *mdata, tnode_t *slist, int lexlevel)*/
/*
 *	creates a new back-end block
 */
static tnode_t *slick64_block_create (tnode_t *body, map_t *mdata, tnode_t *slist, int lexlevel)
{
	slick64_blockhook_t *bh;
	tnode_t *blk;

	bh = slick64_blockhook_create (lexlevel);
	blk = tnode_create (mdata->target->tag_BLOCK, NULL, body, slist, (void *)bh);

	return blk;
}
/*}}}*/
/*{{{  static tnode_t *slick64_const_create (tnode_t *val, map_t *mdata, void *data, int size, typecat_e typecat)*/
/*
 *	creates a new back-end constant
 */
static tnode_t *slick64_const_create (tnode_t *val, map_t *mdata, void *data, int size, typecat_e typecat)
{
	slick64_consthook_t *ch;
	tnode_t *cnst;

	ch = slick64_consthook_create (data, size);
	ch->orgnode = val;
	ch->typecat = typecat;
	cnst = tnode_create (mdata->target->tag_CONST, NULL, val, (void *)ch);

	return cnst;
}
/*}}}*/
/*{{{  static tnode_t *slick64_blockref_create (tnode_t *block, tnode_t *body, map_t *mdata)*/
/*
 *	creates a new back-end block reference node (used for procedure instances and the like)
 */
static tnode_t *slick64_blockref_create (tnode_t *block, tnode_t *body, map_t *mdata)
{
	slick64_blockrefhook_t *brh = slick64_blockrefhook_create (block);
	tnode_t *blockref;

	blockref = tnode_create (slick64_target.tag_BLOCKREF, NULL, body, (void *)brh);

	return blockref;
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


/*{{{  static int slick64_bytesfor_name (langops_t *lops, tnode_t *name, target_t *target)*/
/*
 *	used to get the type-size of a back-end name
 *	returns type-size or -1 if not known
 */
static int slick64_bytesfor_name (langops_t *lops, tnode_t *name, target_t *target)
{
	slick64_namehook_t *nh;

	if (name->tag != slick64_target.tag_NAME) {
		return -1;
	}
	nh = (slick64_namehook_t *)tnode_nthhookof (name, 0);
	if (!nh) {
		return -1;
	}
	return nh->typesize;
}
/*}}}*/

/*{{{  static int slick64_precode_const (compops_t *cops, tnode_t **cnst, codegen_t *cgen)*/
/*
 *	does pre-code for a back-end constant
 *	returns 0 to stop walk, 1 to continue
 */
static int slick64_precode_const (compops_t *cops, tnode_t **cnst, codegen_t *cgen)
{
	slick64_priv_t *spriv = (slick64_priv_t *)cgen->target->priv;
	slick64_consthook_t *ch = (slick64_consthook_t *)tnode_nthhookof (*cnst, 0);
	tnode_t *cref;

	if (ch->label <= 0) {
		ch->label = codegen_new_label (cgen);
	}

	/* move this into pre-codes and leave a reference */
	parser_addtolist (spriv->precodelist, *cnst);
	cref = tnode_create (spriv->tag_CONSTREF, NULL, (void *)ch);
	tnode_promotechooks (*cnst, cref);
	*cnst = cref;
	return 1;
}
/*}}}*/
/*{{{  static int slick64_codegen_const (compops_t *cops, tnode_t *cnst, codegen_t *cgen)*/
/*
 *	does code-generation for a constant -- these have been pulled out in front of the program
 *	returns 0 to stop walk, 1 to continue
 */
static int slick64_codegen_const (compops_t *cops, tnode_t *cnst, codegen_t *cgen)
{
	slick64_consthook_t *ch = (slick64_consthook_t *)tnode_nthhookof (cnst, 0);

#if 0
fprintf (stderr, "slick64_codegen_const(): ch->label = %d, ch->labrefs = %d\n", ch->label, ch->labrefs);
#endif
	if (ch->label > 0) {
		slick64_coder_setlabel (cgen, ch->label);
		slick64_coder_constblock (cgen, ch->byteptr, ch->size);
	}
	return 0;
}
/*}}}*/

/*{{{  static int slick64_codegen_nameref (compops_t *cops, tnode_t *nameref, codegen_t *cgen)*/
/*
 *	generates code to load a name -- usually happens inside a RESULT
 *	return 0 to stop walk, 1 to continue
 */
static int slick64_codegen_nameref (compops_t *cops, tnode_t *nameref, codegen_t *cgen)
{
	codegen_callops (cgen, loadname, nameref, 0);
	return 0;
}
/*}}}*/
/*{{{  static tnode_t *slick64_gettype_nameref (langops_t *lops, tnode_t *node, tnode_t *defaulttype)*/
/*
 *	returns the type of a name reference
 *	returns type on success, NULL on failure
 */
static tnode_t *slick64_gettype_nameref (langops_t *lops, tnode_t *node, tnode_t *defaulttype)
{
	/* transparent */
	return typecheck_gettype (tnode_nthsubof (node, 0), defaulttype);
}
/*}}}*/

/*{{{  static int slick64_preallocate_block (compops_t *cops, tnode_t *blk, target_t *target)*/
/*
 *	does pre-allocation for a back-end block
 *	returns 0 to stop walk, 1 to continue
 */
static int slick64_preallocate_block (compops_t *cops, tnode_t *blk, target_t *target)
{
	slick64_priv_t *spriv = (slick64_priv_t *)target->priv;

	if (blk->tag == target->tag_BLOCK) {
		slick64_blockhook_t *bh = (slick64_blockhook_t *)tnode_nthhookof (blk, 0);

#if 0
fprintf (stderr, "slick64_preallocate_block(): preallocating block, ws=%d, vs=%d, ms=%d\n", bh->alloc_ws, bh->alloc_vs, bh->alloc_ms);
#endif
		if (bh->addstaticlink) {
			tnode_t **stptr = tnode_nthsubaddr (blk, 1);
			slick64_namehook_t *nh;
			tnode_t *name;

#if 0
fprintf (stderr, "slick64_preallocate_block(): adding static-link..\n");
#endif
			if (!*stptr) {
				*stptr = parser_newlistnode (NULL);
			} else if (!parser_islistnode (*stptr)) {
				tnode_t *slist = parser_newlistnode (NULL);

				parser_addtolist (slist, *stptr);
				*stptr = slist;
			}

			nh = slick64_namehook_create (bh->lexlevel, target->pointersize, 0, target->pointersize, 0);
			name = tnode_create (target->tag_NAME, NULL, tnode_create (target->tag_STATICLINK, NULL), NULL, (void *)nh);

			parser_addtolist_front (*stptr, name);
		}
	}

	return 1;
}
/*}}}*/
/*{{{  static int slick64_precode_block (compops_t *cops, tnode_t **tptr, codegen_t *cgen)*/
/*
 *	does pre-code generation for a back-end block
 *	return 0 to stop walk, 1 to continue it
 */
static int slick64_precode_block (compops_t *cops, tnode_t **tptr, codegen_t *cgen)
{
	slick64_blockhook_t *bh = (slick64_blockhook_t *)tnode_nthhookof (*tptr, 0);

	if ((*tptr)->tag != slick64_target.tag_BLOCK) {
		nocc_internal ("slick64_precode_block(): block not back-end BLOCK, was [%s]", (*tptr)->tag->name);
	}
	if (!bh->entrylab) {
		/* give it an entry-point label */
		bh->entrylab = codegen_new_label (cgen);
	}
	return 1;
}
/*}}}*/
/*{{{  static int slick64_codegen_block (compops_t *cops, tnode_t *blk, codegen_t *cgen)*/
/*
 *	does code generation for a back-end block
 *	return 0 to stop walk, 1 to continue it
 */
static int slick64_codegen_block (compops_t *cops, tnode_t *blk, codegen_t *cgen)
{
	slick64_priv_t *spriv = (slick64_priv_t *)cgen->target->priv;
	int ws_size, vs_size, ms_size;
	int ws_offset, adjust;
	int elab, lexlevel;

	if (blk->tag != slick64_target.tag_BLOCK) {
		nocc_internal ("slick64_codegen_block(): block not back-end BLOCK, was [%s]", blk->tag->name);
		return 0;
	}
	cgen->target->be_getblocksize (blk, &ws_size, &ws_offset, &vs_size, &ms_size, &adjust, &elab);
	lexlevel = cgen->target->be_blocklexlevel (blk);
	dynarray_setsize (cgen->be_blks, lexlevel + 1);
	DA_SETNTHITEM (cgen->be_blks, lexlevel, blk);

	slick64_cgstate_newpush (cgen);

	if (elab) {
		codegen_callops (cgen, setlabel, elab);
	}
	codegen_callops (cgen, wsadjust, -(ws_offset - adjust));

	codegen_subcodegen (tnode_nthsubof (blk, 0), cgen);
	codegen_callops (cgen, wsadjust, (ws_offset - adjust));

	DA_SETNTHITEM (cgen->be_blks, lexlevel, NULL);
	dynarray_setsize (cgen->be_blks, lexlevel);

	slick64_cgstate_popfree (cgen);

	return 0;
}
/*}}}*/


/*{{{  static void slick64_coder_setlabel (codegen_t *cgen, int lbl)*/
/*
 *	sets a numeric label
 */
static void slick64_coder_setlabel (codegen_t *cgen, int lbl)
{
	codegen_write_fmt (cgen, ".setlabel\t%d\n", lbl);
	return;
}
/*}}}*/
/*{{{  static void slick64_coder_constblock (codegen_t *cgen, void *ptr, int size)*/
/*
 *	generates a constant block of data
 */
static void slick64_coder_constblock (codegen_t *cgen, void *ptr, int size)
{
	int i;
	char *buffer = (char *)smalloc (128);

	for (i=0; i<size; i+=16) {
		int j, slen;

		slen = sprintf (buffer, ".byte\t");
		for (j=0; ((i+j) < size) && (j<16); j++) {
			slen += sprintf (buffer + slen, "%s0x%2.2x", (!j ? "" : ", "), *(unsigned char *)(ptr + i + j));
		}
		slen += sprintf (buffer + slen, "\n");

		codegen_write_bytes (cgen, buffer, slen);
	}
	sfree (buffer);
	return;
}
/*}}}*/



/*{{{  static int slick64_be_codegen_init (codegen_t *cgen, lexfile_t *srcfile)*/
/*
 *	initialises the back-end code generation for the slick/x86-64 target
 */
static int slick64_be_codegen_init (codegen_t *cgen, lexfile_t *srcfile)
{
	slick64_priv_t *spriv = (slick64_priv_t *)cgen->target->priv;
	coderops_t *cops;
	char hostnamebuf[128];
	char timebuf[128];

	codegen_write_fmt (cgen, "/*\n *\t%s\n", cgen->fname);
	codegen_write_fmt (cgen, " *\tcompiled from %s\n", srcfile->filename ?: "(unknown)");
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
	codegen_write_fmt (cgen, " *\ton host %s at %s\n", hostnamebuf, timebuf);
	codegen_write_fmt (cgen, " *\tsource language: %s, target: %s\n", parser_langname (srcfile) ?: "(unknown)", compopts.target_str);
	codegen_write_string (cgen, " */\n\n");

	cops = (coderops_t *)smalloc (sizeof (coderops_t));
	memset ((void *)cops, 0, sizeof (coderops_t));

	cgen->cops = cops;

	/*
	 *	create pre-code node if not already here -- constants go wherever (at end will do)
	 */
	if (!spriv->precodelist) {
		tnode_t *precode = tnode_create (spriv->tag_PRECODE, NULL, parser_newlistnode (SLOCN (srcfile)), *(cgen->cinsertpoint));

		*(cgen->cinsertpoint) = precode;
		spriv->precodelist = tnode_nthsubof (precode, 0);
	}

	if (!compopts.notmainmodule) {
		/* FIXME: generate entry-point probably */
	}

	return 0;
}
/*}}}*/
/*{{{  static int slick64_be_codegen_final (codegen_t *cgen, lexfile_t *srcfile)*/
/*
 *	shuts-down back-end code generation for slick/x86-64 target
 */
static int slick64_be_codegen_final (codegen_t *cgen, lexfile_t *srcfile)
{
	sfree (cgen->cops);
	cgen->cops = NULL;

	codegen_write_string (cgen, "\n/*\n *\tend of compilation\n */\n\n");
	return 0;
}
/*}}}*/



/*{{{  static int slick64_target_init (target_t *target)*/
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
	tnd->hook_dumptree = slick64_namehook_dumptree;
	cops = tnode_newcompops ();
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "bytesfor", 2, LANGOPTYPE (slick64_bytesfor_name));
	tnd->lops = lops;

	i = -1;
	target->tag_NAME = tnode_newnodetag ("SLICK64NAME", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  slick64:nameref -- SLICK64NAMEREF*/
	i = -1;
	tnd = tnode_newnodetype ("slick64:nameref", &i, 1, 0, 1, TNF_NONE);		/* subnodes: original name; hooks: slick64_namehook_t */
	tnd->hook_dumptree = slick64_namehook_dumptree;
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (slick64_codegen_nameref));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "gettype", 2, LANGOPTYPE (slick64_gettype_nameref));
	tnd->lops = lops;

	i = -1;
	target->tag_NAMEREF = tnode_newnodetag ("SLICK64NAMEREF", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  slick64:block -- SLICK64BLOCK*/
	i = -1;
	tnd = tnode_newnodetype ("slick64:block", &i, 2, 0, 1, TNF_NONE);		/* subnodes: block body, statics; hooks: slick64_blockhook_t */
	tnd->hook_dumptree = slick64_blockhook_dumptree;
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "preallocate", 2, COMPOPTYPE (slick64_preallocate_block));
	tnode_setcompop (cops, "precode", 2, COMPOPTYPE (slick64_precode_block));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (slick64_codegen_block));
	tnd->ops = cops;

	i = -1;
	target->tag_BLOCK = tnode_newnodetag ("SLICK64BLOCK", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  slick64:const -- SLICK64CONST*/
	i = -1;
	tnd = tnode_newnodetype ("slick64:const", &i, 1, 0, 1, TNF_NONE);		/* subnodes: original; hooks: slick64_consthook_t */
	tnd->hook_dumptree = slick64_consthook_dumptree;
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "precode", 2, COMPOPTYPE (slick64_precode_const));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (slick64_codegen_const));
	tnd->ops = cops;

	i = -1;
	target->tag_CONST = tnode_newnodetag ("SLICK64CONST", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  slick64:precode -- SLICK64PRECODE*/
	i = -1;
	tnd = tnode_newnodetype ("slick64:precode", &i, 2, 0, 0, TNF_NONE);

	i = -1;
	spriv->tag_PRECODE = tnode_newnodetag ("SLICK64PRECODE", &i, tnd, NTF_NONE);

	/*}}}*/

	target->initialised = 1;
	return 0;
}
/*}}}*/





