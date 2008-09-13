/*
 *	kroccifccsp.c -- KRoC/CIF/CCSP back-end
 *	Copyright (C) 2008 Fred Barnes <frmb@kent.ac.uk>
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
static int kroccifccsp_target_init (target_t *target);

static void kroccifccsp_do_betrans (tnode_t **tptr, betrans_t *be);
static void kroccifccsp_do_premap (tnode_t **tptr, map_t *map);
static void kroccifccsp_do_namemap (tnode_t **tptr, map_t *map);
static void kroccifccsp_do_bemap (tnode_t **tptr, map_t *map);
static void kroccifccsp_do_preallocate (tnode_t *tptr, target_t *target);
static void kroccifccsp_do_precode (tnode_t **tptr, codegen_t *cgen);
static void kroccifccsp_do_codegen (tnode_t *tptr, codegen_t *cgen);

static int kroccifccsp_be_codegen_init (codegen_t *cgen, lexfile_t *srcfile);
static int kroccifccsp_be_codegen_final (codegen_t *cgen, lexfile_t *srcfile);


/*}}}*/
/*{{{  private data*/

static chook_t *codegeninithook = NULL;
static chook_t *codegenfinalhook = NULL;


/*}}}*/

/*{{{  target_t for this target*/
target_t kroccifccsp_target = {
	initialised:	0,
	name:		"kroccifccsp",
	tarch:		"cifccsp",
	tvendor:	"kroc",
	tos:		NULL,
	desc:		"KRoC CIF/CCSP C-code",
	extn:		"c",
	tcap: {
		can_do_fp: 1,
		can_do_dmem: 1,
	},
	bws: {
		ds_min: 0,
		ds_io: 0,
		ds_altio: 0,
		ds_wait: 24,
		ds_max: 24
	},
	aws: {
		as_alt: 4,
		as_par: 12,
	},

	chansize:	0,
	charsize:	0,
	intsize:	0,
	pointersize:	0,
	slotsize:	0,
	structalign:	0,
	maxfuncreturn:	0,

	tag_NAME:	NULL,
	tag_NAMEREF:	NULL,
	tag_BLOCK:	NULL,
	tag_CONST:	NULL,
	tag_INDEXED:	NULL,
	tag_BLOCKREF:	NULL,
	tag_STATICLINK:	NULL,
	tag_RESULT:	NULL,

	init:		kroccifccsp_target_init,
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
	be_codegen_init:	kroccifccsp_be_codegen_init,
	be_codegen_final:	kroccifccsp_be_codegen_final,

	be_precode_seenproc:	NULL,

	be_do_betrans:		kroccifccsp_do_betrans,
	be_do_premap:		kroccifccsp_do_premap,
	be_do_namemap:		kroccifccsp_do_namemap,
	be_do_bemap:		kroccifccsp_do_bemap,
	be_do_preallocate:	kroccifccsp_do_preallocate,
	be_do_precode:		kroccifccsp_do_precode,
	be_do_codegen:		kroccifccsp_do_codegen,

	priv:		NULL
};

/*}}}*/


/*{{{  static int kroccifccsp_prewalktree_codegen (tnode_t *node, void *data)*/
/*
 *	prewalktree for code generation, calls comp-ops "lcodegen" routine where present
 *	returns 0 to stop walk, 1 to continue
 */
static int kroccifccsp_prewalktree_codegen (tnode_t *node, void *data)
{
	codegen_t *cgen = (codegen_t *)data;
	codegeninithook_t *cgih = (codegeninithook_t *)tnode_getchook (node, codegeninithook);
	codegenfinalhook_t *cgfh = (codegenfinalhook_t *)tnode_getchook (node, codegenfinalhook);
	int i = 1;

	/*{{{  do initialisers*/
	while (cgih) {
		if (cgih->init) {
			cgih->init (node, cgen, cgih->arg);
		}
		cgih = cgih->next;
	}
	/*}}}*/

	if (node->tag->ndef->ops && tnode_hascompop_i (node->tag->ndef->ops, (int)COPS_LCODEGEN)) {
		i = tnode_callcompop_i (node->tag->ndef->ops, (int)COPS_LCODEGEN, 2, node, cgen);
	}

	/*{{{  if finalisers, do subnodes then finalisers*/
	if (cgfh) {
		int nsnodes, j;
		tnode_t **snodes = tnode_subnodesof (node, &nsnodes);

		for (j=0; j<nsnodes; j++) {
			tnode_prewalktree (snodes[j], kroccifccsp_prewalktree_codegen, (void *)cgen);
		}

		i = 0;
		while (cgfh) {
			if (cgfh->final) {
				cgfh->final (node, cgen, cgfh->arg);
			}
			cgfh = cgfh->next;
		}
	}
	/*}}}*/

	return i;
}

/*}}}*/


/*{{{  static void kroccifccsp_do_betrans (tnode_t **tptr, betrans_t *be)*/
/*
 *	intercepts back-end transform pass
 */
static void kroccifccsp_do_betrans (tnode_t **tptr, betrans_t *be)
{
	nocc_message ("kroccifccsp_do_betrans(): here!");
	return;
}
/*}}}*/
/*{{{  static void kroccifccsp_do_premap (tnode_t **tptr, map_t *map)*/
/*
 *	intercepts pre-map pass
 */
static void kroccifccsp_do_premap (tnode_t **tptr, map_t *map)
{
	nocc_message ("kroccifccsp_do_premap(): here!");
	return;
}
/*}}}*/
/*{{{  static void kroccifccsp_do_namemap (tnode_t **tptr, map_t *map)*/
/*
 *	intercepts name-map pass
 */
static void kroccifccsp_do_namemap (tnode_t **tptr, map_t *map)
{
	nocc_message ("kroccifccsp_do_namemap(): here!");
	return;
}
/*}}}*/
/*{{{  static void kroccifccsp_do_bemap (tnode_t **tptr, map_t *map)*/
/*
 *	intercepts be-map pass
 */
static void kroccifccsp_do_bemap (tnode_t **tptr, map_t *map)
{
	nocc_message ("kroccifccsp_do_bemap(): here!");
	return;
}
/*}}}*/
/*{{{  static void kroccifccsp_do_preallocate (tnode_t *tptr, target_t *target)*/
/*
 *	intercepts pre-allocate pass
 */
static void kroccifccsp_do_preallocate (tnode_t *tptr, target_t *target)
{
	nocc_message ("kroccifccsp_do_preallocate(): here!");
	return;
}
/*}}}*/
/*{{{  static void kroccifccsp_do_precode (tnode_t **tptr, codegen_t *cgen)*/
/*
 *	intercepts pre-code pass
 */
static void kroccifccsp_do_precode (tnode_t **tptr, codegen_t *cgen)
{
	nocc_message ("kroccifccsp_do_precode(): here!");
	return;
}
/*}}}*/
/*{{{  static void kroccifccsp_do_codegen (tnode_t *tptr, codegen_t *cgen)*/
/*
 *	intercepts code-gen pass
 */
static void kroccifccsp_do_codegen (tnode_t *tptr, codegen_t *cgen)
{
	tnode_prewalktree (tptr, kroccifccsp_prewalktree_codegen, (void *)cgen);
	return;
}
/*}}}*/


/*{{{  static void kroccifccsp_coder_comment (codegen_t *cgen, const char *fmt, ...)*/
/*
 *	generates a comment
 */
static void kroccifccsp_coder_comment (codegen_t *cgen, const char *fmt, ...)
{
	char *buf = (char *)smalloc (1024);
	va_list ap;
	int i;

	va_start (ap, fmt);
	strcpy (buf, "/* ");
	i = vsnprintf (buf + 3, 1016, fmt, ap);
	va_end (ap);

	if (i > 0) {
		i += 3;
		strcpy (buf + i, " */\n");
		codegen_write_bytes (cgen, buf, i + 4);
	}

	sfree (buf);
	return;
}
/*}}}*/


/*{{{  static int kroccifccsp_be_codegen_init (codegen_t *cgen, lexfile_t *srcfile)*/
/*
 *	initialises back-end code generation for KRoC CIF/CCSP target
 *	returns 0 on success, non-zero on failure
 */
static int kroccifccsp_be_codegen_init (codegen_t *cgen, lexfile_t *srcfile)
{
	char hostnamebuf[128];
	char timebuf[128];
	coderops_t *cops;

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
	cops->comment = kroccifccsp_coder_comment;

	cgen->cops = cops;

	return 0;
}
/*}}}*/
/*{{{  static int kroccifccsp_be_codegen_final (codegen_t *cgen, lexfile_t *srcfile)*/
/*
 *	shutdown back-end code generation for KRoC CIF/CCSP target
 *	returns 0 on success, non-zero on failure
 */
static int kroccifccsp_be_codegen_final (codegen_t *cgen, lexfile_t *srcfile)
{
	sfree (cgen->cops);
	cgen->cops = NULL;

	codegen_write_fmt (cgen, "/*\n *\tend of code generation\n */\n\n");

	return 0;
}
/*}}}*/


/*{{{  int kroccifccsp_init (void)*/
/*
 *	initialises the KRoC CIF/CCSP back-end
 *	returns 0 on success, non-zero on error
 */
int kroccifccsp_init (void)
{
	/* register the target */
	if (target_register (&kroccifccsp_target)) {
		nocc_error ("kroccifccsp_init(): failed to register target!");
		return 1;
	}

	/* setup local stuff */
	codegeninithook = codegen_getcodegeninithook ();
	codegenfinalhook = codegen_getcodegenfinalhook ();

	return 0;
}
/*}}}*/
/*{{{  int kroccifccsp_shutdown (void)*/
/*
 *	shuts down the KRoC CIF/CCSP back-end
 *	returns 0 on success, non-zero on error
 */
int kroccifccsp_shutdown (void)
{
	/* unregister the target */
	if (target_unregister (&kroccifccsp_target)) {
		nocc_error ("kroccifccsp_shutdown(): failed to unregister target!");
		return 1;
	}

	return 0;
}
/*}}}*/


/*{{{  static int kroccifccsp_target_init (target_t *target)*/
/*
 *	initialises the KRoC CIF/CCSP target
 *	returns 0 on success, non-zero on error
 */
static int kroccifccsp_target_init (target_t *target)
{
	if (target->initialised) {
		nocc_internal ("kroccifccsp_target_init(): already initialised!");
		return 1;
	}

	target->initialised = 1;
	return 0;
}
/*}}}*/

