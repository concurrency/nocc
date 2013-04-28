/*
 *	guppy_decls.c -- variables and other named things
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
	int flags;
} fparaminfo_t;

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

	if (name->tag != gup.tag_NAME) {
		scope_error (name, ss, "name not raw-name!");
		return 0;
	}
	rawname = tnode_nthhookof (name, 0);

	sname = name_lookupss (rawname, ss);
	if (sname) {
		/* resolved */
		*node = NameNodeOf (sname);
		tnode_free (name);
	} else {
		scope_error (name, ss, "unresolved name \"%s\"", rawname);
	}

	return 1;
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

	if (bename) {
		tnode_t *tname = map->target->newnameref (bename, map);
		*node = tname;
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
/*{{{  static int guppy_isconst_namenode (langops_t *lops, tnode_t *node)*/
/*
 *	returns non-zero if the name is a constant (trivial)
 */
static int guppy_isconst_namenode (langops_t *lops, tnode_t *node)
{
	if ((node->tag == gup.tag_NVALABBR) || (node->tag == gup.tag_NVALPARAM)) {
		return 1;
	}
	return 0;
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
			if ((*rname == 's') || (*rname == 'S')) {
				return 2;
			}
			return 3;
		}
	}
	return 0;
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
	tnode_t *name = tnode_nthsubof (*node, 0);
	tnode_t *type = tnode_nthsubof (*node, 1);
	name_t *varname;
	tnode_t *newname;
	char *rawname;
	tnode_t *initdecl = NULL;
	ntdef_t *tag = NULL;
	int isabbrev = 0;

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

	rawname = (char *)tnode_nthhookof (name, 0);
	varname = name_addscopename (rawname, *node, type, NULL);

	if ((*node)->tag == gup.tag_VARDECL) {
		tag = isabbrev ? gup.tag_NABBR : gup.tag_NDECL;
	} else if ((*node)->tag == gup.tag_VALDECL) {
		tag = gup.tag_NVALABBR;
	} else {
		scope_error (*node, ss, "unknown vdecl type [%s]", (*node)->tag->name);
		return 0;
	}
	newname = tnode_createfrom (tag, name, varname);
	SetNameNode (varname, newname);
	if (initdecl) {
		/* put back scoped initialiser */
		tnode_setnthsub (*node, 2, initdecl);
	}
	tnode_setnthsub (*node, 0, newname);

	tnode_free (name);
	ss->scoped++;

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
	if (node->tag == gup.tag_VALDECL) {
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

	/* NOTE: bytesfor returns the number of workspace words required for a variable of some type */

	/* this will be target->wordsize in most cases */
	tsize = tnode_bytesfor (*typep, map->target);

	/* if we have an initialiser, map that first */
	if (*initp) {
		map_submapnames (initp, map);
	}
#if 0
fhandle_printf (FHAN_STDERR, "guppy_namemap_vdecl(): my-tag=[%s], *namep =\n", (*node)->tag->name);
tnode_dumptree (*namep, 1, FHAN_STDERR);
fhandle_printf (FHAN_STDERR, ">> *initp =\n");
tnode_dumptree (*initp, 1, FHAN_STDERR);
#endif
	bename = map->target->newname (*namep, NULL, map, tsize, 0, 0, 0, tsize, 0);
	cccsp_set_initialiser (bename, *initp);

	tnode_setchook (*namep, map->mapchook, (void *)bename);
	*node = bename;

	return 0;
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

	if (name->tag != gup.tag_NAME) {
		scope_error (*node, ss, "parameter name not raw-name, found [%s:%s]", name->tag->ndef->name, name->tag->name);
		return 0;
	}
	rawname = tnode_nthhookof (name, 0);

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
	int tsize, psize, indir = 0;

	if ((*typep)->tag == gup.tag_CHAN) {
		/* channel */
		tsize = map->target->chansize;
	} else {
		/* how big? */
		tsize = tnode_bytesfor (*typep, map->target);
	}
	psize = tsize;

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

	newstr = string_fmt ("%s %s", tstr, myname);
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

/*{{{  static int guppy_codegen_fparaminit (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	does code-generation for a formal-parameter initialiser
 *	returns 0 to stop walk, 1 to continue
 */
static int guppy_codegen_fparaminit (compops_t *cops, tnode_t *node, codegen_t *cgen)
{
	int pno = constprop_intvalof (tnode_nthsubof (node, 0));
	tnode_t *type = tnode_nthsubof (node, 1);
	char *ctype = NULL;

	langops_getctypeof (type, &ctype);
	codegen_write_fmt (cgen, "ProcGetParam (wptr, %d, %s)", pno, ctype ?: "int32_t");
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
		if ((items[i]->tag == gup.tag_VARDECL) || (items[i]->tag == gup.tag_VALDECL)) {
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

						parser_delfromlist (vdname, j);
						j--, nvitems--;
						parser_insertinlist (dlist, newdecl, i+1);
						nitems++;
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
		if ((items[i]->tag == gup.tag_VARDECL) || (items[i]->tag == gup.tag_VALDECL)) {
			tnode_t *vname = tnode_nthsubof (items[i], 0);

			if (vname->tag == gup.tag_ASSIGN) {
				/* this one, pull apart */
				tnode_t *namecopy = tnode_copytree (tnode_nthsubof (vname, 0));		/* LHS copy */
				tnode_t *seqblock, *instlist;

				tnode_setnthsub (items[i], 0, namecopy);
				instlist = parser_newlistnode (OrgFileOf (items[i]));
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
fprintf (stderr, "guppy_scopein_declblock(): here!  complete block is:\n");
tnode_dumptree (*node, 1, stderr);
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
	// codegen_write_fmt
	return 1;
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
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (guppy_namemap_namenode));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "gettype", 2, LANGOPTYPE (guppy_gettype_namenode));
	tnode_setlangop (lops, "getname", 2, LANGOPTYPE (guppy_getname_namenode));
	tnode_setlangop (lops, "isconst", 1, LANGOPTYPE (guppy_isconst_namenode));
	tnode_setlangop (lops, "guesstlp", 1, LANGOPTYPE (guppy_guesstlp_namenode));
	tnd->lops = lops;

	i = -1;
	gup.tag_NDECL = tnode_newnodetag ("N_DECL", &i, tnd, NTF_NONE);
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
	gup.tag_NFIELD = tnode_newnodetag ("N_FIELD", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_NFCNDEF = tnode_newnodetag ("N_FCNDEF", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_NENUM = tnode_newnodetag ("N_ENUM", &i, tnd, NTF_NONE);
	i = -1;
	gup.tag_NENUMVAL = tnode_newnodetag ("N_ENUMVAL", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  guppy:vdecl -- VARDECL, VALDECL*/
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
	gup.tag_VALDECL = tnode_newnodetag ("VALDECL", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  guppy:fparam -- FPARAM*/
	i = -1;
	tnd = tnode_newnodetype ("guppy:fparam", &i, 3, 0, 0, TNF_NONE);				/* subnodes: name; type; initialiser, hooks: fparaminfo */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (guppy_scopein_fparam));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (guppy_namemap_fparam));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "getdescriptor", 2, LANGOPTYPE (guppy_getdescriptor_fparam));
	tnd->lops = lops;

	i = -1;
	gup.tag_FPARAM = tnode_newnodetag ("FPARAM", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  guppy:fparaminit -- FPARAMINIT*/
	i = -1;
	tnd = tnode_newnodetype ("guppy:fparaminit", &i, 2, 0, 0, TNF_NONE);				/* subnodes: parameter, type */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (guppy_codegen_fparaminit));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnd->lops = lops;

	i = -1;
	gup.tag_FPARAMINIT = tnode_newnodetag ("FPARAMINIT", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  guppy:declblock -- DECLBLOCK*/
	i = -1;
	tnd = tnode_newnodetype ("guppy:declblock", &i, 2, 0, 0, TNF_NONE);				/* subnodes: decls; process */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "autoseq", 2, COMPOPTYPE (guppy_autoseq_declblock));
	tnode_setcompop (cops, "flattenseq", 1, COMPOPTYPE (guppy_flattenseq_declblock));
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (guppy_scopein_declblock));
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
	tnd->ops = cops;

	i = -1;
	gup.tag_ENUMDEF = tnode_newnodetag ("ENUMDEF", &i, tnd, NTF_INDENTED_NAME_LIST);

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

