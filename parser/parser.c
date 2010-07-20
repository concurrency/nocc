/*
 *	parser.c -- nocc parser framework
 *	Copyright (C) 2004-2008 Fred Barnes <frmb@kent.ac.uk>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/*{{{  includes*/
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
#include "extn.h"
#include "names.h"
#include "fcnlib.h"

/*}}}*/
/*{{{  private types*/
typedef struct TAG_reducer {
	char *name;
	void (*reduce)(dfastate_t *, parsepriv_t *, void *);
	void *rarg;
} reducer_t;

typedef struct TAG_ngrule {
	char *name;
	void *grule;
} ngrule_t;

typedef struct TAG_tagmarker {
	char *name;
	ntdef_t *tag;
} tagmarker_t;

/*}}}*/
/*{{{  private data*/

STATICSTRINGHASH (reducer_t *, reducers, 6);
STATICDYNARRAY (reducer_t *, areducers);

static ntdef_t *tag_LIST;
static ntdef_t *tag_TESTTRUE;
static ntdef_t *tag_TESTFALSE;

STATICSTRINGHASH (ngrule_t *, ngrules, 6);
STATICDYNARRAY (ngrule_t *, angrules);

STATICSTRINGHASH (tagmarker_t *, tagmarkers, 4);
STATICDYNARRAY (tagmarker_t *, atagmarkers);

/*}}}*/
/*{{{  forward declarations*/
static void parser_freetagmarker (tagmarker_t *tm);

/*}}}*/


/*{{{  int parser_init (void)*/
/*
 *	initialises the parser
 *	returns 0 on success, non-zero on failure
 */
int parser_init (void)
{
	tndef_t *tnd;
	int i;

	stringhash_sinit (reducers);
	dynarray_init (areducers);

	tag_LIST = tnode_lookupnodetag ("list");

	fcnlib_addfcn ("parser_inlistreduce", (void *)parser_inlistreduce, 0, 3);
	parser_register_reduce ("Rinlist", parser_inlistreduce, NULL);
	fcnlib_addfcn ("parser_inlistfixup", (void *)parser_inlistfixup, 1, 1);

	stringhash_sinit (ngrules);
	dynarray_init (angrules);

	stringhash_sinit (tagmarkers);
	dynarray_init (atagmarkers);

	parser_register_grule ("parser:nullreduce", parser_decode_grule ("N+R-"));
	parser_register_grule ("parser:listresult", parser_decode_grule ("R+N-"));
	parser_register_grule ("parser:rewindtokens", parser_decode_grule ("T*"));
	parser_register_grule ("parser:eattoken", parser_decode_grule ("T+@t"));

	/* create TESTTRUE/TESTFALSE nodes */
	i = -1;
	tnd = tnode_newnodetype ("parser:testnode", &i, 0, 0, 0, TNF_NONE);

	i = -1;
	tag_TESTTRUE = tnode_newnodetag ("TESTTRUE", &i, tnd, NTF_NONE);
	i = -1;
	tag_TESTFALSE = tnode_newnodetag ("TESTFALSE", &i, tnd, NTF_NONE);

	parser_register_grule ("parser:testtrue", parser_decode_grule ("C[TESTTRUE]0R-"));
	parser_register_grule ("parser:testfalse", parser_decode_grule ("C[TESTFALSE]0R-"));

	return 0;
}
/*}}}*/
/*{{{  int parser_shutdown (void)*/
/*
 *	shuts-down the parser
 *	returns 0 on success, non-zero on failure
 */
int parser_shutdown (void)
{
	int i;

	/* FIXME: should free up reducers, really .. */

	/* free up tag-markers */
	for (i=0; i<DA_CUR (atagmarkers); i++) {
		parser_freetagmarker (DA_NTHITEM (atagmarkers, i));
	}

	dynarray_trash (atagmarkers);
	stringhash_trash (tagmarkers);

	return 0;
}
/*}}}*/
/*{{{  void parser_error (lexfile_t *lf, const char *fmt, ...)*/
/*
 *	called by parser-bits when an error is encountered
 */
void parser_error (lexfile_t *lf, const char *fmt, ...)
{
	va_list ap;
	int n;
	char *warnbuf = (char *)smalloc (512);

	va_start (ap, fmt);
	n = sprintf (warnbuf, "%s:%d (error) ", lf->fnptr, lf->lineno);
	vsnprintf (warnbuf + n, 512 - n, fmt, ap);
	va_end (ap);

	lf->errcount++;
	nocc_message (warnbuf);
	sfree (warnbuf);

	return;
}
/*}}}*/
/*{{{  void parser_warning (lexfile_t *lf, const char *fmt, ...)*/
/*
 *	called by parser-bits for warnings
 */
void parser_warning (lexfile_t *lf, const char *fmt, ...)
{
	va_list ap;
	int n;
	char *warnbuf = (char *)smalloc (512);

	va_start (ap, fmt);
	n = sprintf (warnbuf, "%s:%d (warning) ", lf->fnptr, lf->lineno);
	vsnprintf (warnbuf + n, 512 - n, fmt, ap);
	va_end (ap);

	lf->warncount++;
	nocc_message (warnbuf);
	sfree (warnbuf);

	return;
}
/*}}}*/
/*{{{  void parser_error_line (lexfile_t *lf, const int lineno, const char *fmt, ...)*/
/*
 *	called by parser-bits when an error is encountered
 *	(line-number handled explicitly)
 */
void parser_error_line (lexfile_t *lf, const int lineno, const char *fmt, ...)
{
	va_list ap;
	int n;
	char *warnbuf = (char *)smalloc (512);

	va_start (ap, fmt);
	n = sprintf (warnbuf, "%s:%d (error) ", lf->fnptr, lineno);
	vsnprintf (warnbuf + n, 512 - n, fmt, ap);
	va_end (ap);

	lf->errcount++;
	nocc_message (warnbuf);
	sfree (warnbuf);

	return;
}
/*}}}*/
/*{{{  void parser_warning_line (lexfile_t *lf, const int lineno, const char *fmt, ...)*/
/*
 *	called by parser-bits for warnings
 */
void parser_warning_line (lexfile_t *lf, const int lineno, const char *fmt, ...)
{
	va_list ap;
	int n;
	char *warnbuf = (char *)smalloc (512);

	va_start (ap, fmt);
	n = sprintf (warnbuf, "%s:%d (warning) ", lf->fnptr, lineno);
	vsnprintf (warnbuf + n, 512 - n, fmt, ap);
	va_end (ap);

	lf->warncount++;
	nocc_message (warnbuf);
	sfree (warnbuf);

	return;
}
/*}}}*/
/*{{{  int parser_markerror (lexfile_t *lf)*/
/*
 *	'marks' the lexfile error-count (returns it)
 */
int parser_markerror (lexfile_t *lf)
{
	return lf->errcount;
}
/*}}}*/
/*{{{  int parser_checkerror (lexfile_t *lf, const int mark)*/
/*
 *	compares error counts to see if something errored while parsing
 *	returns 0 if no error, non-zero otherwise
 */
int parser_checkerror (lexfile_t *lf, const int mark)
{
	return (lf->errcount - mark);
}
/*}}}*/

/*{{{  static tagmarker_t *parser_newtagmarker (void)*/
/*
 *	creates a new tagmarker_t structure.
 */
static tagmarker_t *parser_newtagmarker (void)
{
	tagmarker_t *tm = (tagmarker_t *)smalloc (sizeof (tagmarker_t));

	tm->name = NULL;
	tm->tag = NULL;

	return tm;
}
/*}}}*/
/*{{{  static void parser_freetagmarker (tagmarker_t *tm)*/
/*
 *	frees a tagmarker_t structure.
 */
static void parser_freetagmarker (tagmarker_t *tm)
{
	if (!tm) {
		nocc_internal ("parser_freetagmarker(): NULL tag-marker");
		return;
	}
	if (tm->name) {
		sfree (tm->name);
		tm->name = NULL;
	}
	sfree (tm);

	return;
}
/*}}}*/
/*{{{  static tagmarker_t *parser_findtagmarker (const char *name, const int create)*/
/*
 *	finds a particular tag-marker, either creates new or fails
 */
static tagmarker_t *parser_findtagmarker (const char *name, const int create)
{
	tagmarker_t *tm;

	tm = stringhash_lookup (tagmarkers, name);
	if (tm) {
		return tm;
	}
	if (!create) {
		return NULL;
	}

	tm = parser_newtagmarker ();
	tm->name = string_dup (name);

	stringhash_insert (tagmarkers, tm, tm->name);
	dynarray_add (atagmarkers, tm);

	return tm;
}
/*}}}*/

/*{{{  parsepriv_t *parser_newparsepriv (void)*/
/*
 *	creates a new parsepriv_t structure
 */
parsepriv_t *parser_newparsepriv (void)
{
	parsepriv_t *pp = (parsepriv_t *)smalloc (sizeof (parsepriv_t));

	pp->lhook = NULL;
	pp->lf = NULL;
	dynarray_init (pp->tokstack);

	return pp;
}
/*}}}*/
/*{{{  void parser_freeparsepriv (parsepriv_t *pp)*/
/*
 *	frees a parsepriv_t structure
 */
void parser_freeparsepriv (parsepriv_t *pp)
{
	if (!pp) {
		nocc_internal ("parser_freeparsepriv(): NULL state");
		return;
	}

	if (DA_CUR (pp->tokstack)) {
		int i;

		nocc_warning ("parser_freeparsepriv(): %d leftover tokens", DA_CUR (pp->tokstack));
		for (i=0; i<DA_CUR (pp->tokstack); i++) {
			lexer_freetoken (DA_NTHITEM (pp->tokstack, i));
		}
	}
	dynarray_trash (pp->tokstack);
	pp->lhook = NULL;
	sfree (pp);
	return;
}
/*}}}*/
/*{{{  token_t *parser_peektok (parsepriv_t *pp)*/
/*
 *	peeks at a token without removing it from the stack
 */
token_t *parser_peektok (parsepriv_t *pp)
{
	if (!pp) {
		nocc_internal ("parser_peektok(): NULL state");
		return NULL;
	}
	if (!DA_CUR (pp->tokstack)) {
		nocc_error ("parser_peektok(): token-stack empty");
		return NULL;
	}
	return DA_NTHITEM (pp->tokstack, DA_CUR (pp->tokstack) - 1);
}
/*}}}*/
/*{{{  token_t *parser_gettok (parsepriv_t *pp)*/
/*
 *	removes a token from the token-stack
 */
token_t *parser_gettok (parsepriv_t *pp)
{
	token_t *tok;

	if (!pp) {
		nocc_internal ("parser_peektok(): NULL state");
		return NULL;
	}
	if (!DA_CUR (pp->tokstack)) {
		nocc_error ("parser_peektok(): token-stack empty");
		return NULL;
	}
	tok = DA_NTHITEM (pp->tokstack, DA_CUR (pp->tokstack) - 1);
	dynarray_delitem (pp->tokstack, DA_CUR (pp->tokstack) - 1);

	return tok;
}
/*}}}*/
/*{{{  void parser_pushtok (parsepriv_t *pp, token_t *tok)*/
/*
 *	pushes a token onto the token-stack
 */
void parser_pushtok (parsepriv_t *pp, token_t *tok)
{
	if (!pp) {
		nocc_internal ("parser_pushtok(): NULL state");
		return;
	}
	dynarray_add (pp->tokstack, tok);
	return;
}
/*}}}*/


/*{{{  tnode_t *parser_parse (lexfile_t *lf)*/
/*
 *	parses a file, producing some top-level element (language-specifics define what is parsed at the top-level)
 */
tnode_t *parser_parse (lexfile_t *lf)
{
	tnode_t *tree;
	parsepriv_t *pp;

	if (!lf->parser) {
		return NULL;
	}
	pp = parser_newparsepriv ();
	pp->lf = lf;
	lf->ppriv = (void *)pp;
	lf->parser->init (lf);

	tree = lf->parser->parse (lf);

	parser_freeparsepriv (pp);
	lf->parser->shutdown (lf);

#if 0 && defined(DEBUG)
fprintf (stderr, "parser_parse(): parse finished!.  Got tree:\n");
tnode_dumptree (tree, stderr);
#endif
	if (lf->errcount) {
		if (tree) {
			tnode_free (tree);
		}
		tree = NULL;
	}
	return tree;
}
/*}}}*/
/*{{{  tnode_t *parser_descparse (lexfile_t *lf)*/
/*
 *	parses a set of descriptors, producing some tree of declarations (language specific)
 *	the lexfile_t is a bit synthetic (really a buffer)
 */
tnode_t *parser_descparse (lexfile_t *lf)
{
	tnode_t *tree;
	parsepriv_t *pp;

	if (!lf->parser) {
		return NULL;
	}
	pp = parser_newparsepriv ();
	pp->lf = lf;
	lf->ppriv = (void *)pp;
	lf->parser->init (lf);

	tree = lf->parser->descparse (lf);

	parser_freeparsepriv (pp);
	lf->parser->shutdown (lf);

	if (lf->errcount) {
		if (tree) {
			tnode_free (tree);
		}
		tree = NULL;
	}
	return tree;
}
/*}}}*/
/*{{{  char *parser_langname (lexfile_t *lf)*/
/*
 *	returns the language name associated with a lexfile
 */
char *parser_langname (lexfile_t *lf)
{
	if (!lf->parser) {
		return NULL;
	}
	return lf->parser->langname;
}
/*}}}*/

/*{{{  int parser_gettesttags (ntdef_t **truep, ntdef_t **falsep)*/
/*
 *	gets hold of the two TRUE/FALSE test tags
 *	returns 0 on success, non-zero on failure
 */
int parser_gettesttags (ntdef_t **truep, ntdef_t **falsep)
{
	if (truep) {
		*truep = tag_TESTTRUE;
	}
	if (falsep) {
		*falsep = tag_TESTFALSE;
	}
	return 0;
}
/*}}}*/

/*{{{  tnode_t *parser_newlistnode (lexfile_t *lf)*/
/*
 *	creates and returns a new "list" node
 */
tnode_t *parser_newlistnode (lexfile_t *lf)
{
	tnode_t **array;
	int *cur, *max;
	tnode_t *node;
	
	/* create a blank array */
	array = (tnode_t **)smalloc (10 * sizeof (tnode_t *));
	cur = (int *)(array);
	max = (int *)(array + 1);

	*cur = 0;
	*max = 8;
	node = tnode_create (tag_LIST, lf, (void *)array);

	return node;
}
/*}}}*/
/*{{{  tnode_t *parser_buildlistnode (lexfile_t *lf, ...)*/
/*
 *	creates and returns a new "list" node, populated with items
 */
tnode_t *parser_buildlistnode (lexfile_t *lf, ...)
{
	va_list ap;
	tnode_t *node = parser_newlistnode (lf);
	tnode_t *item;

	va_start (ap, lf);
	for (item = va_arg (ap, tnode_t *); item; item = va_arg (ap, tnode_t *)) {
		parser_addtolist (node, item);
	}
	va_end (ap);

	return node;
}
/*}}}*/
/*{{{  tnode_t **parser_addtolist (tnode_t *list, tnode_t *item)*/
/*
 *	adds an item to a list-node, returns a pointer to it in the list
 */
tnode_t **parser_addtolist (tnode_t *list, tnode_t *item)
{
	tnode_t **array;
	int *cur, *max;

	array = (tnode_t **)tnode_nthhookof (list, 0);
	if (!array) {
		nocc_internal ("parser_addtolist(): null array in list!");
		return NULL;
	}
	cur = (int *)(array);
	max = (int *)(array + 1);
	if (*cur == *max) {
		/* need to make the array a bit larger */
		array = (tnode_t **)srealloc (array, (*max + 2) * sizeof (tnode_t *), (*max + 10) * sizeof (tnode_t *));

		cur = (int *)(array);
		max = (int *)(array + 1);

		*max = *max + 8;
		tnode_setnthhook (list, 0, array);
	}
	array[*cur + 2] = item;
	*cur = *cur + 1;

	return (array + (*cur + 1));
}
/*}}}*/
/*{{{  tnode_t **parser_addtolist_front (tnode_t *list, tnode_t *item)*/
/*
 *	adds an item to the front of a list, returns a pointer to it in the list
 */
tnode_t **parser_addtolist_front (tnode_t *list, tnode_t *item)
{
	tnode_t **array;
	int *cur, *max;
	int i;

	array = (tnode_t **)tnode_nthhookof (list, 0);
	if (!array) {
		nocc_internal ("parser_addtolist(): null array in list!");
		return NULL;
	}
	cur = (int *)(array);
	max = (int *)(array + 1);
	if (*cur == *max) {
		/* need to make the array a bit larger */
		array = (tnode_t **)srealloc (array, (*max + 2) * sizeof (tnode_t *), (*max + 10) * sizeof (tnode_t *));

		cur = (int *)(array);
		max = (int *)(array + 1);

		*max = *max + 8;
		tnode_setnthhook (list, 0, array);
	}
	for (i=*cur + 1; i>1; i--) {
		array[i+1] = array[i];
	}
	array[2] = item;
	*cur = *cur + 1;

	return (array + 2);
}
/*}}}*/
/*{{{  tnode_t *parser_delfromlist (tnode_t *list, int idx)*/
/*
 *	removes an item from a list-node by index, returns it
 */
tnode_t *parser_delfromlist (tnode_t *list, int idx)
{
	tnode_t **array;
	int *cur, *max;
	tnode_t *saveditem;

	if (list->tag != tag_LIST) {
		nocc_internal ("parser_delfromlist(): not list-node! [%s]", list->tag->name);
		return NULL;
	}
	array = (tnode_t **)tnode_nthhookof (list, 0);
	if (!array) {
		nocc_internal ("parser_delfromlist(): null array in list!");
		return NULL;
	}
	cur = (int *)array;
	max = (int *)(array + 1);

	if ((idx < 0) || (idx >= *cur)) {
		nocc_internal ("parser_delfromlist(): item %d not in list (%d/%d items)", idx, *cur, *max);
		return NULL;
	}

	saveditem = array[idx + 2];
	if (idx == (*cur - 1)) {
		/* removing the last item, easy :) */
	} else {
		int i;

		for (i=idx; i<(*cur-1); i++) {
			array[i+2] = array[i+3];
		}
	}
	array[*cur + 1] = NULL;		/* blank last item always */
	*cur = *cur - 1;

	return saveditem;
}
/*}}}*/
/*{{{  tnode_t *parser_getfromlist (tnode_t *list, int idx)*/
/*
 *	returns an item from a list-node by index
 */
tnode_t *parser_getfromlist (tnode_t *list, int idx)
{
	tnode_t **array;
	int *cur, *max;

	if (list->tag != tag_LIST) {
		nocc_internal ("parser_getfromlist(): not list-node! [%s]", list->tag->name);
		return NULL;
	}
	array = (tnode_t **)tnode_nthhookof (list, 0);
	if (!array) {
		nocc_internal ("parser_getfromlist(): null array in list!");
		return NULL;
	}
	cur = (int *)array;
	max = (int *)(array + 1);

	if ((idx < 0) || (idx >= *cur)) {
		nocc_internal ("parser_getfromlist(): item %d not in list (%d/%d items)", idx, *cur, *max);
		return NULL;
	}

	return array[idx + 2];
}
/*}}}*/
/*{{{  void parser_insertinlist (tnode_t *list, tnode_t *item, int idx)*/
/*
 *	inserts an item into a list at a particular index (rest are shuffled along)
 */
void parser_insertinlist (tnode_t *list, tnode_t *item, int idx)
{
	tnode_t **array;
	int *cur, *max;
	int i;

	array = (tnode_t **)tnode_nthhookof (list, 0);
	if (!array) {
		nocc_internal ("parser_insertinlist(): null array in list!");
		return;
	}
	cur = (int *)(array);
	max = (int *)(array + 1);
	if (*cur == *max) {
		/* need to make the array a bit larger */
		array = (tnode_t **)srealloc (array, (*max + 2) * sizeof (tnode_t *), (*max + 10) * sizeof (tnode_t *));

		cur = (int *)(array);
		max = (int *)(array + 1);

		*max = *max + 8;
		tnode_setnthhook (list, 0, array);
	}
	if (idx > *cur) {
		nocc_internal ("parser_insertinlist(): cannot insert at position %d (%d/%d item list)", idx, *cur, *max);
		return;
	}
	for (i=*cur + 1; i>=(idx + 2); i--) {
		array[i+1] = array[i];
	}
	array[idx + 2] = item;
	*cur = *cur + 1;
	return;
}
/*}}}*/
/*{{{  void parser_mergeinlist (tnode_t *list, tnode_t *sublist, int idx)*/
/*
 *	merges one list into another, trashes the sub-list
 */
void parser_mergeinlist (tnode_t *list, tnode_t *sublist, int idx)
{
	tnode_t **array;
	int *cur, *max;
	tnode_t **sarray;
	int *scur, *smax;
	int i;

	if (!parser_islistnode (list) || !parser_islistnode (sublist)) {
		nocc_internal ("parser_mergeinlist(): NULL list or sublist");
		return;
	}

	array = (tnode_t **)tnode_nthhookof (list, 0);
	if (!array) {
		nocc_internal ("parser_mergeinlist(): NULL array in list!");
		return;
	}
	cur = (int *)array;
	max = (int *)(array + 1);

	sarray = (tnode_t **)tnode_nthhookof (sublist, 0);
	if (!sarray) {
		nocc_internal ("parser_mergeinlist(): NULL array in sublist!");
		return;
	}
	scur = (int *)sarray;
	smax = (int *)(sarray + 1);

	if ((*cur + *scur) >= *max) {
		/* need to make the array a bit larger */
		array = (tnode_t **)srealloc (array, (*max + 2) * sizeof (tnode_t *), (*max + *scur + 2) * sizeof (tnode_t *));

		cur = (int *)array;
		max = (int *)(array + 1);

		*max = (*max + *scur);
		tnode_setnthhook (list, 0, array);
	}
	if (idx > *cur) {
		nocc_internal ("parser_mergeinlist(): cannot merge at position %d (%d/%d item list)", idx, *cur, *max);
		return;
	}

	/* move everything from (idx -> *cur) along by (*scur) */
	for (i = (*cur + 1); i >= (idx+2); i--) {
		array[i + *scur] = array[i];
	}

	/* copy the contents of sarray over at index 'idx' */
	for (i = 2; i < (*scur + 2); i++) {
		array[i+idx] = sarray[i];
	}
	*cur += *scur;

	parser_trashlist (sublist);
	return;
}
/*}}}*/
/*{{{  tnode_t *parser_rmfromlist (tnode_t *list, tnode_t *item)*/
/*
 *	removes an item from a list-node by reference, returns it
 */
tnode_t *parser_rmfromlist (tnode_t *list, tnode_t *item)
{
	tnode_t **array;
	int *cur;
	int i;

	if (list->tag != tag_LIST) {
		nocc_internal ("parser_rmfromlist(): not list-node! [%s]", list->tag->name);
		return NULL;
	}
	array = (tnode_t **)tnode_nthhookof (list, 0);
	if (!array) {
		nocc_internal ("parser_rmfromlist(): null array in list!");
		return NULL;
	}

	cur = (int *)array;

	for (i=0; i<*cur; i++) {
		if (array[i+2] == item) {
			/* remove this one */
			return parser_delfromlist (list, i);
		}
	}
	return NULL;
}
/*}}}*/
/*{{{  int parser_islistnode (tnode_t *node)*/
/*
 *	returns non-zero if the node given is a list-node
 */
int parser_islistnode (tnode_t *node)
{
	if (!node) {
		return 0;
	}
	return (node->tag == tag_LIST);
}
/*}}}*/
/*{{{  tnode_t **parser_getlistitems (tnode_t *list, int *nitems)*/
/*
 *	returns a pointer to the first item in the list, and sets "nitems" to indicate
 *	how many there are
 */
tnode_t **parser_getlistitems (tnode_t *list, int *nitems)
{
	tnode_t **array;

	array = (tnode_t **)tnode_nthhookof (list, 0);
	if (!array) {
		nocc_internal ("parser_addtolist(): null array in list!");
		return NULL;
	}
	*nitems = *(int *)(array);
	return (array + 2);
}
/*}}}*/
/*{{{  void parser_inlistreduce (dfastate_t *dfast, parsepriv_t *pp, void *rarg)*/
/*
 *	this reduces into a list
 */
void parser_inlistreduce (dfastate_t *dfast, parsepriv_t *pp, void *rarg)
{
	tnode_t *node;

	if (!parser_islistnode (dfast->local)) {
		/* make into a listnode, set it */
		node = dfast->local;

		dfast->local = parser_newlistnode (pp->lf);
		parser_addtolist (dfast->local, node);
	}
	dfast->ptr = parser_addtolist (dfast->local, NULL);

	return;
}
/*}}}*/
/*{{{  int parser_cleanuplist (tnode_t *list)*/
/*
 *	removes NULL items from a list
 *	returns the number of items removed on success, < 0 on failure
 */
int parser_cleanuplist (tnode_t *list)
{
	tnode_t **array;
	int *cur, *max;
	int removed = 0;
	int i, j;

	if (list->tag != tag_LIST) {
		nocc_internal ("parser_cleanuplist(): not list-node! [%s]", list->tag->name);
		return -1;
	}
	array = (tnode_t **)tnode_nthhookof (list, 0);
	if (!array) {
		nocc_internal ("parser_cleanuplist(): null array in list!");
		return -1;
	}
	cur = (int *)array;
	max = (int *)(array + 1);

	for (i=j=0; i<*cur; i++) {
		if (i > j) {
			array[j+2] = array[i+2];
		}
		if (array[i+2]) {
			/* keep this one */
			j++;
		}
	}
	removed = (i-j);
	for (; j<i; j++) {
		/* blank */
		array[j+2] = NULL;
	}
	*cur = *cur - removed;

	return removed;
}
/*}}}*/
/*{{{  void parser_inlistfixup (void **tos)*/
/*
 *	this is used to fixup after inlistreduce's, which leaves a NULL at the end of the list
 */
void parser_inlistfixup (void **tos)
{
	tnode_t *list = (tnode_t *)(*tos);

	if (parser_islistnode (list)) {
		tnode_t **array;
		int *cur, *max;

		array = (tnode_t **)tnode_nthhookof (list, 0);
		if (!array) {
			nocc_internal ("parser_inlistfixup(): null array in list!");
			return;
		}
		cur = (int *)(array);
		max = (int *)(array + 1);
		if ((*cur > 0) && (!array[*cur + 1])) {
			*cur = *cur - 1;
		} else {
			nocc_warning ("parser_inlistfixup(): nothing to fixup here..");
		}
	} else {
		nocc_warning ("parser_inlistfixup(): not a list!");
	}
	return;
}
/*}}}*/
/*{{{  void parser_sortlist (tnode_t *list, int (*cmpfcn)(tnode_t *, tnode_t *))*/
/*
 *	sorts the items in a listnode -- NOTE: only for certain things really!
 */
void parser_sortlist (tnode_t *list, int (*cmpfcn)(tnode_t *, tnode_t *))
{
	tnode_t **array;
	int *cur, *max;

	if (list->tag != tag_LIST) {
		nocc_internal ("parser_sortlist(): not list-node! [%s]", list->tag->name);
		return;
	}
	array = (tnode_t **)tnode_nthhookof (list, 0);
	if (!array) {
		nocc_internal ("parser_sortlist(): null array in list!");
		return;
	}
	cur = (int *)array;
	max = (int *)(array + 1);

#if 0
fprintf (stderr, "parser_sortlist(): array = 0x%8.8x, cur = 0x%8.8x (%d), max = 0x%8.8x (%d)\n", (unsigned int)array, (unsigned int)cur, *cur, (unsigned int)max, *max);
#endif
	if (*cur < 2) {
		return;			/* nothing to do */
	}
	da_qsort ((void **)(array + 2), 0, *cur - 1, (int (*)(void *, void *))cmpfcn);
	return;
}
/*}}}*/
/*{{{  void parser_trashlist (tnode_t *list)*/
/*
 *	trashes a listnode -- non-deep free effectively
 */
void parser_trashlist (tnode_t *list)
{
	tnode_t **array;
	int *cur, *max;
	int i;

	array = (tnode_t **)tnode_nthhookof (list, 0);
	if (!array) {
		nocc_internal ("parser_trashlist(): null array in list!");
		return;
	}
	cur = (int *)(array);
	max = (int *)(array + 1);

	for (i=0; i<*cur; i++) {
		array[i] = NULL;
	}
	tnode_free (list);
	return;
}
/*}}}*/
/*{{{  int parser_countlist (tnode_t *list)*/
/*
 *	counts the number of items in a listnode
 */
int parser_countlist (tnode_t *list)
{
	tnode_t **array;
	int *cur, *max;
	int i;

	array = (tnode_t **)tnode_nthhookof (list, 0);
	if (!array) {
		nocc_internal ("parser_countlist(): null array in list!");
		return -1;
	}
	cur = (int *)(array);

	return *cur;
}
/*}}}*/


/*{{{  int parser_register_reduce (const char *name, void (*reduce)(dfastate_t *, parsepriv_t *, void *), void *rarg)*/
/*
 *	registers a reduction function.  returns 0 on success, -1 on error
 */
int parser_register_reduce (const char *name, void (*reduce)(dfastate_t *, parsepriv_t *, void *), void *rarg)
{
	reducer_t *rd;

	rd = stringhash_lookup (reducers, name);
	if (rd) {
		nocc_warning ("parser_register_reduce(): reduction [%s] already registered!", name);
		return -1;
	}
	rd = (reducer_t *)smalloc (sizeof (reducer_t));
	rd->name = string_dup (name);
	rd->reduce = reduce;
	rd->rarg = rarg;
	stringhash_insert (reducers, rd, rd->name);
	dynarray_add (areducers, rd);

	return 0;
}
/*}}}*/
/*{{{  void (*parser_lookup_reduce (const char *name))(dfastate_t *, parsepriv_t *, void *)*/
/*
 *	looks up a reduction function
 */
void (*parser_lookup_reduce (const char *name))(dfastate_t *, parsepriv_t *, void *)
{
	reducer_t *rd = stringhash_lookup (reducers, name);

	return rd ? rd->reduce : NULL;
}
/*}}}*/
/*{{{  void *parser_lookup_rarg (const char *name)*/
/*
 *	looks up the argument for a reduction function
 */
void *parser_lookup_rarg (const char *name)
{
	reducer_t *rd = stringhash_lookup (reducers, name);

	return rd ? rd->rarg : NULL;
}
/*}}}*/


/*{{{  intcode operators*/
#define ICDE_END 0		/* end of intcode */
#define ICDE_NSPOP 1		/* pop from nodestack, push onto local stack */
#define ICDE_NSPUSH 2		/* pop from local stack, push onto nodestack */
#define ICDE_NULL 3		/* push NULL onto the local stack */
#define ICDE_TSPOP 4		/* take a token off the token-stack, push onto local stack */
#define ICDE_MODPTR 5		/* use following function-pointer to modify local stack [(void **) -> void] */
#define ICDE_MOD 6		/* use following function-pointer to modify local stack [(void *) -> (void *)] */
#define ICDE_RSET 7		/* pop from local stack, make result */
#define ICDE_RGET 8		/* get result, push onto local stack */
#define ICDE_COMBINE 9		/* make new node using following count nodes taken from local stack, followed by ntdef_t pointer */
#define ICDE_REV 10		/* swap two nodes at top of local stack */
#define ICDE_ROTLEFT 11		/* rotate the local stack left */
#define ICDE_ROTRIGHT 12	/* rotate the local stack right */
#define ICDE_USERMOD 13		/* use following function-pointer to modify local stack [(void **, int)] */
#define ICDE_SETORIGIN_N 14	/* set combine origin using node n-back on the local stack */
#define ICDE_SETORIGIN_T 15	/* set combine origin using a token n-back on the local stack */
#define ICDE_SETORIGIN_NS 16	/* set combine origin using node n-back on the nodestack */
#define ICDE_SETORIGIN_TS 17	/* set combine origin using a token n-back on the token-stack */
#define ICDE_CONSUME_N 18	/* consume node at the top of the local stack */
#define ICDE_CONSUME_T 19	/* consume token at the top of the local stack */
#define ICDE_TSREWIND 20	/* rewind the token-stack by pushing all tokens back into the lexer */
#define ICDE_TS1REWIND 21	/* push a single token from the token-stack back into the lexer */
#define ICDE_ALLREV 22		/* reverse the order of the local stack */
#define ICDE_MAKELIST 23	/* make a new list node using following count nodes taken from the local stack, result pushed back onto local stack */
#define ICDE_FOLDINTO 24	/* fold the node at the top of the local stack (popped) into the following count'th subnode of the next node on the local stack */
#define ICDE_EXTRACT 25		/* extract the following count'th subnode from the item at the top of the local stack and push onto the local stack */
#define ICDE_SETTAGMARK 26	/* set tag marker, followed by pointer to tagmarker_t structure and nt_def_t structure */
#define ICDE_COMBINETAG 27	/* make a new node using following count nodes taken from local stack, followed by tagmarker_t pointer */

/*}}}*/


/*{{{  void parser_generic_reduce (dfastate_t *dfast, parsepriv_t *pp, void *rarg)*/
/*
 *	this is the generic reduction function, supplied with a type of
 *	intcode (!) that does various things
 */
void parser_generic_reduce (dfastate_t *dfast, parsepriv_t *pp, void *rarg)
{
	unsigned int *arg = (unsigned int *)rarg;
	void *lnstk[16];
	int lncnt = 0;
	int ipos;
	lexfile_t *org_file = NULL;
	int org_line = 0;

	/* execute reduction machine! */
	for (ipos = 0; arg[ipos] != ICDE_END; ipos++) {
		switch (arg[ipos]) {
			/*{{{  NSPOP*/
		case ICDE_NSPOP:
			lnstk[lncnt++] = (void *)dfa_popnode (dfast);
			break;
			/*}}}*/
			/*{{{  NSPUSH*/
		case ICDE_NSPUSH:
			dfa_pushnode (dfast, (tnode_t *)lnstk[--lncnt]);
			break;
			/*}}}*/
			/*{{{  NULL*/
		case ICDE_NULL:
			lnstk[lncnt++] = NULL;
			break;
			/*}}}*/
			/*{{{  TSPOP*/
		case ICDE_TSPOP:
			lnstk[lncnt++] = (void *)parser_gettok (pp);
			break;
			/*}}}*/
			/*{{{  TSREWIND*/
		case ICDE_TSREWIND:
			while (DA_CUR (pp->tokstack) > 0) {
				token_t *xtok = parser_gettok (pp);

				lexer_pushback (xtok->origin, xtok);
			}
			break;
			/*}}}*/
			/*{{{  TS1REWIND*/
		case ICDE_TS1REWIND:
			if (!DA_CUR (pp->tokstack)) {
				nocc_error ("parser_generic_reduce(): empty token stack for TS1REWIND!");
			} else {
				token_t *xtok = parser_gettok (pp);

				lexer_pushback (xtok->origin, xtok);
			}
			break;
			/*}}}*/
			/*{{{  ALLREV*/
		case ICDE_ALLREV:
			{
				int i;

				for (i=0; i<(lncnt / 2); i++) {
					void *tmp = lnstk[lncnt - (i+1)];

					lnstk[lncnt - (i+1)] = lnstk[i];
					lnstk[i] = tmp;
				}
			}
			break;
			/*}}}*/
			/*{{{  MODPTR*/
		case ICDE_MODPTR:
			{
				void (*func)(void **) = (void (*)(void **))arg[++ipos];

				func (&(lnstk[lncnt - 1]));
			}
			break;
			/*}}}*/
			/*{{{  MOD*/
		case ICDE_MOD:
			{
				void *(*func)(void *) = (void *(*)(void *))arg[++ipos];

				lnstk[lncnt - 1] = func (lnstk[lncnt - 1]);
			}
			break;
			/*}}}*/
			/*{{{  RSET*/
		case ICDE_RSET:
			*(dfast->ptr) = (tnode_t *)lnstk[--lncnt];
			break;
			/*}}}*/
			/*{{{  RGET*/
		case ICDE_RGET:
			lnstk[lncnt++] = (void *)(*(dfast->ptr));
			*(dfast->ptr) = NULL;
			break;
			/*}}}*/
			/*{{{  SETTAGMARK*/
		case ICDE_SETTAGMARK:
			{
				tagmarker_t *tm = (tagmarker_t *)arg[++ipos];
				ntdef_t *tag = (ntdef_t *)arg[++ipos];

#if 1
fprintf (stderr, "settagmark: tagmark [%s], tag [%s]\n", tm->name, tag->name);
#endif
				tm->tag = tag;
			}
			break;
			/*}}}*/
			/*{{{  COMBINE, COMBINETAG*/
		case ICDE_COMBINE:
		case ICDE_COMBINETAG:
			{
				int usetag = (arg[ipos] == ICDE_COMBINETAG);
				int ccnt = (int)arg[++ipos];
				ntdef_t *tag = (ntdef_t *)arg[++ipos];
				tnode_t *rnode;

				/* fixup tag if using marker */
				if (usetag) {
					tagmarker_t *tm = (tagmarker_t *)tag;

					tag = tm->tag;
#if 1
fprintf (stderr, "combinetag: tag is [%s]\n", tag->name);
#endif
				}
				/*{{{  check number of arguments in use w.r.t. the node we're building*/
				if (ccnt != (tag->ndef->nsub + tag->ndef->nname + tag->ndef->nhooks)) {
					nocc_serious ("parser_generic_reduce(): building node [%s] with %d arguments, but expected %d",
							tag->name, ccnt, tag->ndef->nsub + tag->ndef->nname + tag->ndef->nhooks);
				}

				/*}}}*/
				/*{{{  arg-count cases -- bit ugly [sane way to do this without abusing var-args?] */
				switch (ccnt) {
				case 0:
					rnode = tnode_create (tag, NULL);
					break;
				case 1:
					rnode = tnode_create (tag, NULL, lnstk[lncnt - 1]);
					lncnt--;
					break;
				case 2:
					rnode = tnode_create (tag, NULL, lnstk[lncnt - 2], lnstk[lncnt - 1]);
					lncnt -= 2;
					break;
				case 3:
					rnode = tnode_create (tag, NULL, lnstk[lncnt - 3], lnstk[lncnt - 2], lnstk[lncnt - 1]);
					lncnt -= 3;
					break;
				case 4:
					rnode = tnode_create (tag, NULL, lnstk[lncnt - 4], lnstk[lncnt - 3], lnstk[lncnt - 2], lnstk[lncnt - 1]);
					lncnt -= 4;
					break;
				case 5:
					rnode = tnode_create (tag, NULL, lnstk[lncnt - 5], lnstk[lncnt - 4], lnstk[lncnt - 3],
							lnstk[lncnt - 2], lnstk[lncnt - 1]);
					lncnt -= 5;
					break;
				case 6:
					rnode = tnode_create (tag, NULL, lnstk[lncnt - 6], lnstk[lncnt - 5], lnstk[lncnt - 4],
							lnstk[lncnt - 3], lnstk[lncnt - 2], lnstk[lncnt - 1]);
					lncnt -= 6;
					break;
				case 7:
					rnode = tnode_create (tag, NULL, lnstk[lncnt - 7], lnstk[lncnt - 6], lnstk[lncnt - 5],
							lnstk[lncnt - 4], lnstk[lncnt - 3], lnstk[lncnt - 2], lnstk[lncnt - 1]);
					lncnt -= 7;
					break;
				case 8:
					rnode = tnode_create (tag, NULL, lnstk[lncnt - 8], lnstk[lncnt - 7], lnstk[lncnt - 6],
							lnstk[lncnt - 5], lnstk[lncnt - 4], lnstk[lncnt - 3], lnstk[lncnt - 2], lnstk[lncnt - 1]);
					lncnt -= 8;
					break;
				default:
					nocc_internal ("parser_generic_reduce(): unable to handle %d-node reduction", ccnt);
					rnode = NULL;
					break;
				}

				/*}}}*/
				if (org_file && org_line) {
					rnode->org_file = org_file;
					rnode->org_line = org_line;
					org_file = NULL;
					org_line = 0;
				}
				lnstk[lncnt++] = (void *)rnode;
			}
			break;
			/*}}}*/
			/*{{{  SETORIGIN_N*/
		case ICDE_SETORIGIN_N:
			{
				int offset = (int)arg[++ipos];
				tnode_t *onode = (tnode_t *)(lnstk[lncnt - (offset + 1)]);

				org_file = onode->org_file;
				org_line = onode->org_line;
			}
			break;
			/*}}}*/
			/*{{{  SETORIGIN_T*/
		case ICDE_SETORIGIN_T:
			{
				int offset = (int)arg[++ipos];
				token_t *otok = (token_t *)(lnstk[lncnt - (offset + 1)]);

				org_file = otok->origin;
				org_line = otok->lineno;
			}
			break;
			/*}}}*/
			/*{{{  SETORIGIN_NS*/
		case ICDE_SETORIGIN_NS:
			{
				int offset = (int)arg[++ipos];

				if (offset >= DA_CUR (dfast->nodestack)) {
					nocc_error ("parser_generic_reduce(): referenced origin %d not on node-stack", offset);
				} else {
					tnode_t *onode = DA_NTHITEM (dfast->nodestack, DA_CUR (dfast->nodestack) - (offset + 1));

					if (!onode) {
						nocc_error ("parser_generic_reduce(): referenced origin %d on node-stack is null", offset);
					} else {
						org_file = onode->org_file;
						org_line = onode->org_line;
					}
				}
			}
			break;
			/*}}}*/
			/*{{{  SETORIGIN_TS*/
		case ICDE_SETORIGIN_TS:
			{
				int offset = (int)arg[++ipos];

				if (offset >= DA_CUR (pp->tokstack)) {
					nocc_error ("parser_generic_reduce(): referenced origin %d not on token-stack", offset);
				} else {
					token_t *otok = DA_NTHITEM (pp->tokstack, DA_CUR (pp->tokstack) - (offset + 1));

					if (!otok) {
						nocc_error ("parser_generic_reduce(): referenced origin %d on token-stack is null", offset);
					} else {
						org_file = otok->origin;
						org_line = otok->lineno;
					}
				}
			}
			break;
			/*}}}*/
			/*{{{  REV*/
		case ICDE_REV:
			if (lncnt > 1) {
				void *tmp = lnstk[lncnt - 1];

				lnstk[lncnt - 1] = lnstk[lncnt - 2];
				lnstk[lncnt - 2] = tmp;
			}
			break;
			/*}}}*/
			/*{{{  USERMOD*/
		case ICDE_USERMOD:
			{
				void (*func)(void **, int) = (void (*)(void **, int))arg[++ipos];

				func (&(lnstk[0]), lncnt);
			}
			break;
			/*}}}*/
			/*{{{  ROTLEFT*/
		case ICDE_ROTLEFT:
			if (lncnt > 1) {
				void *saved = lnstk[0];
				int i;

				for (i=1; i<lncnt; i++) {
					lnstk[i-1] = lnstk[i];
				}
				lnstk[lncnt - 1] = saved;
			}
			break;
			/*}}}*/
			/*{{{  ROTRIGHT*/
		case ICDE_ROTRIGHT:
			if (lncnt > 1) {
				void *saved = lnstk[lncnt - 1];
				int i;

				for (i=lncnt - 1; i>0; i--) {
					lnstk[i] = lnstk[i-1];
				}
				lnstk[0] = saved;
			}
			break;
			/*}}}*/
			/*{{{  CONSUME_N*/
		case ICDE_CONSUME_N:
			{
				tnode_t *tmp = (tnode_t *)lnstk[--lncnt];

				tnode_free (tmp);
			}
			break;
			/*}}}*/
			/*{{{  CONSUME_T*/
		case ICDE_CONSUME_T:
			{
				token_t *tmp = (token_t *)lnstk[--lncnt];

				lexer_freetoken (tmp);
			}
			break;
			/*}}}*/
			/*{{{  ICDE_MAKELIST*/
		case ICDE_MAKELIST:
			{
				int listcnt = (int)arg[++ipos];
				tnode_t *list = parser_newlistnode (NULL);

				for (; listcnt > 0; listcnt--) {
					tnode_t *item = (tnode_t *)lnstk[--lncnt];

					parser_addtolist (list, item);
				}

				if (org_file && org_line) {
					list->org_file = org_file;
					list->org_line = org_line;
					org_file = NULL;
					org_line = 0;
				}
				lnstk[lncnt++] = (void *)list;
			}
			break;
			/*}}}*/
			/*{{{  ICDE_EXTRACT*/
		case ICDE_EXTRACT:
			{
				int sn = (int)arg[++ipos];
				tnode_t *node = (tnode_t *)lnstk[lncnt - 1];	/* TOS */
				tnode_t *item = NULL;

				if (sn >= node->tag->ndef->nsub) {
					nocc_serious ("parser_generic_reduce(): extracting subnode %d from [%s], but only got %d",
							sn, node->tag->name, node->tag->ndef->nsub);
				}
				item = tnode_nthsubof (node, sn);
				tnode_setnthsub (node, sn, NULL);

				lnstk[lncnt++] = (tnode_t *)item;
			}
			break;
			/*}}}*/
			/*{{{  ICDE_FOLDINTO*/
		case ICDE_FOLDINTO:
			{
				int sn = (int)arg[++ipos];
				tnode_t *item = (tnode_t *)lnstk[--lncnt];
				tnode_t *node = (tnode_t *)lnstk[lncnt - 1];	/* TOS */

				if (sn >= node->tag->ndef->nsub) {
					nocc_serious ("parser_generic_reduce(): folding [%s] into [%s] at subnode %d, but only got %d",
							item->tag->name, node->tag->name, sn, node->tag->ndef->nsub);
				}
				if (tnode_nthsubof (node, sn)) {
					tnode_t *xitem = tnode_nthsubof (node, sn);

					nocc_warning ("parser_generic_reduce(): freeing existing subnode [%s] in [%s] subnode %d",
							xitem->tag->name, node->tag->name, sn);
					tnode_free (xitem);
				}
				tnode_setnthsub (node, sn, item);
			}
			break;
			/*}}}*/
		}
	}
	return;
}
/*}}}*/
/*{{{  void *parser_decode_grule (const char *rule, ...)*/
/*
 *	processes a generic reduction rule
 */
void *parser_decode_grule (const char *rule, ...)
{
	va_list ap;
	char *xrule = (char *)rule;
	unsigned int *icode = NULL;
	int ilen = 0;
	int lsdepth = 0;
	void *userparams[16];		/* XXX: limit */
	int uplen = 0;
	int i;

	va_start (ap, rule);
	for (; *xrule != '\0'; xrule++) {
		switch (*xrule) {
			/*{{{  N -- nodestack operation*/
		case 'N':
			xrule++;
			switch (*xrule) {
			case '+':
				lsdepth++;
				ilen++;
				break;
			case '-':
				lsdepth--;
				ilen++;
				break;
			default:
				goto report_error_out;
			}
			break;
			/*}}}*/
			/*{{{  0 -- null push*/
		case '0':
			ilen++;
			lsdepth++;
			break;
			/*}}}*/
			/*{{{  V -- reverse/swap stack items*/
		case 'V':
			if (xrule[1] == '*') {
				/* reverse all items */
				xrule++;
			}	/* else reverse top two */
			if (!lsdepth) {
				nocc_warning ("parser_decode_grule(): strange swap nothing..");
			}
			ilen++;
			break;
			/*}}}*/
			/*{{{  T -- token-stack operation*/
		case 'T':
			xrule++;
			if (*xrule == '+') {
				ilen++;
				lsdepth++;
			} else if ((*xrule == '*') || (*xrule == '1')) {
				ilen++;
			} else {
				goto report_error_out;
			}
			break;
			/*}}}*/
			/*{{{  X -- user-supplied operations*/
		case 'X':
			if (xrule[1] == '[') {
				/* name of symbol provided */
				char *tname;
				void *sym;

				xrule+=2;
				for (tname = xrule; (*xrule != ']') && (*xrule != '\0'); xrule++);
				tname = string_ndup (tname, (int)(xrule - tname));

				sym = fcnlib_findfunction (tname);
				if (!sym) {
					sym = extn_findsymbol (tname);
					if (!sym) {
						nocc_error ("parser_decode_grule(): unknown function [%s]", tname);
						sfree (tname);
						return NULL;
					}
				}
				sfree (tname);
				if (xrule[1] == '*') {
					xrule++;
				}
				userparams[uplen++] = sym;
			} else {
				if (xrule[1] == '*') {
					xrule++;
					userparams[uplen++] = (void *)va_arg (ap, void (*)(void **));
				} else {
					userparams[uplen++] = (void *)va_arg (ap, void *(*)(void *));
				}
			}
			ilen += 2;
			break;
			/*}}}*/
			/*{{{  Y -- user-supplied whole-stack operation*/
		case 'Y':
			if (xrule[1] == '[') {
				/* name of symbol provided */
				char *tname;
				void *sym;

				xrule+=2;
				for (tname = xrule; (*xrule != ']') && (*xrule != '\0'); xrule++);
				tname = string_ndup (tname, (int)(xrule - tname));

				sym = fcnlib_findfunction (tname);
				if (!sym) {
					sym = extn_findsymbol (tname);
					if (!sym) {
						nocc_error ("parser_decode_grule(): unknown function [%s]", tname);
						sfree (tname);
						return NULL;
					}
				}
				sfree (tname);
				if (xrule[1] == '*') {
					xrule++;
				}
				userparams[uplen++] = sym;
			} else {
				userparams[uplen++] = (void *)va_arg (ap, void (*)(void **, int));
			}
			ilen += 2;
			break;
			/*}}}*/
			/*{{{  R -- result push or pull*/
		case 'R':
			xrule++;
			switch (*xrule) {
			case '+':
				lsdepth++;
				ilen++;
				break;
			case '-':
				lsdepth--;
				ilen++;
				break;
			default:
				goto report_error_out;
			}
			break;
			/*}}}*/
			/*{{{  L -- make list*/
		case 'L':
			xrule++;
			if ((*xrule < '0') || (*xrule > '9')) {
				goto report_error_out;
			} else {
				/* adjustment is minus "n", plus 1 */
				lsdepth -= (int)(*xrule - '0');
				if (lsdepth < 0) {
					/* pretty bad */
					nocc_error ("parser_decode_grule(): local stack underflow at char %d in \"%s\"", (int)(xrule - rule), rule);
					return NULL;
				}
				lsdepth++;
			}
			ilen += 2;
			break;
			/*}}}*/
			/*{{{  E -- extract*/
		case 'E':
			xrule++;
			if ((*xrule < '0') || (*xrule > '9')) {
				goto report_error_out;
			} else {
				/* adjustment is plus 1, but must have at least one thing */
				if (lsdepth < 1) {
					nocc_error ("parser_decode_grule(): local stack underflow at char %d in \"%s\"", (int)(xrule - rule), rule);
					return NULL;
				}
				lsdepth++;
			}
			ilen += 2;
			break;
			/*}}}*/
			/*{{{  F -- fold into*/
		case 'F':
			xrule++;
			if ((*xrule < '0') || (*xrule > '9')) {
				goto report_error_out;
			} else {
				/* adjustment is minus 1, but must have at least one thing left */
				lsdepth--;
				if (lsdepth < 1) {
					nocc_error ("parser_decode_grule(): local stack underflow at char %d in \"%s\"", (int)(xrule - rule), rule);
					return NULL;
				}
			}
			ilen += 2;
			break;
			/*}}}*/
			/*{{{  M -- set tag mark*/
		case 'M':
			{
				tagmarker_t *tm = NULL;
				ntdef_t *namedtag = NULL;

				xrule++;
				if (*xrule == '[') {
					char *tmname = NULL;
					char *tnname = NULL;

					xrule++;
					for (tmname = xrule; (*xrule != ']') && (*xrule != '\0'); xrule++);
					tmname = string_ndup (tmname, (int)(xrule - tmname));

					tm = parser_findtagmarker (tmname, 1);
					if (!tm) {
						nocc_error ("parser_decode_grule(): failed to find/create tag-marker [%s]", tmname);
						sfree (tmname);
						return NULL;
					}
					sfree (tmname);
					if (*xrule != ']') {
						goto report_error_out;
					}
					xrule++;

					if (*xrule != '[') {
						goto report_error_out;
					}
					xrule++;
					for (tnname = xrule; (*xrule != ']') && (*xrule != '\0'); xrule++);
					tnname = string_ndup (tnname, (int)(xrule - tnname));

					namedtag = tnode_lookupnodetag (tnname);
					if (!namedtag) {
						nocc_error ("parser_decode_grule(): unknown tag name [%s]", tnname);
						sfree (tnname);
						return NULL;
					}
					sfree (tnname);
					if (*xrule != ']') {
						goto report_error_out;
					}

				} else {
					nocc_error ("parser_decode_grule(): malformed set tag-marker (M)");
					return NULL;
				}

				userparams[uplen++] = (void *)tm;
				userparams[uplen++] = (void *)namedtag;
				ilen += 3;
			}
			break;
			/*}}}*/
			/*{{{  C -- condense into new token (combine)*/
		case 'C':
			xrule++;
			{
				ntdef_t *namedtag = NULL;
				tagmarker_t *tm = NULL;

				if (*xrule == '[') {
					/* named rule, not passed as an argument */
					char *tname;

					xrule++;
					for (tname = xrule; (*xrule != ']') && (*xrule != '\0'); xrule++);
					tname = string_ndup (tname, (int)(xrule - tname));

					namedtag = tnode_lookupnodetag (tname);
					if (!namedtag) {
						nocc_error ("parser_decode_grule(): unknown tag name [%s]", tname);
						sfree (tname);
						return NULL;
					}
					sfree (tname);
					if (*xrule == ']') {
						xrule++;
					}
				} else if (*xrule == '{') {
					/* named tag-marker */
					char *tname;

					xrule++;
					for (tname = xrule; (*xrule != '}') && (*xrule != '\0'); xrule++);
					tname = string_ndup (tname, (int)(xrule - tname));

					tm = parser_findtagmarker (tname, 1);

					if (!tm) {
						nocc_error ("parser_decode_grule(): unknown tag-marker [%s]", tname);
						sfree (tname);
						return NULL;
					}
					sfree (tname);
					if (*xrule == '}') {
						xrule++;
					}
				}

				if ((*xrule < '0') || (*xrule > '9')) {
					goto report_error_out;
				} else {
					/* adjustment is minus "n", plus 1 */
					lsdepth -= (int)(*xrule - '0');
					if (lsdepth < 0) {
						/* pretty bad.. */
						nocc_error ("parser_decode_grule(): local stack underflow at char %d in \"%s\"", (int)(xrule - rule), rule);
						return NULL;
					}
					lsdepth++;
				}

				if (tm) {
					userparams[uplen++] = (void *)tm;
				} else if (namedtag) {
					userparams[uplen++] = (void *)namedtag;
				} else {
					userparams[uplen++] = (void *)va_arg (ap, ntdef_t *);
				}
				ilen += 3;
			}
			break;
			/*}}}*/
			/*{{{  S -- set origin for combine*/
		case 'S':
			xrule++;
			switch (*xrule) {
			case 't':
			case 'n':
			case 'T':
			case 'N':
				break;
			default:
				goto report_error_out;
			}
			if ((xrule[1] < '0') || (xrule[1] > '9')) {
				xrule++;
				goto report_error_out;
			} else {
				int dval = (int)(xrule[1] - '0');

				switch (*xrule) {
				case 't':
				case 'n':
					if (dval >= lsdepth) {
						nocc_error ("parser_decode_grule(): combine source underflows local stack near char %d in \"%s\"", (int)(xrule - rule), rule);
						return NULL;
					}
					break;
				case 'T':
				case 'N':
					/* needs a "run-time" check */
					break;
				}
				xrule++;
			}
			ilen += 2;
			break;
			/*}}}*/
			/*{{{  > -- rotate stack right/up*/
		case '>':
			ilen++;
			break;
			/*}}}*/
			/*{{{  < -- rotate stack left/down*/
		case '<':
			ilen++;
			break;
			/*}}}*/
			/*{{{  @ -- consume something from the top of the local stack*/
		case '@':
			xrule++;
			switch (*xrule) {
			case 't':
			case 'n':
				ilen++;
				lsdepth--;
				break;
			default:
				goto report_error_out;
			}
			break;
			/*}}}*/
		default:
			goto report_error_out;
		}
		if (lsdepth < 0) {
			/* pretty bad.. */
			nocc_error ("parser_decode_grule(): local stack underflow at char %d in \"%s\"", (int)(xrule - rule), rule);
			return NULL;
		}
	}
	va_end (ap);

	/* check lsdepth */
	if (lsdepth) {
		nocc_error ("parser_decode_grule(): local stack imbalance %d for \"%s\"", lsdepth, rule);
		return NULL;
	}

	/* if still here, good.  decode proper */
	ilen++;			/* for _END */
	uplen = 0;
	icode = (unsigned int *)smalloc (ilen * sizeof (unsigned int));
	for (i=0, xrule=(char *)rule; (*xrule != '\0') && (i < ilen); xrule++, i++) {
		switch (*xrule) {
			/*{{{  N -- nodestack operation*/
		case 'N':
			xrule++;
			icode[i] = (*xrule == '+') ? ICDE_NSPOP : ICDE_NSPUSH;
			break;
			/*}}}*/
			/*{{{  0 -- null push*/
		case '0':
			icode[i] = ICDE_NULL;
			break;
			/*}}}*/
			/*{{{  V -- reverse/swap stack entries*/
		case 'V':
			if (xrule[1] == '*') {
				xrule++;
				icode[i] = ICDE_ALLREV;
			} else {
				icode[i] = ICDE_REV;
			}
			break;
			/*}}}*/
			/*{{{  T -- token-stack operation*/
		case 'T':
			xrule++;
			if (*xrule == '+') {
				icode[i] = ICDE_TSPOP;
			} else if (*xrule == '*') {
				icode[i] = ICDE_TSREWIND;
			} else if (*xrule == '1') {
				icode[i] = ICDE_TS1REWIND;
			}
			break;
			/*}}}*/
			/*{{{  X -- user-supplied operations*/
		case 'X':
			if (xrule[1] == '[') {
				/* name of a symbol provided */
				for (xrule+=2; (*xrule != ']') && (*xrule != '\0'); xrule++);
			}

			if (xrule[1] == '*') {
				xrule++;
				icode[i++] = ICDE_MODPTR;
			} else {
				icode[i++] = ICDE_MOD;
			}
			icode[i] = (unsigned int)(userparams[uplen++]);
			break;
			/*}}}*/
			/*{{{  Y -- user-supplied whole stack operation*/
		case 'Y':
			icode[i++] = ICDE_USERMOD;
			icode[i] = (unsigned int)(userparams[uplen++]);
			break;
			/*}}}*/
			/*{{{  R -- result push or pull*/
		case 'R':
			xrule++;
			icode[i] = (*xrule == '+') ? ICDE_RGET : ICDE_RSET;
			break;
			/*}}}*/
			/*{{{  L -- make list*/
		case 'L':
			xrule++;
			icode[i++] = ICDE_MAKELIST;
			icode[i] = (int)(*xrule - '0');
			break;
			/*}}}*/
			/*{{{  E -- extract from*/
		case 'E':
			xrule++;
			icode[i++] = ICDE_EXTRACT;
			icode[i] = (int)(*xrule - '0');
			break;
			/*}}}*/
			/*{{{  F -- fold into*/
		case 'F':
			xrule++;
			icode[i++] = ICDE_FOLDINTO;
			icode[i] = (int)(*xrule - '0');
			break;
			/*}}}*/
			/*{{{  M -- set tag marker*/
		case 'M':
			xrule++;
			if (*xrule == '[') {
				for (xrule++; (*xrule != ']') && (*xrule != '\0'); xrule++);
				xrule++;
			}
			if (*xrule == '[') {
				for (xrule++; (*xrule != ']') && (*xrule != '\0'); xrule++);
			}
			icode[i++] = ICDE_SETTAGMARK;
			icode[i++] = (unsigned int)(userparams[uplen++]);
			icode[i] = (unsigned int)(userparams[uplen++]);
			break;
			/*}}}*/
			/*{{{  C -- condense into new token*/
		case 'C':
			{
				int code = ICDE_COMBINE;

				xrule++;
				if (*xrule == '[') {
					for (xrule++; (*xrule != ']') && (*xrule != '\0'); xrule++);
					xrule++;
				} else if (*xrule == '{') {
					for (xrule++; (*xrule != '}') && (*xrule != '\0'); xrule++);
					xrule++;

					code = ICDE_COMBINETAG;
				}
					
				icode[i++] = code;
				icode[i++] = (int)(*xrule - '0');
				icode[i] = (unsigned int)(userparams[uplen++]);
			}
			break;
			/*}}}*/
			/*{{{  S -- set origin for combine*/
		case 'S':
			xrule++;
			switch (*xrule) {
			case 't':
				icode[i++] = ICDE_SETORIGIN_T;
				break;
			case 'n':
				icode[i++] = ICDE_SETORIGIN_N;
				break;
			case 'T':
				icode[i++] = ICDE_SETORIGIN_TS;
				break;
			case 'N':
				icode[i++] = ICDE_SETORIGIN_NS;
				break;
			}
			xrule++;
			icode[i] = (unsigned int)(*xrule - '0');
			break;
			/*}}}*/
			/*{{{  > -- rotate stack right/up*/
		case '>':
			icode[i] = ICDE_ROTRIGHT;
			break;
			/*}}}*/
			/*{{{  < -- rotate stack left/down*/
		case '<':
			icode[i] = ICDE_ROTLEFT;
			break;
			/*}}}*/
			/*{{{  @ -- consume something from the local stack*/
		case '@':
			xrule++;
			switch (*xrule) {
			case 't':
				icode[i] = ICDE_CONSUME_T;
				break;
			case 'n':
				icode[i] = ICDE_CONSUME_N;
				break;
			}
			break;
			/*}}}*/
		}
	}
	icode[i] = ICDE_END;

	return (void *)icode;
report_error_out:
	nocc_error ("parser_decode_grule(): error at char %d (%c) in \"%s\"", (int)(xrule - rule), (*xrule == '\0') ? '#' : *xrule, rule);
	return NULL;
}
/*}}}*/
/*{{{  void parser_free_grule (void *rarg)*/
/*
 *	frees a generic reduction rule
 *	(not expecting to be used that much, if at all)
 */
void parser_free_grule (void *rarg)
{
	if (rarg) {
		sfree (rarg);
	}
	return;
}
/*}}}*/
/*{{{  char *parser_nameof_reducer (void *reducefcn, void *reducearg)*/
/*
 *	tries to find the name associated with a reduction function
 *	(mainly for debugging)
 */
char *parser_nameof_reducer (void *reducefcn, void *reducearg)
{
	static char namebuf[128];
	int i;

	if (!reducefcn) {
		strcpy (namebuf, "(null)");
	} else if ((void *)parser_generic_reduce == reducefcn) {
		strcpy (namebuf, "(generic unknown)");

		for (i=0; i<DA_CUR (angrules); i++) {
			ngrule_t *gredex = DA_NTHITEM (angrules, i);

			if (gredex->grule == reducearg) {
				strcpy (namebuf, gredex->name);
				break;
			}
		}
	} else {
		strcpy (namebuf, "(unknown)");
		for (i=0; i<DA_CUR (areducers); i++) {
			reducer_t *redex = DA_NTHITEM (areducers, i);

			if ((void *)redex->reduce == reducefcn) {
				strcpy (namebuf, redex->name);
				break;
			}
		}
	}

	return (char *)&namebuf[0];
}
/*}}}*/

/*{{{  int parser_register_grule (const char *name, void *grule)*/
/*
 *	registers a new grule
 *	return 0 on success non-zero on failure
 */
int parser_register_grule (const char *name, void *grule)
{
	ngrule_t *ngr = (ngrule_t *)smalloc (sizeof (ngrule_t));

	ngr->name = string_dup (name);
	ngr->grule = grule;

	if (!grule) {
		nocc_internal ("parser_register_grule(): NULL rule");
		return -1;
	}
	stringhash_insert (ngrules, ngr, ngr->name);
	dynarray_add (angrules, ngr);

	return 0;
}
/*}}}*/
/*{{{  void *parser_lookup_grule (const char *name)*/
/*
 *	looks up a generic reduction rule by name
 *	returns rule on success, NULL on failure
 */
void *parser_lookup_grule (const char *name)
{
	ngrule_t *ngr = stringhash_lookup (ngrules, name);

	return ngr ? ngr->grule : NULL;
}
/*}}}*/
/*{{{  static int parser_cmp_grule (ngrule_t *one, ngrule_t *two)*/
/*
 *	compares two grule entries (for sorting)
 */
static int parser_cmp_grule (ngrule_t *one, ngrule_t *two)
{
	if (one == two) {
		return 0;
	}
	return strcmp (one->name, two->name);
}
/*}}}*/
/*{{{  void parser_dumpgrules (FILE *stream)*/
/*
 *	dumps named generic reductions (debugging/info)
 */
void parser_dumpgrules (FILE *stream)
{
	int i;

	fprintf (stream, "generic reduction rules:\n");
	dynarray_qsort (angrules, parser_cmp_grule);
	for (i=0; i<DA_CUR (angrules); i++) {
		ngrule_t *ngr = DA_NTHITEM (angrules, i);

		fprintf (stream, "  %-32s: 0x%8.8x\n", ngr->name, (unsigned int)ngr->grule);
	}
	return;
}
/*}}}*/

