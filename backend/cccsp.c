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
#include "cccsp.h"
#include "parsepriv.h"

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

static void cccsp_be_getblocksize (tnode_t *blk, int *wsp, int *wsoffsp, int *vsp, int *msp, int *adjp, int *elabp);
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
	.be_getblocksize =	cccsp_be_getblocksize,
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
	int wptr_count;

	char *cc_path;				/* path to C compiler */
	char *cc_flags;				/* C compiler flags for KRoC/CCSP */
	char *cc_incpath;			/* C compiler flags for header includes */
	char *cc_libpath;			/* C compiler flags for where libraries live */
	char *cc_ldflags;			/* C compiler linker flags for building executables */

	ntdef_t *tag_ADDROF;
	ntdef_t *tag_NWORDSOF;
	ntdef_t *tag_LABEL;			/* used for when we implant 'goto' */
	ntdef_t *tag_LABELREF;
	ntdef_t *tag_GOTO;

	ntdef_t *tag_WPTR;
	ntdef_t *tag_WORKSPACE;
	ntdef_t *tag_WPTRTYPE;
	ntdef_t *tag_WORKSPACETYPE;
	ntdef_t *tag_UTYPE;
	ntdef_t *tag_ARRAYSUB;
	ntdef_t *tag_RECORDSUB;

	chook_t *wsfixuphook;
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

	int my_size;				/* words required by directly declared things */
	int nest_size;				/* words required by nested blocks (max sum) */
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

typedef struct TAG_cccsp_wptrhook {
	char *name;				/* e.g. wptr0 */
} cccsp_wptrhook_t;

typedef struct TAG_cccsp_workspacehook {
	char *name;				/* e.g. ws0 */
	int isdyn;				/* needs to be dynamically allocated at run-time */
	int nparams;				/* number of parameters, -1 if unknown */
	int nwords;				/* number of words, -1 if unknown */
} cccsp_workspacehook_t;

typedef struct TAG_cccsp_utypehook {
	char *name;				/* e.g. "gt_foo" */
	int nwords;				/* number of words for the type itself, -1 if unknown */
} cccsp_utypehook_t;

typedef struct TAG_cccsp_indexhook {
	int indir;				/* desired indirection on arraysub/recordsub */
} cccsp_indexhook_t;

/*}}}*/
/*{{{  private data*/

static chook_t *codegeninithook = NULL;
static chook_t *codegenfinalhook = NULL;
static chook_t *cccspoutfilehook = NULL;
static chook_t *cccspsfifilehook = NULL;
static void *cccsp_set_outfile = NULL;			/* string copy (char*) for the above compiler hook */

static int cccsp_bepass = 0;

static chook_t *cccsp_ctypestr = NULL;
static int cccsp_coder_inparamlist = 0;

static cccsp_apicall_t cccsp_apicall_table[] = {
	{NOAPI, "", 0},
	{CHAN_IN, "ChanIn", 32},
	{CHAN_OUT, "ChanOut", 32},
	{STOP_PROC, "SetErrW", 32},
	{PROC_PAR, "ProcPar", 32},
	{LIGHT_PROC_INIT, "LightProcInit", 32},
	{PROC_PARAM, "ProcParam", 8},
	{GET_PROC_PARAM, "GetProcParam", 8},
	{MEM_ALLOC, "MAlloc", 32},
	{MEM_RELEASE, "MRelease", 32},
	{MEM_RELEASE_CHK, "MReleaseChk", 32},
	{STR_INIT, "GuppyStringInit", 32},
	{STR_FREE, "GuppyStringFree", 32},
	{STR_ASSIGN, "GuppyStringAssign", 64},
	{STR_CONCAT, "GuppyStringConcat", 64},
	{STR_CLEAR, "GuppyStringClear", 32},
	{CHAN_INIT, "ChanInit", 8},
	{TIMER_READ, "TimerRead", 32},
	{TIMER_WAIT, "TimerWait", 32}
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
/*{{{  static int cccsp_opthandler_stopat (cmd_option_t *opt, char ***argwalk, int *argleft)*/
/*
 *	option handler for CCCSP pass "stop" options
 */
static int cccsp_opthandler_stopat (cmd_option_t *opt, char ***argwalk, int *argleft)
{
	compopts.stoppoint = (int)(opt->arg);
#if 0
fprintf (stderr, "cccsp_opthandler_stopat(): setting stop point to %d\n", compopts.stoppoint);
#endif
	return 0;
}
/*}}}*/
/*{{{  char *cccsp_make_entryname (const char *name, const int procabs)*/
/*
 *	turns a front-end name into a C-CCSP name for a function-entry point.
 *	returns newly allocated name.
 */
char *cccsp_make_entryname (const char *name, const int procabs)
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
	fhandle_printf (stream, "<blockhook addr=\"0x%8.8x\" lexlevel=\"%d\" my_size=\"%d\" nest_size=\"%d\" />\n",
			(unsigned int)bh, bh->lexlevel, bh->my_size, bh->nest_size);
	return;
}
/*}}}*/
/*{{{  static cccsp_blockhook_t *cccsp_blockhook_create (int ll, int sz, int nest)*/
/*
 *	creates a block-hook
 */
static cccsp_blockhook_t *cccsp_blockhook_create (int ll, int sz, int nest)
{
	cccsp_blockhook_t *bh = (cccsp_blockhook_t *)smalloc (sizeof (cccsp_blockhook_t));

	bh->lexlevel = ll;
	bh->my_size = sz;
	bh->nest_size = nest;

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
/*{{{  cccsp_wptrhook_t routines*/
/*{{{  static void cccsp_wptrhook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)*/
/*
 *	dumps a cccsp_wptrhook_t (debugging)
 */
static void cccsp_wptrhook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)
{
	cccsp_wptrhook_t *whook = (cccsp_wptrhook_t *)hook;

	cccsp_isetindent (stream, indent);
	fhandle_printf (stream, "<wptrhook name=\"%s\" addr=\"0x%8.8x\" />\n", whook->name, (unsigned int)whook);
	return;
}
/*}}}*/
/*{{{  static cccsp_wptrhook_t *cccsp_wptrhook_create (int id)*/
/*
 *	creates a new cccsp_wptrhook_t
 */
static cccsp_wptrhook_t *cccsp_wptrhook_create (int id)
{
	cccsp_wptrhook_t *whook = (cccsp_wptrhook_t *)smalloc (sizeof (cccsp_wptrhook_t));

	whook->name = string_fmt ("wptr%d", id);
	return whook;
}
/*}}}*/
/*}}}*/
/*{{{  cccsp_workspacehook_t routines*/
/*{{{  static void cccsp_workspacehook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)*/
/*
 *	dumps a cccsp_workspacehook_t (debugging)
 */
static void cccsp_workspacehook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)
{
	cccsp_workspacehook_t *whook = (cccsp_workspacehook_t *)hook;

	cccsp_isetindent (stream, indent);
	fhandle_printf (stream, "<workspacehook name=\"%s\" isdyn=\"%d\" nparams=\"%d\" nwords=\"%d\" addr=\"0x%8.8x\" />\n",
			whook->name, whook->isdyn, whook->nparams, whook->nwords, (unsigned int)whook);
	return;
}
/*}}}*/
/*{{{  static cccsp_workspacehook_t *cccsp_workspacehook_create (int id)*/
/*
 *	creates a new cccsp_workspacehook_t
 */
static cccsp_workspacehook_t *cccsp_workspacehook_create (int id)
{
	cccsp_workspacehook_t *whook = (cccsp_workspacehook_t *)smalloc (sizeof (cccsp_workspacehook_t));

	whook->name = string_fmt ("ws%d", id);
	whook->isdyn = 0;
	whook->nparams = -1;
	whook->nwords = -1;

	return whook;
}
/*}}}*/
/*}}}*/
/*{{{  cccsp_utypehook_t routines*/
/*{{{  static void cccsp_utypehook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)*/
/*
 *	dumps a cccsp_utypehook_t (debugging)
 */
static void cccsp_utypehook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)
{
	cccsp_utypehook_t *uthook = (cccsp_utypehook_t *)hook;

	cccsp_isetindent (stream, indent);
	fhandle_printf (stream, "<utypehook name=\"%s\" nwords=\"%d\" addr=\"0x%8.8x\" />\n",
			uthook->name, uthook->nwords, (unsigned int)uthook);
	return;
}
/*}}}*/
/*{{{  static cccsp_utypehook_t *cccsp_utypehook_create (const char *name)*/
/*
 *	creates a new cccsp_utypehook_t
 */
static cccsp_utypehook_t *cccsp_utypehook_create (const char *name)
{
	cccsp_utypehook_t *uthook = (cccsp_utypehook_t *)smalloc (sizeof (cccsp_utypehook_t));

	uthook->name = string_fmt ("gt_%s", name);
	uthook->nwords = -1;

	return uthook;
}
/*}}}*/
/*}}}*/
/*{{{  cccsp_indexhook_t routines*/
/*{{{  static void cccsp_indexhook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)*/
/*
 *	dumps a cccsp_indexhook_t (debugging)
 */
static void cccsp_indexhook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)
{
	cccsp_indexhook_t *idh = (cccsp_indexhook_t *)hook;

	cccsp_isetindent (stream, indent);
	fhandle_printf (stream, "<indexhook indir=\"%d\" addr=\"0x%8.8x\" />\n",
			idh->indir, (unsigned int)idh);
	return;
}
/*}}}*/
/*{{{  static cccsp_indexhook_t *cccsp_indexhook_create (int indir)*/
/*
 *	creates a new cccsp_indexhook_t
 */
static cccsp_indexhook_t *cccsp_indexhook_create (int indir)
{
	cccsp_indexhook_t *idh = (cccsp_indexhook_t *)smalloc (sizeof (cccsp_indexhook_t));

	idh->indir = indir;

	return idh;
}
/*}}}*/
/*}}}*/

/*{{{  static void *cccsp_outfilehook_copy (void *hook)*/
/*
 *	copy for cccsp:outfile compiler hook
 */
static void *cccsp_outfilehook_copy (void *hook)
{
	if (!hook) {
		return NULL;
	}
	return (char *)string_dup ((char *)hook);
}
/*}}}*/
/*{{{  static void cccsp_outfilehook_free (void *hook)*/
/*
 *	free for cccsp:outfile compiler hook
 */
static void cccsp_outfilehook_free (void *hook)
{
	if (!hook) {
		return;
	}
	sfree (hook);
}
/*}}}*/
/*{{{  static void cccsp_outfilehook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)*/
/*
 *	dump-tree for a cccsp:outfile compiler hook
 */
static void cccsp_outfilehook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)
{
	cccsp_isetindent (stream, indent);
	fhandle_printf (stream, "<cccsp:outfile value=\"%s\" />\n",
			hook ? (char *)hook : "");
	return;
}
/*}}}*/

/*{{{  static void *cccsp_sfifilehook_copy (void *hook)*/
/*
 *	copy for cccsp:sfifile compiler hook
 */
static void *cccsp_sfifilehook_copy (void *hook)
{
	if (!hook) {
		return NULL;
	}
	return (char *)string_dup ((char *)hook);
}
/*}}}*/
/*{{{  static void cccsp_sfifilehook_free (void *hook)*/
/*
 *	free for cccsp:sfifile compiler hook
 */
static void cccsp_sfifilehook_free (void *hook)
{
	if (!hook) {
		return;
	}
	sfree (hook);
}
/*}}}*/
/*{{{  static void cccsp_sfifilehook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)*/
/*
 *	dump-tree for a cccsp:sfifile compiler hook
 */
static void cccsp_sfifilehook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)
{
	cccsp_isetindent (stream, indent);
	fhandle_printf (stream, "<cccsp:sfifile value=\"%s\" />\n",
			hook ? (char *)hook : "");
	return;
}
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

#if 0
fhandle_printf (FHAN_STDERR, "cccsp_nameref_create(): created new nameref at 0x%8.8x\n", (unsigned int)name);
#endif
	return name;
}
/*}}}*/
/*{{{  static tnode_t *cccsp_block_create (tnode_t *body, map_t *mdata, tnode_t *slist, int lexlevel)*/
/*
 *	creates a new back-end block
 */
static tnode_t *cccsp_block_create (tnode_t *body, map_t *mdata, tnode_t *slist, int lexlevel)
{
	cccsp_priv_t *kpriv = (cccsp_priv_t *)mdata->target->priv;
	cccsp_blockhook_t *bh;
	tnode_t *blk;

	bh = cccsp_blockhook_create (lexlevel, 0, 0);
	blk = tnode_create (mdata->target->tag_BLOCK, SLOCI, body, slist, (void *)bh);

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
/*{{{  int cccsp_stkwords_apicallnode (tnode_t *call)*/
/*
 *	returns the number of stack words needed for a particular API call (excluding parameters)
 *	extracts straight from call structure, returns < 0 on error
 */
int cccsp_stkwords_apicallnode (tnode_t *call)
{
	int val;

	if (!constprop_isconst (call)) {
		nocc_internal ("cccsp_stkwords_apicallnode(): oops, call not constant, got [%s]", call->tag->name);
		return -1;
	}
	val = constprop_intvalof (call);
	if ((val <= 0) || (val > (int)CCCSP_APICALL_LAST)) {
		nocc_internal ("cccsp_stkwords_apicallnode(): oops, invalid API call %d", val);
		return -1;
	}
	return cccsp_apicall_table[val].stkwords;
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

#if 0
fhandle_printf (FHAN_STDERR, "cccsp_set_indir(): setting NAMEREF at 0x%8.8x to %d\n", (unsigned int)benode, indir);
#endif
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
/*{{{  char *cccsp_make_apicallname (tnode_t *call)*/
/*
 *	returns the entry-name of a particular API call (verbatim)
 *	returned string is newly allocated
 */
char *cccsp_make_apicallname (tnode_t *call)
{
	int val;

	if (!constprop_isconst (call)) {
		nocc_internal ("cccsp_make_apicallname(): oops, call not constant, got [%s]", call->tag->name);
		return string_dup ("invalid");
	}
	val = constprop_intvalof (call);
	if ((val <= 0) || (val > (int)CCCSP_APICALL_LAST)) {
		nocc_internal ("cccsp_make_apicallname(): oops, invalid API call %d", val);
		return string_dup ("invalid");
	}

	return string_dup (cccsp_apicall_table[val].name);
}
/*}}}*/

/*{{{  int cccsp_set_toplevelname (name_t *tlname, target_t *target)*/
/*
 *	sets the top-level name, needed to be able to call, etc.
 *	returns 0 on success, non-zero on failure
 */
int cccsp_set_toplevelname (name_t *tlname, target_t *target)
{
	cccsp_priv_t *kpriv = (cccsp_priv_t *)target->priv;

	kpriv->last_toplevelname = tlname;
	return 0;
}
/*}}}*/
/*{{{  tnode_t *cccsp_create_wptr (srclocn_t *org, target_t *target)*/
/*
 *	creates a new workspace-pointer (leaf node in effect)
 */
tnode_t *cccsp_create_wptr (srclocn_t *org, target_t *target)
{
	cccsp_priv_t *kpriv = (cccsp_priv_t *)target->priv;
	tnode_t *node, *type;
	cccsp_wptrhook_t *whook;
	
	whook = cccsp_wptrhook_create (kpriv->wptr_count);
	kpriv->wptr_count++;

	type = tnode_create (kpriv->tag_WPTRTYPE, org);
	node = tnode_create (kpriv->tag_WPTR, org, type, whook);

	return node;
}
/*}}}*/
/*{{{  tnode_t *cccsp_create_wptr (srclocn_t *org, target_t *target)*/
/*
 *	creates a new workspace (leaf node in effect)
 */
tnode_t *cccsp_create_workspace (srclocn_t *org, target_t *target)
{
	cccsp_priv_t *kpriv = (cccsp_priv_t *)target->priv;
	tnode_t *node, *type;
	cccsp_workspacehook_t *whook;
	
	whook = cccsp_workspacehook_create (kpriv->wptr_count);
	kpriv->wptr_count++;

	type = tnode_create (kpriv->tag_WORKSPACETYPE, org);
	node = tnode_create (kpriv->tag_WORKSPACE, org, type, whook);

	return node;
}
/*}}}*/
/*{{{  int cccsp_set_workspace_nparams (tnode_t *wsnode, int nparams)*/
/*
 *	sets the number of parameters for the space-reserving WORKSPACE type
 *	returns 0 on success, non-zero on error
 */
int cccsp_set_workspace_nparams (tnode_t *wsnode, int nparams)
{
	cccsp_priv_t *kpriv = (cccsp_priv_t *)cccsp_target.priv;
	cccsp_workspacehook_t *whook;

	if (wsnode->tag != kpriv->tag_WORKSPACE) {
		return 1;
	}
	whook = (cccsp_workspacehook_t *)tnode_nthhookof (wsnode, 0);

	whook->nparams = nparams;
	return 0;
}
/*}}}*/
/*{{{  int cccsp_set_workspace_nwords (tnode_t *wsnode, int nwords)*/
/*
 *	sets the number of stack words for the space-reserving WORKSPACE type
 *	returns 0 on success, non-zero on error
 */
int cccsp_set_workspace_nwords (tnode_t *wsnode, int nwords)
{
	cccsp_priv_t *kpriv = (cccsp_priv_t *)cccsp_target.priv;
	cccsp_workspacehook_t *whook;

	if (wsnode->tag != kpriv->tag_WORKSPACE) {
		return 1;
	}
	whook = (cccsp_workspacehook_t *)tnode_nthhookof (wsnode, 0);

	whook->nwords = nwords;
	return 0;
}
/*}}}*/
/*{{{  tnode_t *cccsp_create_workspace_nwordsof (tnode_t *wsnode, target_t *target)*/
/*
 *	creates a node expression that returns the number of words associated with a WORKSPACE
 */
tnode_t *cccsp_create_workspace_nwordsof (tnode_t *wsnode, target_t *target)
{
	cccsp_priv_t *kpriv = (cccsp_priv_t *)target->priv;
	tnode_t *t;
	
	t = tnode_create (kpriv->tag_NWORDSOF, SLOCI, wsnode);

	return t;
}
/*}}}*/
/*{{{  tnode_t *cccsp_create_utype (srclocn_t *org, target_t *target, const char *name, tnode_t *fields)*/
/*
 *	creates a new UTYPE node, used for user-defined structured types
 */
tnode_t *cccsp_create_utype (srclocn_t *org, target_t *target, const char *name, tnode_t *fields)
{
	tnode_t *node;
	cccsp_priv_t *kpriv = (cccsp_priv_t *)target->priv;
	cccsp_utypehook_t *uthook = cccsp_utypehook_create (name);

	node = tnode_create (kpriv->tag_UTYPE, org, fields, uthook);

	return node;
}
/*}}}*/
/*{{{  tnode_t *cccsp_create_arraysub (srclocn_t *org, target_t *target, tnode_t *base, tnode_t *index, int indir)*/
/*
 *	creates a new ARRAYSUB node, used for accessing array elements
 */
tnode_t *cccsp_create_arraysub (srclocn_t *org, target_t *target, tnode_t *base, tnode_t *index, int indir)
{
	tnode_t *node;
	cccsp_priv_t *kpriv = (cccsp_priv_t *)target->priv;
	cccsp_indexhook_t *idh = cccsp_indexhook_create (indir);

	node = tnode_create (kpriv->tag_ARRAYSUB, org, base, index, idh);

	return node;
}
/*}}}*/
/*{{{  tnode_t *cccsp_create_recordsub (srclocn_t *org, target_t *target, tnode_t *base, tnode_t *field, int indir)*/
/*
 *	creates a new RECORDSUB node, used for accessing record fields (mostly within user-defined types)
 */
tnode_t *cccsp_create_recordsub (srclocn_t *org, target_t *target, tnode_t *base, tnode_t *field, int indir)
{
	tnode_t *node;
	cccsp_priv_t *kpriv = (cccsp_priv_t *)target->priv;
	cccsp_indexhook_t *idh = cccsp_indexhook_create (indir);

	node = tnode_create (kpriv->tag_RECORDSUB, org, base, field, idh);

	return node;
}
/*}}}*/

/*{{{  static int cccsp_prewalktree_preallocate (tnode_t *node, void *data)*/
/*
 *	walk-tree for preallocate, calls comp-ops "lpreallocate" routine where present
 *	returns 0 to stop walk, 1 to continue
 */
static int cccsp_prewalktree_preallocate (tnode_t *node, void *data)
{
	int r = 1;
	cccsp_preallocate_t *cpa = (cccsp_preallocate_t *)data;

	if (node->tag->ndef->ops && tnode_hascompop_i (node->tag->ndef->ops, (int)COPS_LPREALLOCATE)) {
		r = tnode_callcompop_i (node->tag->ndef->ops, (int)COPS_LPREALLOCATE, 2, node, cpa);
	} else if (node->tag->ndef->ops && tnode_hascompop_i (node->tag->ndef->ops, (int)COPS_PREALLOCATE)) {
		r = tnode_callcompop_i (node->tag->ndef->ops, (int)COPS_PREALLOCATE, 2, node, cpa->target);
	}

	return r;
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
/*{{{  static int cccsp_modprewalktree_precode (tnode_t **nodep, void *data)*/
/*
 *	modprewalktree for pre-codegen, calls comp-ops "lprecode" routine where present
 *	returns 0 to stop walk, 1 to continue
 */
static int cccsp_modprewalktree_precode (tnode_t **nodep, void *data)
{
	codegen_t *cgen = (codegen_t *)data;
	int i = 1;

	if ((*nodep)->tag->ndef->ops && tnode_hascompop_i ((*nodep)->tag->ndef->ops, (int)COPS_LPRECODE)) {
		i = tnode_callcompop_i ((*nodep)->tag->ndef->ops, (int)COPS_LPRECODE, 2, nodep, cgen);
	} else if ((*nodep)->tag->ndef->ops && tnode_hascompop_i ((*nodep)->tag->ndef->ops, (int)COPS_PRECODE)) {
		i = tnode_callcompop_i ((*nodep)->tag->ndef->ops, (int)COPS_PRECODE, 2, nodep, cgen);
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
/*{{{  static int cccsp_prewalktree_cccspdcg (tnode_t *node, void *data)*/
/*
 *	called during tree-walk for direct-call-graph generation
 *	returns 0 to stop walk, 1 to continue
 */
static int cccsp_prewalktree_cccspdcg (tnode_t *node, void *data)
{
	int r = 1;
	cccsp_dcg_t *dcg = (cccsp_dcg_t *)data;

	if (node->tag->ndef->ops && tnode_hascompop (node->tag->ndef->ops, "cccsp:dcg")) {
		r = tnode_callcompop (node->tag->ndef->ops, "cccsp:dcg", 2, node, dcg);
	}
	return r;
}
/*}}}*/
/*{{{  static int cccsp_prewalktree_cccspdcgfix (tnode_t *node, void *data)*/
/*
 *	called during tree-walk for fixing-up allocations
 *	returns 0 to stop walk, 1 to continue
 */
static int cccsp_prewalktree_cccspdcgfix (tnode_t *node, void *data)
{
	int r = 1;

	if (node->tag->ndef->ops && tnode_hascompop (node->tag->ndef->ops, "cccsp:dcgfix")) {
		r = tnode_callcompop (node->tag->ndef->ops, "cccsp:dcgfix", 1, node);
	}
	return r;
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
		cmd->process_id = NULL;
		cmd->langhook = NULL;
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
	cccsp_preallocate_t *cpa = (cccsp_preallocate_t *)smalloc (sizeof (cccsp_preallocate_t));

	cpa->target = target;
	cpa->lexlevel = 0;

	cccsp_preallocate_subtree (tptr, cpa);

	sfree (cpa);
	return;
}
/*}}}*/
/*{{{  static void cccsp_do_precode (tnode_t **tptr, codegen_t *cgen)*/
/*
 *	intercepts pre-code pass
 */
static void cccsp_do_precode (tnode_t **tptr, codegen_t *cgen)
{
	cccsp_precode_subtree (tptr, cgen);
	return;
}
/*}}}*/
/*{{{  static void cccsp_do_codegen (tnode_t *tptr, codegen_t *cgen)*/
/*
 *	intercepts code-gen pass;  called for each tree-node that 'codegen' runs over.
 */
static void cccsp_do_codegen (tnode_t *tptr, codegen_t *cgen)
{
#if 0
fhandle_printf (FHAN_STDERR, "cccsp_do_codegen(): cgen->fname=[%s], tptr=[%s]\n", cgen->fname, tptr ? tptr->tag->name : "(null)");
#endif
	if (cccsp_set_outfile) {
		/* first time this has been called, probably the top-level list for guppy */
		tnode_setchook (tptr, cccspoutfilehook, cccsp_set_outfile);
		cccsp_set_outfile = NULL;
	}
	tnode_prewalktree (tptr, cccsp_prewalktree_codegen, (void *)cgen);
	return;
}
/*}}}*/

/*{{{  int cccsp_preallocate_subtree (tnode_t *tptr, cccsp_preallocate_t *cpa)*/
/*
 *	does preallocate sub-tree walk
 *	returns 0 on success, non-zero on error
 */
int cccsp_preallocate_subtree (tnode_t *tptr, cccsp_preallocate_t *cpa)
{
	tnode_prewalktree (tptr, cccsp_prewalktree_preallocate, (void *)cpa);
	return 0;
}
/*}}}*/
/*{{{  int cccsp_precode_subtree (tnode_t **nodep, codegen_t *cgen)*/
/*
 *	does precode sub-tree walk
 *	returns 0 on success, non-zero on error
 */
int cccsp_precode_subtree (tnode_t **nodep, codegen_t *cgen)
{
	tnode_modprewalktree (nodep, cccsp_modprewalktree_precode, (void *)cgen);
	return 0;
}
/*}}}*/
/*{{{  int cccsp_cccspdcg_subtree (tnode_t *tptr, cccsp_dcg_t *dcg)*/
/*
 *	does cccsp:dcg sub-tree walk
 *	returns 0 on success, non-zero on error
 */
int cccsp_cccspdcg_subtree (tnode_t *tptr, cccsp_dcg_t *dcg)
{
	tnode_prewalktree (tptr, cccsp_prewalktree_cccspdcg, (void *)dcg);
	return 0;
}
/*}}}*/
/*{{{  int cccsp_cccspdcgfix_subtree (tnode_t *tptr)*/
/*
 *	does cccsp:dcgfix sub-tree walk
 *	returns 0 on success, non-zero on error
 */
int cccsp_cccspdcgfix_subtree (tnode_t *tptr)
{
	tnode_prewalktree (tptr, cccsp_prewalktree_cccspdcgfix, NULL);
	return 0;
}
/*}}}*/
/*{{{  int cccsp_getblockspace (tnode_t *beblk, int *mysize, int *nestsize)*/
/*
 *	gets a back-end block space requirements
 *	returns 0 on success, non-zero on failure
 */
int cccsp_getblockspace (tnode_t *beblk, int *mysize, int *nestsize)
{
	if (beblk->tag == cccsp_target.tag_BLOCK) {
		cccsp_blockhook_t *bh = (cccsp_blockhook_t *)tnode_nthhookof (beblk, 0);

		if (mysize) {
			*mysize = bh->my_size;
		}
		if (nestsize) {
			*nestsize = bh->nest_size;
		}
		return 0;
	}
	return -1;
}
/*}}}*/
/*{{{  int cccsp_addtofixups (tnode_t *beblk, tnode_t *node)*/
/*
 *	adds something to the list of fixups for a back-end block, processed *after* prealloc
 *	returns 0 on success, non-zero on failure
 */
int cccsp_addtofixups (tnode_t *beblk, tnode_t *node)
{
	tnode_t *flist;
	cccsp_priv_t *kpriv = (cccsp_priv_t *)cccsp_target.priv;

	flist = (tnode_t *)tnode_getchook (beblk, kpriv->wsfixuphook);
	if (!flist) {
		flist = parser_newlistnode (SLOCI);
		tnode_setchook (beblk, kpriv->wsfixuphook, flist);
	}
#if 0
fhandle_printf (FHAN_STDERR, "cccsp_addtofixups(): beblk=[%s:%s] node=[%s:%s]\n", beblk->tag->ndef->name, beblk->tag->name,
		node->tag->ndef->name, node->tag->name);
#endif
	parser_addtolist (flist, node);
	return 0;
}
/*}}}*/
/*{{{  cccsp_sfi_entry_t *cccsp_sfiofname (name_t *name, int pinst)*/
/*
 *	during the SFI passes, can be used to find the entry for a particular name
 *	will create anew if does not exist
 */
cccsp_sfi_entry_t *cccsp_sfiofname (name_t *name, int pinst)
{
	cccsp_sfi_entry_t *sfient;
	char *entryname = cccsp_make_entryname (name->me->name, pinst);

	sfient = cccsp_sfi_lookupornew (entryname);
	sfree (entryname);
	return sfient;
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
	return;

	if (node->org->org_file != kpriv->lastfile) {
		kpriv->lastfile = node->org->org_file;
		codegen_write_fmt (cgen, "#file %s\n", node->org->org_file->filename ?: "(unknown)");
	}
	codegen_write_fmt (cgen, "#line %d\n", node->org->org_line);

	return;
}
/*}}}*/
/*{{{  static void cccsp_coder_c_procentry (codegen_t *cgen, name_t *name, tnode_t *params, int pinst)*/
/*
 *	creates a procedure/function entry-point
 */
static void cccsp_coder_c_procentry (codegen_t *cgen, name_t *name, tnode_t *params, int pinst)
{
	cccsp_priv_t *kpriv = (cccsp_priv_t *)cgen->target->priv;
	char *entryname = cccsp_make_entryname (name->me->name, pinst);

	/*
	 *	updated: for CIF, this is a little different now
	 */

	codegen_ssetindent (cgen);
	if (!params) {
		/* shouldn't happen now Workspace is properly parameterised */
		nocc_internal ("cccsp_coder_c_procentry(): no parameters, but did expect some..");

		/* parameters-in-workspace */
		codegen_write_fmt (cgen, "void %s (void)\n", entryname);
	} else {
		int i, nparams;
		tnode_t **plist;

		cccsp_coder_inparamlist++;
		plist = parser_getlistitems (params, &nparams);
		codegen_write_fmt (cgen, "void %s (", entryname);
		for (i=0; i<nparams; i++) {
			if (i > 0) {
				codegen_write_fmt (cgen, ", ");
			}
			codegen_subcodegen (plist[i], cgen);
		}
		codegen_write_fmt (cgen, ")\n");
		cccsp_coder_inparamlist--;
	}
	sfree (entryname);

	return;
}
/*}}}*/
/*{{{  static void cccsp_coder_c_proccall (codegen_t *cgen, const char *name, tnode_t *params, int isapi, tnode_t *apires)*/
/*
 *	creates a function instance
 */
static void cccsp_coder_c_proccall (codegen_t *cgen, const char *name, tnode_t *params, int isapi, tnode_t *apires)
{
	char *procname;
	
	if (isapi) {
		cccsp_apicall_t *apic = &(cccsp_apicall_table[isapi]);

		procname = string_dup (apic->name);
	} else {
		procname = cccsp_make_entryname (name, 0);
	}

	codegen_ssetindent (cgen);
	if (!params) {
		/* parameters in workspace, but should probably not be called like this! */
		codegen_write_fmt (cgen, "%s ();\n", procname);
		codegen_warning (cgen, "cccsp_coder_c_proccall(): unexpected params == NULL here, for %s", procname);
	} else {
		int i, nparams;
		tnode_t **plist;

		if (apires) {
			/* has some result that we assign to */
			codegen_subcodegen (apires, cgen);
			codegen_write_fmt (cgen, " = ");
		}
		codegen_write_fmt (cgen, "%s (", procname);
		plist = parser_getlistitems (params, &nparams);
		for (i=0; i<nparams; i++) {
			if (i > 0) {
				codegen_write_fmt (cgen, ", ");
			}
			codegen_subcodegen (plist[i], cgen);
		}
		codegen_write_fmt (cgen, ");\n");
	}
	sfree (procname);

	return;
}
/*}}}*/

/*{{{  static void cccsp_be_getblocksize (tnode_t *blk, int *wsp, int *wsoffsp, int *vsp, int *msp, int *adjp, int *elabp)*/
/*
 *	gets the block-size for a back-end block (called by library code in this case)
 */
static void cccsp_be_getblocksize (tnode_t *blk, int *wsp, int *wsoffsp, int *vsp, int *msp, int *adjp, int *elabp)
{
	cccsp_priv_t *kpriv = (cccsp_priv_t *)cccsp_target.priv;
	cccsp_blockhook_t *bh;
	
	if (blk->tag != cccsp_target.tag_BLOCK) {
		nocc_internal ("cccsp_be_getblocksize(): not BLOCK, got [%s:%s]", blk->tag->ndef->name, blk->tag->name);
		return;
	}
	bh = (cccsp_blockhook_t *)tnode_nthhookof (blk, 0);

	if (wsp) {
		*wsp = bh->my_size;
	}
	if (wsoffsp) {
		*wsoffsp = 0;
	}
	if (vsp) {
		*vsp = bh->nest_size;
	}
	if (msp) {
		*msp = 0;
	}
	if (adjp) {
		*adjp = 0;
	}
	if (elabp) {
		*elabp = 0;
	}
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

	/* make a note of the output file-name in some global data */
	cccsp_set_outfile = (void *)string_dup (cgen->fname);

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
		int blk_my, blk_nest;

		if (!kpriv->last_toplevelname) {
			codegen_error (cgen, "cccsp_be_codegen_final(): no top-level process set");
			return -1;
		}

		entryname = cccsp_make_entryname (NameNameOf (kpriv->last_toplevelname), 1);

		beblk = tnode_nthsubof (NameDeclOf (kpriv->last_toplevelname), 2);
		if (beblk->tag != cgen->target->tag_BLOCK) {
			toplevelparams = NULL;
			blk_my = 0;
			blk_nest = 4096;
		} else {
			toplevelparams = tnode_nthsubof (beblk, 1);
			cccsp_getblockspace (beblk, &blk_my, &blk_nest);
			blk_nest += 16;
		}
#if 0
fhandle_printf (FHAN_STDERR, "cccsp_be_codegen_final(): generating interface, top-level parameters (space = %d,%d) are:\n", blk_my, blk_nest);
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
			codegen_write_fmt (cgen, "extern void process_screen_string (Workspace wptr);\n");
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
		codegen_write_fmt (cgen, "	ws_user = (word *)MAlloc (wptr, WORKSPACE_SIZE(%d,%d) * 4);\n", nparams, blk_nest);
		codegen_write_fmt (cgen, "	p_user = LightProcInit (wptr, ws_user, %d, %d);\n", nparams, blk_nest);
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
			codegen_write_fmt (cgen, "		p_scr, process_screen_string,\n");
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

/*{{{  static int cccsp_lpreallocate_name (compops_t *cops, tnode_t *node, cccsp_preallocate_t *cpa)*/
/*
 *	does pre-allocate for a back-end name: counts up words of stack usage.
 *	returns 0 to stop walk, 1 to continue
 */
static int cccsp_lpreallocate_name (compops_t *cops, tnode_t *node, cccsp_preallocate_t *cpa)
{
	cccsp_priv_t *kpriv = (cccsp_priv_t *)cpa->target->priv;
	cccsp_namehook_t *nh = (cccsp_namehook_t *)tnode_nthhookof (node, 0);
	tnode_t *src = tnode_nthsubof (node, 0);

	if (src->tag == kpriv->tag_WORKSPACE) {
		cccsp_workspacehook_t *wsh = (cccsp_workspacehook_t *)tnode_nthhookof (src, 0);

#if 0
fhandle_printf (FHAN_STDERR, "cccsp_lpreallocate_name(): came across WORKSPACE declaration isdyn=%d of (%d,%d)\n",
		wsh->isdyn, wsh->nparams, wsh->nwords);
#endif
		if ((wsh->nparams < 0) || (wsh->nwords < 0)) {
			/* don't know yet, will need to make this dynamic */
			wsh->isdyn = 1;
		} else {
			cpa->collect += (wsh->nparams + wsh->nwords + 8);
		}
	} else {
		/* FIXME: assuming two words.. */
		cpa->collect += 2;
	}

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
	cccsp_priv_t *kpriv = (cccsp_priv_t *)cgen->target->priv;
	char *indirstr = (char *)smalloc (nh->indir + 1);
	tnode_t *src = tnode_nthsubof (name, 0);
	int i;

	for (i=0; i<nh->indir; i++) {
		indirstr[i] = '*';
	}
	indirstr[i] = '\0';

	if (src->tag == kpriv->tag_WORKSPACE) {
		/* slightly special case for these */
		cccsp_workspacehook_t *whook = (cccsp_workspacehook_t *)tnode_nthhookof (src, 0);

		if (whook->nwords < 0) {
			codegen_warning (cgen, "cccsp_lcodegen_name(): nwords not set, assuming 1024..");
			whook->nwords = 1024;
		}

		codegen_ssetindent (cgen);
		if (!whook->isdyn) {
			codegen_write_fmt (cgen, "%s %s[WORKSPACE_SIZE(%d,%d)];\n", nh->ctype, nh->cname, whook->nparams, whook->nwords);
		}
	} else {
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
	}
	return 0;
}
/*}}}*/
/*{{{  static int cccsp_bytesfor_name (langops_t *lops, tnode_t *node, target_t *target)*/
/*
 *	returns the number of bytes required for a back-end name, or < 0 on error.
 */
static int cccsp_bytesfor_name (langops_t *lops, tnode_t *node, target_t *target)
{
	cccsp_namehook_t *nh = (cccsp_namehook_t *)tnode_nthhookof (node, 0);

#if 1
fhandle_printf (FHAN_STDERR, "cccsp_bytesfor_name(): here, node =\n");
tnode_dumptree (node, 1, FHAN_STDERR);
#endif
	return nh->typesize;
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
/*{{{  static int cccsp_lpreallocate_block (compops_t *cops, tnode_t *blk, cccsp_preallocate_t *cpa)*/
/*
 *	does pre-allocation for a back-end block
 *	returns 0 to stop walk, 1 to continue
 */
static int cccsp_lpreallocate_block (compops_t *cops, tnode_t *blk, cccsp_preallocate_t *cpa)
{
	cccsp_priv_t *kpriv = (cccsp_priv_t *)cpa->target->priv;
	cccsp_blockhook_t *bh = (cccsp_blockhook_t *)tnode_nthhookof (blk, 0);
	tnode_t *slist = tnode_nthsubof (blk, 1);
	int saved_collect = cpa->collect;
	tnode_t *flist;
	
	/* sub-preallocate nested blocks */
	cpa->collect = 0;
	cccsp_preallocate_subtree (tnode_nthsubof (blk, 0), cpa);
	bh->nest_size = cpa->collect;

	if (slist) {
		cpa->collect = 0;
		cccsp_preallocate_subtree (slist, cpa);
		bh->my_size = cpa->collect;
	}

	/* see if we have any fixups to deal with */
	flist = (tnode_t *)tnode_getchook (blk, kpriv->wsfixuphook);
	if (flist) {
		tnode_t **fitems;
		int nfitems, i;

		fitems = parser_getlistitems (flist, &nfitems);
		for (i=0; i<nfitems; i++) {
			if (fitems[i]->tag == kpriv->tag_WORKSPACE) {
				cccsp_workspacehook_t *wsh = (cccsp_workspacehook_t *)tnode_nthhookof (fitems[i], 0);

#if 0
fhandle_printf (FHAN_STDERR, "cccsp_lpreallocate_block(): fixing up, bh->my_size=%d, bh->nest_size=%d, wsh->nparams=%d, wsh->nwords=%d\n",
		bh->my_size, bh->nest_size, wsh->nparams, wsh->nwords);
#endif
				if (wsh->nparams < 0) {
					wsh->nparams = bh->my_size;
				}
				if (wsh->nwords < 0) {
					wsh->nwords = bh->nest_size;
				}
			} else {
				nocc_internal ("cccsp_lpreallocate_block(): unhandled fixup for [%s:%s]",
						fitems[i]->tag->ndef->name, fitems[i]->tag->name);
				return 0;
			}
		}
	}

	cpa->collect = saved_collect + (bh->nest_size + bh->my_size);

	return 0;
}
/*}}}*/
/*{{{  static int cccsp_lprecode_block (compops_t *cops, tnode_t **nodep, codegen_t *cgen)*/
/*
 *	does pre-code-generation for a back-end block;  deals with any deferred WS fixups
 *	returns 0 to stop walk, 1 to continue
 */
static int cccsp_lprecode_block (compops_t *cops, tnode_t **nodep, codegen_t *cgen)
{
	return 1;
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
	} else if (mod->tag == kpriv->tag_NWORDSOF) {
		tnode_t *op = tnode_nthsubof (mod, 0);
		cccsp_workspacehook_t *whook;

		if (op->tag != kpriv->tag_WORKSPACE) {
			nocc_internal ("cccsp_lcodegen_modifier(): NWORDSOF operand not WORKSPACE, got [%s]", op->tag->name);
			return 0;
		}
		whook = (cccsp_workspacehook_t *)tnode_nthhookof (op, 0);

		codegen_write_fmt (cgen, "%d", whook->nwords);
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

/*{{{  static int cccsp_namemap_wptr (compops_t *cops, tnode_t **nodep, map_t *map)*/
/*
 *	does name-mapping for the slightly special WPTR node
 *	returns 0 to stop walk, 1 to continue
 */
static int cccsp_namemap_wptr (compops_t *cops, tnode_t **nodep, map_t *map)
{
	tnode_t *bename;

	bename = tnode_getchook (*nodep, map->mapchook);
	if (!bename) {
		/* first occurance, create new name for it */
		bename = map->target->newname (*nodep, NULL, map, 4, 0, 0, 0, 4, 0);

		tnode_setchook (*nodep, map->mapchook, (void *)bename);
		*nodep = bename;
	} else {
		tnode_t *tname = map->target->newnameref (bename, map);

		*nodep = tname;
	}

	return 0;
}
/*}}}*/
/*{{{  static tnode_t *cccsp_gettype_wptr (langops_t *lops, tnode_t *node, tnode_t *default_type)*/
/*
 *	gets the type of a special WPTR node (trivial)
 */
static tnode_t *cccsp_gettype_wptr (langops_t *lops, tnode_t *node, tnode_t *default_type)
{
	return tnode_nthsubof (node, 0);
}
/*}}}*/
/*{{{  static int cccsp_getname_wptr (langops_t *lops, tnode_t *node, char **str)*/
/*
 *	gets the name of a special WPTR node (trivial)
 */
static int cccsp_getname_wptr (langops_t *lops, tnode_t *node, char **str)
{
	cccsp_wptrhook_t *whook = (cccsp_wptrhook_t *)tnode_nthhookof (node, 0);

	if (*str) {
		sfree (*str);
	}
	*str = string_dup (whook->name);
	return 0;
}
/*}}}*/

/*{{{  static int cccsp_namemap_workspace (compops_t *cops, tnode_t **nodep, map_t *map)*/
/*
 *	does name-mapping for the slightly special WORKSPACE node
 *	returns 0 to stop walk, 1 to continue
 */
static int cccsp_namemap_workspace (compops_t *cops, tnode_t **nodep, map_t *map)
{
	tnode_t *bename;

	bename = tnode_getchook (*nodep, map->mapchook);
	if (!bename) {
		/* first occurance, create new name for it.  FIXME: sizes are busted */
		bename = map->target->newname (*nodep, NULL, map, 4, 0, 0, 0, 4, 0);

		tnode_setchook (*nodep, map->mapchook, (void *)bename);
		*nodep = bename;
	} else {
		tnode_t *tname = map->target->newnameref (bename, map);

		*nodep = tname;
	}

	return 0;
}
/*}}}*/
/*{{{  static tnode_t *cccsp_gettype_workspace (langops_t *lops, tnode_t *node, tnode_t *default_type)*/
/*
 *	gets the type of a special WORKSPACE node (trivial)
 */
static tnode_t *cccsp_gettype_workspace (langops_t *lops, tnode_t *node, tnode_t *default_type)
{
	return tnode_nthsubof (node, 0);
}
/*}}}*/
/*{{{  static int cccsp_getname_workspace (langops_t *lops, tnode_t *node, char **str)*/
/*
 *	gets the name of a special WORKSPACE node (trivial)
 */
static int cccsp_getname_workspace (langops_t *lops, tnode_t *node, char **str)
{
	cccsp_workspacehook_t *whook = (cccsp_workspacehook_t *)tnode_nthhookof (node, 0);

	if (*str) {
		sfree (*str);
	}
	*str = string_dup (whook->name);
	return 0;
}
/*}}}*/

/*{{{  static int cccsp_getctypeof_type (langops_t *lops, tnode_t *t, char **str)*/
/*
 *	gets the C type of a WORKSPACE node (trivial)
 */
static int cccsp_getctypeof_type (langops_t *lops, tnode_t *t, char **str)
{
	cccsp_priv_t *kpriv = (cccsp_priv_t *)cccsp_target.priv;
	char *lstr;

	if (t->tag == kpriv->tag_WPTRTYPE) {
		lstr = string_dup ("Workspace");
	} else if (t->tag == kpriv->tag_WORKSPACETYPE) {
		lstr = string_dup ("word");
	} else {
		nocc_internal ("cccsp_getctypeof_type(): unhandled [%s]", t->tag->name);
		return 0;
	}

	if (*str) {
		sfree (*str);
	}
	*str = lstr;

	return 0;
}
/*}}}*/

/*{{{  static int cccsp_lpreallocate_utype (compops_t *cops, tnode_t *node, cccsp_preallocate_t *cpa)*/
/*
 *	does pre-allocation for a UTYPE node
 *	returns 0 to stop walk, 1 to continue
 */
static int cccsp_lpreallocate_utype (compops_t *cops, tnode_t *node, cccsp_preallocate_t *cpa)
{
	return 1;
}
/*}}}*/
/*{{{  static int cccsp_lcodegen_utype (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	does code-generation for a UTYPE node
 *	returns 0 to stop walk, 1 to continue
 */
static int cccsp_lcodegen_utype (compops_t *cops, tnode_t *node, codegen_t *cgen)
{
	cccsp_utypehook_t *uthook = (cccsp_utypehook_t *)tnode_nthhookof (node, 0);

	codegen_ssetindent (cgen);
	codegen_write_fmt (cgen, "typedef struct TAG_%s {\n", uthook->name);
	cgen->indent++;
	codegen_subcodegen (tnode_nthsubof (node, 0), cgen);
	cgen->indent--;
	codegen_write_fmt (cgen, "} %s;\n\n", uthook->name);

	return 0;
}
/*}}}*/
/*{{{  static int cccsp_getctypeof_utype (langops_t *lops, tnode_t *t, char **str)*/
/*
 *	gets the C type of a UTYPE node (trivial)
 */
static int cccsp_getctypeof_utype (langops_t *lops, tnode_t *t, char **str)
{
	cccsp_priv_t *kpriv = (cccsp_priv_t *)cccsp_target.priv;
	char *lstr;

	if (t->tag == kpriv->tag_UTYPE) {
		cccsp_utypehook_t *uthook = (cccsp_utypehook_t *)tnode_nthhookof (t, 0);

		lstr = string_dup (uthook->name);
	} else {
		nocc_internal ("cccsp_getctypeof_utype(): unhandled [%s]", t->tag->name);
		return 0;
	}

	if (*str) {
		sfree (*str);
	}
	*str = lstr;

	return 0;
}
/*}}}*/

/*{{{  static int cccsp_lcodegen_indexnode (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	does code-generation for an index-node (arraysub/recordsub)
 *	returns 0 to stop walk, 1 to continue
 */
static int cccsp_lcodegen_indexnode (compops_t *cops, tnode_t *node, codegen_t *cgen)
{
	cccsp_priv_t *kpriv = (cccsp_priv_t *)cgen->target->priv;
	cccsp_indexhook_t *idh = (cccsp_indexhook_t *)tnode_nthhookof (node, 0);

	if (node->tag == kpriv->tag_ARRAYSUB) {
		codegen_subcodegen (tnode_nthsubof (node, 0), cgen);
		codegen_write_fmt (cgen, "[");
		codegen_subcodegen (tnode_nthsubof (node, 1), cgen);
		codegen_write_fmt (cgen, "]");
	} else if (node->tag == kpriv->tag_RECORDSUB) {
		codegen_subcodegen (tnode_nthsubof (node, 0), cgen);
		codegen_write_fmt (cgen, "->");
		codegen_subcodegen (tnode_nthsubof (node, 1), cgen);
	}
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

	if (tnode_newcompop ("cccsp:dcg", COPS_INVALID, 2, INTERNAL_ORIGIN) < 0) {
		nocc_serious ("cccsp_init(): failed to add \"cccsp:dcg\" compiler operation");
		return 1;
	}
	if (tnode_newcompop ("cccsp:dcgfix", COPS_INVALID, 1, INTERNAL_ORIGIN) < 0) {
		nocc_serious ("cccsp_init(): failed to add \"cccsp:dcgfix\" compiler operation");
		return 1;
	}

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

/*{{{  static int cccsp_run_cc (char *cmd)*/
/*
 *	runs the C compiler to build something -- mostly a wrapper around fork/exec here
 *	returns 0 on success, non-zero on failure
 */
static int cccsp_run_cc (char *cmd)
{
	char **bits = split_string (cmd, 1);
	int fres;
	int rval = 0;
	int i;

#if 0
	/* FIXME: may be just "gcc" or similar for the compiler, not a full path */
	if (fhandle_access (bits[0], X_OK)) {
		/* cannot find compiler executable */
		if (compopts.verbose) {
			nocc_warning ("cannot execute compiler [%s]", bits[0]);
		}
		rval = -1;
		goto out_free;
	}
#endif

	fres = fork ();
	if (fres < 0) {
		nocc_error ("cannot execute compiler [%s], fork() failed: %s", bits[0], strerror (errno));
		rval = -1;
		goto out_free;
	}

	if (fres == 0) {
		/* we are the child */
		execvp (bits[0], bits);
		_exit (1);			/* failed if we get this far */
	} else {
		pid_t wres;
		int status = 0;

		wres = waitpid (fres, &status, 0);
		if (wres < 0) {
			nocc_serious ("cccsp_run_cc(): wait() failed: %s", strerror (errno));
			rval = -1;
			goto out_free;
		} else if (!WIFEXITED (status)) {
			nocc_serious ("cccsp_run_cc(): child process wait()ed, but didn't exit normally?, status = 0x%8.8x", (unsigned int)status);
			rval = -1;
			goto out_free;
		} else if (WEXITSTATUS (status)) {
			/* bad, so return error */
			rval = -1;
			goto out_free;
		}
		/* else, assume all was good */
	}

out_free:
	for (i=0; bits[i]; i++) {
		if (bits[i]) {
			sfree (bits[i]);
			bits[i] = NULL;
		}
	}
	sfree (bits);

	return rval;
}
/*}}}*/
/*{{{  static int cccsp_cc_compile_cpass (tnode_t **treeptr, lexfile_t *srclf, target_t *target)*/
/*
 *	compiler pass for CC-compile
 *	returns 0 on success, non-zero on failure
 */
static int cccsp_cc_compile_cpass (tnode_t **treeptr, lexfile_t *srclf, target_t *target)
{
	cccsp_priv_t *kpriv = (cccsp_priv_t *)target->priv;
	char *ccodefile;
	char *objfname, *sfifname, *ch;
	char *ccmd, *eincl;
	char *langlib = NULL;
	char *sfifiles = NULL;

	ccodefile = (char *)tnode_getchook (*treeptr, cccspoutfilehook);
	if (!ccodefile) {
		nocc_error ("cccsp_cc_compile_cpass(): did not find cccsp:outfile hook at top-level [%s]", (*treeptr)->tag->name);
		return -1;
	}

	/*{{{  sort out output file-name and .su file-name*/
	for (ch = ccodefile + (strlen (ccodefile) - 1); (ch > ccodefile) && (ch[-1] != '.'); ch--);
	if (compopts.notmainmodule) {
		/* generate object name */
		if (ch == ccodefile) {
			/* slightly odd perhaps */
			objfname = string_fmt ("%s.o", ccodefile);
			sfifname = string_fmt ("%s.su", ccodefile);
		} else {
			int plen = (int)(ch - ccodefile);

			objfname = string_fmt ("%s.o", ccodefile);		/* will be long enough */
			strcpy (objfname + plen, "o");
			sfifname = string_fmt ("%s.su", ccodefile);
			strcpy (sfifname + plen, "su");
		}
	} else {
		/* generate path name */
		if (ch == ccodefile) {
			/* slightly odd perhaps, cannot replace with same though */
			objfname = string_fmt ("%s.out", ccodefile);
			sfifname = string_fmt ("%s.out.su", ccodefile);
		} else {
			int plen = (int)(ch - ccodefile);

			objfname = string_ndup (ccodefile, plen - 1);
			sfifname = string_fmt ("%s.su", objfname);
		}
	}

	/*}}}*/
	/*{{{  find out where verb-header.h lives*/
	if (!DA_CUR (compopts.epath)) {
		eincl = string_dup ("");
	} else {
		int i;

		eincl = NULL;
		for (i=0; !eincl && (i<DA_CUR (compopts.epath)); i++) {
			/* look for where cccsp/verb-header.h lives */
			char *tmpstr = string_fmt ("%s/cccsp/verb-header.h", DA_NTHITEM (compopts.epath, i));

			if (!fhandle_access (tmpstr, R_OK)) {
				/* this directory for includes */
				eincl = string_fmt ("-I%s", DA_NTHITEM (compopts.epath, i));
			}
			sfree (tmpstr);
		}
		if (!eincl) {
			nocc_serious ("cccsp_cc_compile_cpass(): failed to find where cccsp/verb-header.h lives, giving up..");
			return -1;
		}
	}

	/*}}}*/
	/*{{{  find out where language libraries are*/
	/* Note: if we're building an executable, need to figure out where language-specific library parts might be */
	if (!compopts.notmainmodule) {
		int i;
		char **langlibs_obj;
		char **langlibs_src;

		if (!srclf->parser) {
			nocc_error ("cccsp_cc_compile_cpass(): did not find a parser structure for src [%s]", srclf->fnptr);
			return -1;
		} else if (!srclf->parser->getlanglibs) {
			nocc_error ("cccsp_cc_compile_cpass(): expected to find language libraries, but unsupported by parser");
			return -1;
		}

		langlibs_obj = srclf->parser->getlanglibs (target, 0);
		langlibs_src = srclf->parser->getlanglibs (target, 1);

		for (i=0; langlibs_obj[i]; i++) {
			/*{{{  for each object (and maybe source)*/
			int j;
			char *found_obj = NULL;
			char *found_src = NULL;
			char *found_sfi = NULL;

			for (j=0; !found_obj && (j<DA_CUR (compopts.epath)); j++) {
				char *tmpstr = string_fmt ("%s/%s", DA_NTHITEM (compopts.epath, j), langlibs_obj[i]);

				if (!fhandle_access (tmpstr, R_OK)) {
					/* this exists, use it */
					found_obj = string_dup (tmpstr);
				}
				sfree (tmpstr);
			}
			if (langlibs_src[i]) {
				if (found_obj) {
					/*{{{  look for source in the same place first*/
					int endlen = strlen (langlibs_obj[i]);
					char *t2str = string_dup (found_obj);
					int slen = strlen (t2str);
					char *tmpstr;
					
					t2str[slen - endlen] = '\0';
					tmpstr = string_fmt ("%s%s", t2str, langlibs_src[i]);
					sfree (t2str);
					if (!fhandle_access (tmpstr, R_OK)) {
						/* this source file */
						found_src = string_dup (tmpstr);
					}
					sfree (tmpstr);

					/*}}}*/
				}

				/* else, look everywhere for source */
				for (j=0; !found_src && (j<DA_CUR (compopts.epath)); j++) {
					char *tmpstr = string_fmt ("%s/%s", DA_NTHITEM (compopts.epath, j), langlibs_src[i]);

					if (!fhandle_access (tmpstr, R_OK)) {
						/* exists, use it */
						found_src = string_dup (tmpstr);
					}
					sfree (tmpstr);
				}

				if (found_src && found_obj) {
					/*{{{  if source and object are in different places, clear object if not exist */
					int endlen = strlen (langlibs_src[i]);
					int slen = strlen (found_src) - endlen;

					if (!strncmp (found_src, found_obj, slen)) {
						/* in same place anyway, both exist, continue */
					} else {
						char *t2str = string_dup (found_src);
						char *tmpstr;

						t2str[slen - endlen] = '\0';
						tmpstr = string_fmt ("%s%s", t2str, langlibs_obj[i]);
						sfree (t2str);

						if (!fhandle_access (tmpstr, R_OK)) {
							/* object does exist here, so use that in preference */
							sfree (found_obj);
							found_obj = string_dup (tmpstr);
						} else {
							/* object does not exist here, so create */
							sfree (found_obj);
							found_obj = NULL;
						}
						sfree (tmpstr);
					}

					/*}}}*/
				}
			}

			/*{{{  see if there is an .su alongside the .o*/
			{
				char *objf;

				if (found_obj) {
					objf = string_dup (found_obj);
				} else if (found_src) {
					int endlen = strlen (langlibs_src[i]);
					int slen = strlen (found_src);

					objf = string_fmt ("%s%s", found_src, langlibs_obj[i]);
					strcpy (objf + (slen - endlen), langlibs_obj[i]);
				} else {
					objf = NULL;
				}

				if (objf) {
					char *ch;

					for (ch = objf + (strlen (objf) - 1); (ch > objf) && (ch[-1] != '.'); ch--);
					if (ch > objf) {
						*ch = '\0';
						found_sfi = string_fmt ("%ssu", objf);
					} else {
						found_sfi = string_fmt ("%s.su", objf);
					}
				}
			}
			/*}}}*/

#if 0
fhandle_printf (FHAN_STDERR, "here: found_src=[%s] found_obj=[%s] found_sfi=[%s]\n", found_src ?: "", found_obj ?: "", found_sfi ?: "");
#endif
			if (found_src && (!found_obj || (fhandle_cnewer (found_src, found_obj) > 0))) {
				char *xcmd;
				
				if (!found_obj) {
					int endlen = strlen (langlibs_src[i]);
					int slen = strlen (found_src);

					found_obj = (char *)smalloc (slen + strlen (langlibs_obj[i]));		/* ample */
					strncpy (found_obj, found_src, slen - endlen);
					strcpy (found_obj + (slen - endlen), langlibs_obj[i]);
				}

				xcmd = string_fmt ("%s -fstack-usage -c %s %s %s -o %s %s", kpriv->cc_path, kpriv->cc_incpath, kpriv->cc_flags,
						eincl, found_obj, found_src);
				/* attempt to build object from source */
#if 0
fhandle_printf (FHAN_STDERR, "here: want to build library object with [%s]\n", xcmd);
#endif
				if (cccsp_run_cc (xcmd)) {
					nocc_error ("failed to compile [%s] to [%s]", found_src, found_obj);
					return -1;
				} else if (fhandle_access (found_obj, R_OK)) {
					nocc_error ("failed to generate something when compiling [%s] to [%s]", found_src, found_obj);
					return -1;
				}

				/* else, we should have compiled it okay! */
				if (compopts.verbose) {
					nocc_message ("cccsp generated library file %s", langlibs_obj[i]);
				}
				sfree (xcmd);
			}

			if (!fhandle_access (found_sfi, R_OK)) {
#if 0
fhandle_printf (FHAN_STDERR, "here: want to consume stack-info in [%s]\n", found_sfi);
#endif
				if (!sfifiles) {
					sfifiles = string_dup (found_sfi);
				} else {
					char *tmpstr = string_fmt ("%s %s", sfifiles, found_sfi);

					sfree (sfifiles);
					sfifiles = tmpstr;
				}
			}

			/* assert: here found_obj is sensible */
			if (langlib) {
				char *tmpstr = string_fmt ("%s %s", langlib, found_obj);

				sfree (langlib);
				langlib = tmpstr;
			} else {
				langlib = found_obj;
				found_obj = NULL;
			}

			if (found_src) {
				sfree (found_src);
				found_src = NULL;
			}
			if (found_obj) {
				sfree (found_obj);
				found_obj = NULL;
			}
			if (found_sfi) {
				sfree (found_sfi);
				found_sfi = NULL;
			}
			/*}}}*/
		}

		if (!langlib) {
			nocc_serious ("cccsp_cc_compile_cpass(): failed to find language libraries, giving up..");
			return -1;
		}
	}
	/*}}}*/

#if 0
fhandle_printf (FHAN_STDERR, "cccsp_cc_compile_cpass(): ccodefile=[%s] objfname=[%s]\n", ccodefile, objfname);
#endif

	if (compopts.notmainmodule) {
		/* compile to object */
		ccmd = string_fmt ("%s -fstack-usage -c %s %s %s -o %s %s", kpriv->cc_path, kpriv->cc_incpath, kpriv->cc_flags, eincl, objfname, ccodefile);
	} else {
		ccmd = string_fmt ("%s -fstack-usage %s %s %s -o %s %s %s %s -lccsp %s", kpriv->cc_path, kpriv->cc_incpath, kpriv->cc_flags, eincl,
				objfname, ccodefile, kpriv->cc_libpath, kpriv->cc_ldflags, langlib);
	}

	/* do it! */
	if (cccsp_run_cc (ccmd)) {
		nocc_error ("failed to compile object/executable [%s]", objfname);
		return -1;
	}

	if (!fhandle_access (sfifname, R_OK)) {
		/* got the stack-usage file too, so add to list */
		if (sfifiles) {
			char *tmpstr = string_fmt ("%s %s", sfifiles, sfifname);

			sfree (sfifiles);
			sfifiles = tmpstr;
		} else {
			sfifiles = string_dup (sfifname);
		}
	}

#if 0
fhandle_printf (FHAN_STDERR, "cccsp_cc_compile_cpass(): sfifiles=[%s]\n", sfifiles ?: "");
#endif

	if (sfifiles) {
		tnode_setchook (*treeptr, cccspsfifilehook, (void*)sfifiles);
		sfifiles = NULL;
	}
	if (langlib) {
		sfree (langlib);
	}
	if (sfifname) {
		sfree (sfifname);
	}
	sfree (objfname);
	sfree (eincl);
	sfree (ccmd);

	return 0;
}
/*}}}*/
/*{{{  static int cccsp_cc_sfi_cpass (tnode_t **treeptr, lexfile_t *srclf, target_t *target)*/
/*
 *	compiler pass that collects up stack-frame information
 *	returns 0 on success, non-zero on error
 */
static int cccsp_cc_sfi_cpass (tnode_t **treeptr, lexfile_t *srclf, target_t *target)
{
	int i;
	char *apif = NULL;
	char *sfifiles = (char *)tnode_getchook (*treeptr, cccspsfifilehook);
	cccsp_dcg_t *dcg;

	cccsp_sfi_init ();

	/*{{{  find where the api-call-chain file lives and load it*/
	for (i=0; !apif && (i<DA_CUR (compopts.epath)); i++) {
		char *tmpstr = string_fmt ("%s/cccsp/api-call-chain", DA_NTHITEM (compopts.epath, i));

		if (!fhandle_access (tmpstr, R_OK)) {
			apif = string_dup (tmpstr);
		}
		sfree (tmpstr);
	}
	if (!apif) {
		nocc_error ("failed to find cccsp/api-call-chain file..");
		return -1;
	} else {
		cccsp_sfi_loadcalls (apif);
	}

	/*}}}*/
	/*{{{  construct the direct-call-graph (tree) for stack allocation*/
	dcg = (cccsp_dcg_t *)smalloc (sizeof (cccsp_dcg_t));

	dcg->target = target;
	dcg->thisfcn = NULL;

	tnode_prewalktree (*treeptr, cccsp_prewalktree_cccspdcg, (void *)dcg);

	/*}}}*/
	/*{{{  if we have stack-information, collect it*/
	if (sfifiles) {
		char **bits = split_string (sfifiles, 1);
		int j;

		for (j=0; bits[j]; j++) {
			cccsp_sfi_loadusage (bits[j]);
			sfree (bits[j]);
		}
		sfree (bits);
	}

	/*}}}*/
	/*{{{  calculate required allocations*/
	if (cccsp_sfi_calc_alloc ()) {
		nocc_error ("failed to calculate allocations, giving up..");
		cccsp_sfi_dumptable (FHAN_STDERR);
		return -1;
	}

	/*}}}*/
	/*{{{  fixup information in the tree*/
	tnode_prewalktree (*treeptr, cccsp_prewalktree_cccspdcgfix, NULL);

	/*}}}*/

	cccsp_sfi_dumptable (FHAN_STDERR);

	return 0;
}
/*}}}*/
/*{{{  static int cccsp_get_kroc_env (char *flag, char **target)*/
/*
 *	attempts to run the 'kroc' script to extract various C compiler information
 *	returns 0 on success, non-zero on error
 */
static int cccsp_get_kroc_env (char *flag, char **target)
{
	char **abits = (char **)smalloc (3 * sizeof (char *));
	int fres;
	int pipe_fd[2];
	int rval = 0;

	abits[0] = string_dup (compopts.cccsp_kroc);
	abits[1] = string_dup (flag);
	abits[2] = NULL;

	if (pipe (pipe_fd)) {
		nocc_serious ("cccsp_get_kroc_env(): pipe() failed: %s", strerror (errno));
		return -1;
	}

	fres = fork ();
	if (fres == -1) {
		nocc_serious ("cccsp_get_kroc_env(): fork() failed: %s", strerror (errno));
		return -1;
	}
	if (fres == 0) {
		/* we are the child process */
		int saved_stdout = dup (1);
		int saved_stderr = dup (2);

		close (pipe_fd[0]);			/* close read-end */
		dup2 (pipe_fd[1], 1);			/* make stdout = pipe */
		dup2 (pipe_fd[1], 2);			/* make stderr = pipe */
		execv (compopts.cccsp_kroc, abits);

		/* if we get here, it's gone wrong -- recover stdout/stderr */
		dup2 (saved_stdout, 1);
		dup2 (saved_stderr, 2);

		nocc_internal ("cccsp_get_kroc_env(): failed to run %s: %s", compopts.cccsp_kroc, strerror (errno));
		_exit (1);
	} else {
		/* we are the parent process */
		pid_t wres;
		int status = 0;
		char rbuf[1024];
		int rlen = 0;

		close (pipe_fd[1]);			/* close write-end */
		for (;;) {
			int r;

			r = read (pipe_fd[0], &(rbuf[rlen]), 1024 - rlen);
			if (r < 0) {
				nocc_serious ("cccsp_get_kroc_env(): read() from pipe failed: %s", strerror (errno));
				close (pipe_fd[0]);
				rval = -1;
				goto out_free;
			} else if (!r) {
				/* EOF: program done probably */
				break;			/* for() */
			} else {
				rlen += r;
				if (rlen >= 1024) {
					break;		/* for() */
				}
			}
		}
		close (pipe_fd[0]);			/* done with read-end */

		if (rlen > 0) {
			int i;

			/* truncate string at first/single line */
			for (i=0; (i<rlen) && (rbuf[i] != '\n') && (rbuf[i] != '\r') && (rbuf[i] != '\0'); i++);
			rbuf[i] = '\0';

			if (*target) {
				sfree (*target);
			}
			*target = string_dup (rbuf);
		} else {
			/* nothing received */
			rval = -1;
		}
#if 0
fhandle_printf (FHAN_STDERR, "cccsp_get_kroc_env(): rlen = %d\n", rlen);
{ char *rxbuf = mkhexbuf ((unsigned char *)rbuf, rlen);
fhandle_printf (FHAN_STDERR, "cccsp_get_kroc_env(): rbuf = [%s]\n", rxbuf);
sfree (rxbuf); }
#endif

		/* now, wait for the child process */
		wres = waitpid (fres, &status, 0);
		if (wres < 0) {
			nocc_serious ("cccsp_get_kroc_env(): wait() failed: %s", strerror (errno));
			rval = -1;
			goto out_free;
		} else if (!WIFEXITED (status)) {
			nocc_serious ("cccsp_get_kroc_env(): child process wait()ed, but didn't exit normally?, status = 0x%8.8x", (unsigned int)status);
			rval = -1;
			goto out_free;
		}
		/* else, assume all was good; not bothered about kroc's exit code */
	}

	/* only reached in the parent process */
out_free:
	sfree (abits[0]);
	sfree (abits[1]);
	sfree (abits);

	return rval;
}
/*}}}*/
/*{{{  static int cccsp_init_kroc_env (cccsp_priv_t *kpriv)*/
/*
 *	initialises specific settings that KRoC provides (cc and flags)
 *	returns 0 on success, non-zero on error
 */
static int cccsp_init_kroc_env (cccsp_priv_t *kpriv)
{
	if (cccsp_get_kroc_env ("--cc", &kpriv->cc_path) ||
			cccsp_get_kroc_env ("--cflags", &kpriv->cc_flags) ||
			cccsp_get_kroc_env ("--ccincpath", &kpriv->cc_incpath) ||
			cccsp_get_kroc_env ("--cclibpath", &kpriv->cc_libpath) ||
			cccsp_get_kroc_env ("--ldflags", &kpriv->cc_ldflags)) {
		return -1;
	}
	if (compopts.verbose) {
		nocc_message ("cccsp using C compiler [%s]", kpriv->cc_path);
		if (compopts.verbose > 1) {
			nocc_message ("cccsp C compiler flags: cflags=[%s] incpath=[%s] libpath=[%s] ldflags=[%s]",
					kpriv->cc_flags, kpriv->cc_incpath, kpriv->cc_libpath, kpriv->cc_ldflags);
		}
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

	cccsp_bepass = 0;

	kpriv = (cccsp_priv_t *)smalloc (sizeof (cccsp_priv_t));
	kpriv->lastfile = NULL;
	kpriv->last_toplevelname = NULL;
	kpriv->wptr_count = 0;

	kpriv->cc_path = NULL;
	kpriv->cc_flags = NULL;
	kpriv->cc_incpath = NULL;
	kpriv->cc_libpath = NULL;
	kpriv->cc_ldflags = NULL;

	kpriv->tag_ADDROF = NULL;
	kpriv->tag_NWORDSOF = NULL;
	kpriv->tag_LABEL = NULL;
	kpriv->tag_LABELREF = NULL;
	kpriv->tag_GOTO = NULL;

	kpriv->tag_WPTR = NULL;
	kpriv->tag_WORKSPACE = NULL;
	kpriv->tag_WPTRTYPE = NULL;
	kpriv->tag_WORKSPACETYPE = NULL;
	kpriv->tag_UTYPE = NULL;
	kpriv->tag_ARRAYSUB = NULL;
	kpriv->tag_RECORDSUB = NULL;

	kpriv->wsfixuphook = tnode_lookupornewchook ("cccsp:wsfixup");

	target->priv = (void *)kpriv;

	cccsp_init_options (kpriv);

	if (!compopts.cccsp_kroc) {
		nocc_warning ("cccsp: path to kroc not set, will not be able to make object files");
	} else if (access (compopts.cccsp_kroc, X_OK)) {
		nocc_warning ("cccsp: cannot execute kroc script: %s", strerror (errno));
	} else if (cccsp_init_kroc_env (kpriv)) {
		nocc_warning ("cccsp: failed to get compiler information from kroc script [%s]", compopts.cccsp_kroc);
		/* clear out, throwing memory away possibly.. */
		kpriv->cc_path = NULL;
		kpriv->cc_flags = NULL;
		kpriv->cc_incpath = NULL;
		kpriv->cc_libpath = NULL;
		kpriv->cc_ldflags = NULL;
	}

	/*{{{  new compiler hooks*/
	cccspoutfilehook = tnode_lookupornewchook ("cccsp:outfile");
	cccspoutfilehook->chook_copy = cccsp_outfilehook_copy;
	cccspoutfilehook->chook_free = cccsp_outfilehook_free;
	cccspoutfilehook->chook_dumptree = cccsp_outfilehook_dumptree;

	cccspsfifilehook = tnode_lookupornewchook ("cccsp:sfifile");
	cccspsfifilehook->chook_copy = cccsp_sfifilehook_copy;
	cccspsfifilehook->chook_free = cccsp_sfifilehook_free;
	cccspsfifilehook->chook_dumptree = cccsp_sfifilehook_dumptree;

	/*}}}*/
	/*{{{  sort out some new compiler passes if we can*/
	if (kpriv->cc_path) {
		int stopat;

		stopat = nocc_laststopat() + 1;
		opts_add ("stop-cc-compile", '\0', cccsp_opthandler_stopat, (void *)stopat, "1stop after CC compile pass");
		if (nocc_addcompilerpass ("cc-compile", INTERNAL_ORIGIN, "codegen", 0, (int (*)(void *))cccsp_cc_compile_cpass,
				CPASS_TREEPTR | CPASS_LEXFILE | CPASS_TARGET, stopat, NULL)) {
			nocc_serious ("cccsp_target_init(): failed to add \"cc-compile\" compiler pass");
			return 1;
		}

		stopat = nocc_laststopat() + 1;
		opts_add ("stop-cc-sfi", '\0', cccsp_opthandler_stopat, (void *)stopat, "1stop after CC stack-frame info");
		if (nocc_addcompilerpass ("cc-sfi", INTERNAL_ORIGIN, "cc-compile", 0, (int (*)(void *))cccsp_cc_sfi_cpass,
				CPASS_TREEPTR | CPASS_LEXFILE | CPASS_TARGET, stopat, NULL)) {
			nocc_serious ("cccsp_target_init(): failed to add \"cc-sfi\" compiler pass");
			return 1;
		}
	}

	/*}}}*/

	/* setup back-end nodes */
	/*{{{  cccsp:name -- CCCSPNAME*/
	i = -1;
	tnd = tnode_newnodetype ("cccsp:name", &i, 2, 0, 1, TNF_NONE);		/* subnodes: original name, in-scope body (NULL); hooks: cccsp_namehook_t */
	tnd->hook_dumptree = cccsp_namehook_dumptree;
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "lpreallocate", 2, COMPOPTYPE (cccsp_lpreallocate_name));
	tnode_setcompop (cops, "lcodegen", 2, COMPOPTYPE (cccsp_lcodegen_name));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "bytesfor", 2, LANGOPTYPE (cccsp_bytesfor_name));
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
	tnode_setcompop (cops, "lpreallocate", 2, COMPOPTYPE (cccsp_lpreallocate_block));
	tnode_setcompop (cops, "lprecode", 2, COMPOPTYPE (cccsp_lprecode_block));
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
	/*{{{  cccsp:modifier -- CCCSPADDROF, CCCSPNWORDSOF*/
	i = -1;
	tnd = tnode_newnodetype ("cccsp:modifier", &i, 1, 0, 0, TNF_NONE);	/* subnodes: operand */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "lcodegen", 2, COMPOPTYPE (cccsp_lcodegen_modifier));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnd->lops = lops;

	i = -1;
	kpriv->tag_ADDROF = tnode_newnodetag ("CCCSPADDROF", &i, tnd, NTF_NONE);
	i = -1;
	kpriv->tag_NWORDSOF = tnode_newnodetag ("CCCSPNWORDSOF", &i, tnd, NTF_NONE);

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
	/*{{{  cccsp:wptr -- CCCSPWPTR*/
	i = -1;
	tnd = tnode_newnodetype ("cccsp:wptr", &i, 1, 0, 1, TNF_NONE);			/* subnodes: type; hooks: cccsp_wptrhook_t */
	tnd->hook_dumptree = cccsp_wptrhook_dumptree;
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (cccsp_namemap_wptr));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "gettype", 2, LANGOPTYPE (cccsp_gettype_wptr));
	tnode_setlangop (lops, "getname", 2, LANGOPTYPE (cccsp_getname_wptr));
	tnd->lops = lops;

	i = -1;
	kpriv->tag_WPTR = tnode_newnodetag ("CCCSPWPTR", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  cccsp:workspace -- CCCSPWORKSPACE*/
	i = -1;
	tnd = tnode_newnodetype ("cccsp:workspace", &i, 1, 0, 1, TNF_NONE);		/* subnodes: type; hooks: cccsp_workspacehook_t */
	tnd->hook_dumptree = cccsp_workspacehook_dumptree;
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (cccsp_namemap_workspace));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "gettype", 2, LANGOPTYPE (cccsp_gettype_workspace));
	tnode_setlangop (lops, "getname", 2, LANGOPTYPE (cccsp_getname_workspace));
	tnd->lops = lops;

	i = -1;
	kpriv->tag_WORKSPACE = tnode_newnodetag ("CCCSPWORKSPACE", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  cccsp:type -- CCCSPWPTRTYPE, CCCSPWORKSPACETYPE*/
	i = -1;
	tnd = tnode_newnodetype ("cccsp:type", &i, 0, 0, 0, TNF_NONE);
	cops = tnode_newcompops ();
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "getctypeof", 2, LANGOPTYPE (cccsp_getctypeof_type));
	tnd->lops = lops;

	i = -1;
	kpriv->tag_WPTRTYPE = tnode_newnodetag ("CCCSPWPTRTYPE", &i, tnd, NTF_NONE);
	i = -1;
	kpriv->tag_WORKSPACETYPE = tnode_newnodetag ("CCCSPWORKSPACETYPE", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  cccsp:utype -- CCCSPUTYPE*/
	i = -1;
	tnd = tnode_newnodetype ("cccsp:utype", &i, 1, 0, 1, TNF_NONE);		/* subnodes: be-type-tree; hooks: cccsp_utypehook_t */
	tnd->hook_dumptree = cccsp_utypehook_dumptree;
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "lpreallocate", 2, COMPOPTYPE (cccsp_lpreallocate_utype));
	tnode_setcompop (cops, "lcodegen", 2, COMPOPTYPE (cccsp_lcodegen_utype));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "getctypeof", 2, LANGOPTYPE (cccsp_getctypeof_utype));
	tnd->lops = lops;

	i = -1;
	kpriv->tag_UTYPE = tnode_newnodetag ("CCCSPUTYPE", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  cccsp:indexnode -- ARRAYSUB, RECORDSUB*/
	i = -1;
	tnd = tnode_newnodetype ("cccsp:indexnode", &i, 2, 0, 1, TNF_NONE);	/* subnodes: base, index; hooks: cccsp_indexhook_t */
	tnd->hook_dumptree = cccsp_indexhook_dumptree;
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "lcodegen", 2, COMPOPTYPE (cccsp_lcodegen_indexnode));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnd->lops = lops;

	i = -1;
	kpriv->tag_ARRAYSUB = tnode_newnodetag ("CCCSPARRAYSUB", &i, tnd, NTF_NONE);
	i = -1;
	kpriv->tag_RECORDSUB = tnode_newnodetag ("CCCSPRECORDSUB", &i, tnd, NTF_NONE);

	/*}}}*/

	target->initialised = 1;
	return 0;
}
/*}}}*/

