/*
 *	usagecheck.c -- parallel usage checker for NOCC
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
#include "parsepriv.h"
#include "dfa.h"
#include "names.h"
#include "usagecheck.h"


/*}}}*/

/*{{{  private types*/
typedef struct TAG_uchk_chook_set {
	DYNARRAY (tnode_t *, items);
	DYNARRAY (uchk_mode_t, modes);
} uchk_chook_set_t;

typedef struct TAG_uchk_chook {
	DYNARRAY (uchk_chook_set_t *, parusage);
} uchk_chook_t;

typedef struct TAG_uchk_taghook {
	tnode_t *node;
	uchk_mode_t mode;
	int do_nested;
} uchk_taghook_t;

/*}}}*/
/*{{{  private data*/
static chook_t *uchk_chook = NULL;
static chook_t *uchk_taghook = NULL;


/*}}}*/


/*{{{  static void uchk_isetindent (FILE *stream, int indent)*/
/*
 *	sets indentation level
 */
static void uchk_isetindent (FILE *stream, int indent)
{
	int i;

	for (i=0; i<indent; i++) {
		fprintf (stream, "    ");
	}
	return;
}
/*}}}*/
/*{{{  static void uchk_chook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	dumps a usage-check compiler hook
 */
static void uchk_chook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	uchk_chook_t *uchook = (uchk_chook_t *)hook;

	uchk_isetindent (stream, indent);
	fprintf (stream, "<chook id=\"usagecheck\">\n");
	if (uchook && DA_CUR (uchook->parusage)) {
		int i;

		uchk_isetindent (stream, indent + 1);
		fprintf (stream, "<parusage nsets=\"%d\">\n", DA_CUR (uchook->parusage));
		for (i=0; i<DA_CUR (uchook->parusage); i++) {
			uchk_chook_set_t *parset = DA_NTHITEM (uchook->parusage, i);

			if (!parset) {
				uchk_isetindent (stream, indent + 2);
				fprintf (stream, "<nullset />\n");
			} else {
				int j;

				uchk_isetindent (stream, indent + 2);
				fprintf (stream, "<parset nitems=\"%d\">\n", DA_CUR (parset->items));

				for (j=0; j<DA_CUR (parset->items); j++) {
					uchk_isetindent (stream, indent + 3);
					fprintf (stream, "<mode mode=\"0x%8.8x\" />\n", (unsigned int)DA_NTHITEM (parset->modes, j));

					tnode_dumptree (DA_NTHITEM (parset->items, j), indent + 3, stream);
				}

				uchk_isetindent (stream, indent + 2);
				fprintf (stream, "</parset>\n");
			}
		}
		uchk_isetindent (stream, indent + 1);
		fprintf (stream, "</parusage>\n");
	}
	uchk_isetindent (stream, indent);
	fprintf (stream, "</chook>\n");

	return;
}
/*}}}*/
/*{{{  static void uchk_taghook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	dumps a usage-check-tag compiler hook
 */
static void uchk_taghook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	uchk_taghook_t *thook = (uchk_taghook_t *)hook;

	uchk_isetindent (stream, indent);
	if (!thook) {
		fprintf (stream, "<chook id=\"uchk:tagged\" value=\"(null)\" />\n");
	} else {
		uchk_mode_t mode = thook->mode;
		char buf[256];
		int x = 0;

		if (mode & USAGE_READ) {
			x += sprintf (buf + x, "read ");
		}
		if (mode & USAGE_WRITE) {
			x += sprintf (buf + x, "write ");
		}
		if (mode & USAGE_INPUT) {
			x += sprintf (buf + x, "input ");
		}
		if (mode & USAGE_XINPUT) {
			x += sprintf (buf + x, "xinput ");
		}
		if (mode & USAGE_OUTPUT) {
			x += sprintf (buf + x, "output ");
		}
		if (x > 0) {
			buf[x-1] = '\0';
		}
		fprintf (stream, "<chook id=\"uchk:tagged\" node=\"0x%8.8x\" mode=\"%s\" />\n", (unsigned int)thook->node, buf);
	}

	return;
}
/*}}}*/


/*{{{  static int uchk_prewalk_tree (tnode_t *node, void *data)*/
/*
 *	called to do usage-checks on tree nodes
 *	returns 0 to stop walk, 1 to continue
 */
static int uchk_prewalk_tree (tnode_t *node, void *data)
{
	uchk_state_t *ucstate = (uchk_state_t *)data;
	uchk_taghook_t *thook;
	int result = 1;

	if (!node) {
		nocc_internal ("uchk_prewalk_tree(): NULL tree!");
		return 0;
	}
	if (!ucstate) {
		nocc_internal ("uchk_prewalk_tree(): NULL state!");
		return 0;
	}
#if 0
fprintf (stderr, "uchk_prewalk_tree(): [%s]\n", node->tag->name);
#endif

	thook = tnode_getchook (node, uchk_taghook);
	if (thook) {
		/* gets applied before any implicit usage-check */
		if (usagecheck_addname (node, ucstate, thook->mode)) {
			return 0;
		}
		result = thook->do_nested;
	}

	if (node->tag->ndef->lops && node->tag->ndef->lops->do_usagecheck) {
		result = node->tag->ndef->lops->do_usagecheck (node, ucstate);
	}

	return result;
}
/*}}}*/
/*{{{  static int uchk_prewalk_cleantree (tnode_t *node, void *data)*/
/*
 *	removes usage-check tag-hooks from the parse tree
 *	returns 0 to stop walk, 1 to continue
 */
static int uchk_prewalk_cleantree (tnode_t *node, void *data)
{
	uchk_taghook_t *thook = (uchk_taghook_t *)tnode_getchook (node, uchk_taghook);

	if (thook) {
		sfree (thook);
		tnode_setchook (node, uchk_taghook, NULL);
	}
	return 1;
}
/*}}}*/


/*{{{  int usagecheck_init (void)*/
/*
 *	initialises parallel usage checker
 *	returns 0 on success, non-zero on error
 */
int usagecheck_init (void)
{
	if (!uchk_chook) {
		uchk_chook = tnode_newchook ("usagecheck");
		uchk_chook->chook_dumptree = uchk_chook_dumptree;

		uchk_taghook = tnode_newchook ("uchk:tagged");
		uchk_taghook->chook_dumptree = uchk_taghook_dumptree;
	}
	return 0;
}
/*}}}*/
/*{{{  int usagecheck_shutdown (void)*/
/*
 *	shuts-down parallel usage checker
 *	returns 0 on success, non-zero on error
 */
int usagecheck_shutdown (void)
{
	return 0;
}
/*}}}*/


/*{{{  int usagecheck_addname (tnode_t *node, uchk_state_t *ucstate, uchk_mode_t mode)*/
/*
 *	adds a name to parallel usage with the given mode
 *	returns 0 on success, non-zero on failure
 */
int usagecheck_addname (tnode_t *node, uchk_state_t *ucstate, uchk_mode_t mode)
{
	uchk_chook_t *uchook;
	uchk_chook_set_t *ucset;

	if ((ucstate->ucptr < 0) || (ucstate->ucptr >= DA_CUR (ucstate->ucstack)) || (ucstate->ucptr >= DA_CUR (ucstate->setptrs))) {
		nocc_internal ("usagecheck_addname(): [%s]: ucstate->ucptr=%d, DA_CUR(ucstack)=%d, DA_CUR(setptrs)=%d", node->tag->name, ucstate->ucptr, DA_CUR (ucstate->ucstack), DA_CUR (ucstate->setptrs));
		return -1;
	}
	uchook = DA_NTHITEM (ucstate->ucstack, ucstate->ucptr);
	ucset = (uchk_chook_set_t *)DA_NTHITEM (ucstate->setptrs, ucstate->ucptr);

	if (!uchook || !ucset) {
		nocc_internal ("usagecheck_addname(): [%s]: uchook=0x%8.8x, ucset=0x%8.8x", node->tag->name, (unsigned int)uchook, (unsigned int)ucset);
		return -1;
	}

	dynarray_add (ucset->items, node);
	dynarray_add (ucset->modes, mode);

	return 0;
}
/*}}}*/


/*{{{  int usagecheck_begin_branches (tnode_t *node, uchk_state_t *ucstate)*/
/*
 *	starts a new usage-checking node
 *	returns 0 on success, non-zero on failure
 */
int usagecheck_begin_branches (tnode_t *node, uchk_state_t *ucstate)
{
	int idx = ucstate->ucptr + 1;
	uchk_chook_t *uchook = (uchk_chook_t *)tnode_getchook (node, uchk_chook);
	
	if (!uchook) {
		uchook = (uchk_chook_t *)smalloc (sizeof (uchk_chook_t));
		dynarray_init (uchook->parusage);

		tnode_setchook (node, uchk_chook, (void *)uchook);
	}

	dynarray_setsize (ucstate->ucstack, idx + 1);
	dynarray_setsize (ucstate->setptrs, idx + 1);
	DA_SETNTHITEM (ucstate->ucstack, idx, (void *)uchook);
	DA_SETNTHITEM (ucstate->setptrs, idx, NULL);
	ucstate->ucptr++;

	return 0;
}
/*}}}*/
/*{{{  int usagecheck_end_branches (tnode_t *node, uchk_state_t *ucstate)*/
/*
 *	ends a usage-checking node
 *	returns 0 on success, non-zero on failure
 */
int usagecheck_end_branches (tnode_t *node, uchk_state_t *ucstate)
{
	int idx = ucstate->ucptr;
	uchk_chook_t *uchook = (uchk_chook_t *)tnode_getchook (node, uchk_chook);

	if (!uchook) {
		nocc_internal ("usagecheck_end_branches(): node has no usagecheck hook");
		return -1;
	}
	if ((idx < 0) || (idx >= DA_CUR (ucstate->ucstack)) || (idx >= DA_CUR (ucstate->setptrs))) {
		nocc_internal ("usagecheck_end_branches(): idx=%d, DA_CUR(ucstack)=%d, DA_CUR(setptrs)=%d", idx, DA_CUR (ucstate->ucstack), DA_CUR (ucstate->setptrs));
		return -1;
	}
	if (DA_NTHITEM (ucstate->ucstack, idx) != uchook) {
		nocc_internal ("usagecheck_end_branches(): hook stack mismatch, expected 0x%8.8x actually got 0x%8.8x", (unsigned int)uchook, (unsigned int)DA_NTHITEM (ucstate->ucstack, idx));
		return -1;
	}
	DA_SETNTHITEM (ucstate->ucstack, idx, NULL);			/* left attached in the tree */
	DA_SETNTHITEM (ucstate->setptrs, idx, NULL);
	dynarray_setsize (ucstate->ucstack, idx);
	dynarray_setsize (ucstate->setptrs, idx);
	ucstate->ucptr--;

	return 0;
}
/*}}}*/
/*{{{  int usagecheck_branch (tnode_t *tree, uchk_state_t *ucstate)*/
/*
 *	called to do parallel-usage checking on a sub-tree, sets up new set for items collected
 *	returns 0 on success, non-zero on failure
 */
int usagecheck_branch (tnode_t *tree, uchk_state_t *ucstate)
{
	uchk_chook_t *uchook;
	uchk_chook_set_t *ucset;
	
	if ((ucstate->ucptr < 0) || (ucstate->ucptr >= DA_CUR (ucstate->ucstack)) || (ucstate->ucptr >= DA_CUR (ucstate->setptrs))) {
		nocc_internal ("usagecheck_branch(): ucstate->ucptr=%d, DA_CUR(ucstack)=%d, DA_CUR(setptrs)=%d", ucstate->ucptr, DA_CUR (ucstate->ucstack), DA_CUR (ucstate->setptrs));
		return -1;
	}
	uchook = DA_NTHITEM (ucstate->ucstack, ucstate->ucptr);

	ucset = (uchk_chook_set_t *)smalloc (sizeof (uchk_chook_set_t));
	dynarray_init (ucset->items);
	dynarray_init (ucset->modes);

	dynarray_add (uchook->parusage, ucset);
	DA_SETNTHITEM (ucstate->setptrs, ucstate->ucptr, (void *)ucset);

	tnode_prewalktree (tree, uchk_prewalk_tree, (void *)ucstate);

	DA_SETNTHITEM (ucstate->setptrs, ucstate->ucptr, NULL);
	return 0;
}
/*}}}*/
/*{{{  void usagecheck_newbranch (uchk_state_t *ucstate)*/
/*
 *	called to start a new branch
 */
void usagecheck_newbranch (uchk_state_t *ucstate)
{
	uchk_chook_t *uchook;
	uchk_chook_set_t *ucset;

	if ((ucstate->ucptr < 0) || (ucstate->ucptr >= DA_CUR (ucstate->ucstack)) || (ucstate->ucptr >= DA_CUR (ucstate->setptrs))) {
		nocc_internal ("usagecheck_newbranch(): ucstate->ucptr=%d, DA_CUR(ucstack)=%d, DA_CUR(setptrs)=%d", ucstate->ucptr, DA_CUR (ucstate->ucstack), DA_CUR (ucstate->setptrs));
		return;
	}
	uchook = DA_NTHITEM (ucstate->ucstack, ucstate->ucptr);

	ucset = (uchk_chook_set_t *)smalloc (sizeof (uchk_chook_set_t));
	dynarray_init (ucset->items);
	dynarray_init (ucset->modes);

	dynarray_add (uchook->parusage, ucset);
	DA_SETNTHITEM (ucstate->setptrs, ucstate->ucptr, (void *)ucset);

	return;
}
/*}}}*/
/*{{{  void usagecheck_endbranch (uchk_state_t *ucstate)*/
/*
 *	called to end a branch
 */
void usagecheck_endbranch (uchk_state_t *ucstate)
{
	uchk_chook_t *uchook;
	uchk_chook_set_t *ucset;

	if ((ucstate->ucptr < 0) || (ucstate->ucptr >= DA_CUR (ucstate->ucstack)) || (ucstate->ucptr >= DA_CUR (ucstate->setptrs))) {
		nocc_internal ("usagecheck_endbranch(): ucstate->ucptr=%d, DA_CUR(ucstack)=%d, DA_CUR(setptrs)=%d", ucstate->ucptr, DA_CUR (ucstate->ucstack), DA_CUR (ucstate->setptrs));
		return;
	}
	uchook = DA_NTHITEM (ucstate->ucstack, ucstate->ucptr);
	ucset = (uchk_chook_set_t *)DA_NTHITEM (ucstate->setptrs, ucstate->ucptr);

	if (!uchook || !ucset) {
		nocc_internal ("usagecheck_endbranch(): uchook=0x%8.8x, ucset=0x%8.8x", (unsigned int)uchook, (unsigned int)ucset);
		return;
	}

	DA_SETNTHITEM (ucstate->setptrs, ucstate->ucptr, NULL);
	return;
}
/*}}}*/


/*{{{  int usagecheck_subtree (tnode_t *tree, uchk_state_t *ucstate)*/
/*
 *	called to do parallel-usage checking on a sub-tree (plain)
 *	returns 0 on success, non-zero on failure
 */
int usagecheck_subtree (tnode_t *tree, uchk_state_t *ucstate)
{
	tnode_prewalktree (tree, uchk_prewalk_tree, (void *)ucstate);

	return 0;
}
/*}}}*/
/*{{{  int usagecheck_tree (tnode_t *tree, langparser_t *lang)*/
/*
 *	does parallel usage checking on the given tree
 *	returns 0 on success, non-zero on failure
 */
int usagecheck_tree (tnode_t *tree, langparser_t *lang)
{
	uchk_state_t *ucstate = (uchk_state_t *)smalloc (sizeof (uchk_state_t));

	dynarray_init (ucstate->ucstack);
	dynarray_init (ucstate->setptrs);
	ucstate->ucptr = -1;

	tnode_prewalktree (tree, uchk_prewalk_tree, (void *)ucstate);

	dynarray_trash (ucstate->ucstack);
	dynarray_trash (ucstate->setptrs);
	sfree (ucstate);

	/* clean up markers */
	tnode_prewalktree (tree, uchk_prewalk_cleantree, NULL);

	return 0;
}
/*}}}*/


/*{{{  int usagecheck_marknode (tnode_t *node, uchk_mode_t mode, int do_nested)*/
/*
 *	marks a node for usage-checking later on
 *	returns 0 on success, non-zero on failure
 */
int usagecheck_marknode (tnode_t *node, uchk_mode_t mode, int do_nested)
{
	uchk_taghook_t *thook = (uchk_taghook_t *)tnode_getchook (node, uchk_taghook);

	if (thook) {
		if (thook->node != node) {
			nocc_internal ("usagecheck_marknode(): node/taghook mislinkage, node=0x%8.8x [%s], thook->node=0x%8.8x [%s]", (unsigned int)node, node->tag->name, (unsigned int)thook->node, thook->node->tag->name);
			return -1;
		}
		if (thook->do_nested != do_nested) {
			nocc_internal ("usagecheck_marknode(): node/taghook do_nested differ");
			return -1;
		}

		if ((thook->mode & mode) != mode) {
			nocc_warning ("usagecheck_marknode(): node [%s] already marked with 0x%8.8x(%d), merging in 0x%8.8x(%d)", node->tag->name, (unsigned int)thook->mode, thook->do_nested, (unsigned int)mode, do_nested);
		}

		thook->mode |= mode;
		thook->do_nested |= do_nested;
	} else {
		thook = (uchk_taghook_t *)smalloc (sizeof (uchk_taghook_t));

		thook->node = node;
		thook->mode = mode;
		thook->do_nested = do_nested;

		tnode_setchook (node, uchk_taghook, (void *)thook);
	}
	return 0;
}
/*}}}*/

