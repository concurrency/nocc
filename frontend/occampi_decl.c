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
#include "feunit.h"
#include "names.h"
#include "scope.h"
#include "prescope.h"
#include "library.h"
#include "typecheck.h"
#include "precheck.h"
#include "usagecheck.h"
#include "map.h"
#include "target.h"
#include "transputer.h"
#include "codegen.h"
#include "langops.h"


/*}}}*/

/*
 *	this file contains the compiler front-end routines for occam-pi
 *	declarations, parameters and names.
 */



/*{{{  static void occampi_initchandecl (tnode_t *node, codegen_t *cgen, void *arg)*/
/*
 *	does initialiser code-gen for a channel declaration
 */
static void occampi_initchandecl (tnode_t *node, codegen_t *cgen, void *arg)
{
	tnode_t *chantype = (tnode_t *)arg;
	int ws_off, vs_off, ms_off;

	/* FIXME: assuming single channel for now.. */
	cgen->target->be_getoffsets (node, &ws_off, &vs_off, &ms_off);

#if 0
fprintf (stderr, "occampi_initchandecl(): node=[%s], allocated at [%d,%d,%d], type is:\n", node->tag->name, ws_off, vs_off, ms_off);
tnode_dumptree (chantype, 1, stderr);
#endif
	codegen_callops (cgen, loadconst, 0);
	codegen_callops (cgen, storelocal, ws_off);
	codegen_callops (cgen, comment, "initchandecl");

	return;
}
/*}}}*/
/*{{{  static void occampi_initabbrev (tnode_t *node, codegen_t *cgen, void *arg)*/
/*
 *	does initialiser code-gen for an abbreviation
 */
static void occampi_initabbrev (tnode_t *node, codegen_t *cgen, void *arg)
{
	tnode_t *abbrev = (tnode_t *)arg;
	int ws_off, vs_off, ms_off;
	tnode_t *mappedrhs;

	cgen->target->be_getoffsets (node, &ws_off, &vs_off, &ms_off);
#if 0
fprintf (stderr, "occampi_initabbrev(): node=[%s], allocated at [%d,%d,%d]:\n", node->tag->name, ws_off, vs_off, ms_off);
tnode_dumptree (node, 1, stderr);
#endif
	mappedrhs = (tnode_t *)tnode_getchook (node, cgen->pc_chook);

#if 0
fprintf (stderr, "occampi_initabbrev(): node=[%s], mappedrhs =\n", node->tag->name);
tnode_dumptree (mappedrhs, 1, stderr);
#endif
	if (abbrev->tag == opi.tag_VALABBREV) {
		codegen_callops (cgen, loadname, mappedrhs, 0);
	} else {
		codegen_callops (cgen, loadpointer, mappedrhs, 0);
	}
	codegen_callops (cgen, storelocal, ws_off);
	codegen_callops (cgen, comment, "initabbrev");
	return;
}
/*}}}*/

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
	tnode_t *type;
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

	if (scope_subtree (tnode_nthsubaddr (*node, 1), ss)) {
		/* failed to scope type */
		return 0;
	}

	type = tnode_nthsubof (*node, 1);

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
fprintf (stderr, "occampi_scopeout_vardecl: here! sname->me->name = \"%s\"\n", sname->me->name);
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
	void (*initfunc)(tnode_t *, codegen_t *, void *) = NULL;
	void *initarg = NULL;

#if 0
fprintf (stderr, "occampi_namemap_vardecl(): here!  target is [%s].  Type is:\n", map->target->name);
tnode_dumptree (type, 1, stderr);
#endif
	if (type->tag == opi.tag_CHAN) {
		/* channels need 1 word */
		tsize = map->target->chansize;
		/* also need initialising */
		initfunc = occampi_initchandecl;
		initarg = (void *)type;
	} else {
		/* see how big this type is */
		tsize = tnode_bytesfor (type, map->target);
	}

	bename = map->target->newname (*namep, *bodyp, map, tsize, 0, 0, 0, tsize, 0);		/* FIXME! */

	if (initfunc) {
		/*{{{  set initialiser up*/
		codegen_setinithook (bename, initfunc, initarg);

		/*}}}*/
	}
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


/*{{{  static int occampi_prescope_abbrev (tnode_t **nodep, prescope_t *ps)*/
/*
 *	called to prescope an abbreviation declaration
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_prescope_abbrev (tnode_t **nodep, prescope_t *ps)
{
	tnode_t *name = tnode_nthsubof (*nodep, 0);

	if (parser_islistnode (name)) {
		prescope_error (*nodep, ps, "occampi_prescope_abbrev(): unexpected list");
	}
	return 1;
}
/*}}}*/
/*{{{  static int occampi_scopein_abbrev (tnode_t **nodep, scope_t *ss)*/
/*
 *	called to scope an abbreviation
 *	returns 0 to stop walk, 1 to continue, -1 to stop and prevent scopeout
 */
static int occampi_scopein_abbrev (tnode_t **nodep, scope_t *ss)
{
	tnode_t *name = tnode_nthsubof (*nodep, 0);
	tnode_t *type;
	char *rawname;
	name_t *sname = NULL;
	tnode_t *newname;

	if (name->tag != opi.tag_NAME) {
		scope_error (name, ss, "name not raw-name!");
		return 0;
	}
	rawname = tnode_nthhookof (name, 0);
#if 0
fprintf (stderr, "occampi_scopein_abbrev: here! rawname = \"%s\"\n", rawname);
#endif

	if (scope_subtree (tnode_nthsubaddr (*nodep, 1), ss)) {
		/* failed to scope type */
		return -1;
	}

	type = tnode_nthsubof (*nodep, 1);

	if (scope_subtree (tnode_nthsubaddr (*nodep, 3), ss)) {
		/* failed to scope RHS */
		return -1;
	}

	sname = name_addscopename (rawname, *nodep, type, NULL);
	if ((*nodep)->tag == opi.tag_VALABBREV) {
		newname = tnode_createfrom (opi.tag_NVALABBR, name, sname);
	} else {
		newname = tnode_createfrom (opi.tag_NABBR, name, sname);
	}
	SetNameNode (sname, newname);
	tnode_setnthsub (*nodep, 0, newname);

	/* free the old name */
	tnode_free (name);
	ss->scoped++;

#if 0
fprintf (stderr, "occampi_scopein_abbrev(): body before scoping it is:\n");
tnode_dumptree (tnode_nthsubof (*nodep, 2), 1, stderr);
#endif
	scope_subtree (tnode_nthsubaddr (*nodep, 2), ss);

	return 0;
}
/*}}}*/
/*{{{  static int occampi_scopeout_abbrev (tnode_t **nodep, scope_t *ss)*/
/*
 *	called to scope-out an abbreviation
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_scopeout_abbrev (tnode_t **nodep, scope_t *ss)
{
	tnode_t *name = tnode_nthsubof (*nodep, 0);
	name_t *sname;

	if (name->tag != (((*nodep)->tag == opi.tag_VALABBREV) ? opi.tag_NVALABBR : opi.tag_NABBR)) {
		scope_error (name, ss, "not NABBR/NVALABBR!");
		return 0;
	}
	sname = tnode_nthnameof (name, 0);

#if 0
fprintf (stderr, "occampi_scopeout_abbrev: here! sname->me->name = \"%s\"\n", sname->me->name);
#endif

	name_descopename (sname);

	return 1;
}
/*}}}*/
/*{{{  static int occampi_typecheck_abbrev (tnode_t *node, typecheck_t *tc)*/
/*
 *	does type-checking for an abbreviation -- mainly to ensure that the type is present/inferred
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_typecheck_abbrev (tnode_t *node, typecheck_t *tc)
{
	tnode_t **namep = tnode_nthsubaddr (node, 0);
	tnode_t **typep = tnode_nthsubaddr (node, 1);
	tnode_t **rhsp = tnode_nthsubaddr (node, 3);
	tnode_t **xtypep = NULL;

	if ((*namep)->tag->ndef == opi.node_NAMENODE) {
		name_t *name = tnode_nthnameof (*namep, 0);
		
		xtypep = NameTypeAddr (name);
	}

#if 0
fprintf (stderr, "occampi_typecheck_abbrev(): *xtypep=0x%8.8x, *typep=0x%8.8x, *rhsp=\n", (unsigned int)(*xtypep), (unsigned int)(*typep));
tnode_dumptree (*rhsp, 1, stderr);
#endif
	if (*typep && xtypep && !*xtypep) {
		tnode_t *rtype;

		*xtypep = *typep;		/* set NAMENODE type to type in abbreviation */
		/* typecheck RHS */
		rtype = typecheck_gettype (*rhsp, *typep);
		if (!rtype) {
			typecheck_error (node, tc, "failed to get type from RHS for abbreviation");
			return 0;
		} else {
			typecheck_typeactual (*typep, rtype, node, tc);
		}
	} else if (!*typep && xtypep && *xtypep) {
		tnode_t *rtype;

		*typep = *xtypep;		/* set ABBRNODE to type in name */
		/* typecheck RHS */
		rtype = typecheck_gettype (*rhsp, *typep);
		if (!rtype) {
			typecheck_error (node, tc, "failed to get type from RHS for abbreviation");
			return 0;
		} else {
			typecheck_typeactual (*typep, rtype, node, tc);
		}
	} else if (!*typep && (!xtypep || !*xtypep)) {
		tnode_t *rtype;

		/* no type, look at RHS */
		if (typecheck_subtree (*rhsp, tc)) {
			return 0;
		}
		rtype = typecheck_gettype (*rhsp, NULL);
		if (!rtype) {
			typecheck_error (node, tc, "failed to get type from RHS for abbreviation");
			return 0;
		}
		*typep = tnode_copytree (rtype);
		if (xtypep && !*xtypep) {
			*xtypep = *typep;
		}
	} else {
		tnode_t *rtype;

		/* typecheck RHS */
		rtype = typecheck_gettype (*rhsp, *typep);
		if (!rtype) {
			typecheck_error (node, tc, "failed to get type from RHS for abbreviation");
			return 0;
		} else {
			typecheck_typeactual (*typep, rtype, node, tc);
		}
	}
	/* typecheck body */
	typecheck_subtree (tnode_nthsubof (node, 2), tc);

	return 0;
}
/*}}}*/
/*{{{  static int occampi_namemap_abbrev (tnode_t **nodep, map_t *map)*/
/*
 *	transforms an abbreviation into a back-end name
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_namemap_abbrev (tnode_t **nodep, map_t *map)
{
	tnode_t **namep = tnode_nthsubaddr (*nodep, 0);
	tnode_t *type = tnode_nthsubof (*nodep, 1);
	tnode_t **bodyp = tnode_nthsubaddr (*nodep, 2);
	tnode_t **rhsp = tnode_nthsubaddr (*nodep, 3);
	tnode_t *bename;
	void (*initfunc)(tnode_t *, codegen_t *, void *) = NULL;
	void *initarg = NULL;

#if 0
fprintf (stderr, "occampi_namemap_abbrev(): here!  target is [%s].  Type is:\n", map->target->name);
tnode_dumptree (type, 1, stderr);
#endif
	initfunc = occampi_initabbrev;
	initarg = *nodep;	/* handle on the original abbreviation */

	bename = map->target->newname (*namep, *bodyp, map, map->target->pointersize, 0, 0, 0, map->target->pointersize, 1);

	/*{{{  set initialiser up*/
	codegen_setinithook (bename, initfunc, initarg);

	/*}}}*/

	/* map the RHS and attach to it gets mapped */
	map_submapnames (rhsp, map);
#if 0
fprintf (stderr, "occampi_namemap_abbrev(): target is [%s].  mapped RHS, got:\n", map->target->name);
tnode_dumptree (*rhsp, 1, stderr);
#endif
	tnode_setchook (bename, map->allocevhook, (void *)*rhsp);
	tnode_setchook (bename, map->precodehook, (void *)*rhsp);

	tnode_setchook (*namep, map->mapchook, (void *)bename);
#if 0
fprintf (stderr, "got new bename:\n");
tnode_dumptree (bename, 1, stderr);
#endif

	*nodep = bename;
	bodyp = tnode_nthsubaddr (*nodep, 1);
	// tnode_setnthsub (*node, 0, bename);

	/* map the body */
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
	char *rawname = (char *)tnode_nthhookof (tnode_nthsubof (*node, 0), 0);

	if (library_makepublic (node, rawname)) {
		return 1;			/* go round again */
	}
	ops->last_type = NULL;
	if (!tnode_nthsubof (*node, 1)) {
		/* no parameters, create empty list */
		tnode_setnthsub (*node, 1, parser_newlistnode (NULL));
	} else if (tnode_nthsubof (*node, 1) && !parser_islistnode (tnode_nthsubof (*node, 1))) {
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
/*{{{  static int occampi_fetrans_procdecl (tnode_t **node)*/
/*
 *	does front-end transforms on a PROC definition
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_fetrans_procdecl (tnode_t **node)
{
	chook_t *deschook = tnode_lookupchookbyname ("fetrans:descriptor");
	char *dstr = NULL;

	if (!deschook) {
		return 1;
	}
	langops_getdescriptor (*node, &dstr);
	if (dstr) {
		tnode_setchook (*node, deschook, (void *)dstr);
	}

	return 1;
}
/*}}}*/
/*{{{  static int occampi_precheck_procdecl (tnode_t *node)*/
/*
 *	does pre-checking on PROC declaration
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_precheck_procdecl (tnode_t *node)
{
#if 0
fprintf (stderr, "occampi_precheck_procdecl(): here!\n");
#endif
	precheck_subtree (tnode_nthsubof (node, 2));		/* precheck this body */
	precheck_subtree (tnode_nthsubof (node, 3));		/* precheck in-scope code */
#if 0
fprintf (stderr, "occampi_precheck_procdecl(): returning 0\n");
#endif
	return 0;
}
/*}}}*/
/*{{{  static int occampi_usagecheck_procdecl (tnode_t *node, uchk_state_t *ucstate)*/
/*
 *	does usage-checking on a PROC declaration
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_usagecheck_procdecl (tnode_t *node, uchk_state_t *ucstate)
{
	usagecheck_begin_branches (node, ucstate);
	usagecheck_newbranch (ucstate);
	usagecheck_subtree (tnode_nthsubof (node, 2), ucstate);		/* usage-check this body */
	usagecheck_subtree (tnode_nthsubof (node, 3), ucstate);		/* usage-check in-scope code */
	usagecheck_endbranch (ucstate);
	usagecheck_end_branches (node, ucstate);
	return 0;
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
/*{{{  static int occampi_precode_procdecl (tnode_t **nodep, codegen_t *cgen)*/
/*
 *	pre-code for PROC definition, used to determine the last PROC in a file
 *	(entry-point if main module).
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_precode_procdecl (tnode_t **nodep, codegen_t *cgen)
{
	tnode_t *node = *nodep;
	tnode_t *name = tnode_nthsubof (node, 0);

	/* walk body */
	codegen_subprecode (tnode_nthsubaddr (node, 2), cgen);

	codegen_precode_seenproc (cgen, tnode_nthnameof (name, 0), node);

	/* pre-code stuff following declaration */
	codegen_subprecode (tnode_nthsubaddr (node, 3), cgen);
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
	cgen->target->be_getblocksize (body, &ws_size, &ws_offset, &vs_size, &ms_size, &adjust, NULL);

	pname = tnode_nthnameof (name, 0);
	codegen_callops (cgen, comment, "PROC %s = %d,%d,%d,%d,%d", pname->me->name, ws_size, ws_offset, vs_size, ms_size, adjust);
	codegen_callops (cgen, setwssize, ws_size, adjust);
	codegen_callops (cgen, setvssize, vs_size);
	codegen_callops (cgen, setmssize, ms_size);
	codegen_callops (cgen, setnamedlabel, pname->me->name);

	/* adjust workspace and generate code for body */
	// codegen_callops (cgen, wsadjust, -(ws_offset - adjust));
	codegen_subcodegen (body, cgen);
	// codegen_callops (cgen, wsadjust, (ws_offset - adjust));

	/* return */
	codegen_callops (cgen, procreturn, adjust);

	/* generate code following declaration */
	codegen_subcodegen (tnode_nthsubof (node, 3), cgen);
#if 0
fprintf (stderr, "occampi_codegen_procdecl!\n");
#endif
	return 0;
}
/*}}}*/
/*{{{  static int occampi_getdescriptor_procdecl (tnode_t *node, char **str)*/
/*
 *	generates a descriptor line for a PROC definition
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_getdescriptor_procdecl (tnode_t *node, char **str)
{
	tnode_t *name = tnode_nthsubof (node, 0);
	char *realname;
	tnode_t *params = tnode_nthsubof (node, 1);

	if (*str) {
		/* shouldn't get this here, but.. */
		nocc_warning ("occampi_getdescriptor_procdecl(): already had descriptor [%s]", *str);
		sfree (*str);
	}
	realname = NameNameOf (tnode_nthnameof (name, 0));
	*str = (char *)smalloc (strlen (realname) + 10);

	sprintf (*str, "PROC %s (", realname);
	if (parser_islistnode (params)) {
		int nitems, i;
		tnode_t **items = parser_getlistitems (params, &nitems);

		for (i=0; i<nitems; i++) {
			tnode_t *param = items[i];

			langops_getdescriptor (param, str);
			if (i < (nitems - 1)) {
				char *newstr = (char *)smalloc (strlen (*str) + 5);

				sprintf (newstr, "%s, ", *str);
				sfree (*str);
				*str = newstr;
			}
		}
	} else {
		langops_getdescriptor (params, str);
	}

	{
		char *newstr = (char *)smalloc (strlen (*str) + 5);

		sprintf (newstr, "%s)", *str);
		sfree (*str);
		*str = newstr;
	}
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
fprintf (stderr, "occampi_prescope_fparam(): prescoping formal parameter! *node = \n");
tnode_dumptree (*node, 1, stderr);
#endif

	if (tnode_nthsubof (*node, 1)) {
		tnode_t **namep = tnode_nthsubaddr (*node, 0);

		ops->last_type = tnode_nthsubof (*node, 1);
		if ((ops->last_type->tag == opi.tag_ASINPUT) || (ops->last_type->tag == opi.tag_ASOUTPUT)) {
			/* lose this from the type, associated primarily with name in FPARAMs */
			ops->last_type = tnode_nthsubof (ops->last_type, 0);
		}

		/* maybe fixup ASINPUT/ASOUTPUT here too */
		if (((*namep)->tag == opi.tag_ASINPUT) || ((*namep)->tag == opi.tag_ASOUTPUT)) {
			tnode_t *name = tnode_nthsubof (*namep, 0);
			tnode_t *type = *namep;

			tnode_setnthsub (type, 0, tnode_nthsubof (*node, 1));
			*namep = name;
			tnode_setnthsub (*node, 1, type);
		}
	} else if (!ops->last_type) {
		prescope_error (*node, ps, "missing type on formal parameter");
	} else {
		/* set type field for formal parameter */
		tnode_t **namep = tnode_nthsubaddr (*node, 0);

#if 0
fprintf (stderr, "occampi_prescope_fparam(): setting type on formal param, last_type = \n");
tnode_dumptree (ops->last_type, 1, stderr);
#endif
		if (((*namep)->tag == opi.tag_ASINPUT) || ((*namep)->tag == opi.tag_ASOUTPUT)) {
			tnode_t *name = tnode_nthsubof (*namep, 0);
			tnode_t *type = *namep;

			*namep = name;
			tnode_setnthsub (type, 0, tnode_copytree (ops->last_type));
			tnode_setnthsub (*node, 1, type);
		} else {
			tnode_setnthsub (*node, 1, tnode_copytree (ops->last_type));
		}
#if 0
fprintf (stderr, "occampi_prescope_fparam(): put in type, *node = \n");
tnode_dumptree (*node, 1, stderr);
#endif
	}
	return 1;
}
/*}}}*/
/*{{{  static int occampi_scopein_fparam (tnode_t **node, scope_t *ss)*/
/*
 *	called to scope a formal parmeter
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_scopein_fparam (tnode_t **node, scope_t *ss)
{
	tnode_t *name = tnode_nthsubof (*node, 0);
	tnode_t **typep = tnode_nthsubaddr (*node, 1);
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

	/* scope the type first */
	if (scope_subtree (typep, ss)) {
		return 0;
	}

	sname = name_addscopename (rawname, *node, *typep, NULL);
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
		tsize = tnode_bytesfor (type, map->target);
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
/*{{{  static int occampi_getdescriptor_fparam (tnode_t *node, char **str)*/
/*
 *	gets descriptor information for a formal parameter
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_getdescriptor_fparam (tnode_t *node, char **str)
{
	tnode_t *name = tnode_nthsubof (node, 0);
	tnode_t *type = tnode_nthsubof (node, 1);
	char *typestr = NULL;
	char *pname = NameNameOf (tnode_nthnameof (name, 0));

	/* get type first */
	langops_getdescriptor (type, &typestr);
	if (!typestr) {
		typestr = string_dup ("ANY");
	}

	if (*str) {
		char *newstr = (char *)smalloc (strlen (*str) + strlen (typestr) + strlen (pname) + 4);

		sprintf (newstr, "%s%s %s", *str, typestr, pname);
		sfree (*str);
		*str = newstr;
	} else {
		*str = (char *)smalloc (strlen (typestr) + strlen (pname) + 4);
		sprintf (*str, "%s %s", typestr, pname);
	}

	sfree (typestr);

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
#if 0
fprintf (stderr, "occmpi_gettype_namenode(): node = [%s], name:\n", node->tag->name);
name_dumpname (name, 1, stderr);
fprintf (stderr, "   \"   name->type:\n");
tnode_dumptree (name->type, 1, stderr);
#endif
		return name->type;
	}
	nocc_fatal ("occampi_gettype_namenode(): name has NULL type (FIXME!)");
	return NULL;
}
/*}}}*/
/*{{{  static int occampi_bytesfor_namenode (tnode_t *node, target_t *target)*/
/*
 *	returns the number of bytes in a name-node, associated with its type only
 */
static int occampi_bytesfor_namenode (tnode_t *node, target_t *target)
{
	if (node->tag == opi.tag_NDATATYPEDECL) {
		name_t *name = tnode_nthnameof (node, 0);
		tnode_t *type, *decl;

		type = NameTypeOf (name);
		decl = NameDeclOf (name);
#if 0
fprintf (stderr, "occampi_bytesfor_namenode(): type = ");
tnode_dumptree (type, 1, stderr);
#endif

		return tnode_bytesfor (decl, target);
	} else if (node->tag == opi.tag_NFIELD) {
		name_t *name = tnode_nthnameof (node, 0);
		tnode_t *type = NameTypeOf (name);

#if 0
fprintf (stderr, "occampi_bytesfor_namenode(): [N_FIELD], type =\n");
tnode_dumptree (type, 1, stderr);
#endif
		return tnode_bytesfor (type, target);
	}
	nocc_error ("occampi_bytesfor_namenode(): no bytes for [%s]", node->tag->name);
	return -1;
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
/*{{{  static int occampi_usagecheck_namenode (tnode_t *node, uchk_state_t *uc)*/
/*
 *	does usage-check on a namenode
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_usagecheck_namenode (tnode_t *node, uchk_state_t *uc)
{
	if (node->tag == opi.tag_NVALABBR) {
		/* allowed to be at the outermost lex-level.. */
		if (uc->ucptr < 0) {
			return 0;
		}
	}
	if (usagecheck_addname (node, uc, uc->defmode)) {
		return 0;
	}
	return 1;
}
/*}}}*/


/*{{{  static void occampi_procdecl_dfaeh_stuck (dfanode_t *dfanode, token_t *tok)*/
/*
 *	called by parser when it gets stuck in an occampi:procdecl DFA node
 */
static void occampi_procdecl_dfaeh_stuck (dfanode_t *dfanode, token_t *tok)
{
	char msgbuf[1024];
	int gone = 0;
	int max = 1023;

	gone += snprintf (msgbuf + gone, max - gone, "parse error at %s in PROC declaration", lexer_stokenstr (tok));
	if (DA_CUR (dfanode->match)) {
		int n;

		gone += snprintf (msgbuf + gone, max - gone, ", expected ");
		for (n=0; n<DA_CUR (dfanode->match); n++) {
			token_t *match = DA_NTHITEM (dfanode->match, n);

			gone += snprintf (msgbuf + gone, max - gone, "%s%s", !n ? "" : ((n == DA_CUR (dfanode->match) - 1) ? " or " : ", "), lexer_stokenstr (match));
		}
	}
	parser_error (tok->origin, msgbuf);
	return;
}
/*}}}*/



/*{{{  static int occampi_decl_init_nodes (void)*/
/*
 *	sets up declaration and name nodes for occam-pi
 *	returns 0 on success, non-zero on error
 */
static int occampi_decl_init_nodes (void)
{
	tndef_t *tnd;
	compops_t *cops;
	langops_t *lops;
	int i;

	/*{{{  occampi:rawnamenode -- NAME*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:rawnamenode", &i, 0, 0, 1, TNF_NONE);				/* hooks: raw-name */
	tnd->hook_free = occampi_rawnamenode_hook_free;
	tnd->hook_copy = occampi_rawnamenode_hook_copy;
	tnd->hook_dumptree = occampi_rawnamenode_hook_dumptree;
	cops = tnode_newcompops ();
	cops->scopein = occampi_scopein_rawname;
	tnd->ops = cops;

	i = -1;
	opi.tag_NAME = tnode_newnodetag ("NAME", &i, tnd, NTF_NONE);
	/*}}}*/
	/*{{{  occampi:namenode -- N_DECL, N_PARAM, N_VALPARAM, N_PROCDEF, N_DATATYPEDECL, N_FIELD, N_ABBR, N_VALABBR, N_FUNCDEF, N_CHANTYPEDECL, N_PROCTYPEDECL*/
	i = -1;
	tnd = opi.node_NAMENODE = tnode_newnodetype ("occampi:namenode", &i, 0, 1, 0, TNF_NONE);	/* subnames: name */
	cops = tnode_newcompops ();
	cops->gettype = occampi_gettype_namenode;
	cops->bytesfor = occampi_bytesfor_namenode;
	cops->namemap = occampi_namemap_namenode;
	tnd->ops = cops;

	lops = tnode_newlangops ();
	lops->do_usagecheck = occampi_usagecheck_namenode;
	tnd->lops = lops;

	i = -1;
	opi.tag_NDECL = tnode_newnodetag ("N_DECL", &i, opi.node_NAMENODE, NTF_NONE);
	i = -1;
	opi.tag_NPARAM = tnode_newnodetag ("N_PARAM", &i, opi.node_NAMENODE, NTF_NONE);
	i = -1;
	opi.tag_NVALPARAM = tnode_newnodetag ("N_VALPARAM", &i, opi.node_NAMENODE, NTF_NONE);
	i = -1;
	opi.tag_NPROCDEF = tnode_newnodetag ("N_PROCDEF", &i, opi.node_NAMENODE, NTF_NONE);
	i = -1;
	opi.tag_NDATATYPEDECL = tnode_newnodetag ("N_DATATYPEDECL", &i, opi.node_NAMENODE, NTF_NONE);
	i = -1;
	opi.tag_NFIELD = tnode_newnodetag ("N_FIELD", &i, opi.node_NAMENODE, NTF_NONE);
	i = -1;
	opi.tag_NABBR = tnode_newnodetag ("N_ABBR", &i, opi.node_NAMENODE, NTF_NONE);
	i = -1;
	opi.tag_NVALABBR = tnode_newnodetag ("N_VALABBR", &i, opi.node_NAMENODE, NTF_NONE);
	i = -1;
	opi.tag_NFUNCDEF = tnode_newnodetag ("N_FUNCDEF", &i, opi.node_NAMENODE, NTF_NONE);
	i = -1;
	opi.tag_NCHANTYPEDECL = tnode_newnodetag ("N_CHANTYPEDECL", &i, opi.node_NAMENODE, NTF_SYNCTYPE);
	i = -1;
	opi.tag_NPROCTYPEDECL = tnode_newnodetag ("N_PROCTYPEDECL", &i, opi.node_NAMENODE, NTF_NONE);
	/*}}}*/
	/*{{{  occampi:hiddennode -- HIDDENPARAM*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:hiddennode", &i, 1, 0, 0, TNF_NONE);			/* subnodes: hidden-param */
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
	tnd = tnode_newnodetype ("occampi:vardecl", &i, 3, 0, 0, TNF_SHORTDECL);		/* subnodes: name; type; in-scope-body */
	cops = tnode_newcompops ();
	cops->prescope = occampi_prescope_vardecl;
	cops->scopein = occampi_scopein_vardecl;
	cops->scopeout = occampi_scopeout_vardecl;
	cops->namemap = occampi_namemap_vardecl;
	tnd->ops = cops;
	i = -1;
	opi.tag_VARDECL = tnode_newnodetag ("VARDECL", &i, tnd, NTF_NONE);
	/*}}}*/
	/*{{{  occampi:fparam -- FPARAM, VALFPARAM*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:fparam", &i, 2, 0, 0, TNF_NONE);			/* subnodes: name; type */
	cops = tnode_newcompops ();
	cops->prescope = occampi_prescope_fparam;
	cops->scopein = occampi_scopein_fparam;
	cops->gettype = occampi_gettype_fparam;
	cops->namemap = occampi_namemap_fparam;
	tnd->ops = cops;
	lops = tnode_newlangops ();
	lops->getdescriptor = occampi_getdescriptor_fparam;
	tnd->lops = lops;
	i = -1;
	opi.tag_FPARAM = tnode_newnodetag ("FPARAM", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_VALFPARAM = tnode_newnodetag ("VALFPARAM", &i, tnd, NTF_NONE);
	/*}}}*/
	/*{{{  occampi:abbrevnode -- ABBREV, VALABBREV*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:abbrevnode", &i, 4, 0, 0, TNF_SHORTDECL);		/* subnodes: name; type; in-scope-body; expr */
	cops = tnode_newcompops ();
	cops->prescope = occampi_prescope_abbrev;
	cops->scopein = occampi_scopein_abbrev;
	cops->scopeout = occampi_scopeout_abbrev;
	cops->typecheck = occampi_typecheck_abbrev;
	cops->namemap = occampi_namemap_abbrev;
	tnd->ops = cops;
	i = -1;
	opi.tag_ABBREV = tnode_newnodetag ("ABBREV", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_VALABBREV = tnode_newnodetag ("VALABBREV", &i, tnd, NTF_NONE);
	/*}}}*/
	/*{{{  occampi:procdecl -- PROCDECL*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:procdecl", &i, 4, 0, 0, TNF_LONGDECL);		/* subnodes: name; fparams; body; in-scope-body */
	cops = tnode_newcompops ();
	cops->prescope = occampi_prescope_procdecl;
	cops->scopein = occampi_scopein_procdecl;
	cops->scopeout = occampi_scopeout_procdecl;
	cops->namemap = occampi_namemap_procdecl;
	cops->gettype = occampi_gettype_procdecl;
	cops->precheck = occampi_precheck_procdecl;
	cops->fetrans = occampi_fetrans_procdecl;
	cops->precode = occampi_precode_procdecl;
	cops->codegen = occampi_codegen_procdecl;
	tnd->ops = cops;
	lops = tnode_newlangops ();
	lops->getdescriptor = occampi_getdescriptor_procdecl;
	lops->do_usagecheck = occampi_usagecheck_procdecl;
	tnd->lops = lops;
	i = -1;
	opi.tag_PROCDECL = tnode_newnodetag ("PROCDECL", &i, tnd, NTF_NONE);
	/*}}}*/

	return 0;
}
/*}}}*/
/*{{{  static int occampi_decl_reg_reducers (void)*/
/*
 *	registers reductions for declaration nodes
 */
static int occampi_decl_reg_reducers (void)
{
	parser_register_grule ("opi:namereduce", parser_decode_grule ("T+St0XC1R-", occampi_nametoken_to_hook, opi.tag_NAME));
	parser_register_grule ("opi:namepush", parser_decode_grule ("T+St0XC1N-", occampi_nametoken_to_hook, opi.tag_NAME));
	parser_register_grule ("opi:2namepush", parser_decode_grule ("T+St0XC1N-T+St0XC1N-", occampi_nametoken_to_hook, opi.tag_NAME, occampi_nametoken_to_hook, opi.tag_NAME));
	parser_register_grule ("opi:fparam1nsreduce", parser_decode_grule ("N+Sn00C2R-", opi.tag_FPARAM));
	parser_register_grule ("opi:fparam2nsreduce", parser_decode_grule ("N+Sn0N+C2R-", opi.tag_FPARAM));
	parser_register_grule ("opi:fparam2tsreduce", parser_decode_grule ("T+St0XC1T+XC1C2R-", occampi_nametoken_to_hook, opi.tag_NAME, occampi_nametoken_to_hook, opi.tag_NAME, opi.tag_FPARAM));
	parser_register_grule ("opi:fparam1tsreduce", parser_decode_grule ("T+St0XC10C2R-", occampi_nametoken_to_hook, opi.tag_NAME, opi.tag_FPARAM));
	parser_register_grule ("opi:fparam1tsreducei", parser_decode_grule ("T+St0XC1C10C2R-", occampi_nametoken_to_hook, opi.tag_NAME, opi.tag_ASINPUT, opi.tag_FPARAM));
	parser_register_grule ("opi:fparam1tsreduceo", parser_decode_grule ("T+St0XC1C10C2R-", occampi_nametoken_to_hook, opi.tag_NAME, opi.tag_ASOUTPUT, opi.tag_FPARAM));
	parser_register_grule ("opi:declreduce", parser_decode_grule ("SN1N+N+0C3R-", opi.tag_VARDECL));
	parser_register_grule ("opi:xdeclreduce", parser_decode_grule ("SN1N+N+V0C3R-", opi.tag_VARDECL));
	parser_register_grule ("opi:procdeclreduce", parser_decode_grule ("SN1N+N+V00C4R-", opi.tag_PROCDECL));
	parser_register_grule ("opi:abbrreduce", parser_decode_grule ("SN1N+N+N+<0VC4R-", opi.tag_ABBREV));
	parser_register_grule ("opi:valabbrreduce", parser_decode_grule ("SN1N+N+N+<0VC4R-", opi.tag_VALABBREV));
	parser_register_grule ("opi:notypeabbrreduce", parser_decode_grule ("SN1N+N+0<0VC4R-", opi.tag_ABBREV));
	parser_register_grule ("opi:notypevalabbrreduce", parser_decode_grule ("SN1N+N+0<0VC4R-", opi.tag_VALABBREV));
	parser_register_grule ("opi:directedchaninput", parser_decode_grule ("SN0N+C1N-", opi.tag_ASINPUT));
	parser_register_grule ("opi:directedchanoutput", parser_decode_grule ("SN0N+C1N-", opi.tag_ASOUTPUT));

	return 0;
}
/*}}}*/
/*{{{  static dfattbl_t **occampi_decl_init_dfatrans (int *ntrans)*/
/*
 *	initialises DFA transition tables for declaration nodes
 */
static dfattbl_t **occampi_decl_init_dfatrans (int *ntrans)
{
	DYNARRAY (dfattbl_t *, transtbl);

	dynarray_init (transtbl);

	dynarray_add (transtbl, dfa_bnftotbl ("occampi:name ::= +Name {<opi:namereduce>}"));
	dynarray_add (transtbl, dfa_bnftotbl ("occampi:namelist ::= { occampi:name @@, 1 }"));

	dynarray_add (transtbl, dfa_transtotbl ("occampi:vardecl ::= [ 0 occampi:primtype 3 ] [ 0 +@CHAN 9 ] " \
				"[ 1 occampi:protocol 2 ] " \
				"[ 2 {<opi:chanpush>} -* 3 ] " \
				"[ 3 -@FUNCTION <occampi:fdeclstarttype> ] " \
				"[ 3 occampi:namelist 4 ] " \
				"[ 4 @@: 5 ] " \
				"[ 4 @IS 6 ] " \
				"[ 5 {<opi:declreduce>} -* ] " \
				"[ 6 occampi:operand 7 ] " \
				"[ 7 @@: 8 ] " \
				"[ 8 {<opi:abbrreduce>} -* ] " \
				"[ 9 +@TYPE 10 ] [ 9 -* 12 ] " \
				"[ 10 {<parser:rewindtokens>} -* 11 ] " \
				"[ 11 -* <occampi:chantypedecl> ] " \
				"[ 12 {<parser:eattoken>} -* 1 ]"));
	dynarray_add (transtbl, dfa_transtotbl ("occampi:vardecl:bracketstart ::= [ 0 occampi:arrayspec 1 ] [ 1 occampi:vardecl 2 ] [ 2 {Roccampi:arrayfold} -* ]"));

	dynarray_add (transtbl, dfa_transtotbl ("occampi:procdecl ::= [ 0 +@PROC 6 ] [ 1 occampi:name 2 ] [ 2 @@( 3 ] [ 3 occampi:fparamlist 4 ] [ 4 @@) 5 ] [ 5 {<opi:procdeclreduce>} -* ] " \
				"[ 6 +@TYPE 7 ] [ 6 -* 8 ] [ 7 {<parser:rewindtokens>} -* <occampi:proctypedecl> ] [ 8 {<parser:eattoken>} -* 1 ]"));

	dynarray_add (transtbl, dfa_transtotbl ("occampi:valabbrdecl ::= [ 0 +Name 1 ] [ 1 {<opi:namepush>} -* 2 ] [ 2 @IS 3 ] [ 2 +Name 6 ] [ 3 occampi:expr 4 ] [ 4 @@: 5 ] " \
				"[ 5 {<opi:notypevalabbrreduce>} -* ] [ 6 {<opi:namepush>} -* 7 ] [ 7 @IS 8 ] [ 8 occampi:expr 9 ] [ 9 {<opi:valabbrreduce>} -* ]"));
	dynarray_add (transtbl, dfa_transtotbl ("occampi:valabbrdecl +:= [ 0 occampi:primtype 1 ] [ 1 occampi:name 2 ] [ 2 @IS 3 ] [ 3 occampi:expr 4 ] [ 4 @@: 5 ] [ 5 {<opi:valabbrreduce>} -* ]"));
	dynarray_add (transtbl, dfa_transtotbl ("occampi:abbrdecl ::= [ 0 @VAL <occampi:valabbrdecl> ]"));

	dynarray_add (transtbl, dfa_transtotbl ("occampi:fparam ::= [ 0 occampi:primtype 1 ] [ 0 @CHAN 3 ] [ 0 occampi:name 9 ] " \
				"[ 1 occampi:name 2 ] [ 2 {<opi:fparam2nsreduce>} ] [ 2 -* ] " \
				"[ 3 occampi:protocol 4 ] [ 3 @@! 16 ] [ 3 @@? 19 ] [ 4 {<opi:chanpush>} ] [ 4 occampi:name 5 ] [ 5 @@! 6 ] [ 5 @@? 7 ] [ 5 -* 8 ] [ 6 {<opi:directedchanoutput>} ] [ 6 -* 8 ] " \
				"[ 7 {<opi:directedchaninput>} ] [ 7 -* 8 ] [ 8 {<opi:fparam2nsreduce>} -* ] " \
				"[ 9 -* 10 ] [ 10 @@! 11 ] [ 10 @@? 12 ] [ 10 -* 13 ] [ 11 {<opi:directedchanoutput>} ] [ 11 -* 13 ] [ 12 {<opi:directedchaninput>} ] [ 12 -* 13 ] " \
				"[ 13 occampi:name 14 ] [ 13 -* 15 ] [ 14 {<opi:fparam2nsreduce>} -* ] [ 15 {<opi:fparam1nsreduce>} -* ] " \
				"[ 16 occampi:protocol 17 ] [ 17 {<opi:chanpush>} -* 18 ] [ 18 {<opi:directedchanoutput>} occampi:name 5 ] " \
				"[ 19 occampi:protocol 20 ] [ 20 {<opi:chanpush>} -* 21 ] [ 21 {<opi:directedchaninput>} occampi:name 5 ]"));
	
	dynarray_add (transtbl, dfa_bnftotbl ("occampi:fparamlist ::= ( -@@) {<opi:nullset>} | { occampi:fparam @@, 1 } )"));


	dynarray_add (transtbl, dfa_transtotbl ("occampi:namestartname +:= [ 0 +Name 1 ] [ 1 {<opi:namepush>} ] [ 1 @@: 2 ] [ 2 {<opi:declreduce>} -* ]"));
	dynarray_add (transtbl, dfa_transtotbl ("occampi:namestartname +:= [ 0 @IS 1 ] [ 1 occampi:operand 7 ] [ 7 @@: 8 ] [ 8 {<opi:notypeabbrreduce>} -* ]"));

	*ntrans = DA_CUR (transtbl);
	return DA_PTR (transtbl);
}
/*}}}*/
/*{{{  static int occampi_decl_post_setup (void)*/
/*
 *	does post-setup for initialisation
 */
static int occampi_decl_post_setup (void)
{
	static dfaerrorhandler_t procdecl_eh = { occampi_procdecl_dfaeh_stuck };

	dfa_seterrorhandler ("occampi:procdecl", &procdecl_eh);

	return 0;
}
/*}}}*/


/*{{{  occampi_decl_feunit (feunit_t)*/
feunit_t occampi_decl_feunit = {
	init_nodes: occampi_decl_init_nodes,
	reg_reducers: occampi_decl_reg_reducers,
	init_dfatrans: occampi_decl_init_dfatrans,
	post_setup: occampi_decl_post_setup
};


/*}}}*/

