/*
 *	valueset.c -- code to handle mappings from values to somethings automatically (used when building CASE structures)
 *	Copyright (C) 2008-2016 Fred Barnes <frmb@kent.ac.uk>
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
#ifdef have_config_h
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/types.h>
#include <errno.h>

#include "nocc.h"
#include "support.h"
#include "version.h"
#include "fhandle.h"
#include "origin.h"
#include "symbols.h"
#include "keywords.h"
#include "opts.h"
#include "tnode.h"
#include "crypto.h"
#include "treeops.h"
#include "xml.h"
#include "lexer.h"
#include "parser.h"
#include "parsepriv.h"
#include "dfa.h"
#include "names.h"
#include "langops.h"
#include "metadata.h"
#include "valueset.h"

/*}}}*/
/*{{{  private data*/


/*}}}*/


/*{{{  int valueset_init (void)*/
/*
 *	initialises value-set handling
 *	returns 0 on success, non-zero on failure
 */
int valueset_init (void)
{
	return 0;
}
/*}}}*/
/*{{{  int valueset_shutdown (void)*/
/*
 *	shuts-down value-set handling
 *	returns 0 on success, non-zero on failure
 */
int valueset_shutdown (void)
{
	return 0;
}
/*}}}*/


/*{{{  static valueset_t *vset_newvset (void)*/
/*
 *	creates a new valueset_t structure
 */
static valueset_t *vset_newvset (void)
{
	valueset_t *vset = (valueset_t *)smalloc (sizeof (valueset_t));

	dynarray_init (vset->values);
	dynarray_init (vset->links);
	vset->v_min = 0;
	vset->v_max = -1;
	vset->v_base = 0;
	vset->v_limit = 0;
	vset->strat = STRAT_NONE;

	return vset;
}
/*}}}*/
/*{{{  static void vset_freevset (valueset_t *vset)*/
/*
 *	frees a valueset_t structure
 */
static void vset_freevset (valueset_t *vset)
{
	if (!vset) {
		nocc_serious ("vset_freevset(): NULL pointer!");
		return;
	}
	dynarray_trash (vset->values);
	dynarray_trash (vset->links);

	sfree (vset);
	return;
}
/*}}}*/
/*{{{  static void vset_isetindent (fhandle_t *stream, int indent)*/
/*
 *	sets indentation level
 */
static void vset_isetindent (fhandle_t *stream, int indent)
{
	int i;

	for (i=0; i<indent; i++) {
		fhandle_printf (stream, "    ");
	}
	return;
}
/*}}}*/
/*{{{  static void vset_dosort (valueset_t *vset, int first, int last)*/
/*
 *	does a quick-sort on a value-set
 */
static void vset_dosort (valueset_t *vset, int first, int last)
{
	int i, j;
	int64_t pivot;

#if 0
fprintf (stderr, "da_qsort(): array=%p, first=%d, last=%d\n", array, first, last);
#endif
	pivot = vset->values[(first + last) >> 1];
	i = first;
	j = last;
	while (i <= j) {
#if 0
fprintf (stderr, "da_qsort(): i=%d, j=%d, pivot=(%p), array[i]=(%p), array[j]=(%p)\n", i, j, pivot, array[i], array[j]);
#endif
		while (vset->values[i] < pivot) {
			i++;
		}
		while (vset->values[j] > pivot) {
			j--;
		}
		if (i <= j) {
			int tmp_val;
			tnode_t *tmp_link;

			tmp_val = vset->values[i];
			tmp_link = vset->links[i];
			vset->values[i] = vset->values[j];
			vset->links[i] = vset->links[j];
			vset->values[j] = tmp_val;
			vset->links[j] = tmp_link;
			i++;
			j--;
		}
	}
	if (j > first) {
		vset_dosort (vset, first, j);
	}
	if (i < last) {
		vset_dosort (vset, i, last);
	}
	return;
}
/*}}}*/



/*{{{  valueset_t *valueset_create (void)*/
/*
 *	creates a new valueset_t structure (empty)
 *	returns structure on success, NULL on failure
 */
valueset_t *valueset_create (void)
{
	valueset_t *vset = vset_newvset ();

	return vset;
}
/*}}}*/
/*{{{  void valueset_free (valueset_t *vset)*/
/*
 *	frees a valueset_t structure
 */
void valueset_free (valueset_t *vset)
{
	if (vset) {
		vset_freevset (vset);
	}
	return;
}
/*}}}*/
/*{{{  void valueset_dumptree (valueset_t *vset, int indent, fhandle_t *stream)*/
/*
 *	dumps a valueset_t structure (debugging)
 */
void valueset_dumptree (valueset_t *vset, int indent, fhandle_t *stream)
{
	vset_isetindent (stream, indent);
	if (vset) {
		int i;

		fhandle_printf (stream, "<valueset addr=\"%p\" nvalues=\"%d\" nlinks=\"%d\" min=\"%ld\" max=\"%ld\" base=\"%ld\" limit=\"%ld\" strategy=\"",
				vset, DA_CUR (vset->values), DA_CUR (vset->links), vset->v_min, vset->v_max,
				vset->v_base, vset->v_limit);
		switch (vset->strat) {
		case STRAT_NONE:
			fhandle_printf (stream, "none");
			break;
		case STRAT_CHAIN:
			fhandle_printf (stream, "chain");
			break;
		case STRAT_TABLE:
			fhandle_printf (stream, "table");
			break;
		case STRAT_HASH:
			fhandle_printf (stream, "hash");
			break;
		}
		fhandle_printf (stream, "\">\n");
		for (i=0; (i < DA_CUR (vset->values)) && (i < DA_CUR (vset->links)); i++) {
			int val = DA_NTHITEM (vset->values, i);
			tnode_t *link = DA_NTHITEM (vset->links, i);

			vset_isetindent (stream, indent + 1);
			fhandle_printf (FHAN_STDERR, "<valuesetitem value=\"%d\" link=\"%p\" linktag=\"%s\" />\n",
					val, link, link ? link->tag->name : "");
		}
		vset_isetindent (stream, indent);
		fhandle_printf (stream, "</valueset>\n");
	} else {
		fhandle_printf (stream, "<valueset value=\"\" />\n");
	}
	return;
}
/*}}}*/


/*{{{  int valueset_insert (valueset_t *vset, int64_t val, tnode_t *link)*/
/*
 *	inserts data into a value-set
 *	returns 0 on success, non-zero on failure (already here)
 */
int valueset_insert (valueset_t *vset, int64_t val, tnode_t *link)
{
	if (!vset) {
		nocc_serious ("valueset_insert(): NULL vset!");
		return -1;
	}

	dynarray_add (vset->values, val);
	dynarray_add (vset->links, link);

	if (vset->v_min > vset->v_max) {
		/* first time */
		vset->v_min = val;
		vset->v_max = val;
	} else if (val < vset->v_min) {
		vset->v_min = val;
	} else if (val > vset->v_max) {
		vset->v_max = val;
	}

	return 0;
}
/*}}}*/
/*{{{  int valueset_decide (valueset_t *vset)*/
/*
 *	decides how best to handle a data-set, and fixes structures to this end
 *	returns 0 on success, non-zero on failure
 */
int valueset_decide (valueset_t *vset)
{
	if (!vset) {
		nocc_serious ("valueset_decide(): NULL vset!");
		return -1;
	}

	vset->strat = STRAT_NONE;

	/* set base and limit (range) */
	vset->v_base = vset->v_min;
	vset->v_limit = (vset->v_max - vset->v_min) + 1;
	
	if (DA_CUR (vset->values) < 4) {
		/*{{{  if there are very few choices, use a chain */
		vset->strat = STRAT_CHAIN;
		/*}}}*/
	} else if (DA_CUR (vset->values) < (vset->v_limit * 3)) {
		/*{{{  if the spread of values is not more than 2x the size, use a straight jump-table*/
		vset->strat = STRAT_TABLE;
		/*}}}*/
	} else {
		/*{{{  anything overly-complex, probably want to hash*/
		/* FIXME: don't support this yet, do as TABLE */
		vset->strat = STRAT_TABLE;
		/*}}}*/
	}

	return 0;
}
/*}}}*/
/*{{{  int valueset_sort (valueset_t *vset)*/
/*
 *	sorts a data-set based on value
 *	returns 0 on success, non-zero on failure
 */
int valueset_sort (valueset_t *vset)
{
	if (!vset) {
		nocc_serious ("valueset_decide(): NULL vset!");
		return -1;
	}

	if (DA_CUR (vset->values) < 2) {
		return 0;
	}

	vset_dosort (vset, 0, DA_CUR (vset->values) - 1);

	return 0;
}
/*}}}*/
/*{{{  int valueset_insertblanks (valueset_t *vset, tnode_t *link)*/
/*
 *	inserts blanks into a value-set for jump-table handling
 *	returns 0 on success, non-zero on failure
 */
int valueset_insertblanks (valueset_t *vset, tnode_t *link)
{
	int i, v;

	if (!vset) {
		nocc_serious ("valueset_insertblanks(): NULL vset!");
		return -1;
	}

	for (i = 0, v = vset->v_min; v <= vset->v_max; v++, i++) {
		int this_val = DA_NTHITEM (vset->values, i);

		if (this_val > v) {
			/* insert something for 'v' here */
			dynarray_insert (vset->values, (int64_t)v, i);
			dynarray_insert (vset->links, link, i);
		}
	}

	return 0;
}
/*}}}*/


