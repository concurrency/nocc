/*
 *	cccsp.c -- KRoC/CIF/CCSP back-end
 *	Copyright (C) 2008-2011 Fred Barnes <frmb@kent.ac.uk>
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
static int cccsp_target_init (target_t *target);

static void cccsp_do_betrans (tnode_t **tptr, betrans_t *be);
static void cccsp_do_premap (tnode_t **tptr, map_t *map);
static void cccsp_do_namemap (tnode_t **tptr, map_t *map);
static void cccsp_do_bemap (tnode_t **tptr, map_t *map);
static void cccsp_do_preallocate (tnode_t *tptr, target_t *target);
static void cccsp_do_precode (tnode_t **tptr, codegen_t *cgen);
static void cccsp_do_codegen (tnode_t *tptr, codegen_t *cgen);

static int cccsp_be_codegen_init (codegen_t *cgen, lexfile_t *srcfile);
static int cccsp_be_codegen_final (codegen_t *cgen, lexfile_t *srcfile);

static tnode_t *cccsp_name_create (tnode_t *fename, tnode_t *body, map_t *mdata, int asize_wsh, int asize_wsl, int asize_vs, int asize_ms, int tsize, int ind);
static tnode_t *cccsp_nameref_create (tnode_t *bename, map_t *mdata);
static tnode_t *cccsp_block_create (tnode_t *body, map_t *mdata, tnode_t *slist, int lexlevel);
static tnode_t *cccsp_blockref_create (tnode_t *bloc, tnode_t *body, map_t *mdata);


/*}}}*/

/*{{{  target_t for this target*/
target_t cccsp_target = {
	.initialised =		0,
	.name =			"cccsp",
	.tarch =		"c",
	.tvendor =		"ccsp",
	.tos =			NULL,
	.desc =			"CCSP C code",
	.extn =			"c",
	.tcap = {
		.can_do_fp = 1,
		.can_do_dmem = 1,
	},
	.bws = {
		.ds_min = 0,
		.ds_io = 0,
		.ds_altio = 0,
		.ds_wait = 24,
		.ds_max = 24
	},
	.aws = {
		.as_alt = 4,
		.as_par = 12,
	},

	.chansize =		0,
	.charsize =		0,
	.intsize =		0,
	.pointersize =		0,
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

	.init =			cccsp_target_init,
	.newname =		cccsp_name_create,
	.newnameref =		cccsp_nameref_create,
	.newblock =		cccsp_block_create,
	.newconst =		NULL,
	.newindexed =		NULL,
	.newblockref =		cccsp_blockref_create,
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
	.be_codegen_init =	cccsp_be_codegen_init,
	.be_codegen_final =	cccsp_be_codegen_final,

	.be_precode_seenproc =	NULL,

	.be_do_betrans =	cccsp_do_betrans,
	.be_do_premap =		cccsp_do_premap,
	.be_do_namemap =	cccsp_do_namemap,
	.be_do_bemap =		cccsp_do_bemap,
	.be_do_preallocate =	cccsp_do_preallocate,
	.be_do_precode =	cccsp_do_precode,
	.be_do_codegen =	cccsp_do_codegen,

	.priv =		NULL
};

/*}}}*/
/*{{{  private types*/
typedef struct TAG_cccsp_priv {
	lexfile_t *lastfile;
} cccsp_priv_t;

typedef struct TAG_cccsp_namehook {
	char *cname;		/* low-level variable name */
	int lexlevel;		/* lexical level */
	int alloc_wsh;		/* allocation in high-workspace */
	int alloc_wsl;		/* allocation in low-workspace */
	int alloc_vs;		/* allocation in vectorspace */
	int alloc_ms;		/* allocation in mobilespace */
	int typesize;		/* size of the actual type (if known) */
	int indir;		/* indirection count (0 = real-thing, 1 = pointer, 2 = pointer-pointer, etc.) */
	typecat_e typecat;	/* type category */
} cccsp_namehook_t;

typedef struct TAG_kroccifccs_namerefhook {
	tnode_t *nnode;				/* underlying back-end name-nodE */
	cccsp_namehook_t *nhook;		/* underlying name-hook */
} cccsp_namerefhook_t;

typedef struct TAG_cccsp_blockhook {
	int lexlevel;				/* lexical level of this block */
} cccsp_blockhook_t;

typedef struct TAG_cccsp_blockrefhook {
	tnode_t *block;
} cccsp_blockrefhook_t;


/*}}}*/
/*{{{  private data*/

static chook_t *codegeninithook = NULL;
static chook_t *codegenfinalhook = NULL;

static chook_t *cccsp_ctypestr = NULL;


/*}}}*/


/*{{{  void cccsp_isetindent (FILE *stream, int indent)*/
/*
 *	set-indent for debugging output
 */
void cccsp_isetindent (FILE *stream, int indent)
{
	int i;

	for (i=0; i<indent; i++) {
		fprintf (stream, "    ");
	}
	return;
}
/*}}}*/

/*{{{  static int cccsp_init_options (cccsp_priv_t *kpriv)*/
/*
 *	initialises options for the KRoC-CIF/CCSP back-end
 *	returns 0 on success, non-zero on failure
 */
static int cccsp_init_options (cccsp_priv_t *kpriv)
{
	// opts_add ("norangechecks", '\0', cccsp_opthandler_flag, (void *)1, "1do not generate range-checks");
	return 0;
}
/*}}}*/

/*{{{  cccsp_namehook_t routines*/
/*{{{  static void cccsp_namehook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	dumps hook data for debugging
 */
static void cccsp_namehook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	cccsp_namehook_t *nh = (cccsp_namehook_t *)hook;

	cccsp_isetindent (stream, indent);
	fprintf (stream, "<namehook addr=\"0x%8.8x\" cname=\"%s\" lexlevel=\"%d\" allocwsh=\"%d\" allocwsl=\"%d\" allocvs=\"%d\" allocms=\"%d\" typesize=\"%d\" indir=\"%d\" typecat=\"0x%8.8x\" />\n",
			(unsigned int)nh, nh->cname, nh->lexlevel, nh->alloc_wsh, nh->alloc_wsl, nh->alloc_vs, nh->alloc_ms,
			nh->typesize, nh->indir, (unsigned int)nh->typecat);
	return;
}
/*}}}*/
/*{{{  static cccsp_namehook_t *cccsp_namehook_create (char *cname, int ll, int asize_wsh, int asize_wsl, int asize_vs, int asize_ms, int tsize, int ind)*/
/*
 *	creates a name-hook
 */
static cccsp_namehook_t *cccsp_namehook_create (char *cname, int ll, int asize_wsh, int asize_wsl, int asize_vs, int asize_ms, int tsize, int ind)
{
	cccsp_namehook_t *nh = (cccsp_namehook_t *)smalloc (sizeof (cccsp_namehook_t));

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
/*{{{  cccsp_namerefhook_t routines*/
/*{{{  static void cccsp_namerefhook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	dumps hook data for debugging
 */
static void cccsp_namerefhook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	cccsp_namerefhook_t *nh = (cccsp_namerefhook_t *)hook;

	cccsp_isetindent (stream, indent);
	fprintf (stream, "<namerefhook addr=\"0x%8.8x\" nnode=\"0x%8.8x\" nhook=\"0x%8.8x\" cname=\"%s\" />\n",
			(unsigned int)nh, (unsigned int)nh->nnode, (unsigned int)nh->nhook, (nh->nhook ? nh->nhook->cname : ""));
	return;
}
/*}}}*/
/*{{{  static cccsp_namerefhook_t *cccsp_namerefhook_create (tnode_t *nnode, cccsp_namehook_t *nhook)*/
/*
 *	creates a name-ref-hook
 */
static cccsp_namerefhook_t *cccsp_namerefhook_create (tnode_t *nnode, cccsp_namehook_t *nhook)
{
	cccsp_namerefhook_t *nh = (cccsp_namerefhook_t *)smalloc (sizeof (cccsp_namerefhook_t));

	nh->nnode = nnode;
	nh->nhook = nhook;

	return nh;
}
/*}}}*/
/*}}}*/
/*{{{  cccsp_blockhook_t routines*/
/*{{{  static void cccsp_blockhook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	dumps hook for debugging
 */
static void cccsp_blockhook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	cccsp_blockhook_t *bh = (cccsp_blockhook_t *)hook;

	cccsp_isetindent (stream, indent);
	fprintf (stream, "<blockhook addr=\"0x%8.8x\" lexlevel=\"%d\" />\n",
			(unsigned int)bh, bh->lexlevel);
	return;
}
/*}}}*/
/*{{{  static cccsp_blockhook_t *cccsp_blockhook_create (int ll)*/
/*
 *	creates a block-hook
 */
static cccsp_blockhook_t *cccsp_blockhook_create (int ll)
{
	cccsp_blockhook_t *bh = (cccsp_blockhook_t *)smalloc (sizeof (cccsp_blockhook_t));

	bh->lexlevel = ll;

	return bh;
}
/*}}}*/
/*}}}*/
/*{{{  cccsp_blockrefhook_t routines*/
/*{{{  static void cccsp_blockrefhook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	dumps hook (debugging)
 */
static void cccsp_blockrefhook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	cccsp_blockrefhook_t *brh = (cccsp_blockrefhook_t *)hook;
	tnode_t *blk = brh->block;

	if (blk && parser_islistnode (blk)) {
		int nitems, i;
		tnode_t **blks = parser_getlistitems (blk, &nitems);

		cccsp_isetindent (stream, indent);
		fprintf (stream, "<blockrefhook addr=\"0x%8.8x\" block=\"0x%8.8x\" nblocks=\"%d\" blocks=\"", (unsigned int)brh, (unsigned int)blk, nitems);
		for (i=0; i<nitems; i++ ) {
			if (i) {
				fprintf (stream, ",");
			}
			fprintf (stream, "0x%8.8x", (unsigned int)blks[i]);
		}
		fprintf (stream, "\" />\n");
	} else {
		cccsp_isetindent (stream, indent);
		fprintf (stream, "<blockrefhook addr=\"0x%8.8x\" block=\"0x%8.8x\" />\n", (unsigned int)brh, (unsigned int)blk);
	}

	return;
}
/*}}}*/
/*{{{  static cccsp_blockrefhook_t *cccsp_blockrefhook_create (tnode_t *block)*/
/*
 *	creates a new hook (populated)
 */
static cccsp_blockrefhook_t *cccsp_blockrefhook_create (tnode_t *block)
{
	cccsp_blockrefhook_t *brh = (cccsp_blockrefhook_t *)smalloc (sizeof (cccsp_blockrefhook_t));

	brh->block = block;

	return brh;
}
/*}}}*/
/*}}}*/


/*{{{  static tnode_t *cccsp_name_create (tnode_t *fename, tnode_t *body, map_t *mdata, int asize_wsh, int asize_wsl, int asize_vs, int asize_ms, int tsize, int ind)*/
/*
 *	creates a new back-end name-node
 */
static tnode_t *cccsp_name_create (tnode_t *fename, tnode_t *body, map_t *mdata, int asize_wsh, int asize_wsl, int asize_vs, int asize_ms, int tsize, int ind)
{
	target_t *xt = mdata->target;		/* must be us! */
	tnode_t *name;
	cccsp_namehook_t *nh;
	char *cname = NULL;

	langops_getname (fename, &cname);
	if (!cname) {
		cname = string_dup ("unknown");
	}
	nh = cccsp_namehook_create (cname, mdata->lexlevel, asize_wsh, asize_wsl, asize_vs, asize_ms, tsize, ind);
	name = tnode_create (xt->tag_NAME, NULL, fename, body, (void *)nh);

	return name;
}
/*}}}*/
/*{{{  static tnode_t *cccsp_nameref_create (tnode_t *bename, map_t *mdata)*/
/*
 *	creates a new back-end name-reference node
 */
static tnode_t *cccsp_nameref_create (tnode_t *bename, map_t *mdata)
{
	cccsp_namerefhook_t *nh;
	cccsp_namehook_t *be_nh;
	tnode_t *name, *fename;

	be_nh = (cccsp_namehook_t *)tnode_nthhookof (bename, 0);
	nh = cccsp_namerefhook_create (bename, be_nh);

	fename = tnode_nthsubof (bename, 0);
	name = tnode_create (mdata->target->tag_NAMEREF, NULL, fename, (void *)nh);

	return name;
}
/*}}}*/
/*{{{  static tnode_t *cccsp_block_create (tnode_t *body, map_t *mdata, tnode_t *slist, int lexlevel)*/
/*
 *	creates a new back-end block
 */
static tnode_t *cccsp_block_create (tnode_t *body, map_t *mdata, tnode_t *slist, int lexlevel)
{
	cccsp_blockhook_t *bh;
	tnode_t *blk;

	bh = cccsp_blockhook_create (lexlevel);
	blk = tnode_create (mdata->target->tag_BLOCK, NULL, body, slist, (void *)bh);

	return blk;
}
/*}}}*/
/*{{{  static tnode_t *cccsp_blockref_create (tnode_t *block, tnode_t *body, map_t *mdata)*/
/*
 *	creates a new back-end block reference node (used for procedure instances and the like)
 */
static tnode_t *cccsp_blockref_create (tnode_t *block, tnode_t *body, map_t *mdata)
{
	cccsp_blockrefhook_t *brh = cccsp_blockrefhook_create (block);
	tnode_t *blockref;

	blockref = tnode_create (cccsp_target.tag_BLOCKREF, NULL, body, (void *)brh);

	return blockref;
}
/*}}}*/


/*{{{  static int cccsp_prewalktree_codegen (tnode_t *node, void *data)*/
/*
 *	prewalktree for code generation, calls comp-ops "lcodegen" routine where present
 *	returns 0 to stop walk, 1 to continue
 */
static int cccsp_prewalktree_codegen (tnode_t *node, void *data)
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
	} else if (node->tag->ndef->ops && tnode_hascompop_i (node->tag->ndef->ops, (int)COPS_CODEGEN)) {
		i = tnode_callcompop_i (node->tag->ndef->ops, (int)COPS_CODEGEN, 2, node, cgen);
	}

	/*{{{  if finalisers, do subnodes then finalisers*/
	if (cgfh) {
		int nsnodes, j;
		tnode_t **snodes = tnode_subnodesof (node, &nsnodes);

		for (j=0; j<nsnodes; j++) {
			tnode_prewalktree (snodes[j], cccsp_prewalktree_codegen, (void *)cgen);
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
/*{{{  static int cccsp_modprewalktree_namemap (tnode_t **nodep, void *data)*/
/*
 *	modprewalktree for name-mapping, calls comp-ops "lnamemap" routine where present
 *	returns 0 to stop walk, 1 to continue
 */
static int cccsp_modprewalktree_namemap (tnode_t **nodep, void *data)
{
	map_t *map = (map_t *)data;
	int i = 1;

	if ((*nodep)->tag->ndef->ops && tnode_hascompop_i ((*nodep)->tag->ndef->ops, (int)COPS_LNAMEMAP)) {
		i = tnode_callcompop_i ((*nodep)->tag->ndef->ops, (int)COPS_LNAMEMAP, 2, nodep, map);
	} else if ((*nodep)->tag->ndef->ops && tnode_hascompop_i ((*nodep)->tag->ndef->ops, (int)COPS_NAMEMAP)) {
		i = tnode_callcompop_i ((*nodep)->tag->ndef->ops, (int)COPS_NAMEMAP, 2, nodep, map);
	}

	return i;
}
/*}}}*/
/*{{{  static int cccsp_modprewalktree_betrans (tnode_t **tptr, void *arg)*/
/*
 *	does back-end transform for parse-tree nodes (CCSP/C specific)
 *	returns 0 to stop walk, 1 to continue
 */
static int cccsp_modprewalktree_betrans (tnode_t **tptr, void *arg)
{
	int i = 1;

	if (*tptr && (*tptr)->tag->ndef->ops && tnode_hascompop_i ((*tptr)->tag->ndef->ops, (int)COPS_BETRANS)) {
		i = tnode_callcompop_i ((*tptr)->tag->ndef->ops, (int)COPS_BETRANS, 2, tptr, (betrans_t *)arg);
	}
	return i;
}
/*}}}*/


/*{{{  static void cccsp_do_betrans (tnode_t **tptr, betrans_t *be)*/
/*
 *	intercepts back-end transform pass
 */
static void cccsp_do_betrans (tnode_t **tptr, betrans_t *be)
{
	tnode_modprewalktree (tptr, cccsp_modprewalktree_betrans, (void *)be);
	return;
}
/*}}}*/
/*{{{  static void cccsp_do_premap (tnode_t **tptr, map_t *map)*/
/*
 *	intercepts pre-map pass
 */
static void cccsp_do_premap (tnode_t **tptr, map_t *map)
{
	nocc_message ("cccsp_do_premap(): here!");
	return;
}
/*}}}*/
/*{{{  static void cccsp_do_namemap (tnode_t **tptr, map_t *map)*/
/*
 *	intercepts name-map pass
 */
static void cccsp_do_namemap (tnode_t **tptr, map_t *map)
{
	tnode_modprewalktree (tptr, cccsp_modprewalktree_namemap, (void *)map);
	return;
}
/*}}}*/
/*{{{  static void cccsp_do_bemap (tnode_t **tptr, map_t *map)*/
/*
 *	intercepts be-map pass
 */
static void cccsp_do_bemap (tnode_t **tptr, map_t *map)
{
	nocc_message ("cccsp_do_bemap(): here!");
	return;
}
/*}}}*/
/*{{{  static void cccsp_do_preallocate (tnode_t *tptr, target_t *target)*/
/*
 *	intercepts pre-allocate pass
 */
static void cccsp_do_preallocate (tnode_t *tptr, target_t *target)
{
	nocc_message ("cccsp_do_preallocate(): here!");
	return;
}
/*}}}*/
/*{{{  static void cccsp_do_precode (tnode_t **tptr, codegen_t *cgen)*/
/*
 *	intercepts pre-code pass
 */
static void cccsp_do_precode (tnode_t **tptr, codegen_t *cgen)
{
	nocc_message ("cccsp_do_precode(): here!");
	return;
}
/*}}}*/
/*{{{  static void cccsp_do_codegen (tnode_t *tptr, codegen_t *cgen)*/
/*
 *	intercepts code-gen pass
 */
static void cccsp_do_codegen (tnode_t *tptr, codegen_t *cgen)
{
	tnode_prewalktree (tptr, cccsp_prewalktree_codegen, (void *)cgen);
	return;
}
/*}}}*/


/*{{{  static void cccsp_coder_comment (codegen_t *cgen, const char *fmt, ...)*/
/*
 *	generates a comment
 */
static void cccsp_coder_comment (codegen_t *cgen, const char *fmt, ...)
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
/*{{{  static void cccsp_coder_debugline (codegen_t *cgen, tnode_t *node)*/
/*
 *	generates debugging information (e.g. before a process)
 */
static void cccsp_coder_debugline (codegen_t *cgen, tnode_t *node)
{
	cccsp_priv_t *kpriv = (cccsp_priv_t *)cgen->target->priv;

#if 0
	nocc_message ("cccsp_coder_debugline(): [%s], line %d", node->tag->name, node->org_line);
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
/*{{{  static void cccsp_coder_c_procentry (codegen_t *cgen, name_t *name, tnode_t *params)*/
/*
 *	creates a procedure/function entry-point
 */
static void cccsp_coder_c_procentry (codegen_t *cgen, name_t *name, tnode_t *params)
{
	codegen_write_fmt (cgen, "void %s (", name->me->name);
	if (!parser_islistnode (params) || !parser_countlist (params)) {
		codegen_write_fmt (cgen, "void");
	} else {
		int nparams, i;
		tnode_t **plist = parser_getlistitems (params, &nparams);

		for (i=0; i<nparams; i++) {
			if (i) {
				codegen_write_fmt (cgen, ",");
			}
			codegen_subcodegen (plist[i], cgen);
		}
	}
	codegen_write_fmt (cgen, ")\n");
	return;
}
/*}}}*/


/*{{{  static int cccsp_be_codegen_init (codegen_t *cgen, lexfile_t *srcfile)*/
/*
 *	initialises back-end code generation for KRoC CIF/CCSP target
 *	returns 0 on success, non-zero on failure
 */
static int cccsp_be_codegen_init (codegen_t *cgen, lexfile_t *srcfile)
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
	cops->comment = cccsp_coder_comment;
	cops->debugline = cccsp_coder_debugline;

	cops->c_procentry = cccsp_coder_c_procentry;

	cgen->cops = cops;

	return 0;
}
/*}}}*/
/*{{{  static int cccsp_be_codegen_final (codegen_t *cgen, lexfile_t *srcfile)*/
/*
 *	shutdown back-end code generation for KRoC CIF/CCSP target
 *	returns 0 on success, non-zero on failure
 */
static int cccsp_be_codegen_final (codegen_t *cgen, lexfile_t *srcfile)
{
	sfree (cgen->cops);
	cgen->cops = NULL;

	codegen_write_fmt (cgen, "/*\n *\tend of code generation\n */\n\n");

	return 0;
}
/*}}}*/


/*{{{  static int cccsp_lcodegen_block (compops_t *cops, tnode_t *blk, codegen_t *cgen)*/
/*
 *	does code-generation for a back-end block
 *	returns 0 to stop walk, 1 to continue
 */
static int cccsp_lcodegen_block (compops_t *cops, tnode_t *blk, codegen_t *cgen)
{
	cccsp_priv_t *kpriv = (cccsp_priv_t *)cgen->target->priv;

	codegen_write_fmt (cgen, "{\n");
	codegen_subcodegen (tnode_nthsubof (blk, 0), cgen);
	codegen_write_fmt (cgen, "}\n");

	return 0;
}
/*}}}*/


/*{{{  int cccsp_init (void)*/
/*
 *	initialises the KRoC CIF/CCSP back-end
 *	returns 0 on success, non-zero on error
 */
int cccsp_init (void)
{
	/* register the target */
	if (target_register (&cccsp_target)) {
		nocc_error ("cccsp_init(): failed to register target!");
		return 1;
	}

	/* setup local stuff */
	codegeninithook = codegen_getcodegeninithook ();
	codegenfinalhook = codegen_getcodegenfinalhook ();

	return 0;
}
/*}}}*/
/*{{{  int cccsp_shutdown (void)*/
/*
 *	shuts down the KRoC CIF/CCSP back-end
 *	returns 0 on success, non-zero on error
 */
int cccsp_shutdown (void)
{
	/* unregister the target */
	if (target_unregister (&cccsp_target)) {
		nocc_error ("cccsp_shutdown(): failed to unregister target!");
		return 1;
	}

	return 0;
}
/*}}}*/


/*{{{  static int cccsp_target_init (target_t *target)*/
/*
 *	initialises the KRoC CIF/CCSP target
 *	returns 0 on success, non-zero on error
 */
static int cccsp_target_init (target_t *target)
{
	tndef_t *tnd;
	cccsp_priv_t *kpriv;
	compops_t *cops;
	langops_t *lops;
	int i;

	if (target->initialised) {
		nocc_internal ("cccsp_target_init(): already initialised!");
		return 1;
	}

	kpriv = (cccsp_priv_t *)smalloc (sizeof (cccsp_priv_t));
	kpriv->lastfile = NULL;
	target->priv = (void *)kpriv;

	cccsp_init_options (kpriv);

	/* setup back-end nodes */
	/*{{{  cccsp:name -- CCCSPNAME*/
	i = -1;
	tnd = tnode_newnodetype ("cccsp:name", &i, 2, 0, 1, TNF_NONE);		/* subnodes: original name, in-scope body; hooks: cccsp_namehook_t */
	tnd->hook_dumptree = cccsp_namehook_dumptree;
	cops = tnode_newcompops ();
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnd->lops = lops;

	i = -1;
	target->tag_NAME = tnode_newnodetag ("CCCSPNAME", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  cccsp:nameref -- CCCSPNAMEREF*/
	i = -1;
	tnd = tnode_newnodetype ("cccsp:nameref", &i, 1, 0, 1, TNF_NONE);		/* subnodes: original name; hooks: cccsp_namerefhook_t */
	tnd->hook_dumptree = cccsp_namerefhook_dumptree;
	cops = tnode_newcompops ();
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnd->lops = lops;

	i = -1;
	target->tag_NAMEREF = tnode_newnodetag ("CCCSPNAMEREF", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  cccsp:block -- CCCSPBLOCK*/
	i = -1;
	tnd = tnode_newnodetype ("cccsp:block", &i, 2, 0, 1, TNF_NONE);		/* subnodes: block body, statics; hooks: cccsp_blockhook_t */
	tnd->hook_dumptree = cccsp_blockhook_dumptree;
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "lcodegen", 2, COMPOPTYPE (cccsp_lcodegen_block));
	tnd->ops = cops;

	i = -1;
	target->tag_BLOCK = tnode_newnodetag ("CCCSPBLOCK", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  cccsp:blockref -- CCCSPBLOCKREF*/
	i = -1;
	tnd = tnode_newnodetype ("cccsp_blockref", &i, 1, 0, 1, TNF_NONE);	/* subnodes: body; hooks: cccsp_blockrefhook_t */
	tnd->hook_dumptree = cccsp_blockrefhook_dumptree;

	i = -1;
	target->tag_BLOCKREF = tnode_newnodetag ("CCCSPBLOCKREF", &i, tnd, NTF_NONE);

	/*}}}*/

	target->initialised = 1;
	return 0;
}
/*}}}*/
