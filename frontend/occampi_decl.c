/*
 *	occampi_decl.c -- occam-pi declaration and name handling for NOCC
 *	Copyright (C) 2005 Fred Barnes <frmb@kent.ac.uk>
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
 *	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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
#include "symbols.h"
#include "keywords.h"
#include "lexer.h"
#include "lexpriv.h"
#include "tnode.h"
#include "parser.h"
#include "dfa.h"
#include "parsepriv.h"
#include "occampi.h"
#include "names.h"
#include "scope.h"
#include "prescope.h"
#include "map.h"
#include "target.h"
#include "transputer.h"
#include "codegen.h"


/*}}}*/

/*
 *	this file contains the compiler front-end routines for occam-pi
 *	declarations, parameters and names.
 */


/*{{{  static int occampi_prescope_vardecl (tnode_t **node, prescope_t *ps)*/
/*
 *	called to prescope a variable declaration
 */
static int occampi_prescope_vardecl (tnode_t **node, prescope_t *ps)
{
	tnode_t *name = tnode_nthsubof (*node, 0);
	tnode_t *type = tnode_nthsubof (*node, 1);
	tnode_t **bodyptr = tnode_nthsubaddr (*node, 2);

	if (parser_islistnode (name)) {
		/* need to break up the list into individual declarations */
		int nnames, i;
		tnode_t **namelist = parser_getlistitems (name, &nnames);

		/* change this one */
		tnode_setnthsub (*node, 0, namelist[0]);
		namelist[0] = NULL;

		/* change the rest */
		for (i=1; i<nnames; i++) {
			tnode_t *newdecl = tnode_createfrom (opi.tag_VARDECL, namelist[i], namelist[i], tnode_copytree (type), *bodyptr);

			*bodyptr = newdecl;
			bodyptr = tnode_nthsubaddr (newdecl, 2);
			namelist[i] = NULL;
		}

		/* free the list of names */
		tnode_free (name);
	}
	return 1;
}
/*}}}*/
/*{{{  static int occampi_scopein_vardecl (tnode_t **node, scope_t *ss)*/
/*
 *	called to scope a variable declaration
 */
static int occampi_scopein_vardecl (tnode_t **node, scope_t *ss)
{
	tnode_t *name = tnode_nthsubof (*node, 0);
	tnode_t *type = tnode_nthsubof (*node, 1);
	char *rawname;
	name_t *sname = NULL;
	tnode_t *newname;

	if (name->tag != opi.tag_NAME) {
		scope_error (name, ss, "name not raw-name!");
		return 0;
	}
	rawname = tnode_nthhookof (name, 0);
#if 0
fprintf (stderr, "occampi_scopein_vardecl: here! rawname = \"%s\"\n", rawname);
#endif

	sname = name_addscopename (rawname, *node, type, NULL);
	newname = tnode_createfrom (opi.tag_NDECL, name, sname);
	SetNameNode (sname, newname);
	tnode_setnthsub (*node, 0, newname);

	/* free the old name */
	tnode_free (name);
	ss->scoped++;
	return 1;
}
/*}}}*/
/*{{{  static int occampi_scopeout_vardecl (tnode_t **node, scope_t *ss)*/
/*
 *	called to scope-out a variable declaration
 */
static int occampi_scopeout_vardecl (tnode_t **node, scope_t *ss)
{
	tnode_t *name = tnode_nthsubof (*node, 0);
	name_t *sname;

	if (name->tag != opi.tag_NDECL) {
		scope_error (name, ss, "not NDECL!");
		return 0;
	}
	sname = tnode_nthnameof (name, 0);

#if 0
fprintf (stderr, "occampi_scopein_vardecl: here! sname->me->name = \"%s\"\n", sname->me->name);
#endif

	name_descopename (sname);

	return 1;
}
/*}}}*/
/*{{{  static int occampi_namemap_vardecl (tnode_t **node, map_t *map)*/
/*
 *	transforms the name declared into a back-end name
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_namemap_vardecl (tnode_t **node, map_t *map)
{
	tnode_t **namep = tnode_nthsubaddr (*node, 0);
	tnode_t *type = tnode_nthsubof (*node, 1);
	tnode_t **bodyp = tnode_nthsubaddr (*node, 2);
	tnode_t *bename;
	int tsize;

#if 0
fprintf (stderr, "occampi_namemap_vardecl(): here!  target is [%s].  Type is:\n", map->target->name);
tnode_dumptree (type, 1, stderr);
#endif
	if (type->tag == opi.tag_CHAN) {
		/* channels need 1 word */
		tsize = map->target->chansize;
	} else {
		/* see how big this type is */
		tsize = tnode_bytesfor (type);
	}

	bename = map->target->newname (*namep, *bodyp, map, tsize, 0, 0, 0, tsize, 0);		/* FIXME! */
	tnode_setchook (*namep, map->mapchook, (void *)bename);
#if 0
fprintf (stderr, "got new bename:\n");
tnode_dumptree (bename, 1, stderr);
#endif

	*node = bename;
	bodyp = tnode_nthsubaddr (*node, 1);
	// tnode_setnthsub (*node, 0, bename);

	/* scope the body */
	map_submapnames (bodyp, map);
	return 0;
}
/*}}}*/

/*{{{  static int occampi_prescope_procdecl (tnode_t **node, prescope_t *ps)*/
/*
 *	called to prescope a PROC definition
 */
static int occampi_prescope_procdecl (tnode_t **node, prescope_t *ps)
{
	occampi_prescope_t *ops = (occampi_prescope_t *)(ps->hook);

	ops->last_type = NULL;
	if (!parser_islistnode (tnode_nthsubof (*node, 1))) {
		/* turn single parameter into a list-node */
		tnode_t *list = parser_newlistnode (NULL);

		parser_addtolist (list, tnode_nthsubof (*node, 1));
		tnode_setnthsub (*node, 1, list);
	}
	return 1;
}
/*}}}*/
/*{{{  static int occampi_scopein_procdecl (tnode_t **node, scope_t *ss)*/
/*
 *	called to scope a PROC definition
 */
static int occampi_scopein_procdecl (tnode_t **node, scope_t *ss)
{
	tnode_t *name = tnode_nthsubof (*node, 0);
	tnode_t **paramsptr = tnode_nthsubaddr (*node, 1);
	tnode_t **bodyptr = tnode_nthsubaddr (*node, 2);
	void *nsmark;
	char *rawname;
	name_t *procname;
	tnode_t *newname;

	nsmark = name_markscope ();

	/* walk parameters and body */
	tnode_modprepostwalktree (paramsptr, scope_modprewalktree, scope_modpostwalktree, (void *)ss);
	tnode_modprepostwalktree (bodyptr, scope_modprewalktree, scope_modpostwalktree, (void *)ss);

	name_markdescope (nsmark);

	/* declare and scope PROC name, then check process in the scope of it */
	rawname = tnode_nthhookof (name, 0);
	procname = name_addscopename (rawname, *node, *paramsptr, NULL);
	newname = tnode_createfrom (opi.tag_NPROCDEF, name, procname);
	SetNameNode (procname, newname);
	tnode_setnthsub (*node, 0, newname);

	/* free old name, check process */
	tnode_free (name);
	tnode_modprepostwalktree (tnode_nthsubaddr (*node, 3), scope_modprewalktree, scope_modpostwalktree, (void *)ss);
	ss->scoped++;

	return 0;		/* already walked child nodes */
}
/*}}}*/
/*{{{  static int occampi_scopeout_procdecl (tnode_t **node, scope_t *ss)*/
/*
 *	called to scope a PROC definition
 */
static int occampi_scopeout_procdecl (tnode_t **node, scope_t *ss)
{
	tnode_t *name = tnode_nthsubof (*node, 0);
	name_t *sname;

	if (name->tag != opi.tag_NPROCDEF) {
		scope_error (name, ss, "not NPROCDEF!");
		return 0;
	}
	sname = tnode_nthnameof (name, 0);

	name_descopename (sname);

	return 1;
}
/*}}}*/
/*{{{  static tnode_t *occampi_gettype_procdecl (tnode_t *node, tnode_t *defaulttype)*/
/*
 *	returns the type of a PROC definition (= parameter list)
 */
static tnode_t *occampi_gettype_procdecl (tnode_t *node, tnode_t *defaulttype)
{
	return tnode_nthsubof (node, 1);
}
/*}}}*/
/*{{{  static int occampi_namemap_procdecl (tnode_t **node, map_t *map)*/
/*
 *	name-maps a PROC definition
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_namemap_procdecl (tnode_t **node, map_t *map)
{
	tnode_t *blk;
	tnode_t *saved_blk = map->thisblock;
	tnode_t **saved_params = map->thisprocparams;
	tnode_t **paramsptr;
	tnode_t *tmpname;

	blk = map->target->newblock (tnode_nthsubof (*node, 2), map, tnode_nthsubof (*node, 1), map->lexlevel + 1);
	map->thisblock = blk;
	map->thisprocparams = tnode_nthsubaddr (*node, 1);
	map->lexlevel++;

	/* map formal params and body */
	paramsptr = tnode_nthsubaddr (*node, 1);
	map_submapnames (paramsptr, map);
	map_submapnames (tnode_nthsubaddr (blk, 0), map);		/* do this under the back-end block */
	map->lexlevel--;
	map->thisblock = saved_blk;
	map->thisprocparams = saved_params;

	/* insert the BLOCK node before the body of the process */
	tnode_setnthsub (*node, 2, blk);

	/* map scoped body */
	map_submapnames (tnode_nthsubaddr (*node, 3), map);

	/* add static-link, etc. if required and return-address */
	if (!parser_islistnode (*paramsptr)) {
		tnode_t *flist = parser_newlistnode (NULL);

		parser_addtolist (flist, *paramsptr);
		*paramsptr = flist;
	}

	tmpname = map->target->newname (tnode_create (opi.tag_HIDDENPARAM, NULL, tnode_create (opi.tag_RETURNADDRESS, NULL)), NULL, map,
			map->target->pointersize, 0, 0, 0, map->target->pointersize, 0);
	parser_addtolist_front (*paramsptr, tmpname);

	return 0;
}
/*}}}*/
/*{{{  static int occampi_codegen_procdecl (tnode_t *node, codegen_t *cgen)*/
/*
 *	generates code for a PROC definition
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_codegen_procdecl (tnode_t *node, codegen_t *cgen)
{
	tnode_t *body = tnode_nthsubof (node, 2);
	tnode_t *name = tnode_nthsubof (node, 0);
	int ws_size, vs_size, ms_size;
	int ws_offset, adjust;
	name_t *pname;

	body = tnode_nthsubof (node, 2);
	cgen->target->be_getblocksize (body, &ws_size, &ws_offset, &vs_size, &ms_size, &adjust);

	pname = tnode_nthnameof (name, 0);
	codegen_callops (cgen, comment, "PROC %s = %d,%d,%d,%d,%d", pname->me->name, ws_size, ws_offset, vs_size, ms_size, adjust);
	codegen_callops (cgen, setwssize, ws_size, adjust);
	codegen_callops (cgen, setvssize, vs_size);
	codegen_callops (cgen, setmssize, ms_size);
	codegen_callops (cgen, setnamedlabel, pname->me->name);

	/* adjust workspace and generate code for body */
	codegen_callops (cgen, wsadjust, -(ws_offset - adjust));
	codegen_subcodegen (tnode_nthsubof (body, 0), cgen);
	codegen_callops (cgen, wsadjust, (ws_offset - adjust));

	/* return */
	codegen_callops (cgen, procreturn);

	/* generate code following declaration */
	codegen_subcodegen (tnode_nthsubof (node, 3), cgen);
#if 0
fprintf (stderr, "occampi_codegen_procdecl!\n");
#endif
	return 0;
}
/*}}}*/


/*{{{  static int occampi_prescope_fparam (tnode_t **node, prescope_t *ps)*/
/*
 *	called to prescope a formal parameter
 */
static int occampi_prescope_fparam (tnode_t **node, prescope_t *ps)
{
	occampi_prescope_t *ops = (occampi_prescope_t *)(ps->hook);

#if 0
fprintf (stderr, "occampi_prescope_fparam(): prescoping formal parameter!\n");
#endif

	if (tnode_nthsubof (*node, 1)) {
		ops->last_type = tnode_nthsubof (*node, 1);
	} else if (!ops->last_type) {
		prescope_error (*node, ps, "missing type on formal parameter");
	} else {
		/* set type field for formal parameter */
		tnode_setnthsub (*node, 1, tnode_copytree (ops->last_type));
	}
	return 1;
}
/*}}}*/
/*{{{  static int occampi_scopein_fparam (tnode_t **node, scope_t *ss)*/
/*
 *	called to scope a formal parmeter
 */
static int occampi_scopein_fparam (tnode_t **node, scope_t *ss)
{
	tnode_t *name = tnode_nthsubof (*node, 0);
	tnode_t *type = tnode_nthsubof (*node, 1);
	char *rawname;
	name_t *sname = NULL;
	tnode_t *newname;

	if (name->tag != opi.tag_NAME) {
		scope_error (name, ss, "name not raw-name!");
		return 0;
	}
	rawname = tnode_nthhookof (name, 0);
#if 0
fprintf (stderr, "occampi_scopein_fparam: here! rawname = \"%s\"\n", rawname);
#endif

	sname = name_addscopename (rawname, *node, type, NULL);
	newname = tnode_createfrom (opi.tag_NPARAM, name, sname);
	SetNameNode (sname, newname);
	tnode_setnthsub (*node, 0, newname);

	/* free the old name */
	tnode_free (name);
	ss->scoped++;
	return 1;
}
/*}}}*/
/*{{{  static tnode_t *occampi_gettype_fparam (tnode_t *node, tnode_t *defaulttype)*/
/*
 *	returns the type of a formal parameter
 */
static tnode_t *occampi_gettype_fparam (tnode_t *node, tnode_t *defaulttype)
{
	return tnode_nthsubof (node, 1);
}
/*}}}*/
/*{{{  static int occampi_namemap_fparam (tnode_t **node, map_t *map)*/
/*
 *	transforms a formal parameter into a back-end name
 *	returns 0 to stop walk, 1 to continue;
 */
static int occampi_namemap_fparam (tnode_t **node, map_t *map)
{
	tnode_t **namep = tnode_nthsubaddr (*node, 0);
	tnode_t *type = tnode_nthsubof (*node, 1);
	tnode_t *bename;
	int tsize, indir;

#if 0
fprintf (stderr, "occampi_namemap_fparam(): here!  target is [%s].  Type is:\n", map->target->name);
tnode_dumptree (type, 1, stderr);
#endif
	if (type->tag == opi.tag_CHAN) {
		/* channels need 1 word */
		tsize = map->target->chansize;
	} else {
		/* see how big this type is */
		tsize = tnode_bytesfor (type);
	}

	if ((*namep)->tag == opi.tag_NPARAM) {
		indir = 1;
	} else {
		indir = 0;
	}
	bename = map->target->newname (*namep, NULL, map, 4, 0, 0, 0, tsize, indir);		/* FIXME! */
	tnode_setchook (*namep, map->mapchook, (void *)bename);

	*node = bename;
	return 0;
}
/*}}}*/

/*{{{  static void occampi_rawnamenode_hook_free (void *hook)*/
/*
 *	frees a rawnamenode hook (name-bytes)
 */
static void occampi_rawnamenode_hook_free (void *hook)
{
	if (hook) {
		sfree (hook);
	}
	return;
}
/*}}}*/
/*{{{  static void *occampi_rawnamenode_hook_copy (void *hook)*/
/*
 *	copies a rawnamenode hook (name-bytes)
 */
static void *occampi_rawnamenode_hook_copy (void *hook)
{
	char *rawname = (char *)hook;

	if (rawname) {
		return string_dup (rawname);
	}
	return NULL;
}
/*}}}*/
/*{{{  static void occampi_rawnamenode_hook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	dump-tree for rawnamenode hook (name-bytes)
 */
static void occampi_rawnamenode_hook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	occampi_isetindent (stream, indent);
	fprintf (stream, "<rawnamenode value=\"%s\" />\n", hook ? (char *)hook : "(null)");
	return;
}
/*}}}*/

/*{{{  static int occampi_scopein_rawname (tnode_t **node, scope_t *ss)*/
/*
 *	scopes in a free-floating name
 */
static int occampi_scopein_rawname (tnode_t **node, scope_t *ss)
{
	tnode_t *name = *node;
	char *rawname;
	name_t *sname = NULL;

	if (name->tag != opi.tag_NAME) {
		scope_error (name, ss, "name not raw-name!");
		return 0;
	}
	rawname = tnode_nthhookof (name, 0);

#if 0
fprintf (stderr, "occampi_scopein_rawname: here! rawname = \"%s\"\n", rawname);
#endif
	sname = name_lookup (rawname);
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

/*{{{  static tnode_t *occampi_gettype_namenode (tnode_t *node, tnode_t *default_type)*/
/*
 *	returns the type of a name-node (trivial)
 */
static tnode_t *occampi_gettype_namenode (tnode_t *node, tnode_t *default_type)
{
	name_t *name = tnode_nthnameof (node, 0);

	if (!name) {
		nocc_fatal ("occampi_gettype_namenode(): NULL name!");
		return NULL;
	}
	if (name->type) {
		return name->type;
	}
	nocc_fatal ("occampi_gettype_namenode(): name has NULL type (FIXME!)");
	return NULL;
}
/*}}}*/
/*{{{  static int occampi_namemap_namenode (tnode_t **node, map_t *map)*/
/*
 *	transforms given name into a back-end name
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_namemap_namenode (tnode_t **node, map_t *map)
{
	tnode_t *bename = tnode_getchook (*node, map->mapchook);
	tnode_t *tname;

#if 0
fprintf (stderr, "occampi_namemap_namenode(): here 1! bename =\n");
tnode_dumptree (bename, 1, stderr);
#endif
	if (bename) {
		tname = map->target->newnameref (bename, map);
		*node = tname;
	}
	return 0;
}
/*}}}*/


/*{{{  int occampi_decl_nodes_init (void)*/
/*
 *	sets up declaration and name nodes for occam-pi
 *	returns 0 on success, non-zero on error
 */
int occampi_decl_nodes_init (void)
{
	tndef_t *tnd;
	compops_t *cops;
	int i;

	/*{{{  occampi:rawnamenode -- NAME*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:rawnamenode", &i, 0, 0, 1, TNF_NONE);
	tnd->hook_free = occampi_rawnamenode_hook_free;
	tnd->hook_copy = occampi_rawnamenode_hook_copy;
	tnd->hook_dumptree = occampi_rawnamenode_hook_dumptree;
	cops = tnode_newcompops ();
	cops->scopein = occampi_scopein_rawname;
	tnd->ops = cops;
	i = -1;
	opi.tag_NAME = tnode_newnodetag ("NAME", &i, tnd, NTF_NONE);
	/*}}}*/
	/*{{{  occampi:namenode -- N_DECL, N_PARAM, N_PROCDEF*/
	i = -1;
	tnd = opi.node_NAMENODE = tnode_newnodetype ("occampi:namenode", &i, 0, 1, 0, TNF_NONE);
	cops = tnode_newcompops ();
	cops->gettype = occampi_gettype_namenode;
	cops->namemap = occampi_namemap_namenode;
	tnd->ops = cops;
	i = -1;
	opi.tag_NDECL = tnode_newnodetag ("N_DECL", &i, opi.node_NAMENODE, NTF_NONE);
	i = -1;
	opi.tag_NPARAM = tnode_newnodetag ("N_PARAM", &i, opi.node_NAMENODE, NTF_NONE);
	i = -1;
	opi.tag_NPROCDEF = tnode_newnodetag ("N_PROCDEF", &i, opi.node_NAMENODE, NTF_NONE);
	/*}}}*/
	/*{{{  occampi:hiddennode -- HIDDENPARAM*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:hiddennode", &i, 1, 0, 0, TNF_NONE);
	i = -1;
	opi.tag_HIDDENPARAM = tnode_newnodetag ("HIDDENPARAM", &i, tnd, NTF_NONE);
	/*}}}*/
	/*{{{  occampi:leafnode -- RETURNADDRESS*/
	tnd = tnode_lookupnodetype ("occampi:leafnode");
	i = -1;
	opi.tag_RETURNADDRESS = tnode_newnodetag ("RETURNADDRESS", &i, tnd, NTF_NONE);
	/*}}}*/
	/*{{{  occampi:vardecl -- VARDECL*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:vardecl", &i, 3, 0, 0, TNF_SHORTDECL);
	cops = tnode_newcompops ();
	cops->prescope = occampi_prescope_vardecl;
	cops->scopein = occampi_scopein_vardecl;
	cops->scopeout = occampi_scopeout_vardecl;
	cops->namemap = occampi_namemap_vardecl;
	tnd->ops = cops;
	i = -1;
	opi.tag_VARDECL = tnode_newnodetag ("VARDECL", &i, tnd, NTF_NONE);
	/*}}}*/
	/*{{{  occampi:fparam -- FPARAM*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:fparam", &i, 2, 0, 0, TNF_NONE);
	cops = tnode_newcompops ();
	cops->prescope = occampi_prescope_fparam;
	cops->scopein = occampi_scopein_fparam;
	cops->gettype = occampi_gettype_fparam;
	cops->namemap = occampi_namemap_fparam;
	tnd->ops = cops;
	i = -1;
	opi.tag_FPARAM = tnode_newnodetag ("FPARAM", &i, tnd, NTF_NONE);
	/*}}}*/
	/*{{{  occampi:procdecl -- PROCDECL*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:procdecl", &i, 4, 0, 0, TNF_LONGDECL);
	cops = tnode_newcompops ();
	cops->prescope = occampi_prescope_procdecl;
	cops->scopein = occampi_scopein_procdecl;
	cops->scopeout = occampi_scopeout_procdecl;
	cops->namemap = occampi_namemap_procdecl;
	cops->gettype = occampi_gettype_procdecl;
	cops->codegen = occampi_codegen_procdecl;
	tnd->ops = cops;
	i = -1;
	opi.tag_PROCDECL = tnode_newnodetag ("PROCDECL", &i, tnd, NTF_NONE);
	/*}}}*/

	return 0;
}
/*}}}*/

