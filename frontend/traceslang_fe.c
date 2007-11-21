/*
 *	traceslang_fe.c -- traces language front-end
 *	Copyright (C) 2007 Fred Barnes <frmb@kent.ac.uk>
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
#include <sys/types.h>
#include <unistd.h>

#include "nocc.h"
#include "support.h"
#include "version.h"
#include "origin.h"
#include "symbols.h"
#include "keywords.h"
#include "tnode.h"
#include "lexer.h"
#include "lexpriv.h"
#include "parser.h"
#include "parsepriv.h"
#include "treeops.h"
#include "opts.h"
#include "traceslang.h"
#include "traceslang_fe.h"
#include "tracescheck.h"


/*}}}*/
/*{{{  private data*/

STATICPOINTERHASH (ntdef_t *, validtracenametypes, 4);


/*}}}*/


/*{{{  int traceslang_register_frontend (void)*/
/*
 *	registers the traceslang lexer
 *	returns 0 on success, non-zero on failure
 */
int traceslang_register_frontend (void)
{
	traceslang_lexer.parser = &traceslang_parser;
	traceslang_parser.lexer = &traceslang_lexer;

	if (lexer_registerlang (&traceslang_lexer)) {
		return -1;
	}

	/*{{{  initialise other-language trace type stuff*/
	pointerhash_sinit (validtracenametypes);

	/*}}}*/

	return 0;
}
/*}}}*/
/*{{{  int traceslang_unregister_frontend (void)*/
/*
 *	unregisters the traceslang lexer
 *	returns 0 on success, non-zero on failure
 */
int traceslang_unregister_frontend (void)
{
	if (lexer_unregisterlang (&traceslang_lexer)) {
		return -1;
	}
	return 0;
}
/*}}}*/

/*{{{  tnode_t *traceslang_newevent (tnode_t *locn)*/
/*
 *	creates a new TRACESLANGEVENT (leaf) node
 *	returns node on success, NULL on failure
 */
tnode_t *traceslang_newevent (tnode_t *locn)
{
	tnode_t *enode = tnode_createfrom (traceslang.tag_EVENT, locn);

	return enode;
}
/*}}}*/
/*{{{  tnode_t *traceslang_newnparam (tnode_t *locn)*/
/*
 *	creates a new TRACESLANGNPARAM (name) node
 *	returns node on success, NULL on failure
 */
tnode_t *traceslang_newnparam (tnode_t *locn)
{
	tnode_t *nnode = tnode_createfrom (traceslang.tag_NPARAM, locn, NULL);

	return nnode;
}
/*}}}*/

/*{{{  int traceslang_isequal (tnode_t *n1, tnode_t *n2)*/
/*
 *	tests to see whether two trace nodes are equal
 *	returns zero if equal, non-zero otherwise
 */
int traceslang_isequal (tnode_t *n1, tnode_t *n2)
{
	if (n1 == n2) {
		return 0;
	}
	if (!n1 && n2) {
		return 1;
	} else if (n1 && !n2) {
		return -1;
	}
	if (n1->tag != n2->tag) {
		return -2;
	}

	/* same tag, determins likeness */
	if ((n1->tag == traceslang.tag_SEQ) || (n1->tag == traceslang.tag_PAR) || (n1->tag == traceslang.tag_DET) || (n1->tag == traceslang.tag_NDET)) {
		/* lists of things */
		int n1c, n2c, i;
		tnode_t **n1items = parser_getlistitems (tnode_nthsubof (n1, 0), &n1c);
		tnode_t **n2items = parser_getlistitems (tnode_nthsubof (n2, 0), &n2c);

		if (n1c != n2c) {
			return n1c - n2c;
		}
		for (i=0; i<n1c; i++) {
			int r = traceslang_isequal (n1items[i], n2items[i]);

			if (r) {
				return r;
			}
		}
		/* equivalent! */
		return 0;
	} else if ((n1->tag == traceslang.tag_INPUT) || (n1->tag == traceslang.tag_OUTPUT)) {
		return traceslang_isequal (tnode_nthsubof (n1, 0), tnode_nthsubof (n2, 0));
	} else if ((n1->tag == traceslang.tag_SKIP) || (n1->tag == traceslang.tag_STOP) || (n1->tag == traceslang.tag_CHAOS) || (n1->tag == traceslang.tag_DIV)) {
		/* simple processes are always equivalent */
		return 0;
	}
	return -42;
}
/*}}}*/

/*{{{  int traceslang_registertracetype (ntdef_t *tag)*/
/*
 *	registers a node-type from another language as a valid way of specifying traces
 *	returns 0 on success, non-zero on failure
 */
int traceslang_registertracetype (ntdef_t *tag)
{
	if (pointerhash_lookup (validtracenametypes, tag)) {
		nocc_warning ("traceslang_registertracetype(): tag (%s,%s) already registered!", tag->name, tag->ndef->name);
		return -1;
	}
	pointerhash_insert (validtracenametypes, tag, tag);
	return 0;
}
/*}}}*/
/*{{{  int traceslang_unregistertracetype (ntdef_t *tag)*/
/*
 *	unregisters a node-type from another language as a valid way of specifying traces
 *	returns 0 on success, non-zero on failure
 */
int traceslang_unregistertracetype (ntdef_t *tag)
{
	ntdef_t *xtag = pointerhash_lookup (validtracenametypes, tag);

	if (!xtag) {
		nocc_warning ("traceslang_unregistertracetype(): tag (%s,%s) is not reigstered!", tag->name, tag->ndef->name);
		return -1;
	} else if (xtag != tag) {
		nocc_warning ("traceslang_unregistertracetype(): tag (%s,%s) does not match registered tag (%s,%s)!",
				tag->name, tag->ndef->name, xtag->name, xtag->ndef->name);
		return -1;
	}
	pointerhash_remove (validtracenametypes, tag, tag);
	return 0;
}
/*}}}*/
/*{{{  int traceslang_isregisteredtracetype (ntdef_t *tag)*/
/*
 *	returns non-zero if the given tag is a regsitered trace type
 */
int traceslang_isregisteredtracetype (ntdef_t *tag)
{
	if (pointerhash_lookup (validtracenametypes, tag)) {
		return 1;
	}
	return 0;
}
/*}}}*/

/*{{{  static copycontrol_e trlang_structurecopyfcn (tnode_t *node)*/
/*
 *	used when duplicating the structure of a traceslang tree
 *	returns copy control status (for tnode_copyoraliastree)
 */
static copycontrol_e trlang_structurecopyfcn (tnode_t *node)
{
	if (tnode_ntflagsof (node) & NTF_TRACESLANGCOPYALIAS) {
		return COPY_ALIAS;
	}
	return (COPY_SUBS | COPY_HOOKS | COPY_CHOOKS);
}
/*}}}*/
/*{{{  tnode_t *traceslang_structurecopy (tnode_t *expr)*/
/*
 *	does a structure copy on a traceslang tree -- duplicates structural nodes, but leaves others intact
 *	returns new tree on success, NULL on failure
 */
tnode_t *traceslang_structurecopy (tnode_t *expr)
{
	return tnode_copyoraliastree (expr, trlang_structurecopyfcn);
}
/*}}}*/

/*{{{  int traceslang_noskiporloop (tnode_t **exprp)*/
/*
 *	this goes through a trace and removes trailing "Skip"s, or turns trailing ends into fixpoint loops
 *	returns 0 on success, non-zero on failure
 */
int traceslang_noskiporloop (tnode_t **exprp)
{
	traceslang_erefset_t *tailrefs = traceslang_lasteventsp (exprp);
	int i;

	for (i=0; i<DA_CUR (tailrefs->events); i++) {
		tnode_t **eventp = DA_NTHITEM (tailrefs->events, i);
		tnode_t *ev = *eventp;

		if (ev->tag == traceslang.tag_SKIP) {
			/* remove from traces */
			*eventp = NULL;
			tnode_free (ev);
		} else if (ev->tag == traceslang.tag_NPARAM) {
			/* trailing event, sequential with fixpoint instance */
			/* FIXME! */
		}
	}

	traceslang_freerefset (tailrefs);
	return 0;
}
/*}}}*/
/*{{{  static int traceslang_simplifyexpr_modpostwalk (tnode_t **nodep, void *arg)*/
/*
 *	called to simplify a traces-language expression (postwalk order)
 *	returns 0 to stop walk, 1 to continue
 */
static int traceslang_simplifyexpr_modpostwalk (tnode_t **nodep, void *arg)
{
	tnode_t *n = *nodep;

	if ((n->tag == traceslang.tag_SEQ) || (n->tag == traceslang.tag_PAR) || (n->tag == traceslang.tag_DET) || (n->tag == traceslang.tag_NDET)) {
		/*{{{  SEQ,PAR,DET,NDET -- collapse singles, merge nested*/
		int nitems, i;
		tnode_t *ilist = tnode_nthsubof (n, 0);
		tnode_t **items;
		
		parser_cleanuplist (ilist);
		items = parser_getlistitems (ilist, &nitems);

		if (nitems == 1) {
			/* single item! */
			tnode_t *tmp = items[0];

			items[0] = NULL;
			tnode_free (n);
			*nodep = tmp;
		} else {
			/* otherwise, see if we can merge nested trees */
			for (i=0; i<nitems; i++) {
				if (items[i]->tag == n->tag) {
					/* collapse this one */
					int nsubitems, j;
					tnode_t *sublist = tnode_nthsubof (items[i], 0);
					tnode_t **subitems = parser_getlistitems (sublist, &nsubitems);

					/* replacing item 'i' to start with */
					parser_delfromlist (ilist, i);

					for (j=0; j<nsubitems; j++, i++) {
						parser_insertinlist (ilist, subitems[j], i);
						subitems[j] = NULL;
					}
					i--;

					/* reset items incase it moved! */
					items = parser_getlistitems (ilist, &nitems);
				}
			}
		}
		/*}}}*/
	} else if ((n->tag == traceslang.tag_INPUT) || (n->tag == traceslang.tag_OUTPUT)) {
		/*{{{  INPUT,OUTPUT -- remove if NULL body*/
		tnode_t *item = tnode_nthsubof (n, 0);

		if (!item) {
			*nodep = NULL;
			tnode_free (n);
		}
		/*}}}*/
	}
	return 1;
}
/*}}}*/
/*{{{  tnode_t *traceslang_simplifyexpr (tnode_t *expr)*/
/*
 *	simplifies a traces-language expression (collapses single SEQs, etc.)
 *	returns new tree on success, NULL on failure
 */
tnode_t *traceslang_simplifyexpr (tnode_t *expr)
{
	tnode_t *result = expr;

	tnode_modpostwalktree (&result, traceslang_simplifyexpr_modpostwalk, NULL);

	return result;
}
/*}}}*/
/*{{{  tnode_t *traceslang_listtondet (tnode_t *expr)*/
/*
 *	turns a simple list of traces into a non-deterministic choice of them
 *	returns new tree on success, NULL on failure
 */
tnode_t *traceslang_listtondet (tnode_t *expr)
{
	int nitems, i;
	tnode_t **items;

	if (!expr) {
		nocc_serious ("traceslang_listtondet(): NULL expression!");
		return NULL;
	}
	if (!parser_islistnode (expr)) {
		nocc_serious ("traceslang_listtondet(): expression is not a list! (%s,%s)", expr->tag->name, expr->tag->ndef->name);
		return expr;
	}
	items = parser_getlistitems (expr, &nitems);
	if (nitems == 1) {
		/* collapse single item */
		tnode_t *tmp = items[0];

		items[0] = NULL;
		tnode_free (expr);
		return tmp;
	} else if (nitems > 1) {
		/* make a non-deterministic choice */
		return tnode_createfrom (traceslang.tag_NDET, expr, expr);
	}
	return expr;
}
/*}}}*/

/*{{{  traceslang_eset_t *traceslang_newset (void)*/
/*
 *	creates a new (blank) event set
 */
traceslang_eset_t *traceslang_newset (void)
{
	traceslang_eset_t *eset = (traceslang_eset_t *)smalloc (sizeof (traceslang_eset_t));

	dynarray_init (eset->events);
	return eset;
}
/*}}}*/
/*{{{  void traceslang_freeset (traceslang_eset_t *eset)*/
/*
 *	frees an event set
 */
void traceslang_freeset (traceslang_eset_t *eset)
{
	if (!eset) {
		nocc_internal ("traceslang_freeset(): NULL set!");
		return;
	}
	dynarray_trash (eset->events);
	sfree (eset);
	return;
}
/*}}}*/
/*{{{  void traceslang_dumpset (traceslang_eset_t *eset, int indent, FILE *stream)*/
/*
 *	dumps a set of events -- debugging
 */
void traceslang_dumpset (traceslang_eset_t *eset, int indent, FILE *stream)
{
	traceslang_isetindent (stream, indent);
	if (!eset) {
		fprintf (stream, "<traceslang:eset items=\"0\" value=\"null\" />\n");
	} else {
		int i;

		fprintf (stream, "<traceslang:eset items=\"%d\">\n", DA_CUR (eset->events));
		for (i=0; i<DA_CUR (eset->events); i++) {
			tnode_t *event = DA_NTHITEM (eset->events, i);

			tnode_dumptree (event, indent + 1, stream);
		}
		traceslang_isetindent (stream, indent);
		fprintf (stream, "</traceslang:eset>\n");
	}
}
/*}}}*/

/*{{{  traceslang_erefset_t *traceslang_newrefset (void)*/
/*
 *	creates a new traceslang_erefset_t structure
 */
traceslang_erefset_t *traceslang_newrefset (void)
{
	traceslang_erefset_t *eset = (traceslang_erefset_t *)smalloc (sizeof (traceslang_erefset_t));

	dynarray_init (eset->events);
	return eset;
}
/*}}}*/
/*{{{  void traceslang_freerefset (traceslang_erefset_t *eset)*/
/*
 *	frees a traceslang_erefset_t structure
 */
void traceslang_freerefset (traceslang_erefset_t *eset)
{
	if (!eset) {
		nocc_internal ("traceslang_freerefset(): NULL set!");
		return;
	}
	dynarray_trash (eset->events);
	sfree (eset);
	return;
}
/*}}}*/
/*{{{  void traceslang_dumprefset (traceslang_erefset_t *eset, int indent, FILE *stream)*/
/*
 *	dumps a traceslang_erefset_t structure (debugging)
 */
void traceslang_dumprefset (traceslang_erefset_t *eset, int indent, FILE *stream)
{
	traceslang_isetindent (stream, indent);
	if (!eset) {
		fprintf (stream, "<traceslang:erefset items=\"0\" value=\"null\" />\n");
	} else {
		int i;

		fprintf (stream, "<traceslang:erefset items=\"%d\">\n", DA_CUR (eset->events));
		for (i=0; i<DA_CUR (eset->events); i++) {
			tnode_t **eventp = DA_NTHITEM (eset->events, i);

			tnode_dumptree (*eventp, indent + 1, stream);
		}
		traceslang_isetindent (stream, indent);
		fprintf (stream, "</traceslang:erefset>\n");
	}
}
/*}}}*/

/*{{{  static int traceslang_firstevents_prewalk (tnode_t *node, void *arg)*/
/*
 *	called to extract the set of leading traces from a traces specification (prewalk order)
 *	returns 0 to stop walk, 1 to continue
 */
static int traceslang_firstevents_prewalk (tnode_t *node, void *arg)
{
	traceslang_eset_t *eset = (traceslang_eset_t *)arg;

	if (parser_islistnode (node)) {
		/*{{{  list node -- step through*/
		return 1;
		/*}}}*/
	} else if (node->tag == traceslang.tag_SEQ) {
		/*{{{  SEQ -- only the first can happen*/
		tnode_t *body = tnode_nthsubof (node, 0);
		int nitems, i;
		tnode_t **items = parser_getlistitems (body, &nitems);
		int count = DA_CUR (eset->events);

		for (i=0; (i<nitems) && (DA_CUR (eset->events) == count); i++) {
			/* walk this item */
			tnode_prewalktree (items[i], traceslang_firstevents_prewalk, (void *)eset);
		}

		/*}}}*/
	} else if ((node->tag == traceslang.tag_PAR) || (node->tag == traceslang.tag_NDET) || (node->tag == traceslang.tag_DET)) {
		/*{{{  PAR,NDET,DET -- first event from all branches*/
		return 1;
		/*}}}*/
	} else if ((node->tag == traceslang.tag_INPUT) || (node->tag == traceslang.tag_OUTPUT)) {
		/*{{{  INPUT,OUTPUT -- first event from subnode*/
		return 1;
		/*}}}*/
	} else if (node->tag == traceslang.tag_NPARAM) {
		/*{{{  NPARAM -- an event!*/
		dynarray_add (eset->events, node);
		/*}}}*/
	} else if ((node->tag == traceslang.tag_SKIP) || (node->tag == traceslang.tag_STOP) || (node->tag == traceslang.tag_CHAOS) || (node->tag == traceslang.tag_DIV)) {
		/*{{{  SKIP,STOP,CHAOS,DIV -- do count as events*/
		dynarray_add (eset->events, node);
		/*}}}*/
	} else {
		nocc_serious ("traceslang_firstevents_prewalk(): unexpected node (%s,%s)", node->tag->name, node->tag->ndef->name);
	}
	return 0;
}
/*}}}*/
/*{{{  static int traceslang_lastevents_prewalk (tnode_t *node, void *arg)*/
/*
 *	called to extract the set of trailing traces from a traces specification (prewalk order)
 *	returns 0 to stop walk, 1 to continue
 */
static int traceslang_lastevents_prewalk (tnode_t *node, void *arg)
{
	traceslang_eset_t *eset = (traceslang_eset_t *)arg;
	
	if (parser_islistnode (node)) {
		/*{{{  list node -- step through*/
		return 1;
		/*}}}*/
	} else if (node->tag == traceslang.tag_SEQ) {
		/*{{{  SEQ -- the last thing which happens*/
		tnode_t *body = tnode_nthsubof (node, 0);
		int nitems, i;
		tnode_t **items = parser_getlistitems (body, &nitems);
		int count = DA_CUR (eset->events);

		for (i=nitems-1; (i>=0) && (DA_CUR (eset->events) == count); i--) {
			/* walk this item */
			tnode_prewalktree (items[i], traceslang_lastevents_prewalk, (void *)eset);
		}

		/*}}}*/
	} else if ((node->tag == traceslang.tag_PAR) || (node->tag == traceslang.tag_NDET) || (node->tag == traceslang.tag_DET)) {
		/*{{{  PAR,NDET,DET -- last event from all branches*/
		return 1;
		/*}}}*/
	} else if ((node->tag == traceslang.tag_INPUT) || (node->tag == traceslang.tag_OUTPUT)) {
		/*{{{  INPUT,OUTPUT -- last event from subnode*/
		return 1;
		/*}}}*/
	} else if (node->tag == traceslang.tag_NPARAM) {
		/*{{{  NPARAM -- an event!*/
		dynarray_add (eset->events, node);
		/*}}}*/
	} else if ((node->tag == traceslang.tag_SKIP) || (node->tag == traceslang.tag_STOP) || (node->tag == traceslang.tag_CHAOS) || (node->tag == traceslang.tag_DIV)) {
		/*{{{  SKIP,STOP,CHAOS,DIV -- do count as events*/
		dynarray_add (eset->events, node);
		/*}}}*/
	} else {
		nocc_serious ("traceslang_lastevents_prewalk(): unexpected node (%s,%s)", node->tag->name, node->tag->ndef->name);
	}
	return 0;
}
/*}}}*/
/*{{{  static int traceslang_firsteventsp_modprewalk (tnode_t **nodep, void *arg)*/
/*
 *	called to extract the set of leading traces from a traces specification (modprewalk order)
 *	returns 0 to stop walk, 1 to continue
 */
static int traceslang_firsteventsp_modprewalk (tnode_t **nodep, void *arg)
{
	traceslang_erefset_t *eset = (traceslang_erefset_t *)arg;
	tnode_t *n = *nodep;

	if (parser_islistnode (n)) {
		/*{{{  list node -- step through*/
		return 1;
		/*}}}*/
	} else if (n->tag == traceslang.tag_SEQ) {
		/*{{{  SEQ -- only the first can happen*/
		tnode_t *body = tnode_nthsubof (n, 0);
		int nitems, i;
		tnode_t **items = parser_getlistitems (body, &nitems);
		int count = DA_CUR (eset->events);

		for (i=0; (i<nitems) && (DA_CUR (eset->events) == count); i++) {
			/* walk this item */
			tnode_modprewalktree (&(items[i]), traceslang_firsteventsp_modprewalk, (void *)eset);
		}

		/*}}}*/
	} else if ((n->tag == traceslang.tag_PAR) || (n->tag == traceslang.tag_NDET) || (n->tag == traceslang.tag_DET)) {
		/*{{{  PAR,NDET,DET -- first event from all branches*/
		return 1;
		/*}}}*/
	} else if ((n->tag == traceslang.tag_INPUT) || (n->tag == traceslang.tag_OUTPUT)) {
		/*{{{  INPUT,OUTPUT -- first event from subnode*/
		return 1;
		/*}}}*/
	} else if (n->tag == traceslang.tag_NPARAM) {
		/*{{{  NPARAM -- an event!*/
		dynarray_add (eset->events, nodep);
		/*}}}*/
	} else if ((n->tag == traceslang.tag_SKIP) || (n->tag == traceslang.tag_STOP) || (n->tag == traceslang.tag_CHAOS) || (n->tag == traceslang.tag_DIV)) {
		/*{{{  SKIP,STOP,CHAOS,DIV -- do count as events*/
		dynarray_add (eset->events, nodep);
		/*}}}*/
	} else {
		nocc_serious ("traceslang_firsteventsp_modprewalk(): unexpected node (%s,%s)", n->tag->name, n->tag->ndef->name);
	}
	return 0;
}
/*}}}*/
/*{{{  static int traceslang_lasteventsp_modprewalk (tnode_t **nodep, void *arg)*/
/*
 *	called to extract the set of trailing traces from a traces specification (modprewalk order)
 *	returns 0 to stop walk, 1 to continue
 */
static int traceslang_lasteventsp_modprewalk (tnode_t **nodep, void *arg)
{
	traceslang_eset_t *eset = (traceslang_eset_t *)arg;
	tnode_t *n = *nodep;
	
	if (parser_islistnode (n)) {
		/*{{{  list node -- step through*/
		return 1;
		/*}}}*/
	} else if (n->tag == traceslang.tag_SEQ) {
		/*{{{  SEQ -- the last thing which happens*/
		tnode_t *body = tnode_nthsubof (n, 0);
		int nitems, i;
		tnode_t **items = parser_getlistitems (body, &nitems);
		int count = DA_CUR (eset->events);

		for (i=nitems-1; (i>=0) && (DA_CUR (eset->events) == count); i--) {
			/* walk this item */
			tnode_modprewalktree (&(items[i]), traceslang_lasteventsp_modprewalk, (void *)eset);
		}

		/*}}}*/
	} else if ((n->tag == traceslang.tag_PAR) || (n->tag == traceslang.tag_NDET) || (n->tag == traceslang.tag_DET)) {
		/*{{{  PAR,NDET,DET -- last event from all branches*/
		return 1;
		/*}}}*/
	} else if ((n->tag == traceslang.tag_INPUT) || (n->tag == traceslang.tag_OUTPUT)) {
		/*{{{  INPUT,OUTPUT -- last event from subnode*/
		return 1;
		/*}}}*/
	} else if (n->tag == traceslang.tag_NPARAM) {
		/*{{{  NPARAM -- an event!*/
		dynarray_add (eset->events, nodep);
		/*}}}*/
	} else if ((n->tag == traceslang.tag_SKIP) || (n->tag == traceslang.tag_STOP) || (n->tag == traceslang.tag_CHAOS) || (n->tag == traceslang.tag_DIV)) {
		/*{{{  SKIP,STOP,CHAOS,DIV -- do count as events*/
		dynarray_add (eset->events, nodep);
		/*}}}*/
	} else {
		nocc_serious ("traceslang_lasteventsp_modprewalk(): unexpected node (%s,%s)", n->tag->name, n->tag->ndef->name);
	}
	return 0;
}
/*}}}*/
/*{{{  traceslang_eset_t *traceslang_firstevents (tnode_t *expr)*/
/*
 *	extracts the set of leading events from a traces specification
 *	returns a set structure on success, NULL on failure
 */
traceslang_eset_t *traceslang_firstevents (tnode_t *expr)
{
	traceslang_eset_t *eset = traceslang_newset ();

	tnode_prewalktree (expr, traceslang_firstevents_prewalk, (void *)eset);

	return eset;
}
/*}}}*/
/*{{{  traceslang_eset_t *traceslang_lastevents (tnode_t *expr)*/
/*
 *	extracts the set of trailing events from a traces specification
 *	returns a set structure on success, NULL on failure
 */
traceslang_eset_t *traceslang_lastevents (tnode_t *expr)
{
	traceslang_eset_t *eset = traceslang_newset ();

	tnode_prewalktree (expr, traceslang_lastevents_prewalk, (void *)eset);

	return eset;
}
/*}}}*/
/*{{{  traceslang_erefset_t *traceslang_firsteventsp (tnode_t **exprp)*/
/*
 *	extracts the set of leading events from a traces specification (by reference)
 *	returns a set structure on success, NULL on failure
 */
traceslang_erefset_t *traceslang_firsteventsp (tnode_t **exprp)
{
	traceslang_erefset_t *erset = traceslang_newrefset ();

	tnode_modprewalktree (exprp, traceslang_firsteventsp_modprewalk, (void *)erset);

	return erset;
}
/*}}}*/
/*{{{  traceslang_erefset_t *traceslang_lasteventsp (tnode_t **exprp)*/
/*
 *	extracts the set of trailing events from a traces specification (by reference)
 *	returns a set structure on success, NULL on failure
 */
traceslang_erefset_t *traceslang_lasteventsp (tnode_t **exprp)
{
	traceslang_erefset_t *erset = traceslang_newrefset ();

	tnode_modprewalktree (exprp, traceslang_lasteventsp_modprewalk, (void *)erset);

	return erset;
}
/*}}}*/

