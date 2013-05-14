/*
 *	krocllvm.c -- KRoC/LLVM back-end
 *	Copyright (C) 2010-2013 Fred Barnes <frmb@kent.ac.uk>
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
#include "fhandle.h"
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
#include "transputer.h"
#include "codegen.h"
#include "allocate.h"

/*}}}*/
/*{{{  local constants*/

#define KROCLLVM_MAX_DEPTH 32

/*}}}*/
/*{{{  forward decls*/
static int krocllvm_target_init (target_t *target);

static tnode_t *krocllvm_name_create (tnode_t *fename, tnode_t *body, map_t *mdata, int asize_wsh, int asize_wsl, int asize_vs, int asize_ms, int tsize, int ind);
static tnode_t *krocllvm_nameref_create (tnode_t *bename, map_t *mdata);
static tnode_t *krocllvm_block_create (tnode_t *body, map_t *mdata, tnode_t *slist, int lexlevel);
static tnode_t *krocllvm_const_create (tnode_t *val, map_t *mdata, void *data, int size, typecat_e typecat);
static tnode_t *krocllvm_indexed_create (tnode_t *base, tnode_t *index, int isize, int offset);
static tnode_t *krocllvm_blockref_create (tnode_t *block, tnode_t *body, map_t *mdata);
static tnode_t *krocllvm_result_create (tnode_t *expr, map_t *mdata);
static void krocllvm_inresult (tnode_t **nodep, map_t *mdata);

static int krocllvm_be_codegen_init (codegen_t *cgen, lexfile_t *srcfile);
static int krocllvm_be_codegen_final (codegen_t *cgen, lexfile_t *srcfile);
static int krocllvm_be_allocsize (tnode_t *node, int *pwsh, int *pwsl, int *pvs, int *pms);
static int krocllvm_be_typesize (tnode_t *node, int *typesize, int *indir);
static void krocllvm_be_settypecat (tnode_t *bename, typecat_e typecat);
static void krocllvm_be_gettypecat (tnode_t *bename, typecat_e *tcptr);
static void krocllvm_be_setoffsets (tnode_t *bename, int ws_offset, int vs_offset, int ms_offset, int ms_shadow);
static void krocllvm_be_getoffsets (tnode_t *bename, int *wsop, int *vsop, int *msop, int *mssp);
static int krocllvm_be_blocklexlevel (tnode_t *blk);
static void krocllvm_be_setblocksize (tnode_t *blk, int ws, int ws_offs, int vs, int ms, int adjust);
static void krocllvm_be_getblocksize (tnode_t *blk, int *wsp, int *wsoffsp, int *vsp, int *msp, int *adjp, int *elabp);
static tnode_t *krocllvm_be_getorgnode (tnode_t *node);
static tnode_t **krocllvm_be_blockbodyaddr (tnode_t *blk);
static int krocllvm_be_regsfor (tnode_t *benode);

static void krocllvm_be_precode_seenproc (codegen_t *cgen, name_t *name, tnode_t *node);

/*}}}*/

/*{{{  target_t for this target*/
target_t krocllvm_target = {
	.initialised =	0,
	.name =		"krocllvm",
	.tarch =		"llvm",
	.tvendor =	"kroc",
	.tos =		NULL,
	.desc =		"KRoC LLVM",
	.extn =		"ll",
	.tcap = {
		.can_do_fp = 1,
		.can_do_dmem = 1,
	},
	.bws = {
		.ds_min = 12,
		.ds_io = 16,
		.ds_altio = 16,
		.ds_wait = 24,
		.ds_max = 24
	},
	.aws = {
		.as_alt = 4,
		.as_par = 12,
	},

	.chansize =	4,
	.charsize =	1,
	.intsize =	4,
	.pointersize =	4,
	.slotsize =	4,
	.structalign =	4,
	.maxfuncreturn =	0,
	.skipallocate =	0,

	.tag_NAME =	NULL,
	.tag_NAMEREF =	NULL,
	.tag_BLOCK =	NULL,
	.tag_CONST =	NULL,
	.tag_INDEXED =	NULL,
	.tag_BLOCKREF =	NULL,
	.tag_STATICLINK =	NULL,
	.tag_RESULT =	NULL,

	.init =		krocllvm_target_init,
	.newname =	krocllvm_name_create,
	.newnameref =	krocllvm_nameref_create,
	.newblock =	krocllvm_block_create,
	.newconst =	krocllvm_const_create,
	.newindexed =	krocllvm_indexed_create,
	.newblockref =	krocllvm_blockref_create,
	.newresult =	krocllvm_result_create,
	.inresult =	krocllvm_inresult,

	.be_getorgnode =		krocllvm_be_getorgnode,
	.be_blockbodyaddr =	krocllvm_be_blockbodyaddr,
	.be_allocsize =		krocllvm_be_allocsize,
	.be_typesize =		krocllvm_be_typesize,
	.be_settypecat =		krocllvm_be_settypecat,
	.be_gettypecat =		krocllvm_be_gettypecat,
	.be_setoffsets =		krocllvm_be_setoffsets,
	.be_getoffsets =		krocllvm_be_getoffsets,
	.be_blocklexlevel =	krocllvm_be_blocklexlevel,
	.be_setblocksize =	krocllvm_be_setblocksize,
	.be_getblocksize =	krocllvm_be_getblocksize,
	.be_codegen_init =	krocllvm_be_codegen_init,
	.be_codegen_final =	krocllvm_be_codegen_final,

	.be_precode_seenproc =	krocllvm_be_precode_seenproc,

	.be_do_betrans =		NULL,
	.be_do_premap =		NULL,
	.be_do_namemap =		NULL,
	.be_do_bemap =		NULL,
	.be_do_preallocate =	NULL,
	.be_do_precode =		NULL,
	.be_do_codegen =		NULL,

	.priv =		NULL
};

/*}}}*/
/*{{{  private types*/
typedef struct TAG_krocllvm_namehook {
	int lexlevel;				/* lexical level */
	int alloc_wsh;				/* allocation in high-workspace */
	int alloc_wsl;				/* allocation in low-workspace */
	int typesize;				/* size of the actual type (if known) */
	int indir;				/* indirection count (0 = real-thing, 1 = pointer, 2 = pointer-pointer, etc.) */
	int ws_offset;				/* workspace offset in current block */
	typecat_e typecat;			/* type category */
} krocllvm_namehook_t;

typedef struct TAG_krocllvm_blockhook {
	int lexlevel;				/* lexical level */
	int alloc_ws;				/* workspace requirements */
	int static_adjust;			/* adjustment for statics (e.g. PROC params, etc.) */
	int ws_offset;				/* workspace offset for the block (includes static-adjust) */
	int entrylab;				/* entry-point label */
	int addstaticlink;			/* whether it needs a staticlink */
	int addfbp;				/* whether it needs a FORK barrier */
} krocllvm_blockhook_t;

typedef struct TAG_krocllvm_blockrefhook {
	tnode_t *block;
} krocllvm_blockrefhook_t;

typedef struct TAG_krocllvm_consthook {
	void *byteptr;
	int size;				/* constant size (bytes) */
	int label;
	int labrefs;				/* number of references to the label */
	tnode_t *orgnode;
	typecat_e typecat;			/* type category for constant */
} krocllvm_consthook_t;

typedef struct TAG_krocllvm_indexedhook {
	int isize;				/* index size */
	int offset;				/* offset */
} krocllvm_indexedhook_t;

typedef struct TAG_krocllvm_memspacehook {
	int ws;					/* bytes of workspace */
	int adjust;				/* workspace adjustment (PROC entry/exit) */
	int vs;					/* bytes of vectorspace (not used anymore) */
	int ms;					/* bytes of mobilespace (not used anymore) */
} krocllvm_memspacehook_t;

typedef struct TAG_krocllvm_priv {
	ntdef_t *tag_PRECODE;
	ntdef_t *tag_CONSTREF;
	ntdef_t *tag_JENTRY;
	ntdef_t *tag_DESCRIPTOR;
	ntdef_t *tag_FBP;			/* fork-barrier pointer */
	tnode_t *precodelist;
	tnode_t *postcodelist;
	name_t *toplevelname;

	chook_t *mapchook;
	chook_t *resultsubhook;

	lexfile_t *lastfile;

	struct {
		unsigned int stoperrormode : 1;
	} options;

	int regcount;				/* general register numbering */
	int labcount;				/* sub-label numbering */
	int tmpcount;				/* temporary register numbering */
	char *lastfunc;				/* last function name */
	char *lastdesc;				/* last descriptor text */

	krocllvm_memspacehook_t *tl_mem;	/* top-level memory requirements (last PROC seen) */
} krocllvm_priv_t;

typedef struct TAG_krocllvm_resultsubhook {
	int result_regs;
	int result_fregs;
	DYNARRAY (tnode_t **, sublist);
} krocllvm_resultsubhook_t;

typedef struct TAG_krocllvm_coderref {
	int regs[KROCLLVM_MAX_DEPTH];		/* register numbers */
	int types[KROCLLVM_MAX_DEPTH];		/* register types */
	int nregs;
} krocllvm_coderref_t;

typedef struct TAG_krocllvm_cgstate {
	int wsreg;					/* workspace register number */
	DYNARRAY (krocllvm_coderref_t *, cdrefs);	/* list of coder references (built up for some code-gen sequences) */
} krocllvm_cgstate_t;

/*}}}*/
/*{{{  register type definitions (for LLVM)*/

#define LLVM_TYPE_VOID 0x00000000
#define LLVM_TYPE_INT 0x00001000		/* integer type, low order 12 bits specifies number of bits */
#define LLVM_TYPE_FP 0x00002000			/* floating-point type, low order 12 bits specifies size (32,64,128) */
#define LLVM_TYPE_SIGNED 0x00004000		/* flag for signed integers */
#define LLVM_TYPE_PTR 0x00008000		/* flag for pointer type */
#define LLVM_TYPE_PPTR 0x00010000		/* flag for pointer-pointer type */

#define LLVM_TYPE_I8PTR ((LLVM_TYPE_INT | 8) | LLVM_TYPE_PTR)
#define LLVM_TYPE_I32PTR ((LLVM_TYPE_INT | 32) | LLVM_TYPE_PTR)
#define LLVM_TYPE_I32 (LLVM_TYPE_INT | 32)

#define LLVM_SIZE_I32 (4)
#define LLVM_SIZE_SHIFT (2)

/*}}}*/
/*{{{  kernel entry-points*/

#define KIFACE_MAX_PARAMS (5)

typedef struct TAG_krocllvm_kientry {
	int call;				/* I_OUTBYTE, etc. */
	int iregs;				/* number of input registers */
	int oregs;				/* number of output registers */
	int itypes[KIFACE_MAX_PARAMS];		/* input parameter types */
	int otypes[KIFACE_MAX_PARAMS];		/* output parameter types */
	char *ename;				/* entry-point name */
} krocllvm_kientry_t;

static krocllvm_kientry_t kitable[] = {
	{I_OUTBYTE, 2, 0, {LLVM_TYPE_I32, LLVM_TYPE_I32, LLVM_TYPE_VOID,}, {LLVM_TYPE_VOID, }, "kernel_Y_outbyte"},
	{I_OUT, 3, 0, {LLVM_TYPE_I32, LLVM_TYPE_I32, LLVM_TYPE_I32, LLVM_TYPE_VOID,}, {LLVM_TYPE_VOID, }, "kernel_Y_out"},
	{I_INVALID, 0, 0, {LLVM_TYPE_VOID, }, {LLVM_TYPE_VOID, }, NULL}
};

/*}}}*/


/*{{{  void krocllvm_isetindent (fhandle_t *stream, int indent)*/
/*
 *	set indent for debuggint output
 */
void krocllvm_isetindent (fhandle_t *stream, int indent)
{
	int i;

	for (i=0; i<indent; i++) {
		fhandle_printf (stream, "    ");
	}
	return;
}
/*}}}*/


/*{{{  static int krocllvm_opthandler_flag (cmd_option_t *opt, char ***argwalk, int *argleft)*/
/*
 *	option handler for this target's options
 *	returns 0 on success, non-zero on failure
 */
static int krocllvm_opthandler_flag (cmd_option_t *opt, char ***argwalk, int *argleft)
{
	int optv = (int)opt->arg;
	int flagval = 1;
	krocllvm_priv_t *kpriv = (krocllvm_priv_t *)krocllvm_target.priv;

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
/*{{{  static int krocllvm_init_options (krocllvm_priv_t *kpriv)*/
/*
 *	initialises options for the KRoC-LLVM back-end
 *	returns 0 on success, non-zero on failure
 */
static int krocllvm_init_options (krocllvm_priv_t *kpriv)
{
	opts_add ("stoperrormode", '\0', krocllvm_opthandler_flag, (void *)1, "1use stop error-mode");
	opts_add ("halterrormode", '\0', krocllvm_opthandler_flag, (void *)-1, "1use halt error-mode");

	return 0;
}
/*}}}*/

/*{{{  static krocllvm_coderref_t *krocllvm_newcoderref (void)*/
/*
 *	creates a new, blank, krocllvm_coderref_t structure.
 */
static krocllvm_coderref_t *krocllvm_newcoderref (void)
{
	krocllvm_coderref_t *cr;

	cr = (krocllvm_coderref_t *)smalloc (sizeof (krocllvm_coderref_t));
	cr->nregs = 0;

	return cr;
}
/*}}}*/
/*{{{  static void krocllvm_freecoderref (krocllvm_coderref_t *cr)*/
/*
 *	frees a krocllvm_coderref_t structure
 */
static void krocllvm_freecoderref (krocllvm_coderref_t *cr)
{
	if (!cr) {
		nocc_internal ("krocllvm_freecoderref(): NULL pointer!");
		return;
	}
	sfree (cr);
}
/*}}}*/
/*{{{  static krocllvm_coderref_t *krocllvm_newcoderref_init (codegen_t *cgen, const int lltype)*/
/*
 *	creates a new virtual register for LLVM code-gen
 */
static krocllvm_coderref_t *krocllvm_newcoderref_init (codegen_t *cgen, const int lltype)
{
	krocllvm_coderref_t *cr = krocllvm_newcoderref ();
	krocllvm_priv_t *kpriv = (krocllvm_priv_t *)krocllvm_target.priv;

	cr->regs[0] = ++kpriv->regcount;
	cr->types[0] = lltype;
	cr->nregs = 1;

	return cr;
}
/*}}}*/
/*{{{  static void krocllvm_addcoderref (codegen_t *cgen, krocllvm_coderref_t *cr, const int lltype)*/
/*
 *	creates a new virtual register for LLVM code-gen (in existing reg-set)
 */
static void krocllvm_addcoderref (codegen_t *cgen, krocllvm_coderref_t *cr, const int lltype)
{
	krocllvm_priv_t *kpriv = (krocllvm_priv_t *)krocllvm_target.priv;

	cr->regs[cr->nregs] = ++kpriv->regcount;
	cr->types[cr->nregs] = lltype;
	cr->nregs++;

	return;
}
/*}}}*/
/*{{{  static int krocllvm_newreg (codegen_t *cgen)*/
/*
 *	creates a new virtual register for LLVM code-gen (untyped)
 */
static int krocllvm_newreg (codegen_t *cgen)
{
	krocllvm_priv_t *kpriv = (krocllvm_priv_t *)krocllvm_target.priv;

	return ++kpriv->regcount;
}
/*}}}*/
/*{{{  static char *krocllvm_typestr (const int type)*/
/*
 *	generates a type string for an LLVM type (e.g. "i32")
 */
static char *krocllvm_typestr (const int type)
{
	static char buf[16];
	static int boffs = 0;
	int thisoff;

	if (type == LLVM_TYPE_VOID) {
		sprintf (&buf[boffs], "void");
	} else if (type & LLVM_TYPE_INT) {
		sprintf (&buf[boffs], "i%d", type & 0xfff);
	} else if (type & LLVM_TYPE_FP) {
		if ((type & 0xfff) == 32) {
			sprintf (&buf[boffs], "float");
		} else if ((type & 0xfff) == 64) {
			sprintf (&buf[boffs], "double");
		} else if ((type & 0xfff) == 128) {
			sprintf (&buf[boffs], "fp128");
		} else {
			nocc_internal ("krocllvm_typestr(): invalid FP type 0x%8.8x\n", (unsigned int)type);
		}
	} else {
		nocc_internal ("krocllvm_typestr(): invalid type 0x%8.8x\n", (unsigned int)type);
	}

	thisoff = boffs;
	boffs = 8 - boffs;

	return &buf[thisoff];
}
/*}}}*/


/*{{{  krocllvm_namehook_t routines*/
/*{{{  static void krocllvm_namehook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)*/
/*
 *	dumps hook data for debugging
 */
static void krocllvm_namehook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)
{
	krocllvm_namehook_t *nh = (krocllvm_namehook_t *)hook;

	krocllvm_isetindent (stream, indent);
	fhandle_printf (stream, "<namehook addr=\"0x%8.8x\" lexlevel=\"%d\" allocwsh=\"%d\" allocwsl=\"%d\" typesize=\"%d\" indir=\"%d\" wsoffset=\"%d\" typecat=\"0x%8.8x\" />\n",
			(unsigned int)nh, nh->lexlevel, nh->alloc_wsh, nh->alloc_wsl, nh->typesize, nh->indir, nh->ws_offset, (unsigned int)nh->typecat);
	return;
}
/*}}}*/
/*{{{  static krocllvm_namehook_t *krocllvm_namehook_create (int ll, int asize_wsh, int asize_wsl, int tsize, int ind)*/
/*
 *	creates a name-hook
 */
static krocllvm_namehook_t *krocllvm_namehook_create (int ll, int asize_wsh, int asize_wsl, int tsize, int ind)
{
	krocllvm_namehook_t *nh = (krocllvm_namehook_t *)smalloc (sizeof (krocllvm_namehook_t));

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
/*{{{  krocllvm_blockhook_t routines*/
/*{{{  static void krocllvm_blockhook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)*/
/*
 *	dumps hook for debugging
 */
static void krocllvm_blockhook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)
{
	krocllvm_blockhook_t *bh = (krocllvm_blockhook_t *)hook;

	krocllvm_isetindent (stream, indent);
	fhandle_printf (stream, "<blockhook addr=\"0x%8.8x\" lexlevel=\"%d\" allocws=\"%d\" adjust=\"%d\" wsoffset=\"%d\" entrylab=\"%d\" addstaticlink=\"%d\" addfbp=\"%d\" />\n",
			(unsigned int)bh, bh->lexlevel, bh->alloc_ws, bh->static_adjust, bh->ws_offset, bh->entrylab, bh->addstaticlink, bh->addfbp);
	return;
}
/*}}}*/
/*{{{  static krocllvm_blockhook_t *krocllvm_blockhook_create (int ll)*/
/*
 *	creates a block-hook
 */
static krocllvm_blockhook_t *krocllvm_blockhook_create (int ll)
{
	krocllvm_blockhook_t *bh = (krocllvm_blockhook_t *)smalloc (sizeof (krocllvm_blockhook_t));

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
/*{{{  krocllvm_blockrefhook_t routines*/
/*{{{  static void krocllvm_blockrefhook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)*/
/*
 *	dumps hook (debugging)
 */
static void krocllvm_blockrefhook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)
{
	krocllvm_blockrefhook_t *brh = (krocllvm_blockrefhook_t *)hook;
	tnode_t *blk = brh->block;

	if (blk && parser_islistnode (blk)) {
		int nitems, i;
		tnode_t **blks = parser_getlistitems (blk, &nitems);

		krocllvm_isetindent (stream, indent);
		fhandle_printf (stream, "<blockrefhook addr=\"0x%8.8x\" block=\"0x%8.8x\" nblocks=\"%d\" blocks=\"", (unsigned int)brh, (unsigned int)blk, nitems);
		for (i=0; i<nitems; i++ ) {
			if (i) {
				fhandle_printf (stream, ",");
			}
			fhandle_printf (stream, "0x%8.8x", (unsigned int)blks[i]);
		}
		fhandle_printf (stream, "\" />\n");
	} else {
		krocllvm_isetindent (stream, indent);
		fhandle_printf (stream, "<blockrefhook addr=\"0x%8.8x\" block=\"0x%8.8x\" />\n", (unsigned int)brh, (unsigned int)blk);
	}

	return;
}
/*}}}*/
/*{{{  static krocllvm_blockrefhook_t *krocllvm_blockrefhook_create (tnode_t *block)*/
/*
 *	creates a new hook (populated)
 */
static krocllvm_blockrefhook_t *krocllvm_blockrefhook_create (tnode_t *block)
{
	krocllvm_blockrefhook_t *brh = (krocllvm_blockrefhook_t *)smalloc (sizeof (krocllvm_blockrefhook_t));

	brh->block = block;

	return brh;
}
/*}}}*/
/*}}}*/
/*{{{  krocllvm_consthook_t routines*/
/*{{{  static void krocllvm_consthook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)*/
/*
 *	dumps hook for debugging
 */
static void krocllvm_consthook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)
{
	krocllvm_consthook_t *ch = (krocllvm_consthook_t *)hook;

	krocllvm_isetindent (stream, indent);
	fhandle_printf (stream, "<consthook addr=\"0x%8.8x\" data=\"0x%8.8x\" size=\"%d\" label=\"%d\" labrefs=\"%d\" orgnode=\"0x%8.8x\" orgnodetag=\"%s\" typecat=\"0x%8.8x\" />\n",
			(unsigned int)ch, (unsigned int)ch->byteptr, ch->size, ch->label, ch->labrefs,
			(unsigned int)ch->orgnode, ch->orgnode ? ch->orgnode->tag->name : "", (unsigned int)ch->typecat);
	return;
}
/*}}}*/
/*{{{  static krocllvm_consthook_t *krocllvm_consthook_create (void *ptr, int size)*/
/*
 *	creates a constant-hook
 */
static krocllvm_consthook_t *krocllvm_consthook_create (void *ptr, int size)
{
	krocllvm_consthook_t *ch = (krocllvm_consthook_t *)smalloc (sizeof (krocllvm_consthook_t));

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
/*{{{  krocllvm_indexedhook_t routines*/
/*{{{  static void krocllvm_indexedhook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)*/
/*
 *	dumps hook for debugging
 */
static void krocllvm_indexedhook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)
{
	krocllvm_indexedhook_t *ih = (krocllvm_indexedhook_t *)hook;

	krocllvm_isetindent (stream, indent);
	fhandle_printf (stream, "<indexhook isize=\"%d\" offset=\"%d\" />\n", ih->isize, ih->offset);
	return;
}
/*}}}*/
/*{{{  static krocllvm_indexedhook_t *krocllvm_indexedhook_create (int isize, int offset)*/
/*
 *	creates a indexed hook
 */
static krocllvm_indexedhook_t *krocllvm_indexedhook_create (int isize, int offset)
{
	krocllvm_indexedhook_t *ih = (krocllvm_indexedhook_t *)smalloc (sizeof (krocllvm_indexedhook_t));

	ih->isize = isize;
	ih->offset = offset;
	return ih;
}
/*}}}*/
/*}}}*/
/*{{{  special-node hook routines*/
/*{{{  static void krocllvm_specialhook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)*/
/*
 *	dumps hook data for debugging
 */
static void krocllvm_specialhook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)
{
	krocllvm_isetindent (stream, indent);
	fhandle_printf (stream, "<specialhook addr=\"0x%8.8x\" />\n", (unsigned int)hook);
}
/*}}}*/
/*}}}*/
/*{{{  krocllvm_memspacehook_t routines*/
/*{{{  static void krocllvm_memspacehook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)*/
/*
 *	dumps a krocllvm_memspacehook_t structure (debugging)
 */
static void krocllvm_memspacehook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)
{
	return;
}
/*}}}*/
/*{{{  static krocllvm_memspacehook_t *krocllvm_memspacehook_create (void)*/
/*
 *	creates a new krocllvm_memspacehook_t
 */
static krocllvm_memspacehook_t *krocllvm_memspacehook_create (void)
{
	krocllvm_memspacehook_t *msh = (krocllvm_memspacehook_t *)smalloc (sizeof (krocllvm_memspacehook_t));

	msh->ws = 0;
	msh->adjust = 0;
	msh->vs = 0;
	msh->ms = 0;

	return msh;
}
/*}}}*/
/*{{{  static void krocllvm_memspacehook_free (void *hook)*/
/*
 *	frees a krocllvm_memspacehook_t
 */
static void krocllvm_memspacehook_free (void *hook)
{
	krocllvm_memspacehook_t *msh = (krocllvm_memspacehook_t *)hook;

	if (!msh) {
		return;
	}
	sfree (msh);
	return;
}
/*}}}*/
/*}}}*/
/*{{{  krocllvm_resultsubhook_t routines*/
/*{{{  static void krocllvm_resultsubhook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)*/
/*
 *	dumps hook for debugging
 */
static void krocllvm_resultsubhook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)
{
	krocllvm_resultsubhook_t *rh = (krocllvm_resultsubhook_t *)hook;
	int i;

	krocllvm_isetindent (stream, indent);
	fhandle_printf (stream, "<chook:resultsubhook regs=\"%d\" fregs=\"%d\">\n", rh->result_regs, rh->result_fregs);
	for (i=0; i<DA_CUR (rh->sublist); i++) {
		tnode_t *ref = *(DA_NTHITEM (rh->sublist, i));

		krocllvm_isetindent (stream, indent+1);
		fhandle_printf (stream, "<noderef nodetype=\"%s\" type=\"%s\" addr=\"0x%8.8x\" />\n", ref->tag->ndef->name, ref->tag->name, (unsigned int)ref);
	}
	krocllvm_isetindent (stream, indent);
	fhandle_printf (stream, "</chook:resultsubhook>\n");
	return;
}
/*}}}*/
/*{{{  static krocllvm_resultsubhook_t *krocllvm_resultsubhook_create (void)*/
/*
 *	creates a blank result hook
 */
static krocllvm_resultsubhook_t *krocllvm_resultsubhook_create (void)
{
	krocllvm_resultsubhook_t *rh = (krocllvm_resultsubhook_t *)smalloc (sizeof (krocllvm_resultsubhook_t));

	rh->result_regs = -1;
	rh->result_fregs = -1;
	dynarray_init (rh->sublist);

	return rh;
}
/*}}}*/
/*{{{  static void krocllvm_resultsubhook_free (void *hook)*/
/*
 *	frees a result hook
 */
static void krocllvm_resultsubhook_free (void *hook)
{
	krocllvm_resultsubhook_t *rh = (krocllvm_resultsubhook_t *)hook;

	if (!rh) {
		return;
	}
	dynarray_trash (rh->sublist);
	sfree (rh);

	return;
}
/*}}}*/
/*}}}*/
/*{{{  krocllvm_cgstate_t routines*/
/*{{{  static krocllvm_cgstate_t *krocllvm_cgstate_create (void)*/
/*
 *	creates a new krocllvm_cgstate_t
 */
static krocllvm_cgstate_t *krocllvm_cgstate_create (void)
{
	krocllvm_cgstate_t *cgs = (krocllvm_cgstate_t *)smalloc (sizeof (krocllvm_cgstate_t));

	cgs->wsreg = -1;
	dynarray_init (cgs->cdrefs);

	return cgs;
}
/*}}}*/
/*{{{  static void krocllvm_cgstate_destroy (krocllvm_cgstate_t *cgs)*/
/*
 *	destroys a krocllvm_cgstate_t
 */
static void krocllvm_cgstate_destroy (krocllvm_cgstate_t *cgs)
{
	int i;

	if (!cgs) {
		nocc_internal ("krocllvm_cgstate_destroy(): null state!");
		return;
	}
	for (i=0; i<DA_CUR (cgs->cdrefs); i++) {
		krocllvm_freecoderref (DA_NTHITEM (cgs->cdrefs, i));
	}
	dynarray_trash (cgs->cdrefs);

	sfree (cgs);

	return;
}
/*}}}*/

/*{{{  static krocllvm_cgstate_t *krocllvm_cgstate_newpush (codegen_t *cgen)*/
/*
 *	creates a new code-gen state and pushes it onto the code-gen stack, also returns it
 */
static krocllvm_cgstate_t *krocllvm_cgstate_newpush (codegen_t *cgen)
{
	krocllvm_cgstate_t *cgs = krocllvm_cgstate_create ();

	dynarray_add (cgen->tcgstates, (void *)cgs);

	return cgs;
}
/*}}}*/
/*{{{  static krocllvm_cgstate_t *krocllvm_cgstate_copypush (codegen_t *cgen)*/
/*
 *	copies and pushes the current code-gen state, also returns it
 */
static krocllvm_cgstate_t *krocllvm_cgstate_copypush (codegen_t *cgen)
{
	krocllvm_cgstate_t *cgs;

	cgs = krocllvm_cgstate_create ();
	if (!DA_CUR (cgen->tcgstates)) {
		codegen_warning (cgen, "krocllvm_cgstate_copypush(): no previous state -- creating new");
	} else {
		krocllvm_cgstate_t *lastcgs = (krocllvm_cgstate_t *)DA_NTHITEM (cgen->tcgstates, DA_CUR (cgen->tcgstates) - 1);

		cgs->wsreg = lastcgs->wsreg;
	}
	dynarray_add (cgen->tcgstates, (void *)cgs);

	return cgs;
}
/*}}}*/
/*{{{  static void krocllvm_cgstate_popfree (codegen_t *cgen)*/
/*
 *	pops and frees code-gen state
 */
static void krocllvm_cgstate_popfree (codegen_t *cgen)
{
	krocllvm_cgstate_t *cgs;

	if (!DA_CUR (cgen->tcgstates)) {
		nocc_internal ("krocllvm_cgstate_popfree(): no tcgstates!");
		return;
	}
	cgs = (krocllvm_cgstate_t *)DA_NTHITEM (cgen->tcgstates, DA_CUR (cgen->tcgstates) - 1);

	DA_SETNTHITEM (cgen->tcgstates, DA_CUR (cgen->tcgstates) - 1, NULL);
	dynarray_delitem (cgen->tcgstates, DA_CUR (cgen->tcgstates) - 1);

	krocllvm_cgstate_destroy (cgs);
	return;
}
/*}}}*/
/*{{{  static krocllvm_cgstate_t *krocllvm_cgstate_cur (codegen_t *cgen)*/
/*
 *	returns the current code-gen state
 */
static krocllvm_cgstate_t *krocllvm_cgstate_cur (codegen_t *cgen)
{
	if (!DA_CUR (cgen->tcgstates)) {
		codegen_error (cgen, "krocllvm_cgstate_cur(): no current code-gen state..");
		return NULL;
	}
	return (krocllvm_cgstate_t *)DA_NTHITEM (cgen->tcgstates, DA_CUR (cgen->tcgstates) - 1);
}
/*}}}*/
/*{{{  static int krocllvm_cgstate_tsdelta (codegen_t *cgen, int delta)*/
/*
 *	adjusts the integer stack -- emits a warning if it overflows or underflows
 *	returns the new stack depth
 */
static int krocllvm_cgstate_tsdelta (codegen_t *cgen, int delta)
{
	krocllvm_priv_t *kpriv = (krocllvm_priv_t *)cgen->target->priv;
	krocllvm_cgstate_t *cgs = krocllvm_cgstate_cur (cgen);

	if (!cgs) {
		codegen_error (cgen, "krocllvm_cgstate_tsdelta(): no stack to adjust!");
		return 0;
	}
#if 1
fprintf (stderr, "krocllvm_cgstate_tsdelta(): should probably not call this!\n");
#endif
	return 0;
}
/*}}}*/
/*{{{  static int krocllvm_cgstate_tsfpdelta (codegen_t *cgen, int delta)*/
/*
 *	adjusts the floating-point stack -- emits a warning if it overflows or underflows
 *	returns the new stack depth
 */
static int krocllvm_cgstate_tsfpdelta (codegen_t *cgen, int delta)
{
	krocllvm_priv_t *kpriv = (krocllvm_priv_t *)cgen->target->priv;
	krocllvm_cgstate_t *cgs = krocllvm_cgstate_cur (cgen);

	if (!cgs) {
		codegen_error (cgen, "krocllvm_cgstate_tsfpdelta(): no stack to adjust!");
		return 0;
	}
#if 1
fprintf (stderr, "krocllvm_cgstate_tsfpdelta(): should probably not call this!\n");
#endif

	return 0;
}
/*}}}*/
/*{{{  static int krocllvm_cgstate_tszero (codegen_t *cgen)*/
/*
 *	zeros the integer stack (needed for trashing the stack after a function return)
 *	return the old stack depth
 */
static int krocllvm_cgstate_tszero (codegen_t *cgen)
{
	krocllvm_priv_t *kpriv = (krocllvm_priv_t *)cgen->target->priv;
	krocllvm_cgstate_t *cgs = krocllvm_cgstate_cur (cgen);
	int r;

	if (!cgs) {
		codegen_error (cgen, "krocllvm_cgstate_tszero(): no stack to adjust!");
		return 0;
	}
#if 1
fprintf (stderr, "krocllvm_cgstate_tszero(): should probably not call this!\n");
#endif

	return 0;
}
/*}}}*/
/*}}}*/


/*{{{  static void krocllvm_setlastlabel (codegen_t *cgen, const char *name)*/
/*
 *	records the last named label dropped in the output stream
 */
static void krocllvm_setlastlabel (codegen_t *cgen, const char *name)
{
	krocllvm_priv_t *kpriv = (krocllvm_priv_t *)(cgen->target->priv);

	if (!kpriv) {
		codegen_error (cgen, "krocllvm_setlastlabel(): NULL private state!");
		return;
	}
	if (kpriv->lastfunc) {
		sfree (kpriv->lastfunc);
		kpriv->lastfunc = NULL;
	}
	
	kpriv->lastfunc = string_dup (name);

	return;
}
/*}}}*/


/*{{{  static tnode_t *krocllvm_name_create (tnode_t *fename, tnode_t *body, map_t *mdata, int asize_wsh, int asize_wsl, int asize_vs, int asize_ms, int tsize, int ind)*/
/*
 *	allocates a new back-end name-node
 */
static tnode_t *krocllvm_name_create (tnode_t *fename, tnode_t *body, map_t *mdata, int asize_wsh, int asize_wsl, int asize_vs, int asize_ms, int tsize, int ind)
{
	target_t *xt = mdata->target;		/* must be us! */
	tnode_t *name;
	krocllvm_namehook_t *nh;
	tnode_t *blk = map_thisblock_cll (mdata);

	nh = krocllvm_namehook_create (mdata->lexlevel, asize_wsh, asize_wsl, tsize, ind);
	name = tnode_create (xt->tag_NAME, NULL, fename, body, (void *)nh);

	if (blk) {
		krocllvm_blockhook_t *bh = (krocllvm_blockhook_t *)tnode_nthhookof (blk, 0);

		/* FIXME: specials go here if needed */
	}

	return name;
}
/*}}}*/
/*{{{  static tnode_t *krocllvm_nameref_create (tnode_t *bename, map_t *mdata)*/
/*
 *	allocates a new back-end name-ref-node
 */
static tnode_t *krocllvm_nameref_create (tnode_t *bename, map_t *mdata)
{
	krocllvm_namehook_t *nh, *be_nh;
	krocllvm_blockhook_t *bh;
	tnode_t *name, *fename;
	tnode_t *blk = map_thisblock_cll (mdata);

	if (!blk) {
		nocc_internal ("krocllvm_nameref_create(): reference to name outside of block");
		return NULL;
	}
	bh = (krocllvm_blockhook_t *)tnode_nthhookof (blk, 0);
	be_nh = (krocllvm_namehook_t *)tnode_nthhookof (bename, 0);
#if 0
fprintf (stderr, "krocllvm_nameref_create (): referenced lexlevel=%d, map lexlevel=%d, enclosing block lexlevel=%d\n", be_nh->lexlevel, mdata->lexlevel, bh->lexlevel);
#endif
	if (be_nh->lexlevel < bh->lexlevel) {
		/*{{{  need a static-link to get at this one*/
		int i;

		for (i=bh->lexlevel; i>be_nh->lexlevel; i--) {
			tnode_t *llblk = map_thisblock_ll (mdata, i);
			krocllvm_blockhook_t *llbh = (krocllvm_blockhook_t *)tnode_nthhookof (llblk, 0);

			if (llbh) {
				llbh->addstaticlink = 1;
			} else {
				nocc_warning ("krocllvm_nameref_create(): no block at lexlevel %d", i);
			}
		}
		/*}}}*/
	}
	/* nh = krocllvm_namehook_create (be_nh->lexlevel, 0, 0, 0, 0, be_nh->typesize, be_nh->indir); */
	nh = krocllvm_namehook_create (mdata->lexlevel, 0, 0, be_nh->typesize, be_nh->indir);
	nh->typecat = be_nh->typecat;				/* copy over type-category */

	fename = tnode_nthsubof (bename, 0);
	name = tnode_create (mdata->target->tag_NAMEREF, NULL, fename, (void *)nh);

	return name;
}
/*}}}*/
/*{{{  static tnode_t *krocllvm_block_create (tnode_t *body, map_t *mdata, tnode_t *slist, int lexlevel)*/
/*
 *	creates a new back-end block
 */
static tnode_t *krocllvm_block_create (tnode_t *body, map_t *mdata, tnode_t *slist, int lexlevel)
{
	krocllvm_blockhook_t *bh;
	tnode_t *blk;

	bh = krocllvm_blockhook_create (lexlevel);
	blk = tnode_create (mdata->target->tag_BLOCK, NULL, body, slist, (void *)bh);

	return blk;
}
/*}}}*/
/*{{{  static tnode_t *krocllvm_const_create (tnode_t *val, map_t *mdata, void *data, int size, typecat_e typecat)*/
/*
 *	creates a new back-end constant
 */
static tnode_t *krocllvm_const_create (tnode_t *val, map_t *mdata, void *data, int size, typecat_e typecat)
{
	krocllvm_consthook_t *ch;
	tnode_t *cnst;

	ch = krocllvm_consthook_create (data, size);
	ch->orgnode = val;
	ch->typecat = typecat;
	cnst = tnode_create (mdata->target->tag_CONST, NULL, val, (void *)ch);

	return cnst;
}
/*}}}*/
/*{{{  static tnode_t *krocllvm_indexed_create (tnode_t *base, tnode_t *index, int isize, int offset)*/
/*
 *	creates a new back-end indexed node (used for arrays and the like)
 */
static tnode_t *krocllvm_indexed_create (tnode_t *base, tnode_t *index, int isize, int offset)
{
	krocllvm_indexedhook_t *ih;
	tnode_t *indxd;

	ih = krocllvm_indexedhook_create (isize, offset);
	indxd = tnode_create (krocllvm_target.tag_INDEXED, NULL, base, index, (void *)ih);

	return indxd;
}
/*}}}*/
/*{{{  static tnode_t *krocllvm_blockref_create (tnode_t *block, tnode_t *body, map_t *mdata)*/
/*
 *	creates a new back-end block reference node (used for procedure instances and the like)
 */
static tnode_t *krocllvm_blockref_create (tnode_t *block, tnode_t *body, map_t *mdata)
{
	krocllvm_blockrefhook_t *brh = krocllvm_blockrefhook_create (block);
	tnode_t *blockref;

	blockref = tnode_create (krocllvm_target.tag_BLOCKREF, NULL, body, (void *)brh);

	return blockref;
}
/*}}}*/
/*{{{  static tnode_t *krocllvm_result_create (tnode_t *expr, map_t *mdata)*/
/*
 *	creates a new back-end result node (used for expressions and the like)
 */
static tnode_t *krocllvm_result_create (tnode_t *expr, map_t *mdata)
{
	krocllvm_priv_t *kpriv = (krocllvm_priv_t *)mdata->target->priv;
	krocllvm_resultsubhook_t *rh;
	tnode_t *res;

	res = tnode_create (krocllvm_target.tag_RESULT, NULL, expr);

	rh = krocllvm_resultsubhook_create ();
	tnode_setchook (res, kpriv->resultsubhook, (void *)rh);

	return res;
}
/*}}}*/
/*{{{  static void krocllvm_inresult (tnode_t **node, map_t *mdata)*/
/*
 *	adds a (back-end) node to the sub-list of a back-end result node, used in expressions and the like
 */
static void krocllvm_inresult (tnode_t **nodep, map_t *mdata)
{
	krocllvm_priv_t *kpriv = (krocllvm_priv_t *)mdata->target->priv;
	krocllvm_resultsubhook_t *rh;

	if (!mdata->thisberesult) {
		nocc_internal ("krocllvm_inresult(): not inside any result!");
		return;
	}

	rh = (krocllvm_resultsubhook_t *)tnode_getchook (mdata->thisberesult, kpriv->resultsubhook);
	if (!rh) {
		nocc_internal ("krocllvm_inresult(): missing resultsubhook!");
		return;
	}

	dynarray_add (rh->sublist, nodep);

	return;
}
/*}}}*/


/*{{{  int krocllvm_init (void)*/
/*
 *	initialises the KRoC-LLVM back-end
 *	return 0 on success, non-zero on error
 */
int krocllvm_init (void)
{
	if (target_register (&krocllvm_target)) {
		nocc_error ("krocllvm_init(): failed to register target!");
		return 1;
	}
	return 0;
}
/*}}}*/
/*{{{  int krocllvm_shutdown (void)*/
/*
 *	shuts-down the KRoC-LLVM back-end
 *	returns 0 on success, non-zero on error
 */
int krocllvm_shutdown (void)
{
	if (target_unregister (&krocllvm_target)) {
		nocc_error ("krocllvm_shutdown(): failed to unregister target!");
		return 1;
	}
	return 0;
}
/*}}}*/



/*{{{  static int krocllvm_be_allocsize (tnode_t *node, int *pwsh, int *pwsl, int *pvs, int *pms)*/
/*
 *	retrieves the number of bytes to be allocated to a back-end name
 *	returns 0 on success, non-zero on failure
 */
static int krocllvm_be_allocsize (tnode_t *node, int *pwsh, int *pwsl, int *pvs, int *pms)
{
	if (node->tag == krocllvm_target.tag_BLOCKREF) {
		/*{{{  space required for block reference*/
		krocllvm_blockrefhook_t *brh = (krocllvm_blockrefhook_t *)tnode_nthhookof (node, 0);
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
fprintf (stderr, "krocllvm_be_allocsize(): block-list, %d items\n", nitems);
#endif
			ws = 0;
			vs = 0;
			ms = 0;
			for (i=0; i<nitems; i++) {
				int lws, lvs, lms;
				int elab;
				
				krocllvm_be_getblocksize (blks[i], &lws, &wsoffs, &lvs, &lms, &adj, &elab);
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

			krocllvm_be_getblocksize (blk, &ws, &wsoffs, &vs, &ms, &adj, &elab);
		}

#if 0
fprintf (stderr, "krocllvm_be_allocsize(): got block size from BLOCKREF, ws=%d, wsoffs=%d, vs=%d, ms=%d, adj=%d\n", ws, wsoffs, vs, ms, adj);
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
	} else if (node->tag == krocllvm_target.tag_NAME) {
		/*{{{  space required for name*/
		krocllvm_namehook_t *nh;

		nh = (krocllvm_namehook_t *)tnode_nthhookof (node, 0);
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
			*pvs = 0;
		}
		if (pms) {
			*pms = 0;
		}
		/*}}}*/
	} else if (node->tag == krocllvm_target.tag_NAMEREF) {
		/*{{{  space required for name-reference (usually nothing)*/
		krocllvm_namehook_t *nh = (krocllvm_namehook_t *)tnode_nthhookof (node, 0);

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
			*pvs = 0;
		}
		if (pms) {
			*pms = 0;
		}
		/*}}}*/
	} else {
#if 0
fprintf (stderr, "krocllvm_be_allocsize(): unknown type node=[%s]\n", node->tag->name);
#endif
		nocc_warning ("krocllvm_be_allocsize(): unknown node type [%s]", node->tag->name);
		return -1;
	}

	return 0;
}
/*}}}*/
/*{{{  static int krocllvm_be_typesize (tnode_t *node, int *typesize, int *indir)*/
/*
 *	gets the typesize for a back-end name
 *	returns 0 on success, non-zero on failure
 */
static int krocllvm_be_typesize (tnode_t *node, int *typesize, int *indir)
{
	krocllvm_priv_t *kpriv = (krocllvm_priv_t *)krocllvm_target.priv;

	if (!kpriv) {
		nocc_warning ("krocllvm_be_typesize(): called outside of back-end context (no private data)");
		return -1;
	}
	if (node->tag == krocllvm_target.tag_NAME) {
		/*{{{  typesize of a NAME*/
		krocllvm_namehook_t *nh = (krocllvm_namehook_t *)tnode_nthhookof (node, 0);

		if (!nh) {
			return -1;
		}
		if (typesize) {
			*typesize = nh->typesize;
		}
		if (indir) {
			*indir = nh->indir;
		}
		/*}}}*/
	} else if (node->tag == krocllvm_target.tag_NAMEREF) {
		/*{{{  typesize of a NAMEREF*/
		krocllvm_namehook_t *nh = (krocllvm_namehook_t *)tnode_nthhookof (node, 0);

		if (!nh) {
			return -1;
		}
		if (typesize) {
			*typesize = nh->typesize;
		}
		if (indir) {
			*indir = nh->indir;
		}
		/*}}}*/
	} else if (node->tag == kpriv->tag_CONSTREF) {
		/*{{{  typesize of a constant reference*/
		krocllvm_consthook_t *ch = (krocllvm_consthook_t *)tnode_nthhookof (node, 0);

		if (typesize) {
			*typesize = ch->size;
		}
		if (indir) {
			*indir = 0;		/* yep, really here! */
		}
		/*}}}*/
	} else {
		nocc_warning ("krocllvm_be_typesize(): unknown node type [%s]", node->tag->name);
		return -1;
	}
	return 0;
}
/*}}}*/
/*{{{  static void krocllvm_be_settypecat (tnode_t *bename, typecat_e typecat)*/
/*
 *	sets the type-category for a back-end name
 */
static void krocllvm_be_settypecat (tnode_t *bename, typecat_e typecat)
{
	krocllvm_namehook_t *nh;

	if ((bename->tag != krocllvm_target.tag_NAME) && (bename->tag != krocllvm_target.tag_NAMEREF)) {
		nocc_internal ("krocllvm_be_settypecat(): not a NAME/NAMEREF!");
		return;
	}
	nh = (krocllvm_namehook_t *)tnode_nthhookof (bename, 0);
	if (!nh) {
		nocc_internal ("krocllvm_be_settypecat(): NAME/NAMEREF has no hook");
		return;
	}

	nh->typecat = typecat;

	return;
}
/*}}}*/
/*{{{  static void krocllvm_be_gettypecat (tnode_t *bename, typecat_e *tcptr)*/
/*
 *	gets the type-category for a back-end name
 */
static void krocllvm_be_gettypecat (tnode_t *bename, typecat_e *tcptr)
{
	krocllvm_namehook_t *nh;

	if ((bename->tag != krocllvm_target.tag_NAME) && (bename->tag != krocllvm_target.tag_NAMEREF)) {
		nocc_internal ("krocllvm_be_settypecat(): not a NAME/NAMEREF!");
		return;
	}
	nh = (krocllvm_namehook_t *)tnode_nthhookof (bename, 0);
	if (!nh) {
		nocc_internal ("krocllvm_be_settypecat(): NAME/NAMEREF has no hook");
		return;
	}

	if (tcptr) {
		*tcptr = nh->typecat;
	}

	return;
}
/*}}}*/
/*{{{  static void krocllvm_be_setoffsets (tnode_t *bename, int ws_offset, int vs_offset, int ms_offset, int ms_shadow)*/
/*
 *	sets the offsets for a back-end name after allocation
 */
static void krocllvm_be_setoffsets (tnode_t *bename, int ws_offset, int vs_offset, int ms_offset, int ms_shadow)
{
	krocllvm_namehook_t *nh;

	if ((bename->tag != krocllvm_target.tag_NAME) && (bename->tag != krocllvm_target.tag_NAMEREF)) {
		nocc_internal ("krocllvm_be_setoffsets(): not a NAME/NAMEREF!");
		return;
	}
	nh = (krocllvm_namehook_t *)tnode_nthhookof (bename, 0);
	if (!nh) {
		nocc_internal ("krocllvm_be_setoffsets(): NAME/NAMEREF has no hook");
		return;
	}
	nh->ws_offset = ws_offset;

	return;
}
/*}}}*/
/*{{{  static void krocllvm_be_getoffsets (tnode_t *bename, int *wsop, int *vsop, int *msop, int *mssp)*/
/*
 *	gets the offsets for a back-end name after allocation
 */
static void krocllvm_be_getoffsets (tnode_t *bename, int *wsop, int *vsop, int *msop, int *mssp)
{
	krocllvm_namehook_t *nh;

	if ((bename->tag != krocllvm_target.tag_NAME) && (bename->tag != krocllvm_target.tag_NAMEREF)) {
		nocc_internal ("krocllvm_be_getoffsets(): not a NAME/NAMEREF!");
		return;
	}
	nh = (krocllvm_namehook_t *)tnode_nthhookof (bename, 0);
	if (!nh) {
		nocc_internal ("krocllvm_be_getoffsets(): NAME/NAMEREF has no hook");
		return;
	}
	if (wsop) {
		*wsop = nh->ws_offset;
	}
	if (vsop) {
		*vsop = 0;
	}
	if (msop) {
		*msop = 0;
	}
	if (mssp) {
		*mssp = 0;
	}

	return;
}
/*}}}*/
/*{{{  static int krocllvm_be_blocklexlevel (tnode_t *blk)*/
/*
 *	returns the lex-level of a block (or name/nameref)
 */
static int krocllvm_be_blocklexlevel (tnode_t *blk)
{
	if (blk->tag == krocllvm_target.tag_BLOCK) {
		krocllvm_blockhook_t *bh = (krocllvm_blockhook_t *)tnode_nthhookof (blk, 0);

		if (!bh) {
			nocc_internal ("krocllvm_be_blocklexlevel(): BLOCK has no hook");
			return -1;
		}
		return bh->lexlevel;
	} else if (blk->tag == krocllvm_target.tag_NAME) {
		krocllvm_namehook_t *nh = (krocllvm_namehook_t *)tnode_nthhookof (blk, 0);

		if (!nh) {
			nocc_internal ("krocllvm_be_blocklexlevel(): NAME has no hook");
			return -1;
		}
		return nh->lexlevel;
	} else if (blk->tag == krocllvm_target.tag_NAMEREF) {
		krocllvm_namehook_t *nh = (krocllvm_namehook_t *)tnode_nthhookof (blk, 0);

		if (!nh) {
			nocc_internal ("krocllvm_be_blocklexlevel(): NAMEREF has no hook");
			return -1;
		}
		return nh->lexlevel;
	}
	nocc_internal ("krocllvm_be_blocklexlevel(): don\'t know how to deal with a %s", blk->tag->name);
	return -1;
}
/*}}}*/
/*{{{  static void krocllvm_be_setblocksize (tnode_t *blk, int ws, int ws_offs, int vs, int ms, int adjust)*/
/*
 *	sets back-end block size
 */
static void krocllvm_be_setblocksize (tnode_t *blk, int ws, int ws_offs, int vs, int ms, int adjust)
{
	krocllvm_blockhook_t *bh;

	if (blk->tag != krocllvm_target.tag_BLOCK) {
		nocc_internal ("krocllvm_be_setblocksize(): not a BLOCK!");
		return;
	}
	bh = (krocllvm_blockhook_t *)tnode_nthhookof (blk, 0);
	if (!bh) {
		nocc_internal ("krocllvm_be_setblocksize(): BLOCK has no hook");
		return;
	}
	bh->alloc_ws = ws;
	bh->static_adjust = adjust;
	bh->ws_offset = ws_offs;

	return;
}
/*}}}*/
/*{{{  static void krocllvm_be_getblocksize (tnode_t *blk, int *wsp, int *wsoffsp, int *vsp, int *msp, int *adjp, int *elabp)*/
/*
 *	gets back-end block size
 */
static void krocllvm_be_getblocksize (tnode_t *blk, int *wsp, int *wsoffsp, int *vsp, int *msp, int *adjp, int *elabp)
{
	krocllvm_blockhook_t *bh;

#if 0
fprintf (stderr, "krocllvm_be_getblocksize(): blk =\n");
tnode_dumptree (blk, 1, stderr);
#endif
	if (blk->tag == krocllvm_target.tag_BLOCKREF) {
		krocllvm_blockrefhook_t *brh = (krocllvm_blockrefhook_t *)tnode_nthhookof (blk, 0);

		blk = brh->block;
#if 0
fprintf (stderr, "krocllvm_be_getblocksize(): called on BLOCKREF!\n");
#endif
	}

	if (blk->tag != krocllvm_target.tag_BLOCK) {
		nocc_internal ("krocllvm_be_getblocksize(): not a BLOCK!");
		return;
	}
	bh = (krocllvm_blockhook_t *)tnode_nthhookof (blk, 0);
	if (!bh) {
		nocc_internal ("krocllvm_be_getblocksize(): BLOCK has no hook");
		return;
	}

	if (wsp) {
		*wsp = bh->alloc_ws;
	}
	if (vsp) {
		*vsp = 0;
	}
	if (msp) {
		*msp = 0;
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
/*{{{  static tnode_t *krocllvm_be_getorgnode (tnode_t *node)*/
/*
 *	returns the originating node of a back-end constant (useful in specific code-gen)
 *	returns NULL on failure or none
 */
static tnode_t *krocllvm_be_getorgnode (tnode_t *node)
{
	krocllvm_priv_t *kpriv = (krocllvm_priv_t *)krocllvm_target.priv;

	if (node->tag == krocllvm_target.tag_CONST) {
		krocllvm_consthook_t *ch = (krocllvm_consthook_t *)tnode_nthhookof (node, 0);

		if (ch) {
			return ch->orgnode;
		}
	} else if (node->tag == kpriv->tag_CONSTREF) {
		krocllvm_consthook_t *ch = (krocllvm_consthook_t *)tnode_nthhookof (node, 0);

		if (ch) {
			return ch->orgnode;
		}
	}
	return NULL;
}
/*}}}*/
/*{{{  static tnode_t **krocllvm_be_blockbodyaddr (tnode_t *blk)*/
/*
 *	returns the address of the body of a back-end block (also works for back-end NAMEs)
 */
static tnode_t **krocllvm_be_blockbodyaddr (tnode_t *blk)
{
	if (blk->tag == krocllvm_target.tag_BLOCK) {
		return tnode_nthsubaddr (blk, 0);
	} else if (blk->tag == krocllvm_target.tag_NAME) {
		return tnode_nthsubaddr (blk, 1);
	} else {
		nocc_internal ("krocllvm_be_blockbodyaddr(): block not back-end BLOCK or NAME, was [%s]", blk->tag->name);
	}
	return NULL;
}
/*}}}*/
/*{{{  static int krocllvm_be_regsfor (tnode_t *benode)*/
/*
 *	returns the number of registers required to evaluate something
 */
static int krocllvm_be_regsfor (tnode_t *benode)
{
	if (benode->tag == krocllvm_target.tag_NAME) {
		/* something local, if anything */
		return 1;
	} else if (benode->tag == krocllvm_target.tag_CONST) {
		/* constants are easy */
		return 1;
	} else if (benode->tag == krocllvm_target.tag_RESULT) {
		/* straightforward code-gen for LLVM */
		return 1;
	} else if (benode->tag == krocllvm_target.tag_NAMEREF) {
		/* name references are easy too */
		return 1;
	} else if (benode->tag == krocllvm_target.tag_INDEXED) {
		/* indexed nodes require both sides to be loaded -- probably 3 registers total */
		return 3;
	} else {
#if 1
fprintf (stderr, "krocllvm_be_regsfor(): regsfor [%s] [%s] ?\n", benode->tag->ndef->name, benode->tag->name);
#endif
	}
	return 0;
}
/*}}}*/


/*{{{  static int krocllvm_preallocate_block (compops_t *cops, tnode_t *blk, target_t *target)*/
/*
 *	does pre-allocation for a back-end block
 *	returns 0 to stop walk, 1 to continue
 */
static int krocllvm_preallocate_block (compops_t *cops, tnode_t *blk, target_t *target)
{
	krocllvm_priv_t *kpriv = (krocllvm_priv_t *)target->priv;

	if (blk->tag == target->tag_BLOCK) {
		krocllvm_blockhook_t *bh = (krocllvm_blockhook_t *)tnode_nthhookof (blk, 0);

#if 0
fprintf (stderr, "krocllvm_preallocate_block(): preallocating block, ws=%d\n", bh->alloc_ws);
#endif
		if (bh->addstaticlink) {
			tnode_t **stptr = tnode_nthsubaddr (blk, 1);
			krocllvm_namehook_t *nh;
			tnode_t *name;

#if 0
fprintf (stderr, "krocllvm_preallocate_block(): adding static-link..\n");
#endif
			if (!*stptr) {
				*stptr = parser_newlistnode (NULL);
			} else if (!parser_islistnode (*stptr)) {
				tnode_t *slist = parser_newlistnode (NULL);

				parser_addtolist (slist, *stptr);
				*stptr = slist;
			}

			nh = krocllvm_namehook_create (bh->lexlevel, target->pointersize, 0, target->pointersize, 0);
			name = tnode_create (target->tag_NAME, NULL, tnode_create (target->tag_STATICLINK, NULL), NULL, (void *)nh);

			parser_addtolist_front (*stptr, name);
		}
	}

	return 1;
}
/*}}}*/
/*{{{  static int krocllvm_precode_block (compops_t *cops, tnode_t **tptr, codegen_t *cgen)*/
/*
 *	does pre-code generation for a back-end block
 *	return 0 to stop walk, 1 to continue it
 */
static int krocllvm_precode_block (compops_t *cops, tnode_t **tptr, codegen_t *cgen)
{
	krocllvm_blockhook_t *bh = (krocllvm_blockhook_t *)tnode_nthhookof (*tptr, 0);

	if ((*tptr)->tag != krocllvm_target.tag_BLOCK) {
		nocc_internal ("krocllvm_precode_block(): block not back-end BLOCK, was [%s]", (*tptr)->tag->name);
	}
	if (!bh->entrylab) {
		/* give it an entry-point label */
		bh->entrylab = codegen_new_label (cgen);
	}
	return 1;
}
/*}}}*/
/*{{{  static int krocllvm_codegen_block (compops_t *cops, tnode_t *blk, codegen_t *cgen)*/
/*
 *	does code generation for a back-end block
 *	return 0 to stop walk, 1 to continue it
 */
static int krocllvm_codegen_block (compops_t *cops, tnode_t *blk, codegen_t *cgen)
{
	krocllvm_priv_t *kpriv = (krocllvm_priv_t *)cgen->target->priv;
	int ws_size, vs_size, ms_size;
	int ws_offset, adjust;
	int elab, lexlevel;

	if (blk->tag != krocllvm_target.tag_BLOCK) {
		nocc_internal ("krocllvm_codegen_block(): block not back-end BLOCK, was [%s]", blk->tag->name);
		return 0;
	}
	cgen->target->be_getblocksize (blk, &ws_size, &ws_offset, &vs_size, &ms_size, &adjust, &elab);
	lexlevel = cgen->target->be_blocklexlevel (blk);
	dynarray_setsize (cgen->be_blks, lexlevel + 1);
	DA_SETNTHITEM (cgen->be_blks, lexlevel, blk);

	if (elab) {
		codegen_callops (cgen, setlabel, elab);
	}
	codegen_callops (cgen, wsadjust, -(ws_offset - adjust));

	codegen_subcodegen (tnode_nthsubof (blk, 0), cgen);
	codegen_callops (cgen, wsadjust, (ws_offset - adjust));

	DA_SETNTHITEM (cgen->be_blks, lexlevel, NULL);
	dynarray_setsize (cgen->be_blks, lexlevel);

	return 0;
}
/*}}}*/


/*{{{  static int krocllvm_bytesfor_name (langops_t *lops, tnode_t *name, target_t *target)*/
/*
 *	used to get the type-size of a back-end name
 *	returns type-size or -1 if not known
 */
static int krocllvm_bytesfor_name (langops_t *lops, tnode_t *name, target_t *target)
{
	krocllvm_namehook_t *nh;

	if (name->tag != krocllvm_target.tag_NAME) {
		return -1;
	}
	nh = (krocllvm_namehook_t *)tnode_nthhookof (name, 0);
	if (!nh) {
		return -1;
	}
	return nh->typesize;
}
/*}}}*/

/*{{{  static int krocllvm_precode_const (compops_t *cops, tnode_t **cnst, codegen_t *cgen)*/
/*
 *	does pre-code for a back-end constant
 *	returns 0 to stop walk, 1 to continue
 */
static int krocllvm_precode_const (compops_t *cops, tnode_t **cnst, codegen_t *cgen)
{
	krocllvm_priv_t *kpriv = (krocllvm_priv_t *)cgen->target->priv;
	krocllvm_consthook_t *ch = (krocllvm_consthook_t *)tnode_nthhookof (*cnst, 0);
	tnode_t *cref;

	if (ch->label <= 0) {
		ch->label = codegen_new_label (cgen);
	}

	/* move this into pre-codes and leave a reference */
	parser_addtolist (kpriv->precodelist, *cnst);
	cref = tnode_create (kpriv->tag_CONSTREF, NULL, (void *)ch);
	tnode_promotechooks (*cnst, cref);
	*cnst = cref;
	return 1;
}
/*}}}*/
/*{{{  static int krocllvm_codegen_const (compops_t *cops, tnode_t *cnst, codegen_t *cgen)*/
/*
 *	does code-generation for a constant -- these have been pulled out in front of the program
 *	returns 0 to stop walk, 1 to continue
 */
static int krocllvm_codegen_const (compops_t *cops, tnode_t *cnst, codegen_t *cgen)
{
	krocllvm_consthook_t *ch = (krocllvm_consthook_t *)tnode_nthhookof (cnst, 0);

#if 0
fprintf (stderr, "krocllvm_codegen_const(): ch->label = %d, ch->labrefs = %d\n", ch->label, ch->labrefs);
#endif
	if (ch->label > 0) {
		// krocllvm_coder_setlabel (cgen, ch->label);
		// krocllvm_coder_constblock (cgen, ch->byteptr, ch->size);
	}
	return 0;
}
/*}}}*/

/*{{{  static int krocllvm_codegen_nameref (compops_t *cops, tnode_t *nameref, codegen_t *cgen)*/
/*
 *	generates code to load a name -- usually happens inside a RESULT
 *	return 0 to stop walk, 1 to continue
 */
static int krocllvm_codegen_nameref (compops_t *cops, tnode_t *nameref, codegen_t *cgen)
{
	krocllvm_priv_t *kpriv = (krocllvm_priv_t *)cgen->target->priv;
	krocllvm_cgstate_t *cgs = krocllvm_cgstate_cur (cgen);
	krocllvm_coderref_t *cr;

	cr = codegen_callops_r (cgen, ldname, nameref, 0);
	dynarray_add (cgs->cdrefs, cr);

	return 0;
}
/*}}}*/
/*{{{  static tnode_t *krocllvm_gettype_nameref (langops_t *lops, tnode_t *node, tnode_t *defaulttype)*/
/*
 *	returns the type of a name reference
 *	returns type on success, NULL on failure
 */
static tnode_t *krocllvm_gettype_nameref (langops_t *lops, tnode_t *node, tnode_t *defaulttype)
{
	/* transparent */
	return typecheck_gettype (tnode_nthsubof (node, 0), defaulttype);
}
/*}}}*/

/*{{{  static int krocllvm_codegen_constref (compops_t *cops, tnode_t *constref, codegen_t *cgen)*/
/*
 *	generates code for a constant reference -- loads the constant
 *	return 0 to stop walk, 1 to continue
 */
static int krocllvm_codegen_constref (compops_t *cops, tnode_t *constref, codegen_t *cgen)
{
	krocllvm_consthook_t *ch = (krocllvm_consthook_t *)tnode_nthhookof (constref, 0);
	int val;

#if 0
fprintf (stderr, "krocllvm_codegen_constref(): constref node is:\n");
tnode_dumptree (constref, 1, stderr);
#endif
	if (ch->typecat & TYPE_REAL) {
		/*{{{  loading floating-point constant -- must be done via non-local!*/
		switch (ch->size) {
		default:
			codegen_warning (cgen, "krocllvm_codegen_constref(): unhandled real width %d", ch->size);
			break;
		case 4:
			// krocllvm_coder_loadlabaddr (cgen, ch->label);
			// ch->labrefs++;
			// codegen_callops (cgen, tsecondary, I_FPLDNLSN);
			break;
		case 8:
			// krocllvm_coder_loadlabaddr (cgen, ch->label);
			// ch->labrefs++;
			// codegen_callops (cgen, tsecondary, I_FPLDNLDB);
			break;
		}
		/*}}}*/
	} else {
		/*{{{  loading integer constant*/
		krocllvm_priv_t *kpriv = (krocllvm_priv_t *)cgen->target->priv;
		krocllvm_coderref_t *cr = krocllvm_newcoderref ();
		krocllvm_cgstate_t *cgs = krocllvm_cgstate_cur (cgen);

		krocllvm_addcoderref (cgen, cr, LLVM_TYPE_INT | (ch->size * 8) | ((ch->typecat & TYPE_SIGNED) ? LLVM_TYPE_SIGNED : 0));
		
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
		codegen_write_fmt (cgen, "\t%%reg_%d = bitcast %s %d to %s\n", cr->regs[0], krocllvm_typestr (cr->types[0]), val, krocllvm_typestr (cr->types[0]));
		dynarray_add (cgs->cdrefs, cr);

		// krocllvm_cgstate_tsdelta (cgen, 1);
		/*}}}*/
	}
	return 0;
}
/*}}}*/
/*{{{  static tnode_t *krocllvm_dimtreeof_constref (langops_t *lops, tnode_t *node)*/
/*
 *	returns the dimension tree associated with a back-end constant reference.  If we're asking for
 *	this, assumes that the underlying original node is some sort of array.
 */
static tnode_t *krocllvm_dimtreeof_constref (langops_t *lops, tnode_t *node)
{
	krocllvm_consthook_t *ch = (krocllvm_consthook_t *)tnode_nthhookof (node, 0);

	if (ch->orgnode) {
		return langops_dimtreeof (ch->orgnode);
	}
	return NULL;
}
/*}}}*/

/*{{{  static int krocllvm_codegen_indexed (compops_t *cops, tnode_t *indexed, codegen_t *cgen)*/
/*
 *	generates code for an indexed node -- loads the value
 *	returns 0 to stop walk, 1 to continue
 */
static int krocllvm_codegen_indexed (compops_t *cops, tnode_t *indexed, codegen_t *cgen)
{
	codegen_callops (cgen, loadname, indexed, 0);
	return 0;
}
/*}}}*/

/*{{{  static int krocllvm_namemap_result (compops_t *cops, tnode_t **rnodep, map_t *mdata)*/
/*
 *	name-map for a back-end result, sets hook in the map-data and sub-walks
 *	returns 0 to stop walk, 1 to continue
 */
static int krocllvm_namemap_result (compops_t *cops, tnode_t **rnodep, map_t *mdata)
{
	krocllvm_priv_t *kpriv = (krocllvm_priv_t *)mdata->target->priv;
	krocllvm_resultsubhook_t *rh;
	tnode_t *prevresult;

	rh = (krocllvm_resultsubhook_t *)tnode_getchook (*rnodep, kpriv->resultsubhook);
	if (!rh) {
		nocc_internal ("krocllvm_namemap_result(): missing resultsubhook!");
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
/*{{{  static int krocllvm_bemap_result (compops_t *cops, tnode_t **rnodep, map_t *mdata)*/
/*
 *	back-end map for back-end result, collects up size needed for result evaluation
 *	returns 0 to stop walk, 1 to continue
 */
static int krocllvm_bemap_result (compops_t *cops, tnode_t **rnodep, map_t *mdata)
{
	krocllvm_priv_t *kpriv = (krocllvm_priv_t *)mdata->target->priv;
	krocllvm_resultsubhook_t *rh;

	rh = (krocllvm_resultsubhook_t *)tnode_getchook (*rnodep, kpriv->resultsubhook);
	if (!rh) {
		nocc_internal ("krocllvm_bemap_result(): missing resultsubhook!");
		return 0;
	}

	/* sub-map first */
	map_subbemap (tnode_nthsubaddr (*rnodep, 0), mdata);

	/* nocc_warning ("krocllvm_bemap_result(): no sub-things in result.."); */		/* this is probably ok -- e.g. GETPRI() is transparent */
	rh->result_regs = 1;
	rh->result_fregs = 0;				/* FIXME! -- assumption.. */

	return 0;
}
/*}}}*/
/*{{{  static int krocllvm_codegen_result (compops_t *cops, tnode_t *rnode, codegen_t *cgen)*/
/*
 *	generates code for a result -- evaluates as necessary
 *	returns 0 to stop walk, 1 to continue
 */
static int krocllvm_codegen_result (compops_t *cops, tnode_t *rnode, codegen_t *cgen)
{
	krocllvm_priv_t *kpriv = (krocllvm_priv_t *)krocllvm_target.priv;
	krocllvm_resultsubhook_t *rh = (krocllvm_resultsubhook_t *)tnode_getchook (rnode, kpriv->resultsubhook);
	krocllvm_cgstate_t *cgs = krocllvm_cgstate_cur (cgen);
	tnode_t *expr;
	int i;

#if 1
fprintf (stderr, "krocllvm_codegen_result(): loading %d bits..\n", DA_CUR (rh->sublist));
#endif
	for (i=0; i<DA_CUR (rh->sublist); i++) {
		/* load this */
#if 1
fprintf (stderr, "krocllvm_codegen_result(): result %d is:\n", i);
tnode_dumptree (*(DA_NTHITEM (rh->sublist, i)), 1, FHAN_STDERR);
#endif
		codegen_subcodegen (*(DA_NTHITEM (rh->sublist, i)), cgen);
	}
#if 1
fprintf (stderr, "krocllvm_codegen_result(): cgstate has %d entries in cdrefs\n", DA_CUR (cgs->cdrefs));
#endif
	/* then call code-gen on the argument, if it exists */
	expr = tnode_nthsubof (rnode, 0);
	if (expr) {
		codegen_subcodegen (expr, cgen);
	}
	return 0;
}
/*}}}*/


/*{{{  static char *krocllvm_make_namedlabel (const char *lbl)*/
/*
 *	transforms an internal name into a label
 */
static char *krocllvm_make_namedlabel (const char *lbl)
{
	int lbl_len = strlen (lbl);
	char *belbl = (char *)smalloc (lbl_len + 5);
	char *ch;
	int offs = 0;

	/* cater for slightly special names */
	if (!strncmp (lbl, "C.", 2)) {
		strcpy (belbl, lbl);
		offs = 2;
	} else if (!strncmp (lbl, "CIF.", 4)) {
		strcpy (belbl, lbl);
		offs = 4;
	} else if (!strncmp (lbl, "B.", 2)) {
		strcpy (belbl, lbl);
		offs = 2;
	} else if (!strncmp (lbl, "BX.", 3)) {
		strcpy (belbl, lbl);
		offs = 3;
	} else {
		strcpy (belbl, "O_");
		memcpy (belbl + 2, lbl, lbl_len + 1);
		offs = 2;
	}

	for (ch = belbl + offs; *ch != '\0'; ch++) {
		switch (*ch) {
		case '.':
			*ch = '_';
			break;
		}
	}

	return belbl;
}
/*}}}*/
/*{{{  static int krocllvm_decode_tlp (codegen_t *cgen, const char *desc, int *got_kyb, int *got_scr, int *got_err)*/
/*
 *	decodes top-level process header
 *	returns number of top-level channels/other
 */
static int krocllvm_decode_tlp (codegen_t *cgen, const char *desc, int *got_kyb, int *got_scr, int *got_err)
{
	const char *ch;
	int acount = 0;

	if (strncmp (desc, "PROC ", 5)) {
		codegen_error (cgen, "invalid top-level process signature [%s]", desc);
		return 0;
	}
	for (ch = desc + 5; (*ch != '(') && (*ch != '\0'); ch++);
	if (*ch == '\0') {
		codegen_error (cgen, "invalid top-level process signature [%s]", desc);
		return 0;
	}
	ch++;		/* should be in arguments, or end */

	while ((*ch != '\0') && (*ch != ')')) {
		const char *dh;
		char *dstr;

		/* skip any leading whitespace */
		for (; (*ch == ' ') || (*ch == '\t'); ch++);
		for (dh=ch; (*dh != '\0') && (*dh != ')') && (*dh != ','); dh++);

		/* try and figure out what the parameter 'dstr' represents */
		dstr = string_ndup (ch, (int)(dh - ch));
#if 0
fprintf (stderr, "TLP: probably [%s]\n", dstr);
#endif
		if (!strncmp (dstr, "CHAN! BYTE ", 11)) {
			/* output channel of bytes: screen or error */
			switch (dstr[11]) {
			case 's':
			case 'o':
			default:
				/* assume screen */
				if (!*got_scr) {
					*got_scr = ++acount;
				} else if (!*got_err) {
					/* else, assume error (vaguely) */
					*got_err = ++acount;
				} else {
					codegen_warning (cgen, "confused about top-level process parameter [%s]", dstr);
				}
				break;
			case 'e':
				/* assume error */
				if (!*got_err) {
					*got_err = ++acount;
				} else {
					codegen_warning (cgen, "confused about top-level process parameter [%s]", dstr);
				}
				break;
			}
		} else if (!strncmp (dstr, "CHAN? BYTE ", 11)) {
			/* input channel of bytes: keyboard */
			if (!*got_kyb) {
				*got_kyb = ++acount;
			} else {
				codegen_warning (cgen, "confused about top-level process parameter [%s]", dstr);
			}
		}
		sfree (dstr);

		if (*dh != '\0') {
			ch = dh + 1;
		} else {
			ch = dh;
		}
	}

	return acount;
}
/*}}}*/


/*{{{  static int krocllvm_precode_special (compops_t *cops, tnode_t **spec, codegen_t *cgen)*/
/*
 *	does pre-code for a back-end special
 *	returns 0 to stop walk, 1 to continue
 */
static int krocllvm_precode_special (compops_t *cops, tnode_t **spec, codegen_t *cgen)
{
	return 1;
}
/*}}}*/
/*{{{  static int krocllvm_codegen_special (compops_t *cops, tnode_t *spec, codegen_t *cgen)*/
/*
 *	does code-gen for a back-end special
 *	returns 0 to stop walk, 1 to continue
 */
static int krocllvm_codegen_special (compops_t *cops, tnode_t *spec, codegen_t *cgen)
{
	krocllvm_priv_t *kpriv = (krocllvm_priv_t *)cgen->target->priv;

	if (spec->tag == kpriv->tag_JENTRY) {
		if (kpriv->toplevelname) {
			char *belbl = krocllvm_make_namedlabel (NameNameOf (kpriv->toplevelname));
			int got_kyb = 0, got_scr = 0, got_err = 0;
			int ntl = 0;
			krocllvm_memspacehook_t *msh = kpriv->tl_mem;

			if (kpriv->lastdesc) {
#if 1
fprintf (stderr, "krocllvm_codegen_special: JENTRY: lastdesc = [%s]\n", kpriv->lastdesc);
#endif
				ntl = krocllvm_decode_tlp (cgen, kpriv->lastdesc, &got_kyb, &got_scr, &got_err);
			}

			codegen_write_fmt (cgen, "; .jentry\t%s\n\n", belbl);
			codegen_write_fmt (cgen, "declare i32 @occam_start (i32, i8**, i8*, i8*, i8**, i8*, i32, i32, i32)\n\n");

			if (got_kyb) {
				codegen_write_fmt (cgen, "@tlp_%d = internal constant [ 10 x i8 ] c\"keyboard?\\00\"\n", got_kyb);
			}
			if (got_scr) {
				codegen_write_fmt (cgen, "@tlp_%d = internal constant [ 8 x i8 ] c\"screen!\\00\"\n", got_scr);
			}
			if (got_err) {
				codegen_write_fmt (cgen, "@tlp_%d = internal constant [ 7 x i8 ] c\"error!\\00\"\n", got_err);
			}

			codegen_write_fmt (cgen, "@tlp_desc = internal constant [ %d x i8* ] [", ntl + 1);
			if (got_kyb) {
				codegen_write_fmt (cgen, " i8* getelementptr ([ 10 x i8 ]* @tlp_%d, i32 0, i32 0),", got_kyb);
			}
			if (got_scr) {
				codegen_write_fmt (cgen, " i8* getelementptr ([ 8 x i8 ]* @tlp_%d, i32 0, i32 0),", got_scr);
			}
			if (got_err) {
				codegen_write_fmt (cgen, " i8* getelementptr ([ 7 x i8 ]* @tlp_%d, i32 0, i32 0),", got_err);
			}
			codegen_write_fmt (cgen, " i8* null]\n\n");

			codegen_write_fmt (cgen, "define fastcc void @code_exit (i8* %%sched, i32* %%wptr) {\n");
			codegen_write_fmt (cgen, "\tret void\n}\n\n");

			codegen_write_fmt (cgen, "define void @code_entry (i8* %%sched, i32* %%wptr) {\n");
			codegen_write_fmt (cgen, "\t%%iptr_ptr = getelementptr i32* %%wptr, i32 -1\n");
			codegen_write_fmt (cgen, "\t%%iptr_val = load i32* %%iptr_ptr\n");
			codegen_write_fmt (cgen, "\t%%iptr = inttoptr i32 %%iptr_val to void (i8*, i32*)*\n");
			// codegen_write_fmt (cgen, "\t%%wptr2 = getelementptr i32* %%wptr, i32 -%d\n", msh ? (msh->adjust >> LLVM_SIZE_SHIFT): 0);
			codegen_write_fmt (cgen, "\ttail call fastcc void %%iptr (i8* %%sched, i32* %%wptr) noreturn\n");
			codegen_write_fmt (cgen, "\tret void\n}\n\n");

			codegen_write_fmt (cgen, "define i32 @main (i32 %%argc, i8** %%argv) {\n");
			codegen_write_fmt (cgen, "entry:\n");
			codegen_write_fmt (cgen, "\t%%code_entry = bitcast void (i8*, i32*)* @code_entry to i8*\n");
			codegen_write_fmt (cgen, "\t%%code_exit = bitcast void (i8*, i32*)* @code_exit to i8*\n");
			codegen_write_fmt (cgen, "\t%%start_proc = bitcast void (i8*, i32*)* @%s to i8*\n", belbl);
			codegen_write_fmt (cgen, "\t%%ret = call i32 @occam_start (i32 %%argc, i8** %%argv,");
			codegen_write_fmt (cgen, " i8* %%code_entry, i8* %%code_exit,");
			codegen_write_fmt (cgen, " i8** getelementptr ([%d x i8*]* @tlp_desc, i32 0, i32 0),", ntl + 1);
			codegen_write_fmt (cgen, " i8* %%start_proc,");
			if (msh) {
				codegen_write_fmt (cgen, " i32 %d, i32 %d, i32 %d)\n", msh->ws >> LLVM_SIZE_SHIFT, msh->vs >> LLVM_SIZE_SHIFT, msh->ms >> LLVM_SIZE_SHIFT);
			} else {
				codegen_write_fmt (cgen, " i32 512, i32 0, i32 0)\n");
			}
			codegen_write_fmt (cgen, "\tret i32 %%ret\n");
			codegen_write_fmt (cgen, "}\n\n");
			// codegen_write_fmt (cgen, ".jentry\t%s\n", belbl);
			sfree (belbl);
		}
	} else if (spec->tag == kpriv->tag_DESCRIPTOR) {
		char **str = (char **)tnode_nthhookaddr (spec, 0);

		/* *str should be a descriptor line */
		codegen_write_fmt (cgen, "; .descriptor\t\"%s\"\n", *str);
	}
	return 1;
}
/*}}}*/
/*{{{  static void krocllvm_be_precode_seenproc (codegen_t *cgen, name_t *name, tnode_t *node)*/
/*
 *	called during pre-code traversal with names of PROC definitions
 */
static void krocllvm_be_precode_seenproc (codegen_t *cgen, name_t *name, tnode_t *node)
{
	krocllvm_priv_t *kpriv = (krocllvm_priv_t *)cgen->target->priv;
	chook_t *fetransdeschook = tnode_lookupchookbyname ("fetrans:descriptor");

	kpriv->toplevelname = name;
	if (fetransdeschook) {
		void *dhook = tnode_getchook (node, fetransdeschook);

		if (dhook) {
			/* add descriptor special node */
			char *dstr = (char *)dhook;
			char *ch, *dh;

#if 0
fprintf (stderr, "krocllvm_be_precode_seenproc(): seen descriptor [%s], doing back-end name trans\n", dstr);
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
					bename = krocllvm_make_namedlabel (ch);
					*ch = '\0';
					sprintf (newdesc, "%s%s %s", dstr, bename, dh);
					tnode_setchook (node, fetransdeschook, newdesc);
					dhook = (void *)newdesc;
					dstr = newdesc;
				}
			}
#if 0
fprintf (stderr, "krocllvm_be_precode_seenproc(): descriptor now [%s]\n", dstr);
#endif
			tnode_t *dspec = tnode_create (kpriv->tag_DESCRIPTOR, NULL, (void *)string_dup (dstr));
			kpriv->lastdesc = dstr;

			parser_addtolist (kpriv->precodelist, dspec);
		}
	}
	return;
}
/*}}}*/


/*{{{  static coderref_t krocllvm_coder_ldptr (codegen_t *cgen, tnode_t *name, int offset)*/
/*
 *	loads a constant
 */
static coderref_t krocllvm_coder_ldptr (codegen_t *cgen, tnode_t *name, int offset)
{
	krocllvm_priv_t *kpriv = (krocllvm_priv_t *)(krocllvm_target.priv);
	int ref_lexlevel, act_lexlevel;
	krocllvm_coderref_t *cr = krocllvm_newcoderref ();
	krocllvm_cgstate_t *cgs = krocllvm_cgstate_cur (cgen);

	if (name->tag == krocllvm_target.tag_NAMEREF) {
		/*{{{  FIXME: loading pointer to name*/
		krocllvm_namehook_t *nh = (krocllvm_namehook_t *)tnode_nthhookof (name, 0);
		tnode_t *fename = tnode_nthsubof (name, 0);
		tnode_t *fenamehook = (tnode_t *)tnode_getchook (fename, kpriv->mapchook);
		int local;

		ref_lexlevel = nh->lexlevel;
		act_lexlevel = cgen->target->be_blocklexlevel (fenamehook);
		local = (ref_lexlevel == act_lexlevel);

		if (!local) {
			/*{{{  non-local load*/
			/*}}}*/
		} else {
			/*{{{  local load*/
			krocllvm_addcoderref (cgen, cr, LLVM_TYPE_INT | 32);

			codegen_write_fmt (cgen, "\t; loadnameptr %d, %d+%d\n", nh->indir, nh->ws_offset, offset);

			if (nh->indir == 0) {
				codegen_write_fmt (cgen, "\t%%reg_%d = getelementptr i32* %%wptr_%d, i32 %d\n", cr->regs[0], cgs->wsreg, (nh->ws_offset + offset) >> LLVM_SIZE_SHIFT);
			} else {
				int i, tr;

				tr = krocllvm_newreg (cgen);
				codegen_write_fmt (cgen, "\t%%reg_%d = getelementptr i32* %%wptr_%d, i32 %d\n", tr, cgs->wsreg, (nh->ws_offset >> LLVM_SIZE_SHIFT));
				codegen_write_fmt (cgen, "\t%%reg_%d = load i32* %%reg_%d\n", cr->regs[0], tr);

				/* FIXME: nonlocal */
			}
			/*}}}*/
		}
		/*}}}*/
	} else if (name->tag == krocllvm_target.tag_INDEXED) {
		/*{{{  FIXME: indexed node*/

		/*}}}*/
	} else if (name->tag == kpriv->tag_CONSTREF) {
		/*{{{  FIXME: loading pointer to a constant*/

		/*}}}*/
	} else if (name->tag == krocllvm_target.tag_NAME) {
		/*{{{  FIXME: loading pointer to name with no scope (specials only!)*/

		/*}}}*/
	} else if (name->tag == krocllvm_target.tag_RESULT) {
		/*{{{  FIXME: loading pointer to some evaluated result*/

		/*}}}*/
	} else {
		nocc_warning ("krocllvm_coder_ldptr(): don\'t know how to load a pointer to [%s]", name->tag->name);
	}

	return (coderref_t)cr;
}
/*}}}*/
/*{{{  static coderref_t krocllvm_coder_ldname (codegen_t *cgen, tnode_t *name, int offset)*/
/*
 *	loads a back-end name
 */
static coderref_t krocllvm_coder_ldname (codegen_t *cgen, tnode_t *name, int offset)
{
	krocllvm_priv_t *kpriv = (krocllvm_priv_t *)(krocllvm_target.priv);
	krocllvm_coderref_t *cr = krocllvm_newcoderref ();

	if (name->tag == krocllvm_target.tag_NAMEREF) {
		/*{{{  loading name via reference*/
		krocllvm_namehook_t *nh = (krocllvm_namehook_t *)tnode_nthhookof (name, 0);
		int i;

#if 0
fprintf (stderr, "krocllvm_coder_loadname(): NAMEREF, name =\n");
tnode_dumptree (name, 1, stderr);
// fprintf (stderr, "krocllvm_coder_loadname(): hook typecat = 0x%8.8x\n", (unsigned int)nh->typecat);
#endif
		switch (nh->indir) {
		case 0:
			if (nh->typecat & TYPE_REAL) {
				/*{{{  floating-point type*/
				switch (nh->typesize) {
				default:
					codegen_warning (cgen, "krocllvm_coder_loadname(): unhandled REAL typesize %d", nh->typesize);
					break;
				case 4:
					codegen_callops (cgen, loadlocalpointer, nh->ws_offset + offset);
					codegen_callops (cgen, tsecondary, I_FPLDNLSN);
					break;
				case 8:
					codegen_callops (cgen, loadlocalpointer, nh->ws_offset + offset);
					codegen_callops (cgen, tsecondary, I_FPLDNLDB);
					break;
				}

				/*}}}*/
			} else {
				/*{{{  integer (or pointer) type*/
				switch (nh->typesize) {
				default:
					/* word or don't know, just do load-local (word) */
					codegen_write_fmt (cgen, "\tldl\t%d\n", (nh->ws_offset + offset) >> LLVM_SIZE_SHIFT);
					break;
				case 1:
					/* byte-size load */
					codegen_callops (cgen, loadlocalpointer, (nh->ws_offset + offset) >> LLVM_SIZE_SHIFT);
					codegen_write_fmt (cgen, "\tlb\n");
					break;
				case 2:
					/* half-word-size load */
					codegen_callops (cgen, loadlocalpointer, (nh->ws_offset + offset) >> LLVM_SIZE_SHIFT);
					codegen_write_fmt (cgen, "\tlw\n");
					break;
				}

				/*}}}*/
			}
			break;
		default:
			codegen_write_fmt (cgen, "\tldl\t%d\n", nh->ws_offset >> LLVM_SIZE_SHIFT);

			for (i=0; i<(nh->indir - 1); i++) {
				codegen_write_fmt (cgen, "\tldnl\t0\n");
			}
			if (offset) {
				codegen_callops (cgen, addconst, offset);
			}
			if (nh->typecat & TYPE_REAL) {
				/*{{{  floating-point type*/

				switch (nh->typesize) {
				default:
					codegen_warning (cgen, "krocllvm_coder_loadname(): unhandled REAL typesize %d", nh->typesize);
					break;
				case 4:
					codegen_callops (cgen, tsecondary, I_FPLDNLSN);
					break;
				case 8:
					codegen_callops (cgen, tsecondary, I_FPLDNLDB);
					break;
				}

				/*}}}*/
			} else {
				/*{{{  integer (or pointer) type*/
				switch (nh->typesize) {
				default:
					/* word or don't know, just do load non-local (word) */
					codegen_write_fmt (cgen, "\tldnl\t0\n");
					break;
				case 1:
					/* byte-size load */
					codegen_write_fmt (cgen, "\tlb\n");
					break;
				case 2:
					/* half-word-size load */
					codegen_write_fmt (cgen, "\tlw\n");
					break;
				}

				/*}}}*/
			}
			break;
		}
		/*}}}*/
	} else if (name->tag == krocllvm_target.tag_INDEXED) {
		/*{{{  load something out of an indexed node (array typically)*/
		krocllvm_indexedhook_t *ih = (krocllvm_indexedhook_t *)tnode_nthhookof (name, 0);

		codegen_callops_r (cgen, ldptr, tnode_nthsubof (name, 0), 0);
		codegen_callops_r (cgen, ldname, tnode_nthsubof (name, 1), 0);

		if (ih->isize > 1) {
			codegen_callops (cgen, loadconst, ih->isize);
			codegen_write_fmt (cgen, "\tprod\n");
		}
#if 0
fprintf (stderr, "krocetc_coder_loadname(): about to SUM base and offset..\n");
#endif
		codegen_write_fmt (cgen, "\tsum\n");

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
			codegen_error (cgen, "krocllvm_coder_loadname(): INDEXED: index size %d not supported here", ih->isize);
			break;
		}
		/*}}}*/
	} else if (name->tag == kpriv->tag_CONSTREF) {
		/*{{{  load constant via reference*/
		krocllvm_consthook_t *ch = (krocllvm_consthook_t *)tnode_nthhookof (name, 0);
		int val;

	#if 0
	fprintf (stderr, "krocllvm_codegen_constref(): constref node is:\n");
	tnode_dumptree (constref, 1, stderr);
	#endif
		if (ch->typecat & TYPE_REAL) {
			/*{{{  loading floating-point constant -- must be done via non-local!*/
			switch (ch->size) {
			default:
				codegen_warning (cgen, "krocllvm_codegen_constref(): unhandled real width %d", ch->size);
				break;
			case 4:
				// krocllvm_coder_loadlabaddr (cgen, ch->label);
				// ch->labrefs++;
				// codegen_callops (cgen, tsecondary, I_FPLDNLSN);
				break;
			case 8:
				// krocllvm_coder_loadlabaddr (cgen, ch->label);
				// ch->labrefs++;
				// codegen_callops (cgen, tsecondary, I_FPLDNLDB);
				break;
			}
			/*}}}*/
		} else {
			/*{{{  loading integer constant*/
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
			krocllvm_addcoderref (cgen, cr, LLVM_TYPE_INT | (ch->size * 8));

			codegen_write_fmt (cgen, "\t; loadconstant %d\n", val);
			codegen_write_fmt (cgen, "\t%%reg_%d = bitcast i32 %d to i32\n", cr->regs[0], val);
			/*}}}*/
		}

		/*}}}*/
	} else if (name->tag == krocllvm_target.tag_CONST) {
		/*{{{  constant*/
		codegen_write_fmt (cgen, "; FIXME: load name / CONST\n");
		/*}}}*/
	} else if (name->tag == krocllvm_target.tag_BLOCKREF) {
		/*{{{  block reference*/
		/* this should be used to call things like functions, etc. */
		tnode_t *subref = tnode_nthsubof (name, 0);

		codegen_subcodegen (subref, cgen);
		/*}}}*/
	} else if (name->tag == krocllvm_target.tag_NAME) {
		/*{{{  loading a name with no scope (specials only!)*/
		tnode_t *realname = tnode_nthsubof (name, 0);

		if (realname->tag == cgen->target->tag_STATICLINK) {
			krocllvm_namehook_t *nh = (krocllvm_namehook_t *)tnode_nthhookof (name, 0);

			codegen_write_fmt (cgen, "\tldl\t%d\n", nh->ws_offset);
		} else {
			nocc_warning ("krocllvm_coder_loadname(): don\'t know how to load a name of [%s]", name->tag->name);
		}
		/*}}}*/
	} else if (name->tag == krocllvm_target.tag_RESULT) {
		/*{{{  loading some evaluated result*/
#if 0
fprintf (stderr, "krocetc_coder_loadname(): loading RESULT\n");
#endif
		codegen_subcodegen (name, cgen);

		/*}}}*/
	}

	return (coderref_t)cr;
}
/*}}}*/
/*{{{  static coderref_t krocllvm_coder_ldconst (codegen_t *cgen, int val, int bits, int issigned)*/
/*
 *	loads an integer constant.
 */
static coderref_t krocllvm_coder_ldconst (codegen_t *cgen, int val, int bits, int issigned)
{
	krocllvm_priv_t *kpriv = (krocllvm_priv_t *)(krocllvm_target.priv);
	krocllvm_coderref_t *cr = krocllvm_newcoderref ();

	krocllvm_addcoderref (cgen, cr, LLVM_TYPE_INT | bits | (issigned ? LLVM_TYPE_SIGNED : 0));

	codegen_write_fmt (cgen, "\t%%reg_%d = bitcast %s %d to %s\n", cr->regs[0], krocllvm_typestr (cr->types[0]), val, krocllvm_typestr (cr->types[0]));

	return (coderref_t)cr;
}
/*}}}*/
/*{{{  static coderref_t krocllvm_coder_iop (codegen_t *cgen, int instr, ...)*/
/*
 *	performs some integer operation on registers.
 */
static coderref_t krocllvm_coder_iop (codegen_t *cgen, int instr, ...)
{
	va_list ap;
	int i;
	krocllvm_coderref_t *cr;

	va_start (ap, instr);
	switch (instr) {
	case I_ADD:
		{
			krocllvm_coderref_t *op0 = va_arg (ap, krocllvm_coderref_t *);
			krocllvm_coderref_t *op1 = va_arg (ap, krocllvm_coderref_t *);

			cr = krocllvm_newcoderref_init (cgen, op0->types[0]);
			codegen_write_fmt (cgen, "\t%%reg_%d = %%reg_%d\n", cr->regs[0], op0->regs[0]);
		}
		break;
	default:
		codegen_error (cgen, "LLVM does not support integer operation %d", instr);
		cr = krocllvm_newcoderref ();
		break;
	}
	va_end (ap);

	return (coderref_t)cr;
}
/*}}}*/
/*{{{  static void krocllvm_coder_stname (codegen_t *cgen, tnode_t *name, int offset, coderref_t val)*/
/*
 *	stores something in a back-end name
 */
static void krocllvm_coder_stname (codegen_t *cgen, tnode_t *name, int offset, coderref_t val)
{
	krocllvm_priv_t *kpriv = (krocllvm_priv_t *)(cgen->target->priv);
	krocllvm_cgstate_t *cgs = krocllvm_cgstate_cur (cgen);
	krocllvm_coderref_t *c_val = (krocllvm_coderref_t *)val;

	if (name->tag == krocllvm_target.tag_NAMEREF) {
		/*{{{  store into a name reference*/
		krocllvm_namehook_t *nh = (krocllvm_namehook_t *)tnode_nthhookof (name, 0);
		int i;

		switch (nh->indir) {
		case 0:
			if (nh->typecat & TYPE_REAL) {
				/*{{{  store floating-point*/
				codegen_write_fmt (cgen, "; FIXME: write real\n");
				/*}}}*/
			} else {
				/*{{{  store integer*/
				switch (nh->typesize) {
				default:
					/* word size or don't know, just do store-local */
					codegen_write_fmt (cgen, "\t%%tmp_%d = getelementptr\ti32* %%wptr_%d, i32 %d\n", kpriv->tmpcount, cgs->wsreg, (nh->ws_offset + offset) >> LLVM_SIZE_SHIFT);
					codegen_write_fmt (cgen, "\tstore i32 %%reg_%d, i32* %%tmp_%d\n", c_val->regs[0], kpriv->tmpcount);
					kpriv->tmpcount++;
					break;
				}
				/*}}}*/
			}
			break;
		}
		/* FIXME! */

		/*}}}*/
	} else if (name->tag == krocllvm_target.tag_INDEXED) {
		/*{{{  store into an indexed node*/
		/* FIXME! */

		/*}}}*/
	} else {
		nocc_warning ("krocllvm_coder_storename(): don\'t know how to store into a [%s]", name->tag->name);
	}
	return;
}
/*}}}*/
/*{{{  static void krocllvm_coder_kicall (codegen_t *cgen, int call, ...)*/
/*
 *	do run-time kernel call
 */
static void krocllvm_coder_kicall (codegen_t *cgen, int call, ...)
{
	krocllvm_priv_t *kpriv = (krocllvm_priv_t *)cgen->target->priv;
	krocllvm_cgstate_t *cgs = krocllvm_cgstate_cur (cgen);
	int idx;

	for (idx=0; (kitable[idx].call != call) && (kitable[idx].call != I_INVALID); idx++);
	if (kitable[idx].call == I_INVALID) {
		codegen_warning (cgen, "unsupported kernel call %d", call);
		codegen_write_fmt (cgen, "; INVALID KICALL %d\n", call);
	} else {
		int iregs, oregs, i;
		va_list ap;
		int nargs;

		iregs = kitable[idx].iregs;
		oregs = kitable[idx].oregs;

		nargs = iregs + oregs;

		codegen_write_fmt (cgen, "\t; KICALL %d (%d,%d)\n", call, iregs, oregs);
		/* call point */
		codegen_write_fmt (cgen, "\ttail call fastcc void @%s_%d (i8* %%sched, i32* %%wptr_%d", kpriv->lastfunc, kpriv->labcount, cgs->wsreg);
		va_start (ap, call);
		for (i=0; i<nargs; i++) {
			krocllvm_coderref_t *r_arg = va_arg (ap, krocllvm_coderref_t *);

			codegen_write_fmt (cgen, ", i32 %%reg_%d", r_arg->regs[0]);
		}
		va_end (ap);
		codegen_write_fmt (cgen, ") noreturn\n");
		codegen_write_fmt (cgen, "\tret void\n");
		codegen_write_fmt (cgen, "}\n\n");
		cgs->wsreg++;
		codegen_write_fmt (cgen, "define private fastcc void @%s_%d (i8* %%sched, i32* %%wptr_%d", kpriv->lastfunc, kpriv->labcount, cgs->wsreg);
		va_start (ap, call);
		for (i=0; i<nargs; i++) {
			krocllvm_coderref_t *r_arg = va_arg (ap, krocllvm_coderref_t *);

			codegen_write_fmt (cgen, ", i32 %%reg_%d", r_arg->regs[0]);
		}
		va_end (ap);
		codegen_write_fmt (cgen, ") {\n");
		kpriv->labcount++;

		/* firstly, drop return address in Wptr[-1] */
		codegen_write_fmt (cgen, "\t%%tmp_%d = getelementptr i32* %%wptr_%d, i32 -1\n", kpriv->tmpcount, cgs->wsreg);
		codegen_write_fmt (cgen, "\t%%tmp_%d = bitcast void (i8*, i32*)* @%s_%d to i8*\n", kpriv->tmpcount + 1, kpriv->lastfunc, kpriv->labcount);
		codegen_write_fmt (cgen, "\t%%tmp_%d = ptrtoint i8* %%tmp_%d to i32\n", kpriv->tmpcount + 2, kpriv->tmpcount + 1);
		codegen_write_fmt (cgen, "\tstore i32 %%tmp_%d, i32* %%tmp_%d\n", kpriv->tmpcount + 2, kpriv->tmpcount);
		kpriv->tmpcount += 3;

		/* make call */
		codegen_write_fmt (cgen, "\t%%tmp_%d = call i32* @%s (i8* %%sched, i32* %%wptr_%d", kpriv->tmpcount, kitable[idx].ename, cgs->wsreg);
		va_start (ap, call);
		for (i=0; i<nargs; i++) {
			krocllvm_coderref_t *r_arg = va_arg (ap, krocllvm_coderref_t *);

			codegen_write_fmt (cgen, ", i32 %%reg_%d", r_arg->regs[0]);
		}
		va_end (ap);
		codegen_write_fmt (cgen, ")\n");
		codegen_write_fmt (cgen, "\t%%tmp_%d = getelementptr i32* %%tmp_%d, i32 -1\n", kpriv->tmpcount + 1, kpriv->tmpcount);
		codegen_write_fmt (cgen, "\t%%tmp_%d = load i32* %%tmp_%d\n", kpriv->tmpcount + 2, kpriv->tmpcount + 1);
		codegen_write_fmt (cgen, "\t%%tmp_%d = inttoptr i32 %%tmp_%d to void (i8*, i32*)*\n", kpriv->tmpcount + 3, kpriv->tmpcount + 2);
		codegen_write_fmt (cgen, "\ttail call fastcc void %%tmp_%d (i8* %%sched, i32* %%tmp_%d) noreturn\n", kpriv->tmpcount + 3, kpriv->tmpcount);
		codegen_write_fmt (cgen, "\tret void\n");
		kpriv->tmpcount += 3;

		/* return point, with new Wptr */
		cgs->wsreg++;
		codegen_write_fmt (cgen, "}\n\n");
		codegen_write_fmt (cgen, "define private fastcc void @%s_%d (i8* %%sched, i32* %%wptr_%d) {\n", kpriv->lastfunc, kpriv->labcount, cgs->wsreg);

		kpriv->labcount++;
	}
	//codegen_write_fmt (cgen, "define fastcc void @%s (i8* %%sched, i32* %%wptr_%d) {\n", belbl, cgs->wsreg);

	// krocllvm_coderref_t *r_chan = (krocllvm_coderref_t *)chan;
	// krocllvm_coderref_t *r_val = (krocllvm_coderref_t *)val;

	// codegen_write_fmt (cgen, "; KICALL2: %d with %s, %s\n", call, krocllvm_typestr (r_chan->types[0]), krocllvm_typestr (r_val->types[0]));

	return;
}
/*}}}*/
/*{{{  static void krocllvm_coder_freeref (codegen_t *cgen, coderref_t ref)*/
/*
 *	free coder reference
 */
static void krocllvm_coder_freeref (codegen_t *cgen, coderref_t ref)
{
	krocllvm_coderref_t *r_ref = (krocllvm_coderref_t *)ref;

	if (!r_ref) {
		codegen_warning (cgen, "krocllvm_coder_freeref(): NULL reference given!");
		return;
	}

	krocllvm_freecoderref (r_ref);
}
/*}}}*/
/*{{{  static void krocllvm_coder_wsadjust (codegen_t *cgen, int adjust)*/
/*
 *	generates workspace adjustment
 */
static void krocllvm_coder_wsadjust (codegen_t *cgen, int adjust)
{
	krocllvm_priv_t *kpriv = (krocllvm_priv_t *)(krocllvm_target.priv);
	krocllvm_cgstate_t *cgs = krocllvm_cgstate_cur (cgen);
	int newws = ++kpriv->regcount;

	if (!cgs) {
		codegen_error (cgen, "krocllvm_coder_wsadjust(): no current generator state!\n");
		return;
	}
	codegen_write_fmt (cgen, "\t; wsadjust %d\n", adjust);
	codegen_write_fmt (cgen, "\t%%wptr_%d = getelementptr i32* %%wptr_%d, i32 %d\n", newws, cgs->wsreg, adjust >> LLVM_SIZE_SHIFT);
	cgs->wsreg = newws;
	return;
}
/*}}}*/
/*{{{  static coderref_t krocllvm_coder_getref (codegen_t *cgen)*/
/*
 *	called to get reference from the private stack
 */
static coderref_t krocllvm_coder_getref (codegen_t *cgen)
{
	krocllvm_cgstate_t *cgs = krocllvm_cgstate_cur (cgen);
	coderref_t cr;

	if (!DA_CUR (cgs->cdrefs)) {
		nocc_internal ("krocllvm_coder_getref(): nothing to return!");
		return NULL;
	}
	cr = (coderref_t)DA_NTHITEM (cgs->cdrefs, DA_CUR (cgs->cdrefs) - 1);
	dynarray_delitem (cgs->cdrefs, DA_CUR (cgs->cdrefs) - 1);

	return cr;
}
/*}}}*/
/*{{{  static void krocllvm_coder_addref (codegen_t *cgen, coderref_t cr)*/
/*
 *	called to add reference to the private stack
 */
static void krocllvm_coder_addref (codegen_t *cgen, coderref_t cr)
{
	krocllvm_cgstate_t *cgs = krocllvm_cgstate_cur (cgen);
	krocllvm_coderref_t *ccr = (krocllvm_coderref_t *)cr;

	dynarray_add (cgs->cdrefs, ccr);

	return;
}
/*}}}*/


/*{{{  static void krocllvm_coder_comment (codegen_t *cgen, const char *fmt, ...)*/
/*
 *	generates a comment
 */
static void krocllvm_coder_comment (codegen_t *cgen, const char *fmt, ...)
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
/*{{{  static void krocllvm_coder_setmemsize (codegen_t *cgen, int ws, int adjust, int vs, int ms)*/
/*
 *	generates memory requirements
 */
static void krocllvm_coder_setmemsize (codegen_t *cgen, int ws, int adjust, int vs, int ms)
{
	krocllvm_priv_t *kpriv = (krocllvm_priv_t *)(krocllvm_target.priv);

	if (kpriv->tl_mem && !compopts.notmainmodule) {
		krocllvm_memspacehook_free (kpriv->tl_mem);
		kpriv->tl_mem = NULL;
	}
	if (!compopts.notmainmodule) {
		kpriv->tl_mem = krocllvm_memspacehook_create ();
		kpriv->tl_mem->ws = ws;
		kpriv->tl_mem->adjust = adjust;
		kpriv->tl_mem->vs = vs;
		kpriv->tl_mem->ms = ms;
	}

	codegen_write_fmt (cgen, "; .SETMEMSIZE %d, %d, %d, %d\n", ws, adjust, vs, ms);
}
/*}}}*/
/*{{{  static void krocllvm_coder_setnamelabel (codegen_t *cgen, name_t *name)*/
/*
 *	sets a named label, but from a name_t (includes namespace)
 */
static void krocllvm_coder_setnamelabel (codegen_t *cgen, name_t *name)
{
	char *lbl = name_newwholename (name);

	codegen_callops (cgen, setnamedlabel, lbl);
	sfree (lbl);
	return;
}
/*}}}*/
/*{{{  static void krocllvm_coder_setnamedlabel (codegen_t *cgen, const char *lbl)*/
/*
 *	sets a named label
 */
static void krocllvm_coder_setnamedlabel (codegen_t *cgen, const char *lbl)
{
	codegen_write_fmt (cgen, "; .setnamedlabel \"%s\"\n", lbl);

	return;
}
/*}}}*/
/*{{{  static void krocllvm_coder_setlabel (codegen_t *cgen, int lbl)*/
/*
 *	sets a numeric label
 */
static void krocllvm_coder_setlabel (codegen_t *cgen, int lbl)
{
	codegen_write_fmt (cgen, "; .setlabel %d\n", lbl);

	return;
}
/*}}}*/
/*{{{  static void krocllvm_coder_procentry (codegen_t *cgen, const char *lbl)*/
/*
 *	generates a procedure entry stub
 */
static void krocllvm_coder_procentry (codegen_t *cgen, const char *lbl)
{
	krocllvm_priv_t *kpriv = (krocllvm_priv_t *)(krocllvm_target.priv);
	krocllvm_cgstate_t *cgs;
	char *belbl = krocllvm_make_namedlabel (lbl);

	codegen_write_fmt (cgen, "; .procentry \"%s\"\n", lbl);

	cgs = krocllvm_cgstate_newpush (cgen);
	cgs->wsreg = ++kpriv->regcount;

	codegen_write_fmt (cgen, "define fastcc void @%s (i8* %%sched, i32* %%wptr_%d) {\n", belbl, cgs->wsreg);
	codegen_write_fmt (cgen, "entry:\n");

	krocllvm_setlastlabel (cgen, belbl);

	sfree (belbl);

}
/*}}}*/
/*{{{  static void krocllvm_coder_procnameentry (codegen_t *cgen, name_t *name)*/
/*
 *	generates a procedure entry from a name_t (includes namespace)
 */
static void krocllvm_coder_procnameentry (codegen_t *cgen, name_t *name)
{
	char *lbl = name_newwholename (name);

	codegen_callops (cgen, procentry, lbl);
	sfree (lbl);
	return;
}
/*}}}*/
/*{{{  static void krocllvm_coder_procreturn (codegen_t *cgen, int adjust)*/
/*
 *	generates a procedure return
 */
static void krocllvm_coder_procreturn (codegen_t *cgen, int adjust)
{
	krocllvm_priv_t *kpriv = (krocllvm_priv_t *)(krocllvm_target.priv);
	krocllvm_cgstate_t *cgs = krocllvm_cgstate_cur (cgen);

	codegen_write_fmt (cgen, "\t; .procreturn %d\n", adjust);
	codegen_write_fmt (cgen, "\t%%tmp_%d = load i32* %%wptr_%d\n", kpriv->tmpcount, cgs->wsreg);
	codegen_write_fmt (cgen, "\t%%tmp_%d = inttoptr i32 %%tmp_%d to void (i8*, i32*)*\n", kpriv->tmpcount + 1, kpriv->tmpcount);
	codegen_write_fmt (cgen, "\t%%tmp_%d = getelementptr i32* %%wptr_%d, i32 %d\n", kpriv->tmpcount + 2, cgs->wsreg, adjust >> LLVM_SIZE_SHIFT);
	codegen_write_fmt (cgen, "\ttail call fastcc void %%tmp_%d (i8* %%sched, i32* %%tmp_%d) noreturn\n", kpriv->tmpcount + 1, kpriv->tmpcount + 2);
	codegen_write_fmt (cgen, "\tret void\n");
	kpriv->tmpcount += 3;

	codegen_write_fmt (cgen, "}\n\n");

	krocllvm_cgstate_popfree (cgen);
}
/*}}}*/
/*{{{  static void krocllvm_coder_debugline (codegen_t *cgen, tnode_t *node)*/
/*
 *	generates debugging information (e.g. before a process)
 */
static void krocllvm_coder_debugline (codegen_t *cgen, tnode_t *node)
{
	krocllvm_priv_t *kpriv = (krocllvm_priv_t *)cgen->target->priv;

	if (!node->org) {
		/* nothing to generate */
		return;
	}
	if (node->org->org_file != kpriv->lastfile) {
		kpriv->lastfile = node->org->org_file;
		codegen_write_fmt (cgen, "; .sourcefile %s\n", node->org->org_file->filename ?: "(unknown)");
	}
	codegen_write_fmt (cgen, "; .sourceline %d\n", node->org->org_line);

	return;
}
/*}}}*/


/*{{{  static int krocllvm_be_codegen_init (codegen_t *cgen, lexfile_t *srcfile)*/
/*
 *	initialises back-end code generation for KRoC/LLVM target
 */
static int krocllvm_be_codegen_init (codegen_t *cgen, lexfile_t *srcfile)
{
	krocllvm_priv_t *kpriv = (krocllvm_priv_t *)cgen->target->priv;
	coderops_t *cops;
	char hostnamebuf[128];
	char timebuf[128];
	int i;

	/* write header */
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
	memset ((void *)cops, 0, sizeof (coderops_t));

	/* FIXME: need coder operations! */
	cops->ldptr = krocllvm_coder_ldptr;
	cops->ldname = krocllvm_coder_ldname;
	cops->ldconst = krocllvm_coder_ldconst;
	cops->iop = krocllvm_coder_iop;
	cops->stname = krocllvm_coder_stname;
	cops->kicall = krocllvm_coder_kicall;
	cops->freeref = krocllvm_coder_freeref;
	cops->wsadjust = krocllvm_coder_wsadjust;
	cops->getref = krocllvm_coder_getref;

	cops->comment = krocllvm_coder_comment;
	cops->setmemsize = krocllvm_coder_setmemsize;
	cops->setnamelabel = krocllvm_coder_setnamelabel;
	cops->setnamedlabel = krocllvm_coder_setnamedlabel;
	cops->setlabel = krocllvm_coder_setlabel;
	cops->procentry = krocllvm_coder_procentry;
	cops->procnameentry = krocllvm_coder_procnameentry;
	cops->procreturn = krocllvm_coder_procreturn;
	cops->debugline = krocllvm_coder_debugline;

	cgen->cops = cops;

	/*
	 *	create pre-code node if not already here -- constants go at the start
	 */
	if (!kpriv->precodelist) {
		tnode_t *precode = tnode_create (kpriv->tag_PRECODE, NULL, parser_newlistnode (SLOCN (srcfile)),
				*(cgen->cinsertpoint), parser_newlistnode (SLOCN (srcfile)));

		*(cgen->cinsertpoint) = precode;
		kpriv->precodelist = tnode_nthsubof (precode, 0);
		kpriv->postcodelist = tnode_nthsubof (precode, 2);
	}

	if (!compopts.notmainmodule) {
		/* insert JENTRY marker at end-of-tree */
		tnode_t *jentry = tnode_create (kpriv->tag_JENTRY, NULL, NULL);

		parser_addtolist (kpriv->postcodelist, jentry);
	}

	/* additional routines */
	codegen_write_file (cgen, "krocllvm-preamble.ll");

	/* emit kernel-entry-call points */
	for (i=0; kitable[i].call != I_INVALID; i++) {
		int nargs = kitable[i].iregs + kitable[i].oregs;
		int j;

		codegen_write_fmt (cgen, "declare i32* @%s (i8*, i32*", kitable[i].ename);
		for (j=0; j<nargs; j++) {
			codegen_write_fmt (cgen, ", i32");
		}
		codegen_write_fmt (cgen, ")\n");
	}
	codegen_write_fmt (cgen, "\n\n");

	return 0;
}
/*}}}*/
/*{{{  static int krocllvm_be_codegen_final (codegen_t *cgen, lexfile_t *srcfile)*/
/*
 *	shutdown bakc-end code generation for KRoC/LLVM target
 */
static int krocllvm_be_codegen_final (codegen_t *cgen, lexfile_t *srcfile)
{
	krocllvm_priv_t *kpriv = (krocllvm_priv_t *)cgen->target->priv;

	if (kpriv) {
		/*{{{  clean-up local state*/
		if (kpriv->lastfunc) {
			sfree (kpriv->lastfunc);
			kpriv->lastfunc = NULL;
		}
		if (kpriv->tl_mem) {
			krocllvm_memspacehook_free (kpriv->tl_mem);
			kpriv->tl_mem = NULL;
		}
		kpriv->lastdesc = NULL;

		/*}}}*/
	}

	sfree (cgen->cops);
	cgen->cops = NULL;

	codegen_write_string (cgen, "\n;\n;\tend of compilation\n;\n");
	return 0;
}
/*}}}*/


/*{{{  static int krocllvm_target_init (target_t *target)*/
/*
 *	initialises the KRoC-LLVM target
 *	returns 0 on success, non-zero on error
 */
static int krocllvm_target_init (target_t *target)
{
	tndef_t *tnd;
	compops_t *cops;
	langops_t *lops;
	krocllvm_priv_t *kpriv;
	int i;

	if (target->initialised) {
		nocc_internal ("krocllvm_target_init(): already initialised!");
		return 1;
	}
#if 1
fprintf (stderr, "krocllvm_target_init(): initialising!\n");
#endif

	kpriv = (krocllvm_priv_t *)smalloc (sizeof (krocllvm_priv_t));
	kpriv->precodelist = NULL;
	kpriv->postcodelist = NULL;
	kpriv->toplevelname = NULL;
	kpriv->mapchook = tnode_lookupornewchook ("map:mapnames");
	kpriv->resultsubhook = tnode_lookupornewchook ("krocllvm:resultsubhook");

	kpriv->lastfile = NULL;
	kpriv->options.stoperrormode = 0;

	kpriv->regcount = 0;
	kpriv->labcount = 0;
	kpriv->tmpcount = 0;
	kpriv->lastfunc = NULL;
	kpriv->lastdesc = NULL;

	kpriv->tl_mem = NULL;

	target->priv = (void *)kpriv;

	krocllvm_init_options (kpriv);

	/* setup back-end nodes */
	/*{{{  krocllvm:name -- KROCLLVMNAME*/
	i = -1;
	tnd = tnode_newnodetype ("krocllvm:name", &i, 2, 0, 1, TNF_NONE);		/* subnodes: original name, in-scope body; hooks: krocllvm_namehook_t */
	tnd->hook_dumptree = krocllvm_namehook_dumptree;
	cops = tnode_newcompops ();
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "bytesfor", 2, LANGOPTYPE (krocllvm_bytesfor_name));
	tnd->lops = lops;

	i = -1;
	target->tag_NAME = tnode_newnodetag ("KROCLLVMNAME", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  krocllvm_nameref -- KROCLLVMNAMEREF*/
	i = -1;
	tnd = tnode_newnodetype ("krocllvm:nameref", &i, 1, 0, 1, TNF_NONE);
	tnd->hook_dumptree = krocllvm_namehook_dumptree;
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (krocllvm_codegen_nameref));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "gettype", 2, LANGOPTYPE (krocllvm_gettype_nameref));
	tnd->lops = lops;

	i = -1;
	target->tag_NAMEREF = tnode_newnodetag ("KROCLLVMNAMEREF", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  krocllvm:block -- KROCLLVMBLOCK*/
	i = -1;
	tnd = tnode_newnodetype ("krocllvm_block", &i, 2, 0, 1, TNF_NONE);		/* subnodes: block body, statics; hooks: krocllvm_blockhook_t */
	tnd->hook_dumptree = krocllvm_blockhook_dumptree;
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "preallocate", 2, COMPOPTYPE (krocllvm_preallocate_block));
	tnode_setcompop (cops, "precode", 2, COMPOPTYPE (krocllvm_precode_block));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (krocllvm_codegen_block));
	tnd->ops = cops;

	i = -1;
	target->tag_BLOCK = tnode_newnodetag ("KROCLLVMBLOCK", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  krocllvm:const -- KROCLLVMCONST*/
	i = -1;
	tnd = tnode_newnodetype ("krocllvm:const", &i, 1, 0, 1, TNF_NONE);		/* subnodes: original; hooks: krocllvm_consthook_t */
	tnd->hook_dumptree = krocllvm_consthook_dumptree;
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "precode", 2, COMPOPTYPE (krocllvm_precode_const));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (krocllvm_codegen_const));
	tnd->ops = cops;

	i = -1;
	target->tag_CONST = tnode_newnodetag ("KROCLLVMCONST", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  krocllvm:precode -- KROCLLVMPRECODE*/
	i = -1;
	tnd = tnode_newnodetype ("krocllvm:precode", &i, 3, 0, 0, TNF_NONE);			/* nodes: precode-list, body, postcode-list */

	i = -1;
	kpriv->tag_PRECODE = tnode_newnodetag ("KROCLLVMPRECODE", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  krocllvm:special -- KROCLLVMJENTRY, KROCLLVMDESCRIPTOR*/
	i = -1;
	tnd = tnode_newnodetype ("krocllvm:special", &i, 0, 0, 1, TNF_NONE);
	tnd->hook_dumptree = krocllvm_specialhook_dumptree;
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "precode", 2, COMPOPTYPE (krocllvm_precode_special));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (krocllvm_codegen_special));
	tnd->ops = cops;

	i = -1;
	kpriv->tag_JENTRY = tnode_newnodetag ("KROCLLVMJENTRY", &i, tnd, NTF_NONE);
	i = -1;
	kpriv->tag_DESCRIPTOR = tnode_newnodetag ("KROCLLVMDESCRIPTOR", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  krocllvm:constref -- KROCLLVMCONSTREF*/
	i = -1;
	tnd = tnode_newnodetype ("krocllvm:constref", &i, 0, 0, 1, TNF_NONE);			/* hooks: 0 = krocllvm_consthook_t */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (krocllvm_codegen_constref));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "dimtreeof", 1, LANGOPTYPE (krocllvm_dimtreeof_constref));
	tnd->lops = lops;

	i = -1;
	kpriv->tag_CONSTREF = tnode_newnodetag ("KROCLLVMCONSTREF", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  krocllvm:indexed -- KROCLLVMINDEXED*/
	i = -1;
	tnd = tnode_newnodetype ("krocllvm:indexed", &i, 2, 0, 1, TNF_NONE);			/* subnodes: 0 = base, 1 = index;  hooks: 0 = krocllvm_indexedhook_t */
	tnd->hook_dumptree = krocllvm_indexedhook_dumptree;
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (krocllvm_codegen_indexed));
	tnd->ops = cops;

	i = -1;
	target->tag_INDEXED = tnode_newnodetag ("KROCLLVMINDEXED", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  krocllvm:blockref -- KROCLLVMBLOCKREF*/
	i = -1;
	tnd = tnode_newnodetype ("krocllvm:blockref", &i, 1, 0, 1, TNF_NONE);			/* subnodes: body; hooks: krocllvm_blockrefhook_t */
	tnd->hook_dumptree = krocllvm_blockrefhook_dumptree;

	i = -1;
	target->tag_BLOCKREF = tnode_newnodetag ("KROCLLVMBLOCKREF", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  krocllvm:staticlink -- KROCLLVMSTATICLINK*/
	i = -1;
	tnd = tnode_newnodetype ("krocllvm:staticlink", &i, 0, 0, 0, TNF_NONE);

	i = -1;
	target->tag_STATICLINK = tnode_newnodetag ("KROCLLVMSTATICLINK", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  krocllvm:hiddenparam -- KROCLLVMFBP*/
	i = -1;
	tnd = tnode_newnodetype ("krocllvm:hiddenparam", &i, 0, 0, 0, TNF_NONE);

	i = -1;
	kpriv->tag_FBP = tnode_newnodetag ("KROCLLVMFBP", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  krocllvm:result -- KROCLLVMRESULT*/
	i = -1;
	tnd = tnode_newnodetype ("krocllvm:result", &i, 1, 0, 0, TNF_NONE);
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (krocllvm_namemap_result));
	tnode_setcompop (cops, "bemap", 2, COMPOPTYPE (krocllvm_bemap_result));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (krocllvm_codegen_result));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	lops->passthrough = 1;
	tnd->lops = lops;

	i = -1;
	target->tag_RESULT = tnode_newnodetag ("KROCLLVMRESULT", &i, tnd, NTF_NONE);

	/*}}}*/

	target->initialised = 1;
	return 0;
}
/*}}}*/

