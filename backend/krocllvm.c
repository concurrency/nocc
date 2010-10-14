/*
 *	krocllvm.c -- KRoC/LLVM back-end
 *	Copyright (C) 2010 Fred Barnes <frmb@kent.ac.uk>
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
#include "transputer.h"
#include "codegen.h"
#include "allocate.h"

/*}}}*/
/*{{{  forward decls*/
static int krocllvm_target_init (target_t *target);

static int krocllvm_be_codegen_init (codegen_t *cgen, lexfile_t *srcfile);
static int krocllvm_be_codegen_final (codegen_t *cgen, lexfile_t *srcfile);

/*}}}*/

/*{{{  target_t for this target*/
target_t krocllvm_target = {
	initialised:	0,
	name:		"krocllvm",
	tarch:		"llvm",
	tvendor:	"kroc",
	tos:		NULL,
	desc:		"KRoC LLVM",
	extn:		"ll",
	tcap: {
		can_do_fp: 1,
		can_do_dmem: 1,
	},
	bws: {
		ds_min: 12,
		ds_io: 16,
		ds_altio: 16,
		ds_wait: 24,
		ds_max: 24
	},
	aws: {
		as_alt: 4,
		as_par: 12,
	},

	chansize:	4,
	charsize:	1,
	intsize:	4,
	pointersize:	4,
	slotsize:	4,
	structalign:	4,
	maxfuncreturn:	0,
	skipallocate:	0,

	tag_NAME:	NULL,
	tag_NAMEREF:	NULL,
	tag_BLOCK:	NULL,
	tag_CONST:	NULL,
	tag_INDEXED:	NULL,
	tag_BLOCKREF:	NULL,
	tag_STATICLINK:	NULL,
	tag_RESULT:	NULL,

	init:		krocllvm_target_init,
	newname:	NULL,
	newnameref:	NULL,
	newblock:	NULL,
	newconst:	NULL,
	newindexed:	NULL,
	newblockref:	NULL,
	newresult:	NULL,
	inresult:	NULL,

	be_getorgnode:		NULL,
	be_blockbodyaddr:	NULL,
	be_allocsize:		NULL,
	be_typesize:		NULL,
	be_settypecat:		NULL,
	be_gettypecat:		NULL,
	be_setoffsets:		NULL,
	be_getoffsets:		NULL,
	be_blocklexlevel:	NULL,
	be_setblocksize:	NULL,
	be_getblocksize:	NULL,
	be_codegen_init:	krocllvm_be_codegen_init,
	be_codegen_final:	krocllvm_be_codegen_final,

	be_precode_seenproc:	NULL,

	be_do_betrans:		NULL,
	be_do_premap:		NULL,
	be_do_namemap:		NULL,
	be_do_bemap:		NULL,
	be_do_preallocate:	NULL,
	be_do_precode:		NULL,
	be_do_codegen:		NULL,

	priv:		NULL
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

typedef struct TAG_krocllvm_priv {
	ntdef_t *tag_PRECODE;
	ntdef_t *tag_CONSTREF;
	ntdef_t *tag_JENTRY;
	ntdef_t *tag_DESCRIPTOR;
	ntdef_t *tag_FBP;			/* fork-barrier pointer */
	tnode_t *precodelist;
	name_t *toplevelname;

	chook_t *mapchook;
	chook_t *resultsubhook;

	lexfile_t *lastfile;

	struct {
		unsigned int stoperrormode : 1;
	} options;
} krocllvm_priv_t;

typedef struct TAG_krocllvm_resultsubhook {
	int result_regs;
	int result_fregs;
	DYNARRAY (tnode_t **, sublist);
} krocllvm_resultsubhook_t;

typedef struct TAG_krocllvm_cgstate {
	int depth;				/* register depth */
	int *iregs;				/* integer registers */
	int *fregs;				/* floating-point registers */
} krocllvm_cgstate_t;

/*}}}*/


/*{{{  void krocllvm_isetindent (FILE *stream, int indent)*/
/*
 *	set indent for debuggint output
 */
void krocllvm_isetindent (FILE *stream, int indent)
{
	int i;

	for (i=0; i<indent; i++) {
		fprintf (stream, "    ");
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
	cops->comment = krocllvm_coder_comment;

	cgen->cops = cops;

	return 0;
}
/*}}}*/
/*{{{  static int krocllvm_be_codegen_final (codegen_t *cgen, lexfile_t *srcfile)*/
/*
 *	shutdown bakc-end code generation for KRoC/LLVM target
 */
static int krocllvm_be_codegen_final (codegen_t *cgen, lexfile_t *srcfile)
{
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

	kpriv = (krocllvm_priv_t *)smalloc (sizeof (krocllvm_priv_t));
	kpriv->precodelist = NULL;
	kpriv->toplevelname = NULL;
	kpriv->mapchook = tnode_lookupornewchook ("map:mapnames");
	kpriv->resultsubhook = tnode_lookupornewchook ("krocllvm:resultsubhook");

	kpriv->lastfile = NULL;
	kpriv->options.stoperrormode = 0;

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
	tnd = tnode_newnodetype ("krocllvm:precode", &i, 2, 0, 0, TNF_NONE);

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

