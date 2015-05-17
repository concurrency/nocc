/*
 *	guppy_decls.c -- variables and other named things
 *	Copyright (C) 2010-2015 Fred Barnes <frmb@kent.ac.uk>
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

#include "nocc.h"
#include "support.h"
#include "version.h"
#include "fhandle.h"
#include "origin.h"
#include "symbols.h"
#include "keywords.h"
#include "lexer.h"
#include "lexpriv.h"
#include "tnode.h"
#include "parser.h"
#include "dfa.h"
#include "dfaerror.h"
#include "parsepriv.h"
#include "guppy.h"
#include "feunit.h"
#include "fcnlib.h"
#include "names.h"
#include "scope.h"
#include "prescope.h"
#include "library.h"
#include "typecheck.h"
#include "constprop.h"
#include "precheck.h"
#include "usagecheck.h"
#include "map.h"
#include "target.h"
#include "transputer.h"
#include "codegen.h"
#include "langops.h"
#include "fetrans.h"
#include "betrans.h"
#include "metadata.h"
#include "tracescheck.h"
#include "mobilitycheck.h"
#include "cccsp.h"



/*}}}*/
/*{{{  private types*/
#define FPARAM_IS_NONE 0
#define FPARAM_IS_VAL 1
#define FPARAM_IS_RES 2
#define FPARAM_IS_INIT 3

typedef struct TAG_fparaminfo {
	int flags;			/* flags of the above (FPARAM_IS_...) */
} fparaminfo_t;

#define DTN_BITSIZE	(4)

typedef struct TAG_deftypename {
	char *name;			/* type name */
} deftypename_t;


/*}}}*/
/*{{{  private data*/

STATICSTRINGHASH(deftypename_t *, deftypenames, DTN_BITSIZE);

static ntdef_t *guppy_testtruetag, *guppy_testfalsetag;

/*}}}*/


/*{{{  static fparaminfo_t *guppy_newfparaminfo (int flags)*/
/*
 *	creates a new fparaminfo_t structure
 */
static fparaminfo_t *guppy_newfparaminfo (int flags)
{
	fparaminfo_t *fpi = (fparaminfo_t *)smalloc (sizeof (fparaminfo_t));

	fpi->flags = flags;

	return fpi;
}
/*}}}*/
/*{{{  static void guppy_freefparaminfo (fparaminfo_t *fpi)*/
/*
 *	frees a fparaminfo_t structure
 */
static void guppy_freefparaminfo (fparaminfo_t *fpi)
{
	if (!fpi) {
		nocc_internal ("guppy_freefparaminfo(): NULL argument!");
		return;
	}
	sfree (fpi);
	return;
}
/*}}}*/

/*{{{  static void guppy_fparaminfo_hook_free (void *hook)*/
/*
 *	frees a fparaminfo_t hook
 */
static void guppy_fparaminfo_hook_free (void *hook)
{
	guppy_freefparaminfo ((fparaminfo_t *)hook);
}
/*}}}*/
/*{{{  static void *guppy_fparaminfo_hook_copy (void *hook)*/
/*
 *	copies a fparaminfo_t hook
 */
static void *guppy_fparaminfo_hook_copy (void *hook)
{
	fparaminfo_t *fpi = (fparaminfo_t *)hook;
	fparaminfo_t *nxt;

	if (!fpi) {
		return NULL;
	}
	nxt = guppy_newfparaminfo (fpi->flags);

	return (void *)nxt;
}
/*}}}*/
/*{{{  static void guppy_fparaminfo_hook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)*/
/*
 *	dump-tree for a fparaminfo hook
 */
static void guppy_fparaminfo_hook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)
{
	guppy_isetindent (stream, indent);
	if (!hook) {
		fhandle_printf (stream, "<fparaminfo value=\"(null)\" />\n");
	} else {
		fparaminfo_t *fpi = (fparaminfo_t *)hook;

		fhandle_printf (stream, "<fparaminfo flags=\"%d\" />\n",fpi->flags);
	}
}
/*}}}*/

/*{{{  static void guppy_rawnamenode_hook_free (void *hook)*/
/*
 *	frees a rawnamenode hook (name-bytes)
 */
static void guppy_rawnamenode_hook_free (void *hook)
{
	if (hook) {
		sfree (hook);
	}
	return;
}
/*}}}*/
/*{{{  static void *guppy_rawnamenode_hook_copy (void *hook)*/
/*
 *	copies a rawnamenode hook (name-bytes)
 */
static void *guppy_rawnamenode_hook_copy (void *hook)
{
	char *rawname = (char *)hook;

	if (rawname) {
		return string_dup (rawname);
	}
	return NULL;
}
/*}}}*/
/*{{{  static void guppy_rawnamenode_hook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)*/
/*
 *	dump-tree for rawnamenode hook (name-bytes)
 */
static void guppy_rawnamenode_hook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)
{
	guppy_isetindent (stream, indent);
	fhandle_printf (stream, "<rawnamenode value=\"%s\" />\n", hook ? (char *)hook : "(null)");
	return;
}
/*}}}*/

/*{{{  static void *guppy_checktypename (void *arg)*/
/*
 *	looks at a Name token and decides whether or not it's a type name
 *	pushes the token back into the lexer, returns test node
 */
static void *guppy_checktypename (void *arg)
{
	token_t *tok = (token_t *)arg;
	lexfile_t *lf = tok->origin;
	tnode_t *node;
	deftypename_t *dtn;

	if (!lf) {
		nocc_internal ("guppy_checktypename(): NULL origin for token [%s]", lexer_stokenstr (tok));
		return NULL;
	}
	if (tok->type != NAME) {
		nocc_internal ("guppy_checktypename(): token not NAME [%s]", lexer_stokenstr (tok));
		return NULL;
	}

	dtn = stringhash_lookup (deftypenames, tok->u.name);

	if (dtn) {
		node = tnode_create (guppy_testtruetag, SLOCI);
	} else {
		node = tnode_create (guppy_testfalsetag, SLOCI);
	}

#if 0
fhandle_printf (FHAN_STDERR, "guppy_checktypename(): here, name = [%s], node =\n", tok->u.name);
tnode_dumptree (node, 1, FHAN_STDERR);
#endif
	lexer_pushback (lf, tok);

	return (void *)node;
}
/*}}}*/

/*{{{  static int guppy_scopein_rawnamenode (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	scopes in a raw namenode (resolving free names)
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_scopein_rawnamenode (compops_t *cops, tnode_t **node, scope_t *ss)
{
	tnode_t *name = *node;
	char *rawname;
	name_t *sname = NULL;
	guppy_scope_t *gss = (guppy_scope_t *)ss->langpriv;

	if (name->tag != gup.tag_NAME) {
		scope_error (name, ss, "name not raw-name!");
		return 0;
	}
	rawname = tnode_nthhookof (name, 0);

	if (gss->resolve_nametype_first) {
		/* typically when looking to resolve in array/record subscriptions, or other similar things */
		sname = name_lookupss_nodetag (rawname, ss, gss->resolve_nametype_first);
	} else {
		sname = name_lookupss (rawname, ss);
	}

	if (sname) {
		/* resolved */
		*node = NameNodeOf (sname);
		tnode_free (name);

		if (gss && (ss->lexlevel > NameLexlevelOf (sname))) {
			int i;

			for (i=0; i<DA_CUR (gss->crosses); i++) {
				tnode_t *fvlist = DA_NTHITEM (gss->crosses, i);
				int fvll = DA_NTHITEM (gss->cross_lexlevels, i);

				if (NameLexlevelOf (sname) < fvll) {
					tnode_t **fvitems;
					int i, nfvitems;

					/* only add if it's not already here */
					fvitems = parser_getlistitems (fvlist, &nfvitems);
					for (i=0; i<nfvitems; i++) {
						if (fvitems[i] == NameNodeOf (sname)) {
							break;
						}
					}
					if (i == nfvitems) {
						parser_addtolist (fvlist, NameNodeOf (sname));
					}
				}
			}
		}
	} else {
		scope_error (name, ss, "unresolved name \"%s\"", rawname);
	}

	return 1;
}
/*}}}*/

/*{{{  static int guppy_fetrans15_namenode (compops_t *cops, tnode_t **nodep, guppy_fetrans15_t *fe15)*/
/*
 *	does fetrans1.5 on a name-node -- should trash if expecting a process (could be leftover temporary by fetrans1)
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_fetrans15_namenode (compops_t *cops, tnode_t **nodep, guppy_fetrans15_t *fe15)
{
	if (fe15->expt_proc) {
#if 0
fhandle_printf (FHAN_STDERR, "guppy_fetrans15_namenode(): name, but expecting process! node is:\n");
tnode_dumptree (*nodep, 1, FHAN_STDERR);
#endif
		tnode_warning (*nodep, "result lost from instance");
		*nodep = tnode_createfrom (gup.tag_SKIP, *nodep);
	}

	return 0;
}
/*}}}*/
/*{{{  static int guppy_namemap_namenode (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	does name-mapping for a lone name-reference.
 *	returns 0 to stop walk, 1 to continue.
 */
static int guppy_namemap_namenode (compops_t *cops, tnode_t **node, map_t *map)
{
	tnode_t *bename = tnode_getchook (*node, map->mapchook);
	cccsp_mapdata_t *cmap = (cccsp_mapdata_t *)map->hook;

#if 0
fhandle_printf (FHAN_STDERR, "guppy_namemap_namenode(): bename = 0x%8.8x, target_indir=%d, *node =\n",
		(unsigned int)bename, cmap->target_indir);
tnode_dumptree (*node, 1, FHAN_STDERR);
#endif
	if (bename) {
		tnode_t *tname = map->target->newnameref (bename, map);

		*node = tname;
		if (cmap->target_indir) {
			cccsp_set_indir (tname, cmap->target_indir, map->target);
		}
	}
	return 0;
}
/*}}}*/
/*{{{  static int guppy_codegen_namenode (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	does code-generation for a lone name-reference (usually a function name).
 *	returns 0 to stop walk, 1 to continue.
 */
static int guppy_codegen_namenode (compops_t *cops, tnode_t *node, codegen_t *cgen)
{
	name_t *name = tnode_nthnameof (node, 0);

	if (node->tag == gup.tag_NPFCNDEF) {
		char *fname = cccsp_make_entryname (NameNameOf (name), 1);

		codegen_write_fmt (cgen, "%s", fname);
		sfree (fname);
	}
	return 0;
}
/*}}}*/
/*{{{  static tnode_t *guppy_gettype_namenode (langops_t *lops, tnode_t *node, tnode_t *default_type)*/
/*
 *	returns the type of a name-node (trivial)
 */
static tnode_t *guppy_gettype_namenode (langops_t *lops, tnode_t *node, tnode_t *default_type)
{
	name_t *name = tnode_nthnameof (node, 0);

	if (!name) {
		nocc_fatal ("guppy_gettype_namenode(): NULL name!");
		return NULL;
	}

	/* special case for enums: name type might be a literal integer (assigned enumeration); actual
	 * type is the enumerated name.
	 */
	if (node->tag == gup.tag_NENUMVAL) {
		tnode_t *decl = NameDeclOf (name);

		if (decl->tag != gup.tag_ENUMDEF) {
			/* odd, fail.. */
			tnode_warning (node, "type of enumerated value is not enumerated type?");
			return NULL;
		}
		return tnode_nthsubof (decl, 0);
	}

	if (NameTypeOf (name)) {
		return NameTypeOf (name);
	}
	nocc_fatal ("guppy_gettype_namenode(): name has NULL type (FIXME!)");
	return NULL;
}
/*}}}*/
/*{{{  static int guppy_getname_namenode (langops_t *lops, tnode_t *node, char **str)*/
/*
 *	gets the name of a namenode (var/etc. name)
 *	returns 0 on success, -ve on failure
 */
static int guppy_getname_namenode (langops_t *lops, tnode_t *node, char **str)
{
	char *pname = NameNameOf (tnode_nthnameof (node, 0));

	if (*str) {
		sfree (*str);
	}
	*str = string_dup (pname);
	return 0;
}
/*}}}*/
/*{{{  static int guppy_getdescriptor_namenode (langops_t *lops, tnode_t *node, char **str)*/
/*
 *	gets descriptor string for a named type
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_getdescriptor_namenode (langops_t *lops, tnode_t *node, char **str)
{

	if (node->tag == gup.tag_NENUM) {
		char *xname = string_dup (NameNameOf (tnode_nthnameof (node, 0)));

		if (*str) {
			char *tmpstr = string_fmt ("%s%s", *str, xname);

			sfree (*str);
			sfree (xname);
			*str = tmpstr;
		} else {
			*str = xname;
		}
	} else {
		nocc_error ("guppy_getdescriptor_namenode(): unhandled name [%s:%s]",
				node->tag->ndef->name, node->tag->name);
	}
	return 0;
}
/*}}}*/
/*{{{  static int guppy_isconst_namenode (langops_t *lops, tnode_t *node)*/
/*
 *	returns non-zero if the name is a constant (trivial)
 */
static int guppy_isconst_namenode (langops_t *lops, tnode_t *node)
{
	if ((node->tag == gup.tag_NVALABBR) || (node->tag == gup.tag_NVALPARAM) || (node->tag == gup.tag_NVALDECL)) {
		return 1;
	}
	return 0;
}
/*}}}*/
/*{{{  static int guppy_isvar_namenode (langops_t *lops, tnode_t *node)*/
/*
 *	returns non-zero if the name is a variable (trivial)
 */
static int guppy_isvar_namenode (langops_t *lops, tnode_t *node)
{
	if ((node->tag == gup.tag_NDECL) || (node->tag == gup.tag_NABBR) || (node->tag == gup.tag_NRESABBR) ||
			(node->tag == gup.tag_NPARAM) || (node->tag == gup.tag_NRESPARAM) || (node->tag == gup.tag_NINITPARAM)) {
		return 1;
	}
	return 0;
}
/*}}}*/
/*{{{  static int guppy_isaddressable_namenode (langops_t *lops, tnode_t *node)*/
/*
 *	returns non-zero if the name is addressable (trivial)
 */
static int guppy_isaddressable_namenode (langops_t *lops, tnode_t *node)
{
	return 1;
}
/*}}}*/
/*{{{  static int guppy_guesstlp_namenode (langops_t *lops, tnode_t *node)*/
/*
 *	attempts to guess what top-level parameter for a Guppy program this might represent
 *	returns 1=kyb, 2=scr, 3=err.
 */
static int guppy_guesstlp_namenode (langops_t *lops, tnode_t *node)
{

	if (node->tag == gup.tag_NPARAM) {
		name_t *name = tnode_nthnameof (node, 0);
		char *rname = NameNameOf (name);
		tnode_t *type = NameTypeOf (name);
		int dir = 0;

#if 0
fhandle_printf (FHAN_STDERR, "guppy_guesstlp_namenode(): here, type =\n");
tnode_dumptree (type, 1, FHAN_STDERR);
#endif
		if (type->tag != gup.tag_CHAN) {
			return 0;
		}
		dir = langops_guesstlp (type);
		if (dir == 1) {
			/* input channel, must be keyboard */
			return 1;
		} else if (dir == 2) {
			/* could be screen or error -- check name */
			if ((*rname == 's') || (*rname == 'S') || (*rname == 'o') || (*rname == 'O')) {
				return 2;
			}
			return 3;
		}
	}
	return 0;
}
/*}}}*/
/*{{{  static int guppy_getctypeof_namenode (langops_t *lops, tnode_t *node, char **str)*/
/*
 *	gets the C type of a name-node (only meaningful for type names)
 *	returns 0 on success, non-zero on failure
 */
static int guppy_getctypeof_namenode (langops_t *lops, tnode_t *node, char **str)
{
	if (node->tag == gup.tag_NTYPEDECL) {
		char *lstr;

		lstr = string_fmt ("gt_%s", NameNameOf (tnode_nthnameof (node, 0)));
		if (*str) {
			sfree (*str);
		}
		*str = lstr;
	} else if (node->tag == gup.tag_NENUM) {
		char *lstr;
		
		lstr = string_fmt ("gte_%s", NameNameOf (tnode_nthnameof (node, 0)));
		if (*str) {
			sfree (*str);
		}
		*str = lstr;
	} else {
		nocc_internal ("guppy_getctypeof_namenode(): asked for impossible C type of [%s]", node->tag->name);
		return -1;
	}
	return 0;
}
/*}}}*/
/*{{{  static int guppy_isdefpointer_namenode (langops_t *lops, tnode_t *node)*/
/*
 *	decides whether the name-node is a pointer by default (only meaningful for type names)
 *	returns level-of-indirection on success
 */
static int guppy_isdefpointer_namenode (langops_t *lops, tnode_t *node)
{
	if (node->tag == gup.tag_NTYPEDECL) {
		return 1;
	} else if (node->tag == gup.tag_NENUM) {
		return 0;
	} else {
		nocc_internal ("guppy_isdefpointer_namenode(): asked for impossible node type [%s]", node->tag->name);
	}
	return 0;
}
/*}}}*/
/*{{{  static tnode_t *guppy_initcall_namenode (langops_t *lops, tnode_t *typenode, tnode_t *name)*/
/*
 *	generates an initialiser for a user-defined type
 *	returns initialiser on success, NULL on failure
 */
static tnode_t *guppy_initcall_namenode (langops_t *lops, tnode_t *typenode, tnode_t *name)
{
	if (typenode->tag == gup.tag_NTYPEDECL) {
		tnode_t *inode;

		inode = tnode_create (gup.tag_VARINIT, SLOCI, NULL, typenode, name);
		return inode;
	} else if (typenode->tag == gup.tag_NENUM) {
		return NULL;		/* no initialiser needed */
	} else {
		nocc_internal ("guppy_initcall_namenode(): called for impossible node type [%s]", typenode->tag->name);
	}
	return NULL;
}
/*}}}*/
/*{{{  static tnode_t *guppy_freecall_namenode (langops_t *lops, tnode_t *typenode, tnode_t *name)*/
/*
 *	generates a finaliser for a user-defined type
 *	returns finaliser on success, NULL on failure (or nothing)
 */
static tnode_t *guppy_freecall_namenode (langops_t *lops, tnode_t *typenode, tnode_t *name)
{
	if (typenode->tag == gup.tag_NTYPEDECL) {
		tnode_t *inode;

		inode = tnode_create (gup.tag_VARFREE, SLOCI, NULL, typenode, name);
		return inode;
	} else if (typenode->tag == gup.tag_NENUM) {
		return NULL;		/* no destructor needed */
	} else {
		nocc_internal ("guppy_freecall_namenode(): called for impossible node type [%s]", typenode->tag->name);
	}
	return NULL;
}
/*}}}*/
/*{{{  static int guppy_bytesfor_namenode (langops_t *lops, tnode_t *node, target_t *target)*/
/*
 *	returns the number of bytes required for a name-node (types only)
 */
static int guppy_bytesfor_namenode (langops_t *lops, tnode_t *node, target_t *target)
{
	if (node->tag == gup.tag_NTYPEDECL) {
		int btot = 0;
		tnode_t *ftype;
		tnode_t **fitems;
		int nfitems, i;

#if 0
fhandle_printf (FHAN_STDERR, "guppy_bytesfor_namenode(): node =\n");
tnode_dumptree (node, 1, FHAN_STDERR);
fhandle_printf (FHAN_STDERR, "guppy_bytesfor_namenode(): NameTypeOf(..) =\n");
tnode_dumptree (NameTypeOf (tnode_nthnameof (node, 0)), 1, FHAN_STDERR);
#endif
		ftype = NameTypeOf (tnode_nthnameof (node, 0));
		if (!parser_islistnode (ftype)) {
			nocc_internal ("guppy_bytesfor_namenode(): expected list type in NTYPEDECL, found [%s]", ftype->tag->name);
			return -1;
		}
		fitems = parser_getlistitems (ftype, &nfitems);

		for (i=0; i<nfitems; i++) {
			/* should be a list of FIELDDECLs, or if mapped, list of CCCSPNAMEs */
			int thisone = tnode_bytesfor (fitems[i], target);
#if 0
fhandle_printf (FHAN_STDERR, "guppy_bytesfor_namenode(): looking at [%s] = %d\n", fitems[i]->tag->name, thisone);
#endif

			if (thisone < 0) {
				tnode_warning (node, "guppy_bytesfor_namenode(): unknown size of [%s] in NTYPEDECL", fitems[i]->tag->name);
				thisone = target->pointersize;
			}
			btot += thisone;
		}
#if 0
fhandle_printf (FHAN_STDERR, "guppy_bytesfor_namenode(): typedecl = %d\n", btot);
#endif
		return btot;
	} else if (node->tag == gup.tag_NFIELD) {
		tnode_t *ftype;

		/* this happens in the context of the above */
		ftype = NameTypeOf (tnode_nthnameof (node, 0));
		return tnode_bytesfor (ftype, target);
	}
	return -1;
}
/*}}}*/

/*{{{  static int guppy_prescope_vdecl (compops_t *cops, tnode_t **node, prescope_t *ps)*/
/*
 *	pre-scopes a variable declaration
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_prescope_vdecl (compops_t *cops, tnode_t **node, prescope_t *ps)
{
	return 1;
}
/*}}}*/
/*{{{  static int guppy_scopein_vdecl (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	scopes-in a variable declaration
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_scopein_vdecl (compops_t *cops, tnode_t **node, scope_t *ss)
{
	if ((*node)->tag == gup.tag_VARDECL) {
		/*{{{  scope-in variable declaration*/
		tnode_t *name = tnode_nthsubof (*node, 0);
		tnode_t *type;
		name_t *varname;
		tnode_t *newname;
		char *rawname;
		tnode_t *initdecl = NULL;
		ntdef_t *tag = NULL;
		int isabbrev = 0;
		int isval = 0;

#if 0
fhandle_printf (FHAN_STDERR, "guppy_scopein_vdecl(): VARDECL node: ");
tnode_dumptree (*node, 1, FHAN_STDERR);
#endif
		/* scope the type first */
		scope_subtree (tnode_nthsubaddr (*node, 1), ss);
		type = tnode_nthsubof (*node, 1);

		if (name->tag == gup.tag_ASSIGN) {
			/* initialising-decl */
			scope_subtree (tnode_nthsubaddr (name, 1), ss);
			initdecl = tnode_nthsubof (name, 1);
			tnode_setnthsub (name, 1, NULL);
			name = tnode_nthsubof (name, 0);
		} else if (name->tag == gup.tag_IS) {
			/* abbreviation */
			scope_subtree (tnode_nthsubaddr (name, 1), ss);
			initdecl = tnode_nthsubof (name, 1);
			tnode_setnthsub (name, 1, NULL);
			name = tnode_nthsubof (name, 0);
			isabbrev = 1;
		}
		if (name->tag != gup.tag_NAME) {
			scope_error (*node, ss, "name not raw-name, found [%s:%s]", name->tag->ndef->name, name->tag->name);
			return 0;
		}

		if (type->tag == gup.tag_VALTYPE) {
			isval = 1;
			type = tnode_nthsubof (type, 0);
			tnode_setnthsub (*node, 1, type);
		}

		rawname = (char *)tnode_nthhookof (name, 0);
		varname = name_addscopename (rawname, *node, type, NULL);

		if (!isval) {
			tag = isabbrev ? gup.tag_NABBR : gup.tag_NDECL;
		} else {
			tag = isabbrev ? gup.tag_NVALABBR : gup.tag_NVALDECL;
		}
		newname = tnode_createfrom (tag, name, varname);
		SetNameNode (varname, newname);
		SetNameLexlevel (varname, ss->lexlevel);
		if (initdecl) {
			/* put back scoped initialiser */
			tnode_setnthsub (*node, 2, initdecl);
		}
		tnode_setnthsub (*node, 0, newname);

		tnode_free (name);
		ss->scoped++;
		/*}}}*/
	}
	/* else ignore, must be a FIELDDECL or somesuch, scoped elsewhere */

	return 0;
}
/*}}}*/
/*{{{  static int guppy_scopeout_vdecl (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	scopes-out a variable declaration
 *	return value fairly meaningless (postwalk)
 */
static int guppy_scopeout_vdecl (compops_t *cops, tnode_t **node, scope_t *ss)
{
	return 1;
}
/*}}}*/
/*{{{  static int guppy_typecheck_vdecl (compops_t *cops, tnode_t *node, typecheck_t *tc)*/
/*
 *	does type-checking on a variable declaration -- tries to populate type better for some things
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_typecheck_vdecl (compops_t *cops, tnode_t *node, typecheck_t *tc)
{
	tnode_t *name = tnode_nthsubof (node, 0);

	if ((name->tag == gup.tag_NVALDECL) || (name->tag == gup.tag_NVALABBR)) {
		/* constant: means type cannot change */
		tnode_t **typep = tnode_nthsubaddr (node, 1);

		typecheck_subtree (tnode_nthsubof (node, 2), tc);		/* type-check initialiser */

		if ((*typep)->tag == gup.tag_STRING) {
			tnode_t *init = tnode_nthsubof (node, 2);

			if (init && (init->tag == gup.tag_LITSTRING)) {
				/* special case: 'val string foo is "..."', can get size exactly */
				tnode_t *newtype = typecheck_gettype (init, NULL);
				name_t *name = tnode_nthnameof (tnode_nthsubof (node, 0), 0);

#if 0
fhandle_printf (FHAN_STDERR, "guppy_typecheck_vdecl(): newtype from gettype on literal:\n");
tnode_dumptree (newtype, 1, FHAN_STDERR);
fhandle_printf (FHAN_STDERR, "guppy_typecheck_vdecl(): literal:\n");
tnode_dumptree (init, 1, FHAN_STDERR);
fhandle_printf (FHAN_STDERR, "guppy_typecheck_vdecl(): type on name:\n");
tnode_dumptree (NameTypeOf (name), 1, FHAN_STDERR);
#endif
				tnode_free (*typep);
				*typep = newtype;
				SetNameType (name, newtype);
			}
		}
	}
	return 1;
}
/*}}}*/
/*{{{  static int guppy_namemap_vdecl (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	does name-mapping for a variable declaration of some kind -- generates suitable back-end name.
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_namemap_vdecl (compops_t *cops, tnode_t **node, map_t *map)
{
	tnode_t **namep = tnode_nthsubaddr (*node, 0);
	tnode_t **typep = tnode_nthsubaddr (*node, 1);
	tnode_t **initp = tnode_nthsubaddr (*node, 2);
	tnode_t *bename;
	int tsize;

	if ((*node)->tag == gup.tag_VARDECL) {
		int ind;
		/* NOTE: bytesfor returns the number of workspace words required for a variable of some type */

		/* this will be target->wordsize in most cases */
		tsize = tnode_bytesfor (*typep, map->target);

		/* if we have an initialiser, map that first */
		if (*initp) {
			map_submapnames (initp, map);
		}

		ind = langops_isdefpointer (*typep);
#if 0
fhandle_printf (FHAN_STDERR, "guppy_namemap_vdecl(): my-tag=[%s], *namep =\n", (*node)->tag->name);
tnode_dumptree (*namep, 1, FHAN_STDERR);
fhandle_printf (FHAN_STDERR, ">> *initp =\n");
tnode_dumptree (*initp, 1, FHAN_STDERR);
#endif
		bename = map->target->newname (*namep, NULL, map, tsize, 0, 0, 0, tsize, ind);
		cccsp_set_initialiser (bename, *initp);

		tnode_setchook (*namep, map->mapchook, (void *)bename);
		*node = bename;
	} else if ((*node)->tag == gup.tag_FIELDDECL) {
		tsize = tnode_bytesfor (*typep, map->target);

		bename = map->target->newname (*namep, NULL, map, 0, 0, 0, 0, tsize, 0);
		tnode_setchook (*namep, map->mapchook, (void *)bename);
		*node = bename;
	} else {
		nocc_internal ("guppy_namemap_vdecl(): unhandled [%s]", (*node)->tag->name);
	}

	return 0;
}
/*}}}*/

/*{{{  static int guppy_prescope_fparam (compops_t *cops, tnode_t **nodep, prescope_t *ps)*/
/*
 *	does pre-scoping on a formal parameter (fixes type, etc.)
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_prescope_fparam (compops_t *cops, tnode_t **nodep, prescope_t *ps)
{
	guppy_prescope_t *gps = (guppy_prescope_t *)ps->hook;
	tnode_t **namep = tnode_nthsubaddr (*nodep, 0);
	tnode_t **typep = tnode_nthsubaddr (*nodep, 1);
	int markedin = 0, markedout = 0;
	char *rawpname;

	if ((*namep)->tag == gup.tag_MARKEDIN) {
		*namep = tnode_nthsubof (*namep, 0);
		markedin = 1;
	} else if ((*namep)->tag == gup.tag_MARKEDOUT) {
		*namep = tnode_nthsubof (*namep, 0);
		markedout = 1;
	}
	if ((*namep)->tag != gup.tag_NAME) {
		prescope_error (*nodep, ps, "parameter is not a name, found [%s]", (*namep)->tag->name);
		return 0;
	}
	rawpname = (char *)tnode_nthhookof (*namep, 0);

	if (!*typep) {
		/* borrow type from earlier parameter, if set */
		if (!gps->last_type) {
			prescope_error (*nodep, ps, "no type for parameter [%s]", rawpname);
			return 0;
		} else {
			*typep = tnode_copytree (gps->last_type);
		}
	} else {
		gps->last_type = tnode_copytree (*typep);
	}

	if ((*typep)->tag == gup.tag_CHAN) {
		guppy_chantype_setinout (*typep, markedin, markedout);
	} else if (markedin || markedout) {
		prescope_error (*nodep, ps, "cannot attach direction-specifier to non-channel parameter [%s]", rawpname);
		return 0;
	}
#if 0
fhandle_printf (FHAN_STDERR, "guppy_prescope_fparam(): *nodep =\n");
tnode_dumptree (*nodep, 1, FHAN_STDERR);
#endif

	return 1;
}
/*}}}*/
/*{{{  static int guppy_scopein_fparam (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	scopes-in a formal parameter
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_scopein_fparam (compops_t *cops, tnode_t **node, scope_t *ss)
{
	tnode_t *name = tnode_nthsubof (*node, 0);
	tnode_t **typep = tnode_nthsubaddr (*node, 1);
	char *rawname;
	name_t *sname = NULL;
	tnode_t *newname;
	ntdef_t *tag = gup.tag_NPARAM;
	int isval = 0;

	if (name->tag != gup.tag_NAME) {
		scope_error (*node, ss, "parameter name not raw-name, found [%s:%s]", name->tag->ndef->name, name->tag->name);
		return 0;
	}
	rawname = tnode_nthhookof (name, 0);

	if ((*typep)->tag == gup.tag_VALTYPE) {
		isval = 1;
		*typep = tnode_nthsubof (*typep, 0);
		tag = gup.tag_NVALPARAM;
	}
	/* scope the type first */
	if (scope_subtree (typep, ss)) {
		return 0;
	}

	sname = name_addscopename (rawname, *node, *typep, NULL);
	newname = tnode_createfrom (tag, name, sname);
	SetNameNode (sname, newname);
	tnode_setnthsub (*node, 0, newname);

	/* free old name */
	tnode_free (name);
	ss->scoped++;

	return 1;
}
/*}}}*/
/*{{{  static int guppy_namemap_fparam (compops_t *cops, tnode_t **nodep, map_t *map)*/
/*
 *	does name-mapping for a formal parameter
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_namemap_fparam (compops_t *cops, tnode_t **nodep, map_t *map)
{
	tnode_t **namep = tnode_nthsubaddr (*nodep, 0);
	tnode_t **typep = tnode_nthsubaddr (*nodep, 1);
	tnode_t *bename;
	int tsize, psize, indir;

#if 0
fhandle_printf (FHAN_STDERR, "guppy_namemap_fparam(): *namep =\n");
tnode_dumptree (*namep, 1, FHAN_STDERR);
fhandle_printf (FHAN_STDERR, "guppy_namemap_fparam(): *typep =\n");
tnode_dumptree (*typep, 1, FHAN_STDERR);
#endif
	indir = langops_isdefpointer (*typep);		/* based on the type */
	if (indir) {
		tsize = map->target->pointersize;
	} else {
		/* how big? */
		tsize = tnode_bytesfor (*typep, map->target);
	}
	psize = tsize;

	if (((*namep)->tag == gup.tag_NRESPARAM) || ((*namep)->tag == gup.tag_NPARAM)) {
		/* result or modifiable parameter, so pass pointer-to */
		indir++;
	}

	bename = map->target->newname (*namep, NULL, map, psize, 0, 0, 0, tsize, indir);
	tnode_setchook (*namep, map->mapchook, (void *)bename);

	*nodep = bename;

	return 0;
}
/*}}}*/
/*{{{  static int guppy_getdescriptor_fparam (langops_t *lops, tnode_t *node, char **sptr)*/
/*
 *	gets descriptor string for a formal parameter
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_getdescriptor_fparam (langops_t *lops, tnode_t *node, char **sptr)
{
	tnode_t *name = tnode_nthsubof (node, 0);
	tnode_t *type = tnode_nthsubof (node, 1);
	char *newstr = NULL;
	char *myname = NameNameOf (tnode_nthnameof (name, 0));
	char *tstr = NULL;

	langops_getdescriptor (type, &tstr);
	if (!tstr) {
		nocc_error ("guppy_getdescriptor_fparam(): failed to get descriptor for type [%s:%s]",
				type->tag->ndef->name, type->tag->name);
		return 0;
	}

	if (name->tag == gup.tag_NVALPARAM) {
		newstr = string_fmt ("val %s %s", tstr, myname);
	} else {
		newstr = string_fmt ("%s %s", tstr, myname);
	}
	if (tstr) {
		sfree (tstr);
	}
	if (*sptr) {
		char *tmpstr = string_fmt ("%s%s", *sptr, newstr);

		sfree (*sptr);
		sfree (newstr);
		*sptr = tmpstr;
	} else {
		*sptr = newstr;
	}
	return 0;
}
/*}}}*/
/*{{{  static tnode_t *guppy_gettype_fparam (langops_t *lops, tnode_t *node, tnode_t *default_type)*/
/*
 *	gets the type of a formal parmeter
 */
static tnode_t *guppy_gettype_fparam (langops_t *lops, tnode_t *node, tnode_t *default_type)
{
	tnode_t *type = tnode_nthsubof (node, 1);

	return type;
}
/*}}}*/

/*{{{  static int guppy_codegen_fparaminit (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	does code-generation for a formal-parameter initialiser
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_codegen_fparaminit (compops_t *cops, tnode_t *node, codegen_t *cgen)
{
	int pno = constprop_intvalof (tnode_nthsubof (node, 1));
	tnode_t *type = tnode_nthsubof (node, 2);
	tnode_t *indirt = tnode_nthsubof (node, 3);
	int indir, i;
	char *ctype = NULL;
	char *tindirstr = NULL;

	langops_getctypeof (type, &ctype);
#if 0
fhandle_printf (FHAN_STDERR, "guppy_codegen_fparaminit(): type=\n");
tnode_dumptree (type, 1, FHAN_STDERR);
fhandle_printf (FHAN_STDERR, "guppy_codegen_fparaminit(): indirt=\n");
tnode_dumptree (indirt, 1, FHAN_STDERR);
#endif

	if (indirt && !constprop_isconst (indirt)) {
		nocc_internal ("guppy_codegen_fparaminit(): indirection not constant..  got [%s]", indirt->tag->name);
		return 0;
	} else if (indirt) {
		indir = constprop_intvalof (indirt);
	} else {
		indir = 0;
	}

	tindirstr = (char *)smalloc (indir + 2);
	for (i=0; i<indir; i++) {
		tindirstr[i] = '*';
	}
	tindirstr[i] = '\0';

	codegen_write_fmt (cgen, "ProcGetParam (");
	codegen_subcodegen (tnode_nthsubof (node, 0), cgen);
	codegen_write_fmt (cgen, ", %d, %s%s)", pno, ctype ?: "int32_t", tindirstr);

	return 0;
}
/*}}}*/

/*{{{  static int guppy_namemap_varinit (compops_t *cops, tnode_t **nodep, map_t *map)*/
/*
 *	does name-mapping for a variable initialiser (typically user-defined types)
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_namemap_varinit (compops_t *cops, tnode_t **nodep, map_t *map)
{
	cccsp_mapdata_t *cmd = (cccsp_mapdata_t *)map->hook;
	tnode_t *type = tnode_nthsubof (*nodep, 1);

	if (!tnode_nthsubof (*nodep, 0)) {
		/* set current wptr */
		tnode_setnthsub (*nodep, 0, cmd->process_id);
	}

	if ((*nodep)->tag == gup.tag_VARINIT) {
		/*{{{  variable init (dynamically allocated)*/
		int tsize;
		tnode_t *aparms;
		tnode_t *action;

		tsize = tnode_bytesfor (type, map->target);
#if 0
fhandle_printf (FHAN_STDERR, "guppy_namemap_varinit(): tsize is %d\n", tsize);
#endif
		cmd->target_indir = 1;
		map_submapnames (tnode_nthsubaddr (*nodep, 2), map);
		cmd->target_indir = 0;

		aparms = parser_newlistnode (SLOCI);
		parser_addtolist (aparms, tnode_nthsubof (*nodep, 0));
		parser_addtolist (aparms, constprop_newconst (CONST_INT, NULL, NULL, tsize));
		map_submapnames (&aparms, map);

		action = tnode_create (gup.tag_APICALLR, SLOCI, cccsp_create_apicallname (MEM_ALLOC), aparms, tnode_nthsubof (*nodep, 2));

		*nodep = action;
		/*}}}*/
	} else if ((*nodep)->tag == gup.tag_VARFREE) {
		/*{{{  variable free (dynamically allocated)*/
		tnode_t *action, *aparms;

		cmd->target_indir = 1;
		map_submapnames (tnode_nthsubaddr (*nodep, 2), map);		/* map name */
		cmd->target_indir = 0;

		aparms = parser_newlistnode (SLOCI);
		parser_addtolist (aparms, tnode_nthsubof (*nodep, 0));		/* Wptr */
		map_submapnames (&aparms, map);					/* and map it */
		parser_addtolist (aparms, tnode_nthsubof (*nodep, 2));		/* mapped name (pointer-to) */

		action = tnode_create (gup.tag_APICALL, SLOCI, cccsp_create_apicallname (MEM_RELEASE_CHK), aparms);

		*nodep = action;
		/*}}}*/
	} else if ((*nodep)->tag == gup.tag_STRINIT) {
		/*{{{  string initialise*/
		tnode_t *action, *aparms;

		cmd->target_indir = 1;
		map_submapnames (tnode_nthsubaddr (*nodep, 2), map);		/* map name */
		cmd->target_indir = 0;

		aparms = parser_newlistnode (SLOCI);
		parser_addtolist (aparms, tnode_nthsubof (*nodep, 0));		/* Wptr */
		map_submapnames (&aparms, map);					/* and map it */

		action = tnode_create (gup.tag_APICALLR, SLOCI, cccsp_create_apicallname (STR_INIT), aparms, tnode_nthsubof (*nodep, 2));

		*nodep = action;
		/*}}}*/
	} else if ((*nodep)->tag == gup.tag_STRFREE) {
		/*{{{  string free*/
		tnode_t *action, *aparms;

		cmd->target_indir = 1;
		map_submapnames (tnode_nthsubaddr (*nodep, 2), map);		/* map name */
		cmd->target_indir = 0;

		aparms = parser_newlistnode (SLOCI);
		parser_addtolist (aparms, tnode_nthsubof (*nodep, 0));		/* Wptr */
		map_submapnames (&aparms, map);					/* and map it */
		parser_addtolist (aparms, tnode_nthsubof (*nodep, 2));		/* mapped name (pointer-to) */

		action = tnode_create (gup.tag_APICALL, SLOCI, cccsp_create_apicallname (STR_FREE), aparms);

		*nodep = action;
		/*}}}*/
	} else if ((*nodep)->tag == gup.tag_CHANINIT) {
		/*{{{  channel initialisation*/
		tnode_t *action, *aparms;

		cmd->target_indir = 1;
		map_submapnames (tnode_nthsubaddr (*nodep, 2), map);		/* map name */
		cmd->target_indir = 0;

		aparms = parser_newlistnode (SLOCI);
		parser_addtolist (aparms, tnode_nthsubof (*nodep, 0));		/* Wptr */
		map_submapnames (&aparms, map);					/* and map it */
		parser_addtolist (aparms, tnode_nthsubof (*nodep, 2));		/* mapped name (pointer-to) */

		action = tnode_create (gup.tag_APICALL, SLOCI, cccsp_create_apicallname (CHAN_INIT), aparms);

		*nodep = action;
		/*}}}*/
	} else if ((*nodep)->tag == gup.tag_ARRAYINIT) {
		/*{{{  array allocation*/
		tnode_t *action, *aparms;
		tnode_t *atype, *subtype;
		tnode_t **adims;
		int nadims, i;
		int knownsizes = 1;
		int stbytes;

		cmd->target_indir = 1;
		map_submapnames (tnode_nthsubaddr (*nodep, 2), map);		/* map name */
		cmd->target_indir = 0;

		/* start building parameter list */
		aparms = parser_newlistnode (SLOCI);
		parser_addtolist (aparms, tnode_nthsubof (*nodep, 0));		/* Wptr */


		if (type->tag != gup.tag_ARRAY) {
			nocc_internal ("guppy_namemap_varinit(): type not ARRAY, got [%s:%s]", type->tag->ndef->name, type->tag->name);
			return 0;
		}
		atype = tnode_nthsubof (type, 0);		/* should be dimension tree as a list */
		if (!parser_islistnode (atype)) {
			nocc_internal ("guppy_namemap_varinit(): array-dimtree not list, got [%s:%s]", atype->tag->ndef->name, atype->tag->name);
			return 0;
		}
		adims = parser_getlistitems (atype, &nadims);
		for (i=0; i<nadims; i++) {
			if (!adims[i]) {
				knownsizes = 0;
			}
		}

		subtype = typecheck_getsubtype (type, NULL);
		stbytes = tnode_bytesfor (subtype, map->target);
		if (stbytes <= 0) {
			nocc_internal ("guppy_namemap_varinit(): array sub-type [%s:%s] had invalid size (%d)",
					subtype->tag->ndef->name, subtype->tag->name, stbytes);
			return 0;
		}

		if (knownsizes) {
			/* all dimensions known, so can construct initialiser */
			parser_addtolist (aparms, constprop_newconst (CONST_INT, NULL, NULL, nadims));
			parser_addtolist (aparms, constprop_newconst (CONST_INT, NULL, NULL, stbytes));
			parser_addtolist (aparms, cccsp_create_null (SLOCI, map->target));
			for (i=0; i<nadims; i++) {
				parser_addtolist (aparms, adims[i]);
			}
		}

		map_submapnames (&aparms, map);

		action = tnode_create (gup.tag_APICALLR, SLOCI, cccsp_create_apicallname (knownsizes ? ARRAY_INIT_ALLOC : ARRAY_INIT),
				aparms, tnode_nthsubof (*nodep, 2));

		*nodep = action;
		/*}}}*/
	} else if ((*nodep)->tag == gup.tag_ARRAYFREE) {
		/*{{{  array free*/
		tnode_t *action, *aparms;

		cmd->target_indir = 1;
		map_submapnames (tnode_nthsubaddr (*nodep, 2), map);		/* map name */
		cmd->target_indir = 0;

		aparms = parser_newlistnode (SLOCI);
		parser_addtolist (aparms, tnode_nthsubof (*nodep, 0));		/* Wptr */
		map_submapnames (&aparms, map);					/* and map it */
		parser_addtolist (aparms, tnode_nthsubof (*nodep, 2));		/* mapped name (pointer-to) */

		action = tnode_create (gup.tag_APICALL, SLOCI, cccsp_create_apicallname (ARRAY_FREE), aparms);

		*nodep = action;
		/*}}}*/
	}
	return 0;
}
/*}}}*/

/*{{{  static int guppy_scopein_enumdef (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	scopes in an enumerated type definition
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_scopein_enumdef (compops_t *cops, tnode_t **node, scope_t *ss)
{
	tnode_t *name = tnode_nthsubof (*node, 0);
	tnode_t *elist = tnode_nthsubof (*node, 1);
	char *rawname;
	name_t *ename;
	tnode_t *newname, **items;
	int nitems, i;

	/* declare and scope enum name, check processes in scope */
	rawname = tnode_nthhookof (name, 0);

	ename = name_addscopename (rawname, *node, NULL, NULL);
	newname = tnode_createfrom (gup.tag_NENUM, name, ename);
	SetNameNode (ename, newname);
	tnode_setnthsub (*node, 0, newname);

	tnode_free (name);
	ss->scoped++;

	/* then declare and scope individual enumerated entries */
	items = parser_getlistitems (elist, &nitems);
	for (i=0; i<nitems; i++) {
		name_t *einame;
		tnode_t *eitype;
		tnode_t *enewname;

		if (items[i]->tag == gup.tag_NAME) {
			/* auto-assign value later */
			rawname = tnode_nthhookof (items[i], 0);
			einame = name_addscopename (rawname, *node, NULL, NULL);
			enewname = tnode_createfrom (gup.tag_NENUMVAL, items[i], einame);
			SetNameNode (einame, enewname);

			tnode_free (items[i]);
			items[i] = enewname;
			ss->scoped++;
		} else if (items[i]->tag == gup.tag_ASSIGN) {
			/* assign value now */
			tnode_t **rhsp = tnode_nthsubaddr (items[i], 1);

			rawname = tnode_nthhookof (tnode_nthsubof (items[i], 0), 0);
			if ((*rhsp)->tag != gup.tag_LITINT) {
				scope_error (items[i], ss, "enumerated value is not an integer literal");
				eitype = NULL;
			} else {
				/* fix type of enumerated value */
				eitype = guppy_newprimtype (gup.tag_INT, items[i], 32);
				tnode_setnthsub (*rhsp, 0, eitype);
				eitype = *rhsp;
				*rhsp = NULL;
			}

			einame = name_addscopename (rawname, *node, eitype, NULL);
			enewname = tnode_createfrom (gup.tag_NENUMVAL, items[i], einame);
			SetNameNode (einame, enewname);

			tnode_free (items[i]);
			items[i] = enewname;
			ss->scoped++;
		} else {
			scope_error (items[i], ss, "unsupported structure type in enumerated list");
		}
	}

	return 0;
}
/*}}}*/
/*{{{  static int guppy_scopeout_enumdef (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	scopes-out an enumerated type definition
 *	return value meaningless (postwalk)
 */
static int guppy_scopeout_enumdef (compops_t *cops, tnode_t **node, scope_t *ss)
{
	return 1;
}
/*}}}*/
/*{{{  static int guppy_fetrans15_enumdef (compops_t *cops, tnode_t **nodep, guppy_fetrans15_t *fe15)*/
/*
 *	does fetrans1.5 on an enum definition (do nothing, don't look inside)
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_fetrans15_enumdef (compops_t *cops, tnode_t **nodep, guppy_fetrans15_t *fe15)
{
	return 0;
}
/*}}}*/
/*{{{  static int guppy_typecheck_enumdef (compops_t *cops, tnode_t *node, typecheck_t *tc)*/
/*
 *	does type-checking for an enumerated type definition.
 *	return 0 to stop walk, 1 to continue
 */
static int guppy_typecheck_enumdef (compops_t *cops, tnode_t *node, typecheck_t *tc)
{
	tnode_t *tname = tnode_nthsubof (node, 0);
	tnode_t *tlist = tnode_nthsubof (node, 1);
	int ntitems, i;
	tnode_t **titems;
	
	if (!parser_islistnode (tlist)) {
		typecheck_error (node, tc, "body of enumerated type definition is not a list");
		return 0;
	}

	titems = parser_getlistitems (tlist, &ntitems);
	for (i=0; i<ntitems; i++) {
		tnode_t *item = titems[i];

		if (item->tag != gup.tag_NENUMVAL) {
			typecheck_error (item, tc, "invalid name in enumerated type definition of [%s]", NameNameOf (tnode_nthnameof (tname, 0)));
		} else {
			tnode_t *itype = typecheck_gettype (titems[i], NULL);		/* get item type */

			if (!itype) {
				typecheck_error (item, tc, "failed to get type of item in enumerated type definition of [%s]",
						NameNameOf (tnode_nthnameof (tname, 0)));
			} else if (itype != tname) {
				typecheck_error (item, tc, "enumerated item type is not the same as its definition (got %s:%s).  Urgh?",
						itype->tag->ndef->name, itype->tag->name);
			}
			/* otherwise assume good! */
		}
	}

#if 0
fhandle_printf (FHAN_STDERR, "guppy_typecheck_enumdef(): typecheck okay, %d items\n", ntitems);
#endif
	return 0;
}
/*}}}*/
/*{{{  static int guppy_namemap_enumdef (compops_t *cops, tnode_t **nodep, map_t *map)*/
/*
 *	does name-mapping for an enumerated type definition.
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_namemap_enumdef (compops_t *cops, tnode_t **nodep, map_t *map)
{
	tnode_t *name = tnode_nthsubof (*nodep, 0);
	tnode_t **tlistp = tnode_nthsubaddr (*nodep, 1);
	tnode_t *betype;
	char *rawname = NameNameOf (tnode_nthnameof (name, 0));
	int ntitems, i;
	tnode_t **titems;

	/* manually name-map items in this, since they are just lone names */
	titems = parser_getlistitems (*tlistp, &ntitems);
	for (i=0; i<ntitems; i++) {
		tnode_t **itemp = titems + i;
		char *iname = NameNameOf (tnode_nthnameof (*itemp, 0));
		tnode_t *beiname, *initp;

		beiname = cccsp_create_ename (*itemp, map);
		initp = NameTypeOf (tnode_nthnameof (*itemp, 0));
		cccsp_set_initialiser (beiname, initp);

		// initp = map->target->newconst (
		tnode_setchook (*itemp, map->mapchook, (void *)beiname);
		*itemp = beiname;

	}

	betype = cccsp_create_etype (OrgOf (*nodep), map->target, rawname, *tlistp);

	tnode_setchook (name, map->mapchook, (void *)betype);
	*nodep = betype;

	return 0;
}
/*}}}*/

/*{{{  static int guppy_typecheck_fvnode (compops_t *cops, tnode_t *node, typecheck_t *tc)*/
/*
 *	does type-checking on a free-vars node (removes anything that isn't a variable)
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_typecheck_fvnode (compops_t *cops, tnode_t *node, typecheck_t *tc)
{
	tnode_t *fvlist = tnode_nthsubof (node, 1);
	tnode_t *keeplist = parser_newlistnode (NULL);
	tnode_t **fvitems;
	int i, nfvitems;

	/* type-check body first, incase it fixes any of the types */
	typecheck_subtree (tnode_nthsubof (node, 0), tc);

	fvitems = parser_getlistitems (fvlist, &nfvitems);
	for (i=0; i<nfvitems; i++) {
		int keep = 0;

		if (fvitems[i]->tag == gup.tag_NDECL) {
			keep = 1;
		} else if ((fvitems[i]->tag == gup.tag_NABBR) || (fvitems[i]->tag == gup.tag_NVALABBR) || (fvitems[i]->tag == gup.tag_NRESABBR)) {
			keep = 1;
		} else if ((fvitems[i]->tag == gup.tag_NPARAM) || (fvitems[i]->tag == gup.tag_NVALPARAM) ||
				(fvitems[i]->tag == gup.tag_NRESPARAM) || (fvitems[i]->tag == gup.tag_NINITPARAM)) {
			keep = 1;
		} else if (fvitems[i]->tag == gup.tag_NREPL) {
			keep = 1;
		}

		if (keep) {
			parser_addtolist (keeplist, fvitems[i]);
		}
		fvitems[i] = NULL;
	}
	/* switch lists */
	tnode_free (fvlist);
	tnode_setnthsub (node, 1, keeplist);

	return 0;
}
/*}}}*/

/*{{{  static int guppy_autoseq_declblock (compops_t *cops, tnode_t **node, guppy_autoseq_t *gas)*/
/*
 *	auto-sequence on a declaration block
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_autoseq_declblock (compops_t *cops, tnode_t **node, guppy_autoseq_t *gas)
{
	tnode_t **ilistptr = tnode_nthsubaddr (*node, 1);

#if 0
fprintf (stderr, "guppy_autoseq_declblock(): here!\n");
#endif
	if (parser_islistnode (*ilistptr)) {
		guppy_autoseq_listtoseqlist (ilistptr, gas);
	}
	return 1;
}
/*}}}*/
/*{{{  static int guppy_flattenseq_declblock (compops_t *cops, tnode_t **node)*/
/*
 *	flatten sequences for a declaration block
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_flattenseq_declblock (compops_t *cops, tnode_t **node)
{
	tnode_t *dlist = tnode_nthsubof (*node, 0);
	tnode_t **iptr = tnode_nthsubaddr (*node, 1);
	tnode_t **items;
	int nitems, i;

	items = parser_getlistitems (dlist, &nitems);
	for (i=0; i<nitems; i++) {
		if (items[i]->tag == gup.tag_VARDECL) {
			/* check for multiple declarations */
			tnode_t *vdname = tnode_nthsubof (items[i], 0);
			tnode_t *vdtype = tnode_nthsubof (items[i], 1);
			ntdef_t *tag = items[i]->tag;

			if (parser_islistnode (vdname)) {
				tnode_t **vitems;
				int nvitems, j;

				vitems = parser_getlistitems (vdname, &nvitems);
				if (nvitems == 1) {
					/* singleton, remove list */
					tnode_t *thisvd = parser_delfromlist (vdname, 0);

					parser_trashlist (vdname);
					tnode_setnthsub (items[i], 0, thisvd);
				} else {
					tnode_t *firstname;

					for (j=1; j<nvitems; j++) {
						tnode_t *newdecl = tnode_createfrom (tag, items[i], vitems[j], tnode_copytree (vdtype), NULL);
						int nnitems;

						parser_delfromlist (vdname, j);
						j--, nvitems--;
						parser_insertinlist (dlist, newdecl, i+1);
						nitems++;
						/* oop: if the parser reallocated this, will go haywire! */
						items = parser_getlistitems (dlist, &nnitems);
						if (nitems != nnitems) {
							tnode_error (*node, "guppy_flattenseq_declblock(): nitems = %d, but nnitems = %d",
									nitems, nnitems);
							nocc_internal ("giving up");
						}
					}

					/* fixup first item */
					firstname = parser_delfromlist (vdname, 0);
					parser_trashlist (vdname);
					tnode_setnthsub (items[i], 0, firstname);
				}
			}
		}
	}

	/* pull apart initialising declarations into multiple chunks */
	/* 'iptr' is either SEQ or singleton */
	items = parser_getlistitems (dlist, &nitems);
	for (i=0; i<nitems; i++) {
		if (items[i]->tag == gup.tag_VARDECL) {
			tnode_t *vname = tnode_nthsubof (items[i], 0);

			if (vname->tag == gup.tag_ASSIGN) {
				/* this one, pull apart */
				tnode_t *namecopy = tnode_copytree (tnode_nthsubof (vname, 0));		/* LHS copy */
				tnode_t *seqblock, *instlist;

				tnode_setnthsub (items[i], 0, namecopy);
				instlist = parser_newlistnode (OrgOf (items[i]));
				seqblock = tnode_createfrom (gup.tag_SEQ, items[i], NULL, instlist);

				parser_addtolist (instlist, vname);					/* assignment */
				parser_addtolist (instlist, *iptr);
				*iptr = seqblock;
			}
		}
	}
	return 1;
}
/*}}}*/
/*{{{  static int guppy_scopein_declblock (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	scope-in a declaration block
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_scopein_declblock (compops_t *cops, tnode_t **node, scope_t *ss)
{
	void *nsmark;
	tnode_t *decllist = tnode_nthsubof (*node, 0);
	tnode_t **procptr = tnode_nthsubaddr (*node, 1);
	tnode_t **items;
	int nitems, i;

#if 0
fhandle_printf (FHAN_STDERR, "guppy_scopein_declblock(): here!  complete block is:\n");
tnode_dumptree (*node, 1, FHAN_STDERR);
#endif
	nsmark = name_markscope ();
	items = parser_getlistitems (decllist, &nitems);
	for (i=0; i<nitems; i++) {
		/* scope-in the declaration, save scope-out */
		tnode_modprewalktree (items + i, scope_modprewalktree, (void *)ss);
	}

	/* scope body */
	scope_subtree (procptr, ss);

	for (i=nitems - 1; i>=0; i--) {
		/* scope-out the delcaration */
		tnode_modpostwalktree (items + i, scope_modpostwalktree, (void *)ss);
	}

	name_markdescope (nsmark);
	return 0;
}
/*}}}*/
/*{{{  static int guppy_fetrans15_declblock (compops_t *cops, tnode_t **nodep, guppy_fetrans15_t *fe15)*/
/*
 *	does fetrans1.5 on a declaration block -- just walk body
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_fetrans15_declblock (compops_t *cops, tnode_t **nodep, guppy_fetrans15_t *fe15)
{
	int saved = fe15->expt_proc;

	fe15->expt_proc = 1;
	guppy_fetrans15_subtree (tnode_nthsubaddr (*nodep, 1), fe15);

	fe15->expt_proc = saved;
	return 0;
}
/*}}}*/
/*{{{  static int guppy_betrans_declblock (compops_t *cops, tnode_t **nodep, betrans_t *be)*/
/*
 *	does back-end transforms for a declaration block -- generates odd initialisers for declarations
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_betrans_declblock (compops_t *cops, tnode_t **nodep, betrans_t *be)
{
	tnode_t *decllist = tnode_nthsubof (*nodep, 0);
	tnode_t **bodyp = tnode_nthsubaddr (*nodep, 1);
	tnode_t **decls;
	int ndecls, i;
	tnode_t *seqlist;

	if ((*bodyp)->tag != gup.tag_SEQ) {
		/* insert a new SEQ node to soak up initialisers */
		tnode_t *nseq;

		seqlist = parser_newlistnode (SLOCI);
		nseq = tnode_create (gup.tag_SEQ, OrgOf (*nodep), NULL, seqlist);
		parser_addtolist (seqlist, *bodyp);
		*bodyp = nseq;
	} else {
		seqlist = tnode_nthsubof (*bodyp, 1);
	}

	decls = parser_getlistitems (decllist, &ndecls);
	for (i=0; i<ndecls; i++) {
		if (decls[i]->tag == gup.tag_VARDECL) {
			tnode_t *type = tnode_nthsubof (decls[i], 1);
			tnode_t *initcall = NULL;
			tnode_t *freecall = NULL;

#if 0
fhandle_printf (FHAN_STDERR, "guppy_betrans_declblock(): looking for initcall on [%s]\n", type->tag->name);
#endif
			/* Note: if we have an initialiser, ignore any initcall on the type */
			if (!tnode_nthsubof (decls[i], 2)) {
				initcall = langops_initcall (type, tnode_nthsubof (decls[i], 0));
				if (initcall) {
					parser_addtolist_front (seqlist, initcall);
				}
			}
			freecall = langops_freecall (type, tnode_nthsubof (decls[i], 0));
			if (freecall) {
				parser_addtolist (seqlist, freecall);
			}
		} else {
			nocc_warning ("guppy_betrans_declblock(): unexpected in declarations [%s]", decls[i]->tag->name);
		}
	}

	return 1;
}
/*}}}*/
/*{{{  static int guppy_namemap_declblock (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	does name-mapping for a declaration block -- currently not a lot.
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_namemap_declblock (compops_t *cops, tnode_t **node, map_t *map)
{
	tnode_t *blk;
	tnode_t **bodyp;

	blk = map->target->newblock (*node, map, NULL, map->lexlevel);

	/* map out declarations and then the process */
	map_submapnames (tnode_nthsubaddr (*node, 0), map);
	map_submapnames (tnode_nthsubaddr (*node, 1), map);	/* do under back-end block */

	*node = blk;						/* insert back-end BLOCK before DECLBLOCK */
	return 0;
}
/*}}}*/
/*{{{  static int guppy_codegen_declblock (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	does code-generation for a declaration block -- currently not a lot.
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_codegen_declblock (compops_t *cops, tnode_t *node, codegen_t *cgen)
{
	codegen_subcodegen (tnode_nthsubof (node, 0), cgen);	/* code-generate declarations */
	codegen_subcodegen (tnode_nthsubof (node, 1), cgen);	/* code-generate body */

	return 0;
}
/*}}}*/

/*{{{  static int guppy_oncreate_typedef (compops_t *cops, tnode_t **nodep)*/
/*
 *	called when a new typedef node is created, used to remember what may be types later on
 *	return value ignored
 */
static int guppy_oncreate_typedef (compops_t *cops, tnode_t **nodep)
{
	tnode_t *name = tnode_nthsubof (*nodep, 0);

#if 0
fhandle_printf (FHAN_STDERR, "guppy_oncreate_typedef(): here!, *nodep =\n");
tnode_dumptree (*nodep, 1, FHAN_STDERR);
#endif
	if (name->tag == gup.tag_NAME) {
		char *str = (char *)tnode_nthhookof (name, 0);
		deftypename_t *dtn = stringhash_lookup (deftypenames, str);

		if (!dtn) {
			dtn = (deftypename_t *)smalloc (sizeof (deftypename_t));
			dtn->name = string_dup (str);
			stringhash_insert (deftypenames, dtn, dtn->name);
		}
	}
	return 0;
}
/*}}}*/
/*{{{  static int guppy_scopein_typedef (compops_t *cops, tnode_t **nodep, scope_t *ss)*/
/*
 *	called to scope-in a type definition
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_scopein_typedef (compops_t *cops, tnode_t **nodep, scope_t *ss)
{
	/* Note: any abstract types will have been specialised by this point, once that is in place */
	tnode_t *name = tnode_nthsubof (*nodep, 0);
	tnode_t *type = tnode_nthsubof (*nodep, 1);
	char *rawname;
	name_t *tname;
	tnode_t *newname, **items;
	int nitems, i;
	tnode_t *fdecllist;

	/* scope type contents first */
	fdecllist = parser_newlistnode (SLOCI);
	items = parser_getlistitems (type, &nitems);
	for (i=0; i<nitems; i++) {
		tnode_t *fitype;

		if (items[i]->tag == gup.tag_VARDECL) {
			tnode_t **vditems;
			int nvditems, j;

			/* scope type */
			scope_subtree (tnode_nthsubaddr (items[i], 1), ss);
			fitype = tnode_nthsubof (items[i], 1);

			/* first thing is a list of names */
			vditems = parser_getlistitems (tnode_nthsubof (items[i], 0), &nvditems);
			for (j=0; j<nvditems; j++) {
				name_t *finame;
				tnode_t *fnewname, *fdecl;
				char *frawname;

				if (vditems[j]->tag != gup.tag_NAME) {
					scope_error (vditems[j], ss, "field name is not a name, found [%s]", vditems[j]->tag->name);
					return 0;
				}
				frawname = tnode_nthhookof (vditems[j], 0);
				finame = name_addscopename (frawname, NULL, fitype, NULL);
				fnewname = tnode_createfrom (gup.tag_NFIELD, vditems[j], finame);
				SetNameNode (finame, fnewname);
				fdecl = tnode_createfrom (gup.tag_FIELDDECL, vditems[j], fnewname, fitype, NULL);
				SetNameDecl (finame, fdecl);

				parser_addtolist (fdecllist, fdecl);
				ss->scoped++;
			}
		} else {
			scope_error (items[i], ss, "unsupported structure type [%s] in type definition", items[i]->tag->name);
		}
	}

	/* create a name for the type */
	rawname = tnode_nthhookof (name, 0);
	tname = name_addscopename (rawname, *nodep, fdecllist, NULL);
	newname = tnode_createfrom (gup.tag_NTYPEDECL, *nodep, tname);
	SetNameNode (tname, newname);

	tnode_setnthsub (*nodep, 0, newname);

	/* and put the field-declaration list in here too (FIXME: type should probably be simpler) */
	tnode_setnthsub (*nodep, 1, fdecllist);

	ss->scoped++;

	return 0;
}
/*}}}*/
/*{{{  static int guppy_fetrans15_typedef (compops_t *cops, tnode_t **nodep, guppy_fetrans15_t *fe15)*/
/*
 *	does fetrans1.5 on a type definition (do nothing, don't look inside)
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_fetrans15_typedef (compops_t *cops, tnode_t **nodep, guppy_fetrans15_t *fe15)
{
	return 0;
}
/*}}}*/
/*{{{  static int guppy_typecheck_typedef (compops_t *cops, tnode_t *node, typecheck_t *tc)*/
/*
 *	does type-checking for a structured type definition
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_typecheck_typedef (compops_t *cops, tnode_t *node, typecheck_t *tc)
{
	tnode_t *flist, **fitems;
	int nfitems, i;

	flist = tnode_nthsubof (node, 1);
	fitems = parser_getlistitems (flist, &nfitems);
	for (i=0; i<nfitems; i++) {
		if (fitems[i]->tag != gup.tag_FIELDDECL) {
			typecheck_error (fitems[i], tc, "expected field in type definition, but got [%s]", fitems[i]->tag->name);
		} else {
			/* get type from field-name, not definition */
			tnode_t *fname = tnode_nthsubof (fitems[i], 0);
			tnode_t *ftype = typecheck_gettype (fname, NULL);

			if (!ftype) {
				typecheck_error (fitems[i], tc, "field [%s] has incomplete type", NameNameOf (tnode_nthnameof (fname, 0)));
			}
		}
	}

	return 0;
}
/*}}}*/
/*{{{  static int guppy_namemap_typedef (compops_t *cops, tnode_t **nodep, map_t *map)*/
/*
 *	does name-mapping for a structured type definition
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_namemap_typedef (compops_t *cops, tnode_t **nodep, map_t *map)
{
	tnode_t **flistp = tnode_nthsubaddr (*nodep, 1);
	tnode_t *betype;
	tnode_t *name = tnode_nthsubof (*nodep, 0);
	char *rawname = NameNameOf (tnode_nthnameof (name, 0));

	map_submapnames (flistp, map);
	betype = cccsp_create_utype (OrgOf (*nodep), map->target, rawname, *flistp);

	tnode_setchook (name, map->mapchook, (void *)betype);
	*nodep = betype;
#if 0
fhandle_printf (FHAN_STDERR, "guppy_namemap_typedef(): node to map:\n");
tnode_dumptree (*nodep, 1, FHAN_STDERR);
#endif
	return 0;
}
/*}}}*/

/*{{{  static int guppy_decls_init_nodes (void)*/
/*
 *	sets up declaration and name nodes for Guppy
 *	returns 0 on success, non-zero on error
 */
static int guppy_decls_init_nodes (void)
{
	tndef_t *tnd;
	compops_t *cops;
	langops_t *lops;
	int i;


	/*{{{  register functions first*/
	fcnlib_addfcn ("guppy_checktypename", (void *)guppy_checktypename, 1, 1);

	/*}}}*/

	/*{{{  guppy:rawnamenode -- NAME*/
	i = -1;
	tnd = tnode_newnodetype ("guppy:rawnamenode", &i, 0, 0, 1, TNF_NONE);				/* hooks: raw-name */
	tnd->hook_free = guppy_rawnamenode_hook_free;
	tnd->hook_copy = guppy_rawnamenode_hook_copy;
	tnd->hook_dumptree = guppy_rawnamenode_hook_dumptree;
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (guppy_scopein_rawnamenode));
	tnd->ops = cops;

	i = -1;
	gup.tag_NAME = tnode_newnodetag ("NAME", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  guppy:namenode -- N_DECL, N_ABBR, N_VALABBR, N_RESABBR, N_PARAM, N_VALPARAM, N_RESPARAM, N_INITPARAM, N_REPL, N_TYPEDECL, N_FIELD, N_FCNDEF, N_ENUM, N_ENUMVAL*/
	i = -1;
	tnd = gup.node_NAMENODE = tnode_newnodetype ("guppy:namenode", &i, 0, 1, 0, TNF_NONE);		/* subnames: name */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "fetrans15", 2, COMPOPTYPE (guppy_fetrans15_namenode));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (guppy_namemap_namenode));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (guppy_codegen_namenode));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "gettype", 2, LANGOPTYPE (guppy_gettype_namenode));
	tnode_setlangop (lops, "getdescriptor", 2, LANGOPTYPE (guppy_getdescriptor_namenode));
	tnode_setlangop (lops, "getname", 2, LANGOPTYPE (guppy_getname_namenode));
	tnode_setlangop (lops, "isconst", 1, LANGOPTYPE (guppy_isconst_namenode));
	tnode_setlangop (lops, "isvar", 1, LANGOPTYPE (guppy_isvar_namenode));
	tnode_setlangop (lops, "isaddressable", 1, LANGOPTYPE (guppy_isaddressable_namenode));
	tnode_setlangop (lops, "guesstlp", 1, LANGOPTYPE (guppy_guesstlp_namenode));
	tnode_setlangop (lops, "getctypeof", 2, LANGOPTYPE (guppy_getctypeof_namenode));
	tnode_setlangop (lops, "isdefpointer", 1, LANGOPTYPE (guppy_isdefpointer_namenode));
	tnode_setlangop (lops, "initcall", 2, LANGOPTYPE (guppy_initcall_namenode));
	tnode_setlangop (lops, "freecall", 2, LANGOPTYPE (guppy_freecall_namenode));
	tnode_setlangop (lops, "bytesfor", 2, LANGOPTYPE (guppy_bytesfor_namenode));
	tnd->lops = lops;

	i = -1;
	gup.tag_NDECL = tnode_newnodetag ("N_DECL", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_NVALDECL = tnode_newnodetag ("N_VALDECL", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_NABBR = tnode_newnodetag ("N_ABBR", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_NVALABBR = tnode_newnodetag ("N_VALABBR", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_NRESABBR = tnode_newnodetag ("N_RESABBR", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_NPARAM = tnode_newnodetag ("N_PARAM", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_NVALPARAM = tnode_newnodetag ("N_VALPARAM", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_NRESPARAM = tnode_newnodetag ("N_RESPARAM", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_NINITPARAM = tnode_newnodetag ("N_INITPARAM", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_NREPL = tnode_newnodetag ("N_REPL", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_NTYPEDECL = tnode_newnodetag ("N_TYPEDECL", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_NFIELD = tnode_newnodetag ("N_FIELD", &i, tnd, NTF_HIDDENNAME);
	i = -1;
	gup.tag_NFCNDEF = tnode_newnodetag ("N_FCNDEF", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_NPFCNDEF = tnode_newnodetag ("N_PFCNDEF", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_NENUM = tnode_newnodetag ("N_ENUM", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_NENUMVAL = tnode_newnodetag ("N_ENUMVAL", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  guppy:vdecl -- VARDECL, FIELDDECL*/
	i = -1;
	tnd = tnode_newnodetype ("guppy:vdecl", &i, 3, 0, 0, TNF_NONE);					/* subnodes: name; type; initialiser */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "prescope", 2, COMPOPTYPE (guppy_prescope_vdecl));
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (guppy_scopein_vdecl));
	tnode_setcompop (cops, "scopeout", 2, COMPOPTYPE (guppy_scopeout_vdecl));
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (guppy_typecheck_vdecl));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (guppy_namemap_vdecl));
	tnd->ops = cops;

	i = -1;
	gup.tag_VARDECL = tnode_newnodetag ("VARDECL", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_FIELDDECL = tnode_newnodetag ("FIELDDECL", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  guppy:fparam -- FPARAM*/
	i = -1;
	tnd = tnode_newnodetype ("guppy:fparam", &i, 3, 0, 0, TNF_NONE);				/* subnodes: name; type; initialiser, hooks: fparaminfo */
	tnd->hook_dumptree = guppy_fparaminfo_hook_dumptree;
	tnd->hook_copy = guppy_fparaminfo_hook_copy;
	tnd->hook_free = guppy_fparaminfo_hook_free;
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "prescope", 2, COMPOPTYPE (guppy_prescope_fparam));
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (guppy_scopein_fparam));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (guppy_namemap_fparam));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "getdescriptor", 2, LANGOPTYPE (guppy_getdescriptor_fparam));
	tnode_setlangop (lops, "gettype", 2, LANGOPTYPE (guppy_gettype_fparam));
	tnd->lops = lops;

	i = -1;
	gup.tag_FPARAM = tnode_newnodetag ("FPARAM", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  guppy:fparaminit -- FPARAMINIT*/
	i = -1;
	tnd = tnode_newnodetype ("guppy:fparaminit", &i, 4, 0, 0, TNF_NONE);				/* subnodes: wptr, parameter, type, indirection */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (guppy_codegen_fparaminit));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnd->lops = lops;

	i = -1;
	gup.tag_FPARAMINIT = tnode_newnodetag ("FPARAMINIT", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  guppy:varinit -- VARINIT, VARFREE, STRINIT, STRFREE, CHANINIT, ARRAYINIT, ARRAYFREE*/
	i = -1;
	tnd = tnode_newnodetype ("guppy:varinit", &i, 3, 0, 0, TNF_NONE);				/* subnodes: wptr, type, target-name */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (guppy_namemap_varinit));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnd->lops = lops;

	i = -1;
	gup.tag_VARINIT = tnode_newnodetag ("VARINIT", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_VARFREE = tnode_newnodetag ("VARFREE", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_STRINIT = tnode_newnodetag ("STRINIT", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_STRFREE = tnode_newnodetag ("STRFREE", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_CHANINIT = tnode_newnodetag ("CHANINIT", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_ARRAYINIT = tnode_newnodetag ("ARRAYINIT", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_ARRAYFREE = tnode_newnodetag ("ARRAYFREE", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  guppy:declblock -- DECLBLOCK*/
	i = -1;
	tnd = tnode_newnodetype ("guppy:declblock", &i, 2, 0, 0, TNF_NONE);				/* subnodes: decls; process */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "autoseq", 2, COMPOPTYPE (guppy_autoseq_declblock));
	tnode_setcompop (cops, "flattenseq", 1, COMPOPTYPE (guppy_flattenseq_declblock));
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (guppy_scopein_declblock));
	tnode_setcompop (cops, "fetrans15", 2, COMPOPTYPE (guppy_fetrans15_declblock));
	tnode_setcompop (cops, "betrans", 2, COMPOPTYPE (guppy_betrans_declblock));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (guppy_namemap_declblock));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (guppy_codegen_declblock));
	tnd->ops = cops;

	i = -1;
	gup.tag_DECLBLOCK = tnode_newnodetag ("DECLBLOCK", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  guppy:enumdef -- ENUMDEF*/
	i = -1;
	tnd = tnode_newnodetype ("guppy:enumdef", &i, 2, 0, 0, TNF_LONGDECL);				/* subnodes: name; items */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (guppy_scopein_enumdef));
	tnode_setcompop (cops, "scopeout", 2, COMPOPTYPE (guppy_scopeout_enumdef));
	tnode_setcompop (cops, "fetrans15", 2, COMPOPTYPE (guppy_fetrans15_enumdef));
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (guppy_typecheck_enumdef));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (guppy_namemap_enumdef));
	tnd->ops = cops;

	i = -1;
	gup.tag_ENUMDEF = tnode_newnodetag ("ENUMDEF", &i, tnd, NTF_INDENTED_NAME_LIST);

	/*}}}*/
	/*{{{  guppy:typedef -- TYPEDEF*/
	i = -1;
	tnd = tnode_newnodetype ("guppy:typedef", &i, 3, 0, 0, TNF_LONGDECL);				/* subnodes: name; items; type-params */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "oncreate", 1, COMPOPTYPE (guppy_oncreate_typedef));
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (guppy_scopein_typedef));
	tnode_setcompop (cops, "fetrans15", 2, COMPOPTYPE (guppy_fetrans15_typedef));
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (guppy_typecheck_typedef));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (guppy_namemap_typedef));
	tnd->ops = cops;

	i = -1;
	gup.tag_TYPEDEF = tnode_newnodetag ("TYPEDEF", &i, tnd, NTF_INDENTED_DECL_LIST);

	/*}}}*/
	/*{{{  guppy:fvnode -- FVNODE*/
	i = -1;
	tnd = tnode_newnodetype ("guppy:fvnode", &i, 2, 0, 0, TNF_NONE);
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (guppy_typecheck_fvnode));
	tnd->ops = cops;

	i = -1;
	gup.tag_FVNODE = tnode_newnodetag ("FVNODE", &i, tnd, NTF_NONE);

	/*}}}*/

	return 0;
}
/*}}}*/
/*{{{  static int guppy_decls_post_setup (void)*/
/*
 *	called to do any post-setup on declaration nodes
 *	returns 0 on success, non-zero on error
 */
static int guppy_decls_post_setup (void)
{
	stringhash_sinit (deftypenames);

	parser_gettesttags (&guppy_testtruetag, &guppy_testfalsetag);

	return 0;
}
/*}}}*/


/*{{{  guppy_decls_feunit (feunit_t)*/
feunit_t guppy_decls_feunit = {
	.init_nodes = guppy_decls_init_nodes,
	.reg_reducers = NULL,
	.init_dfatrans = NULL,
	.post_setup = guppy_decls_post_setup,
	.ident = "guppy-decls"
};

/*}}}*/

