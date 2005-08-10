/*
 *	dfa.c -- DFA utilities for nocc
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
#include "names.h"
#include "dfa.h"

/*}}}*/
/*{{{  private stuff*/
typedef struct TAG_nameddfa {
	char *name;
	dfanode_t *inode;
} nameddfa_t;

STATICSTRINGHASH (nameddfa_t *, nameddfas, 6);

typedef struct TAG_deferred_match {
	dfanode_t *inode;		/* initial */
	dfanode_t *enode;		/* end */
	token_t *match;			/* INAME token */
} deferred_match_t;

STATICDYNARRAY (deferred_match_t *, defmatches);

/* forward decls */
static int dfa_idecode_rule (char **bits, int first, int last, dfanode_t *idfa, dfanode_t *edfa, void **fnptrtable, int *fnptr);
static int dfa_idecode_totbl (char **bits, int first, int last, int istate, int estate, dfattbl_t *ttbl, void **fnptrtable, int *fnptr);

/*}}}*/


/*{{{  void dfa_init (void)*/
/*
 *	initialises the DFA engine
 */
void dfa_init (void)
{
	stringhash_init (nameddfas);
	dynarray_init (defmatches);
	return;
}
/*}}}*/
/*{{{  void dfa_shutdown (void)*/
/*
 *	shutdown the DFA engine
 */
void dfa_shutdown (void)
{
	return;
}
/*}}}*/
/*{{{  void dfa_dumpdfa (FILE *stream, dfanode_t *dfa)*/
/*
 *	dumps a DFA and all its nodes (debugging)
 */
void dfa_dumpdfa (FILE *stream, dfanode_t *dfa)
{
	DYNARRAY (dfanode_t *, list);
	int cur;

	dynarray_init (list);
	dynarray_add (list, dfa);

	for (cur = 0; cur < DA_CUR (list); cur++) {
		dfanode_t *thisdfa = DA_NTHITEM (list, cur);
		int i;

		fprintf (stream, "    DFA node: 0x%8.8x (%d in, %d matches, reduce with 0x%8.8x (0x%8.8x))  [%s]\n", (unsigned int)thisdfa, thisdfa->incoming, DA_CUR (thisdfa->match),
				(unsigned int)(thisdfa->reduce), (unsigned int)(thisdfa->rarg), (thisdfa->dfainfo ? ((nameddfa_t *)(thisdfa->dfainfo))->name : "??"));
		for (i=0; i<DA_CUR (thisdfa->match); i++) {
			token_t *t_tok = DA_NTHITEM (thisdfa->match, i);
			dfanode_t *t_pushto = DA_NTHITEM (thisdfa->pushto, i);
			dfanode_t *t_target = DA_NTHITEM (thisdfa->target, i);
			int t_flags = DA_NTHITEM (thisdfa->flags, i);

			fprintf (stream, "      MATCH ");
			lexer_dumptoken_short (stream, t_tok);
			if (t_pushto && (t_flags & DFAFLAG_PUSHSTACK)) {
				fprintf (stream, " PUSH 0x%8.8x", (unsigned int)t_pushto);
				if (t_pushto && t_pushto->dfainfo && (t_pushto->dfainfo != thisdfa->dfainfo)) {
					nameddfa_t *ndfa = (nameddfa_t *)t_pushto->dfainfo;

					fprintf (stream, " (%s)", ndfa->name);
				}
			}
			/* this state must be part of the same DFA, add it if not seen already */
			if (t_target) {
				dynarray_maybeadd (list, t_target);
			}
			fprintf (stream, " -> 0x%8.8x ", (unsigned int)t_target);
			/* show flags */
			fprintf (stream, "[%s%s%s%s]\n", (t_flags & DFAFLAG_NOCONSUME) ? "NC " : "",
					(t_flags & DFAFLAG_KEEP) ? "KP " : "",
					(t_flags & DFAFLAG_PUSHSTACK) ? "PS " : "",
					(t_flags & DFAFLAG_DEFERRED) ? "DF " : "");
		}
	}

	dynarray_trash (list);

	return;
}
/*}}}*/
/*{{{  void dfa_dumpnameddfa (FILE *stream, char *dfaname)*/
/*
 *	dumps a named DFA (debugging)
 */
void dfa_dumpnameddfa (FILE *stream, char *dfaname)
{
	dfanode_t *dfa = dfa_lookupbyname (dfaname);

	if (!dfa) {
		fprintf (stream, "unknown DFA [%s]\n", dfaname);
	} else {
		dfa_dumpdfa (stream, dfa);
	}
	return;
}
/*}}}*/
/*{{{  static void dfa_idumpnameddfa (nameddfa_t *ndfa, char *dfaname, void *voidptr)*/
/*
 *	helper for dfa_dumpdfas()
 */
static void dfa_idumpnameddfa (nameddfa_t *ndfa, char *dfaname, void *voidptr)
{
	FILE *stream = (FILE *)voidptr;

	fprintf (stream, "DFA called [%s] at 0x%8.8x:\n", dfaname, (unsigned int)(ndfa->inode));
	dfa_dumpdfa (stream, ndfa->inode);

	return;
}
/*}}}*/
/*{{{  void dfa_dumpdfas (FILE *stream)*/
/*
 *	dumps all DFAs (that are named)  (debugging)
 */
void dfa_dumpdfas (FILE *stream)
{
	stringhash_walk (nameddfas, dfa_idumpnameddfa, (void *)stream);
	return;
}
/*}}}*/


/*{{{  static dfattblent_t *dfa_newttblent (void)*/
/*
 *	creates a new transition table entry
 */
static dfattblent_t *dfa_newttblent (void)
{
	dfattblent_t *tblent = (dfattblent_t *)smalloc (sizeof (dfattblent_t));

	tblent->s_state = -1;
	tblent->e_state = -1;
	tblent->e_named = NULL;
	tblent->namedptr = NULL;
	tblent->match = NULL;
	tblent->reduce = NULL;
	tblent->rarg = NULL;
	tblent->rname = NULL;

	return tblent;
}
/*}}}*/
/*{{{  static dfattblent_t *dfa_dupttblent (dfattblent_t *tblent)*/
/*
 *	duplicates a transition table entry
 */
static dfattblent_t *dfa_dupttblent (dfattblent_t *tblent)
{
	dfattblent_t *newent = dfa_newttblent ();

	newent->s_state = tblent->s_state;
	newent->e_state = tblent->e_state;
	if (tblent->e_named) {
		newent->e_named = string_dup (tblent->e_named);
	}
	newent->namedptr = tblent->namedptr;
	if (tblent->match) {
		newent->match = string_dup (tblent->match);
	}
	newent->reduce = tblent->reduce;
	newent->rarg = tblent->rarg;
	if (tblent->rname) {
		newent->rname = string_dup (tblent->rname);
	}

	return newent;
}
/*}}}*/
/*{{{  static void dfa_freettblent (dfattblent_t *tblent)*/
/*
 *	frees a transition table entry
 */
static void dfa_freettblent (dfattblent_t *tblent)
{
	if (!tblent) {
		nocc_warning ("dfa_freettblent(): NULL table entry passed!");
		return;
	}
	if (tblent->rname) {
		sfree (tblent->rname);
	}
	if (tblent->match) {
		sfree (tblent->match);
	}
	sfree (tblent);

	return;
}
/*}}}*/
/*{{{  static dfattbl_t *dfa_newttbl (void)*/
/*
 *	creates a new transition table node
 */
static dfattbl_t *dfa_newttbl (void)
{
	dfattbl_t *ttbl = (dfattbl_t *)smalloc (sizeof (dfattbl_t));

	ttbl->name = NULL;
	ttbl->op = 0;
	ttbl->nstates = 0;
	dynarray_init (ttbl->entries);

	return ttbl;
}
/*}}}*/
/*{{{  void dfa_freettbl (dfattbl_t *ttbl)*/
/*
 *	frees a transition table node (and children)
 */
void dfa_freettbl (dfattbl_t *ttbl)
{
	int i;

	if (!ttbl) {
		nocc_warning ("dfa_freettbl(): NULL table passed!");
		return;
	}
	for (i=0; i<DA_CUR (ttbl->entries); i++) {
		dfattblent_t *tblent = DA_NTHITEM (ttbl->entries, i);

		if (tblent) {
			dfa_freettblent (tblent);
		}
	}
	dynarray_trash (ttbl->entries);
	if (ttbl->name) {
		sfree (ttbl->name);
	}
	sfree (ttbl);

	return;
}
/*}}}*/


/*{{{  static deferred_match_t *dfa_newdefmatch (void)*/
/*
 *	creates a new deferred_match_t structure, blank
 */
static deferred_match_t *dfa_newdefmatch (void)
{
	deferred_match_t *dmatch = (deferred_match_t *)smalloc (sizeof (deferred_match_t));

	dmatch->inode = NULL;
	dmatch->enode = NULL;
	dmatch->match = NULL;

	return dmatch;
}
/*}}}*/
/*{{{  static void dfa_freedefmatch (deferred_match_t *dmatch)*/
/*
 *	frees a deferred_match_t structure
 */
static void dfa_freedefmatch (deferred_match_t *dmatch)
{
	sfree (dmatch);
	return;
}
/*}}}*/
/*{{{  static void dfa_deferred_match (dfanode_t *inode, dfanode_t *enode, token_t *iname)*/
/*
 *	creates a new deferred match and adds it to the list
 */
static void dfa_deferred_match (dfanode_t *inode, dfanode_t *enode, token_t *iname)
{
	deferred_match_t *dmatch = dfa_newdefmatch ();

	dmatch->inode = inode;
	dmatch->enode = enode;
	dmatch->match = iname;
#if 0
fprintf (stderr, "dfa_deferred_match(): adding deferred match! iname contents [%s]\n", iname->u.str.ptr);
#endif

	dynarray_add (defmatches, dmatch);

	return;
}
/*}}}*/


/*{{{  dfanode_t *dfa_newnode (void)*/
/*
 *	creates a new DFA node
 */
dfanode_t *dfa_newnode (void)
{
	dfanode_t *dfa;

	dfa = (dfanode_t *)smalloc (sizeof (dfanode_t));
	dynarray_init (dfa->match);
	dynarray_init (dfa->target);
	dynarray_init (dfa->pushto);
	dynarray_init (dfa->flags);
	dfa->reduce = NULL;
	dfa->rarg = NULL;
	dfa->dfainfo = NULL;
	dfa->incoming = 0;

	return dfa;
}
/*}}}*/
/*{{{  dfanode_t *dfa_newnode_init (void (*reduce)(dfastate_t *, parsepriv_t *, void *), void *rarg)*/
/*
 *	creates a new DFA node with some initial settings
 */
dfanode_t *dfa_newnode_init (void (*reduce)(dfastate_t *, parsepriv_t *, void *), void *rarg)
{
	dfanode_t *dfa;

	dfa = dfa_newnode ();
	dfa->reduce = reduce;
	dfa->rarg = rarg;
	dfa->dfainfo = NULL;
	dfa->incoming = 0;

	return dfa;
}
/*}}}*/
/*{{{  void dfa_addmatch (dfanode_t *dfa, token_t *tok, dfanode_t *target, int flags)*/
/*
 *	adds a match to a DFA node
 */
void dfa_addmatch (dfanode_t *dfa, token_t *tok, dfanode_t *target, int flags)
{
	int i;

	if (!dfa) {
		nocc_internal ("dfa_addmatch(): called with NULL dfa");
		return;
	}
	if (flags & DFAFLAG_PUSHSTACK) {
		nocc_internal ("dfa_addmatch(): attempting to add push-stack match: use dfa_addpush()");
		return;
	}

	/* look for existing match */
	for (i=0; i<DA_CUR (dfa->match); i++) {
		token_t *thismatch = DA_NTHITEM (dfa->match, i);

		if ((((tok->type != NOTOKEN) && (thismatch->type != NOTOKEN)) || (tok->type == thismatch->type)) && lexer_tokmatch (thismatch, tok)) {
			nocc_warning ("dfa_addmatch(): displacing existing match");
#if 0
			lexer_dumptoken (stderr, tok);
			lexer_dumptoken (stderr, thismatch);
#endif
			break;		/* for() */
		}
	}
	if (i < DA_CUR (dfa->match)) {
		dfanode_t *xtarget = DA_NTHITEM (dfa->target, i);

		if (xtarget) {
			xtarget->incoming--;
		}
		/* replacing existing match */
		DA_SETNTHITEM (dfa->match, i, tok);
		DA_SETNTHITEM (dfa->target, i, target);
		DA_SETNTHITEM (dfa->pushto, i, NULL);
		DA_SETNTHITEM (dfa->flags, i, flags);
	} else {
		/* adding a new match */
		if ((i > 0) && (dfa->match[i-1]->type == NOTOKEN)) {
			/* this is a match-any, so better insert before it */
			dynarray_insert (dfa->match, tok, i-1);
			dynarray_insert (dfa->target, target, i-1);
			dynarray_insert (dfa->pushto, NULL, i-1);
			dynarray_insert (dfa->flags, flags, i-1);
		} else {
			dynarray_add (dfa->match, tok);
			dynarray_add (dfa->target, target);
			dynarray_add (dfa->pushto, NULL);
			dynarray_add (dfa->flags, flags);
		}
	}
	if (target) {
		target->incoming++;
	}
	return;
}
/*}}}*/
/*{{{  void dfa_addpush (dfanode_t *dfa, token_t *tok, dfanode_t *pushto, dfanode_t *target, int flags)*/
/*
 *	adds a push-match to a DFA
 */
void dfa_addpush (dfanode_t *dfa, token_t *tok, dfanode_t *pushto, dfanode_t *target, int flags)
{
	int i;

	if (!dfa) {
		nocc_internal ("dfa_addpush(): called with NULL dfa");
		return;
	}
	if (!(flags & DFAFLAG_PUSHSTACK) || !pushto) {
		nocc_internal ("dfa_addpush(): attempting to add non-push match: use dfa_addmatch() instead");
		return;
	}

	/* look for existing match */
	for (i=0; i<DA_CUR (dfa->match); i++) {
		token_t *thismatch = DA_NTHITEM (dfa->match, i);

		if (lexer_tokmatch (thismatch, tok)) {
			nocc_warning ("dfa_addmatch(): displacing existing match");
			break;		/* for() */
		}
	}
	if (i < DA_CUR (dfa->match)) {
		dfanode_t *xtarget = DA_NTHITEM (dfa->target, i);

		if (xtarget) {
			xtarget->incoming--;
		}
		/* replacing existing match */
		DA_SETNTHITEM (dfa->match, i, tok);
		DA_SETNTHITEM (dfa->target, i, target);
		DA_SETNTHITEM (dfa->pushto, i, pushto);
		DA_SETNTHITEM (dfa->flags, i, flags);
	} else {
		/* adding a new match */
		if ((i > 0) && (dfa->match[i-1]->type == NOTOKEN)) {
			/* this is a match-any, so better insert before it */
			dynarray_insert (dfa->match, tok, i-1);
			dynarray_insert (dfa->target, target, i-1);
			dynarray_insert (dfa->pushto, pushto, i-1);
			dynarray_insert (dfa->flags, flags, i-1);
		} else {
			dynarray_add (dfa->match, tok);
			dynarray_add (dfa->target, target);
			dynarray_add (dfa->pushto, pushto);
			dynarray_add (dfa->flags, flags);
		}
	}
	if (target) {
		target->incoming++;
	}
	return;
}
/*}}}*/
/*{{{  void dfa_matchpush (dfanode_t *dfa, char *pushto, dfanode_t *target, int deferring)*/
/*
 *	adds match(es) to a DFA node from initial matches from another,
 *	also incorporates a state push
 */
void dfa_matchpush (dfanode_t *dfa, char *pushto, dfanode_t *target, int deferring)
{
	nameddfa_t *ndfa = stringhash_lookup (nameddfas, pushto);
	dfanode_t *tdfa;
	int i;

	if (!ndfa) {
		if (!deferring) {
			nocc_internal ("dfa_matchpush(): no such DFA [%s]", pushto);
			return;
		} else {
			/* otherwise add a deferred entry for it */
			token_t *t_tok = lexer_newtoken (INAME, pushto);

#if 0
fprintf (stderr, "dfa_matchpush(): adding deferred match for [%s] (no DFA)\n", t_tok->u.str.ptr);
#endif
			dfa_addmatch (dfa, t_tok, target, DFAFLAG_DEFERRED);
			dfa_deferred_match (dfa, target, t_tok);
			return;
		}
	}
	tdfa = ndfa->inode;

	/* look to see if the target has any deferred initial matches */
	for (i=0; i<DA_CUR (tdfa->match); i++) {
		int t_flags = DA_NTHITEM (tdfa->flags, i);

		if (t_flags & DFAFLAG_DEFERRED) {
			break;		/* for() */
		}
	}
	if (i < DA_CUR (tdfa->match)) {
		/* yes, had some deferred initial match, defer this one too */
		token_t *t_tok = lexer_newtoken (INAME, pushto);

#if 0
fprintf (stderr, "dfa_matchpush(): adding deferred match for [%s] (initial deferrals)\n", t_tok->u.str.ptr);
#endif
		dfa_addmatch (dfa, t_tok, target, DFAFLAG_DEFERRED);
		dfa_deferred_match (dfa, target, t_tok);
		return;
	}

	/* iterate over target's initial matches, and produce new matches in the current DFA */
	for (i=0; i<DA_CUR (tdfa->match); i++) {
		token_t *t_tok = DA_NTHITEM (tdfa->match, i);
		dfanode_t *t_target = DA_NTHITEM (tdfa->target, i);
		dfanode_t *t_pushto = DA_NTHITEM (tdfa->pushto, i);
		int t_flags = DA_NTHITEM (tdfa->flags, i);

		if ((t_flags & DFAFLAG_PUSHSTACK) && t_pushto) {
			/*{{{  initial match is a push, duplicate it with NOCONSUME*/
			dfa_addpush (dfa, t_tok, tdfa, target, (t_flags & ~DFAFLAG_KEEP) | DFAFLAG_NOCONSUME | DFAFLAG_PUSHSTACK);

			/*}}}*/
		} else {
			/*{{{  regular initial match*/
			dfa_addpush (dfa, t_tok, t_target, target, t_flags | DFAFLAG_PUSHSTACK);

			/*}}}*/
		}

	}

	return;
}
/*}}}*/
/*{{{  void dfa_defaultto (dfanode_t *dfa, char *target)*/
/*
 *	adds a default match to a DFA
 */
void dfa_defaultto (dfanode_t *dfa, char *target)
{
	nameddfa_t *ndfa = stringhash_lookup (nameddfas, target);
	dfanode_t *tdfa;

	if (!ndfa) {
		nocc_internal ("dfa_defaultto(): no such DFA [%s]", target);
		return;
	}
	tdfa = ndfa->inode;

	dfa_addmatch (dfa, lexer_newtoken (NOTOKEN), tdfa, DFAFLAG_NOCONSUME);
	return;
}
/*}}}*/
/*{{{  void dfa_defaultpush (dfanode_t *dfa, char *pushto, dfanode_t *target)*/
/*
 *	adds a default match to the DFA that also pushes it
 */
void dfa_defaultpush (dfanode_t *dfa, char *pushto, dfanode_t *target)
{
	nameddfa_t *ndfa = stringhash_lookup (nameddfas, pushto);
	dfanode_t *tdfa;

	if (!ndfa) {
		nocc_internal ("dfa_defaultpush(): no such DFA [%s]", pushto);
		return;
	}
	tdfa = ndfa->inode;

	dfa_addpush (dfa, lexer_newtoken (NOTOKEN), tdfa, target, DFAFLAG_NOCONSUME | DFAFLAG_PUSHSTACK);
	return;
}
/*}}}*/
/*{{{  void dfa_defaultreturn (dfanode_t *dfa)*/
/*
 *	adds a default return to a DFA
 */
void dfa_defaultreturn (dfanode_t *dfa)
{
	dfa_addmatch (dfa, lexer_newtoken (NOTOKEN), NULL, DFAFLAG_NOCONSUME);
	return;
}
/*}}}*/
/*{{{  static int dfa_setname_walk (dfanode_t *dfa, nameddfa_t *ndfa)*/
/*
 *	this walks a DFA setting the "dfainfo" field on any DFA nodes that don't have it yet
 */
static int dfa_setname_walk (dfanode_t *dfa, nameddfa_t *ndfa)
{
	int i;

	if (!dfa || dfa->dfainfo) {
		return 0;
	}
	dfa->dfainfo = (void *)ndfa;
	for (i=0; i<DA_CUR (dfa->match); i++) {
		dfanode_t *target = DA_NTHITEM (dfa->target, i);

		if (target) {
			dfa_setname_walk (target, ndfa);
		}
	}
	return i;
}
/*}}}*/
/*{{{  int dfa_setname (dfanode_t *dfa, char *name)*/
/*
 *	sets the name of a DFA -- adds to named DFAs
 *	returns 0 if a new name is added, 1 if an existing one is replaced
 */
int dfa_setname (dfanode_t *dfa, char *name)
{
	nameddfa_t *ndfa;

	ndfa = stringhash_lookup (nameddfas, name);
	if (!ndfa) {
		ndfa = (nameddfa_t *)smalloc (sizeof (nameddfa_t));
		ndfa->name = string_dup (name);
		ndfa->inode = dfa;

		stringhash_insert (nameddfas, ndfa, ndfa->name);
		dfa_setname_walk (dfa, ndfa);
	} else if (ndfa->inode == dfa) {
		return 0;		/* already here! */
	} else {
		nocc_warning ("dfa_setname(): replacing existing definition for [%s]", ndfa->name);
		ndfa->inode = dfa;
		return 1;
	}
	return 0;
}
/*}}}*/
/*{{{  dfanode_t *dfa_lookupbyname (char *name)*/
/*
 *	finds a DFA based on its name
 */
dfanode_t *dfa_lookupbyname (char *name)
{
	nameddfa_t *ndfa = stringhash_lookup (nameddfas, name);

	if (ndfa) {
		return ndfa->inode;
	}
	return NULL;
}
/*}}}*/
/*{{{  int dfa_findmatch (dfanode_t *dfa, token_t *tok, dfanode_t **r_pushto, dfanode_t **r_target, int *r_flags)*/
/*
 *	finds a match in the given DFA, populates arguments if/with valid pointers
 *	returns 1 if found, 0 if not found
 */
int dfa_findmatch (dfanode_t *dfa, token_t *tok, dfanode_t **r_pushto, dfanode_t **r_target, int *r_flags)
{
	int i;

	for (i=0; i<DA_CUR (dfa->match); i++) {
		token_t *t_match = DA_NTHITEM (dfa->match, i);

		if (lexer_tokmatch (t_match, tok)) {
			if (r_pushto) {
				*r_pushto = DA_NTHITEM (dfa->pushto, i);
			}
			if (r_target) {
				*r_target = DA_NTHITEM (dfa->target, i);
			}
			if (r_flags) {
				*r_flags = DA_NTHITEM (dfa->flags, i);
			}
			return 1;
		}
	}
	return 0;
}
/*}}}*/


/*{{{  static int dfa_idecode_setredex (dfanode_t *dfa, const char *redex, void **fnptrtable, int *fnptr)*/
/*
 *	sets the reduction function for a particular DFA
 *	returns 0 on success, -1 on error
 */
static int dfa_idecode_setredex (dfanode_t *dfa, const char *redex, void **fnptrtable, int *fnptr)
{
	if (dfa->reduce) {
		nocc_error ("dfa_idecode_setredex(): node already has reduction! specifed here was [%s]", redex);
		return -1;
	}
	if (!strcmp (redex, "{}")) {
		/* pick one from "fnptrtable" */
		dfa->reduce = (void (*)(dfastate_t *, parsepriv_t *, void *))(fnptrtable[*fnptr]);
		(*fnptr)++;
		/* also pick up the argument */
		dfa->rarg = fnptrtable[*fnptr];
		(*fnptr)++;
	} else if (redex[1] == '<') {
		/* named generic reduction */
		char *rdx = string_ndup (redex + 2, strlen (redex) - 4);
		void *gredex;

		gredex = parser_lookup_grule (rdx);
		if (!gredex) {
			nocc_error ("dfa_idecode_setredex(): no such generic reduction [%s]", rdx);
			sfree (rdx);
			return -1;
		}
		dfa->reduce = parser_generic_reduce;
		dfa->rarg = gredex;
		sfree (rdx);
	} else {
		char *rdx = string_ndup (redex + 1, strlen (redex) - 2);

		dfa->reduce = parser_lookup_reduce (rdx);
		dfa->rarg = parser_lookup_rarg (rdx);
		sfree (rdx);
	}
#if 0
fprintf (stderr, "dfa_idecode_setredex(): got reduction: 0x%8.8x (0x%8.8x)\n", (unsigned int)(dfa->reduce), (unsigned int)(dfa->rarg));
#endif
	return 0;
}
/*}}}*/
/*{{{  static int dfa_idecode_choice (char **bits, int first, int last, dfanode_t *idfa, dfanode_t *edfa, void **fnptrtable, int *fnptr)*/
/*
 *	decodes "bits" from "first" to "last" forming a choice between "idfa" and "edfa",
 *	eating up "fnptrtable" elements as necessary (left-to-right, counting in "fnptr")
 *	returns 0 on success, -1 on failure
 */
static int dfa_idecode_choice (char **bits, int first, int last, dfanode_t *idfa, dfanode_t *edfa, void **fnptrtable, int *fnptr)
{
	int i;

	/* essentially this just involves going through each choice and recursively processing with dfa_idecode_rule() */
	for (i = first + 1; i < last;) {
		int next;
		int nesting = 0;

		/*{{{  scan to end of this choice*/
		for (next = i + 1; next < last; next++) {
			if (*bits[next] == '(') {
				nesting++;
			} else if (*bits[next] == ')') {
				nesting--;
			}
			if (!nesting && (*bits[next] == '|')) {
				break;			/* for() */
			}
		}
		/*}}}*/
		/* choice components are bits[i..(next-1)] */
		/* NOTE: if a choice finishes with a reduction "{.." then we
		 * stick it in as an extra node, followed by an any-transition to "edfa"
		 */
		if (*bits[next-1] == '{') {
			/* extra node requried */
			dfanode_t *tmpnode = dfa_newnode ();

			dfa_idecode_rule (bits, i, next-1, idfa, tmpnode, fnptrtable, fnptr);
			dfa_addmatch (tmpnode, lexer_newtoken (NOTOKEN), edfa, DFAFLAG_NOCONSUME);
		} else {
			/* just decode */
			dfa_idecode_rule (bits, i, next-1, idfa, edfa, fnptrtable, fnptr);
		}
		if (*bits[next] == '|') {
			i = next + 1;
		} else {
			i = next;
		}
	}

	return 0;
}
/*}}}*/
/*{{{  static int dfa_idecode_single (const char *mbit, dfanode_t *idfa, dfanode_t *edfa, int deferring)*/
/*
 *	decodes a single match between two DFA nodes
 *	return 0 on success, -1 on failure
 */
static int dfa_idecode_single (const char *mbit, dfanode_t *idfa, dfanode_t *edfa, int deferring)
{
	token_t *mtok = NULL;
	int flags = 0;
	char *bit = (char *)mbit;

	switch (*bit) {
		/*{{{  "(,|,),{" -- invalid here*/
	case '(':
	case '|':
	case ')':
	case '{':
		nocc_error ("dfa_idecode_singla(): cannot decode [%s]", bit);
		return -1;
		/*}}}*/
	}

	/* convert match to token */
	switch (*bit) {
		/*{{{  "+" -- keep token*/
	case '+':
		flags |= DFAFLAG_KEEP;
		bit++;
		break;
		/*}}}*/
		/*{{{  "-" -- don't consume token*/
	case '-':
		flags |= DFAFLAG_NOCONSUME;
		bit++;
		break;
		/*}}}*/
	}
	switch (*bit) {
		/*{{{  "*" -- match any*/
	case '*':
		mtok = lexer_newtoken (NOTOKEN);
		break;
		/*}}}*/
		/*{{{  built-ins*/
	case 'C':
		if (!strcmp (bit, "Comment")) {
			mtok = lexer_newtoken (COMMENT);
		}
		break;
	case 'E':
		if (!strcmp (bit, "End")) {
			mtok = lexer_newtoken (END);
		}
		break;
	case 'I':
		if (!strcmp (bit, "Indent")) {
			mtok = lexer_newtoken (INDENT);
		} else if (!strcmp (bit, "Integer")) {
			mtok = lexer_newtoken (INTEGER, 0);
		}
		break;
	case 'N':
		if (!strcmp (bit, "Name")) {
			mtok = lexer_newtoken (NAME, NULL);
		} else if (!strcmp (bit, "Newline")) {
			mtok = lexer_newtoken (NEWLINE);
		}
		break;
	case 'O':
		if (!strcmp (bit, "Outdent")) {
			mtok = lexer_newtoken (OUTDENT);
		}
		break;
	case 'R':
		if (!strcmp (bit, "Real")) {
			mtok = lexer_newtoken (REAL, (double)0.0);
		}
		break;
	case 'S':
		if (!strcmp (bit, "String")) {
			mtok = lexer_newtoken (STRING, NULL);
		}
		break;
		/*}}}*/
		/*{{{  "@" -- keyword or symbol*/
	case '@':
		bit++;
		if (*bit == '@') {
			/* symbol */
			mtok = lexer_newtoken (SYMBOL, bit + 1);
		} else {
			/* keyword */
			mtok = lexer_newtoken (KEYWORD, bit);
		}
		break;
		/*}}}*/
		/*{{{  default -- must be sub-match*/
	default:
		dfa_matchpush (idfa, bit, edfa, deferring);
		mtok = NULL;
		break;
		/*}}}*/
	}

	if (mtok) {
		/*{{{  add match*/
		dfa_addmatch (idfa, mtok, edfa, flags);
		/*}}}*/
	}

	return 0;
}
/*}}}*/
/*{{{  static int dfa_idecode_rule (char **bits, int first, int last, dfanode_t *idfa, dfanode_t *edfa, void **fnptrtable, int *fnptr)*/
/*
 *	decodes "bits" from "first" to "last", linking "idfa" to "edfa" along the way,
 *	"fnptrtable" contains the user-supplied reduction functions
 *	returns 0 on success, -1 on failure
 */
static int dfa_idecode_rule (char **bits, int first, int last, dfanode_t *idfa, dfanode_t *edfa, void **fnptrtable, int *fnptr)
{
	int i;
	int ef_last;		/* effective last */

#if 0
fprintf (stderr, "dfa_idecode_rule(): first = %d, last = %d (%d bits):\n    ", first, last, (last - first) + 1);
for (i=first; i<=last; i++) {
	fprintf (stderr, "%s ", bits[i]);
}
fprintf (stderr, "\n");
#endif
	for (ef_last = last; (ef_last > first) && (*bits[ef_last] == '{'); ef_last--);

	for (i = first; i <= last;) {
		dfanode_t *nextdfa;
		char *bit = bits[i];

		/* decide how to process this one first */
		switch (*bit) {
		case '(':		/* choice start */
			{
				int next;
				int nesting = 0;

				/*{{{  scan to end of choice*/
				for (next = i+1; next <= ef_last; next++) {
					if (*bits[next] == ')') {
						if (!nesting) {
							break;		/* for() */
						}
						nesting--;
					} else if (*bits[next] == '(') {
						nesting++;
					}
				}
				if (nesting) {
					nocc_error ("idfa_idecode_rule(): unbalanced nesting!");
					return -1;
				}
				/*}}}*/
				/*{{{  and decode it!*/
				if (next == ef_last) {
					nextdfa = edfa;
				} else {
					/* need an intermediate DFA state at end of the choice */
					nextdfa = dfa_newnode ();
				}
				dfa_idecode_choice (bits, i, next, idfa, nextdfa, fnptrtable, fnptr);
				idfa = nextdfa;
				i = next + 1;
				/*}}}*/
			}
			break;
		case '{':		/* reduction -- not expected here */
			nocc_error ("dfa_idecode_rule(): unexpected reduction specified [%s]", bit);
			return -1;
		default:
			/*{{{  deal with single match*/
			if (i == ef_last) {
				nextdfa = edfa;
			} else {
				/* need an intermediate DFA state */
				nextdfa = dfa_newnode ();
			}
			dfa_idecode_single (bit, idfa, nextdfa, 0);
			idfa = nextdfa;
			i++;
			break;
			/*}}}*/
		}

		/*{{{  check for a reduction*/
		if ((i <= last) && (*(bits[i]) == '{')) {
			/* idfa is the "next" state, so place reduction rule here */
			dfa_idecode_setredex (idfa, bits[i], fnptrtable, fnptr);
			i++;
		}
		/*}}}*/
	}

	return 0;
}
/*}}}*/
/*{{{  static int dfa_idecode_totbl_choice (char **bits, int first, int last, int istate, int estate, dfattbl_t *ttbl, void **fnptrtable, int *fnptr)*/
/*
 *	decodes "bits" from "first" to "last" as a choice, building transition table in "ttbl",
 *	"fnptrtable" contains user-supplied reduction functions (and argument).  "istate" is the
 *	start-of-choice state, "estate" is the end-of-choice state.
 *	returns 0 on success, -1 on failure
 */
static int dfa_idecode_totbl_choice (char **bits, int first, int last, int istate, int estate, dfattbl_t *ttbl, void **fnptrtable, int *fnptr)
{
	int i;
	int v = 0;

	/* essentially this just involves going through each choice and recursively processing with dfa_idecode_totbl() */
	for (i = first + 1; !v && (i < last);) {
		int next;
		int nesting = 0;

		/*{{{  scan to end of this choice*/
		for (next = i + 1; next < last; next++) {
			if (*bits[next] == '(') {
				nesting++;
			} else if (*bits[next] == ')') {
				nesting--;
			}
			if (!nesting && (*bits[next] == '|')) {
				break;			/* for() */
			}
		}
		/*}}}*/
		/*{{{  process it*/
		/* choice components are bits[i..(next-1)] */
		/* NOTE: if a choice finishes with a reduction "{.." then we
		 * stick it in as an extra node, followed by an any-transition to "edfa"
		 */
		if ((*bits[next-1] == '{') && (bits[next-1][1] != '\0')) {
			/* extra node requried */
			int nstate = ttbl->nstates++;
			dfattblent_t *tblent;

			v = dfa_idecode_totbl (bits, i, next-1, istate, nstate, ttbl, fnptrtable, fnptr);
			tblent = dfa_newttblent ();
			tblent->s_state = nstate;
			tblent->e_state = estate;
			tblent->match = string_dup ("-*");

			dynarray_add (ttbl->entries, tblent);
		} else {
			/* just decode */
			v = dfa_idecode_totbl (bits, i, next-1, istate, estate, ttbl, fnptrtable, fnptr);
		}
		if (*bits[next] == '|') {
			i = next + 1;
		} else {
			i = next;
		}
		/*}}}*/
	}
	
	return v;
}
/*}}}*/
/*{{{  static int dfa_idecode_totbl (char **bits, int first, int last, int istate, int estate, dfattbl_t *ttbl, void **fnptrtable, int *fnptr)*/
/*
 *	decodes "bits" from "first" to "last", building a transition table in "ttbl",
 *	"fnptrtable" contains user-supplied reduction functions (and argument).
 *	returns 0 on success, -1 on failure
 */
static int dfa_idecode_totbl (char **bits, int first, int last, int istate, int estate, dfattbl_t *ttbl, void **fnptrtable, int *fnptr)
{
	int i;
	int ef_last;		/* effective last */

#if 0
fprintf (stderr, "dfa_idecode_totbl(): istate=%d, estate=%d, first=%d, last=%d (%d bits):\n    ", istate, estate, first, last, (last - first) + 1);
for (i=first; i<=last; i++) {
	fprintf (stderr, "%s ", bits[i]);
}
fprintf (stderr, "\n");
#endif
	for (ef_last = last; (ef_last > first) && ((*bits[ef_last] == '{') && (bits[ef_last][1] != '\0')); ef_last--);
	
	for (i = first; i <= last; ) {

		switch (*(bits[i])) {
			/*{{{  '(' -- choice start*/
		case '(':
			{
				int next;
				int nesting = 0;
				int nstate;

				/*{{{  scan to end of choice*/
				for (next = i+1; next <= ef_last; next++) {
					if (*bits[next] == ')') {
						if (!nesting) {
							break;		/* for() */
						}
						nesting--;
					} else if (*bits[next] == '(') {
						nesting++;
					}
				}
				if (nesting) {
					nocc_error ("idfa_idecode_rule(): unbalanced choice nesting!");
					return -1;
				}
				/*}}}*/
				/*{{{  and decode it*/
				if (next == ef_last) {
					nstate = estate;
				} else {
					nstate = ttbl->nstates++;
				}
				if (dfa_idecode_totbl_choice (bits, i, next, istate, nstate, ttbl, fnptrtable, fnptr)) {
					return -1;
				}

				istate = nstate;
				i = next + 1;
				/*}}}*/
			}
			break;
			/*}}}*/
			/*{{{  '{' -- reduction or loop*/
		case '{':
			if (bits[i][1] != '\0') {
				nocc_error ("dfa_idecode_totbl(): unexpected reduction specified [%s]", bits[i]);
				return -1;
			} else {
				/* looping structure -- strict on the syntax of this, "{ subrule match N }" */
				int l1state, l2state;
				int nstate;
				char *mfirst, *msecond;
				dfattblent_t *tblent;
				int nspec;

				i++;
				mfirst = string_dup (bits[i]);
				i++;
				msecond = string_dup (bits[i]);
				i++;
				if (*bits[i] == '0') {
					nspec = 0;
				} else if (*bits[i] == '1') {
					nspec = 1;
				} else {
					nspec = -1;
				}
				i++;

				if (i == ef_last) {
					nstate = estate;
				} else {
					/* need an extra step */
					nstate = ttbl->nstates++;
				}

				l1state = ttbl->nstates++;
				l2state = ttbl->nstates++;
#if 0
fprintf (stderr, "dfa_idecode_totbl(): loop: i=%d, first=%d, last=%d, ef_last=%d, istate=%d, estate=%d, nspec=%d, l1state=%d, l2state=%d, nstate=%d\n", i, first, last, ef_last, istate, estate, nspec, l1state, l2state, nstate);
#endif

				/*{{{  build transitions*/
				/*{{{  [ istate mfirst l1state ]*/
				tblent = dfa_newttblent ();
				tblent->s_state = istate;
				tblent->e_state = l1state;
				tblent->match = string_dup (mfirst);
				dynarray_add (ttbl->entries, tblent);

				/*}}}*/
				/*{{{  [ l1state {<parser:nullreduce>} ]*/
				tblent = dfa_newttblent ();
				tblent->s_state = l1state;
				tblent->reduce = parser_generic_reduce;
				tblent->rarg = parser_lookup_grule ("parser:nullreduce");
				dynarray_add (ttbl->entries, tblent);

				/*}}}*/
				/*{{{  [ l1state msecond l2state ]*/
				tblent = dfa_newttblent ();
				tblent->s_state = l1state;
				tblent->e_state = l2state;
				tblent->match = msecond;
				dynarray_add (ttbl->entries, tblent);

				/*}}}*/
				/*{{{  [ l2state {Rinlist} ]*/
				tblent = dfa_newttblent ();
				tblent->s_state = l2state;
				tblent->reduce = parser_inlistreduce;
				tblent->rarg = NULL;
				dynarray_add (ttbl->entries, tblent);

				/*}}}*/
				/*{{{  [ l2state mfirst l1state ]*/
				tblent = dfa_newttblent ();
				tblent->s_state = l2state;
				tblent->e_state = l1state;
				tblent->match = mfirst;
				dynarray_add (ttbl->entries, tblent);

				/*}}}*/
				/*{{{  [ l1state -* nstate ]*/
				tblent = dfa_newttblent ();
				tblent->s_state = l1state;
				tblent->e_state = nstate;
				tblent->match = string_dup ("-*");
				dynarray_add (ttbl->entries, tblent);

				/*}}}*/
				if (nspec == 0) {
					/*{{{  [ istate -* nstate ]*/
					tblent = dfa_newttblent ();
					tblent->s_state = istate;
					tblent->e_state = nstate;
					tblent->match = string_dup ("-*");
					dynarray_add (ttbl->entries, tblent);
					/*}}}*/
				}
				/*}}}*/

				istate = nstate;
				i++;
			}
			break;
			/*}}}*/
			/*{{{  '[' -- optional*/
		case '[':
			{
				int next;
				int nesting = 0;
				int nstate;

				/*{{{  scan to end of optional*/
				for (next = i+1; next <= ef_last; next++) {
					if (*bits[next] == ']') {
						if (!nesting) {
							break;		/* for() */
						}
						nesting--;
					} else if (*bits[next] == '[') {
						nesting++;
					}
				}
				if (nesting) {
					nocc_error ("idfa_idecode_rule(): unbalanced optional nesting!");
					return -1;
				}
				/*}}}*/
				/*{{{  decode it*/
				if (next == ef_last) {
					nstate = estate;
				} else {
					nstate = ttbl->nstates++;
				}

				if (dfa_idecode_totbl (bits, i + 1, next - 1, istate, nstate, ttbl, fnptrtable, fnptr)) {
					return -1;
				}

				/* and we can also go there directly */
				{
					dfattblent_t *tblent = dfa_newttblent ();

					tblent->s_state = istate;
					tblent->e_state = nstate;
					tblent->match = string_dup ("-*");

					dynarray_add (ttbl->entries, tblent);
				}

				istate = nstate;
				i = next + 1;
				/*}}}*/
			}
			break;
			/*}}}*/
			/*{{{  default -- deal with single match*/
		default:
			{
				dfattblent_t *tblent = dfa_newttblent ();

				tblent->s_state = istate;
				if (i == ef_last) {
					tblent->e_state = estate;
					istate = estate;
				} else {
					/* need an intermediate state */
					istate = ttbl->nstates++;
					tblent->e_state = istate;
				}
				tblent->match = string_dup (bits[i]);

				dynarray_add (ttbl->entries, tblent);
				i++;
			}
			break;
			/*}}}*/
		}

		/*{{{  check for a reduction*/
		if ((i <= last) && ((*(bits[i]) == '{') && (bits[i][1] != '\0'))) {
			dfattblent_t *tblent = dfa_newttblent ();

			tblent->s_state = istate;

			switch (bits[i][1]) {
			case '}':
				/* reduction was given in param-table */
				tblent->reduce = (void (*)(dfastate_t *, parsepriv_t *, void *))(fnptrtable[*fnptr]);
				(*fnptr)++;
				tblent->rarg = fnptrtable[*fnptr];
				(*fnptr)++;
				break;
			case '<':
				/* named generic reduction */
				{
					char *rdx = string_ndup (bits[i] + 2, strlen (bits[i]) - 4);
					void *gredex;

					gredex = parser_lookup_grule (rdx);
					if (!gredex) {
						tblent->rname = string_dup (bits[i]);
					} else {
						tblent->rname = string_dup (bits[i]);		/* keep handy for debugging */
						tblent->reduce = parser_generic_reduce;
						tblent->rarg = gredex;
					}

					sfree (rdx);
				}
				break;
			default:
				/* named reduction */
				{
					char *rdx = string_ndup (bits[i] + 1, strlen (bits[i]) - 2);

					tblent->rname = string_dup (bits[i]);			/* keep handy for debugging */
					tblent->reduce = parser_lookup_reduce (rdx);
					if (tblent->reduce) {
						tblent->rarg = parser_lookup_rarg (rdx);
					} else {
						tblent->rname = string_dup (bits[i]);
					}
					sfree (rdx);
				}
				break;
			}

			dynarray_add (ttbl->entries, tblent);
			i++;
		}
		/*}}}*/
	}

	return 0;
}
/*}}}*/
/*{{{  static int dfa_idecode_checkmatch (const char *mbit, int lookuperr)*/
/*
 *	checks that a single match is valid, used by both decoders
 *	return 0 on error, advancement of "mbit" on success
 *	"lookuperr" is: 0 = ignore all lookup errors, 1 = fail all lookup errors, 2 = ignore only sub-spec lookup errors
 */
static int dfa_idecode_checkmatch (const char *mbit, int lookuperr)
{
	keyword_t *key = NULL;
	symbol_t *sym = NULL;
	dfanode_t *dfarule = NULL;
	int allow_subspec = 1;
	char *bit = (char *)mbit;
	char *xbit = bit;

	switch (*bit) {
	case '+':	/* push token */
		bit++;
		allow_subspec = 0;
		break;
	case '-':	/* push-back token */
		bit++;
		allow_subspec = 0;
		break;
	}

	/* try match as: @keyword, @@symbol, or built-in */
	if (*bit == '@') {
		bit++;
		if (*bit == '@') {
			int bl = strlen (bit) - 1;

			bit++;
			sym = symbols_lookup (bit, bl);
			if (!sym && lookuperr) {
				nocc_message ("dfa_idecode_checkmatch(): unknown symbol spec [%s]", xbit);
				return 0;
			}
			bit += bl;
		} else {
			int bl = strlen (bit);

			key = keywords_lookup (bit, bl);
			if (!key && lookuperr) {
				nocc_message ("dfa_idecode_checkmatch(): unknown keyword spec [%s]", xbit);
				return 0;
			}
			bit += bl;
		}
	} else {
		int gottok = 0;

		switch (*bit) {
			/*{{{  match builtins (upper-case starts)*/
		case 'I':
			if (!strcmp (bit, "Indent")) {
				gottok = 1;
				bit += 6;
			} else if (!strcmp (bit, "Integer")) {
				gottok = 1;
				bit += 7;
			}
			break;
		case 'N':
			if (!strcmp (bit, "Newline")) {
				gottok = 1;
				bit += 7;
			} else if (!strcmp (bit, "Name")) {
				gottok = 1;
				bit += 4;
			}
			break;
		case 'O':
			if (!strcmp (bit, "Outdent")) {
				gottok = 1;
				bit += 7;
			}
			break;
		case 'R':
			if (!strcmp (bit, "Real")) {
				gottok = 1;
				bit += 4;
			}
			break;
		case 'S':
			if (!strcmp (bit, "String")) {
				gottok = 1;
				bit += 6;
			}
			break;
		case 'C':
			if (!strcmp (bit, "Comment")) {
				gottok = 1;
				bit += 7;
			}
			break;
		case 'E':
			if (!strcmp (bit, "End")) {
				gottok = 1;
				bit += 3;
			}
			break;
			/*}}}*/
			/*{{{  * -- special match for any*/
		case '*':
			gottok = 1;
			bit++;
			break;
			/*}}}*/
			/*{{{  default -- if allowed a sub-spec, look it up*/
		default:
			/* check for sensible name */
			if ((*bit < 'a') || (*bit > 'z')) {
				gottok = 0;
			} else if (allow_subspec) {
				dfarule = dfa_lookupbyname (bit);
				if (dfarule || !lookuperr || (lookuperr == 2)) {
					gottok = 1;
				}
				bit += strlen (bit);
			}
			break;
			/*}}}*/
		}
		if (!gottok) {
			nocc_message ("dfa_idecode_checkmatch(): unknown rule spec [%s]", xbit);
			return 0;
		}
	}
	return (int)(bit - mbit);
}
/*}}}*/
/*{{{  static int dfa_idecode_checkrule (char **bits, va_list args, void **fnptrtable, int *nfnptrs, int lookuperr)*/
/*
 *	checks a DFA rule for sanity/correctness
 *	returns 0 on success, -1 on failure
 */
static int dfa_idecode_checkrule (char **bits, va_list args, void **fnptrtable, int *nfnptrs, int lookuperr)
{
	int i = 0;
	int cdepth = 0;
	int odepth = 0;

	/* check for a leading "name ::=" */
	if (bits[0] && bits[1] && bits[2] && !strcmp (bits[1], "::=")) {
		/* yes, have a leading "name ::=" */
		i = 2;
	} else if (bits[0] && bits[1] && bits[2] && !strcmp (bits[1], "+:=")) {
		/* leading "name +:=" */
		i = 2;
	}

	for (; bits[i]; i++) {
		char *bit = bits[i];

		/* look at rule bit and check */
		switch (*bit) {
			/*{{{  '|' -- choice separator*/
		case '|':
			if (cdepth < 1) {
				nocc_message ("dfa_idecode_checkrule(): choice separator outside choice");
				return -1;
			}
			bit++;
			break;
			/*}}}*/
			/*{{{  '(' -- choice start*/
		case '(':
			cdepth++;
			bit++;
			break;
			/*}}}*/
			/*{{{  ')' -- choice end*/
		case ')':
			if (!cdepth) {
				nocc_message ("dfa_idecode_checkrule(): choice end without start");
				return -1;
			}
			cdepth--;
			bit++;
			break;
			/*}}}*/
			/*{{{  '[' -- optional start*/
		case '[':
			bit++;
			odepth++;
			break;
			/*}}}*/
			/*{{{  ']' -- optional end*/
		case ']':
			if (!odepth) {
				nocc_message ("dfa_idecode_checkrule(): optional end without start");
				return -1;
			}
			odepth--;
			bit++;
			break;
			/*}}}*/
			/*{{{  '{' -- reduction specification or loop*/
		case '{':
			bit++;
			switch (*bit) {
				/*{{{  '\0' -- start of a loop specification -- not allowed to be nested*/
			case '\0':
				/* syntax is strictly:  { subrule match N }
				 * N will be either 0 or 1
				 */
				{
					int v;

					for (v=0; v<3; v++) {
						i++;
						if (!bits[i]) {
							nocc_message ("dfa_idecode_checkrule(): unexpected end of rule");
							return -1;
						} else if (!strcmp (bits[i], "}")) {
							nocc_message ("dfa_idecode_checkrule(): unexpected end of loop rule");
							return -1;
						}
					}
					/* bits[i] should be either 0 or 1 */
					if (((bits[i][0] != '0') && (bits[i][0] != '1')) || (bits[i][1] != '\0')) {
						nocc_message ("dfa_idecode_checkrule(): expected 0 or 1, found [%s]", bits[i]);
						return -1;
					}
					i++;
					if (!bits[i] || strcmp (bits[i], "}")) {
						nocc_message ("dfa_idecode_checkrule(): expected end of loop, found [%s]", bits[i] ?: "");
						return -1;
					}
				}
				break;
				/*}}}*/
				/*{{{  '}' -- using a reduction passed as an argument*/
			case '}':
				{
					void (*reduce)(dfastate_t *, parsepriv_t *, void *);

					reduce = va_arg (args, void (*)(dfastate_t *, parsepriv_t *, void *));
					fnptrtable[*nfnptrs] = (void *)reduce;
					(*nfnptrs)++;
					fnptrtable[*nfnptrs] = va_arg (args, void *);
					(*nfnptrs)++;
					bit++;
				}
				break;
				/*}}}*/
				/*{{{  '<' -- named generic reduction*/
			case '<':
				bit++;
				{
					char *nstart = bit;

					for (bit++; (*bit != '>') && (*bit != '\0'); bit++);
					if ((*bit != '>') || (bit[1] != '}')) {
						nocc_message ("dfa_idecode_checkrule(): broken reduction specification [%s]", bits[i]);
						return -1;
					}

					nstart = string_ndup (nstart, (int)(bit - nstart));
					if (!parser_lookup_grule (nstart) && lookuperr) {
						nocc_message ("dfa_idecode_checkrule(): unknown generic reduction [%s]", nstart);
						sfree (nstart);
						return -1;
					}
					sfree (nstart);
					bit += 2;
				}
				break;
				/*}}}*/
				/*{{{  default -- named reduction*/
			default:
				{
					char *nstart = bit;

					for (bit++; (*bit != '}') && (*bit != '\0'); bit++);
					if (*bit != '}') {
						nocc_message ("dfa_idecode_checkrule(): broken reduction specification [%s]", bits[i]);
						return -1;
					}

					nstart = string_ndup (nstart, (int)(bit - nstart));
					if (!parser_lookup_reduce (nstart) && lookuperr) {
						nocc_message ("dfa_idecode_checkrule(): unknown reduction [%s]", nstart);
						sfree (nstart);
						return -1;
					}
					sfree (nstart);
					bit++;
				}
				break;
				/*}}}*/
			}
			break;
			/*}}}*/
			/*{{{  '}' -- end of loop*/
		case '}':
			nocc_message ("dfa_idecode_checkrule(): loop end without start");
			return -1;
			/*}}}*/
			/*{{{  default - regular match*/
		default:
			{
				int j;

				j = dfa_idecode_checkmatch (bit, lookuperr);
				if (!j) {
					/* already reported error */
					return -1;
				}
				bit += j;
			}
			break;
			/*}}}*/
		}

		/* bit should be left pointing at the end of the bit -- i.e. terminator */
		if (*bit != '\0') {
			nocc_message ("dfa_idecode_checkrule(): junk found in bit [%s]", bits[i]);
			return -1;
		}

	}
	if (cdepth || odepth) {
		nocc_message ("dfa_idecode_checkrule(): unbalanced %s", (cdepth ? "choice" : "optional"));
		return -1;
	}
	return 0;
}
/*}}}*/
/*{{{  dfanode_t *dfa_decoderule (const char *rule, ...)*/
/*
 *	decodes a DFA rule (from text description)
 *	new version with varargs and more intelligent processing
 */
dfanode_t *dfa_decoderule (const char *rule, ...)
{
	dfanode_t *idfa = NULL;
	char *local = string_dup (rule);
	char **bits = split_string (local, 0);
	int i;
	va_list ap;
	char *name = NULL;
	void **fnptrtable = (void **)smalloc (64 * sizeof (void *));		/* XXX: limit */
	int nfnptrs = 0;

	/* check rule for sanity first */
	va_start (ap, rule);
	if (dfa_idecode_checkrule (bits, ap, fnptrtable, &nfnptrs, 1) < 0) {
		idfa = NULL;
	} else {
		dfanode_t *edfa = NULL;
		int start = 0;

		va_end (ap);
		va_start (ap, rule);

		/*{{{  okay, transform into DFA*/
		idfa = dfa_newnode ();
		edfa = dfa_newnode ();

		for (i=0; bits[i]; i++);	/* count bits */
		if (bits[0] && bits[1] && !strcmp (bits[1], "::=")) {
			start = 2;
			name = bits[0];
		}
		nfnptrs = 0;
		dfa_idecode_rule (bits, start, i-1, idfa, edfa, fnptrtable, &nfnptrs);
		/*}}}*/

		/* if this DFA is named, make edfa a return-node */
		if (edfa && name) {
			dfa_defaultreturn (edfa);
		}
	}
	va_end (ap);

	if (idfa && name) {
		dfa_setname (idfa, name);
	}

	sfree (bits);
	sfree (local);
	sfree (fnptrtable);

	return idfa;
}
/*}}}*/


/*{{{  static int dfa_idecodetrans_check (const char **bits, int *nstates, void ***fnptrtable, int *nfnptrs, va_list args, int lookuperr)*/
/*
 *	checks that a DFA transition description is valid, counts the number
 *	of states seen.  also scoops up var-arg parameters and sticks them in "fnptrtable", counting in "nfnptrs"
 *	returns 0 on success, non-zero on failure
 */
static int dfa_idecodetrans_check (const char **bits, int *nstates, void **fnptrtable, int *nfnptrs, va_list args, int lookuperr)
{
	int i = 0;
	int v;

	/* check for a leading "name ::="  or "name +:=" */
	if (bits[0] && bits[1] && bits[2] && !strcmp (bits[1], "::=")) {
		i = 2;		/* leading name */
	} else if (bits[0] && bits[1] && bits[2] && !strcmp (bits[1], "+:=")) {
		i = 2;		/* leading name */
	}

	*nstates = 0;
	for (; bits[i]; i++) {
		/*{{{  check and expect "[" */
		if (strcmp (bits[i], "[")) {
			nocc_message ("dfa_idecodetrans_check(): expected \"[\", found \"%s\"", bits[i]);
			return -1;
		}
		i++;
		if (!bits[i]) {
			goto unexpected_end;
		}

		/*}}}*/
		/*{{{  check and expect starting state number*/
		if (sscanf (bits[i], "%d", &v) != 1) {
			nocc_message ("dfa_idecodetrans_check(): expected integer, found [%s]", bits[i]);
			return -1;
		} else if (v > *nstates) {
			*nstates = v;
		}
		i++;
		if (!bits[i]) {
			goto unexpected_end;
		}

		/*}}}*/

		/* possibly have a reduction rule for this state */
		if ((bits[i][0] == '{') && (bits[i][1] == '}')) {
			/*{{{  empty reduction, means scoop up var-arg*/
			void (*rfunc)(dfastate_t *, parsepriv_t *, void *);
			void *rarg;

			rfunc = va_arg (args, void (*)(dfastate_t *, parsepriv_t *, void *));
			rarg = va_arg (args, void *);
#if 0
fprintf (stderr, "dfa_idecodetrans_check(): got reduction arg: 0x%8.8x\n", (unsigned int)(rfunc));
#endif
			/* add to fnptr table */
			fnptrtable[*nfnptrs] = (void *)rfunc;
			*nfnptrs = *nfnptrs + 1;
			fnptrtable[*nfnptrs] = rarg;
			*nfnptrs = *nfnptrs + 1;
			i++;
			/*}}}*/
		} else if ((bits[i][0] == '{') && (bits[i][1] == '<')) {
			/*{{{  generic reduction name*/
			int sl = strlen (bits[i]);
			void (*rfunc)(dfastate_t *, parsepriv_t *, void *) = parser_generic_reduce;
			void *rarg = NULL;
			char *rname;

			if ((bits[i][sl - 1] != '}') || (bits[i][sl - 2] != '>')) {
				nocc_message ("dfa_idecodetrans_check(): expected \'{<...>}\', found \'%s\'", bits[i]);
				return -1;
			}

			rname = string_ndup (bits[i] + 2, sl - 4);
			rarg = parser_lookup_grule (rname);
			if (!rarg && lookuperr) {
				nocc_message ("dfa_idecodetrans_check(): unknown generic reduction [%s]", rname);
				sfree (rname);
				return -1;
			} else if (!rarg) {
				rfunc = NULL;
			}
			sfree (rname);

			if (rfunc && lookuperr) {
				/* add to fnptr table */
				fnptrtable[*nfnptrs] = (void *)rfunc;
				*nfnptrs = *nfnptrs + 1;
				fnptrtable[*nfnptrs] = rarg;
				*nfnptrs = *nfnptrs + 1;
			}

			i++;
			if (!bits[i]) {
				goto unexpected_end;
			}
			/*}}}*/
		} else if (bits[i][0] == '{') {
			/*{{{  named reduction*/
			int sl = strlen (bits[i]);
			char *rname;
			void (*rfunc)(dfastate_t *, parsepriv_t *, void *);
			void *rarg = NULL;

			if (bits[i][sl - 1] != '}') {
				nocc_message ("dfa_idecodetrans_check(): expected \'}\', found \'%c\'", bits[i][sl - 1]);
				return -1;
			}

			rname = string_ndup (bits[i] + 1, sl - 2);
			rfunc = parser_lookup_reduce (rname);
			if (!rfunc && lookuperr) {
				nocc_message ("dfa_idecodetrans_check(): unknown reduction rule [%s]", rname);
				sfree (rname);
				return -1;
			} else if (rfunc) {
				rarg = parser_lookup_rarg (rname);
			}
			sfree (rname);

			if (rfunc && lookuperr) {
				/* and add to fnptr table */
				fnptrtable[*nfnptrs] = (void *)rfunc;
				*nfnptrs = *nfnptrs + 1;
				fnptrtable[*nfnptrs] = rarg;
				*nfnptrs = *nfnptrs + 1;
			}

			i++;
			if (!bits[i]) {
				goto unexpected_end;
			}
			/*}}}*/
		}

		/*{{{  might just be a reduction specification*/
		if (!strcmp (bits[i], "]")) {
			continue;
		}

		/*}}}*/
		/*{{{  expecting a match*/
		if (!dfa_idecode_checkmatch (bits[i], lookuperr)) {
			/* already reported error */
			return -1;
		}
		i++;
		if (!bits[i]) {
			goto unexpected_end;
		}

		/*}}}*/

		/* should either have next state or end of rule */
		if ((*bits[i] >= '0') && (*bits[i] <= '9')) {
			/*{{{  integer next state*/
			if (sscanf (bits[i], "%d", &v) != 1) {
				nocc_message ("dfa_idecodetrans_check(): expected integer, found [%s]", bits[i]);
				return -1;
			} else if (v > *nstates) {
				*nstates = v;
			}
			i++;
			if (!bits[i]) {
				goto unexpected_end;
			}
			/*}}}*/
		} else if (*bits[i] == '<') {
			/*{{{  name or pointer to next state*/
			if (bits[i][1] == '>') {
				/* pointer as vararg */
				fnptrtable[*nfnptrs] = (void *)va_arg (args, dfanode_t *);
				*nfnptrs = *nfnptrs + 1;
			} else {
				/* jump to initial node in named DFA */
				int sl = strlen (bits[i]);
				char *rname;
				dfanode_t *targdfa;

				rname = string_ndup (bits[i] + 1, sl - 2);
				targdfa = dfa_lookupbyname (rname);
				if (!targdfa && lookuperr) {
					nocc_message ("dfa_idecodetrans_check(): unknown DFA target [%s]", rname);
					sfree (rname);
					return -1;
				}
				sfree (rname);

				if (targdfa && lookuperr) {
					fnptrtable[*nfnptrs] = (void *)targdfa;
					*nfnptrs = *nfnptrs + 1;
				}
			}
			i++;
			if (!bits[i]) {
				goto unexpected_end;
			}
			/*}}}*/
		}

		/*{{{  now expecting end-of-rule*/
		if (strcmp (bits[i], "]")) {
			nocc_message ("dfa_idecodetrans_check(): expected \"]\", found \"%s\"", bits[i]);
			return -1;
		}
		/*}}}*/
	}

	return 0;
unexpected_end:
	nocc_message ("dfa_idecodetrans_check(): unexpected end of rule");
	return -1;
}
/*}}}*/
/*{{{  static int dfa_idecodetrans_populate (const char **bits, dfanode_t **nodetable, void **fnptrtable)*/
/*
 *	this semi-decodes "bits", finding matches that are already present
 *	and assigning the nodes to "nodetable".  Used "bits" are set to ".",
 *	but reductions must be left (don't want to hack-up fnptrtable)
 */
static int dfa_idecodetrans_populate (const char **bits, dfanode_t **nodetable, void **fnptrtable)
{
	nocc_internal ("dfa_idecodetrans_populate(): not entirely implemented yet..");

	return 0;
}
/*}}}*/
/*{{{  int dfa_idecodetrans_decode (const char **bits, dfanode_t **nodetable, void **fnptrtable)*/
/*
 *	decodes a DFA transition table
 *	returns 0 on success, non-zero on failure (shouldn't fail, really ..)
 */
int dfa_idecodetrans_decode (const char **bits, dfanode_t **nodetable, void **fnptrtable)
{
	int i = 0;
	int fnptr = 0;

	/* check for a leading "name ::=" or "name +:=" */
	if (bits[0] && bits[1] && bits[2] && (!strcmp (bits[1], "::=") || !strcmp (bits[1], "+:="))) {
		/* yes, have a leading "name .." */
		i = 2;
	}

	for (; bits[i]; i++) {
		int start_state;
		int end_state;
		const char *match;
		dfanode_t *end_node;

		/*{{{  expecting "["*/
		if (strcmp (bits[i], "[")) {
			nocc_message ("dfa_idecodetrans_decode(): expected \"[\", found \"%s\"", bits[i]);
			return -1;
		}
		i++;
		/*}}}*/
		/*{{{  now expecting state number*/
		if (sscanf (bits[i], "%d", &start_state) != 1) {
			nocc_message ("dfa_idecodetrans_decode(): expected integer, found [%s]", bits[i]);
			return -1;
		}
		i++;
		/*}}}*/
		/*{{{  initial state reduction*/
		if (bits[i][0] == '{') {
			if (dfa_idecode_setredex (nodetable[start_state], bits[i], fnptrtable, &fnptr)) {
				return -1;
			}
			i++;
		}
		/*}}}*/
		/*{{{  early end with "]"*/
		if (!strcmp (bits[i], "]")) {
			/* just a reduction spec */
			continue;
		}
		/*}}}*/
		/*{{{  expecting a match, hang on to it*/
		match = bits[i];
		i++;

		/*}}}*/
		/*{{{  expect target*/
		end_state = -2;
		end_node = NULL;
		if ((*bits[i] >= '0') && (*bits[i] <= '9')) {
			if (sscanf (bits[i], "%d", &end_state) != 1) {
				nocc_message ("dfa_idecodetrans_decode(): expected integer, found [%s]", bits[i]);
				return -1;
			} else {
				end_node = nodetable[end_state];
			}
			i++;
		} else if (*bits[i] == ']') {
			end_state = -1;
		} else if (*bits[i] == '<') {
			end_node = (dfanode_t *)fnptrtable[fnptr++];
			end_state = 0;
		}

		/*}}}*/
		/*{{{  check and decode match between DFA nodes*/
		if (dfa_idecode_single (match, nodetable[start_state], (end_state < 0) ? NULL : end_node, 0)) {
			nocc_message ("dfa_idecodetrans_decode(): failed to decode match [%s]", match);
			return -1;
		}

		/*}}}*/
		/*{{{  now expecting end-of-rule*/
		if (strcmp (bits[i], "]")) {
			nocc_message ("dfa_idecodetrans_decode(): expected \"]\", found \"%s\"", bits[i]);
			return -1;
		}
		/*}}}*/
	}

	return 0;
}
/*}}}*/
/*{{{  int dfa_idecodetrans_tbl (dfattblent_t **entries, int nentries, dfanode_t **nodetable)*/
/*
 *	decodes a DFA transition table
 *	returns 0 on success, non-zero on failure (shouldn't fail, really ..)
 */
int dfa_idecodetrans_tbl (dfattblent_t **entries, int nentries, dfanode_t **nodetable)
{
	int i;

	for (i=0; i<nentries; i++) {
		int start_state, end_state;
		dfanode_t *end_node;

		if (!entries[i]) {
			nocc_warning ("dfa_idecodetrans_tbl(): NULL entry in table (ignoring)");
			continue;
		}
		/*{{{  decode entry*/
		start_state = entries[i]->s_state;
#if 0
fprintf (stderr, "dfa_idecodetrans_tbl(): entries[%d]->reduce = 0x%8.8x, entries[%d]->rarg = 0x%8.8x\n", i, (unsigned int)(entries[i]->reduce), i, (unsigned int)(entries[i]->rarg));
#endif
		if (entries[i]->reduce) {
			nodetable[start_state]->reduce = entries[i]->reduce;
			nodetable[start_state]->rarg = entries[i]->rarg;
		} else if (entries[i]->rname) {
			/* must be able to resolve this here! */
			char *rname = entries[i]->rname;

			if ((rname[0] == '{') && (rname[1] == '<')) {
				char *rdx = string_ndup (rname + 2, strlen (rname) - 4);
				void *gredex;

				gredex = parser_lookup_grule (rdx);
				if (!gredex) {
					nocc_error ("dfa_idecodetrans_tbl(): no such generic reduction rule [%s]", rdx);
					sfree (rdx);
					return -1;
				}
				nodetable[start_state]->reduce = parser_generic_reduce;
				nodetable[start_state]->rarg = gredex;

				sfree (rdx);
			} else {
				char *rdx = string_ndup (rname + 1, strlen (rname) - 2);

				nodetable[start_state]->reduce = parser_lookup_reduce (rdx);
				nodetable[start_state]->rarg = parser_lookup_rarg (rdx);
				if (!nodetable[start_state]->reduce) {
					nocc_error ("dfa_idecodetrans_tbl(): no such reduction rule [%s]", rdx);
					sfree (rdx);
					return -1;
				}
				sfree (rdx);
			}
		}

		end_state = -1;
		end_node = NULL;
		if (entries[i]->e_state >= 0) {
			end_state = entries[i]->e_state;
			end_node = nodetable[end_state];
		} else if (entries[i]->e_named) {
			if (entries[i]->namedptr) {
				end_state = 0;
				end_node = (dfanode_t *)(entries[i]->namedptr);
			} else {
				/*{{{  lookup target by name*/
				char *targetname = string_ndup (entries[i]->e_named + 1, strlen (entries[i]->e_named) - 2);
				dfanode_t *targdfa;

				targdfa = dfa_lookupbyname (targetname);
				if (!targdfa) {
					nocc_error ("dfa_idecodetrans_tbl(): no such DFA [%s]", targetname);
					sfree (targetname);
					return -1;
				}
				end_state = 0;
				end_node = targdfa;
				sfree (targetname);
				/*}}}*/
			}
		}

		if (entries[i]->match) {
			if (!dfa_idecode_checkmatch (entries[i]->match, 2)) {
#if 0
fprintf (stderr, "dfa_idecodetrans_tbl(): failed dfa_idecode_checkmatch() for [%s]\n", entries[i]->match);
#endif
				return -1;
			}
			/* allow deferred matches */
			if (dfa_idecode_single (entries[i]->match, nodetable[start_state], (end_state < 0) ? NULL : end_node, 1)) {
				nocc_internal ("dfa_idecodetrans_tbl(): failed to decode match [%s]", entries[i]->match);
			}
		}
		/*}}}*/
	}

	return 0;
}
/*}}}*/
/*{{{  dfattbl_t *dfa_transtotbl (const char *rule, ...)*/
/*
 *	turns a DFA transition system into a set of transition-table entries,
 *	returns the transition table.  (a sort of pre-parse on the transition table,
 *	before we go checking things in detail).
 */
dfattbl_t *dfa_transtotbl (const char *rule, ...)
{
	dfattbl_t *ttbl;
	char *local = string_dup (rule);
	char **bits = split_string (local, 0);
	int i = 0;
	va_list ap;
	int nstates;
	void **fnptrtable = (void **)smalloc (64 * sizeof (void *));		/* XXX: limit */
	int nfnptrs = 0;

	/* check rule */
	va_start (ap, rule);
	if (dfa_idecodetrans_check ((const char **)bits, &nstates, fnptrtable, &nfnptrs, ap, 0)) {
		ttbl = NULL;
	} else {
		ttbl = dfa_newttbl ();
		int fnptr = 0;

		/*{{{  scoop-up and set any leading name*/
		if (bits[0] && bits[1] && !strcmp (bits[1], "::=")) {
			ttbl->name = string_dup (bits[0]);
			ttbl->op = 0;
			i = 2;
		} else if (bits[0] && bits[1] && !strcmp (bits[1], "+:=")) {
			ttbl->name = string_dup (bits[0]);
			ttbl->op = 1;
			i = 2;
		}
		/*}}}*/

		/* now walk over the bits .. (checked for correctness already)*/
		ttbl->nstates = nstates;
		for (; bits[i]; i++) {
			dfattblent_t *tblent = dfa_newttblent ();

			/*{{{  expecting "["*/
			i++;
			/*}}}*/
			/*{{{  state number*/
			sscanf (bits[i], "%d", &(tblent->s_state));
			i++;
			/*}}}*/
			/*{{{  optional reduction*/
			if ((bits[i][0] == '{') && (bits[i][1] == '}')) {
				tblent->reduce = (void (*)(dfastate_t *, parsepriv_t *, void *))(fnptrtable[fnptr++]);
				tblent->rarg = fnptrtable[fnptr++];
				i++;
			} else if ((bits[i][0] == '{') && (bits[i][1] == '<')) {
				char *rdx = string_ndup (bits[i] + 2, strlen (bits[i]) - 4);

				tblent->rarg = parser_lookup_grule (rdx);
				if (tblent->rarg) {
					tblent->reduce = parser_generic_reduce;
				}
				tblent->rname = string_dup (bits[i]);		/* keep handy for debugging regardless */
				sfree (rdx);
				i++;
			} else if (bits[i][0] == '{') {
				char *rdx = string_ndup (bits[i] + 1, strlen (bits[i]) - 2);

				tblent->reduce = parser_lookup_reduce (rdx);
				if (tblent->reduce) {
					tblent->rarg = parser_lookup_rarg (rdx);
				}
				tblent->rname = string_dup (bits[i]);		/* keep handy for debugging regardless */

				sfree (rdx);
				i++;
			}
			/*}}}*/
			/*{{{  might just be a reduction specification*/
			if (!strcmp (bits[i], "]")) {
				dynarray_add (ttbl->entries, tblent);
				continue;
			}
			/*}}}*/
			/*{{{  the match*/
			tblent->match = string_dup (bits[i]);
			i++;
			/*}}}*/
			/*{{{  optional next state*/
			if ((*bits[i] >= '0') && (*bits[i] <= '9')) {
				/*{{{  integer next state*/
				sscanf (bits[i], "%d", &(tblent->e_state));
				i++;
				/*}}}*/
			} else if (*bits[i] == '<') {
				/*{{{  name or pointer to next state*/
				tblent->e_named = string_dup (bits[i]);
				if (bits[i][1] == '>') {
					/* pointer to next state, scoop up */
					tblent->namedptr = fnptrtable[fnptr++];
				}
				i++;
				/*}}}*/
			}
			/*}}}*/
			/*{{{  should be at end of rule "]" -- add*/
			dynarray_add (ttbl->entries, tblent);
			/*}}}*/
		}
	}
	va_end (ap);

	sfree (bits);
	sfree (local);
	sfree (fnptrtable);

	return ttbl;
}
/*}}}*/
/*{{{  dfattbl_t *dfa_bnftotbl (const char *rule, ...)*/
/*
 *	turns a BNF grammer for a DFA transition system into a set of transition-table entries.
 */
dfattbl_t *dfa_bnftotbl (const char *rule, ...)
{
	dfattbl_t *ttbl;
	char *local = string_dup (rule);
	char **bits = split_string (local, 0);
	int i = 0;
	va_list ap;
	void **fnptrtable = (void **)smalloc (64 * sizeof (void *));		/* XXX: limit */
	int nfnptrs = 0;

	/* check rule */
	va_start (ap, rule);
	if (dfa_idecode_checkrule (bits, ap, fnptrtable, &nfnptrs, 0)) {
		ttbl = NULL;
	} else {
		ttbl = dfa_newttbl ();
		int fnptr = 0;
		int nbits;

		/*{{{  scoop-up and set any leading name*/
		if (bits[0] && bits[1] && !strcmp (bits[1], "::=")) {
			ttbl->name = string_dup (bits[0]);
			ttbl->op = 0;
			i = 2;
		} else if (bits[0] && bits[1] && !strcmp (bits[1], "+:=")) {
			ttbl->name = string_dup (bits[0]);
			ttbl->op = 1;
			i = 2;
		}
		/*}}}*/
#if 0
fprintf (stderr, "dfa_bnftotbl(): passed check..! -- ought to decode now (nfnptrs = %d)\n", nfnptrs);
#endif
		ttbl->nstates = 2;
		for (nbits=i; bits[nbits]; nbits++);
		if (dfa_idecode_totbl (bits, i, nbits - 1, 0, 1, ttbl, fnptrtable, &fnptr)) {
			/* failed */
			dfa_freettbl (ttbl);
			ttbl = NULL;
		}
		/* make end-state return */
		if (ttbl) {
			dfattblent_t *tblent = dfa_newttblent ();

			tblent->s_state = 1;
			tblent->match = string_dup ("-*");
			
			dynarray_add (ttbl->entries, tblent);
		}
	}
	va_end (ap);

	sfree (bits);
	sfree (local);
	sfree (fnptrtable);

	return ttbl;
}
/*}}}*/
/*{{{  static void dfa_dumpttblent (FILE *stream, dfattblent_t *tblent)*/
/*
 *	dumps a transition-table entry
 */
static void dfa_dumpttblent (FILE *stream, dfattblent_t *tblent)
{
	fprintf (stream, " [ %d", tblent->s_state);
	if (tblent->rname) {
		fprintf (stream, " %s", tblent->rname);
	} else if (tblent->reduce) {
		fprintf (stream, " {0x%8.8x:0x%8.8x}", (unsigned int)tblent->reduce, (unsigned int)tblent->rarg);
	}
	if (tblent->match) {
		fprintf (stream, " %s", tblent->match);
	}
	if (tblent->e_state > -1) {
		fprintf (stream, " %d", tblent->e_state);
	} else if (tblent->e_named) {
		fprintf (stream, " %s", tblent->e_named);
	}
	fprintf (stream, " ]");

	return;
}
/*}}}*/
/*{{{  static void dfa_dumpttblent_gra (FILE *stream, dfattblent_t *tblent, int estate)*/
/*
 *	dumps a transition-table entry (in UWGVGraph .gra format)
 */
static void dfa_dumpttblent_gra (FILE *stream, dfattblent_t *tblent, int estate)
{
	if (tblent->match) {
		fprintf (stream, "EDGE %d %d TYPE LINE LABEL \"%s\"\n", tblent->s_state, (tblent->e_state < 0) ? estate : tblent->e_state, tblent->match);
	}
	return;
}
/*}}}*/
/*{{{  void dfa_dumpttbl (FILE *stream, dfattbl_t *ttbl)*/
/*
 *	dumps a dfattbl_t transition table
 */
void dfa_dumpttbl (FILE *stream, dfattbl_t *ttbl)
{
	int i;

	if (!ttbl) {
		nocc_internal ("dfa_dumpttbl(): NULL transition table!");
		return;
	}
	if (ttbl->name) {
		fprintf (stream, "[nstates=%d] %s %s", ttbl->nstates, ttbl->name, (ttbl->op == 0) ? "::=" : "+:=");
	} else {
		fprintf (stream, "[nstates=%d] (anon) %s", ttbl->nstates, (ttbl->op == 0) ? "::=" : "+:=");
	}
	for (i=0; i<DA_CUR (ttbl->entries); i++) {
		dfattblent_t *tblent = DA_NTHITEM (ttbl->entries, i);

		if (!tblent) {
			nocc_internal ("dfa_dumpttbl(): NULL table entry!");
			return;
		}
		dfa_dumpttblent (stream, tblent);
	}
	fprintf (stream, "\n");

	return;
}
/*}}}*/
/*{{{  void dfa_dumpttbl_gra (FILE *stream, dfattbl_t *ttbl)*/
/*
 *	dumps a dfattbl_t transition table (in UWGVGraph .gra format)
 */
void dfa_dumpttbl_gra (FILE *stream, dfattbl_t *ttbl)
{
	int i;
	int *seen_states;
	char **seen_reducers;
	int enode;

	if (!ttbl) {
		nocc_internal ("dfa_dumpttbl_gra(): NULL transition table!");
		return;
	}
	fprintf (stream, "#\n# transition table for [%s], nstates = %d, op = %s\n#\n\n", ttbl->name ?: "(anon)", ttbl->nstates, (ttbl->op == 0) ? "::=" : "+:=");

	seen_states = (int *)smalloc ((ttbl->nstates + 1) * sizeof (int));
	seen_reducers = (char **)smalloc ((ttbl->nstates + 1) * sizeof (char *));

	for (i=0; i<=ttbl->nstates; i++) {
		seen_states[i] = 0;
		seen_reducers[i] = NULL;
	}

	for (i=0; i<DA_CUR (ttbl->entries); i++) {
		dfattblent_t *tblent = DA_NTHITEM (ttbl->entries, i);

		if (!tblent) {
			nocc_internal ("dfa_dumpttbl_gra(): NULL table entry!");
			return;
		}
		if (!seen_reducers[tblent->s_state]) {
			if (tblent->rname) {
				seen_reducers[tblent->s_state] = string_dup (tblent->rname);
			} else if (tblent->reduce) {
				seen_reducers[tblent->s_state] = (char *)smalloc (128);
				sprintf (seen_reducers[tblent->s_state], "0x%8.8x (0x%8.8x)", (unsigned int)tblent->reduce, (unsigned int)tblent->rarg);
			}
		}
	}
	for (i=0; i<DA_CUR (ttbl->entries); i++) {
		dfattblent_t *tblent = DA_NTHITEM (ttbl->entries, i);

		if (!seen_states[tblent->s_state]) {
			char *redex = seen_reducers[tblent->s_state];

			seen_states[tblent->s_state] = 1;
			fprintf (stream, "NODE %d TYPE POINT SIZE 5 LABEL \"%d%s%s%s\"\n", tblent->s_state, tblent->s_state, (!tblent->s_state) ? " (start)" : "", redex ? " " : "", redex ?: "");
			if (redex) {
				sfree (redex);
			}
		}
	}

	enode = ttbl->nstates + 1;
	fprintf (stream, "NODE %d TYPE POINT SIZE 5 LABEL \"%d (end)\"\n", enode, enode);

	for (i=0; i<DA_CUR (ttbl->entries); i++) {
		dfattblent_t *tblent = DA_NTHITEM (ttbl->entries, i);

		dfa_dumpttblent_gra (stream, tblent, enode);
	}
	fprintf (stream, "\n");

	sfree (seen_states);
	sfree (seen_reducers);

	return;
}
/*}}}*/

/*{{{  dfanode_t *dfa_tbltodfa (dfattbl_t *ttbl)*/
/*
 *	turns a semi-compiled DFA transition system into a proper DFA system,
 *	setting the name, etc.
 */
dfanode_t *dfa_tbltodfa (dfattbl_t *ttbl)
{
	dfanode_t *idfa;
	dfanode_t **nodetable;
	int i;

	if (!ttbl) {
		nocc_internal ("dfa_tbltodfa(): NULL transition table!");
		return NULL;
	}
	idfa = NULL;
	nodetable = (dfanode_t **)smalloc ((ttbl->nstates + 1) * sizeof (dfanode_t *));
	for (i=0; i<=ttbl->nstates; i++) {
		nodetable[i] = dfa_newnode ();
	}
	idfa = nodetable[0];

	if (dfa_idecodetrans_tbl (DA_PTR (ttbl->entries), DA_CUR (ttbl->entries), nodetable)) {
		/* failed to decode */
		sfree (nodetable);
		return NULL;
	}

	sfree (nodetable);

	if (idfa && ttbl->name) {
		dfa_setname (idfa, ttbl->name);
	}

	return idfa;
}
/*}}}*/
/*{{{  dfanode_t *dfa_decodetrans (char *rule, ...)*/
/*
 *	this decodes a DFA in a slightly different format
 */
dfanode_t *dfa_decodetrans (const char *rule, ...)
{
	dfanode_t *idfa = NULL;
	char *local = string_dup (rule);
	char **bits = split_string (local, 0);
	int i;
	va_list ap;
	char *name = NULL;
	dfanode_t **nodetable;
	int nstates;
	int adding = 0;
	void **fnptrtable = (void **)smalloc (64 * sizeof (void *));		/* XXX: limit */
	int nfnptrs = 0;

	/* check rule for sanity first */
	va_start (ap, rule);
	if (dfa_idecodetrans_check ((const char **)bits, &nstates, fnptrtable, &nfnptrs, ap, 1)) {
		idfa = NULL;
	} else {
		if (bits[0] && bits[1] && !strcmp (bits[1], "::=")) {
			name = bits[0];
		} else if (bits[0] && bits[1] && !strcmp (bits[1], "+:=")) {
			name = bits[0];
			adding = 1;
			if (!dfa_lookupbyname (name)) {
				nocc_error ("dfa_idecodetrans(): cannot add to non-existant name \"%s\"", name);
				/* get out */
				sfree (bits);
				sfree (local);
				sfree (fnptrtable);

				return NULL;
			}
		}

		/* okay, allocate states and actually do the decode */
#if 0
nocc_message ("dfa_idecodetrans(): for [%s], safe, adding = %d, nstates = %d", rule, adding, nstates);
#endif
		nodetable = (dfanode_t **)smalloc ((nstates + 1) * sizeof (dfanode_t *));
		for (i=0; i<=nstates; i++) {
			nodetable[i] = NULL;
		}
		if (adding) {
			/* match in existing states first */
			nodetable[0] = dfa_lookupbyname (name);
			dfa_idecodetrans_populate ((const char **)bits, nodetable, fnptrtable);
		}
		for (i=0; i<=nstates; i++) {
			if (!nodetable[i]) {
				nodetable[i] = dfa_newnode ();
			}
		}
		idfa = nodetable[0];

		nfnptrs = 0;
		dfa_idecodetrans_decode ((const char **)bits, nodetable, fnptrtable);

		sfree (nodetable);
	}
	va_end (ap);


	if (idfa && name) {
		dfa_setname (idfa, name);
	}

	sfree (bits);
	sfree (local);
	sfree (fnptrtable);

	return idfa;
}
/*}}}*/
 
/*{{{  static int dfa_compare_tables (dfattbl_t *first, dfattbl_t *second)*/
/*
 *	compares two tables for sorting;  alphabetical on name (anonymous ones end up first if there are any),
 *	then on operation (principle first, followed by modifications)
 */
static int dfa_compare_tables (dfattbl_t *first, dfattbl_t *second)
{
	int i;

	if (first == second) {
		return 0;
	}
	if ((!first && !second) || (first && second && !first->name && !second->name)) {
		return 0;
	} else if (!first || !first->name) {
		return -1;
	} else if (!second || !second->name) {
		return 1;
	}
	i = strcmp (first->name, second->name);
	if (!i) {
		/* same name */
		if (!first->op) {
			return -1;
		} else if (!second->op) {
			return 1;
		}
		return 0;
	}
	return i;
}
/*}}}*/
/*{{{  static int dfa_compare_entries (dfattblent_t *first, dfattblent_t *second)*/
/*
 *	compares two table entries for sorting; sorts by starting state
 */
static int dfa_compare_entries (dfattblent_t *first, dfattblent_t *second)
{
	return (first->s_state - second->s_state);
}
/*}}}*/
/*{{{  int dfa_mergetables (dfattbl_t **tables, int ntables)*/
/*
 *	processes a list of DFA transition tables, merging and sorting as it goes
 *	returns 0 on success, non-zero on error
 */
int dfa_mergetables (dfattbl_t **tables, int ntables)
{
	int i;
	int r = 0;
	dfattbl_t **tcopy;

	/*{{{  create a copy to work with*/
	tcopy = (dfattbl_t **)mem_ndup (tables, ntables * sizeof (dfattbl_t *));

	/*}}}*/
	/*{{{  sort tables and contents first*/
	da_qsort ((void **)tcopy, 0, ntables - 1, (int (*)(void *, void *))dfa_compare_tables);
#if 0
fprintf (stderr, "table order in tcopy:\n");
for (i=0; i<ntables; i++) {
fprintf (stderr, "    [%s] (%d)\n", tcopy[i]->name ?: "(anon)", tcopy[i]->op);
}
#endif
	for (i=0; i<ntables; i++) {
		if (tcopy[i]) {
			dynarray_qsort (tcopy[i]->entries, dfa_compare_entries);
		}
	}
	/*}}}*/
	/*{{{  find principles, do merges*/
	for (i=0; i<ntables; ) {
		dfattbl_t *prin = tcopy[i];
		int j;

		if (prin->op) {
			/* make it the principle definition for now */
			prin->op = 0;
			/*
			nocc_error ("dfa_mergetables(): missing principle definition for [%s]", prin->name);
			r = -1;
			goto mergetables_out_error;
			*/
		}
		i++;
		j = i;
		for (; (i<ntables) && tcopy[i]->op && !strcmp (prin->name, tcopy[i]->name); i++) {
			dfattbl_t *thisone = tcopy[i];
			int s_diff, x;
			int n;
			int rs_state, ns_state;
			int rs_idx, ns_idx;
			DYNARRAY (dfattblent_t *, working);

			if (strcmp (prin->name, thisone->name)) {
				nocc_error ("dfa_mergetables(): name mismatch [%s] and [%s]", prin->name, thisone->name);
				r = -1;
				goto mergetables_out_error;
			}

			/*{{{  first of all, renumber all states in "thisone" into "working", except state 0 to avoid collisions*/
			dynarray_init (working);
			s_diff = prin->nstates;
			for (x=0; x<DA_CUR (thisone->entries); x++) {
				dfattblent_t *tblent = dfa_dupttblent (DA_NTHITEM (thisone->entries, x));

				if (tblent->s_state > 0) {
					tblent->s_state += s_diff;
				}
				if (tblent->e_state > 0) {
					tblent->e_state += s_diff;
				}
				dynarray_add (working, tblent);
			}
			/*}}}*/
#if 0
fprintf (stderr, "dfa_mergetables(): re-numbered states in copy, got:\n  ");
{ int xi; for (xi=0; xi<DA_CUR(working); xi++) {
dfa_dumpttblent (stderr, DA_NTHITEM (working, xi));
}}
fprintf (stderr, "\n");
#endif
			/*{{{  step through both tables until merges diverge*/
			rs_state = 0, ns_state = 0;
			for (rs_idx = 0, ns_idx = 0; (rs_idx < DA_CUR (prin->entries)) && (ns_idx < DA_CUR (working)); ) {
				dfattblent_t *rs_ent = DA_NTHITEM (prin->entries, rs_idx);
				dfattblent_t *ns_ent = DA_NTHITEM (working, ns_idx);

				rs_state = rs_ent->s_state;
				ns_state = ns_ent->s_state;

#if 0
fprintf (stderr, "dfa_mergetables(): merge-loop: rs_ent=");
dfa_dumpttblent (stderr, rs_ent);
fprintf (stderr, ", ns_ent=");
dfa_dumpttblent (stderr, ns_ent);
fprintf (stderr, "\n");
#endif
				if ((rs_state != ns_state) || strcmp (rs_ent->match, ns_ent->match) || (rs_ent->reduce != ns_ent->reduce) || (rs_ent->rarg != ns_ent->rarg)) {
#if 0
fprintf (stderr, "dfa_mergetables(): merge-loop: mismatch.\n");
#endif
					break;
				}

				/* else, re-number target state in "working" to match target state in principle */
				{
					int old_state = ns_ent->e_state;
					int new_state = rs_ent->e_state;

#if 0
fprintf (stderr, "dfa_mergetables(): merge-loop: match, re-numbering target states.. (%d -> %d)\n", old_state, new_state);
#endif
					for (x=0; x<DA_CUR (working); x++) {
						dfattblent_t *went = DA_NTHITEM (working, x);

						if (went->e_state == old_state) {
							went->e_state = new_state;
						}
						if (went->s_state == old_state) {
							went->s_state = new_state;
						}
					}
				}

				/* remove ns_ent from "working" */
				dfa_freettblent (ns_ent);
				dynarray_delitem (working, ns_idx);

				/* move to next state (update rs_idx, ns_idx) */
				rs_state = rs_ent->e_state;				/* where we want to be */
				for (rs_idx++; rs_idx < DA_CUR (prin->entries); rs_idx++) {
					dfattblent_t *tblent = DA_NTHITEM (prin->entries, rs_idx);

					if (tblent->s_state == rs_state) {
						break;			/* for() */
					}
				}
				for (; ns_idx < DA_CUR (working); ns_idx++) {
					dfattblent_t *tblent = DA_NTHITEM (working, ns_idx);

					if (tblent->s_state == rs_state) {
						break;			/* for() */
					}
				}

				/* go round and try and match some more */
			}
			/*}}}*/
#if 0
fprintf (stderr, "dfa_mergetables(): finished walking.. got:\n  ");
{ int xi; for (xi=0; xi<DA_CUR(working); xi++) {
dfa_dumpttblent (stderr, DA_NTHITEM (working, xi));
}}
fprintf (stderr, "\n");
#endif
			/*{{{  copy across remaining states*/
			for (x=0; x<DA_CUR (working); x++) {
				dfattblent_t *tblent = DA_NTHITEM (working, x);

				dynarray_add (prin->entries, tblent);
			}
			prin->nstates += thisone->nstates;
			/*}}}*/
#if 0
dfa_dumpttbl (stderr, prin);
#endif

			dynarray_trash (working);
		}
	}
	/*}}}*/
#if 0
	/*{{{  debug: dump grammars*/
	for (i=0; i<ntables; i++) {
		dfa_dumpttbl (stderr, tcopy[i]);
	}
	/*}}}*/
#endif
mergetables_out_error:
	/*{{{  free working copy*/
	sfree (tcopy);
	/*}}}*/

	return r;
}
/*}}}*/
/*{{{  int dfa_clear_deferred (void)*/
/*
 *	clears out any/all deferred matches, without error
 *	returns 0 if blank anyway, non-zero otherwise
 */
int dfa_clear_deferred (void)
{
	int i;
	int r = 0;

	for (i=0; i<DA_CUR (defmatches); i++) {
		deferred_match_t *dmatch = DA_NTHITEM (defmatches, i);

		if (dmatch) {
			r++;
			dfa_freedefmatch (dmatch);
		}
	}
	dynarray_trash (defmatches);
	dynarray_init (defmatches);

	return r;
}
/*}}}*/
/*{{{  int dfa_match_deferred (void)*/
/*
 *	process deferred matches in the various DFAs
 *	returns 0 on success, non-zero on failure
 */
int dfa_match_deferred (void)
{
	int i;

	/* XXX: assuming in resolvable order.. */

	for (i=0; i<DA_CUR (defmatches); i++) {
		deferred_match_t *dmatch = DA_NTHITEM (defmatches, i);
		dfanode_t *inode = dmatch->inode;
		int j;

		/* find relevant match in the DFA and remove it */
		for (j=0; j<DA_CUR (inode->match); j++) {
			token_t *thistok = DA_NTHITEM (inode->match, j);

			if (thistok == dmatch->match) {
				break;		/* for() */
			}
		}
		if (j == DA_CUR (inode->match)) {
			nocc_internal ("dfa_match_deferred(): deferred match for [%s] not in node 0x%8.8x(%s)\n", dmatch->match->u.str.ptr,
					(unsigned int)inode, inode->dfainfo ? ((nameddfa_t *)inode->dfainfo)->name : "?");
			return -1;
		}

		dynarray_delitem (inode->match, j);
		dynarray_delitem (inode->target, j);
		dynarray_delitem (inode->pushto, j);
		dynarray_delitem (inode->flags, j);

#if 0
fprintf (stderr, "dfa_match_deferred(): resolving [%s] in [%s]\n", dmatch->match->u.str.ptr, inode->dfainfo ? ((nameddfa_t *)inode->dfainfo)->name : "?");
#endif
		/* put in initial matches */
		dfa_matchpush (dmatch->inode, dmatch->match->u.str.ptr, dmatch->enode, 0);

		/* free this match structure */
		lexer_freetoken (dmatch->match);
		dfa_freedefmatch (dmatch);
		DA_SETNTHITEM (defmatches, i, NULL);
	}
	dynarray_trash (defmatches);

	return 0;
}
/*}}}*/
/*{{{  void dfa_dumpdeferred (FILE *stream)*/
/*
 *	dumps deferred matches (debugging)
 */
void dfa_dumpdeferred (FILE *stream)
{
	int i;

	fprintf (stream, "dfa_dumpdeferred(): %d deferred matches:\n", DA_CUR (defmatches));
	for (i=0; i<DA_CUR (defmatches); i++) {
		deferred_match_t *dmatch = DA_NTHITEM (defmatches, i);

		fprintf (stream, "%2d: 0x%8.8x(%s) --[%s]--> 0x%8.8x(%s)\n", i,
				(unsigned int)dmatch->inode, dmatch->inode->dfainfo ? ((nameddfa_t *)dmatch->inode->dfainfo)->name : "?",
				dmatch->match->u.str.ptr ?: "(null)",
				(unsigned int)dmatch->enode, dmatch->enode->dfainfo ? ((nameddfa_t *)dmatch->enode->dfainfo)->name : "?");
	}
	return;
}
/*}}}*/



/*{{{  int dfa_advance (dfastate_t *dfast, parsepriv_t *pp, token_t *tok)*/
/*
 *	advances the DFA a single state from the given token
 *	returns number of actions (either shifts or reductions), or -1 on error
 */
int dfa_advance (dfastate_t **dfast, parsepriv_t *pp, token_t *tok)
{
	int i;
	dfanode_t *cnode = (*dfast)->cur;
	char *nodename = (cnode->dfainfo ? ((nameddfa_t *)cnode->dfainfo)->name : "?");

	if (!cnode) {
		nocc_internal ("dfa_advance(): NULL position in DFA!");
		return -1;
	}
	if (compopts.debugparser) {
		nocc_message ("dfa_advance(): dfa->cur = 0x%8.8x  TS=%d  NS=%d  RES=0x%8.8x  [%s]", (unsigned int)cnode, DA_CUR (pp->tokstack), DA_CUR ((*dfast)->nodestack),
				(unsigned int)((*dfast)->local), (cnode->dfainfo ? ((nameddfa_t *)(cnode->dfainfo))->name : "??"));
	}
	/* check for a matched exit transition */
	for (i=0; i<DA_CUR (cnode->match); i++) {
		token_t *thistok = DA_NTHITEM (cnode->match, i);

		if (lexer_tokmatch (thistok, tok)) {
			/* match */
			break;
		}
	}
	if (i == DA_CUR (cnode->match)) {
		/* could not advance out of this state! */
		parser_error (pp->lf, "dfa_advance(): stuck in [%s], expecting:", nodename);
		for (i=0; i<DA_CUR (cnode->match); i++) {
			token_t *thistok = DA_NTHITEM (cnode->match, i);

			if (thistok) {
				lexer_dumptoken (stderr, thistok);
			}
		}
		parser_error (pp->lf, "dfa_advance(): got:");
		lexer_dumptoken (stderr, tok);
		return 0;
	} else {
		/* otherwise do something */
		int flags = DA_NTHITEM (cnode->flags, i);
		dfanode_t *target = DA_NTHITEM (cnode->target, i);

		if (flags & DFAFLAG_PUSHSTACK) {
			/*{{{  push the DFA stack*/
			if (compopts.debugparser) {
				nocc_message ("dfa_advance(): push from 0x%8.8x return to 0x%8.8x", (unsigned int)((*dfast)->cur), (unsigned int)target);
			}
			(*dfast)->cur = target;		/* so we go back to the right place */
			*dfast = dfa_newstate (*dfast);
			target = DA_NTHITEM (cnode->pushto, i);
			if (!target) {
				nocc_internal ("dfa_advance(): push to nowhere!");
				return -1;
			}
			/*}}}*/
		}

		if (flags & DFAFLAG_NOCONSUME) {
			/* push this token back into the lexer */
			lexer_pushback (tok->origin, tok);
		} else if (flags & DFAFLAG_KEEP) {
			/* save this on the parser's token-stack */
			parser_pushtok (pp, tok);
		} else {
			/* just matching, free it */
			lexer_freetoken (tok);
		}

		if (target) {
			/* do transition */
			(*dfast)->cur = target;
		} else {
			/*{{{  pop the DFA stack*/
			if ((*dfast)->prev) {
				tnode_t *result;

				/* good, safe -- nodestack must be empty */
				if (DA_CUR ((*dfast)->nodestack)) {
					nocc_error ("dfa_advance(): pop with %d nodes on nodestack", DA_CUR ((*dfast)->nodestack));
					return -1;
				}
				result = (*dfast)->local;
				(*dfast)->local = NULL;
				(*dfast)->ptr = &((*dfast)->local);

				/* pop */
				*dfast = (*dfast)->prev;
				dynarray_add ((*dfast)->nodestack, result);
				target = (*dfast)->cur;
				if (compopts.debugparser) {
					nocc_message ("dfa_advance(): pop to 0x%8.8x.  result = 0x%8.8x", (unsigned int)((*dfast)->cur), (unsigned int)result);
				}
			} else {
				/* this means we hit the top-level */
				(*dfast)->cur = NULL;
			}
			/*}}}*/
		}

		/* reduction ? */
		if (target && target->reduce) {
			if (compopts.debugparser) {
				nocc_message ("dfa_advance(): reducing = 0x%8.8x  TS=%d  NS=%d  RES=0x%8.8x  [%s]", (unsigned int)target, DA_CUR (pp->tokstack), DA_CUR ((*dfast)->nodestack),
						(unsigned int)((*dfast)->local), (target->dfainfo ? ((nameddfa_t *)(target->dfainfo))->name : "??"));
			}
			target->reduce (*dfast, pp, target->rarg);
			if (compopts.debugparser) {
				nocc_message ("dfa_advance(): reduced. = 0x%8.8x  TS=%d  NS=%d  RES=0x%8.8x  [%s]", (unsigned int)target, DA_CUR (pp->tokstack), DA_CUR ((*dfast)->nodestack),
						(unsigned int)((*dfast)->local), (target->dfainfo ? ((nameddfa_t *)(target->dfainfo))->name : "??"));
			}
		}
	}
	return 1;
}
/*}}}*/
/*{{{  void dfa_pushnode (dfastate_t *dfast, tnode_t *node)*/
/*
 *	pushes a node onto the nodestack
 */
void dfa_pushnode (dfastate_t *dfast, tnode_t *node)
{
	if (!dfast) {
		nocc_internal ("dfa_pushnode(): NULL state!");
		return;
	}
	dynarray_add (dfast->nodestack, node);
	return;
}
/*}}}*/
/*{{{  tnode_t *dfa_popnode (dfastate_t *dfast)*/
/*
 *	removes a node from the nodestack
 */
tnode_t *dfa_popnode (dfastate_t *dfast)
{
	tnode_t *node;

	if (!dfast) {
		nocc_internal ("dfa_popnode(): NULL state!");
		return NULL;
	}
	if (!DA_CUR (dfast->nodestack)) {
		if (compopts.debugparser) {
			nocc_error ("DFA error: dfast->cur = 0x%8.8x  [%s]  NS=%d", (unsigned int)(dfast->cur),
					(dfast->cur && dfast->cur->dfainfo) ? ((nameddfa_t *)(dfast->cur->dfainfo))->name : "??",
					DA_CUR (dfast->nodestack));
		}
		nocc_internal ("dfa_popnode(): no node to pop");
		return NULL;
	}
	node = DA_NTHITEM (dfast->nodestack, DA_CUR (dfast->nodestack) - 1);
	dynarray_delitem (dfast->nodestack, DA_CUR (dfast->nodestack) - 1);

	return node;
}
/*}}}*/


/*{{{  tnode_t *dfa_walk (char *rname, lexfile_t *lf)*/
/*
 *	walks the DFA with the specified name with tokens
 *	from the given source.
 *	returns the resulting tree, or NULL on error (reported)
 */
tnode_t *dfa_walk (char *rname, lexfile_t *lf)
{
	token_t *tok;
	dfastate_t *dfast;
	tnode_t *tree;
	dfanode_t *idfa;
	parsepriv_t *pp = (parsepriv_t *)(lf->ppriv);

	idfa = dfa_lookupbyname (rname);
	if (!idfa) {
		parser_error (lf, "dfa_walk(): no such DFA [%s]!", rname);
		return NULL;
	}

	dfast = dfa_newstate (NULL);
	dfast->cur = idfa;

	tok = lexer_nexttoken (lf);
	while (tok && (tok->type != END) && (dfast->cur)) {
		int i;

		i = dfa_advance (&dfast, pp, tok);
		if (i < 0) {
			tok = NULL;
			break;
		} else if (!i) {
			/* DFA got stuck */
			lexer_pushback (lf, tok);
			tok = NULL;
			break;
		}
		tok = lexer_nexttoken (lf);
	}
	if (tok) {
		lexer_pushback (tok->origin, tok);
		tok = NULL;
	}

	/* should be left with a single something */
	if (dfast->prev) {
		parser_error (lf, "parse error");
		if (dfast->local) {
			tnode_free (dfast->local);
			dfast->local = NULL;
			dfast->ptr = &(dfast->local);
		}
	}

	/* token-stack and node-stack should be empty */
	if (DA_CUR (pp->tokstack)) {
		parser_error (lf, "%d leftover tokens on stack:", DA_CUR (pp->tokstack));
		while (DA_CUR (pp->tokstack)) {
			token_t *thistok = DA_NTHITEM (pp->tokstack, 0);

			if (thistok) {
				lexer_dumptoken (stderr, thistok);
				lexer_freetoken (thistok);
				DA_SETNTHITEM (pp->tokstack, 0, NULL);
			}
			dynarray_delitem (pp->tokstack, 0);
		}
	}
	if (DA_CUR (dfast->nodestack)) {
		parser_error (lf, "%d leftover nodes on stack:", DA_CUR (dfast->nodestack));
		while (DA_CUR (dfast->nodestack)) {
			tnode_t *thisnode = DA_NTHITEM (dfast->nodestack, 0);

			if (thisnode) {
				tnode_dumptree (thisnode, 1, stderr);
				tnode_free (thisnode);
				DA_SETNTHITEM (dfast->nodestack, 0, NULL);
			}
			dynarray_delitem (dfast->nodestack, 0);
		}
	}

	tree = dfast->local;
	dfast->local = NULL;
	dfast->ptr = &(dfast->local);

	dfa_freestate (dfast);

	return tree;
}
/*}}}*/


/*{{{  dfastate_t *dfa_newstate (dfastate_t *prev)*/
/*
 *	creates a new dfastate_t structure
 */
dfastate_t *dfa_newstate (dfastate_t *prev)
{
	dfastate_t *dfast;

	dfast = (dfastate_t *)smalloc (sizeof (dfastate_t));
	dfast->prev = prev;
	dfast->cur = (prev ? prev->cur : NULL);
	dfast->local = NULL;
	dfast->ptr = &(dfast->local);
	dynarray_init (dfast->nodestack);

	return dfast;
}
/*}}}*/
/*{{{  dfastate_t *dfa_newstate_init (dfastate_t *prev, char *iname)*/
/*
 *	creates a new dfastate_t structure and initialises to
 *	a particular DFA node
 */
dfastate_t *dfa_newstate_init (dfastate_t *prev, char *iname)
{
	dfastate_t *dfast;

	dfast = dfa_newstate (prev);
	dfast->cur = dfa_lookupbyname (iname);
	if (!dfast->cur) {
		nocc_internal ("dfa_newstate_init(): unknown DFA [%s]", iname);
		return NULL;
	}
	return dfast;
}
/*}}}*/
/*{{{  void dfa_freestate (dfastate_t *dfast)*/
/*
 *	frees a dfastate_t structure
 */
void dfa_freestate (dfastate_t *dfast)
{
	int i;

	if (!dfast) {
		nocc_internal ("dfa_freestate(): NULL state");
		return;
	}
	/* clean up the nodestack */
	for (i=0; i<DA_CUR (dfast->nodestack); i++) {
		tnode_t *thisnode = DA_NTHITEM (dfast->nodestack, i);

		if (thisnode) {
			tnode_free (thisnode);
		}
	}
	dynarray_trash (dfast->nodestack);

	sfree (dfast);
	return;
}
/*}}}*/



