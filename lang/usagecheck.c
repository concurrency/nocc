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
#include "langops.h"
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

static ntdef_t *uchk_tag_USAGE = NULL;

/*}}}*/


/*{{{  static uchk_chook_set_t *uchk_newuchkchookset (void)*/
/*
 *	creates a blank uchk_chook_set_t structure
 */
static uchk_chook_set_t *uchk_newuchkchookset (void)
{
	uchk_chook_set_t *ucset = (uchk_chook_set_t *)smalloc (sizeof (uchk_chook_set_t));

	dynarray_init (ucset->items);
	dynarray_init (ucset->modes);

	return ucset;
}
/*}}}*/
/*{{{  static uchk_chook_t *uchk_newuchkchook (void)*/
/*
 *	creates a blank uchk_chook_t structure
 */
static uchk_chook_t *uchk_newuchkchook (void)
{
	uchk_chook_t *uch = (uchk_chook_t *)smalloc (sizeof (uchk_chook_t));

	dynarray_init (uch->parusage);

	return uch;
}
/*}}}*/
/*{{{  static uchk_taghook_t *uchk_newuchktaghook (void)*/
/*
 *	creates a blank uchk_taghook_t structure
 */
static uchk_taghook_t *uchk_newuchktaghook (void)
{
	uchk_taghook_t *tagh = (uchk_taghook_t *)smalloc (sizeof (uchk_taghook_t));

	tagh->node = NULL;
	tagh->mode = USAGE_NONE;
	tagh->do_nested = 0;

	return tagh;
}
/*}}}*/
/*{{{  static void uchk_freeuchkchookset (uchk_chook_set_t *ucset)*/
/*
 *	frees a uchk_chook_set_t structure
 */
static void uchk_freeuchkchookset (uchk_chook_set_t *ucset)
{
	if (!ucset) {
		return;
	}
	dynarray_trash (ucset->items);
	dynarray_trash (ucset->modes);

	sfree (ucset);
	return;
}
/*}}}*/
/*{{{  static void uchk_freeuchkchook (uchk_chook_t *uch)*/
/*
 *	frees a uchk_chook_t structure (deep)
 */
static void uchk_freeuchkchook (uchk_chook_t *uch)
{
	int i;

	if (!uch) {
		return;
	}
	for (i=0; i<DA_CUR (uch->parusage); i++) {
		uchk_freeuchkchookset (DA_NTHITEM (uch->parusage, i));
	}
	dynarray_trash (uch->parusage);
	sfree (uch);
	return;
}
/*}}}*/
/*{{{  static void uchk_freeuchktaghook (uchk_taghook_t *tagh)*/
/*
 *	frees a uchk_taghook_t structure
 */
static void uchk_freeuchktaghook (uchk_taghook_t *tagh)
{
	if (!tagh) {
		return;
	}
	sfree (tagh);

	return;
}
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
					char buf[256];
					int x = 0;
					uchk_mode_t mode = DA_NTHITEM (parset->modes, j);

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
					} else {
						buf[0] = '\0';
					}

					uchk_isetindent (stream, indent + 3);
					fprintf (stream, "<mode mode=\"0x%8.8x\" flags=\"%s\" />\n", (unsigned int)DA_NTHITEM (parset->modes, j), buf);

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
/*{{{  static void uchk_chook_free (void *hook)*/
/*
 *	called to free a uchk_chook_t compiler hook
 */
static void uchk_chook_free (void *hook)
{
	uchk_chook_t *uch = (uchk_chook_t *)hook;

	uchk_freeuchkchook (uch);
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
/*{{{  static void uchk_taghook_free (void *hook)*/
/*
 *	called to free a uchk_taghook_t compiler hook
 */
static void uchk_taghook_free (void *hook)
{
	uchk_taghook_t *tagh = (uchk_taghook_t *)hook;

	uchk_freeuchktaghook (tagh);
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
		if (node->tag->ndef->lops && tnode_haslangop_i (node->tag->ndef->lops, (int)LOPS_DO_USAGECHECK)) {
			/*{{{  do usage-check on node directly*/
			uchk_mode_t savedmode = ucstate->defmode;

			ucstate->defmode = thook->mode;
			result = tnode_calllangop_i (node->tag->ndef->lops, (int)LOPS_DO_USAGECHECK, 2, node, ucstate);
			ucstate->defmode = savedmode;
			/*}}}*/
		} else {
			if (usagecheck_addname (node, ucstate, thook->mode)) {
				return 0;
			}
			result = thook->do_nested;
		}
	} else {
		if (node->tag->ndef->lops && tnode_haslangop_i (node->tag->ndef->lops, (int)LOPS_DO_USAGECHECK)) {
			result = tnode_calllangop_i (node->tag->ndef->lops, (int)LOPS_DO_USAGECHECK, 2, node, ucstate);
		}
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
	/* NOTE: don't clean up USAGE nodes, this gets done in the post-usage-check pass */
	if (node->tag != uchk_tag_USAGE) {
		uchk_taghook_t *thook = (uchk_taghook_t *)tnode_getchook (node, uchk_taghook);

		if (thook) {
			sfree (thook);
			tnode_clearchook (node, uchk_taghook);
		}
	}
	return 1;
}
/*}}}*/

/*{{{  static int puchk_modprewalk_tree (tnode_t **tptr, void *data)*/
/*
 *	called to do post-usage-checks on tree ndoes
 *	returns 0 to stop walk, 1 to continue
 */
static int puchk_modprewalk_tree (tnode_t **tptr, void *data)
{
	int result = 1;

	if (!tptr) {
		nocc_internal ("puchk_modprewalk_tree(): NULL pointer!");
		return 0;
	}
	if (!*tptr) {
		nocc_internal ("puchk_modprewalk_tree(): NULL tree!");
		return 0;
	}

	if ((*tptr)->tag->ndef->ops && tnode_hascompop_i ((*tptr)->tag->ndef->ops, (int)COPS_POSTUSAGECHECK)) {
		result = tnode_callcompop_i ((*tptr)->tag->ndef->ops, (int)COPS_POSTUSAGECHECK, 1, tptr);
	}

	return result;
}
/*}}}*/


/*{{{  static int occampi_postusagecheck_usagechecknode (compops_t *cops, tnode_t **nodep)*/
/*
 *	does post-usage-checks on a usage-check node (removes these)
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_postusagecheck_usagechecknode (compops_t *cops, tnode_t **nodep)
{
	if ((*nodep)->tag == uchk_tag_USAGE) {
		tnode_t *next = tnode_nthsubof (*nodep, 0);

		tnode_setnthsub (*nodep, 0, NULL);
		tnode_free (*nodep);
		*nodep = next;

		postusagecheck_subtree (nodep);
		return 0;
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
	tndef_t *tnd;
	compops_t *cops;
	langops_t *lops;
	int i;

	/*{{{  nocc:usagechecknode -- USAGE*/
	i = -1;
	tnd = tnode_newnodetype ("nocc:usagechecknode", &i, 1, 0, 0, TNF_TRANSPARENT);
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "postusagecheck", 1, COMPOPTYPE (occampi_postusagecheck_usagechecknode));
	tnd->ops = cops;
	lops = tnode_newlangops_passthrough ();			/* skip through these nodes for language-operations */
	tnd->lops = lops;

	i = -1;
	uchk_tag_USAGE = tnode_newnodetag ("USAGE", &i, tnd, NTF_NONE);

	/*}}}*/

	if (!uchk_chook) {
		uchk_chook = tnode_newchook ("usagecheck");
		uchk_chook->chook_dumptree = uchk_chook_dumptree;
		uchk_chook->chook_free = uchk_chook_free;

		uchk_taghook = tnode_newchook ("uchk:tagged");
		uchk_taghook->chook_dumptree = uchk_taghook_dumptree;
		uchk_taghook->chook_free = uchk_taghook_free;
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


/*{{{  void usagecheck_error (tnode_t *org, uchk_state_t *ucstate, const char *fmt, ...)*/
/*
 *	called by usage-check parts to report an error
 */
void usagecheck_error (tnode_t *org, uchk_state_t *ucstate, const char *fmt, ...)
{
	va_list ap;
	int n;
	char *warnbuf = (char *)smalloc (512);
	lexfile_t *orgfile;

	if (org) {
		orgfile = org->org_file;
	} else {
		orgfile = NULL;
	}

	va_start (ap, fmt);
	n = sprintf (warnbuf, "%s:%d (error): ", orgfile ? orgfile->fnptr : "(unknown)", org ? org->org_line : 0);
	vsnprintf (warnbuf + n, 512 - n, fmt, ap);
	va_end (ap);

	if (orgfile) {
		orgfile->errcount++;
	}
	ucstate->err++;

	nocc_message (warnbuf);
	sfree (warnbuf);

	return;
}
/*}}}*/
/*{{{  void usagecheck_warning (tnode_t *org, uchk_state_t *ucstate, const char *fmt, ...)*/
/*
 *	called by usage-check parts to report a warning
 */
void usagecheck_warning (tnode_t *org, uchk_state_t *ucstate, const char *fmt, ...)
{
	va_list ap;
	int n;
	char *warnbuf = (char *)smalloc (512);
	lexfile_t *orgfile;

	if (org) {
		orgfile = org->org_file;
	} else {
		orgfile = NULL;
	}

	va_start (ap, fmt);
	n = sprintf (warnbuf, "%s:%d (warning): ", orgfile ? orgfile->fnptr : "(unknown)", org ? org->org_line : 0);
	vsnprintf (warnbuf + n, 512 - n, fmt, ap);
	va_end (ap);

	if (orgfile) {
		orgfile->warncount++;
	}
	ucstate->warn++;

	nocc_message (warnbuf);
	sfree (warnbuf);

	return;
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
	int i;

	if (node->tag == uchk_tag_USAGE) {
		node = tnode_nthsubof (node, 0);
	}

#if 1
nocc_message ("usagecheck_addname(): allocating [%s,%s] with mode 0x%x", node->tag->name, node->tag->ndef->name, (int)mode);
#endif
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

	for (i=0; i<DA_CUR (ucset->items); i++) {
		tnode_t *chknode = DA_NTHITEM (ucset->items, i);

		if (chknode == node) {
			uchk_mode_t chkmode = DA_NTHITEM (ucset->modes, i);

			chkmode |= mode;
			DA_SETNTHITEM (ucset->modes, i, chkmode);
			break;		/* for() */
		}
	}

	if (i == DA_CUR (ucset->items)) {
		dynarray_add (ucset->items, node);
		dynarray_add (ucset->modes, mode);
	}

	return 0;
}
/*}}}*/
/*{{{  int usagecheck_mergeall (tnode_t *node, uchk_state_t *ucstate)*/
/*
 *	takes entries from usage-checking sets and adds to the current set
 *	returns 0 on success, 1 if non-added, <0 on failure
 */
int usagecheck_mergeall (tnode_t *node, uchk_state_t *ucstate)
{
	uchk_chook_t *srchook = (uchk_chook_t *)tnode_getchook (node, uchk_chook);
	uchk_chook_t *uchook;
	uchk_chook_set_t *ucset;
	int i;

	if (!srchook || (ucstate->ucptr < 0) || (ucstate->ucptr >= DA_CUR (ucstate->ucstack)) || (ucstate->ucptr >= DA_CUR (ucstate->setptrs))) {
		return 1;
	}
	uchook = DA_NTHITEM (ucstate->ucstack, ucstate->ucptr);
	ucset = (uchk_chook_set_t *)DA_NTHITEM (ucstate->setptrs, ucstate->ucptr);

	if (!uchook || !ucset) {
		return 1;
	}

	/* adding to "ucset" */
	for (i=0; i<DA_CUR (srchook->parusage); i++) {
		uchk_chook_set_t *srcset = DA_NTHITEM (srchook->parusage, i);
		int j;

		for (j=0; j<DA_CUR (srcset->items); j++) {
			tnode_t *srcnode = DA_NTHITEM (srcset->items, j);
			uchk_mode_t srcmode = DA_NTHITEM (srcset->modes, j);
			int k;

			for (k=0; k<DA_CUR (ucset->items); k++) {
				tnode_t *chknode = DA_NTHITEM (ucset->items, k);

				if (srcnode == chknode) {
					uchk_mode_t chkmode = DA_NTHITEM (ucset->modes, k);

					chkmode |= srcmode;
					DA_SETNTHITEM (ucset->modes, k, chkmode);
					break;		/* for(k) */
				}
			}
			if (k == DA_CUR (ucset->items)) {
				dynarray_add (ucset->items, srcnode);
				dynarray_add (ucset->modes, srcmode);
			}

		}
	}
	return 0;
}
/*}}}*/
/*{{{  static int usagecheck_sub_no_overlaps (tnode_t *node, uchk_state_t *ucstate, uchk_chook_set_t *set1, uchk_chook_set_t *set2)*/
/*
 *	checks for overlaps in two sets
 *	returns 0 on success, non-zero on failure (errors reported)
 */
static int usagecheck_sub_no_overlaps (tnode_t *node, uchk_state_t *ucstate, uchk_chook_set_t *set1, uchk_chook_set_t *set2)
{
	int i, j;

	for (i=0; i<DA_CUR (set1->items); i++) {
		tnode_t *item1 = DA_NTHITEM (set1->items, i);
		uchk_mode_t mode1 = DA_NTHITEM (set1->modes, i);

		for (j=0; j<DA_CUR (set2->items); j++) {
			tnode_t *item2 = DA_NTHITEM (set2->items, j);
			uchk_mode_t mode2 = DA_NTHITEM (set2->modes, j);

			if (item1 == item2) {
				/* same item */
				char *i1str = NULL;

				langops_getname (item1, &i1str);

				if ((mode1 & USAGE_INPUT) && (mode2 & USAGE_INPUT)) {
					usagecheck_error (node, ucstate, "parallel inputs on %s", i1str);
				} else if ((mode1 & USAGE_OUTPUT) && (mode2 & USAGE_OUTPUT)) {
					usagecheck_error (node, ucstate, "parallel outputs on %s", i1str);
				} else if ((mode1 & USAGE_WRITE) && (mode2 & USAGE_WRITE)) {
					usagecheck_error (node, ucstate, "%s is written to in parallel", i1str);
				} else if ((mode1 & USAGE_WRITE) && (mode2 & USAGE_READ)) {
					usagecheck_error (node, ucstate, "parallel read/write on %s", i1str);
				} else if ((mode1 & USAGE_READ) && (mode2 & USAGE_WRITE)) {
					usagecheck_error (node, ucstate, "parallel read/write on %s", i1str);
				}

				if (i1str) {
					sfree (i1str);
				}
			}
		}
	}
	return 0;
}
/*}}}*/
/*{{{  int usagecheck_no_overlaps (tnode_t *node, uchk_state_t *ucstate)*/
/*
 *	checks that there are no overlaps in usage-checking sets of the given node
 *	(e.g. when checking PAR nodes for safety)
 *	returns 0 on success, non-zero on failure (errors reported)
 */
int usagecheck_no_overlaps (tnode_t *node, uchk_state_t *ucstate)
{
	uchk_chook_t *srchook = (uchk_chook_t *)tnode_getchook (node, uchk_chook);
	int i;

	if (!srchook) {
		usagecheck_error (node, ucstate, "no sets here..");
		return -1;
	}

	for (i=0; i<DA_CUR (srchook->parusage); i++) {
		uchk_chook_set_t *srcset = DA_NTHITEM (srchook->parusage, i);
		int j;

		/* check these items with those in other sets */
		for (j=i+1; j<DA_CUR (srchook->parusage); j++) {
			uchk_chook_set_t *cmpset = DA_NTHITEM (srchook->parusage, j);
			
			if (usagecheck_sub_no_overlaps (node, ucstate, srcset, cmpset)) {
				/* errored */
				return 1;
			}
		}
	}

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
		uchook = uchk_newuchkchook ();
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

	ucset = uchk_newuchkchookset ();

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

	ucset = uchk_newuchkchookset ();

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
	int res = 0;

	dynarray_init (ucstate->ucstack);
	dynarray_init (ucstate->setptrs);
	ucstate->ucptr = -1;
	ucstate->err = 0;
	ucstate->warn = 0;
	ucstate->defmode = USAGE_NONE;

	tnode_prewalktree (tree, uchk_prewalk_tree, (void *)ucstate);

	res = ucstate->err;

	dynarray_trash (ucstate->ucstack);
	dynarray_trash (ucstate->setptrs);
	sfree (ucstate);

	/* clean up markers */
	tnode_prewalktree (tree, uchk_prewalk_cleantree, NULL);

	return res;
}
/*}}}*/

/*{{{  int postusagecheck_subtree (tnode_t **nodep)*/
/*
 *	does post-usage-checks on a sub-tree
 *	returns 0 on success, non-zero on failure
 */
int postusagecheck_subtree (tnode_t **nodep)
{
	tnode_modprewalktree (nodep, puchk_modprewalk_tree, NULL);
	return 0;
}
/*}}}*/
/*{{{  int postusagecheck_tree (tnode_t **tptr)*/
/*
 *	does post-usage-checks on the given tree
 *	returns 0 on success, non-zero on failure
 */
int postusagecheck_tree (tnode_t **tptr)
{
	tnode_modprewalktree (tptr, puchk_modprewalk_tree, NULL);
	return 0;
}
/*}}}*/


/*{{{  int usagecheck_marknode (tnode_t **nodep, uchk_mode_t mode, int do_nested)*/
/*
 *	marks a node for usage-checking later on
 *	returns 0 on success, non-zero on failure
 */
int usagecheck_marknode (tnode_t **nodep, uchk_mode_t mode, int do_nested)
{
#if 0
nocc_message ("usagecheck_marknode(): marking [%s,%s] with 0x%x", node->tag->name, node->tag->ndef->name, (int)mode);
#endif

	if ((*nodep)->tag == uchk_tag_USAGE) {
		uchk_taghook_t *thook = (uchk_taghook_t *)tnode_getchook (*nodep, uchk_taghook);
		tnode_t *subnode = tnode_nthsubof (*nodep, 0);

		if (thook->node != subnode) {
			nocc_internal ("usagecheck_marknode(): node/taghook mislinkage, subnode=0x%8.8x [%s], thook->node=0x%8.8x [%s]",
					(unsigned int)subnode, subnode->tag->name, (unsigned int)thook->node, thook->node->tag->name);
			return -1;
		}
		if (thook->do_nested != do_nested) {
			nocc_internal ("usagecheck_marknode(): node/taghook do_nested differ");
			return -1;
		}

		if ((thook->mode & mode) != mode) {
			nocc_warning ("usagecheck_marknode(): node [%s] already marked with 0x%8.8x(%d), merging in 0x%8.8x(%d)",
					subnode->tag->name, (unsigned int)thook->mode, thook->do_nested, (unsigned int)mode, do_nested);
		}

		thook->mode |= mode;
		thook->do_nested |= do_nested;
	} else {
		/* insert new USAGE node */
		tnode_t *newnode = tnode_createfrom (uchk_tag_USAGE, *nodep, *nodep);
		uchk_taghook_t *thook = uchk_newuchktaghook ();

		thook->node = *nodep;
		thook->mode = mode;
		thook->do_nested = do_nested;

		tnode_setchook (newnode, uchk_taghook, (void *)thook);

		*nodep = newnode;
	}
	return 0;
}
/*}}}*/

