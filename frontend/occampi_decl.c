/*
 *	occampi_decl.c -- occam-pi declaration and name handling for NOCC
 *	Copyright (C) 2005-2007 Fred Barnes <frmb@kent.ac.uk>
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
#include "occampi.h"
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


/*}}}*/

/*
 *	this file contains the compiler front-end routines for occam-pi
 *	declarations, parameters and names.
 */



/*{{{  static void occampi_initvalabbrev (tnode_t *node, codegen_t *cgen, void *arg)*/
/*
 *	does initialiser code-gen for a value abbreviation (simple types)
 */
static void occampi_initvalabbrev (tnode_t *node, codegen_t *cgen, void *arg)
{
	tnode_t *abbrev = (tnode_t *)arg;
	int ws_off, vs_off, ms_off, ms_shdw;
	tnode_t *mappedrhs;

	cgen->target->be_getoffsets (node, &ws_off, &vs_off, &ms_off, &ms_shdw);
#if 0
fprintf (stderr, "occampi_initvalabbrev(): node=[%s], allocated at [%d,%d,%d]:\n", node->tag->name, ws_off, vs_off, ms_off);
tnode_dumptree (node, 1, stderr);
#endif
	mappedrhs = (tnode_t *)tnode_getchook (node, cgen->pc_chook);

#if 0
fprintf (stderr, "occampi_initvalabbrev(): node=[%s], mappedrhs =\n", node->tag->name);
tnode_dumptree (mappedrhs, 1, stderr);
#endif
	codegen_callops (cgen, loadname, mappedrhs, 0);

	codegen_callops (cgen, storelocal, ws_off);
	codegen_callops (cgen, comment, "initabbrev");
	return;
}
/*}}}*/
/*{{{  static void occampi_initptrabbrev (tnode_t *node, codegen_t *cgen, void *arg)*/
/*
 *	does initialiser code-gen for a pointer abbreviation (non-VAL and arrays, etc.)
 */
static void occampi_initptrabbrev (tnode_t *node, codegen_t *cgen, void *arg)
{
	tnode_t *abbrev = (tnode_t *)arg;
	int ws_off, vs_off, ms_off, ms_shdw;
	tnode_t *mappedrhs;

	cgen->target->be_getoffsets (node, &ws_off, &vs_off, &ms_off, &ms_shdw);
	mappedrhs = (tnode_t *)tnode_getchook (node, cgen->pc_chook);

	codegen_callops (cgen, loadpointer, mappedrhs, 0);
	codegen_callops (cgen, storelocal, ws_off);
	codegen_callops (cgen, comment, "initabbrev");
	return;
}
/*}}}*/

/*{{{  static int occampi_prescope_vardecl (compops_t *cops, tnode_t **node, prescope_t *ps)*/
/*
 *	called to prescope a variable declaration
 */
static int occampi_prescope_vardecl (compops_t *cops, tnode_t **node, prescope_t *ps)
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
/*{{{  static int occampi_scopein_vardecl (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	called to scope a variable declaration
 */
static int occampi_scopein_vardecl (compops_t *cops, tnode_t **node, scope_t *ss)
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
/*{{{  static int occampi_scopeout_vardecl (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	called to scope-out a variable declaration
 */
static int occampi_scopeout_vardecl (compops_t *cops, tnode_t **node, scope_t *ss)
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
/*{{{  static int occampi_premap_vardecl (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	does pre-mapping on a variable declaration node
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_premap_vardecl (compops_t *cops, tnode_t **node, map_t *map)
{
	tnode_t *type = tnode_nthsubof (*node, 1);

	if (type && type->tag->ndef->lops && tnode_haslangop (type->tag->ndef->lops, "premap_typeforvardecl")) {
		int i;

		i = tnode_calllangop (type->tag->ndef->lops, "premap_typeforvardecl", 3, type, *node, map);
		return i;
	}
	return 1;
}
/*}}}*/
/*{{{  static int occampi_constprop_vardecl (compops_t *cops, tnode_t **nodep)*/
/*
 *	does constant propagation on a variable declaration node
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_constprop_vardecl (compops_t *cops, tnode_t **nodep)
{
	return 1;
}
/*}}}*/
/*{{{  static int occampi_fetrans_vardecl (compops_t *cops, tnode_t **node, fetrans_t *fe)*/
/*
 *	does front-end transformations on a variable declaration, for ARRAY types, this includes attaching a dimension list
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_fetrans_vardecl (compops_t *cops, tnode_t **node, fetrans_t *fe)
{
	tnode_t *type = tnode_nthsubof (*node, 1);
	tnode_t *dtree = NULL;

	/* see if the type has a dimension tree */
	dtree = langops_dimtreeof (type);

	return 1;
}
/*}}}*/
/*{{{  static int occampi_namemap_vardecl (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	transforms the name declared into a back-end name
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_namemap_vardecl (compops_t *cops, tnode_t **node, map_t *map)
{
	tnode_t **namep = tnode_nthsubaddr (*node, 0);
	tnode_t **typep = tnode_nthsubaddr (*node, 1);
	tnode_t **bodyp = tnode_nthsubaddr (*node, 2);
	tnode_t *type = *typep;
	tnode_t *bename;
	int tsize;
	int wssize, vssize, mssize, indir;

#if 0
fprintf (stderr, "occampi_namemap_vardecl(): here!  target is [%s].  Type is:\n", map->target->name);
tnode_dumptree (type, 1, stderr);
#endif

	/* see how big this type is */
	tsize = tnode_bytesfor (type, map->target);

	if (type->tag->ndef->lops && tnode_haslangop (type->tag->ndef->lops, "initsizes") && tnode_calllangop (type->tag->ndef->lops, "initsizes", 7, type, *node, &wssize, &vssize, &mssize, &indir, map)) {
		/* some declarations will need special allocation (e.g. in vectorspace and/or mobilespace) -- collected above */
	} else {
		wssize = tsize;
		vssize = 0;
		mssize = 0;
		indir = 0;
	}
	bename = map->target->newname (*namep, *bodyp, map, (wssize < 0) ? 0 : wssize, (wssize < 0) ? -wssize : 0, vssize, mssize, tsize, indir);

	if (type->tag->nt_flags & NTF_NAMEMAPTYPEINDECL) {
		// chook_t *pcevhook = tnode_lookupchookbyname ("precode:vars");

		map_submapnames (typep, map);
		/* pull any of these upwards */
		precode_pullupprecodevars (*namep, *typep);
	}

	if (type->tag->ndef->lops && tnode_haslangop (type->tag->ndef->lops, "initialising_decl")) {
#if 0
fprintf (stderr, "occampi_namemap_vardecl(): calling initialising_decl on the type (%s)\n", type->tag->name);
#endif
		tnode_calllangop (type->tag->ndef->lops, "initialising_decl", 3, type, bename, map);
	}

	tnode_setchook (*namep, map->mapchook, (void *)bename);
#if 0
fprintf (stderr, "got new bename:\n");
tnode_dumptree (bename, 1, stderr);
#endif

	*node = bename;
	bodyp = tnode_nthsubaddr (*node, 1);

	/* map the body */
	map_submapnames (bodyp, map);
	return 0;
}
/*}}}*/
/*{{{  static int occampi_precode_vardecl (compops_t *cops, tnode_t **nodep, codegen_t *cgen)*/
/*
 *	does pre-coding for a variable declaration node
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_precode_vardecl (compops_t *cops, tnode_t **nodep, codegen_t *cgen)
{
	tnode_t *type = tnode_nthsubof (*nodep, 1);

#if 0
	nocc_message ("occampi_precode_vardecl(): here!");
#endif
	if (type && (type->tag->nt_flags & NTF_PRECODETYPEINDECL)) {
		codegen_subprecode (tnode_nthsubaddr (*nodep, 1), cgen);
	}

	return 0;
}
/*}}}*/


/*{{{  static int occampi_prescope_abbrev (compops_t *cops, tnode_t **nodep, prescope_t *ps)*/
/*
 *	called to prescope an abbreviation declaration
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_prescope_abbrev (compops_t *cops, tnode_t **nodep, prescope_t *ps)
{
	tnode_t *name = tnode_nthsubof (*nodep, 0);

	if (parser_islistnode (name)) {
		prescope_error (*nodep, ps, "occampi_prescope_abbrev(): unexpected list");
	}
	return 1;
}
/*}}}*/
/*{{{  static int occampi_scopein_abbrev (compops_t *cops, tnode_t **nodep, scope_t *ss)*/
/*
 *	called to scope an abbreviation
 *	returns 0 to stop walk, 1 to continue, -1 to stop and prevent scopeout
 */
static int occampi_scopein_abbrev (compops_t *cops, tnode_t **nodep, scope_t *ss)
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

	if (!type) {
#if 0
fprintf (stderr, "occampi_scopein_abbrev: here 2! no type, scoped RHS is:\n");
tnode_dumptree (tnode_nthsubof (*nodep, 3), 1, stderr);
#endif
	}

	sname = name_addscopename (rawname, *nodep, type, NULL);
	if (((*nodep)->tag == opi.tag_VALABBREV) || ((*nodep)->tag == opi.tag_VALRETYPES)) {
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
/*{{{  static int occampi_scopeout_abbrev (compops_t *cops, tnode_t **nodep, scope_t *ss)*/
/*
 *	called to scope-out an abbreviation
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_scopeout_abbrev (compops_t *cops, tnode_t **nodep, scope_t *ss)
{
	tnode_t *name = tnode_nthsubof (*nodep, 0);
	name_t *sname;

	if (name->tag != ((((*nodep)->tag == opi.tag_VALABBREV) || ((*nodep)->tag == opi.tag_VALRETYPES)) ? opi.tag_NVALABBR : opi.tag_NABBR)) {
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
/*{{{  static int occampi_typecheck_abbrev (compops_t *cops, tnode_t *node, typecheck_t *tc)*/
/*
 *	does type-checking for an abbreviation -- mainly to ensure that the type is present/inferred
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_typecheck_abbrev (compops_t *cops, tnode_t *node, typecheck_t *tc)
{
	tnode_t **namep = tnode_nthsubaddr (node, 0);
	tnode_t **typep = tnode_nthsubaddr (node, 1);
	tnode_t **rhsp = tnode_nthsubaddr (node, 3);
	tnode_t **xtypep = NULL;
	tnode_t *rtype = NULL;

	if ((*namep)->tag->ndef == opi.node_NAMENODE) {
		name_t *name = tnode_nthnameof (*namep, 0);
		
		xtypep = NameTypeAddr (name);
	}

	/* type-check RHS first */
	typecheck_subtree (*rhsp, tc);
	rtype = typecheck_gettype (*rhsp, NULL);

#if 0
fprintf (stderr, "occampi_typecheck_abbrev(): *xtypep=0x%8.8x, *typep=0x%8.8x, rtype=\n", (unsigned int)(*xtypep), (unsigned int)(*typep));
tnode_dumptree (rtype, 1, stderr);
#endif
	if (*typep && xtypep && !*xtypep) {
		*xtypep = *typep;		/* set NAMENODE type to type in abbreviation */

		/* typecheck RHS */
		rtype = typecheck_gettype (*rhsp, *typep);
		if (!rtype) {
			typecheck_error (node, tc, "failed to get type from RHS for abbreviation");
			return 0;
		}

		typecheck_typeactual (*typep, rtype, node, tc);
	} else if (!*typep && xtypep && *xtypep) {
		*typep = *xtypep;		/* set ABBRNODE to type in name */
		/* typecheck RHS */
		rtype = typecheck_gettype (*rhsp, *typep);
		if (!rtype) {
			typecheck_error (node, tc, "failed to get type from RHS for abbreviation");
			return 0;
		}

		typecheck_typeactual (*typep, rtype, node, tc);
	} else if (!*typep && (!xtypep || !*xtypep)) {
		/* no type, look at RHS */
		if (typecheck_subtree (*rhsp, tc)) {
			return 0;
		}
		rtype = typecheck_gettype (*rhsp, NULL);
		if (!rtype) {
			/* try once more with a default integer type */
			tnode_t *definttype = tnode_create (opi.tag_INT, NULL);

			rtype = typecheck_gettype (*rhsp, definttype);
#if 0
fprintf (stderr, "occampi_typecheck_abbrev(): no RHS type by default, tried INT got:\n");
tnode_dumptree (rtype, 1, stderr);
#endif
			if (!rtype) {
				typecheck_error (node, tc, "failed to get type from RHS for abbreviation");
				return 0;
			} else if (rtype != definttype) {
				tnode_free (definttype);
			}
		}
		*typep = tnode_copytree (rtype);
		if (xtypep && !*xtypep) {
			*xtypep = *typep;
		}

		typecheck_warning (node, tc, "untyped abbreviation, set to %s", (*typep)->tag->name);
	} else {
		/* typecheck RHS */
		rtype = typecheck_gettype (*rhsp, *typep);
		if (!rtype) {
			/* try once more with a default integer type */
			tnode_t *definttype = tnode_create (opi.tag_INT, NULL);

			rtype = typecheck_gettype (*rhsp, definttype);
#if 0
fprintf (stderr, "occampi_typecheck_abbrev(): no RHS type by default, tried INT got:\n");
tnode_dumptree (rtype, 1, stderr);
#endif
			if (rtype != definttype) {
				tnode_free (definttype);
				rtype = NULL;
				typecheck_error (node, tc, "failed to get type from RHS for abbreviation");
				return 0;
			}
		}
		if (rtype) {
			tnode_t *realtype;
#if 0
fprintf (stderr, "occampi_typecheck_abbrev(): both sides have types, *typep=0x%8.8x, rtype=0x%8.8x =\n", (unsigned int)(*typep), (unsigned int)(rtype));
tnode_dumptree (rtype, 1, stderr);
fprintf (stderr, "occampi_typecheck_abbrev(): *typep=0x%8.8x =\n", (unsigned int)(*typep));
tnode_dumptree (*typep, 1, stderr);
#endif
			if ((node->tag == opi.tag_RETYPES) || (node->tag == opi.tag_VALRETYPES)) {
				int rhsbytes = tnode_bytesfor (rtype, NULL);
				int lhsbytes = tnode_bytesfor (*typep, NULL);

#if 0
fprintf (stderr, "occampi_typecheck_abbrev(): RETYPES: rhsbytes = %d, lhsbytes = %d\n", rhsbytes, lhsbytes);
#endif
				if (rhsbytes == -1) {
					/* FIXME: unknown RHS size */
					typecheck_error (node, tc, "right-hand side of retypes has unknown size");
				} else if (lhsbytes == -1) {
					/* FIXME: unknown LHS size */
					typecheck_error (node, tc, "left-hand side of retypes has unknown size");
				} else if (lhsbytes != rhsbytes) {
					typecheck_error (node, tc, "incompatible types for RETYPES");
				}
				realtype = *typep;
			} else {
				realtype = typecheck_typeactual (*typep, rtype, node, tc);
			}
#if 0
fprintf (stderr, "occampi_typecheck_abbrev(): realtype (0x%8.8x) =\n", (unsigned int)realtype);
tnode_dumptree (realtype, 1, stderr);
#endif
			if (realtype) {
				*typep = realtype;		/* put this back */
			}
			if (xtypep) {
				*xtypep = realtype;		/* and in the name */
			}
		}
	}
	/* typecheck body */
	typecheck_subtree (tnode_nthsubof (node, 2), tc);

	return 0;
}
/*}}}*/
/*{{{  static int occampi_constprop_abbrev (compops_t *cops, tnode_t **nodep)*/
/*
 *	does constant propagation on an abbreviation node
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_constprop_abbrev (compops_t *cops, tnode_t **nodep)
{
#if 0
fprintf (stderr, "occampi_constprop_abbrev(): *nodep =\n");
tnode_dumptree (*nodep, 1, stderr);
#endif
	constprop_tree (tnode_nthsubaddr (*nodep, 2));		/* re-do constant propagation on body */
	return 1;
}
/*}}}*/
/*{{{  static int occampi_namemap_abbrev (compops_t *cops, tnode_t **nodep, map_t *map)*/
/*
 *	transforms an abbreviation into a back-end name
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_namemap_abbrev (compops_t *cops, tnode_t **nodep, map_t *map)
{
	tnode_t **namep = tnode_nthsubaddr (*nodep, 0);
	tnode_t *type = tnode_nthsubof (*nodep, 1);
	tnode_t **bodyp = tnode_nthsubaddr (*nodep, 2);
	tnode_t **rhsp = tnode_nthsubaddr (*nodep, 3);
	tnode_t *bename;
	void (*initfunc)(tnode_t *, codegen_t *, void *) = NULL;
	void *initarg = NULL;
	int vsize = -1;

#if 0
fprintf (stderr, "occampi_namemap_abbrev(): here!  target is [%s].  Type is:\n", map->target->name);
tnode_dumptree (type, 1, stderr);
#endif
	if (map->lexlevel <= 0) {
		/* top-level abbreviation -- must be a constant */
		if (!constprop_isconst (*rhsp)) {
			tnode_error (*nodep, "top-level abbreviation must be constant");
		}
		map_submapnames (bodyp, map);
		return 0;
	}

	if (((*nodep)->tag == opi.tag_ABBREV) || ((*nodep)->tag == opi.tag_RETYPES) || langops_valbyref (*rhsp)) {
		initfunc = occampi_initptrabbrev;
		vsize = map->target->pointersize;
	} else {
		initfunc = occampi_initvalabbrev;
		vsize = tnode_bytesfor (*rhsp, map->target);
	}
	initarg = *nodep;	/* handle on the original abbreviation */

	bename = map->target->newname (*namep, *bodyp, map, map->target->pointersize, 0, 0, 0, vsize, 1);

	/*{{{  set initialiser up*/
	codegen_setinithook (bename, initfunc, initarg);

	/*}}}*/

	/* map the RHS and attach so it gets mapped */
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


/*{{{  static int occampi_prescope_procdecl (compops_t *cops, tnode_t **node, prescope_t *ps)*/
/*
 *	called to prescope a PROC definition
 */
static int occampi_prescope_procdecl (compops_t *cops, tnode_t **node, prescope_t *ps)
{
	occampi_prescope_t *ops = (occampi_prescope_t *)(ps->hook);
	char *rawname = (char *)tnode_nthhookof (tnode_nthsubof (*node, 0), 0);

#if 0
nocc_message ("occampi_prescope_procdecl(): rawname = \"%s\", ops->procdepth = %d", rawname, ops->procdepth);
#endif
	if (!ops->procdepth) {
		int x;

		x = library_makepublic (node, rawname);
		if (x) {
			return 1;			/* go round again */
		}
	} else {
		library_makeprivate (node, rawname);
		/* continue processing */
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

	/* prescope params */
	prescope_subtree (tnode_nthsubaddr (*node, 1), ps);

	/* do a prescope on the body, at a higher procdepth */
	ops->procdepth++;
	prescope_subtree (tnode_nthsubaddr (*node, 2), ps);
	ops->procdepth--;

	/* prescope in-scope process */
	prescope_subtree (tnode_nthsubaddr (*node, 3), ps);

	return 0;				/* done them all */
}
/*}}}*/
/*{{{  static int occampi_scopein_procdecl (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	called to scope-in a PROC definition
 */
static int occampi_scopein_procdecl (compops_t *cops, tnode_t **node, scope_t *ss)
{
	tnode_t *name = tnode_nthsubof (*node, 0);
	tnode_t **paramsptr = tnode_nthsubaddr (*node, 1);
	tnode_t **bodyptr = tnode_nthsubaddr (*node, 2);
	tnode_t *traces = NULL;
	void *nsmark;
	char *rawname;
	name_t *procname;
	tnode_t *newname;

	nsmark = name_markscope ();

	/* walk parameters and body */
	tnode_modprepostwalktree (paramsptr, scope_modprewalktree, scope_modpostwalktree, (void *)ss);
	tnode_modprepostwalktree (bodyptr, scope_modprewalktree, scope_modpostwalktree, (void *)ss);

	/*{{{  if we have any attached TRACES, walk these too*/
	traces = (tnode_t *)tnode_getchook (*node, opi.chook_traces);
	if (traces) {
#if 0
fprintf (stderr, "occampi_scopein_procdecl(): have traces!\n");
#endif
		/* won't affect TRACES node, so safe to pass local addr */
		tnode_modprepostwalktree (&traces, scope_modprewalktree, scope_modpostwalktree, (void *)ss);
	}
	/*}}}*/

	name_markdescope (nsmark);

	/* declare and scope PROC name, then check process in the scope of it */
	rawname = tnode_nthhookof (name, 0);
	procname = name_addscopenamess (rawname, *node, *paramsptr, NULL, ss);
	newname = tnode_createfrom (opi.tag_NPROCDEF, name, procname);
	SetNameNode (procname, newname);
	tnode_setnthsub (*node, 0, newname);

	/* free old name, scope process */
	tnode_free (name);
	tnode_modprepostwalktree (tnode_nthsubaddr (*node, 3), scope_modprewalktree, scope_modpostwalktree, (void *)ss);
	ss->scoped++;

	return 0;		/* already walked child nodes */
}
/*}}}*/
/*{{{  static int occampi_scopeout_procdecl (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	called to scope-out a PROC definition
 */
static int occampi_scopeout_procdecl (compops_t *cops, tnode_t **node, scope_t *ss)
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
/*{{{  static tnode_t *occampi_gettype_procdecl (langops_t *lops, tnode_t *node, tnode_t *defaulttype)*/
/*
 *	returns the type of a PROC definition (= parameter list)
 */
static tnode_t *occampi_gettype_procdecl (langops_t *lops, tnode_t *node, tnode_t *defaulttype)
{
	return tnode_nthsubof (node, 1);
}
/*}}}*/
/*{{{  static int occampi_fetrans_procdecl (compops_t *cops, tnode_t **node, fetrans_t *fe)*/
/*
 *	does front-end transforms on a PROC definition
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_fetrans_procdecl (compops_t *cops, tnode_t **node, fetrans_t *fe)
{
	chook_t *deschook = tnode_lookupchookbyname ("fetrans:descriptor");
	char *dstr = NULL;
	tnode_t *params, **plist;
	int i, nparams;

	if (!deschook) {
		return 1;
	}
	langops_getdescriptor (*node, &dstr);
	if (dstr) {
		tnode_setchook (*node, deschook, (void *)dstr);
	}

	/* do fetrans on the name and parameters */
	fetrans_subtree (tnode_nthsubaddr (*node, 0), fe);
	fetrans_subtree (tnode_nthsubaddr (*node, 1), fe);

	params = tnode_nthsubof (*node, 1);
	/* run through parameters and generate/insert hidden parameters */
	plist = parser_getlistitems (params, &nparams);

	for (i=0; i<nparams; i++) {
		tnode_t *param = plist[i];
		tnode_t *hplist = langops_hiddenparamsof (param);

		if (hplist) {
			int j, nhparams;
			tnode_t **hparams = parser_getlistitems (hplist, &nhparams);

			for (j=0; j<nhparams; j++) {
				i++;
				parser_insertinlist (params, hparams[j], i);
				hparams[j] = NULL;
			}
			plist = parser_getlistitems (params, &nparams);
		}
	}

	/* do fetrans on the PROC body and in-scope process */
	fetrans_subtree (tnode_nthsubaddr (*node, 2), fe);
	fetrans_subtree (tnode_nthsubaddr (*node, 3), fe);

	return 0;
}
/*}}}*/
/*{{{  static int occampi_precheck_procdecl (compops_t *cops, tnode_t *node)*/
/*
 *	does pre-checking on PROC declaration
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_precheck_procdecl (compops_t *cops, tnode_t *node)
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
/*{{{  static int occampi_usagecheck_procdecl (langops_t *lops, tnode_t *node, uchk_state_t *ucstate)*/
/*
 *	does usage-checking on a PROC declaration
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_usagecheck_procdecl (langops_t *lops, tnode_t *node, uchk_state_t *ucstate)
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
/*{{{  static int occampi_miscnodetrans_procdecl (compops_t *cops, tnode_t **tptr, occampi_miscnodetrans_t *mnt)*/
/*
 *	does miscnode transformations for a PROC declaration -- hoists metadata to the PROC
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_miscnodetrans_procdecl (compops_t *cops, tnode_t **tptr, occampi_miscnodetrans_t *mnt)
{
	chook_t *metahook = tnode_lookupchookbyname ("misc:metadata");
	chook_t *metalisthook = tnode_lookupchookbyname ("misc:metadatalist");
	opi_metadatalist_t *(*mdlfcn)(void) = (opi_metadatalist_t *(*)(void))fcnlib_findfunction2 ("new_miscmetadatalist", 1, 0);

	if (mnt->md_node) {
		opi_metadatalist_t *mdl = mdlfcn ();

		while (mnt->md_node) {
			tnode_t **nextp = tnode_nthsubaddr (mnt->md_node, 0);
			tnode_t *tmp;

			if (tnode_haschook (mnt->md_node, metahook)) {
				opi_metadata_t *mdata = (opi_metadata_t *)tnode_getchook (mnt->md_node, metahook);

				tnode_clearchook (mnt->md_node, metahook);
				if (mdata) {
					dynarray_add (mdl->items, mdata);
				}
			}

			tmp = *nextp;
			*nextp = NULL;
			tnode_free (mnt->md_node);
			mnt->md_node = tmp;
		}

		mnt->md_iptr = NULL;

		tnode_setchook (*tptr, metalisthook, (void *)mdl);
	}
	return 1;
}
/*}}}*/
/*{{{  static int occampi_betrans_procdecl (compops_t *cops, tnode_t **node, betrans_t *bt)*/
/*
 *	does back-end mapping for a PROC definition -- pulls out nested PROCs
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_betrans_procdecl (compops_t *cops, tnode_t **node, betrans_t *be)
{
	occampi_betrans_t *opibe = (occampi_betrans_t *)be->priv;

	if (!opibe) {
		/* this is a top-level PROC of some kind, put an insertpoint here */
		opibe = (occampi_betrans_t *)smalloc (sizeof (occampi_betrans_t));
		opibe->procdepth = 0;
		opibe->insertpoint = node;
		be->priv = opibe;

		betrans_subtree (tnode_nthsubaddr (*node, 0), be);
		betrans_subtree (tnode_nthsubaddr (*node, 1), be);
		opibe->procdepth = 1;
		betrans_subtree (tnode_nthsubaddr (*node, 2), be);
		opibe->procdepth = 0;

		sfree (opibe);
		be->priv = NULL;

		/* do in-scope body */
		betrans_subtree (tnode_nthsubaddr (*node, 3), be);
	} else {
		/* this is a nested PROC -- move it up to the insertpoint */
		tnode_t *thisproc = *node;
		tnode_t *ibody = tnode_nthsubof (*node, 3);
		tnode_t *ipproc = *(opibe->insertpoint);

		betrans_subtree (tnode_nthsubaddr (*node, 0), be);
		betrans_subtree (tnode_nthsubaddr (*node, 1), be);
		opibe->procdepth++;
		betrans_subtree (tnode_nthsubaddr (*node, 2), be);
		opibe->procdepth--;
		betrans_subtree (tnode_nthsubaddr (*node, 3), be);

		*(opibe->insertpoint) = thisproc;
		tnode_setnthsub (thisproc, 3, ipproc);
		*node = ibody;

	}

	return 0;
}
/*}}}*/
/*{{{  static int occampi_namemap_procdecl (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	name-maps a PROC definition
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_namemap_procdecl (compops_t *cops, tnode_t **node, map_t *map)
{
	tnode_t *blk;
	/* tnode_t *saved_blk = map->thisblock;
	tnode_t **saved_params = map->thisprocparams; */
	tnode_t **paramsptr;
	tnode_t *tmpname;

	blk = map->target->newblock (tnode_nthsubof (*node, 2), map, tnode_nthsubof (*node, 1), map->lexlevel + 1);
	map_pushlexlevel (map, blk, tnode_nthsubaddr (*node, 1));
	/* map->thisblock = blk;
	 * map->thisprocparams = tnode_nthsubaddr (*node, 1);
	 * map->lexlevel++; */

	/* map formal params and body */
	paramsptr = tnode_nthsubaddr (*node, 1);
	map->inparamlist = 1;
#if 0
fprintf (stderr, "occampi_namemap_procdecl(): about to map parameters:\n");
tnode_dumptree (*paramsptr, 1, stderr);
#endif
	map_submapnames (paramsptr, map);
	map->inparamlist = 0;
	map_submapnames (tnode_nthsubaddr (blk, 0), map);		/* do this under the back-end block */

	map_poplexlevel (map);
	/* map->lexlevel--;
	 * map->thisblock = saved_blk;
	 * map->thisprocparams = saved_params; */

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
/*{{{  static int occampi_precode_procdecl (compops_t *cops, tnode_t **nodep, codegen_t *cgen)*/
/*
 *	pre-code for PROC definition, used to determine the last PROC in a file
 *	(entry-point if main module).
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_precode_procdecl (compops_t *cops, tnode_t **nodep, codegen_t *cgen)
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
/*{{{  static int occampi_codegen_procdecl (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	generates code for a PROC definition
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_codegen_procdecl (compops_t *cops, tnode_t *node, codegen_t *cgen)
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
	codegen_callops (cgen, setnamelabel, pname);
	codegen_callops (cgen, procnameentry, pname);
	codegen_callops (cgen, debugline, node);

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
/*{{{  static int occampi_getdescriptor_procdecl (langops_t *lops, tnode_t *node, char **str)*/
/*
 *	generates a descriptor line for a PROC definition
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_getdescriptor_procdecl (langops_t *lops, tnode_t *node, char **str)
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


/*{{{  static int occampi_prescope_fparam (compops_t *cops, tnode_t **node, prescope_t *ps)*/
/*
 *	called to prescope a formal parameter
 */
static int occampi_prescope_fparam (compops_t *cops, tnode_t **node, prescope_t *ps)
{
	occampi_prescope_t *ops = (occampi_prescope_t *)(ps->hook);

#if 0
fprintf (stderr, "occampi_prescope_fparam(): prescoping formal parameter! *node = \n");
tnode_dumptree (*node, 1, stderr);
#endif

	if (tnode_nthsubof (*node, 1)) {
		tnode_t **namep = tnode_nthsubaddr (*node, 0);

		if (ops->last_type) {
			/* this is always a copy.. */
			tnode_free (ops->last_type);
			ops->last_type = NULL;
		}
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

		ops->last_type = tnode_copytree (ops->last_type);

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
/*{{{  static int occampi_scopein_fparam (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	called to scope a formal parmeter
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_scopein_fparam (compops_t *cops, tnode_t **node, scope_t *ss)
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
	newname = tnode_createfrom (((*node)->tag == opi.tag_VALFPARAM) ? opi.tag_NVALPARAM : opi.tag_NPARAM, name, sname);
	SetNameNode (sname, newname);
	tnode_setnthsub (*node, 0, newname);

	/* free the old name */
	tnode_free (name);
	ss->scoped++;

	return 1;
}
/*}}}*/
/*{{{  static int occampi_fetrans_fparam (compops_t *cops, tnode_t **node, fetrans_t *fe)*/
/*
 *	does front-end transforms on a formal parameter
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_fetrans_fparam (compops_t *cops, tnode_t **node, fetrans_t *fe)
{
	return 1;
}
/*}}}*/
/*{{{  static int occampi_namemap_fparam (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	transforms a formal parameter into a back-end name
 *	returns 0 to stop walk, 1 to continue;
 */
static int occampi_namemap_fparam (compops_t *cops, tnode_t **node, map_t *map)
{
	tnode_t *t = *node;
	tnode_t **namep = tnode_nthsubaddr (t, 0);
	tnode_t *type = tnode_nthsubof (t, 1);
	tnode_t *bename;
	int tsize, indir;

#if 0
fprintf (stderr, "occampi_namemap_fparam(): here!  target is [%s].  Type is:\n", map->target->name);
tnode_dumptree (type, 1, stderr);
#endif

	if ((t->tag == opi.tag_FPARAM) || (t->tag == opi.tag_VALFPARAM)) {
		if (type->tag == opi.tag_CHAN) {
			/* channels need 1 word */
			tsize = map->target->chansize;
		} else {
			/* see how big this type is */
			tsize = tnode_bytesfor (type, map->target);
		}

		if ((*node)->tag == opi.tag_VALFPARAM) {
			tnode_t *ftype = tnode_nthsubof (*node, 1);

			indir = langops_valbyref (ftype) ? 1 : 0;
		} else {
			indir = 1;
		}
	} else {
		nocc_internal ("occampi_namemap_fparam(): not FPARAM/VALFPARAM");
		return 0;
	}

#if 0
fprintf (stderr, "occampi_namemap_fparam(): node is [%s], type is [%s], tsize = %d, indir = %d\n", t->tag->name, type->tag->name, tsize, indir);
#endif
	bename = map->target->newname (*namep, NULL, map, 4, 0, 0, 0, tsize, indir);		/* FIXME! */
	tnode_setchook (*namep, map->mapchook, (void *)bename);

	*node = bename;
	return 0;
}
/*}}}*/
/*{{{  static tnode_t *occampi_gettype_fparam (langops_t *lops, tnode_t *node, tnode_t *defaulttype)*/
/*
 *	returns the type of a formal parameter
 */
static tnode_t *occampi_gettype_fparam (langops_t *lops, tnode_t *node, tnode_t *defaulttype)
{
	return tnode_nthsubof (node, 1);
}
/*}}}*/
/*{{{  static int occampi_getdescriptor_fparam (langops_t *lops, tnode_t *node, char **str)*/
/*
 *	gets descriptor information for a formal parameter
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_getdescriptor_fparam (langops_t *lops, tnode_t *node, char **str)
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
		char *newstr = (char *)smalloc (strlen (*str) + strlen (typestr) + strlen (pname) + 8);

		sprintf (newstr, "%s%s%s %s", *str, (node->tag == opi.tag_VALFPARAM) ? "VAL " : "", typestr, pname);
		sfree (*str);
		*str = newstr;
	} else {
		*str = (char *)smalloc (strlen (typestr) + strlen (pname) + 8);
		sprintf (*str, "%s%s %s", (node->tag == opi.tag_VALFPARAM) ? "VAL " : "", typestr, pname);
	}

	sfree (typestr);

	return 0;
}
/*}}}*/
/*{{{  static int occampi_getname_fparam (langops_t *lops, tnode_t *node, char **str)*/
/*
 *	gets the name of a formal parameter
 *	returns 0 on success, -ve on failure
 */
static int occampi_getname_fparam (langops_t *lops, tnode_t *node, char **str)
{
	tnode_t *name = tnode_nthsubof (node, 0);
	char *pname = NameNameOf (tnode_nthnameof (name, 0));

	if (*str) {
		sfree (*str);
	}
	*str = (char *)smalloc (strlen (pname) + 2);
	strcpy (*str, pname);

	return 0;
}
/*}}}*/
/*{{{  static tnode_t *occampi_hiddenparamsof_fparam (langops_t *lops, tnode_t *node)*/
/*
 *	gets the hidden-parameters associated with a normal formal parameter -- e.g. open-arrays
 */
static tnode_t *occampi_hiddenparamsof_fparam (langops_t *lops, tnode_t *node)
{
	tnode_t *name = tnode_nthsubof (node, 0);
	tnode_t *type = tnode_nthsubof (node, 1);
	tnode_t *hplist;

	hplist = langops_hiddenparamsof (type);
	if (hplist) {
		int i, nhitems;
		tnode_t **hitems = parser_getlistitems (hplist, &nhitems);

		for (i=0; i<nhitems; i++) {
			tnode_t *hparam = hitems[i];

			if (hparam->tag == opi.tag_HIDDENDIMEN) {
				if (tnode_nthsubof (hparam, 0)->tag == opi.tag_DIMSIZE) {
					/* dimension size of us -- put in reference */
					tnode_setnthsub (tnode_nthsubof (hparam, 0), 0, name);
				}
			}
		}
	}
#if 0
fprintf (stderr, "occampi_hiddenparamsof_fparam(): here! hplist =\n");
tnode_dumptree (hplist, 1, stderr);
#endif

	return hplist;
}
/*}}}*/


/*{{{  static int occampi_namemap_hiddennode (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	transforms a hidden formal parameter into a back-end name
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_namemap_hiddennode (compops_t *cops, tnode_t **node, map_t *map)
{
	tnode_t *bename;

	/*
	 * if we're in a parameter list, create a real back-end name for this node, otherwise
	 * generate a back-end name-reference for it
	 */
	if (map->inparamlist) {
		bename = map->target->newname (*node, NULL, map, map->target->intsize, 0, 0, 0, map->target->intsize, 0);
		tnode_setchook (*node, map->mapchook, (void *)bename);
	} else {
		tnode_t *rname = (tnode_t *)tnode_getchook (*node, map->mapchook);

		if (!rname) {
			nocc_internal ("occampi_namemap_hiddennode(): not in parameters, and no mapchook linkage");
			return 0;
		}

		bename = map->target->newnameref (rname, map);
	}

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

/*{{{  static int occampi_scopein_rawname (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	scopes in a free-floating name
 */
static int occampi_scopein_rawname (compops_t *cops, tnode_t **node, scope_t *ss)
{
	tnode_t *name = *node;
	char *rawname;
	name_t *sname = NULL;
	occampi_typeattr_t nameattrs = (occampi_typeattr_t)tnode_getchook (name, opi.chook_typeattr);

	if (name->tag != opi.tag_NAME) {
		scope_error (name, ss, "name not raw-name!");
		return 0;
	}
	rawname = tnode_nthhookof (name, 0);

#if 0
fprintf (stderr, "occampi_scopein_rawname: here! rawname = \"%s\"\n", rawname);
#endif
	sname = name_lookupss (rawname, ss);
	if (sname) {
		/* resolved */

		if (nameattrs) {
			tnode_setchook (name, opi.chook_typeattr, NULL);
			*node = tnode_createfrom (opi.tag_TYPESPEC, *node, NameNodeOf (sname));
			tnode_setchook (*node, opi.chook_typeattr, (void *)nameattrs);
		} else {
			*node = NameNodeOf (sname);
		}
		tnode_free (name);
	} else {
		scope_error (name, ss, "unresolved name \"%s\"", rawname);
	}

	return 1;
}
/*}}}*/


/*{{{  static tnode_t *occampi_gettype_namenode (langops_t *lops, tnode_t *node, tnode_t *default_type)*/
/*
 *	returns the type of a name-node (trivial)
 */
static tnode_t *occampi_gettype_namenode (langops_t *lops, tnode_t *node, tnode_t *default_type)
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
#if 0
nocc_message ("occampi_gettype_namenode(): null type on name, node was:");
tnode_dumptree (node, 4, stderr);
#endif
	nocc_fatal ("occampi_gettype_namenode(): name has NULL type (FIXME!)");
	return NULL;
}
/*}}}*/
/*{{{  static int occampi_bytesfor_namenode (langops_t *lops, tnode_t *node, target_t *target)*/
/*
 *	returns the number of bytes in a name-node, associated with its type only
 */
static int occampi_bytesfor_namenode (langops_t *lops, tnode_t *node, target_t *target)
{
	if (node->tag == opi.tag_NREPL) {
		name_t *name = tnode_nthnameof (node, 0);
		tnode_t *type = NameTypeOf (name);

		/* always integer (at the moment) */
		if (target) {
			return target->intsize;
		}
		return tnode_bytesfor (type, target);
	}

	nocc_error ("occampi_bytesfor_namenode(): no bytes for [%s]", node->tag->name);
	return -1;
}
/*}}}*/
/*{{{  static int occampi_constprop_namenode (compops_t *cops, tnode_t **nodep)*/
/*
 *	does constant propagation on a name-node (reduces name to constant if applicable)
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_constprop_namenode (compops_t *cops, tnode_t **nodep)
{
	if (langops_isconst (*nodep)) {
		/* yes, we're constant, substitute */
		name_t *name = tnode_nthnameof (*nodep, 0);
		tnode_t *type = NameTypeOf (name);
		int val = langops_constvalof (*nodep, NULL);

		if (type->tag == opi.tag_BYTE) {
			*nodep = constprop_newconst (CONST_BYTE, *nodep, type, val);
		} else if (type->tag == opi.tag_BOOL) {
			*nodep = constprop_newconst (CONST_BOOL, *nodep, type, val);
		} else if (type->tag == opi.tag_REAL64) {
			double dval;

			langops_constvalof (*nodep, &dval);
			*nodep = constprop_newconst (CONST_DOUBLE, *nodep, type, val);
		} else {
			*nodep = constprop_newconst (CONST_INT, *nodep, type, val);
		}
	}
	return 1;
}
/*}}}*/
/*{{{  static int occampi_namemap_namenode (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	transforms given name into a back-end name
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_namemap_namenode (compops_t *cops, tnode_t **node, map_t *map)
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
/*{{{  static int occampi_usagecheck_namenode (langops_t *lops, tnode_t *node, uchk_state_t *uc)*/
/*
 *	does usage-check on a namenode
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_usagecheck_namenode (langops_t *lops, tnode_t *node, uchk_state_t *uc)
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
/*{{{  static int occampi_getname_namenode (langops_t *lops, tnode_t *node, char **str)*/
/*
 *	gets the name of a namenode (var/etc. name)
 *	return 0 on success, -ve on failure
 */
static int occampi_getname_namenode (langops_t *lops, tnode_t *node, char **str)
{
	char *pname = NameNameOf (tnode_nthnameof (node, 0));

	if (*str) {
		sfree (*str);
	}
	*str = (char *)smalloc (strlen (pname) + 2);
	strcpy (*str, pname);

	return 0;
}
/*}}}*/
/*{{{  static int occampi_isvar_namenode (langops_t *lops, tnode_t *node)*/
/*
 *	returns non-zero if the specified name is a variable (l-value)
 */
static int occampi_isvar_namenode (langops_t *lops, tnode_t *node)
{
	if ((node->tag == opi.tag_NDECL) || (node->tag == opi.tag_NPARAM) || (node->tag == opi.tag_NABBR)) {
		return 1;
	}
	return 0;
}
/*}}}*/
/*{{{  static int occampi_isconst_namenode (langops_t *lops, tnode_t *node)*/
/*
 *	returns non-zero if the given name is (compile-time) constant
 */
static int occampi_isconst_namenode (langops_t *lops, tnode_t *node)
{
	if (node->tag == opi.tag_NVALABBR) {
		name_t *name = tnode_nthnameof (node, 0);
		tnode_t *valdecl = NameDeclOf (name);

		if ((valdecl->tag == opi.tag_VALABBREV) || (valdecl->tag == opi.tag_VALRETYPES)) {
			return langops_isconst (tnode_nthsubof (valdecl, 3));
		}
	}
	return 0;
}
/*}}}*/
/*{{{  static int occampi_constvalof_namenode (langops_t *lops, tnode_t *node, void *ptr)*/
/*
 *	returns the constant value of a name
 */
static int occampi_constvalof_namenode (langops_t *lops, tnode_t *node, void *ptr)
{
	if (node->tag == opi.tag_NVALABBR) {
		name_t *name = tnode_nthnameof (node, 0);
		tnode_t *valdecl = NameDeclOf (name);

		if ((valdecl->tag == opi.tag_VALABBREV) || (valdecl->tag == opi.tag_VALRETYPES)) {
			tnode_t *expr = tnode_nthsubof (valdecl, 3);

			return langops_constvalof (expr, ptr);
		}
	}
	tnode_error (node, "cannot get constant value of this name [%s]", node->tag->name);

	return 0;
}
/*}}}*/
/*{{{  static tnode_t *occampi_dimtreeof_namenode (langops_t *lops, tnode_t *node)*/
/*
 *	returns the dimension-tree associated with a namenode's type (only makes sense for arrays)
 *	returns NULL if not relevant
 */
static tnode_t *occampi_dimtreeof_namenode (langops_t *lops, tnode_t *node)
{
	tnode_t *type = NameTypeOf (tnode_nthnameof (node, 0));

	if (type) {
		return langops_dimtreeof (type);
	}
	return NULL;
}
/*}}}*/


/*{{{  static void *occampi_arraydiminfo_chook_copy (void *chook)*/
/*
 *	copies an arraydiminfo compiler hook
 */
static void *occampi_arraydiminfo_chook_copy (void *chook)
{
	if (chook) {
		return (void *)tnode_copytree ((tnode_t *)chook);
	}
	return NULL;
}
/*}}}*/
/*{{{  static void occampi_arraydiminfo_chook_free (void *chook)*/
/*
 *	frees an arraydiminfo compiler hook
 */
static void occampi_arraydiminfo_chook_free (void *chook)
{
	if (chook) {
		tnode_free ((tnode_t *)chook);
	}
	return;
}
/*}}}*/
/*{{{  static void occampi_arraydiminfo_chook_dumptree (tnode_t *node, void *chook, int indent, FILE *stream)*/
/*
 *	dumps an arraydiminfo compiler hook (debugging)
 */
static void occampi_arraydiminfo_chook_dumptree (tnode_t *node, void *chook, int indent, FILE *stream)
{
	occampi_isetindent (stream, indent);
	fprintf (stream, "<chook:arraydiminfo>\n");
	tnode_dumptree ((tnode_t *)chook, indent + 1, stream);
	occampi_isetindent (stream, indent);
	fprintf (stream, "</chook:arraydiminfo>\n");
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
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (occampi_scopein_rawname));
	tnd->ops = cops;

	i = -1;
	opi.tag_NAME = tnode_newnodetag ("NAME", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  occampi:namenode -- N_DECL, N_PARAM, N_VALPARAM, N_PROCDEF, N_ABBR, N_VALABBR, N_FUNCDEF, N_REPL*/
	i = -1;
	tnd = opi.node_NAMENODE = tnode_newnodetype ("occampi:namenode", &i, 0, 1, 0, TNF_NONE);	/* subnames: name */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "constprop", 1, COMPOPTYPE (occampi_constprop_namenode));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (occampi_namemap_namenode));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "gettype", 2, LANGOPTYPE (occampi_gettype_namenode));
	tnode_setlangop (lops, "bytesfor", 2, LANGOPTYPE (occampi_bytesfor_namenode));
	tnode_setlangop (lops, "do_usagecheck", 2, LANGOPTYPE (occampi_usagecheck_namenode));
	tnode_setlangop (lops, "getname", 2, LANGOPTYPE (occampi_getname_namenode));
	tnode_setlangop (lops, "isvar", 1, LANGOPTYPE (occampi_isvar_namenode));
	tnode_setlangop (lops, "isconst", 1, LANGOPTYPE (occampi_isconst_namenode));
	tnode_setlangop (lops, "constvalof", 2, LANGOPTYPE (occampi_constvalof_namenode));
	tnode_setlangop (lops, "dimtreeof", 1, LANGOPTYPE (occampi_dimtreeof_namenode));
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
	opi.tag_NABBR = tnode_newnodetag ("N_ABBR", &i, opi.node_NAMENODE, NTF_NONE);
	i = -1;
	opi.tag_NVALABBR = tnode_newnodetag ("N_VALABBR", &i, opi.node_NAMENODE, NTF_NONE);
	i = -1;
	opi.tag_NFUNCDEF = tnode_newnodetag ("N_FUNCDEF", &i, opi.node_NAMENODE, NTF_NONE);
	i = -1;
	opi.tag_NREPL = tnode_newnodetag ("N_REPL", &i, opi.node_NAMENODE, NTF_NONE);

	/*}}}*/
	/*{{{  occampi:hiddennode -- HIDDENPARAM, HIDDENDIMEN*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:hiddennode", &i, 1, 0, 0, TNF_NONE);			/* subnodes: hidden-param */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (occampi_namemap_hiddennode));
	tnd->ops = cops;

	i = -1;
	opi.tag_HIDDENPARAM = tnode_newnodetag ("HIDDENPARAM", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_HIDDENDIMEN = tnode_newnodetag ("HIDDENDIMEN", &i, tnd, NTF_NONE);

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
	tnode_setcompop (cops, "prescope", 2, COMPOPTYPE (occampi_prescope_vardecl));
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (occampi_scopein_vardecl));
	tnode_setcompop (cops, "scopeout", 2, COMPOPTYPE (occampi_scopeout_vardecl));
	tnode_setcompop (cops, "constprop", 1, COMPOPTYPE (occampi_constprop_vardecl));
	tnode_setcompop (cops, "fetrans", 2, COMPOPTYPE (occampi_fetrans_vardecl));
	tnode_setcompop (cops, "premap", 2, COMPOPTYPE (occampi_premap_vardecl));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (occampi_namemap_vardecl));
	tnode_setcompop (cops, "precode", 2, COMPOPTYPE (occampi_precode_vardecl));
	tnd->ops = cops;

	i = -1;
	opi.tag_VARDECL = tnode_newnodetag ("VARDECL", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  occampi:fparam -- FPARAM, VALFPARAM*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:fparam", &i, 2, 0, 0, TNF_NONE);			/* subnodes: name; type */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "prescope", 2, COMPOPTYPE (occampi_prescope_fparam));
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (occampi_scopein_fparam));
	tnode_setcompop (cops, "fetrans", 2, COMPOPTYPE (occampi_fetrans_fparam));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (occampi_namemap_fparam));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "getdescriptor", 2, LANGOPTYPE (occampi_getdescriptor_fparam));
	tnode_setlangop (lops, "gettype", 2, LANGOPTYPE (occampi_gettype_fparam));
	tnode_setlangop (lops, "getname", 2, LANGOPTYPE (occampi_getname_fparam));
	tnode_setlangop (lops, "hiddenparamsof", 1, LANGOPTYPE (occampi_hiddenparamsof_fparam));
	tnd->lops = lops;

	i = -1;
	opi.tag_FPARAM = tnode_newnodetag ("FPARAM", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_VALFPARAM = tnode_newnodetag ("VALFPARAM", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  occampi:abbrevnode -- ABBREV, VALABBREV, RETYPES, VALRETYPES*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:abbrevnode", &i, 4, 0, 0, TNF_SHORTDECL);		/* subnodes: name; type; in-scope-body; expr */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "prescope", 2, COMPOPTYPE (occampi_prescope_abbrev));
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (occampi_scopein_abbrev));
	tnode_setcompop (cops, "scopeout", 2, COMPOPTYPE (occampi_scopeout_abbrev));
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (occampi_typecheck_abbrev));
	tnode_setcompop (cops, "constprop", 1, COMPOPTYPE (occampi_constprop_abbrev));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (occampi_namemap_abbrev));
	tnd->ops = cops;

	i = -1;
	opi.tag_ABBREV = tnode_newnodetag ("ABBREV", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_VALABBREV = tnode_newnodetag ("VALABBREV", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_RETYPES = tnode_newnodetag ("RETYPES", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_VALRETYPES = tnode_newnodetag ("VALRETYPES", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  occampi:procdecl -- PROCDECL*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:procdecl", &i, 4, 0, 0, TNF_LONGDECL);		/* subnodes: name; fparams; body; in-scope-body */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "prescope", 2, COMPOPTYPE (occampi_prescope_procdecl));
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (occampi_scopein_procdecl));
	tnode_setcompop (cops, "scopeout", 2, COMPOPTYPE (occampi_scopeout_procdecl));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (occampi_namemap_procdecl));
	tnode_setcompop (cops, "precheck", 1, COMPOPTYPE (occampi_precheck_procdecl));
	tnode_setcompop (cops, "fetrans", 2, COMPOPTYPE (occampi_fetrans_procdecl));
	tnode_setcompop (cops, "betrans", 2, COMPOPTYPE (occampi_betrans_procdecl));
	tnode_setcompop (cops, "precode", 2, COMPOPTYPE (occampi_precode_procdecl));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (occampi_codegen_procdecl));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "getdescriptor", 2, LANGOPTYPE (occampi_getdescriptor_procdecl));
	tnode_setlangop (lops, "gettype", 2, LANGOPTYPE (occampi_gettype_procdecl));
	tnode_setlangop (lops, "do_usagecheck", 2, LANGOPTYPE (occampi_usagecheck_procdecl));
	tnd->lops = lops;

	i = -1;
	opi.tag_PROCDECL = tnode_newnodetag ("PROCDECL", &i, tnd, NTF_INDENTED_PROC);

	/*}}}*/
	/*{{{  compiler hooks -- chook:arraydiminfo*/
	opi.chook_arraydiminfo = tnode_lookupornewchook ("chook:arraydiminfo");
	opi.chook_arraydiminfo->chook_copy = occampi_arraydiminfo_chook_copy;
	opi.chook_arraydiminfo->chook_free = occampi_arraydiminfo_chook_free;
	opi.chook_arraydiminfo->chook_dumptree = occampi_arraydiminfo_chook_dumptree;

	/*}}}*/

	return 0;
}
/*}}}*/
/*{{{  */
/*
 *	called to do any post-setup on declaration nodes
 *	returns 0 on success, non-zero on error
 */
static int occampi_decl_post_setup (void)
{
	tndef_t *tnd = (opi.tag_PROCDECL)->ndef;

	tnode_setcompop (tnd->ops, "miscnodetrans", 2, COMPOPTYPE (occampi_miscnodetrans_procdecl));

	return 0;
}
/*}}}*/


/*{{{  occampi_decl_feunit (feunit_t)*/
feunit_t occampi_decl_feunit = {
	init_nodes: occampi_decl_init_nodes,
	reg_reducers: NULL,
	init_dfatrans: NULL,
	post_setup: occampi_decl_post_setup,
	ident: "occampi-decl"
};


/*}}}*/

