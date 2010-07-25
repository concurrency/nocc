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
typedef struct TAG_krocllvm_priv {
	ntdef_t *tag_PRECODE;
	ntdef_t *tag_JENTRY;
	ntdef_t *tag_DESCRIPTOR;
	ntdef_t *tag_VSP;			/* vectorspace pointer */
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

	/* FIXME: initialise private state */

	target->initialised = 1;
	return 0;
}
/*}}}*/

