/*
 *	allocate.c -- memory allocator for NOCC
 *	Copyright (C) 2005-2016 Fred Barnes <frmb@kent.ac.uk>
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
#include <stdint.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/types.h>

#include "nocc.h"
#include "support.h"
#include "version.h"
#include "fhandle.h"
#include "tnode.h"
#include "names.h"
#include "allocate.h"
#include "typecheck.h"
#include "target.h"

/*}}}*/


/*{{{  private types*/
typedef struct TAG_alloc_ivarmap {
	tnode_t *name;		/* always a back-end name */
	int alloc_wsh;		/* high workspace */
	int alloc_wsl;		/* low workspace */
	int alloc_vs;		/* vectorspace */
	int alloc_ms;		/* mobilespace */
	int refc;		/* reference-count inside the allocator */
	int ws_offset;		/* allocated workspace offset in block */
	int vs_offset;		/* allocated vectorspace offset in block */
	int ms_shadow;		/* offset of mobilespace shadow */
	int ms_offset;		/* allocated mobilespace offset in block */
} alloc_ivarmap_t;

typedef struct TAG_alloc_ovarmap {
	struct TAG_alloc_ovarmap *parent;
	DYNARRAY (alloc_ivarmap_t *, entries);
	DYNARRAY (struct TAG_alloc_ovarmap *, submaps);
	int size, offset;
} alloc_ovarmap_t;

typedef struct TAG_alloc_varmap {
	alloc_ovarmap_t *wsmap, *curwsmap;
	alloc_ovarmap_t *vsmap, *curvsmap;
	alloc_ovarmap_t *msmap, *curmsmap;
	int statics;			/* indicates we're mapping the statics of a block */
} alloc_varmap_t;

typedef struct TAG_alloc_assign {
	struct TAG_alloc_assign *parent;
	int lexlevel;
	tnode_t *block;
	target_t *target;
	chook_t *mapchook;
	chook_t *ev_chook;
} alloc_assign_t;


/*}}}*/

/*{{{  static void allocate_isetindent (fhandle_t *stream, int indent)*/
/*
 *	prints indentation (debugging)
 */
static void allocate_isetindent (fhandle_t *stream, int indent)
{
	int i;

	for (i=0; i<indent; i++) {
		fhandle_printf (stream, "    ");
	}

	return;
}
/*}}}*/


/*{{{  static alloc_ivarmap_t *allocate_ivarmap_create (void)*/
/*
 *	creates a new alloc_ivarmap_t
 */
static alloc_ivarmap_t *allocate_ivarmap_create (void)
{
	alloc_ivarmap_t *ivm = (alloc_ivarmap_t *)smalloc (sizeof (alloc_ivarmap_t));

	ivm->name = NULL;
	ivm->alloc_wsh = 0;
	ivm->alloc_wsl = 0;
	ivm->alloc_vs = 0;
	ivm->alloc_ms = 0;
	ivm->refc = 0;
	ivm->ws_offset = -1;
	ivm->vs_offset = -1;
	ivm->ms_shadow = -1;
	ivm->ms_offset = -1;
	return ivm;
}
/*}}}*/
/*{{{  static void allocate_ivarmap_free (alloc_ivarmap_t *ivm)*/
/*
 *	frees an alloc_ivarmap_t
 */
static void allocate_ivarmap_free (alloc_ivarmap_t *ivm)
{
	ivm->refc--;
	if (!ivm->refc) {
		sfree (ivm);
	}
	return;
}
/*}}}*/
/*{{{  static alloc_ovarmp_t *allocate_ovarmap_create (void)*/
/*
 *	creates a new alloc_ovarmap_t
 */
static alloc_ovarmap_t *allocate_ovarmap_create (void)
{
	alloc_ovarmap_t *ovm = (alloc_ovarmap_t *)smalloc (sizeof (alloc_ovarmap_t));

	ovm->parent = NULL;
	dynarray_init (ovm->submaps);
	dynarray_init (ovm->entries);

	return ovm;
}
/*}}}*/
/*{{{  static void allocate_ovarmap_free (alloc_ovarmap_t *ovm)*/
/*
 *	frees (recursively) an alloc_ovarmap_t structure
 */
static void allocate_ovarmap_free (alloc_ovarmap_t *ovm)
{
	int i;

	for (i=0; i<DA_CUR (ovm->submaps); i++) {
		if (DA_NTHITEM (ovm->submaps, i)) {
			allocate_ovarmap_free (DA_NTHITEM (ovm->submaps, i));
		}
	}
	dynarray_trash (ovm->submaps);
	for (i=0; i<DA_CUR (ovm->entries); i++) {
		if (DA_NTHITEM (ovm->entries, i)) {
			allocate_ivarmap_free (DA_NTHITEM (ovm->entries, i));
		}
	}
	dynarray_trash (ovm->entries);
	sfree (ovm);
	return;
}
/*}}}*/
/*{{{  static alloc_varmap_t *allocate_varmap_create (void)*/
/*
 *	creates a new alloc_varmap_t
 */
static alloc_varmap_t *allocate_varmap_create (void)
{
	alloc_varmap_t *avm = (alloc_varmap_t *)smalloc (sizeof (alloc_varmap_t));

	avm->wsmap = avm->curwsmap = allocate_ovarmap_create ();
	avm->vsmap = avm->curvsmap = allocate_ovarmap_create ();
	avm->msmap = avm->curmsmap = allocate_ovarmap_create ();
	avm->statics = 0;

	return avm;
}
/*}}}*/
/*{{{  static void allocate_varmap_free (alloc_varmap_t *avm)*/
/*
 *	frees (recursively) an alloc_varmap_t structure
 */
static void allocate_varmap_free (alloc_varmap_t *avm)
{
	if (avm->wsmap) {
		allocate_ovarmap_free (avm->wsmap);
	}
	if (avm->vsmap) {
		allocate_ovarmap_free (avm->vsmap);
	}
	if (avm->msmap) {
		allocate_ovarmap_free (avm->msmap);
	}

	sfree (avm);
	return;
}
/*}}}*/
/*{{{  static void allocate_ovarmap_dump (alloc_ovarmap_t *ovm, fhandle_t *stream, int indent)*/
/*
 *	dumps an alloc_ovarmap_t structure (debugging)
 */
static void allocate_ovarmap_dump (alloc_ovarmap_t *ovm, fhandle_t *stream, int indent)
{
	int i;

	allocate_isetindent (stream, indent);

	fhandle_printf (stream, "map at %p: %d sub-maps, %d items (size=%d, offset=%d): ", ovm, DA_CUR (ovm->submaps), DA_CUR (ovm->entries), ovm->size, ovm->offset);
	for (i=0; i<DA_CUR (ovm->entries); i++) {
		alloc_ivarmap_t *ivm = DA_NTHITEM (ovm->entries, i);
		tnode_t *name = ivm->name;

		fhandle_printf (stream, "%p (%s,%s) [%d,%d,%d,%d] @[%d,%d,%d(%d)]   ", name, name->tag->name, tnode_nthsubof (name, 0)->tag->name,
				ivm->alloc_wsh, ivm->alloc_wsl, ivm->alloc_vs, ivm->alloc_ms, ivm->ws_offset, ivm->vs_offset, ivm->ms_offset, ivm->ms_shadow);
	}
	fhandle_printf (stream, "\n");
	for (i=0; i<DA_CUR (ovm->submaps); i++) {
		allocate_ovarmap_dump (DA_NTHITEM (ovm->submaps, i), stream, indent + 1);
	}
	return;
}
/*}}}*/
/*{{{  static void allocate_varmap_dump (alloc_varmap_t *avm, fhandle_t *stream)*/
/*
 *	dumps an alloc_varmap_t structure (debugging)
 */
static void allocate_varmap_dump (alloc_varmap_t *avm, fhandle_t *stream)
{
	if (!avm) {
		fhandle_printf (stream, "NULL avm!\n");
		return;
	}
	if (avm->wsmap) {
		fhandle_printf (stream, "workspace-map at %p, curmap at %p:\n", avm->wsmap, avm->curwsmap);
		allocate_ovarmap_dump (avm->wsmap, stream, 1);
	}
	if (avm->vsmap) {
		fhandle_printf (stream, "vectorspace-map at %p, curmap at %p:\n", avm->vsmap, avm->curvsmap);
		allocate_ovarmap_dump (avm->vsmap, stream, 1);
	}
	if (avm->msmap) {
		fhandle_printf (stream, "mobilespace-map at %p, curmap at %p:\n", avm->msmap, avm->curmsmap);
		allocate_ovarmap_dump (avm->msmap, stream, 1);
	}
	return;
}
/*}}}*/


/*{{{  static void allocate_extravars_chook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)*/
/*
 *	dumps (debugging) extra variables for allocation attached to a node
 */
static void allocate_extravars_chook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)
{
	tnode_t *evars = (tnode_t *)hook;

	allocate_isetindent (stream, indent);
	fhandle_printf (stream, "<chook id=\"alloc:extravars\" addr=\"%p\">\n", hook);
	tnode_dumptree (evars, indent + 1, stream);
	allocate_isetindent (stream, indent);
	fhandle_printf (stream, "</chook>\n");

	return;
}
/*}}}*/


/*{{{  static void allocate_ovarmap_addname (alloc_varmap_t *avm, tnode_t *bename, target_t *target)*/
/*
 *	adds a back-end name to the given map
 */
static void allocate_ovarmap_addname (alloc_varmap_t *avm, tnode_t *bename, target_t *target)
{
	alloc_ivarmap_t *ivm;

	ivm = allocate_ivarmap_create ();
	ivm->name = bename;
	target->be_allocsize (ivm->name, &ivm->alloc_wsh, &ivm->alloc_wsl, &ivm->alloc_vs, &ivm->alloc_ms);

#if 0
fprintf (stderr, "allocate_ovarmap_addname(): new ivm, allocating [%d,%d,%d,%d]\n", ivm->alloc_wsh, ivm->alloc_wsl, ivm->alloc_vs, ivm->alloc_ms);
#endif
	/* always create new ovarmap's for this, in the memory spaces required */
	if (ivm->alloc_wsh || ivm->alloc_wsl) {
		/* allocate in workspace */
		alloc_ovarmap_t *newovm = allocate_ovarmap_create ();
		alloc_ovarmap_t *ovm = avm->curwsmap;

		newovm->parent = ovm;
		dynarray_add (ovm->submaps, newovm);
		/* if this needs high and low WS, goes in at the bottom */
		if (ivm->alloc_wsh && ivm->alloc_wsl) {
#if 0
fprintf (stderr, "allocate_ovarmap_addname(): shared high/low WS, inserting at start-of-list\n");
#endif
			dynarray_insert (newovm->entries, ivm, 0);
		} else {
			dynarray_add (newovm->entries, ivm);
		}

		avm->curwsmap = newovm;
		ivm->refc++;
	}
	if (ivm->alloc_vs) {
		/* allocate in vectorspace */
		alloc_ovarmap_t *newovm = allocate_ovarmap_create ();
		alloc_ovarmap_t *ovm = avm->curvsmap;

		newovm->parent = ovm;
		dynarray_add (ovm->submaps, newovm);
		dynarray_add (newovm->entries, ivm);

		avm->curvsmap = newovm;
		ivm->refc++;
	}
	if (ivm->alloc_ms) {
		/* allocate in mobilespace */
		alloc_ovarmap_t *newovm = allocate_ovarmap_create ();
		alloc_ovarmap_t *ovm = avm->curmsmap;

		newovm->parent = ovm;
		dynarray_add (ovm->submaps, newovm);
		dynarray_add (newovm->entries, ivm);

		avm->curmsmap = newovm;
		ivm->refc++;
	}

	return;
}
/*}}}*/
/*{{{  static void allocate_ovarmap_delname (alloc_varmap_t *avm, tnode_t *bename, target_t *target)*/
/*
 *	descopes a back-end name from the given map
 */
static void allocate_ovarmap_delname (alloc_varmap_t *avm, tnode_t *bename, target_t *target)
{
	int i_ws, i_vs, i_ms;

	/*{{{  search ws, vs and ms maps*/
	if (avm->curwsmap) {
		for (i_ws=0; i_ws<DA_CUR (avm->curwsmap->entries); i_ws++) {
			alloc_ivarmap_t *ivm = DA_NTHITEM (avm->curwsmap->entries, i_ws);

			if (ivm->name == bename) {
				break;
			}
		}
		if (i_ws == DA_CUR (avm->curwsmap->entries)) {
			i_ws = -1;
		}
	} else {
		i_ws = -1;
	}
	if (avm->curvsmap) {
		for (i_vs=0; i_vs<DA_CUR (avm->curvsmap->entries); i_vs++) {
			alloc_ivarmap_t *ivm = DA_NTHITEM (avm->curvsmap->entries, i_vs);

			if (ivm->name == bename) {
				break;
			}
		}
		if (i_vs == DA_CUR (avm->curvsmap->entries)) {
			i_vs = -1;
		}
	} else {
		i_vs = -1;
	}
	if (avm->curmsmap) {
		for (i_ms=0; i_ms<DA_CUR (avm->curmsmap->entries); i_ms++) {
			alloc_ivarmap_t *ivm = DA_NTHITEM (avm->curmsmap->entries, i_ms);

			if (ivm->name == bename) {
				break;
			}
		}
		if (i_ms == DA_CUR (avm->curmsmap->entries)) {
			i_ms = -1;
		}
	} else {
		i_ms = -1;
	}
	/*}}}*/
#if 0
fprintf (stderr, "allocate_ovarmap_delname(): i_ws = %d, i_vs = %d, i_ms = %d\n", i_ws, i_vs, i_ms);
#endif
	/*{{{  (serious) error it not here*/
	if ((i_ws == -1) && (i_vs == -1) && (i_ms == -1)) {
#if 0
fprintf (stderr, "allocate_ovarmap_delname(): bename was:\n");
tnode_dumptree (bename, 1, stderr);
#endif
		nocc_internal ("allocate_ovarmap_delname(): name (%p) not found\n", bename);
		return;
	}
	/*}}}*/
	/*{{{  update parents*/
	if (i_ws >= 0) {
		if (!avm->curwsmap->parent) {
			nocc_internal ("allocate_ovarmap_delname(): no ws parent!\n");
			return;
		}
		avm->curwsmap = avm->curwsmap->parent;
	}
	if (i_vs >= 0) {
		if (!avm->curvsmap->parent) {
			nocc_internal ("allocate_ovarmap_delname(): no vs parent!\n");
			return;
		}
		avm->curvsmap = avm->curvsmap->parent;
	}
	if (i_ms >= 0) {
		if (!avm->curmsmap->parent) {
			nocc_internal ("allocate_ovarmap_delname(): no ms parent!\n");
			return;
		}
		avm->curmsmap = avm->curmsmap->parent;
	}
	/*}}}*/

	return;
}
/*}}}*/
/*{{{  static void allocate_squeeze_map_inner (alloc_ovarmap_t *ovm, alloc_ovarmap_t **refptr, target_t *target)*/
/*
 *	squeezes up space in an individual map
 */
static void allocate_squeeze_map_inner (alloc_ovarmap_t *ovm, alloc_ovarmap_t **refptr, target_t *target)
{
	int i;

	while (DA_CUR (ovm->submaps) == 1) {
		/* move contents of submap into this */
		alloc_ovarmap_t *next_ovm = DA_NTHITEM (ovm->submaps, 0);

		dynarray_delitem (ovm->submaps, 0);
		for (i=0; i<DA_CUR (next_ovm->entries); i++) {
			alloc_ivarmap_t *ivm = DA_NTHITEM (next_ovm->entries, i);

			DA_SETNTHITEM (next_ovm->entries, i, NULL);
			if (ivm->alloc_wsh && ivm->alloc_wsl) {
				/* high and low WS required, put at start of map */
				dynarray_insert (ovm->entries, ivm, 0);
			} else {
				dynarray_add (ovm->entries, ivm);
			}
		}
		for (i=0; i<DA_CUR (next_ovm->submaps); i++) {
			alloc_ovarmap_t *submap = DA_NTHITEM (next_ovm->submaps, i);

			DA_SETNTHITEM (next_ovm->submaps, i, NULL);
			dynarray_add (ovm->submaps, submap);
		}
		if (refptr && (*refptr == next_ovm)) {
			*refptr = ovm;
		}
		allocate_ovarmap_free (next_ovm);

		/* keep doing this map until we can't do any more */
	}

	/* recursively go down and do sub-maps */
	for (i=0; i<DA_CUR (ovm->submaps); i++) {
		allocate_squeeze_map_inner (DA_NTHITEM (ovm->submaps, i), refptr, target);
	}
	return;
}
/*}}}*/
/*{{{  static void allocate_squeeze_map (alloc_varmap_t *varmap, target_t *target)*/
/*
 *	squeezes together entries in a map where possible
 */
static void allocate_squeeze_map (alloc_varmap_t *varmap, target_t *target)
{
	if (varmap->wsmap) {
		allocate_squeeze_map_inner (varmap->wsmap, &(varmap->curwsmap), target);
	}
	if (varmap->vsmap) {
		allocate_squeeze_map_inner (varmap->vsmap, &(varmap->curvsmap), target);
	}
	if (varmap->msmap) {
		allocate_squeeze_map_inner (varmap->msmap, &(varmap->curmsmap), target);
	}

	return;
}
/*}}}*/
/*{{{  static void allocate_squeeze_curmap (alloc_varmap_t *varmap, target_t *target)*/
/*
 *	squeezes together entries in a map where possible, only does it from the current markers however
 */
static void allocate_squeeze_curmap (alloc_varmap_t *varmap, target_t *target)
{
	int i;

	if (varmap->wsmap) {
		for (i=0; i<DA_CUR (varmap->curwsmap->submaps); i++) {
			allocate_squeeze_map_inner (DA_NTHITEM (varmap->curwsmap->submaps, i), NULL, target);
		}
	}
	if (varmap->vsmap) {
		for (i=0; i<DA_CUR (varmap->curvsmap->submaps); i++) {
			allocate_squeeze_map_inner (DA_NTHITEM (varmap->curvsmap->submaps, i), NULL, target);
		}
	}
	if (varmap->msmap) {
		for (i=0; i<DA_CUR (varmap->curmsmap->submaps); i++) {
			allocate_squeeze_map_inner (DA_NTHITEM (varmap->curmsmap->submaps, i), NULL, target);
		}
	}

	return;
}
/*}}}*/


/*{{{  static int allocate_compare_wsentries (alloc_ivarmap_t *ivm1, alloc_ivarmap_t *ivm2)*/
/*
 *	compares two workspace entities -- fudged by the fact that entries sharing high and low ws are pulled down
 */
static int allocate_compare_wsentries (alloc_ivarmap_t *ivm1, alloc_ivarmap_t *ivm2)
{
	if (!ivm1 && !ivm2) {
		return 0;
	} else if (!ivm2) {
		return -1;
	} else if (!ivm1) {
		return 1;
	}

	if (ivm1->alloc_wsh && ivm1->alloc_wsl) {
		return -1;
	} else if (ivm2->alloc_wsh && ivm2->alloc_wsl) {
		return 1;
	}

	/* sort remaining ones by high-size */
	return ivm2->alloc_wsh - ivm1->alloc_wsh;
}
/*}}}*/
/*{{{  static int allocate_compare_submaps (alloc_ovarmap_t *ovm1, alloc_ovarmap_t *ovm2)*/
/*
 *	compares submap sizes for sorting
 */
static int allocate_compare_submaps (alloc_ovarmap_t *ovm1, alloc_ovarmap_t *ovm2)
{
	return ovm2->size - ovm1->size;
}
/*}}}*/
/*{{{  static void allocate_size_map (alloc_ovarmap_t *ovm, target_t *target, int which)*/
/*
 *	sizes up an allocation tree.  "which" indicates ws, vs or ms
 */
static void allocate_size_map (alloc_ovarmap_t *ovm, target_t *target, int which)
{
	int i;

	/*{{{  set size to zero first*/
	ovm->size = 0;
	ovm->offset = 0;		/* only used for workspace */

	/*}}}*/
	/*{{{  size submaps*/
	for (i=0; i<DA_CUR (ovm->submaps); i++) {
		allocate_size_map (DA_NTHITEM (ovm->submaps, i), target, which);
	}

	/*}}}*/
	/*{{{  if any entry shares high-ws and low-ws, it must go first (slot 0 counts as high-ws, possibly used by descheduling calls)*/
#if 0
	if ((which == 0) && DA_CUR (ovm->entries)) {
		dynarray_qsort (ovm->entries, allocate_compare_wsentries);
	}
#endif
	/* FIXME: can't do this here cos it may re-arrange statics (not allowed!) */

	/*}}}*/
	/*{{{  set our size to the size of the largest submap*/
	if (which == 0) {
		/*{{{  workspace allocation is slightly special -- need to track offset carefully*/
		int mostbelow = 0;

		for (i=0; i<DA_CUR (ovm->submaps); i++) {
			alloc_ovarmap_t *submap = DA_NTHITEM (ovm->submaps, i);
			int subrealsize = submap->offset;
			int subbelow = submap->size - submap->offset;

			if (subrealsize > ovm->size) {
				ovm->size = subrealsize;
			}
			if (subbelow > mostbelow) {
				mostbelow = subbelow;
			}
		}

		/* align submaps */
		for (i=0; i<DA_CUR (ovm->submaps); i++) {
			alloc_ovarmap_t *submap = DA_NTHITEM (ovm->submaps, i);

			if (submap->offset < ovm->size) {
				int diff = ovm->size - submap->offset;

				submap->offset += diff;
				submap->size += diff;
			}
		}

		/* set our size and add below-ws space */
		ovm->offset = ovm->size;
		ovm->size += mostbelow;
		/*}}}*/
	} else {
		/*{{{  vectorspace/mobilespace sizing*/
		for (i=0; i<DA_CUR (ovm->submaps); i++) {
			alloc_ovarmap_t *submap = DA_NTHITEM (ovm->submaps, i);

			if (submap->size > ovm->size) {
				ovm->size = submap->size;
			}
		}
		/*}}}*/
	}

	/*}}}*/
	/*{{{  add entries to our size*/
	for (i=0; i<DA_CUR (ovm->entries); i++) {
		alloc_ivarmap_t *ivm = DA_NTHITEM (ovm->entries, i);
		int j, k;

		switch (which) {
			/*{{{  0 -- workspace*/
		case 0:
			j = ivm->alloc_wsh;
			k = ivm->alloc_wsl;
			break;
			/*}}}*/
			/*{{{  1 -- vectorspace*/
		case 1:
			j = ivm->alloc_vs;
			k = 0;
			break;
			/*}}}*/
			/*{{{  2 -- mobilespace*/
		case 2:
			j = ivm->alloc_ms;
			k = target->pointersize;	/* shadow slot */
			break;
			/*}}}*/
			/*{{{  default -- error*/
		default:
			nocc_internal ("allocate_size_map(): bad memory selector %d!\n", which);
			return;
			/*}}}*/
		}

		if (j & (target->slotsize - 1)) {
			/* round up */
			j &= ~(target->slotsize - 1);
			j += target->slotsize;
		}
		if (k & (target->slotsize - 1)) {
			/* round up */
			k &= ~(target->slotsize - 1);
			k += target->slotsize;
		}

		switch (which) {
			/*{{{  0 -- workspace*/
		case 0:
			ovm->size += j;
			ovm->offset += j;

			/* if k (below ws) is bigger than size - offset, adjust size to incorporate */
			if (k > (ovm->size - ovm->offset)) {
				ovm->size += (k - (ovm->size - ovm->offset));
			}
			break;
			/*}}}*/
			/*{{{  1, 2 -- vectorspace/mobilespace*/
		case 1:
		case 2:
			ovm->size += (j + k);
			break;
			/*}}}*/
		}
	}

	/*}}}*/
	/*{{{  sort submaps in descreasing order of size*/
	if (DA_CUR (ovm->submaps)) {
		dynarray_qsort (ovm->submaps, allocate_compare_submaps);
	}

	/*}}}*/

	return;
}
/*}}}*/
/*{{{  static void allocate_size_maps (alloc_varmap_t *avm, target_t *target)*/
/*
 *	sizes up the various maps
 */
static void allocate_size_maps (alloc_varmap_t *avm, target_t *target)
{
	allocate_size_map (avm->wsmap, target, 0);
	allocate_size_map (avm->vsmap, target, 1);
	allocate_size_map (avm->msmap, target, 2);

	return;
}
/*}}}*/


/*{{{  static void allocate_workspace_offsets (alloc_ovarmap_t *ovm, target_t *target)*/
/*
 *	performs workspace allocation (assigning offsets)
 */
static void allocate_workspace_offsets (alloc_ovarmap_t *ovm, target_t *target)
{
	int i;
	int thisoffset = 0;

	/*{{{  do offsets in submaps first*/
	for (i=0; i<DA_CUR (ovm->submaps); i++) {
		alloc_ovarmap_t *submap = DA_NTHITEM (ovm->submaps, i);

		allocate_workspace_offsets (submap, target);
		thisoffset = submap->offset;
	}
	/*}}}*/
	/*{{{  allocate our entries to WS positions (only if high-ws)*/
	for (i=0; i<DA_CUR (ovm->entries); i++) {
		alloc_ivarmap_t *ivm = DA_NTHITEM (ovm->entries, i);
		int wshigh = ivm->alloc_wsh;

		if (wshigh > 0) {
			if (wshigh & (target->slotsize - 1)) {
				/* round up */
				wshigh &= ~(target->slotsize - 1);
				wshigh += target->slotsize;
			}
			ivm->ws_offset = thisoffset;
			thisoffset += wshigh;
		}
	}
	/*}}}*/
	/*{{{  sanity check*/
	if (thisoffset > ovm->offset) {
		nocc_internal ("allocate_workspace_offsets(): finished with thisoffset=%d, ovm->offset=%d", thisoffset, ovm->offset);
		return;
	}
	/*}}}*/

	return;
}
/*}}}*/
/*{{{  static void allocate_vectorspace_offsets (alloc_ovarmap_t *ovm, target_t *target)*/
/*
 *	assigns offsets to objects in vectorspace
 */
static void allocate_vectorspace_offsets (alloc_ovarmap_t *ovm, target_t *target)
{
	int i;
	int thisoffset = 0;

	/*{{{  do offsets in submaps first*/
	for (i=0; i<DA_CUR (ovm->submaps); i++) {
		alloc_ovarmap_t *submap = DA_NTHITEM (ovm->submaps, i);

		allocate_vectorspace_offsets (submap, target);
		thisoffset = submap->offset;
	}
	/*}}}*/
	/*{{{  allocate our entries to VS positions*/
	for (i=0; i<DA_CUR (ovm->entries); i++) {
		alloc_ivarmap_t *ivm = DA_NTHITEM (ovm->entries, i);
		int vsbytes = ivm->alloc_vs;

		if (vsbytes > 0) {
			if (vsbytes & (target->slotsize - 1)) {
				/* round up */
				vsbytes &= ~(target->slotsize - 1);
				vsbytes += target->slotsize;
			}
			ivm->vs_offset = thisoffset;
			thisoffset += vsbytes;
		}
	}
	/*}}}*/
}
/*}}}*/
/*{{{  static void allocate_mobilespace_offsets (alloc_ovarmap_t *ovm, target_t *target)*/
/*
 *	assigns offsets to objects in mobilespace
 */
static void allocate_mobilespace_offsets (alloc_ovarmap_t *ovm, target_t *target)
{
	int i;
	int thisoffset = 0;
	int shoffset = 0;

	/*{{{  do offsets in submaps first*/
	for (i=0; i<DA_CUR (ovm->submaps); i++) {
		alloc_ovarmap_t *submap = DA_NTHITEM (ovm->submaps, i);

		allocate_mobilespace_offsets (submap, target);
		thisoffset = submap->offset;
	}
	/*}}}*/
	/*{{{  offsets after shadows*/
	thisoffset = DA_CUR (ovm->entries) * target->pointersize;

	/*}}}*/
	/*{{{  allocate our entries to (static) mobilespace positions*/
	for (i=0; i<DA_CUR (ovm->entries); i++, shoffset += target->pointersize) {
		alloc_ivarmap_t *ivm = DA_NTHITEM (ovm->entries, i);
		int msbytes = ivm->alloc_ms;

		if (msbytes > 0) {
			if (msbytes & (target->slotsize - 1)) {
				/* round up */
				msbytes &= ~(target->slotsize - 1);
				msbytes += target->slotsize;
			}
			ivm->ms_offset = thisoffset;
			ivm->ms_shadow = shoffset;

			thisoffset += msbytes;
		}
	}
	/*}}}*/
}
/*}}}*/


/*{{{  static void allocate_inner_setoffsets (alloc_ovarmap_t *ovm, target_t *target)*/
/*
 *	sets back-end offsets for a specific map
 */
static void allocate_inner_setoffsets (alloc_ovarmap_t *ovm, target_t *target)
{
	int i;

	/*{{{  do submaps*/
	for (i=0; i<DA_CUR (ovm->submaps); i++) {
		allocate_inner_setoffsets (DA_NTHITEM (ovm->submaps, i), target);
	}
	/*}}}*/
	/*{{{  set item offsets*/
	for (i=0; i<DA_CUR (ovm->entries); i++) {
		alloc_ivarmap_t *ivm = DA_NTHITEM (ovm->entries, i);

		if ((ivm->ws_offset >= 0) || (ivm->vs_offset >= 0) || (ivm->ms_offset >= 0)) {
			target->be_setoffsets (ivm->name, ivm->ws_offset, ivm->vs_offset, ivm->ms_offset, ivm->ms_shadow);
		}
	}
	/*}}}*/

	return;
}
/*}}}*/
/*{{{  static void allocate_setoffsets (alloc_varmap_t *avm, target_t *target)*/
/*
 *	traverses allocation maps and sets back-end offsets
 *	(may set some things more than once, but that's safe)
 */
static void allocate_setoffsets (alloc_varmap_t *avm, target_t *target)
{
	if (avm->wsmap) {
		allocate_inner_setoffsets (avm->wsmap, target);
	}
	if (avm->vsmap) {
		allocate_inner_setoffsets (avm->vsmap, target);
	}
	if (avm->msmap) {
		allocate_inner_setoffsets (avm->msmap, target);
	}

	return;
}
/*}}}*/


/*{{{  static int allocate_prewalktree_names (tnode_t *node, void *data)*/
/*
 *	inside a BLOCK, performs allocation for NAMEs
 *	returns 0 to stop walk, 1 to continue
 */
static int allocate_prewalktree_names (tnode_t *node, void *data)
{
	allocate_t *adata = (allocate_t *)data;
	alloc_varmap_t *avm = (alloc_varmap_t *)adata->allochook;
	target_t *target = adata->target;

	if (node->tag == target->tag_NAME) {
		/* allocate this one */
		allocate_ovarmap_addname (avm, node, target);
		if (!avm->statics) {
			/* do the body then remove */
			tnode_prewalktree (tnode_nthsubof (node, 1), allocate_prewalktree_names, data);
			allocate_ovarmap_delname (avm, node, target);
		}
		return 0;
	} else if (node->tag == target->tag_BLOCK) {
		/* skip this */
		return 0;
	} else if (node->tag == target->tag_BLOCKREF) {
		/* allocate this one */
		allocate_ovarmap_addname (avm, node, target);
		allocate_ovarmap_delname (avm, node, target);
		return 0;
	}
	return 1;
}
/*}}}*/
/*{{{  static int allocate_prewalktree_blocks (tnode_t *node, void *data)*/
/*
 *	performs allocation for BLOCK nodes
 *	returns 0 to stop walk, 1 to continue
 */
static int allocate_prewalktree_blocks (tnode_t *node, void *data)
{
	allocate_t *adata = (allocate_t *)data;

	if (node->tag == adata->target->tag_BLOCK) {
		alloc_varmap_t *avmap;
		void *saved_hook = adata->allochook;

		/*{{{  map nested blocks first*/
#if 0
fprintf (stderr, "allocate_prewalktree_blocks(): allocating for a block at %p\n", node);
#endif
		tnode_prewalktree (tnode_nthsubof (node, 0), allocate_prewalktree_blocks, (void *)adata);

		/*}}}*/
		/*{{{  map allocation in this block, statics first, then body*/
		avmap = allocate_varmap_create ();
		adata->allochook = (void *)avmap;

		avmap->statics = 1;
		tnode_prewalktree (tnode_nthsubof (node, 1), allocate_prewalktree_names, (void *)adata);

		/* squeeze up the statics (more can be added later) */
		allocate_squeeze_map (avmap, adata->target);
#if 0
fprintf (stderr, "allocate_prewalktree_blocks(): allocated statics!  maps are:\n");
allocate_varmap_dump (avmap, stderr);
#endif

		avmap->statics = 0;
		tnode_prewalktree (tnode_nthsubof (node, 0), allocate_prewalktree_names, (void *)adata);

#if 0
fprintf (stderr, "allocate_prewalktree_blocks(): allocated rest!  maps are:\n");
allocate_varmap_dump (avmap, stderr);
#endif

		/*}}}*/
		/*{{{  squeeze up general allocations*/
		allocate_squeeze_curmap (avmap, adata->target);

		/*}}}*/
		/*{{{  size*/
		allocate_size_maps (avmap, adata->target);
#if 0
fprintf (stderr, "allocate_prewalktree_blocks(): sized maps!  maps are:\n");
allocate_varmap_dump (avmap, stderr);
#endif

		/*}}}*/
		/*{{{  allocate workspace, vectorspace and mobilespace offsets*/
		allocate_workspace_offsets (avmap->wsmap, adata->target);
		allocate_vectorspace_offsets (avmap->vsmap, adata->target);
		allocate_mobilespace_offsets (avmap->msmap, adata->target);

		/*}}}*/
		/*{{{  copy offsets back into back-end nodes*/
		allocate_setoffsets (avmap, adata->target);
#if 0
fprintf (stderr, "allocate_prewalktree_blocks(): allocated+copied offsets!  maps are:\n");
allocate_varmap_dump (avmap, stderr);
#endif

		/*}}}*/
		/*{{{  set ws, vs, ms and static-adjust for the block*/
		{
			int adjust = 0;

			if (DA_CUR (avmap->wsmap->submaps) > 0) {
				alloc_ovarmap_t *inner = DA_NTHITEM (avmap->wsmap->submaps, 0);

				adjust = avmap->wsmap->size - inner->size;
			} else {
				adjust = avmap->wsmap->size;
			}
			adata->target->be_setblocksize (node, avmap->wsmap->size, avmap->wsmap->offset, avmap->vsmap->size, avmap->msmap->size, adjust);
		}
		/*}}}*/
		/*{{{  put back previous map*/
		adata->allochook = saved_hook;
		if (compopts.dumpvarmaps) {
			allocate_varmap_dump (avmap, FHAN_STDERR);
		}
#if 0
fprintf (stderr, "allocate_prewalktree_blocks(): allocated a block!  maps are:\n");
allocate_varmap_dump (avmap, stderr);
#endif
		/*}}}*/

		tnode_setchook (node, adata->varmap_chook, (void *)avmap);
		/* allocate_varmap_free (avmap); */
		return 0;
	}

	return 1;
}
/*}}}*/
/*{{{  static int allocate_prewalktree_assign_namerefs (tnode_t *node, void *data)*/
/*
 *	fills in allocation information for name-references (proper)
 *	return 0 to stop walk, 1 to continue
 */
static int allocate_prewalktree_assign_namerefs (tnode_t *node, void *data)
{
	alloc_assign_t *apriv = (alloc_assign_t *)data;
	tnode_t *evars;

	/* if the node has "extra vars" to map, do these first */
#if 0
fprintf (stderr, "allocate_prewalktree_assign_namerefs(): node=%p, data=%p, apriv->ev_chook=%p\n", node, data, apriv->ev_chook);
#endif
	evars = (tnode_t *)tnode_getchook (node, apriv->ev_chook);
	if (evars) {
		/* allocate */
		tnode_prewalktree (evars, allocate_prewalktree_assign_namerefs, data);
	}

	if (node->tag == apriv->target->tag_NAMEREF) {
		int ref_lexlevel = apriv->target->be_blocklexlevel (node);
		int act_lexlevel;
		tnode_t *name = tnode_nthsubof (node, 0);
		tnode_t *namehook = (tnode_t *)tnode_getchook (name, apriv->mapchook);


		/* namehook should be the corresponding back-end NAME */
		if (!namehook) {
			nocc_internal ("allocate_prewalktree_assign_namerefs(): nameref has no mapchook!");
		}
		act_lexlevel = apriv->target->be_blocklexlevel (namehook);
#if 0
fprintf (stderr, "allocate_prewalktree_assign_namerefs(): found NAMREF!  ref_lexlevel = %d, act_lexlevel = %d\n", ref_lexlevel, act_lexlevel);
#endif

		if (ref_lexlevel == act_lexlevel) {
			/* simple :) */
			int ws_offs, vs_offs, ms_offs, ms_shdw;

			apriv->target->be_getoffsets (namehook, &ws_offs, &vs_offs, &ms_offs, &ms_shdw);
			apriv->target->be_setoffsets (node, ws_offs, vs_offs, ms_offs, ms_shdw);
		} else {
			/* actually..  do the same thing here -- let code-gen take care of it */
			int ws_offs, vs_offs, ms_offs, ms_shdw;

			apriv->target->be_getoffsets (namehook, &ws_offs, &vs_offs, &ms_offs, &ms_shdw);
			apriv->target->be_setoffsets (node, ws_offs, vs_offs, ms_offs, ms_shdw);
		}
		return 0;
	}
	return 1;
}
/*}}}*/
/*{{{  static int allocate_prewalktree_assign_blocks (tnode_t *node, void *data)*/
/*
 *	fills in allocation information for name-references and such-like (after allocation of all blocks)
 *	this deals with the blocks
 *	return 0 to stop walk, 1 to continue
 */
static int allocate_prewalktree_assign_blocks (tnode_t *node, void *data)
{
	alloc_assign_t *apriv = (alloc_assign_t *)data;

	if (node->tag == apriv->target->tag_BLOCK) {
		alloc_assign_t *newpriv;

		newpriv = (alloc_assign_t *)smalloc (sizeof (alloc_assign_t));
		newpriv->parent = apriv;
		newpriv->target = apriv->target;
		newpriv->mapchook = apriv->mapchook;
		newpriv->ev_chook = apriv->ev_chook;
		newpriv->block = node;
		newpriv->lexlevel = newpriv->target->be_blocklexlevel (node);

		/* map references in nested blocks */
		tnode_prewalktree (tnode_nthsubof (node, 0), allocate_prewalktree_assign_blocks, (void *)newpriv);

		/* map references in this block */
		tnode_prewalktree (tnode_nthsubof (node, 0), allocate_prewalktree_assign_namerefs, (void *)newpriv);

		sfree (newpriv);
		return 0;
	}
	return 1;
}
/*}}}*/
/*{{{  static int allocate_prewalktree_preallocate (tnode *tptr, void *data)*/
/*
 *	does pre-allocation calls on nodes
 *	returns 0 to stop walk, 1 to continue
 */
static int allocate_prewalktree_preallocate (tnode_t *tptr, void *data)
{
	target_t *target = (target_t *)data;

	if (tptr && tptr->tag->ndef->ops && tnode_hascompop_i (tptr->tag->ndef->ops, (int)COPS_PREALLOCATE)) {
		return tnode_callcompop_i (tptr->tag->ndef->ops, (int)COPS_PREALLOCATE, 2, tptr, target);
	}
	return 1;
}
/*}}}*/


/*{{{  int preallocate_tree (tnode_t **tptr, target_t *target)*/
/*
 * 	top-level pre-allocation
 * 	returns 0 on success, non-zero on failure
 */
int preallocate_tree (tnode_t **tptr, target_t *target)
{
	if (target->be_do_preallocate) {
		target->be_do_preallocate (*tptr, target);
	} else {
		tnode_prewalktree (*tptr, allocate_prewalktree_preallocate, (void *)target);
	}

	return 0;
}
/*}}}*/
/*{{{  int allocate_tree (tnode_t **tptr, target_t *target)*/
/*
 *	top-level workspace/vectorspace/mobilespace allocation
 *	return 0 on success, non-zero on failure
 */
int allocate_tree (tnode_t **tptr, target_t *target)
{
	if (!target->skipallocate) {
		allocate_t *adata = (allocate_t *)smalloc (sizeof (allocate_t));
		alloc_assign_t *apriv;

		adata->target = target;
		adata->allochook = NULL;
		adata->varmap_chook = tnode_lookupornewchook ("alloc:varmap");
		adata->ev_chook = tnode_lookupornewchook ("alloc:extravars");
		adata->ev_chook->chook_dumptree = allocate_extravars_chook_dumptree;
		tnode_prewalktree (*tptr, allocate_prewalktree_blocks, (void *)adata);

		sfree (adata);
		apriv = (alloc_assign_t *)smalloc (sizeof (alloc_assign_t));
		apriv->parent = NULL;
		apriv->block = NULL;
		apriv->lexlevel = -1;
		apriv->target = target;
		apriv->mapchook = tnode_lookupchookbyname ("map:mapnames");
		apriv->ev_chook = tnode_lookupornewchook ("alloc:extravars");

#if 0
fprintf (stderr, "allocate_tree(): about to assign blocks, apriv->ev_chook = %p\n", apriv->ev_chook);
#endif

		tnode_prewalktree (*tptr, allocate_prewalktree_assign_blocks, (void *)apriv);

		sfree (apriv);
	}

	return 0;
}
/*}}}*/


/*{{{  static int allocate_walkovarmap (alloc_ovarmap_t *ovm, int (*map_func)(void *, int, int, int, int, void *), int (*item_func)(void *, tnode_t *, int *, int *, void *), void *maparg, void *itemarg)*/
/*
 *	internal for allocate_walkvarmap() below
 */
static int allocate_walkovarmap (alloc_ovarmap_t *ovm, int (*map_func)(void *, int, int, int, int, void *), int (*item_func)(void *, tnode_t *, int *, int *, void *), void *maparg, void *itemarg)
{
	int r = 0;
	int i;

	if (map_func) {
		r = map_func ((void *)ovm, ovm->size, ovm->offset, DA_CUR (ovm->submaps), DA_CUR (ovm->entries), maparg);
	}
	if (r < 0) {
		/* abort */
		return r;
	}
	if (item_func) {
		for (i=0; i<DA_CUR (ovm->entries); i++) {
			alloc_ivarmap_t *ivm = DA_NTHITEM (ovm->entries, i);
			int sizes[4] = {ivm->alloc_wsh, ivm->alloc_wsl, ivm->alloc_vs, ivm->alloc_ms};
			int offsets[4] = {ivm->ws_offset, ivm->vs_offset, ivm->ms_shadow, ivm->ms_offset};

			/* each (int *) argument is to the data */
			r = item_func ((void *)ivm, ivm->name, sizes, offsets, itemarg);
			if (r < 0) {
				/* abort */
				return r;
			}
		}
	}
	for (i=0; i<DA_CUR (ovm->submaps); i++) {
		r = allocate_walkovarmap (DA_NTHITEM (ovm->submaps, i), map_func, item_func, maparg, itemarg);
		if (r < 0) {
			/* abort */
			return r;
		}
	}
	return r;
}
/*}}}*/
/*{{{  int allocate_walkvarmap (tnode_t *t, int memsp, int (*map_func)(void *, int, int, int, int, void *), int (*item_func)(void *, tnode_t *, int *, int *, void *), void *maparg, void *itemarg)*/
/*
 *	this performs a walk over the given node's varmap ("alloc:varmap" chook), in
 *	the specified memory-space (0=workspace, 1=vectorspace, 2=mobilespace), calling
 *	the given function with each map/sub-map or entry.
 *
 *	returns 0 on success, non-zero on failure
 */
int allocate_walkvarmap (tnode_t *t, int memsp, int (*map_func)(void *, int, int, int, int, void *), int (*item_func)(void *, tnode_t *, int *, int *, void *), void *maparg, void *itemarg)
{
	alloc_varmap_t *avmap = (alloc_varmap_t *)tnode_getchook (t, tnode_lookupornewchook ("alloc:varmap"));
	int r;

	if (!avmap) {
		nocc_warning ("allocate_walkvarmap(): called with map-less tree\n");
		return -1;
	}
	switch (memsp) {
	case 0:
		/* walk workspace */
		if (!avmap->wsmap) {
			nocc_warning ("allocate_walkvarmap(): no workspace map\n");
			return -1;
		}
		r = allocate_walkovarmap (avmap->wsmap, map_func, item_func, maparg, itemarg);
		break;
	case 1:
		/* walk vectorspace */
		if (!avmap->vsmap) {
			nocc_warning ("allocate_walkvarmap(): no vectorspace map\n");
			return -1;
		}
		r = allocate_walkovarmap (avmap->vsmap, map_func, item_func, maparg, itemarg);
		break;
	case 2:
		/* walk vectorspace */
		if (!avmap->msmap) {
			nocc_warning ("allocate_walkvarmap(): no mobilespace map\n");
			return -1;
		}
		r = allocate_walkovarmap (avmap->msmap, map_func, item_func, maparg, itemarg);
		break;
	default:
		nocc_warning ("allocate_walkvarmap(): not a map [%d]\n", memsp);
		return -1;
	}
	return r;
}
/*}}}*/


/*{{{  int allocate_init (void)*/
/*
 *	initialises the allocator
 *	returns 0 on success, non-zero on failure
 */
int allocate_init (void)
{
	return 0;
}
/*}}}*/
/*{{{  int allocate_shutdown (void)*/
/*
 *	shuts-down the allocator
 *	returns 0 on success, non-zero on failure
 */
int allocate_shutdown (void)
{
	return 0;
}
/*}}}*/


