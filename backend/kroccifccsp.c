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

static tnode_t *kroccifccsp_name_create (tnode_t *fename, tnode_t *body, map_t *mdata, int asize_wsh, int asize_wsl, int asize_vs, int asize_ms, int tsize, int ind);
static tnode_t *kroccifccsp_nameref_create (tnode_t *bename, map_t *mdata);
static tnode_t *kroccifccsp_block_create (tnode_t *body, map_t *mdata, tnode_t *slist, int lexlevel);
static tnode_t *kroccifccsp_blockref_create (tnode_t *bloc, tnode_t *body, map_t *mdata);


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
	skipallocate:	1,

	tag_NAME:	NULL,
	tag_NAMEREF:	NULL,
	tag_BLOCK:	NULL,
	tag_CONST:	NULL,
	tag_INDEXED:	NULL,
	tag_BLOCKREF:	NULL,
	tag_STATICLINK:	NULL,
	tag_RESULT:	NULL,

	init:		kroccifccsp_target_init,
	newname:	kroccifccsp_name_create,
	newnameref:	kroccifccsp_nameref_create,
	newblock:	kroccifccsp_block_create,
	newconst:	NULL,
	newindexed:	NULL,
	newblockref:	kroccifccsp_blockref_create,
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
/*{{{  private types*/
typedef struct TAG_kroccifccsp_priv {
	lexfile_t *lastfile;
} kroccifccsp_priv_t;

typedef struct TAG_kroccifccsp_namehook {
	char *cname;		/* low-level variable name */
	int lexlevel;		/* lexical level */
	int alloc_wsh;		/* allocation in high-workspace */
	int alloc_wsl;		/* allocation in low-workspace */
	int alloc_vs;		/* allocation in vectorspace */
	int alloc_ms;		/* allocation in mobilespace */
	int typesize;		/* size of the actual type (if known) */
	int indir;		/* indirection count (0 = real-thing, 1 = pointer, 2 = pointer-pointer, etc.) */
	typecat_e typecat;	/* type category */
} kroccifccsp_namehook_t;

typedef struct TAG_kroccifccs_namerefhook {
	tnode_t *nnode;				/* underlying back-end name-nodE */
	kroccifccsp_namehook_t *nhook;		/* underlying name-hook */
} kroccifccsp_namerefhook_t;

typedef struct TAG_kroccifccsp_blockhook {
	int lexlevel;				/* lexical level of this block */
} kroccifccsp_blockhook_t;

typedef struct TAG_kroccifccsp_blockrefhook {
	tnode_t *block;
} kroccifccsp_blockrefhook_t;


/*}}}*/
/*{{{  private data*/

static chook_t *codegeninithook = NULL;
static chook_t *codegenfinalhook = NULL;

static chook_t *kroccifccsp_ctypestr = NULL;


/*}}}*/


/*{{{  void kroccifccsp_isetindent (FILE *stream, int indent)*/
/*
 *	set-indent for debugging output
 */
void kroccifccsp_isetindent (FILE *stream, int indent)
{
	int i;

	for (i=0; i<indent; i++) {
		fprintf (stream, "    ");
	}
	return;
}
/*}}}*/

/*{{{  static int kroccifccsp_init_options (kroccifccsp_priv_t *kpriv)*/
/*
 *	initialises options for the KRoC-CIF/CCSP back-end
 *	returns 0 on success, non-zero on failure
 */
static int kroccifccsp_init_options (kroccifccsp_priv_t *kpriv)
{
	// opts_add ("norangechecks", '\0', kroccifccsp_opthandler_flag, (void *)1, "1do not generate range-checks");
	return 0;
}
/*}}}*/

/*{{{  kroccifccsp_namehook_t routines*/
/*{{{  static void kroccifccsp_namehook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	dumps hook data for debugging
 */
static void kroccifccsp_namehook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	kroccifccsp_namehook_t *nh = (kroccifccsp_namehook_t *)hook;

	kroccifccsp_isetindent (stream, indent);
	fprintf (stream, "<namehook addr=\"0x%8.8x\" cname=\"%s\" lexlevel=\"%d\" allocwsh=\"%d\" allocwsl=\"%d\" allocvs=\"%d\" allocms=\"%d\" typesize=\"%d\" indir=\"%d\" typecat=\"0x%8.8x\" />\n",
			(unsigned int)nh, nh->cname, nh->lexlevel, nh->alloc_wsh, nh->alloc_wsl, nh->alloc_vs, nh->alloc_ms,
			nh->typesize, nh->indir, (unsigned int)nh->typecat);
	return;
}
/*}}}*/
/*{{{  static kroccifccsp_namehook_t *kroccifccsp_namehook_create (char *cname, int ll, int asize_wsh, int asize_wsl, int asize_vs, int asize_ms, int tsize, int ind)*/
/*
 *	creates a name-hook
 */
static kroccifccsp_namehook_t *kroccifccsp_namehook_create (char *cname, int ll, int asize_wsh, int asize_wsl, int asize_vs, int asize_ms, int tsize, int ind)
{
	kroccifccsp_namehook_t *nh = (kroccifccsp_namehook_t *)smalloc (sizeof (kroccifccsp_namehook_t));

	nh->cname = cname;
	nh->lexlevel = ll;
	nh->alloc_wsh = asize_wsh;
	nh->alloc_wsl = asize_wsl;
	nh->alloc_vs = asize_vs;
	nh->alloc_ms = asize_ms;
	nh->typesize = tsize;
	nh->indir = ind;
	nh->typecat = TYPE_NOTTYPE;

	return nh;
}
/*}}}*/
/*}}}*/
/*{{{  kroccifccsp_namerefhook_t routines*/
/*{{{  static void kroccifccsp_namerefhook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	dumps hook data for debugging
 */
static void kroccifccsp_namerefhook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	kroccifccsp_namerefhook_t *nh = (kroccifccsp_namerefhook_t *)hook;

	kroccifccsp_isetindent (stream, indent);
	fprintf (stream, "<namerefhook addr=\"0x%8.8x\" nnode=\"0x%8.8x\" nhook=\"0x%8.8x\" cname=\"%s\" />\n",
			(unsigned int)nh, (unsigned int)nh->nnode, (unsigned int)nh->nhook, (nh->nhook ? nh->nhook->cname : ""));
	return;
}
/*}}}*/
/*{{{  static kroccifccsp_namerefhook_t *kroccifccsp_namerefhook_create (tnode_t *nnode, kroccifccsp_namehook_t *nhook)*/
/*
 *	creates a name-ref-hook
 */
static kroccifccsp_namerefhook_t *kroccifccsp_namerefhook_create (tnode_t *nnode, kroccifccsp_namehook_t *nhook)
{
	kroccifccsp_namerefhook_t *nh = (kroccifccsp_namerefhook_t *)smalloc (sizeof (kroccifccsp_namerefhook_t));

	nh->nnode = nnode;
	nh->nhook = nhook;

	return nh;
}
/*}}}*/
/*}}}*/
/*{{{  kroccifccsp_blockhook_t routines*/
/*{{{  static void kroccifccsp_blockhook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	dumps hook for debugging
 */
static void kroccifccsp_blockhook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	kroccifccsp_blockhook_t *bh = (kroccifccsp_blockhook_t *)hook;

	kroccifccsp_isetindent (stream, indent);
	fprintf (stream, "<blockhook addr=\"0x%8.8x\" lexlevel=\"%d\" />\n",
			(unsigned int)bh, bh->lexlevel);
	return;
}
/*}}}*/
/*{{{  static kroccifccsp_blockhook_t *kroccifccsp_blockhook_create (int ll)*/
/*
 *	creates a block-hook
 */
static kroccifccsp_blockhook_t *kroccifccsp_blockhook_create (int ll)
{
	kroccifccsp_blockhook_t *bh = (kroccifccsp_blockhook_t *)smalloc (sizeof (kroccifccsp_blockhook_t));

	bh->lexlevel = ll;

	return bh;
}
/*}}}*/
/*}}}*/
/*{{{  kroccifccsp_blockrefhook_t routines*/
/*{{{  static void kroccifccsp_blockrefhook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	dumps hook (debugging)
 */
static void kroccifccsp_blockrefhook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	kroccifccsp_blockrefhook_t *brh = (kroccifccsp_blockrefhook_t *)hook;
	tnode_t *blk = brh->block;

	if (blk && parser_islistnode (blk)) {
		int nitems, i;
		tnode_t **blks = parser_getlistitems (blk, &nitems);

		kroccifccsp_isetindent (stream, indent);
		fprintf (stream, "<blockrefhook addr=\"0x%8.8x\" block=\"0x%8.8x\" nblocks=\"%d\" blocks=\"", (unsigned int)brh, (unsigned int)blk, nitems);
		for (i=0; i<nitems; i++ ) {
			if (i) {
				fprintf (stream, ",");
			}
			fprintf (stream, "0x%8.8x", (unsigned int)blks[i]);
		}
		fprintf (stream, "\" />\n");
	} else {
		kroccifccsp_isetindent (stream, indent);
		fprintf (stream, "<blockrefhook addr=\"0x%8.8x\" block=\"0x%8.8x\" />\n", (unsigned int)brh, (unsigned int)blk);
	}

	return;
}
/*}}}*/
/*{{{  static kroccifccsp_blockrefhook_t *kroccifccsp_blockrefhook_create (tnode_t *block)*/
/*
 *	creates a new hook (populated)
 */
static kroccifccsp_blockrefhook_t *kroccifccsp_blockrefhook_create (tnode_t *block)
{
	kroccifccsp_blockrefhook_t *brh = (kroccifccsp_blockrefhook_t *)smalloc (sizeof (kroccifccsp_blockrefhook_t));

	brh->block = block;

	return brh;
}
/*}}}*/
/*}}}*/


/*{{{  static tnode_t *kroccifccsp_name_create (tnode_t *fename, tnode_t *body, map_t *mdata, int asize_wsh, int asize_wsl, int asize_vs, int asize_ms, int tsize, int ind)*/
/*
 *	creates a new back-end name-node
 */
static tnode_t *kroccifccsp_name_create (tnode_t *fename, tnode_t *body, map_t *mdata, int asize_wsh, int asize_wsl, int asize_vs, int asize_ms, int tsize, int ind)
{
	target_t *xt = mdata->target;		/* must be us! */
	tnode_t *name;
	kroccifccsp_namehook_t *nh;
	char *cname = NULL;

	langops_getname (fename, &cname);
	if (!cname) {
		cname = string_dup ("unknown");
	}
	nh = kroccifccsp_namehook_create (cname, mdata->lexlevel, asize_wsh, asize_wsl, asize_vs, asize_ms, tsize, ind);
	name = tnode_create (xt->tag_NAME, NULL, fename, body, (void *)nh);

	return name;
}
/*}}}*/
/*{{{  static tnode_t *kroccifccsp_nameref_create (tnode_t *bename, map_t *mdata)*/
/*
 *	creates a new back-end name-reference node
 */
static tnode_t *kroccifccsp_nameref_create (tnode_t *bename, map_t *mdata)
{
	kroccifccsp_namerefhook_t *nh;
	kroccifccsp_namehook_t *be_nh;
	tnode_t *name, *fename;

	be_nh = (kroccifccsp_namehook_t *)tnode_nthhookof (bename, 0);
	nh = kroccifccsp_namerefhook_create (bename, be_nh);

	fename = tnode_nthsubof (bename, 0);
	name = tnode_create (mdata->target->tag_NAMEREF, NULL, fename, (void *)nh);

	return name;
}
/*}}}*/
/*{{{  static tnode_t *kroccifccsp_block_create (tnode_t *body, map_t *mdata, tnode_t *slist, int lexlevel)*/
/*
 *	creates a new back-end block
 */
static tnode_t *kroccifccsp_block_create (tnode_t *body, map_t *mdata, tnode_t *slist, int lexlevel)
{
	kroccifccsp_blockhook_t *bh;
	tnode_t *blk;

	bh = kroccifccsp_blockhook_create (lexlevel);
	blk = tnode_create (mdata->target->tag_BLOCK, NULL, body, slist, (void *)bh);

	return blk;
}
/*}}}*/
/*{{{  static tnode_t *kroccifccsp_blockref_create (tnode_t *block, tnode_t *body, map_t *mdata)*/
/*
 *	creates a new back-end block reference node (used for procedure instances and the like)
 */
static tnode_t *kroccifccsp_blockref_create (tnode_t *block, tnode_t *body, map_t *mdata)
{
	kroccifccsp_blockrefhook_t *brh = kroccifccsp_blockrefhook_create (block);
	tnode_t *blockref;

	blockref = tnode_create (kroccifccsp_target.tag_BLOCKREF, NULL, body, (void *)brh);

	return blockref;
}
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
/*{{{  static int kroccifccsp_modprewalktree_namemap (tnode_t **nodep, void *data)*/
/*
 *	modprewalktree for name-mapping, calls comp-ops "lnamemap" routine where present
 *	returns 0 to stop walk, 1 to continue
 */
static int kroccifccsp_modprewalktree_namemap (tnode_t **nodep, void *data)
{
	map_t *map = (map_t *)data;
	int i = 1;

	if ((*nodep)->tag->ndef->ops && tnode_hascompop_i ((*nodep)->tag->ndef->ops, (int)COPS_LNAMEMAP)) {
		i = tnode_callcompop_i ((*nodep)->tag->ndef->ops, (int)COPS_LNAMEMAP, 2, nodep, map);
	}

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
	tnode_modprewalktree (tptr, kroccifccsp_modprewalktree_namemap, (void *)map);
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
/*{{{  static void kroccifccsp_coder_debugline (codegen_t *cgen, tnode_t *node)*/
/*
 *	generates debugging information (e.g. before a process)
 */
static void kroccifccsp_coder_debugline (codegen_t *cgen, tnode_t *node)
{
	kroccifccsp_priv_t *kpriv = (kroccifccsp_priv_t *)cgen->target->priv;

#if 0
	nocc_message ("kroccifccsp_coder_debugline(): [%s], line %d", node->tag->name, node->org_line);
#endif
	if (!node->org_file || !node->org_line) {
		/* nothing to generate */
		return;
	}
	if (node->org_file != kpriv->lastfile) {
		kpriv->lastfile = node->org_file;
		codegen_write_fmt (cgen, "#FILE %s\n", node->org_file->filename ?: "(unknown)");
	}
	codegen_write_fmt (cgen, "#LINE %d\n", node->org_line);

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
	cops->debugline = kroccifccsp_coder_debugline;

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


/*{{{  static int kroccifccsp_lcodegen_block (compops_t *cops, tnode_t *blk, codegen_t *cgen)*/
/*
 *	does code-generation for a back-end block
 *	returns 0 to stop walk, 1 to continue
 */
static int kroccifccsp_lcodegen_block (compops_t *cops, tnode_t *blk, codegen_t *cgen)
{
	kroccifccsp_priv_t *kpriv = (kroccifccsp_priv_t *)cgen->target->priv;

	codegen_write_fmt (cgen, "{\n");
	codegen_subcodegen (tnode_nthsubof (blk, 0), cgen);
	codegen_write_fmt (cgen, "}\n");

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
	tndef_t *tnd;
	kroccifccsp_priv_t *kpriv;
	compops_t *cops;
	langops_t *lops;
	int i;

	if (target->initialised) {
		nocc_internal ("kroccifccsp_target_init(): already initialised!");
		return 1;
	}

	kpriv = (kroccifccsp_priv_t *)smalloc (sizeof (kroccifccsp_priv_t));
	kpriv->lastfile = NULL;
	target->priv = (void *)kpriv;

	kroccifccsp_init_options (kpriv);

	/* setup back-end nodes */
	/*{{{  kroccifccsp:name -- KROCCIFCCSPNAME*/
	i = -1;
	tnd = tnode_newnodetype ("kroccifccsp:name", &i, 2, 0, 1, TNF_NONE);		/* subnodes: original name, in-scope body; hooks: kroccifccsp_namehook_t */
	tnd->hook_dumptree = kroccifccsp_namehook_dumptree;
	cops = tnode_newcompops ();
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnd->lops = lops;

	i = -1;
	target->tag_NAME = tnode_newnodetag ("KROCCIFCCSPNAME", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  kroccifccsp:nameref -- KROCCIFCCSPNAMEREF*/
	i = -1;
	tnd = tnode_newnodetype ("kroccifccsp:nameref", &i, 1, 0, 1, TNF_NONE);		/* subnodes: original name; hooks: kroccifccsp_namerefhook_t */
	tnd->hook_dumptree = kroccifccsp_namerefhook_dumptree;
	cops = tnode_newcompops ();
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnd->lops = lops;

	i = -1;
	target->tag_NAMEREF = tnode_newnodetag ("KROCCIFCCSPNAMEREF", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  kroccifccsp:block -- KROCCIFCCSPBLOCK*/
	i = -1;
	tnd = tnode_newnodetype ("kroccifccsp:block", &i, 2, 0, 1, TNF_NONE);		/* subnodes: block body, statics; hooks: kroccifccsp_blockhook_t */
	tnd->hook_dumptree = kroccifccsp_blockhook_dumptree;
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "lcodegen", 2, COMPOPTYPE (kroccifccsp_lcodegen_block));
	tnd->ops = cops;

	i = -1;
	target->tag_BLOCK = tnode_newnodetag ("KROCCIFCCSPBLOCK", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  kroccifccsp:blockref -- KROCCIFCCSPBLOCKREF*/
	i = -1;
	tnd = tnode_newnodetype ("kroccifccsp_blockref", &i, 1, 0, 1, TNF_NONE);	/* subnodes: body; hooks: kroccifccsp_blockrefhook_t */
	tnd->hook_dumptree = kroccifccsp_blockrefhook_dumptree;

	i = -1;
	target->tag_BLOCKREF = tnode_newnodetag ("KROCCIFCCSPBLOCKREF", &i, tnd, NTF_NONE);

	/*}}}*/

	target->initialised = 1;
	return 0;
}
/*}}}*/

