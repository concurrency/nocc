/*
 *	cccsp.c -- KRoC/CCSP back-end
 *	Copyright (C) 2008-2013 Fred Barnes <frmb@kent.ac.uk>
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
#include "cccsp.h"

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
static tnode_t *cccsp_const_create (tnode_t *feconst, map_t *mdata, void *ptr, int length, typecat_e tcat);


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

	.chansize =		4,
	.charsize =		1,
	.intsize =		4,
	.pointersize =		4,
	.slotsize =		4,
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
	.newconst =		cccsp_const_create,
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
	name_t *last_toplevelname;

	ntdef_t *tag_ADDROF;
	ntdef_t *tag_LABEL;			/* used for when we implant 'goto' */
	ntdef_t *tag_LABELREF;
	ntdef_t *tag_GOTO;
} cccsp_priv_t;

typedef struct TAG_cccsp_namehook {
	char *cname;				/* low-level variable name */
	char *ctype;				/* low-level type */
	int lexlevel;				/* lexical level */
	int typesize;				/* size of the actual type (if known) */
	int indir;				/* indirection count (0 = real-thing, 1 = pointer, 2 = pointer-pointer, etc.) */
	typecat_e typecat;			/* type category */
	tnode_t *initialiser;			/* if this thing has an initialiser (not part of an assignment later) */
} cccsp_namehook_t;

typedef struct TAG_kroccifccsp_namerefhook {
	tnode_t *nnode;				/* underlying back-end name-node */
	cccsp_namehook_t *nhook;		/* underlying name-hook */
	int indir;				/* target indirection (0 = real-thing, 1 = pointer, 2 = pointer-pointer, etc.) */
} cccsp_namerefhook_t;

typedef struct TAG_cccsp_blockhook {
	int lexlevel;				/* lexical level of this block */
} cccsp_blockhook_t;

typedef struct TAG_cccsp_blockrefhook {
	tnode_t *block;
} cccsp_blockrefhook_t;

typedef struct TAG_cccsp_consthook {
	void *data;				/* constant data */
	int length;				/* length (in bytes) */
	typecat_e tcat;				/* type-category */
} cccsp_consthook_t;

typedef struct TAG_cccsp_labelhook {
	char *name;
} cccsp_labelhook_t;

typedef struct TAG_cccsp_labelrefhook {
	cccsp_labelhook_t *lab;
} cccsp_labelrefhook_t;


/*}}}*/
/*{{{  private data*/

static chook_t *codegeninithook = NULL;
static chook_t *codegenfinalhook = NULL;

static chook_t *cccsp_ctypestr = NULL;
static int cccsp_coder_inparamlist = 0;

static cccsp_apicall_t cccsp_apicall_table[] = {
	{NOAPI, "", 0},
	{CHAN_IN, "ChanIn", 1},
	{CHAN_OUT, "ChanOut", 2},
};


/*}}}*/


/*{{{  void cccsp_isetindent (fhandle_t *stream, int indent)*/
/*
 *	set-indent for debugging output
 */
void cccsp_isetindent (fhandle_t *stream, int indent)
{
	int i;

	for (i=0; i<indent; i++) {
		fhandle_printf (stream, "    ");
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
/*{{{  static char *cccsp_make_entryname (const char *name)*/
/*
 *	turns a front-end name into a C-CCSP name for a function-entry point.
 *	returns newly allocated name.
 */
static char *cccsp_make_entryname (const char *name, const int procabs)
{
	char *rname = (char *)smalloc (strlen (name) + 10);
	char *ch;

	if (procabs) {
		sprintf (rname, "gproc_%s", name);
	} else {
		sprintf (rname, "gcf_%s", name);
	}
	for (ch = rname + 4; *ch; ch++) {
		switch (*ch) {
		case '.':
			*ch = '_';
			break;
		default:
			break;
		}
	}

	return rname;
}
/*}}}*/


/*{{{  cccsp_namehook_t routines*/
/*{{{  static void cccsp_namehook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)*/
/*
 *	dumps hook data for debugging
 */
static void cccsp_namehook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)
{
	cccsp_namehook_t *nh = (cccsp_namehook_t *)hook;

	cccsp_isetindent (stream, indent);
	if (nh->initialiser) {
		fhandle_printf (stream, "<namehook addr=\"0x%8.8x\" cname=\"%s\" ctype=\"%s\" lexlevel=\"%d\" " \
				"typesize=\"%d\" indir=\"%d\" typecat=\"0x%8.8x\">\n",
				(unsigned int)nh, nh->cname, nh->ctype, nh->lexlevel, nh->typesize, nh->indir, (unsigned int)nh->typecat);
		tnode_dumptree (nh->initialiser, indent + 1, stream);
		cccsp_isetindent (stream, indent);
		fhandle_printf (stream, "</namehook>\n");
	} else {
		fhandle_printf (stream, "<namehook addr=\"0x%8.8x\" cname=\"%s\" ctype=\"%s\" lexlevel=\"%d\" " \
				"typesize=\"%d\" indir=\"%d\" typecat=\"0x%8.8x\" initialiser=\"(null)\" />\n",
				(unsigned int)nh, nh->cname, nh->ctype, nh->lexlevel, nh->typesize, nh->indir, (unsigned int)nh->typecat);
	}
	return;
}
/*}}}*/
/*{{{  static cccsp_namehook_t *cccsp_namehook_create (char *cname, char *ctype, int ll, int asize_wsh, int asize_wsl, int asize_vs, int asize_ms, int tsize, int ind, tnode_t *init)*/
/*
 *	creates a name-hook
 */
static cccsp_namehook_t *cccsp_namehook_create (char *cname, char *ctype, int ll, int tsize, int ind, tnode_t *init)
{
	cccsp_namehook_t *nh = (cccsp_namehook_t *)smalloc (sizeof (cccsp_namehook_t));

	nh->cname = cname;
	nh->ctype = ctype;
	nh->lexlevel = ll;
	nh->typesize = tsize;
	nh->indir = ind;
	nh->typecat = TYPE_NOTTYPE;
	nh->initialiser = init;

	return nh;
}
/*}}}*/
/*}}}*/
/*{{{  cccsp_namerefhook_t routines*/
/*{{{  static void cccsp_namerefhook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)*/
/*
 *	dumps hook data for debugging
 */
static void cccsp_namerefhook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)
{
	cccsp_namerefhook_t *nh = (cccsp_namerefhook_t *)hook;

	cccsp_isetindent (stream, indent);
	fhandle_printf (stream, "<namerefhook addr=\"0x%8.8x\" nnode=\"0x%8.8x\" nhook=\"0x%8.8x\" indir=\"%d\" cname=\"%s\" />\n",
			(unsigned int)nh, (unsigned int)nh->nnode, (unsigned int)nh->nhook, nh->indir, (nh->nhook ? nh->nhook->cname : ""));
	return;
}
/*}}}*/
/*{{{  static cccsp_namerefhook_t *cccsp_namerefhook_create (tnode_t *nnode, cccsp_namehook_t *nhook, int indir)*/
/*
 *	creates a name-ref-hook
 */
static cccsp_namerefhook_t *cccsp_namerefhook_create (tnode_t *nnode, cccsp_namehook_t *nhook, int indir)
{
	cccsp_namerefhook_t *nh = (cccsp_namerefhook_t *)smalloc (sizeof (cccsp_namerefhook_t));

	nh->nnode = nnode;
	nh->nhook = nhook;
	nh->indir = indir;

	return nh;
}
/*}}}*/
/*}}}*/
/*{{{  cccsp_blockhook_t routines*/
/*{{{  static void cccsp_blockhook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)*/
/*
 *	dumps hook for debugging
 */
static void cccsp_blockhook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)
{
	cccsp_blockhook_t *bh = (cccsp_blockhook_t *)hook;

	cccsp_isetindent (stream, indent);
	fhandle_printf (stream, "<blockhook addr=\"0x%8.8x\" lexlevel=\"%d\" />\n",
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
/*{{{  static void cccsp_blockrefhook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)*/
/*
 *	dumps hook (debugging)
 */
static void cccsp_blockrefhook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)
{
	cccsp_blockrefhook_t *brh = (cccsp_blockrefhook_t *)hook;
	tnode_t *blk = brh->block;

	if (blk && parser_islistnode (blk)) {
		int nitems, i;
		tnode_t **blks = parser_getlistitems (blk, &nitems);

		cccsp_isetindent (stream, indent);
		fhandle_printf (stream, "<blockrefhook addr=\"0x%8.8x\" block=\"0x%8.8x\" nblocks=\"%d\" blocks=\"", (unsigned int)brh, (unsigned int)blk, nitems);
		for (i=0; i<nitems; i++ ) {
			if (i) {
				fhandle_printf (stream, ",");
			}
			fhandle_printf (stream, "0x%8.8x", (unsigned int)blks[i]);
		}
		fhandle_printf (stream, "\" />\n");
	} else {
		cccsp_isetindent (stream, indent);
		fhandle_printf (stream, "<blockrefhook addr=\"0x%8.8x\" block=\"0x%8.8x\" />\n", (unsigned int)brh, (unsigned int)blk);
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
/*{{{  cccsp_consthook_t routines*/
/*{{{  static void cccsp_consthook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)*/
/*
 *	dump-tree for constant hook
 */
static void cccsp_consthook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)
{
	cccsp_consthook_t *ch = (cccsp_consthook_t *)hook;
	char *dstr;

	cccsp_isetindent (stream, indent);
	dstr = mkhexbuf ((unsigned char *)ch->data, ch->length);
	fhandle_printf (stream, "<consthook addr=\"0x%8.8x\" data=\"%s\" length=\"%d\" typecat=\"0x%8.8x\" />\n",
			(unsigned int)ch, dstr, ch->length, (unsigned int)ch->tcat);
	return;
}
/*}}}*/
/*{{{  static cccsp_consthook_t *cccsp_consthook_create (void *data, int length, typecat_e tcat)*/
/*
 *	creates a new constant hook
 */
static cccsp_consthook_t *cccsp_consthook_create (void *data, int length, typecat_e tcat)
{
	cccsp_consthook_t *ch = (cccsp_consthook_t *)smalloc (sizeof (cccsp_consthook_t));

	ch->data = mem_ndup (data, length);
	ch->length = length;
	ch->tcat = tcat;

	return ch;
}
/*}}}*/
/*}}}*/
/*{{{  cccsp_labelhook_t routines*/
/*{{{  static void cccsp_labelhook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)*/
/*
 *	dumps a cccsp_labelhook_t (debugging)
 */
static void cccsp_labelhook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)
{
	cccsp_labelhook_t *lh = (cccsp_labelhook_t *)hook;

	cccsp_isetindent (stream, indent);
	fhandle_printf (stream, "<labelhook name=\"%s\" addr=\"0x%8.8x\" />\n", lh->name, (unsigned int)lh);
	return;
}
/*}}}*/
/*{{{  static cccsp_labelhook_t *cccsp_labelhook_create (const char *str)*/
/*
 *	creates a new cccsp_labelhook_t
 */
static cccsp_labelhook_t *cccsp_labelhook_create (const char *str)
{
	cccsp_labelhook_t *lh = (cccsp_labelhook_t *)smalloc (sizeof (cccsp_labelhook_t));

	lh->name = string_dup (str);
	return lh;
}
/*}}}*/
/*}}}*/
/*{{{  cccsp_labelrefhook_t routines*/
/*{{{  static void cccsp_labelrefhook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)*/
/*
 *	dumps a cccsp_labelrefhook_t (debugging)
 */
static void cccsp_labelrefhook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)
{
	cccsp_labelrefhook_t *lrh = (cccsp_labelrefhook_t *)hook;

	cccsp_isetindent (stream, indent);
	fhandle_printf (stream, "<labelrefhook labaddr=\"0x%8.8x\" name=\"%s\" addr=\"0x%8.8x\" />\n",
			(unsigned int)lrh->lab, lrh->lab ? lrh->lab->name : "(null)", (unsigned int)lrh);
	return;
}
/*}}}*/
/*{{{  static cccsp_labelrefhook_t *cccsp_labelrefhook_create (cccsp_labelhook_t *ref)*/
/*
 *	creates a new cccsp_labelrefhook_t
 */
static cccsp_labelrefhook_t *cccsp_labelrefhook_create (cccsp_labelhook_t *ref)
{
	cccsp_labelrefhook_t *lrh = (cccsp_labelrefhook_t *)smalloc (sizeof (cccsp_labelrefhook_t));

	lrh->lab = ref;
	return lrh;
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
	tnode_t *name, *type;
	cccsp_namehook_t *nh;
	char *cname = NULL;
	char *ctype = NULL;
	int isconst;

	langops_getname (fename, &cname);
	isconst = langops_isconst (fename);

	if (!cname) {
		cname = string_dup ("unknown");
	}
	type = typecheck_gettype (fename, NULL);
	if (type) {
		langops_getctypeof (type, &ctype);

		if (ctype && isconst) {
			/* prefix with "const " */
			char *nctype = string_fmt ("const %s", ctype);

			sfree (ctype);
			ctype = nctype;
		}
	} else {
		ctype = string_dup ("void");
	}
#if 0
fhandle_printf (FHAN_STDERR, "cccsp_name_create(): cname=\"%s\" type =\n", cname);
tnode_dumptree (type, 1, FHAN_STDERR);
fhandle_printf (FHAN_STDERR, ">> ctype is \"%s\", fename =\n", ctype);
tnode_dumptree (fename, 1, FHAN_STDERR);
#endif
	nh = cccsp_namehook_create (cname, ctype, mdata->lexlevel, tsize, ind, NULL);
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
	nh = cccsp_namerefhook_create (bename, be_nh, 0);

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
/*{{{  static tnode_t *cccsp_const_create (tnode_t *feconst, map_t *mdata, void *ptr, int length, typecat_e tcat)*/
/*
 *	creates a new back-end constant
 */
static tnode_t *cccsp_const_create (tnode_t *feconst, map_t *mdata, void *ptr, int length, typecat_e tcat)
{
	cccsp_consthook_t *ch;
	tnode_t *cnst;

	ch = cccsp_consthook_create (ptr, length, tcat);
	cnst = tnode_create (mdata->target->tag_CONST, NULL, feconst, ch);

	return cnst;
}
/*}}}*/

/*{{{  int cccsp_set_initialiser (tnode_t *bename, tnode_t *init)*/
/*
 *	unpleasant: allows explicit setting of an initialiser for C generation (attached to CCCSPNAME).
 *	return 0 on success, non-zero on error.
 */
int cccsp_set_initialiser (tnode_t *bename, tnode_t *init)
{
	cccsp_namehook_t *nh;

	if (!bename || (bename->tag != cccsp_target.tag_NAME)) {
		nocc_serious ("cccsp_set_initialiser(): called with bename = [%s]", bename ? bename->tag->name : "(null)");
		return -1;
	}
	nh = (cccsp_namehook_t *)tnode_nthhookof (bename, 0);
	if (nh->initialiser) {
		nocc_warning ("cccsp_set_initialiser(): displacing existing = [%s]", nh->initialiser->tag->name);
	}
	nh->initialiser = init;
	return 0;
}
/*}}}*/
/*{{{  tnode_t *cccsp_create_apicallname (cccsp_apicall_e apin)*/
/*
 *	creates a new constant node that represents the particular API call (note: not transformed into CCCSPCONST)
 */
tnode_t *cccsp_create_apicallname (cccsp_apicall_e apin)
{
	tnode_t *node = constprop_newconst (CONST_INT, NULL, NULL, (int)apin);

	return node;
}
/*}}}*/
/*{{{  tnode_t *cccsp_create_addrof (tnode_t *arg, target_t *target)*/
/*
 *	creates an address-of modifier.
 */
tnode_t *cccsp_create_addrof (tnode_t *arg, target_t *target)
{
	cccsp_priv_t *kpriv = (cccsp_priv_t *)target->priv;
	tnode_t *node = tnode_createfrom (kpriv->tag_ADDROF, arg, arg);

	return node;
}
/*}}}*/
/*{{{  int cccsp_set_indir (tnode_t *benode, int indir, target_t *target)*/
/*
 *	sets desired indirection on something.
 *	returns 0 on success, non-zero on error.
 */
int cccsp_set_indir (tnode_t *benode, int indir, target_t *target)
{
	if (benode->tag == target->tag_NAMEREF) {
		cccsp_namerefhook_t *nrh = (cccsp_namerefhook_t *)tnode_nthhookof (benode, 0);

		nrh->indir = indir;
	} else {
		nocc_internal ("cccsp_set_indir(): don\'t know how to set indirection on [%s:%s]", benode->tag->ndef->name, benode->tag->name);
		return -1;
	}
	return 0;
}
/*}}}*/
/*{{{  int cccsp_get_indir (tnode_t *benode, target_t *target)*/
/*
 *	gets current indirection on something.
 *	returns indirection-level on success, < 0 on error.
 */
int cccsp_get_indir (tnode_t *benode, target_t *target)
{
	if (benode->tag == target->tag_NAMEREF) {
		cccsp_namerefhook_t *nrh = (cccsp_namerefhook_t *)tnode_nthhookof (benode, 0);

		return nrh->indir;
	} else if (benode->tag == target->tag_NAME) {
		cccsp_namehook_t *nh = (cccsp_namehook_t *)tnode_nthhookof (benode, 0);

		return nh->indir;
	}
	nocc_internal ("cccsp_get_indir(): don\'t know how to get indirection of [%s:%s]", benode->tag->ndef->name, benode->tag->name);
	return -1;
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
	if (!map->hook) {
		cccsp_mapdata_t *cmd = (cccsp_mapdata_t *)smalloc (sizeof (cccsp_mapdata_t));

		cmd->target_indir = 0;
		map->hook = (void *)cmd;
		tnode_modprewalktree (tptr, cccsp_modprewalktree_namemap, (void *)map);
		map->hook = NULL;

		sfree (cmd);
	} else {
		tnode_modprewalktree (tptr, cccsp_modprewalktree_namemap, (void *)map);
	}
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
	if (!node->org) {
		/* nothing to generate */
		return;
	}
	if (node->org->org_file != kpriv->lastfile) {
		kpriv->lastfile = node->org->org_file;
		codegen_write_fmt (cgen, "#FILE %s\n", node->org->org_file->filename ?: "(unknown)");
	}
	codegen_write_fmt (cgen, "#LINE %d\n", node->org->org_line);

	return;
}
/*}}}*/
/*{{{  static void cccsp_coder_c_procentry (codegen_t *cgen, name_t *name, tnode_t *params)*/
/*
 *	creates a procedure/function entry-point
 */
static void cccsp_coder_c_procentry (codegen_t *cgen, name_t *name, tnode_t *params)
{
	cccsp_priv_t *kpriv = (cccsp_priv_t *)cgen->target->priv;
	char *entryname = cccsp_make_entryname (name->me->name, (params == NULL) ? 1 : 0);

	/*
	 *	updated: for CIF, this is a little different now
	 */

	codegen_ssetindent (cgen);
	if (!params) {
		/* parameters-in-workspace */
		codegen_write_fmt (cgen, "void %s (Workspace wptr)\n", entryname);
	} else {
		int i, nparams;
		tnode_t **plist;

		cccsp_coder_inparamlist++;
		plist = parser_getlistitems (params, &nparams);
		codegen_write_fmt (cgen, "void %s (Workspace wptr", entryname);
		for (i=0; i<nparams; i++) {
			codegen_write_fmt (cgen, ", ");
			codegen_subcodegen (plist[i], cgen);
		}
		codegen_write_fmt (cgen, ")\n");
		cccsp_coder_inparamlist--;
	}
	sfree (entryname);

	if (!compopts.notmainmodule && !params) {
		/* save as last (generated in source order) */
		kpriv->last_toplevelname = name;
	}

	return;
}
/*}}}*/
/*{{{  static void cccsp_coder_c_proccall (codegen_t *cgen, const char *name, tnode_t *params, int isapi)*/
/*
 *	creates a function instance
 */
static void cccsp_coder_c_proccall (codegen_t *cgen, const char *name, tnode_t *params, int isapi)
{
	char *procname;
	
	if (isapi) {
		cccsp_apicall_t *apic = &(cccsp_apicall_table[isapi]);

		procname = string_dup (apic->name);
	} else {
		procname = cccsp_make_entryname (name, (params == NULL) ? 1 : 0);
	}

	codegen_ssetindent (cgen);
	if (!params) {
		/* parameters in workspace, but should probably not be called like this! */
		codegen_write_fmt (cgen, "%s (wptr);\n", procname);
		codegen_warning (cgen, "cccsp_coder_c_proccall(): unexpected params == NULL here, for %s", procname);
	} else {
		int i, nparams;
		tnode_t **plist;

		codegen_write_fmt (cgen, "%s (wptr", procname);
		plist = parser_getlistitems (params, &nparams);
		for (i=0; i<nparams; i++) {
			codegen_write_fmt (cgen, ", ");
			codegen_subcodegen (plist[i], cgen);
		}
		codegen_write_fmt (cgen, ");\n");
	}
	sfree (procname);

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
	cops->c_proccall = cccsp_coder_c_proccall;

	cgen->cops = cops;

	/* produce standard includes */
	codegen_write_file (cgen, "cccsp/verb-header.h");

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
	cccsp_priv_t *kpriv = (cccsp_priv_t *)cgen->target->priv;

	if (!compopts.notmainmodule) {
		int has_screen = -1, has_error = -1, has_kyb = -1;
		int nparams = 0;
		char *entryname;
		tnode_t *beblk;
		tnode_t *toplevelparams;

		if (!kpriv->last_toplevelname) {
			codegen_error (cgen, "cccsp_be_codegen_final(): no top-level process set");
			return -1;
		}

		entryname = cccsp_make_entryname (NameNameOf (kpriv->last_toplevelname), 1);

		beblk = tnode_nthsubof (NameDeclOf (kpriv->last_toplevelname), 2);
		if (beblk->tag != cgen->target->tag_BLOCK) {
			toplevelparams = NULL;
		} else {
			toplevelparams = tnode_nthsubof (beblk, 1);
		}
#if 0
fhandle_printf (FHAN_STDERR, "cccsp_be_codegen_final(): generating interface, top-level parameters are:\n");
tnode_dumptree (toplevelparams, 1, FHAN_STDERR);
#endif
		if (toplevelparams) {
			tnode_t **params;
			int i;

			params = parser_getlistitems (toplevelparams, &nparams);
			for (i=0; i<nparams; i++) {
				tnode_t *fename;

				if (params[i]->tag != cgen->target->tag_NAME) {
					nocc_internal ("cccsp_be_codegen_final(): top-level parameter not back-end name, got [%s:%s]",
							params[i]->tag->ndef->name, params[i]->tag->name);
					return -1;
				}
				fename = tnode_nthsubof (params[i], 0);

				switch (langops_guesstlp (fename)) {
				default:
					codegen_error (cgen, "cccsp_be_codegen_final(): could not guess top-level parameter usage (%d)", i);
					return -1;
				case 1:
					if (has_kyb >= 0) {
						codegen_error (cgen, "cccsp_be_codegen_final(): confused, two keyboard channels? (%d)", i);
						return -1;
					}
					has_kyb = i;
					break;
				case 2:
					if (has_screen >= 0) {
						if (has_error >= 0) {
							codegen_error (cgen, "cccsp_be_codegen_final(): confused, two screen channels? (%d)", i);
							return -1;
						}
						has_error = i;
					} else {
						has_screen = i;
					}
					break;
				case 3:
					if (has_error >= 0) {
						codegen_error (cgen, "cccsp_be_codegen_final(): confused, two error channels? (%d)", i);
						return -1;
					}
					has_error = i;
					break;
				}
			}
		}

		/* generate main() and call of top-level process */
		codegen_write_fmt (cgen, "\n\n/* insert main() for top-level process [%s] */\n\n", NameNameOf (kpriv->last_toplevelname));
		if (has_kyb >= 0) {
			codegen_write_fmt (cgen, "extern void process_keyboard (Workspace wptr);\n");
		}
		if (has_screen >= 0) {
			codegen_write_fmt (cgen, "extern void process_screen (Workspace wptr);\n");
		}
		if (has_error >= 0) {
			codegen_write_fmt (cgen, "extern void process_error (Workspace wptr);\n");
		}

		codegen_write_fmt (cgen, "\nvoid process_main (Workspace wptr)\n");
		codegen_write_fmt (cgen, "{\n");
		if (has_kyb >= 0) {
			codegen_write_fmt (cgen, "	Channel ch_kyb;\n");
			codegen_write_fmt (cgen, "	Workspace p_kyb;\n");
			codegen_write_fmt (cgen, "	word *ws_kyb = NULL;\n");
		}
		if (has_screen >= 0) {
			codegen_write_fmt (cgen, "	Channel ch_scr;\n");
			codegen_write_fmt (cgen, "	Workspace p_scr;\n");
			codegen_write_fmt (cgen, "	word *ws_scr = NULL;\n");
		}
		if (has_error >= 0) {
			codegen_write_fmt (cgen, "	Channel ch_err;\n");
			codegen_write_fmt (cgen, "	Workspace p_err;\n");
			codegen_write_fmt (cgen, "	word *ws_err = NULL;\n");
		}
		codegen_write_fmt (cgen, "	Workspace p_user;\n");
		codegen_write_fmt (cgen, "	word *ws_user = NULL;\n");
		codegen_write_fmt (cgen, "\n");
		if (has_kyb >= 0) {
			codegen_write_fmt (cgen, "	ChanInit (wptr, &ch_kyb);\n");
			codegen_write_fmt (cgen, "	ws_kyb = (word *)MAlloc (wptr, WORKSPACE_SIZE(1,1024) * 4);\n");
			codegen_write_fmt (cgen, "	p_kyb = LightProcInit (wptr, ws_kyb, 1, 1024);\n");
			codegen_write_fmt (cgen, "	ProcParam (wptr, p_kyb, 0, &ch_kyb);\n");
		}
		if (has_screen >= 0) {
			codegen_write_fmt (cgen, "	ChanInit (wptr, &ch_scr);\n");
			codegen_write_fmt (cgen, "	ws_scr = (word *)MAlloc (wptr, WORKSPACE_SIZE(1,1024) * 4);\n");
			codegen_write_fmt (cgen, "	p_scr = LightProcInit (wptr, ws_scr, 1, 1024);\n");
			codegen_write_fmt (cgen, "	ProcParam (wptr, p_scr, 0, &ch_scr);\n");
		}
		if (has_error >= 0) {
			codegen_write_fmt (cgen, "	ChanInit (wptr, &ch_err);\n");
			codegen_write_fmt (cgen, "	ws_err = (word *)MAlloc (wptr, WORKSPACE_SIZE(1,1024) * 4);\n");
			codegen_write_fmt (cgen, "	p_err = LightProcInit (wptr, ws_err, 1, 1024);\n");
			codegen_write_fmt (cgen, "	ProcParam (wptr, p_err, 0, &ch_err);\n");
		}
		/* FIXME: need some common-sense to determine likely memory requirements */
		codegen_write_fmt (cgen, "	ws_user = (word *)MAlloc (wptr, WORKSPACE_SIZE(%d,%d) * 4);\n", nparams, 4096);
		codegen_write_fmt (cgen, "	p_user = LightProcInit (wptr, ws_user, %d, %d);\n", nparams, 4096);
		if (has_kyb >= 0) {
			codegen_write_fmt (cgen, "	ProcParam (wptr, p_user, %d, &ch_kyb);\n", has_kyb);
		}
		if (has_screen >= 0) {
			codegen_write_fmt (cgen, "	ProcParam (wptr, p_user, %d, &ch_scr);\n", has_screen);
		}
		if (has_error >= 0) {
			codegen_write_fmt (cgen, "	ProcParam (wptr, p_user, %d, &ch_err);\n", has_error);
		}

		codegen_write_fmt (cgen, "	ProcPar (wptr, %d,\n", nparams + 1);
		if (has_kyb >= 0) {
			codegen_write_fmt (cgen, "		p_kyb, process_keyboard,\n");
		}
		if (has_screen >= 0) {
			codegen_write_fmt (cgen, "		p_scr, process_screen,\n");
		}
		if (has_error >= 0) {
			codegen_write_fmt (cgen, "		p_err, process_error,\n");
		}
		codegen_write_fmt (cgen, "		p_user, %s);\n\n", entryname);
		codegen_write_fmt (cgen, "	Shutdown (wptr);\n");

		codegen_write_fmt (cgen, "}\n\n");
		codegen_write_fmt (cgen, "int main (int argc, char **argv)\n");
		codegen_write_fmt (cgen, "{\n");
		codegen_write_fmt (cgen, "	Workspace p;\n\n");
		codegen_write_fmt (cgen, "	if (!ccsp_init ()) {\n");
		codegen_write_fmt (cgen, "		return 1;\n");
		codegen_write_fmt (cgen, "	}\n\n");
		codegen_write_fmt (cgen, "	p = ProcAllocInitial (0, 1024);\n");
		codegen_write_fmt (cgen, "	ProcStartInitial (p, process_main);\n\n");
		codegen_write_fmt (cgen, "	/* NOT REACHED */\n");
		codegen_write_fmt (cgen, "	return 0;\n");
		codegen_write_fmt (cgen, "}\n");

#if 0
		if (!parser_islistnode (params) || !parser_countlist (params)) {
			codegen_write_fmt (cgen, "void");
		} else {
			int nparams, i;
			tnode_t **plist = parser_getlistitems (params, &nparams);

			cccsp_coder_inparamlist++;
			for (i=0; i<nparams; i++) {
				if (i) {
					codegen_write_fmt (cgen, ",");
				}
				codegen_subcodegen (plist[i], cgen);
			}
			cccsp_coder_inparamlist--;
		}
		codegen_write_fmt (cgen, ")\n");
#endif
	}

	codegen_write_fmt (cgen, "/*\n *\tend of code generation\n */\n\n");

	sfree (cgen->cops);
	cgen->cops = NULL;


	return 0;
}
/*}}}*/

/*{{{  static int cccsp_lcodegen_name (compops_t *cops, tnode_t *name, codegen_t *cgen)*/
/*
 *	does code-generation for a back-end name: produces the C declaration.
 *	returns 0 to stop walk, 1 to continue
 */
static int cccsp_lcodegen_name (compops_t *cops, tnode_t *name, codegen_t *cgen)
{
	cccsp_namehook_t *nh = (cccsp_namehook_t *)tnode_nthhookof (name, 0);
	char *indirstr = (char *)smalloc (nh->indir + 1);
	int i;

	for (i=0; i<nh->indir; i++) {
		indirstr[i] = '*';
	}
	indirstr[i] = '\0';

	if (nh->initialiser) {
		codegen_ssetindent (cgen);
		codegen_write_fmt (cgen, "%s%s %s = ", nh->ctype, indirstr, nh->cname);
		codegen_subcodegen (nh->initialiser, cgen);
		codegen_write_fmt (cgen, ";\n");
	} else if (cccsp_coder_inparamlist) {
		codegen_write_fmt (cgen, "%s%s %s", nh->ctype, indirstr, nh->cname);
	} else {
		codegen_ssetindent (cgen);
		codegen_write_fmt (cgen, "%s%s %s;\n", nh->ctype, indirstr, nh->cname);
	}
	return 0;
}
/*}}}*/
/*{{{  static int cccsp_lcodegen_nameref (compops_t *cops, tnode_t *nameref, codegen_t *cgen)*/
/*
 *	does code-generation for a name-reference: produces the C name, adjusted for pointer-ness.
 *	returns 0 to stop walk, 1 to continue
 */
static int cccsp_lcodegen_nameref (compops_t *cops, tnode_t *nameref, codegen_t *cgen)
{
	cccsp_namerefhook_t *nrf = (cccsp_namerefhook_t *)tnode_nthhookof (nameref, 0);
	cccsp_namehook_t *nh = nrf->nhook;

	if (nrf->indir > nh->indir) {
		/* want more indirection */
		if (nrf->indir == (nh->indir + 1)) {
			codegen_write_fmt (cgen, "&");
		} else {
			codegen_node_error (cgen, nameref, "cccsp_lcodegen_nameref(): too much indirection (wanted %d, got %d)",
					nrf->indir, nh->indir);
		}
	} else if (nrf->indir < nh->indir) {
		/* want less indirection */
		int i;

		for (i=nrf->indir; i<nh->indir; i++) {
			codegen_write_fmt (cgen, "*");
		}
	}
	codegen_write_fmt (cgen, "%s", nh->cname);

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

	codegen_ssetindent (cgen);
	codegen_write_fmt (cgen, "{\n");

	/* do statics/parameters first */
	cgen->indent++;
	codegen_subcodegen (tnode_nthsubof (blk, 1), cgen);
	codegen_subcodegen (tnode_nthsubof (blk, 0), cgen);
	cgen->indent--;

	codegen_ssetindent (cgen);
	codegen_write_fmt (cgen, "}\n");

	return 0;
}
/*}}}*/
/*{{{  static int cccsp_lcodegen_const (compops_t *cops, tnode_t *cnst, codegen_t *cgen)*/
/*
 *	does code-generation for a back-end constant: produces the raw data.
 *	returns 0 to stop walk, 1 to continue
 */
static int cccsp_lcodegen_const (compops_t *cops, tnode_t *cnst, codegen_t *cgen)
{
	cccsp_consthook_t *ch = (cccsp_consthook_t *)tnode_nthhookof (cnst, 0);

	if (ch->tcat & TYPE_INTEGER) {
		if ((ch->length == 1) || (ch->length == 2) || (ch->length == 4)) {
			/* 8/16/32-bit */
			if (ch->tcat & TYPE_SIGNED) {
				int val;
				
				if (ch->length == 1) {
					val = (int)(*(char *)(ch->data));
				} else if (ch->length == 2) {
					val = (int)(*(short int *)(ch->data));
				} else if (ch->length == 4) {
					val = *(int *)(ch->data);
				}

				codegen_write_fmt (cgen, "%d", val);
			} else {
				unsigned int val;

				if (ch->length == 1) {
					val = (unsigned int)(*(unsigned char *)(ch->data));
				} else if (ch->length == 2) {
					val = (unsigned int)(*(unsigned short int *)(ch->data));
				} else if (ch->length == 4) {
					val = *(unsigned int *)(ch->data);
				}

				codegen_write_fmt (cgen, "%u", val);
			}
		} else {
			nocc_serious ("cccsp_lcodegen_const(): unhandled integer size %d", ch->length);
		}
	} else if (ch->tcat & TYPE_REAL) {
		if (ch->length == 4) {
			float val = *(float *)(ch->data);

			codegen_write_fmt (cgen, "%f", val);
		} else if (ch->length == 8) {
			double val = *(double *)(ch->data);

			codegen_write_fmt (cgen, "%lf", val);
		} else {
			nocc_serious ("cccsp_lcodegen_const(): unhandled floating-point size %d", ch->length);
		}
	}
	return 0;
}
/*}}}*/
/*{{{  static int cccsp_lcodegen_modifier (compops_t *cops, tnode_t *mod, codegen_t *cgen)*/
/*
 *	does code-generation for a modifier (e.g. address-of)
 *	returns 0 to stop walk, 1 to continue
 */
static int cccsp_lcodegen_modifier (compops_t *cops, tnode_t *mod, codegen_t *cgen)
{
	cccsp_priv_t *kpriv = (cccsp_priv_t *)cgen->target->priv;

	if (mod->tag == kpriv->tag_ADDROF) {
		tnode_t *op = tnode_nthsubof (mod, 0);

		codegen_write_fmt (cgen, "&(");
		codegen_subcodegen (tnode_nthsubof (mod, 0), cgen);
		codegen_write_fmt (cgen, ")");
	} else {
		codegen_error (cgen, "cccsp_lcodegen_modifier(): unknown modifier [%s]", mod->tag->name);
	}
	return 0;
}
/*}}}*/
/*{{{  static int cccsp_lcodegen_op (compops_t *cops, tnode_t *op, codegen_t *cgen)*/
/*
 *	does code-generation for an operator (e.g. 'goto')
 *	returns 0 to stop walk, 1 to continue
 */
static int cccsp_lcodegen_op (compops_t *cops, tnode_t *op, codegen_t *cgen)
{
	/* FIXME: incomplete! */
	return 1;
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
	kpriv->last_toplevelname = NULL;
	kpriv->tag_ADDROF = NULL;
	target->priv = (void *)kpriv;

	cccsp_init_options (kpriv);

	/* setup back-end nodes */
	/*{{{  cccsp:name -- CCCSPNAME*/
	i = -1;
	tnd = tnode_newnodetype ("cccsp:name", &i, 2, 0, 1, TNF_NONE);		/* subnodes: original name, in-scope body; hooks: cccsp_namehook_t */
	tnd->hook_dumptree = cccsp_namehook_dumptree;
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "lcodegen", 2, COMPOPTYPE (cccsp_lcodegen_name));
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
	tnode_setcompop (cops, "lcodegen", 2, COMPOPTYPE (cccsp_lcodegen_nameref));
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
	/*{{{  cccsp:const -- CCCSPCONST*/
	i = -1;
	tnd = tnode_newnodetype ("cccsp:const", &i, 1, 0, 1, TNF_NONE);		/* subnodes: original const; hooks: cccsp_consthook_t */
	tnd->hook_dumptree = cccsp_consthook_dumptree;
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "lcodegen", 2, COMPOPTYPE (cccsp_lcodegen_const));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnd->lops = lops;

	i = -1;
	target->tag_CONST = tnode_newnodetag ("CCCSPCONST", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  cccsp:modifier -- CCCSPADDROF*/
	i = -1;
	tnd = tnode_newnodetype ("cccsp:modifier", &i, 1, 0, 0, TNF_NONE);	/* subnodes: operand */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "lcodegen", 2, COMPOPTYPE (cccsp_lcodegen_modifier));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnd->lops = lops;

	i = -1;
	kpriv->tag_ADDROF = tnode_newnodetag ("CCCSPADDROF", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  cccsp:label -- CCCSPLABEL*/
	i = -1;
	tnd = tnode_newnodetype ("cccsp:label", &i, 0, 0, 1, TNF_NONE);		/* hooks: cccsp_labelhook_t */
	tnd->hook_dumptree = cccsp_labelhook_dumptree;
	cops = tnode_newcompops ();
	tnd->ops = cops;

	i = -1;
	kpriv->tag_LABEL = tnode_newnodetag ("CCCSPLABEL", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  cccsp:labelref -- CCCSPLABELREF*/
	i = -1;
	tnd = tnode_newnodetype ("cccsp:labelref", &i, 0, 0, 1, TNF_NONE);	/* hooks: cccsp_labelrefhook_t */
	tnd->hook_dumptree = cccsp_labelrefhook_dumptree;
	cops = tnode_newcompops ();
	tnd->ops = cops;

	i = -1;
	kpriv->tag_LABELREF = tnode_newnodetag ("CCCSPLABELREF", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  cccsp:op -- CCCSPGOTO*/
	i = -1;
	tnd = tnode_newnodetype ("cccsp:op", &i, 1, 0, 0, TNF_NONE);		/* subnodes: operator */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "lcodegen", 2, COMPOPTYPE (cccsp_lcodegen_op));
	tnd->ops = cops;

	i = -1;
	kpriv->tag_GOTO = tnode_newnodetag ("CCCSPGOTO", &i, tnd, NTF_NONE);

	/*}}}*/

	target->initialised = 1;
	return 0;
}
/*}}}*/

