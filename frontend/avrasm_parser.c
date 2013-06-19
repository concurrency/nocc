/*
 *	avrasm_parser.c -- AVR assembler parser for nocc
 *	Copyright (C) 2012-2013 Fred Barnes <frmb@kent.ac.uk>
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
#include "lexer.h"
#include "lexpriv.h"
#include "tnode.h"
#include "parser.h"
#include "fcnlib.h"
#include "dfa.h"
#include "parsepriv.h"
#include "avrasm.h"
#include "library.h"
#include "feunit.h"
#include "names.h"
#include "prescope.h"
#include "scope.h"
#include "prescope.h"
#include "typecheck.h"
#include "extn.h"
#include "langdef.h"
#include "opts.h"


/*}}}*/
/*{{{  forward decls*/
static int avrasm_parser_init (lexfile_t *lf);
static void avrasm_parser_shutdown (lexfile_t *lf);
static tnode_t *avrasm_parser_parse (lexfile_t *lf);
static int avrasm_parser_prescope (tnode_t **tptr, prescope_t *ps);
static int avrasm_parser_scope (tnode_t **tptr, scope_t *ss);
static int avrasm_parser_typecheck (tnode_t *tptr, typecheck_t *tc);
static int avrasm_parser_typeresolve (tnode_t **tptr, typecheck_t *tc);

static tnode_t *avrasm_parser_parsemacrodef (lexfile_t *lf);
static tnode_t *avrasm_parser_parsefunctiondef (lexfile_t *lf);
static tnode_t *avrasm_parser_parsehllif (lexfile_t *lf, tnode_t *firstcond);

struct TAG_llscope;

static int avrasm_do_llscope (tnode_t **tptr, struct TAG_llscope *lls);

/*}}}*/
/*{{{  global vars*/

avrasm_pset_t avrasm;

langparser_t avrasm_parser = {
	.langname =		"avrasm",
	.init =			avrasm_parser_init,
	.shutdown =		avrasm_parser_shutdown,
	.parse =		avrasm_parser_parse,
	.descparse =		NULL, // avrasm_parser_descparse,
	.prescope =		avrasm_parser_prescope,
	.scope =		avrasm_parser_scope,
	.typecheck =		avrasm_parser_typecheck,
	.typeresolve =		avrasm_parser_typeresolve,
	.postcheck =		NULL,
	.fetrans =		NULL,
	.getlangdef =		avrasm_getlangdef,
	.getlanglibs =		NULL,
	.maketemp =		NULL,
	.makeseqassign =	NULL,
	.makeseqany =		NULL,
	.tagstruct_hook =	(void *)&avrasm,
	.lexer =		NULL
};


/*}}}*/
/*{{{  private types/vars*/
typedef struct {
	dfanode_t *inode;
	langdef_t *ldef;
} avrasm_parse_t;

typedef struct {
	name_t *name;				/* associated name-node (LLABELDEF linked if set) */
	DYNARRAY (tnode_t **, frefs);		/* forward references (when references "0f" are seen before the label) */
} llscope_entry_t;

typedef struct TAG_llscope {
	DYNARRAY (llscope_entry_t *, flabels);
	DYNARRAY (llscope_entry_t *, blabels);
	int error;
} llscope_t;

static avrasm_parse_t *avrasm_priv = NULL;
static int avrasm_priv_refcount = 0;

static feunit_t *feunit_set[] = {
	&avrasm_program_feunit,
	&avrasm_hll_feunit,
	NULL
};

static chook_t *label_chook = NULL;

/*}}}*/


/*{{{  static int avrasm_opthandler_stopat (cmd_option_t *opt, char ***argwalk, int *argleft)*/
/*
 *	option handler for AVR assembler "stop" options
 */
static int avrasm_opthandler_stopat (cmd_option_t *opt, char ***argwalk, int *argleft)
{
	compopts.stoppoint = (int)(opt->arg);
#if 1
fprintf (stderr, "avrasm_opthandler_stopat(): setting stop point to %d\n", compopts.stoppoint);
#endif
	return 0;
}
/*}}}*/

/*{{{  static avrasm_parse_t *avrasm_newavrasmparse (void)*/
/*
 *	creates a new avrasm_parse_t structure
 */
static avrasm_parse_t *avrasm_newavrasmparse (void)
{
	avrasm_parse_t *avrp = (avrasm_parse_t *)smalloc (sizeof (avrasm_parse_t));

	avrp->inode = NULL;
	avrp->ldef = NULL;

	return avrp;
}
/*}}}*/
/*{{{  static void avrasm_freeavrasmparse (avrasm_parse_t *avrp)*/
/*
 *	frees an avrasm_parse_t structure
 */
static void avrasm_freeavrasmparse (avrasm_parse_t *avrp)
{
	if (!avrp) {
		nocc_warning ("avrasm_freeavrasmparse(): NULL pointer!");
		return;
	}
	if (avrp->ldef) {
		langdef_freelangdef (avrp->ldef);
		avrp->ldef = NULL;
	}
	/* leave inode */
	avrp->inode = NULL;
	sfree (avrp);

	return;
}
/*}}}*/

/*{{{  void avrasm_isetindent (fhandle_t *stream, int indent)*/
/*
 *	set-indent for debugging output
 */
void avrasm_isetindent (fhandle_t *stream, int indent)
{
	int i;

	for (i=0; i<indent; i++) {
		fhandle_printf (stream, "    ");
	}
	return;
}
/*}}}*/
/*{{{  langdef_t *avrasm_getlangdef (void)*/
/*
 *	returns the current language definitions, NULL on failure.
 */
langdef_t *avrasm_getlangdef (void)
{
	if (!avrasm_priv) {
		return NULL;
	}
	return avrasm_priv->ldef;
}
/*}}}*/
/*{{{  int avrasm_langop_inseg (tnode_t *node)*/
/*
 *	decides whether a particular node should be inside a segment in the assembler (instructions, org, constant data, vars, etc.)
 *	returns truth value
 */
int avrasm_langop_inseg (tnode_t *node)
{
	if (!node->tag->ndef->lops || !tnode_haslangop (node->tag->ndef->lops, "avrasm_inseg")) {
		return 0;
	}
	return (int)tnode_calllangop (node->tag->ndef->lops, "avrasm_inseg", 1, node);
}
/*}}}*/

/*{{{  label_chook_t *avrasm_newlabelchook (void)*/
/*
 *	creates a new label_chook_t structure
 */
label_chook_t *avrasm_newlabelchook (void)
{
	label_chook_t *lch = (label_chook_t *)smalloc (sizeof (label_chook_t));

	lch->zone = NULL;
	lch->addr = 0;

	return lch;
}
/*}}}*/
/*{{{  void avrasm_freelabelchook (label_chook_t *lch)*/
/*
 *	frees a label_chook_t structure
 */
void avrasm_freelabelchook (label_chook_t *lch)
{
	if (!lch) {
		nocc_serious ("avrasm_freelabelchook(): NULL pointer!");
		return;
	}
	sfree (lch);
	return;
}
/*}}}*/

/*{{{  static llscope_entry_t *new_llscopeentry (void)*/
/*
 *	creates a new llscope_entry_t structure
 */
static llscope_entry_t *new_llscopeentry (void)
{
	llscope_entry_t *llse = (llscope_entry_t *)smalloc (sizeof (llscope_entry_t));

	llse->name = NULL;
	dynarray_init (llse->frefs);

	return llse;
}
/*}}}*/
/*{{{  static void free_llscopeentry (llscope_entry_t *llse)*/
/*
 *	frees an llscope_entry_t structure
 */
static void free_llscopeentry (llscope_entry_t *llse)
{
	if (!llse) {
		nocc_serious ("free_llscopeentry(): NULL pointer!");
		return;
	}
	dynarray_trash (llse->frefs);
	llse->name = NULL;
	sfree (llse);

	return;
}
/*}}}*/
/*{{{  static llscope_t *new_llscope (void)*/
/*
 *	creates a new llscope_t structure
 */
static llscope_t *new_llscope (void)
{
	llscope_t *lls = (llscope_t *)smalloc (sizeof (llscope_t));

	dynarray_init (lls->flabels);
	dynarray_init (lls->blabels);
	lls->error = 0;

	return lls;
}
/*}}}*/
/*{{{  static void free_llscope (llscope_t *lls)*/
/*
 *	frees an llscope_t structure
 */
static void free_llscope (llscope_t *lls)
{
	int i;

	if (!lls) {
		nocc_serious ("free_llscope(): NULL pointer!");
		return;
	}
	for (i=0; i<DA_CUR (lls->flabels); i++) {
		llscope_entry_t *llse = DA_NTHITEM (lls->flabels, i);

		if (llse) {
			free_llscopeentry (llse);
		}
	}
	for (i=0; i<DA_CUR (lls->blabels); i++) {
		llscope_entry_t *llse = DA_NTHITEM (lls->blabels, i);

		if (llse) {
			free_llscopeentry (llse);
		}
	}
	dynarray_trash (lls->flabels);
	dynarray_trash (lls->blabels);
	sfree (lls);
	return;
}
/*}}}*/
/*{{{  static llscope_entry_t *llscope_lookup_fordef (llscope_t *lls, int labid)*/
/*
 *	returns pointer to specified local label if it exists in "forward" labels, creates it there otherwise
 */
static llscope_entry_t *llscope_lookup_fordef (llscope_t *lls, int labid)
{
	if (labid >= DA_CUR (lls->flabels)) {
		int s = DA_CUR (lls->flabels);
		int i;

		dynarray_setsize (lls->flabels, labid + 1);
		for (i=s; i<labid; i++) {
			DA_SETNTHITEM (lls->flabels, i, NULL);
		}
	}
	if (!DA_NTHITEM (lls->flabels, labid)) {
		llscope_entry_t *llse = new_llscopeentry ();

		DA_SETNTHITEM (lls->flabels, labid, llse);
	}
	return DA_NTHITEM (lls->flabels, labid);
}
/*}}}*/
/*{{{  static llscope_entry_t *llscope_lookup_forref (llscope_t *lls, int labid)*/
/*
 *	returns pointer to a specified local label if it exists in "forward" labels, creates it there otherwise
 */
static llscope_entry_t *llscope_lookup_forref (llscope_t *lls, int labid)
{
	if (labid >= DA_CUR (lls->flabels)) {
		int s = DA_CUR (lls->flabels);
		int i;

		dynarray_setsize (lls->flabels, labid + 1);
		for (i=s; i<labid; i++) {
			DA_SETNTHITEM (lls->flabels, i, NULL);
		}
	}
	if (!DA_NTHITEM (lls->flabels, labid)) {
		llscope_entry_t *llse = new_llscopeentry ();

		DA_SETNTHITEM (lls->flabels, labid, llse);
	}
	return DA_NTHITEM (lls->flabels, labid);
}
/*}}}*/
/*{{{  static llscope_entry_t *llscope_lookup_backref (llscope_t *lls, int labid)*/
/*
 *	returns a pointer to a specified local label if it exists in "backwards" labels, NULL otherwise
 */
static llscope_entry_t *llscope_lookup_backref (llscope_t *lls, int labid)
{
	if (labid >= DA_CUR (lls->blabels)) {
		return NULL;
	}
	return DA_NTHITEM (lls->blabels, labid);
}
/*}}}*/
/*{{{  static void llscope_promote (llscope_t *lls, int labid)*/
/*
 *	promotes a local label from "forward" to "backward", after its definition is seen
 */
static void llscope_promote (llscope_t *lls, int labid)
{
	if (labid >= DA_CUR (lls->blabels)) {
		int s = DA_CUR (lls->blabels);
		int i;

		dynarray_setsize (lls->blabels, labid + 1);
		for (i=s; i<labid; i++) {
			DA_SETNTHITEM (lls->blabels, i, NULL);
		}
	}
	if ((labid >= DA_CUR (lls->flabels)) || (!DA_NTHITEM (lls->flabels, labid))) {
		nocc_internal ("llscope_promote(): local label id %d not in flabels (max %d)!", labid, DA_CUR (lls->flabels));
		return;
	}
	if (DA_NTHITEM (lls->blabels, labid)) {
		free_llscopeentry (DA_NTHITEM (lls->blabels, labid));
	}
	DA_SETNTHITEM (lls->blabels, labid, DA_NTHITEM (lls->flabels, labid));
	DA_SETNTHITEM (lls->flabels, labid, NULL);

	return;
}
/*}}}*/

/*{{{  static int subequ_modprewalk (tnode_t **tptr, void *arg)*/
/*
 *	called for each node walked during the 'subequ' pass
 *	returns 0 to stop walk, 1 to continue
 */
static int subequ_modprewalk (tnode_t **tptr, void *arg)
{
	subequ_t *se = (subequ_t *)arg;
	int i = 1;

	if (*tptr && (*tptr)->tag->ndef->ops && tnode_hascompop ((*tptr)->tag->ndef->ops, "subequ")) {
		i = tnode_callcompop ((*tptr)->tag->ndef->ops, "subequ", 2, tptr, se);
	}
	return i;
}
/*}}}*/
/*{{{  static int submacro_modprewalk (tnode_t **tptr, void *arg)*/
/*
 *	called on each node walked during the 'submacro' pass
 *	returns 0 to stop walk, 1 to continue
 */
static int submacro_modprewalk (tnode_t **tptr, void *arg)
{
	submacro_t *sm = (submacro_t *)arg;
	int i = 1;

	if (*tptr && (*tptr)->tag->ndef->ops && tnode_hascompop ((*tptr)->tag->ndef->ops, "submacro")) {
		i = tnode_callcompop ((*tptr)->tag->ndef->ops, "submacro", 2, tptr, sm);
	}
	return i;
}
/*}}}*/
/*{{{  static int hlltypecheck_modprewalk (tnode_t **tptr, void *arg)*/
/*
 *	called on each node walked during the 'hlltypecheck' pass
 *	returns 0 to stop walk, 1 to continue
 */
static int hlltypecheck_modprewalk (tnode_t **tptr, void *arg)
{
	hlltypecheck_t *hltc = (hlltypecheck_t *)arg;
	int i = 1;

	if (*tptr && (*tptr)->tag->ndef->ops && tnode_hascompop ((*tptr)->tag->ndef->ops, "hlltypecheck")) {
		i = tnode_callcompop ((*tptr)->tag->ndef->ops, "hlltypecheck", 2, tptr, hltc);
	}
	return i;
}
/*}}}*/
/*{{{  static int hllsimplify_modprewalk (tnode_t **tptr, void *arg)*/
/*
 *	called on each node walked during the 'hllsimplify' pass
 *	returns 0 to stop walk, 1 to continue
 */
static int hllsimplify_modprewalk (tnode_t **tptr, void *arg)
{
	hllsimplify_t *hls = (hllsimplify_t *)arg;
	int i = 1;

	if (*tptr && (*tptr)->tag->ndef->ops && tnode_hascompop ((*tptr)->tag->ndef->ops, "hllsimplify")) {
		i = tnode_callcompop ((*tptr)->tag->ndef->ops, "hllsimplify", 2, tptr, hls);
	}
	return i;
}
/*}}}*/

/*{{{  int avrasm_subequ_subtree (tnode_t **tptr, subequ_t *se)*/
/*
 *	does .equ and .def substitution on a parse-tree (already scoped)
 *	returns 0 on success, non-zero on failure
 */
int avrasm_subequ_subtree (tnode_t **tptr, subequ_t *se)
{
	if (!tptr) {
		nocc_serious ("avrasm_subequ_subtree(): NULL tree-pointer");
		return 1;
	} else if (!*tptr) {
		return 0;
	} else {
		tnode_modprewalktree (tptr, subequ_modprewalk, (void *)se);
	}
	return se->errcount;
}
/*}}}*/
/*{{{  int avrasm_submacro_subtree (tnode_t **tptr, submacro_t *sm)*/
/*
 *	does macro substitution on a parse-tree (already scoped)
 *	returns 0 on success, non-zero on failure
 */
int avrasm_submacro_subtree (tnode_t **tptr, submacro_t *sm)
{
	if (!tptr) {
		nocc_serious ("avrasm_submacro_subtree(): NULL tree-pointer");
		return 1;
	} else if (!*tptr) {
		return 0;
	} else {
		tnode_modprewalktree (tptr, submacro_modprewalk, (void *)sm);
	}
	return sm->errcount;
}
/*}}}*/
/*{{{  int avrasm_hlltypecheck_subtree (tnode_t **tptr, hlltypecheck_t *hltc)*/
/*
 *	does high-level type-checks on a parse-tree
 *	returns 0 on success, non-zero on failure
 */
int avrasm_hlltypecheck_subtree (tnode_t **tptr, hlltypecheck_t *hltc)
{
	if (!tptr) {
		nocc_serious ("avrasm_hlltypecheck_subtree(): NULL tree-pointer");
		return 1;
	} else if (!*tptr) {
		return 0;
	} else {
		tnode_modprewalktree (tptr, hlltypecheck_modprewalk, (void *)hltc);
	}
	return hltc->errcount;
}
/*}}}*/
/*{{{  int avrasm_hllsimplify_subtree (tnode_t **tptr, hllsimplify_t *hls)*/
/*
 *	does high-level simplification on a parse-tree
 *	returns 0 on success, non-zero on failure
 */
int avrasm_hllsimplify_subtree (tnode_t **tptr, hllsimplify_t *hls)
{
	if (!tptr) {
		nocc_serious ("avrasm_hllsimplify_subtree(): NULL tree-pointer");
		return 1;
	} else if (!*tptr) {
		return 0;
	} else if (parser_islistnode (*tptr)) {
		int i;
		tnode_t *saved_cxt = hls->list_cxt;		/* save context for later */
		int saved_itm = hls->list_itm;

		hls->list_cxt = *tptr;
		for (i=0; i<parser_countlist (*tptr); i++) {
			tnode_t **nodep = parser_getfromlistptr (*tptr, i);

			hls->list_itm = i;
			tnode_modprewalktree (nodep, hllsimplify_modprewalk, (void *)hls);
		}

		hls->list_itm = saved_itm;
		hls->list_cxt = saved_cxt;

		parser_collapselist (*tptr);
	} else {
		tnode_modprewalktree (tptr, hllsimplify_modprewalk, (void *)hls);
	}
	return hls->errcount;
}
/*}}}*/

/*{{{  static int llscope_modprewalk (tnode_t **tptr, void *arg)*/
/*
 *	called to each node walked during the 'llscope' pass
 *	returns 0 to stop walk, 1 to continue
 */
static int llscope_modprewalk (tnode_t **tptr, void *arg)
{
	int i = 1;

	if (*tptr && (*tptr)->tag->ndef->ops && tnode_hascompop ((*tptr)->tag->ndef->ops, "llscope")) {
		i = tnode_callcompop ((*tptr)->tag->ndef->ops, "llscope", 2, tptr, arg);
	}
	return i;
}
/*}}}*/
/*{{{  static int avrasm_llscope_subtree (tnode_t **tptr, llscope_t *lls)*/
/*
 *	does local-label scoping on a parse-tree (small bits of)
 *	returns 0 on success, non-zero on failure
 */
static int avrasm_llscope_subtree (tnode_t **tptr, llscope_t *lls)
{
	if (!tptr) {
		nocc_serious ("avrasm_llscope_subtree(): NULL tree-pointer");
		return 1;
	} else if (!*tptr) {
		return 0;
	} else {
		tnode_modprewalktree (tptr, llscope_modprewalk, (void *)lls);
	}
	return 0;
}
/*}}}*/
/*{{{  tnode_t *avrasm_llscope_fixref (tnode_t **tptr, int labid, int labdir, void *llsptr)*/
/*
 *	called to fixup a local-label reference when encountered.
 *	returns new (or same) node.
 */
tnode_t *avrasm_llscope_fixref (tnode_t **tptr, int labid, int labdir, void *llsptr)
{
	llscope_t *lls = (llscope_t *)llsptr;
	llscope_entry_t *llse;

	if (labdir > 0) {
		/* forward reference */
		llse = llscope_lookup_forref (lls, labid);
		if (llse->name) {
			tnode_error (*tptr, "impossible forward local-label reference, already declared!");
			lls->error++;
			return NULL;
		}
		dynarray_add (llse->frefs, tptr);
	} else {
		/* backward reference */
		llse = llscope_lookup_backref (lls, labid);
		if (!llse) {
			tnode_error (*tptr, "backward reference to undefined local-label %d", labid);
			lls->error++;
			return NULL;
		}
		if (!llse->name) {
			nocc_internal ("impossible backward local-label reference, no name defined for it!");
			lls->error++;
			return NULL;
		}
		*tptr = tnode_createfrom (avrasm.tag_LLABEL, *tptr, llse->name);
	}
	return *tptr;
}
/*}}}*/
/*{{{  int avrasm_ext_llscope_subtree (tnode_t **tptr, void *llsptr)*/
/*
 *	called from elsewhere to process closed-scope local-labels (e.g. inside high-level function)
 *	returns 0 on success, non-zero on failure
 */
int avrasm_ext_llscope_subtree (tnode_t **tptr, void *llsptr)
{
	llscope_t *lls = (llscope_t *)llsptr;
	int r;

	r = avrasm_do_llscope (tptr, lls);

	return r;
}
/*}}}*/

/*{{{  static int subequ_cpass (tnode_t **treeptr)*/
/*
 *	called to do the compiler-pass for substituting .equ and .def directives
 *	returns 0 on success, non-zero on failure
 */
static int subequ_cpass (tnode_t **treeptr)
{
	subequ_t *se = (subequ_t *)smalloc (sizeof (subequ_t));
	int r;

	se->errcount = 0;
	avrasm_subequ_subtree (treeptr, se);
	r = se->errcount;
	sfree (se);

	return r;
}
/*}}}*/
/*{{{  static int submacro_cpass (tnode_t **treeptr)*/
/*
 *	called to do the compiler-pass for substituting macro definitions
 *	returns 0 on successs, non-zero on failure
 */
static int submacro_cpass (tnode_t **treeptr)
{
	submacro_t *sm = (submacro_t *)smalloc (sizeof (submacro_t));
	int r;

	sm->errcount = 0;
	avrasm_submacro_subtree (treeptr, sm);
	r = sm->errcount;
	sfree (sm);

	/* flatten out any substituted macros in the whole */
	parser_collapselist (*treeptr);

	return r;
}
/*}}}*/
/*{{{  static int llscope_cpass (tnode_t **treeptr)*/
/*
 *	called to do local-label scoping (compiler-pass)
 *	returns 0 on success, non-zero on failure
 */
static int llscope_cpass (tnode_t **treeptr)
{
	llscope_t *lls = new_llscope ();
	int r;

	r = avrasm_do_llscope (treeptr, lls);

	free_llscope (lls);
	return r;
}
/*}}}*/
/*{{{  static int hlltypecheck_cpass (tnode_t **treeptr)*/
/*
 *	does the high-level type-check pass (for named things)
 *	returns 0 on success, non-zero on failure
 */
static int hlltypecheck_cpass (tnode_t **treeptr)
{
	hlltypecheck_t *hltc = (hlltypecheck_t *)smalloc (sizeof (hlltypecheck_t));
	int r;

	hltc->errcount = 0;
	avrasm_hlltypecheck_subtree (treeptr, hltc);
	r = hltc->errcount;
	sfree (hltc);

	return 0;
}
/*}}}*/
/*{{{  static int hllsimplify_cpass (tnode_t **treeptr)*/
/*
 *	does the high-level siplify pass
 *	returns 0 on success, non-zero on failure
 */
static int hllsimplify_cpass (tnode_t **treeptr)
{
	hllsimplify_t *hls = (hllsimplify_t *)smalloc (sizeof (hllsimplify_t));
	int r;

	hls->errcount = 0;
	hls->list_cxt = NULL;
	hls->list_itm = 0;
	hls->eoif_label = NULL;
	hls->eocond_label = NULL;
	hls->expr_target = NULL;
	avrasm_hllsimplify_subtree (treeptr, hls);
	r = hls->errcount;
	sfree (hls);

	return 0;
}
/*}}}*/
/*{{{  static int flatcode_cpass (tnode_t **treeptr)*/
/*
 *	called to do code-flattening for the assembler source
 *	returns 0 on success, non-zero on failure
 */
static int flatcode_cpass (tnode_t **treeptr)
{
	/* all-in-one go over the tree.  Expect the top-level to be a list of things */
	tnode_t *tree = *treeptr;
	tnode_t *curseg = NULL;
	int i;

	if (!parser_islistnode (tree)) {
		nocc_serious ("flatcode_cpass(): passed tree not list! was [%s]", tree->tag->name);
		return 1;
	}

	for (i=0; i<parser_countlist (tree); i++) {
		tnode_t *item = parser_getfromlist (tree, i);

		if (item->tag == avrasm.tag_SEGMENTMARK) {
			curseg = item;
			if (!tnode_nthsubof (curseg, 1)) {
				tnode_t *seglist = parser_newlistnode (NULL);

				tnode_setnthsub (curseg, 1, seglist);
			}
		} else if (item->tag == avrasm.tag_LETDEF) {
			/* remove this, will have been processed by hllsimplify stuff */
			parser_delfromlist (tree, i);
			i--;			/* we got removed */
		} else if (avrasm_langop_inseg (item)) {
			/* needs to be in a segment, do we have one? */
			if (!curseg) {
				/* make one, put in the list just before this one */
				curseg = tnode_createfrom (avrasm.tag_SEGMENTMARK, item, 
							tnode_createfrom (avrasm.tag_TEXTSEG, item),
							parser_newlistnode (NULL));
				parser_insertinlist (tree, curseg, i);
				i++;			/* we moved down */
			}

			/* ASSERT: curset is valid */
			if ((item->tag == avrasm.tag_GLABELDEF) || (item->tag == avrasm.tag_LLABELDEF)) {
				/* tag with right segment */
				label_chook_t *lch = avrasm_newlabelchook ();

				lch->zone = tnode_copytree (tnode_nthsubof (curseg, 0));
				lch->addr = 0;

				tnode_setchook (item, label_chook, (void *)lch);
			}

			parser_addtolist (tnode_nthsubof (curseg, 1), parser_delfromlist (tree, i));
			i--;			/* we got removed */
		}
	}

	return 0;
}
/*}}}*/

/*{{{  static tnode_t *avrasm_includefile (char *fname, lexfile_t *curlf)*/
/*
 *	includes a file
 *	returns a tree or NULL
 */
static tnode_t *avrasm_includefile (char *fname, lexfile_t *curlf)
{
	tnode_t *tree;
	lexfile_t *lf;

	lf = lexer_open (fname);
	if (!lf) {
		parser_error (SLOCN (curlf), "failed to open .include'd file %s", fname);
		return NULL;
	}

	lf->toplevel = 0;
	lf->islibrary = curlf->islibrary;
	lf->sepcomp = curlf->sepcomp;

	if (compopts.verbose) {
		nocc_message ("sub-parsing ...");
	}
	tree = parser_parse (lf);
	if (!tree) {
		parser_error (SLOCN (curlf), "failed to parse .include'd file %s", fname);
	}

	lexer_close (lf);
	return tree;
}
/*}}}*/
/*{{{  static int avrasm_parser_init (lexfile_t *lf)*/
/*
 *	initialises the AVR assembler parser
 *	returns 0 on success, non-zero on failure
 */
static int avrasm_parser_init (lexfile_t *lf)
{
	avrasm_priv_refcount++;
	if (avrasm_priv_refcount > 1) {
		/* must already be initialised */
		return 0;
	}

	if (compopts.verbose) {
		nocc_message ("initialising AVR assembler parser..");
	}
	if (!avrasm_priv) {
		int stopat;

		avrasm_priv = avrasm_newavrasmparse ();

		memset ((void *)&avrasm, 0, sizeof (avrasm));

		avrasm_priv->ldef = langdef_readdefs ("avrasm.ldef");
		if (!avrasm_priv->ldef) {
			nocc_error ("avrasm_parser_init(): failed to load language definitions!");
			return 1;
		}

		/* add various compiler passes, compiler-operations and language-operations */
		stopat = nocc_laststopat () + 1;
		opts_add ("stop-subequ", '\0', avrasm_opthandler_stopat, (void *)stopat, "1stop after subequ pass");
		if (nocc_addcompilerpass ("subequ", INTERNAL_ORIGIN, "scope", 0, (int (*)(void *))subequ_cpass, CPASS_TREEPTR, stopat, NULL)) {
			nocc_serious ("avrasm_parser_init(): failed to add \"subequ\" compiler pass");
			return 1;
		}

		stopat = nocc_laststopat () + 1;
		opts_add ("stop-submacro", '\0', avrasm_opthandler_stopat, (void *)stopat, "1stop after submacro pass");
		if (nocc_addcompilerpass ("submacro", INTERNAL_ORIGIN, "subequ", 0, (int (*)(void *))submacro_cpass, CPASS_TREEPTR, stopat, NULL)) {
			nocc_serious ("avrasm_parser_init(): failed to add \"submacro\" compiler pass");
			return 1;
		}

		stopat = nocc_laststopat () + 1;
		opts_add ("stop-llscope", '\0', avrasm_opthandler_stopat, (void *)stopat, "1stop after llscope pass");
		if (nocc_addcompilerpass ("llscope", INTERNAL_ORIGIN, "submacro", 0, (int (*)(void *))llscope_cpass, CPASS_TREEPTR, stopat, NULL)) {
			nocc_serious ("avrasm_parser_init(): failed to add \"llscope\" compiler pass");
			return 1;
		}

		stopat = nocc_laststopat () + 1;
		opts_add ("stop-hlltypecheck", '\0', avrasm_opthandler_stopat, (void *)stopat, "1stop after high-level type-check pass");
		if (nocc_addcompilerpass ("hlltypecheck", INTERNAL_ORIGIN, "llscope", 0, (int (*)(void *))hlltypecheck_cpass, CPASS_TREEPTR, stopat, NULL)) {
			nocc_serious ("avrasm_parser_init(): failed to add \"hlltypecheck\" compiler pass");
			return 1;
		}

		stopat = nocc_laststopat () + 1;
		opts_add ("stop-hllsimplify", '\0', avrasm_opthandler_stopat, (void *)stopat, "1stop after high-level simplify pass");
		if (nocc_addcompilerpass ("hllsimplify", INTERNAL_ORIGIN, "hlltypecheck", 0, (int (*)(void *))hllsimplify_cpass, CPASS_TREEPTR, stopat, NULL)) {
			nocc_serious ("avrasm_parser_init(): failed to add \"hllsimplify\" compiler pass");
			return 1;
		}

		stopat = nocc_laststopat () + 1;
		opts_add ("stop-flatcode", '\0', avrasm_opthandler_stopat, (void *)stopat, "1stop after flatcode pass");
		if (nocc_addcompilerpass ("flatcode", INTERNAL_ORIGIN, "type-check", 0, (int (*)(void *))flatcode_cpass, CPASS_TREEPTR, stopat, NULL)) {
			nocc_serious ("avrasm_parser_init(): failed to add \"flatcode\" compiler pass");
			return 1;
		}

		if (tnode_newcompop ("subequ", COPS_INVALID, 2, INTERNAL_ORIGIN) < 0) {
			nocc_serious ("avrasm_parser_init(): failed to add \"subequ\" compiler operation");
			return 1;
		}
		if (tnode_newcompop ("submacro", COPS_INVALID, 2, INTERNAL_ORIGIN) < 0) {
			nocc_serious ("avrasm_parser_init(): failed to add \"submacro\" compiler operation");
			return 1;
		}
		if (tnode_newcompop ("llscope", COPS_INVALID, 2, INTERNAL_ORIGIN) < 0) {
			nocc_serious ("avrasm_parser_init(): failed to add \"llscope\" compiler operation");
			return 1;
		}
		if (tnode_newcompop ("hlltypecheck", COPS_INVALID, 2, INTERNAL_ORIGIN) < 0) {
			nocc_serious ("avrasm_parser_init(): failed to add \"hlltypecheck\" compiler operation");
			return 1;
		}
		if (tnode_newcompop ("hllsimplify", COPS_INVALID, 2, INTERNAL_ORIGIN) < 0) {
			nocc_serious ("avrasm_parser_init(): failed to add \"hllsimplify\" compiler operation");
			return 1;
		}
		if (tnode_newlangop ("avrasm_inseg", LOPS_INVALID, 1, INTERNAL_ORIGIN) < 0) {
			nocc_serious ("avrasm_parser_init(): failed to add \"avrasm_inseg\" language operation");
			return 1;
		}

		/* initialise */
		if (feunit_do_init_tokens (0, avrasm_priv->ldef, origin_langparser (&avrasm_parser))) {
			nocc_error ("avrasm_parser_init(): failed to initialise tokens");
			return 1;
		}

		/* register some particular tokens for later comparison */
		avrasm.tok_DOT = lexer_newtoken (SYMBOL, ".");
		avrasm.tok_STRING = lexer_newtoken (STRING, NULL);
		avrasm.tok_PLUS = lexer_newtoken (SYMBOL, "+");
		avrasm.tok_MINUS = lexer_newtoken (SYMBOL, "-");
		avrasm.tok_REGX = lexer_newtoken (KEYWORD, "X");
		avrasm.tok_REGY = lexer_newtoken (KEYWORD, "Y");
		avrasm.tok_REGZ = lexer_newtoken (KEYWORD, "Z");

		/* and some compiler hooks */
		label_chook = tnode_lookupornewchook ("avrasm:labelinfo");

		if (feunit_do_init_nodes (feunit_set, 1, avrasm_priv->ldef, origin_langparser (&avrasm_parser))) {
			nocc_error ("avrasm_parser_init(): failed to initialise nodes");
			return 1;
		}
		if (feunit_do_reg_reducers (feunit_set, 0, avrasm_priv->ldef)) {
			nocc_error ("avrasm_parser_init(): failed to register reducers");
			return 1;
		}
		if (feunit_do_init_dfatrans (feunit_set, 1, avrasm_priv->ldef, &avrasm_parser, 1)) {
			nocc_error ("avrasm_parser_init(): failed to initialise DFAs");
			return 1;
		}
		if (feunit_do_post_setup (feunit_set, 1, avrasm_priv->ldef)) {
			nocc_error ("avrasm_parser_init(): failed to post-setup");
			return 1;
		}
		if (langdef_treecheck_setup (avrasm_priv->ldef)) {
			nocc_serious ("avrasm_parser(): failed to initialise tree-checking!");
			/* linger on */
		}

		avrasm_priv->inode = dfa_lookupbyname ("avrasm:program");
		if (!avrasm_priv->inode) {
			nocc_error ("avrasm_parser_init(): could not find avrasm:program");
			return 1;
		}
		if (compopts.dumpdfas) {
			dfa_dumpdfas (FHAN_STDERR);
		}
		if (compopts.dumpgrules) {
			parser_dumpgrules (FHAN_STDERR);
		}
	}
	return 0;
}
/*}}}*/
/*{{{  static void avrasm_parser_shutdown (lexfile_t *lf)*/
/*
 *	shuts-down the AVR assembler parser
 */
static void avrasm_parser_shutdown (lexfile_t *lf)
{
	avrasm_priv_refcount--;

	if (!avrasm_priv_refcount && avrasm_priv) {
		avrasm_freeavrasmparse (avrasm_priv);
		avrasm_priv = NULL;
	}
	return;
}
/*}}}*/

/*{{{  static tnode_t *avrasm_parse_codelineorspecial (lexfile_t *lf)*/
/*
 *	parses a single codeline, including 'special' handling.  assumes not end-of-tokens.
 *	returns tree on success, NULL on failure
 */
static tnode_t *avrasm_parse_codelineorspecial (lexfile_t *lf)
{
	tnode_t *thisone;

	thisone = dfa_walk ("avrasm:codeline", 0, lf);
	if (!thisone) {
		return NULL;
	}

	if (thisone->tag == avrasm.tag_MACRODEF) {
		/*{{{  slightly special case, parse input until .endmacro*/
		tnode_t *contents;

#if 0
fprintf (stderr, "avrasm_parser_parse(): sub-parse for macrodef, got:\n");
tnode_dumptree (thisone, 1, stderr);
#endif
		contents = avrasm_parser_parsemacrodef (lf);
		if (!contents) {
			parser_error (SLOCN (lf), "bad or empty macro definition");
		}
		tnode_setnthsub (thisone, 2, contents);

		/*}}}*/
	} else if (thisone->tag == avrasm.tag_FCNDEF) {
		/*{{{  another slightly special case, parse input until .endfunction*/
		tnode_t *contents;

		contents = avrasm_parser_parsefunctiondef (lf);
		tnode_setnthsub (thisone, 2, contents);

		/*}}}*/
	} else if (thisone->tag == avrasm.tag_HLLIF) {
		/*{{{  another special case, parser rest of .if structure*/
		tnode_t *contents;

		contents = avrasm_parser_parsehllif (lf, tnode_nthsubof (thisone, 0));
		tnode_setnthsub (thisone, 0, contents);

		/*}}}*/
	}

	return thisone;
}
/*}}}*/
/*{{{  static tnode_t *avrasm_parser_parsemacrodef (lexfile_t *lf)*/
/*
 *	called to parse a macro definition's contents, until .endmacro
 *	returns tree on success, NULL on failure
 */
static tnode_t *avrasm_parser_parsemacrodef (lexfile_t *lf)
{
	token_t *tok;
	tnode_t *tree = parser_newlistnode (SLOCN (lf));

	if (compopts.verbose) {
		nocc_message ("avrasm_parser_parsemacrodef(): starting parse..");
	}

	for (;;) {
		tnode_t *thisone;

		tok = lexer_nexttoken (lf);
		while ((tok->type == NEWLINE) || (tok->type == COMMENT)) {
			lexer_freetoken (tok);
			tok = lexer_nexttoken (lf);
		}
		if ((tok->type == END) || (tok->type == NOTOKEN)) {
			parser_error (SLOCN (lf), "unexpected end-of-file when reading macro definition");
			tnode_free (tree);
			return NULL;
		}
		if (lexer_tokmatch (avrasm.tok_DOT, tok)) {
			token_t *nexttok = lexer_nexttoken (lf);

			if (nexttok && lexer_tokmatchlitstr (nexttok, "endmacro")) {
				/* end-of-macro */
				lexer_freetoken (tok);
				lexer_freetoken (nexttok);

				break;			/* for() */
			} else {
				lexer_pushback (lf, nexttok);
			}
		}
		lexer_pushback (lf, tok);

		thisone = dfa_walk ("avrasm:codeline", 0, lf);
		if (!thisone) {
			break;			/* for() */
		}

		parser_addtolist (tree, thisone);
	}

	return tree;
}
/*}}}*/
/*{{{  static tnode_t *avrasm_parser_parsefunctiondef (lexfile_t *lf)*/
/*
 *	called to parse a function definition's contents, until .endfunction
 *	returns tree on success, NULL on failure
 */
static tnode_t *avrasm_parser_parsefunctiondef (lexfile_t *lf)
{
	token_t *tok;
	tnode_t *tree = parser_newlistnode (SLOCN (lf));

	if (compopts.verbose) {
		nocc_message ("avrasm_parser_parsefunctiondef(): starting parse..");
	}

	for (;;) {
		tnode_t *thisone;

		tok = lexer_nexttoken (lf);
		while ((tok->type == NEWLINE) || (tok->type == COMMENT)) {
			lexer_freetoken (tok);
			tok = lexer_nexttoken (lf);
		}
		if ((tok->type == END) || (tok->type == NOTOKEN)) {
			parser_error (SLOCN (lf), "unexpected end-of-file when reading function definition");
			tnode_free (tree);
			return NULL;
		}
		if (lexer_tokmatch (avrasm.tok_DOT, tok)) {
			token_t *nexttok = lexer_nexttoken (lf);

			if (nexttok && lexer_tokmatchlitstr (nexttok, "endfunction")) {
				/* end-of-macro */
				lexer_freetoken (tok);
				lexer_freetoken (nexttok);

				break;			/* for() */
			} else {
				lexer_pushback (lf, nexttok);
			}
		}
		lexer_pushback (lf, tok);

		thisone = avrasm_parse_codelineorspecial (lf);
		if (!thisone) {
			break;			/* for() */
		}

		parser_addtolist (tree, thisone);
	}

	return tree;
}
/*}}}*/
/*{{{  static tnode_t *avrasm_parser_parsehllif (lexfile_t *lf, tnode_t *firstcond)*/
/*
 *	called to parse a high-level .if structure, until .endif
 *	returns list of conditions / code on success, NULL on failure
 */
static tnode_t *avrasm_parser_parsehllif (lexfile_t *lf, tnode_t *firstcond)
{
	tnode_t *clist = parser_newlistnode (SLOCN (lf));
	token_t *tok;
	tnode_t **bodyp = NULL;
	tnode_t *tmpnode;

	if (compopts.verbose) {
		nocc_message ("avrasm_parser_parsehllif(): starting parse..");
	}

	/* start making first condition */
	tmpnode = tnode_createfrom (avrasm.tag_HLLCOND, firstcond, firstcond, parser_newlistnode (NULL));
	bodyp = tnode_nthsubaddr (tmpnode, 1);
	parser_addtolist (clist, tmpnode);

	for (;;) {
		int doparse = 0;

		tok = lexer_nexttoken (lf);
		while ((tok->type == NEWLINE) || (tok->type == COMMENT)) {
			lexer_freetoken (tok);
			tok = lexer_nexttoken (lf);
		}
		if ((tok->type == END) || (tok->type == NOTOKEN)) {
			parser_error (SLOCN (lf), "unexpected end-of-file when reading .if structure");
			tnode_free (clist);
			return NULL;
		}
		if (lexer_tokmatch (avrasm.tok_DOT, tok)) {
			token_t *nexttok = lexer_nexttoken (lf);

			if (nexttok && lexer_tokmatchlitstr (nexttok, "endif")) {
				/* end-of-if */
				lexer_freetoken (tok);
				lexer_freetoken (nexttok);

				break;			/* for() */
			} else if (nexttok && lexer_tokmatchlitstr (nexttok, "else")) {
				/* else */
				lexer_freetoken (tok);
				lexer_freetoken (nexttok);

				tmpnode = tnode_create (avrasm.tag_HLLCOND, SLOCN (lf), NULL, parser_newlistnode (NULL));
				bodyp = tnode_nthsubaddr (tmpnode, 1);
				parser_addtolist (clist, tmpnode);

				/* read next token to push back in a moment */
				tok = lexer_nexttoken (lf);
			} else if (nexttok && lexer_tokmatchlitstr (nexttok, "elsif")) {
				tnode_t *nextcond;

				/* else-if */
				lexer_freetoken (tok);
				lexer_freetoken (nexttok);

				/* expecting a condition for the next thing */
				nextcond = dfa_walk ("avrasm:hllexpr", 0, lf);
				if (!nextcond) {
					break;		/* for() */
				}

				tmpnode = tnode_createfrom (avrasm.tag_HLLCOND, nextcond, nextcond, parser_newlistnode (NULL));
				bodyp = tnode_nthsubaddr (tmpnode, 1);
				parser_addtolist (clist, tmpnode);

				/* read next token to push back in a moment */
				tok = lexer_nexttoken (lf);
			} else {
				doparse = 1;
				lexer_pushback (lf, nexttok);
			}
		} else {
			doparse = 1;
		}

		lexer_pushback (lf, tok);

		if (doparse) {
			tnode_t *thisone;
			
			thisone = avrasm_parse_codelineorspecial (lf);
			if (!thisone) {
				break;			/* for() */
			}
			parser_addtolist (*bodyp, thisone);
		}
	}

	return clist;
}
/*}}}*/
/*{{{  static tnode_t *avrasm_parser_parse (lexfile_t *lf)*/
/*
 *	called to parse a file (containing AVR assembler)
 *	returns a tree on success, NULL on failure
 *
 *	note: for assembler source, tree is just a list to start with
 */
static tnode_t *avrasm_parser_parse (lexfile_t *lf)
{
	token_t *tok;
	tnode_t *tree = parser_newlistnode (SLOCN (lf));

	if (compopts.verbose) {
		nocc_message ("avrasm_parser_parse(): starting parse..");
	}

	for (;;) {
		tnode_t *thisone;

		tok = lexer_nexttoken (lf);
		while ((tok->type == NEWLINE) || (tok->type == COMMENT)) {
			lexer_freetoken (tok);
			tok = lexer_nexttoken (lf);
		}
		if ((tok->type == END) || (tok->type == NOTOKEN)) {
			/* done */
			lexer_freetoken (tok);
			break;		/* for() */
		}
		if (lexer_tokmatch (avrasm.tok_DOT, tok)) {
			token_t *nexttok = lexer_nexttoken (lf);

			if (nexttok && lexer_tokmatchlitstr (nexttok, "include")) {
				/*{{{  process include'd file, continue*/
				lexer_freetoken (tok);
				lexer_freetoken (nexttok);

				nexttok = lexer_nexttoken (lf);
				if (nexttok && lexer_tokmatch (avrasm.tok_STRING, nexttok)) {
					tnode_t *itree;
					
					itree = avrasm_includefile (nexttok->u.str.ptr, lf);
					lexer_freetoken (nexttok);
					
					if (itree) {
						/* should be another list of stuff, add it to the current program */
						parser_mergeinlist (tree, itree, -1);
					}
				} else {
					parser_error (SLOCN (lf), "while processing .include, expected string but found ");
					lexer_dumptoken (FHAN_STDERR, nexttok);
					lexer_freetoken (nexttok);
				}
				continue;		/* for() */
				/*}}}*/
			} else {
				lexer_pushback (lf, nexttok);
			}
		}
		lexer_pushback (lf, tok);

		thisone = avrasm_parse_codelineorspecial (lf);
		// thisone = dfa_walk ("avrasm:codeline", 0, lf);
		if (!thisone) {
			break;		/* for() */
		}

		/* add to program */
		parser_addtolist (tree, thisone);
	}

	if (compopts.verbose) {
		nocc_message ("leftover tokens:");
	}

	tok = lexer_nexttoken (lf);
	while (tok) {
		if (compopts.verbose) {
			lexer_dumptoken (FHAN_STDERR, tok);
		}
		if ((tok->type == END) || (tok->type == NOTOKEN)) {
			lexer_freetoken (tok);
			break;
		}
		if ((tok->type != NEWLINE) && (tok->type != COMMENT)) {
			lf->errcount++;				/* got errors.. */
		}

		lexer_freetoken (tok);
		tok = lexer_nexttoken (lf);
	}

	return tree;
}
/*}}}*/
/*{{{  static int avrasm_parser_prescope (tnode_t **tptr, prescope_t *ps)*/
/*
 *	called to pre-scope the parse tree
 *	returns 0 on success, non-zero on failure
 */
static int avrasm_parser_prescope (tnode_t **tptr, prescope_t *ps)
{
	ps->hook = NULL;

	if (!*tptr) {
		return -1;
	}

	/* first, attempt to set the default compiler target */
	nocc_setdefaulttarget ("avr", "atmel", NULL);

	tnode_modprewalktree (tptr, prescope_modprewalktree, (void *)ps);

	return ps->err;
}
/*}}}*/
/*{{{  static int avrasm_parser_scope_inner (tnode_t **tptr, scope_t *ss)*/
/*
 *	called to scope part of a parse tree -- must be a list
 *	returns 0 on success, non-zero on failure
 */
static int avrasm_parser_scope_inner (tnode_t **tptr, scope_t *ss)
{
	tnode_t *tree = *tptr;
	tnode_t **items;
	int nitems, i;

	if (!parser_islistnode (tree)) {
		nocc_internal ("avrasm_parser_scope_inner(): tree is not a list! (serious).  Got [%s:%s]\n",
				tree->tag->ndef->name, tree->tag->name);
		return -1;
	}
	items = parser_getlistitems (tree, &nitems);

	for (i=0; i<nitems; i++) {
		tnode_t *node = items[i];

		/* first, scope in label and function names */
		if (node->tag == avrasm.tag_GLABELDEF) {
			tnode_t *lname_node = tnode_nthsubof (node, 0);

			if (lname_node->tag != avrasm.tag_NAME) {
				scope_error (lname_node, ss, "label name not raw-name");
			} else {
				char *rawname = tnode_nthhookof (lname_node, 0);

				if (name_lookupss (rawname, ss)) {
					/* not allowed multiply defined labels in assembler.. */
					scope_error (lname_node, ss, "multiply defined label [%s]", rawname);
				} else {
					name_t *labname;
					tnode_t *namenode;

					labname = name_addscopenamess (rawname, node, NULL, NULL, ss);
					namenode = tnode_createfrom (avrasm.tag_GLABEL, lname_node, labname);
					SetNameNode (labname, namenode);

					tnode_free (lname_node);
					tnode_setnthsub (node, 0, namenode);

					ss->scoped++;
				}
			}
		} else if (node->tag == avrasm.tag_FCNDEF) {
			tnode_t *fname_node = tnode_nthsubof (node, 0);
			tnode_t **fbody_addr = tnode_nthsubaddr (node, 2);

			if ((*fbody_addr) && parser_islistnode (*fbody_addr)) {
				/* run over the body to pick up any global labels */
				avrasm_parser_scope_inner (fbody_addr, ss);
			}

			if (fname_node->tag != avrasm.tag_NAME) {
				scope_error (fname_node, ss, "function name not raw-name, got [%s]", fname_node->tag->name);
			} else {
				char *rawname = tnode_nthhookof (fname_node, 0);

				if (name_lookupss (rawname, ss)) {
					/* not allowed multiply defined functions */
					scope_error (fname_node, ss, "multiply defined name [%s]", rawname);
				} else {
					name_t *fname;
					tnode_t *namenode;

					fname = name_addscopenamess (rawname, node, NULL, NULL, ss);
					namenode = tnode_createfrom (avrasm.tag_FCNNAME, node, fname);
					SetNameNode (fname, namenode);

					tnode_free (fname_node);
					tnode_setnthsub (node, 0, namenode);

					ss->scoped++;
				}
			}
		}
	}

	return 0;
}
/*}}}*/
/*{{{  static int avrasm_parser_scope (tnode_t **tptr, scope_t *ss)*/
/*
 *	called to scope the parse tree
 *	returns 0 on success, non-zero on failure
 */
static int avrasm_parser_scope (tnode_t **tptr, scope_t *ss)
{
	tnode_t *tree = *tptr;
	void *nsmark;

	if (!parser_islistnode (tree)) {
		nocc_internal ("avrasm_parser_scope(): top-level tree is not a list! (serious).  Got [%s:%s]\n",
				tree->tag->ndef->name, tree->tag->name);
		return -1;
	}

	nsmark = name_markscope ();

	/* first look for GLABELs and whatnot, then call scope proper */
	avrasm_parser_scope_inner (tptr, ss);
	tnode_modprepostwalktree (tptr, scope_modprewalktree, scope_modpostwalktree, (void *)ss);

	name_markdescope (nsmark);

	return 0;
}
/*}}}*/
/*{{{  static int avrasm_do_llscope (tnode_t **tptr, llscope_t *lls)*/
/*
 *	called to do local-label scoping on the parse tree
 *	returns 0 on success, non-zero on failure
 */
static int avrasm_do_llscope (tnode_t **tptr, llscope_t *lls)
{
	tnode_t *tree = *tptr;
	tnode_t **items;
	int nitems, i;

	if (!parser_islistnode (tree)) {
		nocc_internal ("avrasm_do_llscope(): passed thing is not a list! (serious).  Got [%s:%s]\n",
				tree->tag->ndef->name, tree->tag->name);
		return -1;
	}
	items = parser_getlistitems (tree, &nitems);

	/* Note: at this point the assembler source is completely flat (and not embedded in segments), so
	 * all local label definitions and instances occur in the order written in the file (but after
	 * macro substitution, so we don't confuse these).  Deliberately ignored on the first scope pass.
	 */
	for (i=0; i<nitems; i++) {
		tnode_t *node = items[i];

		if (node->tag == avrasm.tag_LLABELDEF) {
			tnode_t *lab_id = tnode_nthsubof (node, 0);
			int id, j;
			llscope_entry_t *thisdef;
			name_t *labname;
			char *rawlabname;
			tnode_t *labnamenode;

			if (lab_id->tag != avrasm.tag_LITINT) {
				tnode_error (node, "local label not integer! got [%s]", lab_id->tag->name);
				lls->error++;
				break;		/* for() */
			}
			id = avrasm_getlitintval (lab_id);
			thisdef = llscope_lookup_fordef (lls, id);

			/* create a name for it */
			rawlabname = string_fmt ("L%d", id);
			labnamenode = tnode_createfrom (avrasm.tag_LLABEL, node, NULL);
			labname = name_addname (rawlabname, node, NULL, labnamenode);
			tnode_setnthname (labnamenode, 0, labname);

			thisdef->name = labname;
			llscope_promote (lls, id);

			for (j=0; j<DA_CUR (thisdef->frefs); j++) {
				/* apply fixups, now backward references */
				avrasm_llscope_fixref (DA_NTHITEM (thisdef->frefs, j), id, -1, (void *)lls);
			}
			dynarray_trash (thisdef->frefs);

			/* now all fixups have been applied, reset LLABELDEF to include new name-node */
			tnode_free (lab_id);
			tnode_setnthsub (node, 0, labnamenode);
#if 0
fprintf (stderr, "avrasm_do_llscope(): here1, id = %d\n", id);
#endif
		} else {
			/* call llscope pass on the node to pick up stray references */
			avrasm_llscope_subtree (items + i, lls);
		}
	}

	/* before returning, check forward references -- should not have any! */
	for (i=0; i<DA_CUR (lls->flabels); i++) {
		llscope_entry_t *llse = DA_NTHITEM (lls->flabels, i);

		if (llse) {
			int j;

			nocc_error ("%d undefined reference(s) to local-label %d:", DA_CUR (llse->frefs), i);
			for (j=0; j<DA_CUR (llse->frefs); j++) {
				tnode_error (*(DA_NTHITEM (llse->frefs, j)), " ");
			}
			lls->error++;
		}
	}

	return lls->error;
}
/*}}}*/
/*{{{  static int avrasm_parser_typecheck (tnode_t *tptr, typecheck_t *tc)*/
/*
 *	called to type-check the parse tree
 *	returns 0 on success, non-zero on failure
 */
static int avrasm_parser_typecheck (tnode_t *tptr, typecheck_t *tc)
{
	tnode_prewalktree (tptr, typecheck_prewalktree, (void *)tc);
	return tc->err;
}
/*}}}*/
/*{{{  static int avrasm_parser_typeresolve (tnode_t **tptr, typecheck_t *tc)*/
/*
 *	called to type-resolve the parse tree
 *	returns 0 on success, non-zero on failure
 */
static int avrasm_parser_typeresolve (tnode_t **tptr, typecheck_t *tc)
{
	tnode_modprewalktree (tptr, typeresolve_modprewalktree, (void *)tc);
	return tc->err;
}
/*}}}*/


