/*
 *	krocetc.c -- back-end routines for KRoC modified ETC target
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
#include "names.h"
#include "target.h"
#include "map.h"
#include "transputer.h"
#include "codegen.h"

/*}}}*/


/*{{{  forward decls*/
static int krocetc_target_init (target_t *target);
static char *krocetc_make_namedlabel (const char *lbl);
static tnode_t *krocetc_name_create (tnode_t *fename, tnode_t *body, map_t *mdata, int asize_wsh, int asize_wsl, int asize_vs, int asize_ms, int tsize, int ind);
static tnode_t *krocetc_nameref_create (tnode_t *bename, map_t *mdata);
static tnode_t *krocetc_block_create (tnode_t *body, map_t *mdata, tnode_t *slist, int lexlevel);
static tnode_t *krocetc_const_create (tnode_t *val, map_t *mdata, void *data, int size);
static tnode_t *krocetc_indexed_create (tnode_t *base, tnode_t *index, int isize, int offset);
static tnode_t *krocetc_blockref_create (tnode_t *block, tnode_t *body, map_t *mdata);
static tnode_t *krocetc_result_create (tnode_t *expr, map_t *mdata);
static void krocetc_inresult (tnode_t **nodep, map_t *mdata);
static tnode_t **krocetc_be_blockbodyaddr (tnode_t *blk);
static int krocetc_be_allocsize (tnode_t *node, int *pwsh, int *pwsl, int *pvs, int *pms);
static void krocetc_be_setoffsets (tnode_t *bename, int ws_offset, int vs_offset, int ms_offset);
static void krocetc_be_getoffsets (tnode_t *bename, int *wsop, int *vsop, int *msop);
static int krocetc_be_blocklexlevel (tnode_t *blk);
static void krocetc_be_setblocksize (tnode_t *blk, int ws, int ws_offs, int vs, int ms, int adjust);
static void krocetc_be_getblocksize (tnode_t *blk, int *wsp, int *wsoffsp, int *vsp, int *msp, int *adjp, int *elabp);
static int krocetc_be_codegen_init (codegen_t *cgen, lexfile_t *srcfile);
static int krocetc_be_codegen_final (codegen_t *cgen, lexfile_t *srcfile);
static void krocetc_be_precode_seenproc (codegen_t *cgen, name_t *name, tnode_t *node);

static void krocetc_coder_setlabel (codegen_t *cgen, int lbl);
static void krocetc_coder_constblock (codegen_t *cgen, void *ptr, int size);
static void krocetc_coder_loadlabaddr (codegen_t *cgen, int lbl);

/*}}}*/
/*{{{  target_t for this target*/
target_t krocetc_target = {
	initialised:	0,
	name:		"krocetc",
	tarch:		"etc",
	tvendor:	"kroc",
	tos:		NULL,
	desc:		"KRoC extended T800 transputer-code",
	extn:		"etc",
	tcap: {
		can_do_fp: 1,
		can_do_dmem: 1
	},
	bws: {
		ds_min: 12,
		ds_io: 16,
		ds_altio: 16,
		ds_wait: 24,
		ds_max: 24
	},
	aws: {
		as_par: 12,
	},

	chansize:	4,
	charsize:	1,
	intsize:	4,
	pointersize:	4,
	slotsize:	4,
	structalign:	4,

	tag_NAME:	NULL,
	tag_NAMEREF:	NULL,
	tag_BLOCK:	NULL,
	tag_CONST:	NULL,
	tag_INDEXED:	NULL,
	tag_BLOCKREF:	NULL,
	tag_STATICLINK:	NULL,
	tag_RESULT:	NULL,

	init:		krocetc_target_init,
	newname:	krocetc_name_create,
	newnameref:	krocetc_nameref_create,
	newblock:	krocetc_block_create,
	newconst:	krocetc_const_create,
	newindexed:	krocetc_indexed_create,
	newblockref:	krocetc_blockref_create,
	newresult:	krocetc_result_create,
	inresult:	krocetc_inresult,

	be_blockbodyaddr:	krocetc_be_blockbodyaddr,
	be_allocsize:	krocetc_be_allocsize,
	be_setoffsets:	krocetc_be_setoffsets,
	be_getoffsets:	krocetc_be_getoffsets,
	be_blocklexlevel:	krocetc_be_blocklexlevel,
	be_setblocksize:	krocetc_be_setblocksize,
	be_getblocksize:	krocetc_be_getblocksize,
	be_codegen_init:	krocetc_be_codegen_init,
	be_codegen_final:	krocetc_be_codegen_final,
	
	be_precode_seenproc:	krocetc_be_precode_seenproc,

	priv:		NULL
};

/*}}}*/
/*{{{  private types*/
typedef struct TAG_krocetc_namehook {
	int lexlevel;		/* lexical level */
	int alloc_wsh;		/* allocation in high-workspace */
	int alloc_wsl;		/* allocation in low-workspace */
	int alloc_vs;		/* allocation in vectorspace */
	int alloc_ms;		/* allocation in mobilespace */
	int typesize;		/* size of the actual type (if known) */
	int indir;		/* indirection count (0 = real-thing, 1 = pointer, 2 = pointer-pointer, etc.) */
	int ws_offset;		/* workspace offset in current block */
	int vs_offset;		/* vectorspace offset in current block */
	int ms_offset;		/* mobilespace offset in current block */
} krocetc_namehook_t;

typedef struct TAG_krocetc_blockhook {
	int lexlevel;		/* lexical level */
	int alloc_ws;		/* workspace requirements */
	int alloc_vs;		/* vectorspace requirements */
	int alloc_ms;		/* mobilespace requirements */
	int static_adjust;	/* adjustment for statics (e.g. PROC params, etc.) */
	int ws_offset;		/* workspace offset for the block (includes static-adjust) */
	int entrylab;		/* entry-point label */
	int addstaticlink;	/* whether it needs a staticlink */
} krocetc_blockhook_t;

typedef struct TAG_krocetc_blockrefhook {
	tnode_t *block;
} krocetc_blockrefhook_t;

typedef struct TAG_krocetc_consthook {
	void *byteptr;
	int size;
	int label;
	int labrefs;		/* number of references to the label */
} krocetc_consthook_t;

typedef struct TAG_krocetc_indexedhook {
	int isize;		/* index size */
	int offset;		/* offset */
} krocetc_indexedhook_t;

typedef struct TAG_krocetc_priv {
	ntdef_t *tag_PRECODE;
	ntdef_t *tag_CONSTREF;
	ntdef_t *tag_JENTRY;
	ntdef_t *tag_DESCRIPTOR;
	tnode_t *precodelist;
	name_t *toplevelname;

	int maxtsdepth;		/* integer stack size */
	int maxfpdepth;		/* floating-point stack size */

	chook_t *mapchook;
	chook_t *resultsubhook;

	struct {
		unsigned int stoperrormode:1;
	} options;
} krocetc_priv_t;

typedef struct TAG_krocetc_resultsubhook {
	int eval_regs;
	int result_regs;
	DYNARRAY (tnode_t **, sublist);
} krocetc_resultsubhook_t;

typedef struct TAG_krocetc_cgstate {
	int tsdepth;		/* integer stack depth */
	int fpdepth;		/* floating-point stack depth */
} krocetc_cgstate_t;


/*}}}*/


/*{{{  void krocetc_isetindent (FILE *stream, int indent)*/
/*
 *	set-indent for debugging output
 */
void krocetc_isetindent (FILE *stream, int indent)
{
	int i;

	for (i=0; i<indent; i++) {
		fprintf (stream, "    ");
	}
	return;
}
/*}}}*/


/*{{{  static int krocetc_opthandler_flag (cmd_option_t *opt, char ***argwalk, int *argleft)*/
/*
 *	option handler for this target's options
 *	returns 0 on success, non-zero on failure
 */
static int krocetc_opthandler_flag (cmd_option_t *opt, char ***argwalk, int *argleft)
{
	int optv = (int)opt->arg;
	int flagval = 1;
	krocetc_priv_t *kpriv = (krocetc_priv_t *)krocetc_target.priv;

	if (optv < 0) {
		flagval = 0;
		optv = -optv;
	}
	switch (optv) {
	case 1:
		kpriv->options.stoperrormode = flagval;
		break;
	default:
		return -1;
	}
	return 0;
}
/*}}}*/
/*{{{  static int krocetc_init_options (krocetc_priv_t *kpriv)*/
/*
 *	initialises options for the KRoC-ETC back-end
 *	returns 0 on success, non-zero on failure
 */
static int krocetc_init_options (krocetc_priv_t *kpriv)
{
#if 0
fprintf (stderr, "krocetc_init_options(): adding options to compiler..\n");
#endif
	opts_add ("stoperrormode", '\0', krocetc_opthandler_flag, (void *)1, "1use stop error-mode");
	opts_add ("halterrormode", '\0', krocetc_opthandler_flag, (void *)-1, "1use halt error-mode");

	return 0;
}
/*}}}*/


/*{{{  krocetc_namehook_t routines*/
/*{{{  static void krocetc_namehook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	dumps hook data for debugging
 */
static void krocetc_namehook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	krocetc_namehook_t *nh = (krocetc_namehook_t *)hook;

	krocetc_isetindent (stream, indent);
	fprintf (stream, "<namehook addr=\"0x%8.8x\" lexlevel=\"%d\" allocwsh=\"%d\" allocwsl=\"%d\" allocvs=\"%d\" allocms=\"%d\" typesize=\"%d\" indir=\"%d\" wsoffset=\"%d\" vsoffset=\"%d\" msoffset=\"%d\" />\n",
			(unsigned int)nh, nh->lexlevel, nh->alloc_wsh, nh->alloc_wsl, nh->alloc_vs, nh->alloc_ms, nh->typesize, nh->indir, nh->ws_offset, nh->vs_offset, nh->ms_offset);
	return;
}
/*}}}*/
/*{{{  static krocetc_namehook_t *krocetc_namehook_create (int ll, int asize_wsh, int asize_wsl, int asize_vs, int asize_ms, int tsize, int ind)*/
/*
 *	creates a name-hook
 */
static krocetc_namehook_t *krocetc_namehook_create (int ll, int asize_wsh, int asize_wsl, int asize_vs, int asize_ms, int tsize, int ind)
{
	krocetc_namehook_t *nh = (krocetc_namehook_t *)smalloc (sizeof (krocetc_namehook_t));

	nh->lexlevel = ll;
	nh->alloc_wsh = asize_wsh;
	nh->alloc_wsl = asize_wsl;
	nh->alloc_vs = asize_vs;
	nh->alloc_ms = asize_ms;
	nh->typesize = tsize;
	nh->indir = ind;
	nh->ws_offset = -1;
	nh->vs_offset = -1;
	nh->ms_offset = -1;

	return nh;
}
/*}}}*/
/*}}}*/
/*{{{  krocetc_blockhook_t routines*/
/*{{{  static void krocetc_blockhook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	dumps hook for debugging
 */
static void krocetc_blockhook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	krocetc_blockhook_t *bh = (krocetc_blockhook_t *)hook;

	krocetc_isetindent (stream, indent);
	fprintf (stream, "<blockhook addr=\"0x%8.8x\" lexlevel=\"%d\" allocws=\"%d\" allocvs=\"%d\" allocms=\"%d\" adjust=\"%d\" wsoffset=\"%d\" entrylab=\"%d\" addstaticlink=\"%d\" />\n",
			(unsigned int)bh, bh->lexlevel, bh->alloc_ws, bh->alloc_vs, bh->alloc_ms, bh->static_adjust, bh->ws_offset, bh->entrylab, bh->addstaticlink);
	return;
}
/*}}}*/
/*{{{  static krocetc_blockhook_t *krocetc_blockhook_create (int ll)*/
/*
 *	creates a block-hook
 */
static krocetc_blockhook_t *krocetc_blockhook_create (int ll)
{
	krocetc_blockhook_t *bh = (krocetc_blockhook_t *)smalloc (sizeof (krocetc_blockhook_t));

	bh->lexlevel = ll;
	bh->alloc_ws = 0;
	bh->alloc_vs = 0;
	bh->alloc_ms = 0;
	bh->static_adjust = 0;
	bh->ws_offset = 0;
	bh->entrylab = 0;
	bh->addstaticlink = 0;

	return bh;
}
/*}}}*/
/*}}}*/
/*{{{  krocetc_blockrefhook_t routines*/
/*{{{  static void krocetc_blockrefhook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	dumps hook (debugging)
 */
static void krocetc_blockrefhook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	krocetc_blockrefhook_t *brh = (krocetc_blockrefhook_t *)hook;
	tnode_t *blk = brh->block;

	if (blk && parser_islistnode (blk)) {
		int nitems, i;
		tnode_t **blks = parser_getlistitems (blk, &nitems);

		krocetc_isetindent (stream, indent);
		fprintf (stream, "<blockrefhook addr=\"0x%8.8x\" block=\"0x%8.8x\" nblocks=\"%d\" blocks=\"", (unsigned int)brh, (unsigned int)blk, nitems);
		for (i=0; i<nitems; i++ ) {
			if (i) {
				fprintf (stream, ",");
			}
			fprintf (stream, "0x%8.8x", (unsigned int)blks[i]);
		}
		fprintf (stream, "\" />\n");
	} else {
		krocetc_isetindent (stream, indent);
		fprintf (stream, "<blockrefhook addr=\"0x%8.8x\" block=\"0x%8.8x\" />\n", (unsigned int)brh, (unsigned int)blk);
	}

	return;
}
/*}}}*/
/*{{{  static krocetc_blockrefhook_t *krocetc_blockrefhook_create (tnode_t *block)*/
/*
 *	creates a new hook (populated)
 */
static krocetc_blockrefhook_t *krocetc_blockrefhook_create (tnode_t *block)
{
	krocetc_blockrefhook_t *brh = (krocetc_blockrefhook_t *)smalloc (sizeof (krocetc_blockrefhook_t));

	brh->block = block;

	return brh;
}
/*}}}*/
/*}}}*/
/*{{{  krocetc_consthook_t routines*/
/*{{{  static void krocetc_consthook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	dumps hook for debugging
 */
static void krocetc_consthook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	krocetc_consthook_t *ch = (krocetc_consthook_t *)hook;

	krocetc_isetindent (stream, indent);
	fprintf (stream, "<consthook addr=\"0x%8.8x\" data=\"0x%8.8x\" size=\"%d\" label=\"%d\" labrefs=\"%d\" />\n",
			(unsigned int)ch, (unsigned int)ch->byteptr, ch->size, ch->label, ch->labrefs);
	return;
}
/*}}}*/
/*{{{  static krocetc_consthook_t *krocetc_consthook_create (void *ptr, int size)*/
/*
 *	creates a constant-hook
 */
static krocetc_consthook_t *krocetc_consthook_create (void *ptr, int size)
{
	krocetc_consthook_t *ch = (krocetc_consthook_t *)smalloc (sizeof (krocetc_consthook_t));

	ch->byteptr = mem_ndup (ptr, size);
	ch->size = size;
	ch->label = -1;
	ch->labrefs = 0;

	return ch;
}
/*}}}*/
/*}}}*/
/*{{{  krocetc_indexedhook_t routines*/
/*{{{  static void krocetc_indexedhook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	dumps hook for debugging
 */
static void krocetc_indexedhook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	krocetc_indexedhook_t *ih = (krocetc_indexedhook_t *)hook;

	krocetc_isetindent (stream, indent);
	fprintf (stream, "<indexhook isize=\"%d\" offset=\"%d\" />\n", ih->isize, ih->offset);
	return;
}
/*}}}*/
/*{{{  static krocetc_indexedhook_t *krocetc_indexedhook_create (int isize, int offset)*/
/*
 *	creates a indexed hook
 */
static krocetc_indexedhook_t *krocetc_indexedhook_create (int isize, int offset)
{
	krocetc_indexedhook_t *ih = (krocetc_indexedhook_t *)smalloc (sizeof (krocetc_indexedhook_t));

	ih->isize = isize;
	ih->offset = offset;
	return ih;
}
/*}}}*/
/*}}}*/
/*{{{  special-node hook routines*/
/*{{{  static void krocetc_specialhook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	dumps hook data for debugging
 */
static void krocetc_specialhook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	krocetc_isetindent (stream, indent);
	fprintf (stream, "<specialhook addr=\"0x%8.8x\" />\n", (unsigned int)hook);
}
/*}}}*/
/*}}}*/


/*{{{  krocetc_resultsubhook_t routines*/
/*{{{  static void krocetc_resultsubhook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	dumps hook for debugging
 */
static void krocetc_resultsubhook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	krocetc_resultsubhook_t *rh = (krocetc_resultsubhook_t *)hook;
	int i;

	krocetc_isetindent (stream, indent);
	fprintf (stream, "<chook:resultsubhook eregs=\"%d\" rregs=\"%d\">\n", rh->eval_regs, rh->result_regs);
	for (i=0; i<DA_CUR (rh->sublist); i++) {
		tnode_t *ref = *(DA_NTHITEM (rh->sublist, i));

		krocetc_isetindent (stream, indent+1);
		fprintf (stream, "<noderef nodetype=\"%s\" type=\"%s\" addr=\"0x%8.8x\" />\n", ref->tag->ndef->name, ref->tag->name, (unsigned int)ref);
	}
	krocetc_isetindent (stream, indent);
	fprintf (stream, "</chook:resultsubhook>\n");
	return;
}
/*}}}*/
/*{{{  static krocetc_resultsubhook_t *krocetc_resultsubhook_create (void)*/
/*
 *	creates a blank result hook
 */
static krocetc_resultsubhook_t *krocetc_resultsubhook_create (void)
{
	krocetc_resultsubhook_t *rh = (krocetc_resultsubhook_t *)smalloc (sizeof (krocetc_resultsubhook_t));

	rh->eval_regs = -1;
	rh->result_regs = -1;
	dynarray_init (rh->sublist);

	return rh;
}
/*}}}*/
/*{{{  static void krocetc_resultsubhook_free (void *hook)*/
/*
 *	frees a result hook
 */
static void krocetc_resultsubhook_free (void *hook)
{
	krocetc_resultsubhook_t *rh = (krocetc_resultsubhook_t *)hook;

	if (!rh) {
		return;
	}
	dynarray_trash (rh->sublist);
	sfree (rh);

	return;
}
/*}}}*/
/*}}}*/


/*{{{  krocetc_cgstate_t routines*/
/*{{{  static krocetc_cgstate_t *krocetc_cgstate_create (void)*/
/*
 *	creates a new krocetc_cgstate_t
 */
static krocetc_cgstate_t *krocetc_cgstate_create (void)
{
	krocetc_cgstate_t *cgs = (krocetc_cgstate_t *)smalloc (sizeof (krocetc_cgstate_t));

	cgs->tsdepth = 0;
	cgs->fpdepth = 0;

	return cgs;
}
/*}}}*/
/*{{{  static void krocetc_cgstate_destroy (krocetc_cgstate_t *cgs)*/
/*
 *	destroys a krocetc_cgstate_t
 */
static void krocetc_cgstate_destroy (krocetc_cgstate_t *cgs)
{
	if (!cgs) {
		nocc_internal ("krocetc_cgstate_destroy(): null state!");
		return;
	}
	sfree (cgs);

	return;
}
/*}}}*/
/*}}}*/


/*{{{  static krocetc_cgstate_t *krocetc_cgstate_newpush (codegen_t *cgen)*/
/*
 *	creates a new code-gen state and pushes it onto the code-gen stack, also returns it
 */
static krocetc_cgstate_t *krocetc_cgstate_newpush (codegen_t *cgen)
{
	krocetc_cgstate_t *cgs = krocetc_cgstate_create ();

	dynarray_add (cgen->tcgstates, (void *)cgs);

	return cgs;
}
/*}}}*/
/*{{{  static krocetc_cgstate_t *krocetc_cgstate_copypush (codegen_t *cgen)*/
/*
 *	copies and pushes the current code-gen state, also returns it
 */
static krocetc_cgstate_t *krocetc_cgstate_copypush (codegen_t *cgen)
{
	krocetc_cgstate_t *cgs;

	cgs = krocetc_cgstate_create ();
	if (!DA_CUR (cgen->tcgstates)) {
		codegen_warning (cgen, "krocetc_cgstate_copypush(): no previous state -- creating new");
	} else {
		krocetc_cgstate_t *lastcgs = (krocetc_cgstate_t *)DA_NTHITEM (cgen->tcgstates, DA_CUR (cgen->tcgstates) - 1);

		cgs->tsdepth = lastcgs->tsdepth;
		cgs->fpdepth = lastcgs->fpdepth;
	}
	dynarray_add (cgen->tcgstates, (void *)cgs);

	return cgs;
}
/*}}}*/
/*{{{  static void krocetc_cgstate_popfree (codegen_t *cgen)*/
/*
 *	pops and frees code-gen state
 */
static void krocetc_cgstate_popfree (codegen_t *cgen)
{
	krocetc_cgstate_t *cgs;

	if (!DA_CUR (cgen->tcgstates)) {
		nocc_internal ("krocetc_cgstate_popfree(): no tcgstates!");
		return;
	}
	cgs = (krocetc_cgstate_t *)DA_NTHITEM (cgen->tcgstates, DA_CUR (cgen->tcgstates) - 1);
	if (cgs->tsdepth) {
		codegen_warning (cgen, "krocetc_cgstate_popfree(): integer stack at depth %d", cgs->tsdepth);
	}
	if (cgs->fpdepth) {
		codegen_warning (cgen, "krocetc_cgstate_popfree(): floating-point stack at depth %d", cgs->fpdepth);
	}
	DA_SETNTHITEM (cgen->tcgstates, DA_CUR (cgen->tcgstates) - 1, NULL);
	dynarray_delitem (cgen->tcgstates, DA_CUR (cgen->tcgstates) - 1);

	krocetc_cgstate_destroy (cgs);
	return;
}
/*}}}*/
/*{{{  static krocetc_cgstate_t *krocetc_cgstate_cur (codegen_t *cgen)*/
/*
 *	returns the current code-gen state
 */
static krocetc_cgstate_t *krocetc_cgstate_cur (codegen_t *cgen)
{
	if (!DA_CUR (cgen->tcgstates)) {
		codegen_error (cgen, "krocetc_cgstate_cur(): no current code-gen state..");
		return NULL;
	}
	return (krocetc_cgstate_t *)DA_NTHITEM (cgen->tcgstates, DA_CUR (cgen->tcgstates) - 1);
}
/*}}}*/
/*{{{  static int krocetc_cgstate_tsdelta (codegen_t *cgen, int delta)*/
/*
 *	adjusts the integer stack -- emits a warning if it overflows or underflows
 *	returns the new stack depth
 */
static int krocetc_cgstate_tsdelta (codegen_t *cgen, int delta)
{
	krocetc_priv_t *kpriv = (krocetc_priv_t *)cgen->target->priv;
	krocetc_cgstate_t *cgs = krocetc_cgstate_cur (cgen);

	if (!cgs) {
		codegen_error (cgen, "krocetc_cgstate_tsdelta(): no stack to adjust!");
		return 0;
	}
#if 0
fprintf (stderr, "krocetc_cgstate_tsdelta(): cgs->tsdepth = %d, delta = %d\n", cgs->tsdepth, delta);
#endif
	cgs->tsdepth += delta;
	if (cgs->tsdepth < 0) {
		codegen_warning (cgen, "krocetc_cgstate_tsdelta(): stack underflow");
		cgs->tsdepth = 0;
		codegen_write_fmt (cgen, "\t.tsdepth %d\n", cgs->tsdepth);
	} else if (cgs->tsdepth > kpriv->maxtsdepth) {
		codegen_warning (cgen, "krocetc_cgstate_tsdepth(): stack overflow");
		cgs->tsdepth = kpriv->maxtsdepth;
		codegen_write_fmt (cgen, "\t.tsdepth %d\n", cgs->tsdepth);
	}

	return cgs->tsdepth;
}
/*}}}*/


/*{{{  static tnode_t *krocetc_name_create (tnode_t *fename, tnode_t *body, map_t *mdata, int asize_wsh, int asize_wsl, int asize_vs, int asize_ms, int tsize, int ind)*/
/*
 *	allocates a new back-end name-node
 */
static tnode_t *krocetc_name_create (tnode_t *fename, tnode_t *body, map_t *mdata, int asize_wsh, int asize_wsl, int asize_vs, int asize_ms, int tsize, int ind)
{
	target_t *xt = mdata->target;		/* must be us! */
	tnode_t *name;
	krocetc_namehook_t *nh;

	nh = krocetc_namehook_create (mdata->lexlevel, asize_wsh, asize_wsl, asize_vs, asize_ms, tsize, ind);
	name = tnode_create (xt->tag_NAME, NULL, fename, body, (void *)nh);

	return name;
}
/*}}}*/
/*{{{  static tnode_t *krocetc_nameref_create (tnode_t *bename, map_t *mdata)*/
/*
 *	allocates a new back-end name-ref-node
 */
static tnode_t *krocetc_nameref_create (tnode_t *bename, map_t *mdata)
{
	krocetc_namehook_t *nh, *be_nh;
	krocetc_blockhook_t *bh;
	tnode_t *name, *fename;
	tnode_t *blk = mdata->thisblock;

	if (!blk) {
		nocc_internal ("krocetc_nameref_create(): reference to name outside of block");
		return NULL;
	}
	bh = (krocetc_blockhook_t *)tnode_nthhookof (blk, 0);
	be_nh = (krocetc_namehook_t *)tnode_nthhookof (bename, 0);
#if 0
fprintf (stderr, "krocetc_nameref_create (): referenced lexlevel=%d, map lexlevel=%d, enclosing block lexlevel=%d\n", be_nh->lexlevel, mdata->lexlevel, bh->lexlevel);
#endif
	if (be_nh->lexlevel < bh->lexlevel) {
		/*{{{  need a static-link to get at this one*/
		bh->addstaticlink = 1;
		/*}}}*/
	}
	/* nh = krocetc_namehook_create (be_nh->lexlevel, 0, 0, 0, 0, be_nh->typesize, be_nh->indir); */
	nh = krocetc_namehook_create (mdata->lexlevel, 0, 0, 0, 0, be_nh->typesize, be_nh->indir);

	fename = tnode_nthsubof (bename, 0);
	name = tnode_create (mdata->target->tag_NAMEREF, NULL, fename, (void *)nh);

	return name;
}
/*}}}*/
/*{{{  static tnode_t *krocetc_block_create (tnode_t *body, map_t *mdata, tnode_t *slist, int lexlevel)*/
/*
 *	creates a new back-end block
 */
static tnode_t *krocetc_block_create (tnode_t *body, map_t *mdata, tnode_t *slist, int lexlevel)
{
	krocetc_blockhook_t *bh;
	tnode_t *blk;

	bh = krocetc_blockhook_create (lexlevel);
	blk = tnode_create (mdata->target->tag_BLOCK, NULL, body, slist, (void *)bh);

	return blk;
}
/*}}}*/
/*{{{  static tnode_t *krocetc_const_create (tnode_t *val, map_t *mdata, void *data, int size)*/
/*
 *	creates a new back-end constant
 */
static tnode_t *krocetc_const_create (tnode_t *val, map_t *mdata, void *data, int size)
{
	krocetc_consthook_t *ch;
	tnode_t *cnst;

	ch = krocetc_consthook_create (data, size);
	cnst = tnode_create (mdata->target->tag_CONST, NULL, val, (void *)ch);

	return cnst;
}
/*}}}*/
/*{{{  static tnode_t *krocetc_indexed_create (tnode_t *base, tnode_t *index, int isize, int offset)*/
/*
 *	creates a new back-end indexed node (used for arrays and the like)
 */
static tnode_t *krocetc_indexed_create (tnode_t *base, tnode_t *index, int isize, int offset)
{
	krocetc_indexedhook_t *ih;
	tnode_t *indxd;

	ih = krocetc_indexedhook_create (isize, offset);
	indxd = tnode_create (krocetc_target.tag_INDEXED, NULL, base, index, (void *)ih);

	return indxd;
}
/*}}}*/
/*{{{  static tnode_t *krocetc_blockref_create (tnode_t *block, tnode_t *body, map_t *mdata)*/
/*
 *	creates a new back-end block reference node (used for procedure instances and the like)
 */
static tnode_t *krocetc_blockref_create (tnode_t *block, tnode_t *body, map_t *mdata)
{
	krocetc_blockrefhook_t *brh = krocetc_blockrefhook_create (block);
	tnode_t *blockref;

	blockref = tnode_create (krocetc_target.tag_BLOCKREF, NULL, body, (void *)brh);

	return blockref;
}
/*}}}*/
/*{{{  static tnode_t *krocetc_result_create (tnode_t *expr, map_t *mdata)*/
/*
 *	creates a new back-end result node (used for expressions and the like)
 */
static tnode_t *krocetc_result_create (tnode_t *expr, map_t *mdata)
{
	krocetc_priv_t *kpriv = (krocetc_priv_t *)mdata->target->priv;
	krocetc_resultsubhook_t *rh;
	tnode_t *res;

	res = tnode_create (krocetc_target.tag_RESULT, NULL, expr);

	rh = krocetc_resultsubhook_create ();
	tnode_setchook (res, kpriv->resultsubhook, (void *)rh);

	return res;
}
/*}}}*/
/*{{{  static void krocetc_inresult (tnode_t **node, map_t *mdata)*/
/*
 *	adds a (back-end) node to the sub-list of a back-end result node, used in expressions and the like
 */
static void krocetc_inresult (tnode_t **nodep, map_t *mdata)
{
	krocetc_priv_t *kpriv = (krocetc_priv_t *)mdata->target->priv;
	krocetc_resultsubhook_t *rh;

	if (!mdata->thisberesult) {
		nocc_internal ("krocetc_inresult(): not inside any result!");
		return;
	}

	rh = (krocetc_resultsubhook_t *)tnode_getchook (mdata->thisberesult, kpriv->resultsubhook);
	if (!rh) {
		nocc_internal ("krocetc_inresult(): missing resultsubhook!");
		return;
	}

	dynarray_add (rh->sublist, nodep);

	return;
}
/*}}}*/


/*{{{  int krocetc_init (void)*/
/*
 *	initialises the KRoC-ETC back-end
 *	returns 0 on success, non-zero on error
 */
int krocetc_init (void)
{
	/* register the target */
	if (target_register (&krocetc_target)) {
		nocc_error ("krocetc_init(): failed to register target!");
		return 1;
	}

	return 0;
}
/*}}}*/
/*{{{  int krocetc_shutdown (void)*/
/*
 *	shuts down the KRoC-ETC back-end
 *	returns 0 on success, non-zero on error
 */
int krocetc_shutdown (void)
{
	/* unregister the target */
	if (target_unregister (&krocetc_target)) {
		nocc_error ("krocetc_shutdown(): failed to unregister target!");
		return 1;
	}

	return 0;
}
/*}}}*/


/*{{{  static int krocetc_be_allocsize (tnode_t *node, int *pwsh, int *pwsl, int *pvs, int *pms)*/
/*
 *	retrieves the number of bytes to be allocated to a back-end name
 *	returns 0 on success, non-zero on failure
 */
static int krocetc_be_allocsize (tnode_t *node, int *pwsh, int *pwsl, int *pvs, int *pms)
{
	if (node->tag == krocetc_target.tag_BLOCKREF) {
		/*{{{  space required for block reference*/
		krocetc_blockrefhook_t *brh = (krocetc_blockrefhook_t *)tnode_nthhookof (node, 0);
		tnode_t *blk = brh->block;
		int ws, wsoffs, vs, ms, adj;

		if (!brh) {
			return -1;
		}

		if (pwsh) {
			*pwsh = 0;
		}

		if (parser_islistnode (blk)) {
			tnode_t **blks;
			int nitems, i;

			blks = parser_getlistitems (blk, &nitems);
#if 0
fprintf (stderr, "krocetc_be_allocsize(): block-list, %d items\n", nitems);
#endif
			ws = 0;
			vs = 0;
			ms = 0;
			for (i=0; i<nitems; i++) {
				int lws, lvs, lms;
				int elab;
				
				krocetc_be_getblocksize (blks[i], &lws, &wsoffs, &lvs, &lms, &adj, &elab);
				if (lws > 0) {
					ws += lws;
				}
				if (lvs > 0) {
					vs += lvs;
				}
				if (lms > 0) {
					ms += lms;
				}
			}
		} else {
			int elab;

			krocetc_be_getblocksize (blk, &ws, &wsoffs, &vs, &ms, &adj, &elab);
		}

#if 0
fprintf (stderr, "krocetc_be_allocsize(): got block size from BLOCKREF, ws=%d, wsoffs=%d, vs=%d, ms=%d, adj=%d\n", ws, wsoffs, vs, ms, adj);
#endif

		if (pwsl) {
			*pwsl = ws;
		}
		if (pvs) {
			*pvs = vs;
		}
		if (pms) {
			*pms = ms;
		}
		/*}}}*/
	} else if (node->tag == krocetc_target.tag_NAME) {
		/*{{{  space required for name*/
		krocetc_namehook_t *nh;

		nh = (krocetc_namehook_t *)tnode_nthhookof (node, 0);
		if (!nh) {
			return -1;
		}
		if (pwsh) {
			*pwsh = nh->alloc_wsh;
		}
		if (pwsl) {
			*pwsl = nh->alloc_wsl;
		}
		if (pvs) {
			*pvs = nh->alloc_vs;
		}
		if (pms) {
			*pms = nh->alloc_ms;
		}
		/*}}}*/
	} else {
		return -1;
	}

	return 0;
}
/*}}}*/
/*{{{  static void krocetc_be_setoffsets (tnode_t *bename, int ws_offset, int vs_offset, int ms_offset)*/
/*
 *	sets the offsets for a back-end name after allocation
 */
static void krocetc_be_setoffsets (tnode_t *bename, int ws_offset, int vs_offset, int ms_offset)
{
	krocetc_namehook_t *nh;

	if ((bename->tag != krocetc_target.tag_NAME) && (bename->tag != krocetc_target.tag_NAMEREF)) {
		nocc_internal ("krocetc_be_setoffsets(): not a NAME/NAMEREF!");
		return;
	}
	nh = (krocetc_namehook_t *)tnode_nthhookof (bename, 0);
	if (!nh) {
		nocc_internal ("krocetc_be_setoffsets(): NAME/NAMEREF has no hook");
		return;
	}
	nh->ws_offset = ws_offset;
	nh->vs_offset = vs_offset;
	nh->ms_offset = ms_offset;

	return;
}
/*}}}*/
/*{{{  static void krocetc_be_getoffsets (tnode_t *bename, int *wsop, int *vsop, int *msop)*/
/*
 *	gets the offsets for a back-end name after allocation
 */
static void krocetc_be_getoffsets (tnode_t *bename, int *wsop, int *vsop, int *msop)
{
	krocetc_namehook_t *nh;

	if ((bename->tag != krocetc_target.tag_NAME) && (bename->tag != krocetc_target.tag_NAMEREF)) {
		nocc_internal ("krocetc_be_getoffsets(): not a NAME/NAMEREF!");
		return;
	}
	nh = (krocetc_namehook_t *)tnode_nthhookof (bename, 0);
	if (!nh) {
		nocc_internal ("krocetc_be_getoffsets(): NAME/NAMEREF has no hook");
		return;
	}
	if (wsop) {
		*wsop = nh->ws_offset;
	}
	if (vsop) {
		*vsop = nh->vs_offset;
	}
	if (msop) {
		*msop = nh->ms_offset;
	}

	return;
}
/*}}}*/
/*{{{  static int krocetc_be_blocklexlevel (tnode_t *blk)*/
/*
 *	returns the lex-level of a block (or name/nameref)
 */
static int krocetc_be_blocklexlevel (tnode_t *blk)
{
	if (blk->tag == krocetc_target.tag_BLOCK) {
		krocetc_blockhook_t *bh = (krocetc_blockhook_t *)tnode_nthhookof (blk, 0);

		if (!bh) {
			nocc_internal ("krocetc_be_blocklexlevel(): BLOCK has no hook");
			return -1;
		}
		return bh->lexlevel;
	} else if (blk->tag == krocetc_target.tag_NAME) {
		krocetc_namehook_t *nh = (krocetc_namehook_t *)tnode_nthhookof (blk, 0);

		if (!nh) {
			nocc_internal ("krocetc_be_blocklexlevel(): NAME has no hook");
			return -1;
		}
		return nh->lexlevel;
	} else if (blk->tag == krocetc_target.tag_NAMEREF) {
		krocetc_namehook_t *nh = (krocetc_namehook_t *)tnode_nthhookof (blk, 0);

		if (!nh) {
			nocc_internal ("krocetc_be_blocklexlevel(): NAMEREF has no hook");
			return -1;
		}
		return nh->lexlevel;
	}
	nocc_internal ("krocetc_be_blocklexlevel(): don\'t know how to deal with a %s", blk->tag->name);
	return -1;
}
/*}}}*/
/*{{{  static void krocetc_be_setblocksize (tnode_t *blk, int ws, int ws_offs, int vs, int ms, int adjust)*/
/*
 *	sets back-end block size
 */
static void krocetc_be_setblocksize (tnode_t *blk, int ws, int ws_offs, int vs, int ms, int adjust)
{
	krocetc_blockhook_t *bh;

	if (blk->tag != krocetc_target.tag_BLOCK) {
		nocc_internal ("krocetc_be_setblocksize(): not a BLOCK!");
		return;
	}
	bh = (krocetc_blockhook_t *)tnode_nthhookof (blk, 0);
	if (!bh) {
		nocc_internal ("krocetc_be_setblocksize(): BLOCK has no hook");
		return;
	}
	bh->alloc_ws = ws;
	bh->alloc_vs = vs;
	bh->alloc_ms = ms;
	bh->static_adjust = adjust;
	bh->ws_offset = ws_offs;

	return;
}
/*}}}*/
/*{{{  static void krocetc_be_getblocksize (tnode_t *blk, int *wsp, int *wsoffsp, int *vsp, int *msp, int *adjp, int *elabp)*/
/*
 *	gets back-end block size
 */
static void krocetc_be_getblocksize (tnode_t *blk, int *wsp, int *wsoffsp, int *vsp, int *msp, int *adjp, int *elabp)
{
	krocetc_blockhook_t *bh;

#if 0
fprintf (stderr, "krocetc_be_getblocksize(): blk =\n");
tnode_dumptree (blk, 1, stderr);
#endif
	if (blk->tag == krocetc_target.tag_BLOCKREF) {
		krocetc_blockrefhook_t *brh = (krocetc_blockrefhook_t *)tnode_nthhookof (blk, 0);

		blk = brh->block;
#if 0
fprintf (stderr, "krocetc_be_getblocksize(): called on BLOCKREF!\n");
#endif
	}

	if (blk->tag != krocetc_target.tag_BLOCK) {
		nocc_internal ("krocetc_be_getblocksize(): not a BLOCK!");
		return;
	}
	bh = (krocetc_blockhook_t *)tnode_nthhookof (blk, 0);
	if (!bh) {
		nocc_internal ("krocetc_be_getblocksize(): BLOCK has no hook");
		return;
	}

	if (wsp) {
		*wsp = bh->alloc_ws;
	}
	if (vsp) {
		*vsp = bh->alloc_vs;
	}
	if (msp) {
		*msp = bh->alloc_ms;
	}
	if (adjp) {
		*adjp = bh->static_adjust;
	}
	if (wsoffsp) {
		*wsoffsp = bh->ws_offset;
	}
	if (elabp) {
		*elabp = bh->entrylab;
	}

	return;
}
/*}}}*/
/*{{{  static tnode_t **krocetc_be_blockbodyaddr (tnode_t *blk)*/
/*
 *	returns the address of the body of a back-end block
 */
static tnode_t **krocetc_be_blockbodyaddr (tnode_t *blk)
{
	if (blk->tag != krocetc_target.tag_BLOCK) {
		nocc_internal ("krocetc_be_blockbodyaddr(): block not back-end BLOCK, was [%s]", blk->tag->name);
	}
	return tnode_nthsubaddr (blk, 0);
}
/*}}}*/
/*{{{  static int krocetc_be_regsfor (tnode_t *benode)*/
/*
 *	returns the number of registers required to evaluate something
 */
static int krocetc_be_regsfor (tnode_t *benode)
{
	if (benode->tag == krocetc_target.tag_NAME) {
		/* something local, if anything */
		return 1;
	} else if (benode->tag == krocetc_target.tag_CONST) {
		/* constants are easy */
		return 1;
	} else if (benode->tag == krocetc_target.tag_RESULT) {
		krocetc_priv_t *kpriv = (krocetc_priv_t *)krocetc_target.priv;
		krocetc_resultsubhook_t *rh;

		/* find out in result */
		rh = (krocetc_resultsubhook_t *)tnode_getchook (benode, kpriv->resultsubhook);
		if (!rh) {
			nocc_internal ("krocetc_be_regsfor(): missing resultsubhook in [%s]!", benode->tag->name);
			return 0;
		}

		return rh->eval_regs;
	} else if (benode->tag == krocetc_target.tag_NAMEREF) {
		/* name references are easy too */
		return 1;
	} else {
#if 1
fprintf (stderr, "krocetc_be_regsfor(): regsfor [%s] [%s] ?\n", benode->tag->ndef->name, benode->tag->name);
#endif
	}
	return 0;
}
/*}}}*/

/*{{{  static int krocetc_preallocate_block (tnode_t *blk, target_t *target)*/
/*
 *	does pre-allocation for a back-end block
 *	returns 0 to stop walk, 1 to continue
 */
static int krocetc_preallocate_block (tnode_t *blk, target_t *target)
{
	if (blk->tag == target->tag_BLOCK) {
		krocetc_blockhook_t *bh = (krocetc_blockhook_t *)tnode_nthhookof (blk, 0);

		if (bh->addstaticlink) {
			tnode_t **stptr = tnode_nthsubaddr (blk, 1);
			krocetc_namehook_t *nh;
			tnode_t *name;

#if 0
fprintf (stderr, "krocetc_preallocate_block(): adding static-link..\n");
#endif
			if (!*stptr) {
				*stptr = parser_newlistnode (NULL);
			} else if (!parser_islistnode (*stptr)) {
				tnode_t *slist = parser_newlistnode (NULL);

				parser_addtolist (slist, *stptr);
				*stptr = slist;
			}

			nh = krocetc_namehook_create (bh->lexlevel, target->pointersize, 0, 0, 0, target->pointersize, 0);
			name = tnode_create (target->tag_NAME, NULL, tnode_create (target->tag_STATICLINK, NULL), NULL, (void *)nh);

			parser_addtolist_front (*stptr, name);
		}
	}

	return 1;
}
/*}}}*/
/*{{{  static int krocetc_precode_block (tnode_t **tptr, codegen_t *cgen)*/
/*
 *	does pre-code generation for a back-end block
 *	return 0 to stop walk, 1 to continue it
 */
static int krocetc_precode_block (tnode_t **tptr, codegen_t *cgen)
{
	krocetc_blockhook_t *bh = (krocetc_blockhook_t *)tnode_nthhookof (*tptr, 0);

	if ((*tptr)->tag != krocetc_target.tag_BLOCK) {
		nocc_internal ("krocetc_precode_block(): block not back-end BLOCK, was [%s]", (*tptr)->tag->name);
	}
	if (!bh->entrylab) {
		/* give it an entry-point label */
		bh->entrylab = codegen_new_label (cgen);
	}
	return 1;
}
/*}}}*/
/*{{{  static int krocetc_codegen_block (tnode_t *blk, codegen_t *cgen)*/
/*
 *	does code generation for a back-end block
 *	return 0 to stop walk, 1 to continue it
 */
static int krocetc_codegen_block (tnode_t *blk, codegen_t *cgen)
{
	int ws_size, vs_size, ms_size;
	int ws_offset, adjust;
	int elab, lexlevel;

	if (blk->tag != krocetc_target.tag_BLOCK) {
		nocc_internal ("krocetc_codegen_block(): block not back-end BLOCK, was [%s]", blk->tag->name);
		return 0;
	}
	cgen->target->be_getblocksize (blk, &ws_size, &ws_offset, &vs_size, &ms_size, &adjust, &elab);
	lexlevel = cgen->target->be_blocklexlevel (blk);
	dynarray_setsize (cgen->be_blks, lexlevel + 1);
	DA_SETNTHITEM (cgen->be_blks, lexlevel, blk);

	krocetc_cgstate_newpush (cgen);

	if (elab) {
		codegen_callops (cgen, setlabel, elab);
	}
	codegen_callops (cgen, wsadjust, -(ws_offset - adjust));
	codegen_subcodegen (tnode_nthsubof (blk, 0), cgen);
	codegen_callops (cgen, wsadjust, (ws_offset - adjust));

	DA_SETNTHITEM (cgen->be_blks, lexlevel, NULL);
	dynarray_setsize (cgen->be_blks, lexlevel);

	krocetc_cgstate_popfree (cgen);

	return 0;
}
/*}}}*/

/*{{{  static int krocetc_bytesfor_name (tnode_t *name, target_t *target)*/
/*
 *	used to get the type-size of a back-end name
 *	returns type-size or -1 if not known
 */
static int krocetc_bytesfor_name (tnode_t *name, target_t *target)
{
	krocetc_namehook_t *nh;

	if (name->tag != krocetc_target.tag_NAME) {
		return -1;
	}
	nh = (krocetc_namehook_t *)tnode_nthhookof (name, 0);
	if (!nh) {
		return -1;
	}
	return nh->typesize;
}
/*}}}*/

/*{{{  static int krocetc_precode_const (tnode_t **cnst, codegen_t *cgen)*/
/*
 *	does pre-code for a back-end constant
 *	returns 0 to stop walk, 1 to continue
 */
static int krocetc_precode_const (tnode_t **cnst, codegen_t *cgen)
{
	krocetc_priv_t *kpriv = (krocetc_priv_t *)cgen->target->priv;
	krocetc_consthook_t *ch = (krocetc_consthook_t *)tnode_nthhookof (*cnst, 0);

	if (ch->label <= 0) {
		ch->label = codegen_new_label (cgen);
	}

	/* move this into pre-codes and leave a reference */
	parser_addtolist (kpriv->precodelist, *cnst);
	*cnst = tnode_create (kpriv->tag_CONSTREF, NULL, (void *)ch);
	return 1;
}
/*}}}*/
/*{{{  static int krocetc_codegen_const (tnode_t *cnst, codegen_t *cgen)*/
/*
 *	does code-generation for a constant -- these have been pulled out in front of the program
 *	returns 0 to stop walk, 1 to continue
 */
static int krocetc_codegen_const (tnode_t *cnst, codegen_t *cgen)
{
	krocetc_consthook_t *ch = (krocetc_consthook_t *)tnode_nthhookof (cnst, 0);

#if 0
fprintf (stderr, "krocetc_codegen_const(): ch->label = %d, ch->labrefs = %d\n", ch->label, ch->labrefs);
#endif
	if (ch->label > 0) {
		krocetc_coder_setlabel (cgen, ch->label);
		krocetc_coder_constblock (cgen, ch->byteptr, ch->size);
	}
	return 0;
}
/*}}}*/

/*{{{  static int krocetc_codegen_nameref (tnode_t *nameref, codegen_t *cgen)*/
/*
 *	generates code to load a name -- usually happens inside a RESULT
 *	return 0 to stop walk, 1 to continue
 */
static int krocetc_codegen_nameref (tnode_t *nameref, codegen_t *cgen)
{
	codegen_callops (cgen, loadname, nameref, 0);
	return 0;
}
/*}}}*/

/*{{{  static int krocetc_codegen_constref (tnode_t *constref, codegen_t *cgen)*/
/*
 *	generates code for a constant reference -- loads the constant
 *	return 0 to stop walk, 1 to continue
 */
static int krocetc_codegen_constref (tnode_t *constref, codegen_t *cgen)
{
	krocetc_consthook_t *ch = (krocetc_consthook_t *)tnode_nthhookof (constref, 0);
	int val;

	switch (ch->size) {
	case 1:
		val = (int)(*(unsigned char *)(ch->byteptr));
		break;
	case 2:
		val = (int)(*(unsigned short int *)(ch->byteptr));
		break;
	case 4:
		val = (int)(*(unsigned int *)(ch->byteptr));
		break;
	default:
		val = 0;
		break;
	}
	codegen_write_fmt (cgen, "\tldc\t%d\n", val);
	krocetc_cgstate_tsdelta (cgen, 1);

	return 0;
}
/*}}}*/

/*{{{  static int krocetc_namemap_result (tnode_t **rnodep, map_t *mdata)*/
/*
 *	name-map for a back-end result, sets hook in the map-data and sub-walks
 *	returns 0 to stop walk, 1 to continue
 */
static int krocetc_namemap_result (tnode_t **rnodep, map_t *mdata)
{
	krocetc_priv_t *kpriv = (krocetc_priv_t *)mdata->target->priv;
	krocetc_resultsubhook_t *rh;
	tnode_t *prevresult;

	rh = (krocetc_resultsubhook_t *)tnode_getchook (*rnodep, kpriv->resultsubhook);
	if (!rh) {
		nocc_internal ("krocetc_namemap_result(): missing resultsubhook!");
		return 0;
	}

	/* do name-map on contents with this node set as the result */
	prevresult = mdata->thisberesult;
	mdata->thisberesult = *rnodep;
	map_submapnames (tnode_nthsubaddr (*rnodep, 0), mdata);
	mdata->thisberesult = prevresult;

	return 0;
}
/*}}}*/
/*{{{  static int krocetc_bemap_result (tnode_t **rnodep, map_t *mdata)*/
/*
 *	back-end map for back-end result, collects up size needed for result evaluation
 *	returns 0 to stop walk, 1 to continue
 */
static int krocetc_bemap_result (tnode_t **rnodep, map_t *mdata)
{
	krocetc_priv_t *kpriv = (krocetc_priv_t *)mdata->target->priv;
	krocetc_resultsubhook_t *rh;

	rh = (krocetc_resultsubhook_t *)tnode_getchook (*rnodep, kpriv->resultsubhook);
	if (!rh) {
		nocc_internal ("krocetc_bemap_result(): missing resultsubhook!");
		return 0;
	}

	/* sub-map first */
	map_subbemap (tnode_nthsubaddr (*rnodep, 0), mdata);

	if (DA_CUR (rh->sublist)) {
		int *regfors = (int *)smalloc (DA_CUR (rh->sublist) * sizeof (int));
		int i;
		int rleft;
		int rused = 0;
		int max = 0;

		rleft = 3;						/* FIXME! */
		for (i=0; i<DA_CUR(rh->sublist); i++) {
			regfors[i] = krocetc_be_regsfor (*(DA_NTHITEM (rh->sublist, i)));
			if (regfors[i] > rleft) {
				nocc_warning ("krocetc_bemap_result(): fixme: wanted %d registers, got %d", regfors[i], rleft);
			}
			if ((regfors[i] + rused) > max) {
				max = regfors[i] + rused;
			}
			rleft--;
			rused++;
		}

		sfree (regfors);

		if (rused > max) {
			max = rused;
		}
		rh->eval_regs = max;				/* FIXME! */
		rh->result_regs = 1;				/* FIXME! -- assumption.. */
	} else {
		nocc_warning ("krocetc_bemap_result(): no sub-things in result..");
		rh->eval_regs = 0;
		rh->result_regs = 0;
	}

	return 0;
}
/*}}}*/
/*{{{  static int krocetc_codegen_result (tnode_t *rnode, codegen_t *cgen)*/
/*
 *	generates code for a result -- evaluates as necessary
 *	returns 0 to stop walk, 1 to continue
 */
static int krocetc_codegen_result (tnode_t *rnode, codegen_t *cgen)
{
	krocetc_priv_t *kpriv = (krocetc_priv_t *)krocetc_target.priv;
	krocetc_resultsubhook_t *rh = (krocetc_resultsubhook_t *)tnode_getchook (rnode, kpriv->resultsubhook);
	tnode_t *expr;
	int i;

#if 0
fprintf (stderr, "krocetc_codegen_result(): loading %d bits..\n", DA_CUR (rh->sublist));
#endif
	for (i=0; i<DA_CUR (rh->sublist); i++) {
		/* load this */
		codegen_subcodegen (*(DA_NTHITEM (rh->sublist, i)), cgen);
	}
	/* then call code-gen on the argument, if it exists */
	expr = tnode_nthsubof (rnode, 0);
	if (expr) {
		codegen_subcodegen (expr, cgen);
	}
	return 0;
}
/*}}}*/


/*{{{  static int krocetc_precode_special (tnode_t **spec, codegen_t *cgen)*/
/*
 *	does pre-code for a back-end special
 *	returns 0 to stop walk, 1 to continue
 */
static int krocetc_precode_special (tnode_t **spec, codegen_t *cgen)
{
	return 1;
}
/*}}}*/
/*{{{  static int krocetc_codegen_special (tnode_t *spec, codegen_t *cgen)*/
/*
 *	does code-gen for a back-end special
 *	returns 0 to stop walk, 1 to continue
 */
static int krocetc_codegen_special (tnode_t *spec, codegen_t *cgen)
{
	krocetc_priv_t *kpriv = (krocetc_priv_t *)cgen->target->priv;

	if (spec->tag == kpriv->tag_JENTRY) {
		if (kpriv->toplevelname) {
			char *belbl = krocetc_make_namedlabel (NameNameOf (kpriv->toplevelname));

			codegen_write_fmt (cgen, ".jentry\t%s\n", belbl);
			sfree (belbl);
		}
	} else if (spec->tag == kpriv->tag_DESCRIPTOR) {
		char **str = (char **)tnode_nthhookaddr (spec, 0);

		/* *str should be a descriptor line */
		codegen_write_fmt (cgen, ".descriptor\t\"%s\"\n", *str);
	}
	return 1;
}
/*}}}*/
/*{{{  static void krocetc_be_precode_seenproc (codegen_t *cgen, name_t *name, tnode_t *node)*/
/*
 *	called during pre-code traversal with names of PROC definitions
 */
static void krocetc_be_precode_seenproc (codegen_t *cgen, name_t *name, tnode_t *node)
{
	krocetc_priv_t *kpriv = (krocetc_priv_t *)cgen->target->priv;
	chook_t *fetransdeschook = tnode_lookupchookbyname ("fetrans:descriptor");

	kpriv->toplevelname = name;
	if (fetransdeschook) {
		void *dhook = tnode_getchook (node, fetransdeschook);

		if (dhook) {
			/* add descriptor special node */
			char *dstr = (char *)dhook;
			char *ch, *dh;

#if 0
fprintf (stderr, "krocetc_be_precode_seenproc(): seen descriptor [%s], doing back-end name trans\n", dstr);
#endif
			/* need to do back-end-name transform */
			for (ch = dstr; (*ch != '\0') && strncmp (ch, "PROC", 4); ch++);
			if (*ch == 'P') {
				/* found it */
				ch += 5;
				for (dh=ch; (*dh != '\0') && (*dh != ' '); dh++);
				if (*dh == ' ') {
					/* got PROC name, fix it */
					char *newdesc, *bename;

					newdesc = (char *)smalloc (strlen (dstr) + 6);
					*dh = '\0';
					dh++;		/* rest of descriptor */
					bename = krocetc_make_namedlabel (ch);
					*ch = '\0';
					sprintf (newdesc, "%s%s %s", dstr, bename, dh);
					tnode_setchook (node, fetransdeschook, newdesc);
					dhook = (void *)newdesc;
					dstr = newdesc;
				}
			}
#if 0
fprintf (stderr, "krocetc_be_precode_seenproc(): descriptor now [%s]\n", dstr);
#endif
			tnode_t *dspec = tnode_create (kpriv->tag_DESCRIPTOR, NULL, (void *)string_dup (dstr));

			parser_addtolist (kpriv->precodelist, dspec);
		}
	}
	return;
}
/*}}}*/


/*{{{  static char *krocetc_make_namedlabel (const char *lbl)*/
/*
 *	transforms an internal name into a label
 */
static char *krocetc_make_namedlabel (const char *lbl)
{
	int lbl_len = strlen (lbl);
	char *belbl = (char *)smalloc (lbl_len + 5);
	char *ch;

	strcpy (belbl, "O_");
	memcpy (belbl + 2, lbl, lbl_len + 1);
	for (ch = belbl + 2; *ch != '\0'; ch++) {
		switch (*ch) {
		case '.':
			*ch = '_';
			break;
		}
	}

	return belbl;
}
/*}}}*/
/*{{{  static void krocetc_coder_loadpointer (codegen_t *cgen, tnode_t *name, int offset)*/
/*
 *	loads a back-end pointer
 */
static void krocetc_coder_loadpointer (codegen_t *cgen, tnode_t *name, int offset)
{
	krocetc_priv_t *kpriv = (krocetc_priv_t *)(krocetc_target.priv);
	int ref_lexlevel, act_lexlevel;

#if 0
fprintf (stderr, "krocetc_coder_loadpointer(): kpriv->mapchook = %p\n", kpriv->mapchook);
#endif
	if (name->tag == krocetc_target.tag_NAMEREF) {
		/*{{{  loading pointer to a name*/
		krocetc_namehook_t *nh = (krocetc_namehook_t *)tnode_nthhookof (name, 0);
		tnode_t *fename = tnode_nthsubof (name, 0);
		tnode_t *fenamehook = (tnode_t *)tnode_getchook (fename, kpriv->mapchook);
		int local;

		ref_lexlevel = nh->lexlevel;
		act_lexlevel = cgen->target->be_blocklexlevel (fenamehook);
		local = (ref_lexlevel == act_lexlevel);

#if 0
fprintf (stderr, "krocetc_coder_loadpointer(): [%s] local=%d, ref_lexlevel=%d, act_lexlevel=%d\n", fename->tag->name, local, ref_lexlevel, act_lexlevel);
#endif
		if (!local) {
			/*{{{  non-local load*/
			codegen_callops (cgen, loadlexlevel, act_lexlevel);

			if (nh->indir == 0) {
				codegen_write_fmt (cgen, "\tldnlp\t%d\n", nh->ws_offset + offset);
			} else {
				int i;

				codegen_write_fmt (cgen, "\tldnl\t%d\n", nh->ws_offset);
				for (i=1; i<nh->indir; i++) {
					codegen_write_fmt (cgen, "\tldnl\t0\n");
				}
				if (offset) {
					codegen_write_fmt (cgen, "\tldnlp\t%d\n", offset);
				}
			}
			/*}}}*/
		} else {
			/*{{{  local load*/
			if (nh->indir == 0) {
				codegen_write_fmt (cgen, "\tldlp\t%d\n", nh->ws_offset + offset);
				krocetc_cgstate_tsdelta (cgen, 1);
			} else {
				int i;

				codegen_write_fmt (cgen, "\tldl\t%d\n", nh->ws_offset);
				krocetc_cgstate_tsdelta (cgen, 1);
				for (i=1; i<nh->indir; i++) {
					codegen_write_fmt (cgen, "\tldnl\t0\n");
				}
				if (offset) {
					codegen_write_fmt (cgen, "\tldnlp\t%d\n", offset);
				}
			}
			/*}}}*/
		}
		/*}}}*/
	} else if (name->tag == krocetc_target.tag_INDEXED) {
		/*{{{  load pointer into an indexed node (array typically)*/
		krocetc_indexedhook_t *ih = (krocetc_indexedhook_t *)tnode_nthhookof (name, 0);

		krocetc_coder_loadpointer (cgen, tnode_nthsubof (name, 0), 0);
		codegen_callops (cgen, loadname, tnode_nthsubof (name, 1), 0);
		if (ih->isize > 1) {
			codegen_callops (cgen, loadconst, ih->isize);
			codegen_write_fmt (cgen, "\tprod\n");
			krocetc_cgstate_tsdelta (cgen, -1);
		}
		codegen_write_fmt (cgen, "\tsum\n");
		krocetc_cgstate_tsdelta (cgen, -1);
		/*}}}*/
	} else if (name->tag == kpriv->tag_CONSTREF) {
		/*{{{  loading pointer to a constant*/
		krocetc_consthook_t *ch = (krocetc_consthook_t *)tnode_nthhookof (name, 0);
		
		krocetc_coder_loadlabaddr (cgen, ch->label);
		ch->labrefs++;
		/*}}}*/
	} else if (name->tag == krocetc_target.tag_NAME) {
		/*{{{  loading pointer to a name with no scope (specials only!)*/
		tnode_t *realname = tnode_nthsubof (name, 0);

		if (realname->tag == cgen->target->tag_STATICLINK) {
			codegen_write_fmt (cgen, "\tldlp\t0\n");
			krocetc_cgstate_tsdelta (cgen, 1);
		} else {
			nocc_warning ("krocetc_coder_loadpointer(): don\'t know how to load a pointer to name of [%s]", name->tag->name);
		}
		/*}}}*/
	} else {
		nocc_warning ("krocetc_coder_loadpointer(): don\'t know how to load a pointer to [%s]", name->tag->name);
	}
	return;
}
/*}}}*/
/*{{{  static void krocetc_coder_loadname (codegen_t *cgen, tnode_t *name, int offset)*/
/*
 *	loads a back-end name
 */
static void krocetc_coder_loadname (codegen_t *cgen, tnode_t *name, int offset)
{
	krocetc_priv_t *kpriv = (krocetc_priv_t *)krocetc_target.priv;

	if (name->tag == krocetc_target.tag_NAMEREF) {
		/*{{{  load name via reference*/
		krocetc_namehook_t *nh = (krocetc_namehook_t *)tnode_nthhookof (name, 0);
		int i;

		if (offset && !nh->indir) {
			codegen_write_fmt (cgen, "\tldl\t%d\n", nh->ws_offset + offset);
		} else {
			codegen_write_fmt (cgen, "\tldl\t%d\n", nh->ws_offset);
		}
		krocetc_cgstate_tsdelta (cgen, 1);
		for (i=0; i<nh->indir; i++) {
			if (offset && (i == (nh->indir - 1))) {
				codegen_write_fmt (cgen, "\tldnl\t%d\n", offset);
			} else {
				codegen_write_fmt (cgen, "\tldnl\t0\n");
			}
		}
		/*}}}*/
	} else if (name->tag == krocetc_target.tag_INDEXED) {
		/*{{{  load something out of an indexed node (array typically)*/
		krocetc_indexedhook_t *ih = (krocetc_indexedhook_t *)tnode_nthhookof (name, 0);

		krocetc_coder_loadpointer (cgen, tnode_nthsubof (name, 0), 0);
		krocetc_coder_loadname (cgen, tnode_nthsubof (name, 1), 0);
		if (ih->isize > 1) {
			codegen_callops (cgen, loadconst, ih->isize);
			codegen_write_fmt (cgen, "\tprod\n");
			krocetc_cgstate_tsdelta (cgen, -1);
		}
		codegen_write_fmt (cgen, "\tsum\n");
		krocetc_cgstate_tsdelta (cgen, -1);

		switch (ih->isize) {
		case 1:
			codegen_write_fmt (cgen, "\tlb\n");
			break;
		case 2:
			codegen_write_fmt (cgen, "\tlw\n");
			break;
		case 4:
			codegen_write_fmt (cgen, "\tldnl\t0\n");
			break;
		default:
			codegen_error (cgen, "krocetc_coder_loadname(): INDEXED: index size %d not supported here", ih->isize);
			break;
		}
		/*}}}*/
	} else if (name->tag == kpriv->tag_CONSTREF) {
		/*{{{  load constant via reference*/
		codegen_subcodegen (name, cgen);

		/*}}}*/
	} else if (name->tag == cgen->target->tag_NAME) {
		/*{{{  loading a name with no scope (specials only!)*/
		tnode_t *realname = tnode_nthsubof (name, 0);

		if (realname->tag == cgen->target->tag_STATICLINK) {
			krocetc_namehook_t *nh = (krocetc_namehook_t *)tnode_nthhookof (name, 0);

			codegen_write_fmt (cgen, "\tldl\t%d\n", nh->ws_offset);
			krocetc_cgstate_tsdelta (cgen, 1);
		} else {
			nocc_warning ("krocetc_coder_loadname(): don\'t know how to load a name of [%s]", name->tag->name);
		}
		/*}}}*/
	} else if (name->tag == cgen->target->tag_RESULT) {
		/*{{{  loading some evaluated result*/
		codegen_subcodegen (name, cgen);

		/*}}}*/
	} else {
		nocc_warning ("krocetc_coder_loadname(): don\'t know how to load [%s]", name->tag->name);
	}
	return;
}
/*}}}*/
/*{{{  static void krocetc_coder_loadparam (codegen_t *cgen, tnode_t *node, codegen_parammode_t pmode)*/
/*
 *	loads a back-end something as a parameter
 */
static void krocetc_coder_loadparam (codegen_t *cgen, tnode_t *node, codegen_parammode_t pmode)
{
	switch (pmode) {
	case PARAM_INVALID:
		codegen_error (cgen, "krocetc_coder_loadparam(): invalid parameter mode");
		break;
	case PARAM_REF:
		krocetc_coder_loadpointer (cgen, node, 0);
		break;
	case PARAM_VAL:
		krocetc_coder_loadname (cgen, node, 0);
		break;
	}
	return;
}
/*}}}*/
/*{{{  static void krocetc_coder_loadlocalpointer (codegen_t *cgen, int offset)*/
/*
 *	loads a pointer to something in the local workspace
 */
static void krocetc_coder_loadlocalpointer (codegen_t *cgen, int offset)
{
	codegen_write_fmt (cgen, "\tldlp\t%d\n", offset);
	krocetc_cgstate_tsdelta (cgen, 1);
	return;
}
/*}}}*/
/*{{{  static void krocetc_coder_loadlexlevel (codegen_t *cgen, int lexlevel)*/
/*
 *	loads a pointer to the workspace at "lexlevel", does this through the staticlinks
 */
static void krocetc_coder_loadlexlevel (codegen_t *cgen, int lexlevel)
{
	tnode_t *be_blk = DA_NTHITEM (cgen->be_blks, DA_CUR (cgen->be_blks) - 1);
	int blk_ll = cgen->target->be_blocklexlevel (be_blk);
	int ll;

	if (blk_ll == lexlevel) {
		return;
	}
	for (ll=blk_ll; ll > lexlevel; ll--) {
		tnode_t *thisblk = DA_NTHITEM (cgen->be_blks, ll);
		tnode_t *statics = tnode_nthsubof (thisblk, 1);
		tnode_t *slink;

#if 0
fprintf (stderr, "krocetc_coder_loadlexlevel(): in %d, loading %d..\n", ll, ll-1);
#endif
		if (!statics) {
			nocc_internal ("krocetc_coder_loadlexlevel(): no statics in this block..");
			return;
		}
		slink = treeops_findtwointree (statics, cgen->target->tag_NAME, cgen->target->tag_STATICLINK);
		if (!slink) {
			nocc_internal ("krocetc_coder_loadlexlevel(): no static-link in this block..");
			return;
		}
#if 0
fprintf (stderr, "krocetc_coder_loadlexlevel(): found staticlink..  loading it..\n");
#endif
		codegen_callops (cgen, loadname, slink, 0);
	}

	return;
}
/*}}}*/
/*{{{  static void krocetc_coder_storepointer (codegen_t *cgen, tnode_t *name, int offset)*/
/*
 *	stores a back-end pointer
 */
static void krocetc_coder_storepointer (codegen_t *cgen, tnode_t *name, int offset)
{
	nocc_warning ("krocetc_coder_storepointer(): don\'t know how to store a pointer to [%s]", name->tag->name);
	return;
}
/*}}}*/
/*{{{  static void krocetc_coder_storename (codegen_t *cgen, tnode_t *name, int offset)*/
/*
 *	stores a back-end name
 */
static void krocetc_coder_storename (codegen_t *cgen, tnode_t *name, int offset)
{
	if (name->tag == krocetc_target.tag_NAMEREF) {
		/*{{{  store into a name reference*/
		krocetc_namehook_t *nh = (krocetc_namehook_t *)tnode_nthhookof (name, 0);
		int i;

		switch (nh->indir) {
		case 0:
			codegen_write_fmt (cgen, "\tstl\t%d\n", nh->ws_offset + offset);
			krocetc_cgstate_tsdelta (cgen, -1);
			break;
		default:
			codegen_write_fmt (cgen, "\tldl\t%d\n", nh->ws_offset);
			krocetc_cgstate_tsdelta (cgen, 1);
			for (i=0; i<(nh->indir - 1); i++) {
				codegen_write_fmt (cgen, "\tldnl\t0\n");
			}
			codegen_write_fmt (cgen, "\tstnl\t%d\n", offset);
			krocetc_cgstate_tsdelta (cgen, -2);
			break;
		}
		/*}}}*/
	} else if (name->tag == krocetc_target.tag_INDEXED) {
		/*{{{  store into an indexed node*/
		krocetc_indexedhook_t *ih = (krocetc_indexedhook_t *)tnode_nthhookof (name, 0);

		if (!ih->isize) {
			/*{{{  offset*/
			tnode_t *offsexp = tnode_nthsubof (name, 1);

			/* load a pointer to the base */
			krocetc_coder_loadpointer (cgen, tnode_nthsubof (name, 0), 0);

			if (!offsexp) {
				/* constant offset */
				codegen_write_fmt (cgen, "\tstnl\t%d\n", ih->offset);
				krocetc_cgstate_tsdelta (cgen, -2);
			} else {
				/* variable offset */
				codegen_callops (cgen, comment, "missing store variable offset in INDEXED node");
			}
			/*}}}*/
		} else {
			/*{{{  indexed*/
			/* load BYTE index */
			codegen_callops (cgen, loadname, tnode_nthsubof (name, 1), 0);
			if (ih->isize > 1) {
				codegen_callops (cgen, loadconst, ih->isize);
				codegen_callops (cgen, tsecondary, I_PROD);		/* scale */
			}

			/* load a pointer to the base */
			codegen_callops (cgen, loadpointer, tnode_nthsubof (name, 0), 0);
			codegen_callops (cgen, tsecondary, I_SUM);		/* add */

			switch (ih->isize) {
			case 1:
				codegen_write_fmt (cgen, "\tsb\n");
				krocetc_cgstate_tsdelta (cgen, -2);
				break;
			case 2:
				codegen_write_fmt (cgen, "\tsw\n");
				krocetc_cgstate_tsdelta (cgen, -2);
				break;
			case 4:
				codegen_write_fmt (cgen, "\tstnl\t%d\n", offset);
				krocetc_cgstate_tsdelta (cgen, -2);
				break;
			default:
				codegen_error (cgen, "krocetc_coder_storename(): INDEXED: index size %d not supported here", ih->isize);
				break;
			}
			/*}}}*/
		}

		/*}}}*/
	} else {
		nocc_warning ("krocetc_coder_storename(): don\'t know how to store into a [%s]", name->tag->name);
	}
	return;
}
/*}}}*/
/*{{{  static void krocetc_coder_storelocal (codegen_t *cgen, int ws_offset)*/
/*
 *	stores top of stack in a local workspace slot
 */
static void krocetc_coder_storelocal (codegen_t *cgen, int ws_offset)
{
	codegen_write_fmt (cgen, "\tstl\t%d\n", ws_offset);
	krocetc_cgstate_tsdelta (cgen, -1);
	return;
}
/*}}}*/
/*{{{  static void krocetc_coder_loadconst (codegen_t *cgen, int val)*/
/*
 *	loads a constant
 */
static void krocetc_coder_loadconst (codegen_t *cgen, int val)
{
	codegen_write_fmt (cgen, "\tldc\t%d\n", val);
	krocetc_cgstate_tsdelta (cgen, 1);
	return;
}
/*}}}*/
/*{{{  static void krocetc_coder_addconst (codegen_t *cgen, int val)*/
/*
 *	adds a constant to the thing on the top of the stack
 */
static void krocetc_coder_addconst (codegen_t *cgen, int val)
{
	codegen_write_fmt (cgen, "\tadc\t%d\n", val);
	krocetc_cgstate_tsdelta (cgen, 0);
	return;
}
/*}}}*/
/*{{{  static void krocetc_coder_wsadjust (codegen_t *cgen, int adjust)*/
/*
 *	generates workspace adjustments
 */
static void krocetc_coder_wsadjust (codegen_t *cgen, int adjust)
{
	codegen_write_fmt (cgen, "\tajw\t%d\n", adjust);
	return;
}
/*}}}*/
/*{{{  static void krocetc_coder_comment (codegen_t *cgen, const char *fmt, ...)*/
/*
 *	generates a comment
 */
static void krocetc_coder_comment (codegen_t *cgen, const char *fmt, ...)
{
	char *buf = (char *)smalloc (1024);
	va_list ap;
	int i;

	va_start (ap, fmt);
	strcpy (buf, "; ");
	i = vsnprintf (buf + 2, 1020, fmt, ap);
	va_end (ap);

	if (i > 0) {
		i += 2;
		strcpy (buf + i, "\n");
		codegen_write_bytes (cgen, buf, i + 1);
	}

	sfree (buf);
	return;
}
/*}}}*/
/*{{{  static void krocetc_coder_setwssize (codegen_t *cgen, int ws, int adjust)*/
/*
 *	generates workspace requirements
 */
static void krocetc_coder_setwssize (codegen_t *cgen, int ws, int adjust)
{
	codegen_write_fmt (cgen, ".setws\t%d, %d\n", ws, adjust);
	return;
}
/*}}}*/
/*{{{  static void krocetc_coder_setvssize (codegen_t *cgen, int vs)*/
/*
 *	generates vectorspace requirements
 */
static void krocetc_coder_setvssize (codegen_t *cgen, int vs)
{
	codegen_write_fmt (cgen, ".setvs\t%d\n", vs);
	return;
}
/*}}}*/
/*{{{  static void krocetc_coder_setmssize (codegen_t *cgen, int ms)*/
/*
 *	generates mobilespace requirements
 */
static void krocetc_coder_setmssize (codegen_t *cgen, int ms)
{
	codegen_write_fmt (cgen, ".setms\t%d\n", ms);
	return;
}
/*}}}*/
/*{{{  static void krocetc_coder_setnamedlabel (codegen_t *cgen, const char *lbl)*/
/*
 *	sets a named label
 */
static void krocetc_coder_setnamedlabel (codegen_t *cgen, const char *lbl)
{
	char *belbl = krocetc_make_namedlabel (lbl);

	codegen_write_fmt (cgen, ".setnamedlabel\t\"%s\"\n", belbl);
	sfree (belbl);
	return;
}
/*}}}*/
/*{{{  static void krocetc_coder_setlabel (codegen_t *cgen, int lbl)*/
/*
 *	sets a numeric label
 */
static void krocetc_coder_setlabel (codegen_t *cgen, int lbl)
{
	codegen_write_fmt (cgen, ".setlabel\t%d\n", lbl);
	return;
}
/*}}}*/
/*{{{  static void krocetc_coder_procreturn (codegen_t *cgen, int adjust)*/
/*
 *	generates a procedure return
 */
static void krocetc_coder_procreturn (codegen_t *cgen, int adjust)
{
	codegen_write_fmt (cgen, "\tret\t%d\n", adjust);
	return;
}
/*}}}*/
/*{{{  static void krocetc_coder_tsecondary (codegen_t *cgen, int ins)*/
/*
 *	generates code for a secondary instruction
 */
static void krocetc_coder_tsecondary (codegen_t *cgen, int ins)
{
	transinstr_e tins = (transinstr_e)ins;

	switch (tins) {
	case I_OUT:
		codegen_write_string (cgen, "\tout\n");
		krocetc_cgstate_tsdelta (cgen, -3);
		break;
	case I_IN:
		codegen_write_string (cgen, "\tin\n");
		krocetc_cgstate_tsdelta (cgen, -3);
		break;
	case I_MOVE:
		codegen_write_string (cgen, "\tmove\n");
		krocetc_cgstate_tsdelta (cgen, -3);
		break;
	case I_STARTP:
		codegen_write_string (cgen, "\tstartp\n");
		krocetc_cgstate_tsdelta (cgen, -2);
		break;
	case I_ENDP:
		codegen_write_string (cgen, "\tendp\n");
		krocetc_cgstate_tsdelta (cgen, -1);
		break;
	case I_RESCHEDULE:
		codegen_write_string (cgen, "\t.reschedule\n");
		break;
	case I_BOOLINVERT:
		codegen_write_string (cgen, "\t.boolinvert\n");
		break;
	case I_ADD:
		codegen_write_string (cgen, "\tadd\n");
		krocetc_cgstate_tsdelta (cgen, -1);
		break;
	case I_SUB:
		codegen_write_string (cgen, "\tsub\n");
		krocetc_cgstate_tsdelta (cgen, -1);
		break;
	case I_MUL:
		codegen_write_string (cgen, "\tmul\n");
		krocetc_cgstate_tsdelta (cgen, -1);
		break;
	case I_DIV:
		codegen_write_string (cgen, "\tdiv\n");
		krocetc_cgstate_tsdelta (cgen, -1);
		break;
	case I_REM:
		codegen_write_string (cgen, "\trem\n");
		krocetc_cgstate_tsdelta (cgen, -1);
		break;
	case I_SUM:
		codegen_write_string (cgen, "\tsum\n");
		krocetc_cgstate_tsdelta (cgen, -1);
		break;
	case I_DIFF:
		codegen_write_string (cgen, "\tdiff\n");
		krocetc_cgstate_tsdelta (cgen, -1);
		break;
	case I_PROD:
		codegen_write_string (cgen, "\tprod\n");
		krocetc_cgstate_tsdelta (cgen, -1);
		break;
	case I_STOPP:
		codegen_write_string (cgen, "\tstopp\n");
		break;
	case I_RUNP:
		codegen_write_string (cgen, "\trunp\n");
		krocetc_cgstate_tsdelta (cgen, -1);
		break;
	case I_SETERR:
		codegen_write_string (cgen, "\tseterr\n");
		break;
	case I_GT:
		codegen_write_string (cgen, "\tgt\n");
		krocetc_cgstate_tsdelta (cgen, -1);
		break;
	case I_LT:
		codegen_write_string (cgen, "\tlt\n");
		krocetc_cgstate_tsdelta (cgen, -1);
		break;
	case I_MALLOC:
		codegen_write_string (cgen, "\tmalloc\n");
		krocetc_cgstate_tsdelta (cgen, 0);
		break;
	case I_MRELEASE:
		codegen_write_string (cgen, "\tmrelease\n");
		krocetc_cgstate_tsdelta (cgen, -1);
		break;
	case I_TRAP:
		codegen_write_string (cgen, "\ttrap\n");
		krocetc_cgstate_tsdelta (cgen, 0);
		break;
	default:
		codegen_write_fmt (cgen, "\tFIXME: tsecondary %d\n", ins);
		break;
	}
	return;
}
/*}}}*/
/*{{{  static void krocetc_coder_callnamedlabel (codegen_t *cgen, const char *label, int ws_adjust)*/
/*
 *	generates code to call a named label
 */
static void krocetc_coder_callnamedlabel (codegen_t *cgen, const char *label, int ws_adjust)
{
	char *belbl = krocetc_make_namedlabel (label);

	codegen_write_fmt (cgen, "\tcall\t%s, %d\n", belbl, ws_adjust);
	return;
}
/*}}}*/
/*{{{  static void krocetc_coder_calllabel (codegen_t *cgen, int label, int ws_adjust)*/
/*
 *	generates code to call a numeric label
 */
static void krocetc_coder_calllabel (codegen_t *cgen, int label, int ws_adjust)
{
	codegen_write_fmt (cgen, "\tcall\t%d, %d\n", label, ws_adjust);
	return;
}
/*}}}*/
/*{{{  static void krocetc_coder_constblock (codegen_t *cgen, void *ptr, int size)*/
/*
 *	generates a constant block of data
 */
static void krocetc_coder_constblock (codegen_t *cgen, void *ptr, int size)
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
/*{{{  static void krocetc_coder_loadlabaddr (codegen_t *cgen, int lbl)*/
/*
 *	loads the address of a label
 */
static void krocetc_coder_loadlabaddr (codegen_t *cgen, int lbl)
{
	codegen_write_fmt (cgen, ".loadlabaddr\t%d\n", lbl);
	krocetc_cgstate_tsdelta (cgen, 1);
	return;
}
/*}}}*/
/*{{{  static void krocetc_coder_branch (codegen_t *cgen, int ins, int lbl)*/
/*
 *	generates a branch instruction
 */
static void krocetc_coder_branch (codegen_t *cgen, int ins, int lbl)
{
	transinstr_e tins = (transinstr_e)ins;

	switch (tins) {
	case I_J:
		codegen_write_fmt (cgen, "\tj\t%d\n", lbl);
		break;
	case I_CJ:
		codegen_write_fmt (cgen, "\tcj\t%d\n", lbl);
		krocetc_cgstate_tsdelta (cgen, -1);
		break;
	default:
		codegen_write_fmt (cgen, "\tFIXME: branch %d\n", ins);
		break;
	}
	return;
}
/*}}}*/



/*{{{  static int krocetc_be_codegen_init (codegen_t *cgen, lexfile_t *srcfile)*/
/*
 *	initialises back-end code generation for KRoC/ETC target
 */
static int krocetc_be_codegen_init (codegen_t *cgen, lexfile_t *srcfile)
{
	krocetc_priv_t *kpriv = (krocetc_priv_t *)cgen->target->priv;
	coderops_t *cops;
	char hostnamebuf[128];
	char timebuf[128];

#if 0
fprintf (stderr, "krocetc_be_codegen_init(): here!\n");
#endif
	codegen_write_fmt (cgen, ";\n;\t%s\n", cgen->fname);
	codegen_write_fmt (cgen, ";\tcompiled from %s\n", srcfile->filename ?: "(unknown)");
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
	codegen_write_fmt (cgen, ";\ton host %s at %s\n", hostnamebuf, timebuf);
	codegen_write_fmt (cgen, ";\tsource language: %s, target: %s\n", parser_langname (srcfile) ?: "(unknown)", compopts.target_str);
	codegen_write_string (cgen, ";\n\n");

	cops = (coderops_t *)smalloc (sizeof (coderops_t));
	cops->loadpointer = krocetc_coder_loadpointer;
	cops->loadname = krocetc_coder_loadname;
	cops->loadparam = krocetc_coder_loadparam;
	cops->loadlocalpointer = krocetc_coder_loadlocalpointer;
	cops->loadlexlevel = krocetc_coder_loadlexlevel;
	cops->storepointer = krocetc_coder_storepointer;
	cops->storename = krocetc_coder_storename;
	cops->storelocal = krocetc_coder_storelocal;
	cops->loadconst = krocetc_coder_loadconst;
	cops->addconst = krocetc_coder_addconst;
	cops->wsadjust = krocetc_coder_wsadjust;
	cops->comment = krocetc_coder_comment;
	cops->setwssize = krocetc_coder_setwssize;
	cops->setvssize = krocetc_coder_setvssize;
	cops->setmssize = krocetc_coder_setmssize;
	cops->setnamedlabel = krocetc_coder_setnamedlabel;
	cops->callnamedlabel = krocetc_coder_callnamedlabel;
	cops->setlabel = krocetc_coder_setlabel;
	cops->calllabel = krocetc_coder_calllabel;
	cops->procreturn = krocetc_coder_procreturn;
	cops->tsecondary = krocetc_coder_tsecondary;
	cops->loadlabaddr = krocetc_coder_loadlabaddr;
	cops->branch = krocetc_coder_branch;

	cgen->cops = cops;

	/*
	 *	create pre-code node if not already here -- constants go at the end
	 */
	if (!kpriv->precodelist) {
		tnode_t *precode = tnode_create (kpriv->tag_PRECODE, NULL, parser_newlistnode (srcfile), *(cgen->cinsertpoint));

		*(cgen->cinsertpoint) = precode;
		kpriv->precodelist = tnode_nthsubof (precode, 0);
	}

	if (!compopts.notmainmodule) {
		/* insert JENTRY marker first */
		tnode_t *jentry = tnode_create (kpriv->tag_JENTRY, NULL, NULL);

		parser_addtolist (kpriv->precodelist, jentry);
	}

	return 0;
}
/*}}}*/
/*{{{  static int krocetc_be_codegen_final (codegen_t *cgen, lexfile_t *srcfile)*/
/*
 *	shutdown back-end code generation for KRoC/ETC target
 */
static int krocetc_be_codegen_final (codegen_t *cgen, lexfile_t *srcfile)
{
	sfree (cgen->cops);
	cgen->cops = NULL;

	codegen_write_string (cgen, "\n;\n;\tend of compilation\n;\n");
	return 0;
}
/*}}}*/


/*{{{  static int krocetc_target_init (target_t *target)*/
/*
 *	initialises the KRoC-ETC target
 *	return 0 on success, non-zero on error
 */
static int krocetc_target_init (target_t *target)
{
	tndef_t *tnd;
	compops_t *cops;
	krocetc_priv_t *kpriv;
	int i;

#if 0
fprintf (stderr, "krocetc_target_init(): here!\n");
#endif
	if (target->initialised) {
		nocc_internal ("krocetc_target_init(): already initialised!");
		return 1;
	}

	kpriv = (krocetc_priv_t *)smalloc (sizeof (krocetc_priv_t));
	kpriv->precodelist = NULL;
	kpriv->toplevelname = NULL;
	kpriv->mapchook = tnode_lookupornewchook ("map:mapnames");
	kpriv->resultsubhook = tnode_lookupornewchook ("krocetc:resultsubhook");
	kpriv->resultsubhook->chook_dumptree = krocetc_resultsubhook_dumptree;
	kpriv->resultsubhook->chook_free = krocetc_resultsubhook_free;
	kpriv->maxtsdepth = 3;
	kpriv->maxfpdepth = 3;
	kpriv->options.stoperrormode = 0;			/* halt error-mode by default */

	target->priv = (void *)kpriv;

	krocetc_init_options (kpriv);
#if 0
fprintf (stderr, "krocetc_target_init(): kpriv->mapchook = %p\n", kpriv->mapchook);
#endif

	/* setup back-end nodes */
	/*{{{  krocetc:name -- KROCETCNAME*/
	i = -1;
	tnd = tnode_newnodetype ("krocetc:name", &i, 2, 0, 1, 0);
	tnd->hook_dumptree = krocetc_namehook_dumptree;
	cops = tnode_newcompops ();
	cops->bytesfor = krocetc_bytesfor_name;
	tnd->ops = cops;
	i = -1;
	target->tag_NAME = tnode_newnodetag ("KROCETCNAME", &i, tnd, 0);
	/*}}}*/
	/*{{{  krocetc:nameref -- KROCETCNAMEREF*/
	i = -1;
	tnd = tnode_newnodetype ("krocetc:nameref", &i, 1, 0, 1, 0);
	tnd->hook_dumptree = krocetc_namehook_dumptree;
	cops = tnode_newcompops ();
	cops->codegen = krocetc_codegen_nameref;
	tnd->ops = cops;
	i = -1;
	target->tag_NAMEREF = tnode_newnodetag ("KROCETCNAMEREF", &i, tnd, 0);
	/*}}}*/
	/*{{{  krocetc:block -- KROCETCBLOCK*/
	i = -1;
	tnd = tnode_newnodetype ("krocetc:block", &i, 2, 0, 1, 0);
	tnd->hook_dumptree = krocetc_blockhook_dumptree;
	cops = tnode_newcompops ();
	cops->preallocate = krocetc_preallocate_block;
	cops->precode = krocetc_precode_block;
	cops->codegen = krocetc_codegen_block;
	tnd->ops = cops;
	i = -1;
	target->tag_BLOCK = tnode_newnodetag ("KROCETCBLOCK", &i, tnd, 0);
	/*}}}*/
	/*{{{  krocetc:const -- KROCETCCONST*/
	i = -1;
	tnd = tnode_newnodetype ("krocetc:const", &i, 1, 0, 1, 0);
	tnd->hook_dumptree = krocetc_consthook_dumptree;
	cops = tnode_newcompops ();
	cops->precode = krocetc_precode_const;
	cops->codegen = krocetc_codegen_const;
	tnd->ops = cops;
	i = -1;
	target->tag_CONST = tnode_newnodetag ("KROCETCCONST", &i, tnd, 0);
	/*}}}*/
	/*{{{  krocetc:precode -- KROCETCPRECODE*/
	i = -1;
	tnd = tnode_newnodetype ("krocetc:precode", &i, 2, 0, 0, 0);
	i = -1;
	kpriv->tag_PRECODE = tnode_newnodetag ("KROCETCPRECODE", &i, tnd, 0);
	/*}}}*/
	/*{{{  krocetc:special -- KROCETCJENTRY, KROCETCDESCRIPTOR*/
	i = -1;
	tnd = tnode_newnodetype ("krocetc:special", &i, 0, 0, 1, 0);
	tnd->hook_dumptree = krocetc_specialhook_dumptree;
	cops = tnode_newcompops ();
	cops->precode = krocetc_precode_special;
	cops->codegen = krocetc_codegen_special;
	tnd->ops = cops;
	i = -1;
	kpriv->tag_JENTRY = tnode_newnodetag ("KROCETCJENTRY", &i, tnd, 0);
	i = -1;
	kpriv->tag_DESCRIPTOR = tnode_newnodetag ("KROCETCDESCRIPTOR", &i, tnd, 0);
	/*}}}*/
	/*{{{  krocetc:constref -- KROCETCCONSTREF*/
	i = -1;
	tnd = tnode_newnodetype ("krocetc:constref", &i, 0, 0, 1, 0);
	tnd->hook_dumptree = krocetc_consthook_dumptree;
	cops = tnode_newcompops ();
	cops->codegen = krocetc_codegen_constref;
	tnd->ops = cops;
	i = -1;
	kpriv->tag_CONSTREF = tnode_newnodetag ("KROCETCCONSTREF", &i, tnd, 0);
	/*}}}*/
	/*{{{  krocetc:indexed -- KROCETCINDEXED*/
	i = -1;
	tnd = tnode_newnodetype ("krocetc:indexed", &i, 2, 0, 1, 0);
	tnd->hook_dumptree = krocetc_indexedhook_dumptree;
	i = -1;
	target->tag_INDEXED = tnode_newnodetag ("KROCETCINDEXED", &i, tnd, 0);
	/*}}}*/
	/*{{{  krocetc:blockref -- KROCETCBLOCKREF*/
	i = -1;
	tnd = tnode_newnodetype ("krocetc:blockref", &i, 1, 0, 1, 0);
	tnd->hook_dumptree = krocetc_blockrefhook_dumptree;
	i = -1;
	target->tag_BLOCKREF = tnode_newnodetag ("KROCETCBLOCKREF", &i, tnd, 0);
	/*}}}*/
	/*{{{  krocetc:staticlink -- KROCETCSTATICLINKk*/
	i = -1;
	tnd = tnode_newnodetype ("krocetc:staticlink", &i, 0, 0, 0, 0);
	i = -1;
	target->tag_STATICLINK = tnode_newnodetag ("KROCETCSTATICLINK", &i, tnd, 0);
	/*}}}*/
	/*{{{  krocetc:result -- KROCETCRESULT*/
	i = -1;
	tnd = tnode_newnodetype ("krocetc:result", &i, 1, 0, 0, 0);
	cops = tnode_newcompops ();
	cops->namemap = krocetc_namemap_result;
	cops->bemap = krocetc_bemap_result;
	cops->codegen = krocetc_codegen_result;
	tnd->ops = cops;
	i = -1;
	target->tag_RESULT = tnode_newnodetag ("KROCETCRESULT", &i, tnd, 0);
	/*}}}*/

	target->initialised = 1;
	return 0;
}
/*}}}*/




