/*
 *	guppy_parser.c -- Guppy parser for nocc
 *	Copyright (C) 2010-2015 Fred Barnes <frmb@kent.ac.uk>
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
#include "opts.h"
#include "symbols.h"
#include "keywords.h"
#include "lexer.h"
#include "lexpriv.h"
#include "tnode.h"
#include "parser.h"
#include "fcnlib.h"
#include "langdef.h"
#include "dfa.h"
#include "dfaerror.h"
#include "parsepriv.h"
#include "guppy.h"
#include "library.h"
#include "feunit.h"
#include "langops.h"
#include "names.h"
#include "scope.h"
#include "prescope.h"
#include "typecheck.h"
#include "fetrans.h"
#include "extn.h"
#include "mwsync.h"
#include "metadata.h"
#include "target.h"
#include "cccsp.h"


/*}}}*/

/*{{{  forward decls*/
static int guppy_parser_init (lexfile_t *lf);
static void guppy_parser_shutdown (lexfile_t *lf);
static tnode_t *guppy_parser_parse (lexfile_t *lf);
static tnode_t *guppy_parser_descparse (lexfile_t *lf);
static int guppy_parser_prescope (tnode_t **tptr, prescope_t *ps);
static int guppy_parser_scope (tnode_t **tptr, scope_t *ss);
static int guppy_parser_typecheck (tnode_t *tptr, typecheck_t *tc);
static int guppy_parser_typeresolve (tnode_t **tptr, typecheck_t *tc);
static int guppy_parser_fetrans (tnode_t **tptr, fetrans_t *fe);
static char **guppy_getlanglibs (target_t *target, int src);

static tnode_t *guppy_parser_parseproc (lexfile_t *lf);
static tnode_t *guppy_parser_parsedef (lexfile_t *lf);
static tnode_t *guppy_declorproc (lexfile_t *lf);
static tnode_t *guppy_indented_declorproc_list (lexfile_t *lf);
static tnode_t *guppy_indented_dguard_list (lexfile_t *lf);
static tnode_t *guppy_indented_tcase_list (lexfile_t *lf);
static tnode_t *guppy_indented_exprproc_list (lexfile_t *lf);


/*}}}*/
/*{{{  global vars*/

guppy_pset_t gup;		/* attach tags, etc. here */

langparser_t guppy_parser = {
	.langname =		"guppy",
	.init =			guppy_parser_init,
	.shutdown =		guppy_parser_shutdown,
	.parse =		guppy_parser_parse,
	.descparse =		guppy_parser_descparse,
	.prescope =		guppy_parser_prescope,
	.scope =		guppy_parser_scope,
	.typecheck =		guppy_parser_typecheck,
	.typeresolve =		guppy_parser_typeresolve,
	.postcheck =		NULL,
	.fetrans =		guppy_parser_fetrans,
	.getlangdef =		guppy_getlangdef,
	.getlanglibs =		guppy_getlanglibs,
	.maketemp =		NULL,
	.makeseqassign =	NULL,
	.makeseqany =		NULL,
	.tagstruct_hook =	(void *)&gup,
	.lexer =		NULL
};

/*}}}*/
/*{{{  private types/vars*/
typedef struct {
	dfanode_t *inode;
	langdef_t *langdefs;
} guppy_parse_t;


static guppy_parse_t *guppy_priv = NULL;

static feunit_t *feunit_set[] = {
	&guppy_misc_feunit,
	&guppy_primproc_feunit,
	&guppy_fcndef_feunit,
	&guppy_decls_feunit,
	&guppy_types_feunit,
	&guppy_cnode_feunit,
	&guppy_cflow_feunit,
	&guppy_assign_feunit,
	&guppy_io_feunit,
	&guppy_lit_feunit,
	&guppy_oper_feunit,
	&guppy_instance_feunit,
	&guppy_timer_feunit,
	NULL
};

static ntdef_t *testtruetag, *testfalsetag;
static int tempnamecounter = 0;


/*}}}*/


/*{{{  static int guppy_opthandler_stopat (cmd_option_t *opt, char ***argwalk, int *argleft)*/
/*
 *	option handler for Guppy "stop" options
 */
static int guppy_opthandler_stopat (cmd_option_t *opt, char ***argwalk, int *argleft)
{
	compopts.stoppoint = (int)(opt->arg);
#if 0
fprintf (stderr, "guppy_opthandler_stopat(): setting stop point to %d\n", compopts.stoppoint);
#endif
	return 0;
}
/*}}}*/


/*{{{  guppy_fetrans1_t *guppy_newfetrans1 (void)*/
/*
 *	creates a new guppy_fetrans1_t structure
 */
guppy_fetrans1_t *guppy_newfetrans1 (void)
{
	guppy_fetrans1_t *fe1 = (guppy_fetrans1_t *)smalloc (sizeof (guppy_fetrans1_t));

	dynarray_init (fe1->rnames);
	fe1->inspoint = NULL;
	fe1->decllist = NULL;
	fe1->error = 0;

	return fe1;
}
/*}}}*/
/*{{{  void guppy_freefetrans1 (guppy_fetrans1_t *fe1)*/
/*
 *	frees a guppy_fetrans1_t structure
 */
void guppy_freefetrans1 (guppy_fetrans1_t *fe1)
{
	if (!fe1) {
		nocc_serious ("guppy_freefetrans1(): NULL pointer!");
		return;
	}
	dynarray_trash (fe1->rnames);
	sfree (fe1);
	return;
}
/*}}}*/
/*{{{  guppy_fetrans15_t *guppy_newfetrans15 (void)*/
/*
 *	creates a new guppy_fetrans15_t structure
 */
guppy_fetrans15_t *guppy_newfetrans15 (void)
{
	guppy_fetrans15_t *fe15 = (guppy_fetrans15_t *)smalloc (sizeof (guppy_fetrans15_t));

	fe15->expt_proc = 0;
	fe15->error = 0;

	return fe15;
}
/*}}}*/
/*{{{  void guppy_freefetrans15 (guppy_fetrans15_t *fe15)*/
/*
 *	frees a guppy_fetrans15_t structure
 */
void guppy_freefetrans15 (guppy_fetrans15_t *fe15)
{
	if (!fe15) {
		nocc_serious ("guppy_freefetrans15(): NULL pointer!");
		return;
	}
	sfree (fe15);
	return;
}
/*}}}*/
/*{{{  guppy_fetrans2_t *guppy_newfetrans2 (void)*/
/*
 *	creates a new guppy_fetrans2_t structure
 */
guppy_fetrans2_t *guppy_newfetrans2 (void)
{
	guppy_fetrans2_t *fe2 = (guppy_fetrans2_t *)smalloc (sizeof (guppy_fetrans2_t));

	fe2->error = 0;

	return fe2;
}
/*}}}*/
/*{{{  void guppy_freefetrans2 (guppy_fetrans2_t *fe2)*/
/*
 *	frees a guppy_fetrans2_t structure
 */
void guppy_freefetrans2 (guppy_fetrans2_t *fe2)
{
	if (!fe2) {
		nocc_serious ("guppy_freefetrans2(): NULL pointer!");
		return;
	}
	sfree (fe2);
	return;
}
/*}}}*/
/*{{{  guppy_fetrans3_t *guppy_newfetrans3 (void)*/
/*
 *	creates a new guppy_fetrans3_t structure
 */
guppy_fetrans3_t *guppy_newfetrans3 (void)
{
	guppy_fetrans3_t *fe3 = (guppy_fetrans3_t *)smalloc (sizeof (guppy_fetrans3_t));

	fe3->error = 0;

	return fe3;
}
/*}}}*/
/*{{{  void guppy_freefetrans3 (guppy_fetrans3_t *fe3)*/
/*
 *	frees a guppy_fetrans3_t structure
 */
void guppy_freefetrans3 (guppy_fetrans3_t *fe3)
{
	if (!fe3) {
		nocc_serious ("guppy_freefetrans3(): NULL pointer!");
		return;
	}
	sfree (fe3);
	return;
}
/*}}}*/
/*{{{  static guppy_parse_t *guppy_newguppyparse (void)*/
/*
 *	creates a new guppy_parse_t structure
 */
static guppy_parse_t *guppy_newguppyparse (void)
{
	guppy_parse_t *gpse = (guppy_parse_t *)smalloc (sizeof (guppy_parse_t));

	gpse->inode = NULL;
	gpse->langdefs = NULL;

	return gpse;
}
/*}}}*/
/*{{{  static void guppy_freeguppyparse (guppy_parse_t *gpse)*/
/*
 *	frees an guppy_parse_t structure
 */
static void guppy_freeguppyparse (guppy_parse_t *gpse)
{
	if (!gpse) {
		nocc_warning ("guppy_freeguppyparse(): NULL pointer!");
		return;
	}
	if (gpse->langdefs) {
		langdef_freelangdef (gpse->langdefs);
		gpse->langdefs = NULL;
	}
	/* leave inode alone */
	gpse->inode = NULL;
	sfree (gpse);

	return;
}
/*}}}*/
/*{{{  guppy reductions*/
/*{{{  void *guppy_nametoken_to_hook (void *ntok)*/
/*
 *	turns a name token into a hooknode for a tag_NAME
 */
void *guppy_nametoken_to_hook (void *ntok)
{
	token_t *tok = (token_t *)ntok;
	char *rawname;

	rawname = tok->u.name;
	tok->u.name = NULL;

	lexer_freetoken (tok);

	return (void *)rawname;
}
/*}}}*/
/*{{{  static void guppy_reduce_checkfixio (dfastate_t *dfast, parsepriv_t *pp, void *rarg)*/
/*
 *	used as a reduction to unpick MARKEDIN/MARKEDOUT nodes that should probably be INPUT/OUTPUT actions;
 *	may fabricate things and push into the lexer's token-stack (not a reduction in the conventional sense).
 */
static void guppy_reduce_checkfixio (dfastate_t *dfast, parsepriv_t *pp, void *rarg)
{
	tnode_t *snode = dfa_popnode (dfast);

#if 0
fhandle_printf (FHAN_STDERR, "guppy_reduce_checkfixio(): pp=%p, pp->lf=%p\n", pp, pp->lf);
#endif
	if (snode->tag == gup.tag_MARKEDIN) {
		token_t *tok = lexer_newtoken (SYMBOL, "?");

		snode = tnode_nthsubof (snode, 0);
		tok->origin = pp->lf;
		lexer_pushback (pp->lf, tok);
	} else if (snode->tag == gup.tag_MARKEDOUT) {
		token_t *tok = lexer_newtoken (SYMBOL, "!");

		snode = tnode_nthsubof (snode, 0);
		tok->origin = pp->lf;
		lexer_pushback (pp->lf, tok);
	}

	/* push it back on (maybe modified) */
	dfa_pushnode (dfast, snode);
	return;
}
/*}}}*/

/*}}}*/

/*{{{  static int pullmodule_modprewalk (tnode_t **tptr, void *arg)*/
/*
 *	called before prescope proper to pull up any module definitions, only looks once (arg is to an integer flag)
 *	returns 0 to stop walk, 1 to continue
 */
static int pullmodule_modprewalk (tnode_t **tptr, void *arg)
{
	int *found = (int *)arg;

	if (*found) {
		/* did it */
		return 0;
	}
		
	if (*tptr && parser_islistnode (*tptr)) {
		tnode_t **items;
		int nitems, i;
		ntdef_t *libnodetag = library_getlibnodetag ();

		/* look for library nodes */
		items = parser_getlistitems (*tptr, &nitems);
		for (i=0; i<nitems; i++) {
			if (items[i] && (items[i]->tag == libnodetag)) {
				/* found it!  Put the rest of *this* list inside it */
				tnode_t *libnode = items[i];
				tnode_t *tlist = parser_newlistnode (SLOCI);

				i++;
				while (i < nitems) {
					tnode_t *itm = parser_delfromlist (*tptr, i);

					parser_addtolist (tlist, itm);
					nitems--;
				}

				tnode_setnthsub (libnode, 0, tlist);
				*found = 1;
				return 0;
			}
		}
	}

	return 1;
}
/*}}}*/

/*{{{  static int declify_modprewalk (tnode_t **tptr, void *arg)*/
/*
 *	called for each node walked during the 'declify' pass
 *	returns 0 to stop walk, 1 to continue
 */
static int declify_modprewalk (tnode_t **tptr, void *arg)
{
	guppy_declify_t *gdl = (guppy_declify_t *)arg;
	int i = 1;

	if (*tptr && (*tptr)->tag->ndef->ops && tnode_hascompop ((*tptr)->tag->ndef->ops, "declify")) {
		i = tnode_callcompop ((*tptr)->tag->ndef->ops, "declify", 2, tptr, gdl);
	}
	return i;
}
/*}}}*/
/*{{{  static int autoseq_modprewalk (tnode_t **tptr, void *arg)*/
/*
 *	called for each node walked during the 'auto-sequence' pass
 *	returns 0 to stop walk, 1 to continue
 */
static int autoseq_modprewalk (tnode_t **tptr, void *arg)
{
	guppy_autoseq_t *gas = (guppy_autoseq_t *)arg;
	int i = 1;

	if (*tptr && (*tptr)->tag->ndef->ops && tnode_hascompop ((*tptr)->tag->ndef->ops, "autoseq")) {
		i = tnode_callcompop ((*tptr)->tag->ndef->ops, "autoseq", 2, tptr, gas);
	}
	return i;
}
/*}}}*/
/*{{{  static int flattenseq_modprewalk (tnode_t **tptr, void *arg)*/
/*
 *	called for each node walked during the 'flattenseq' pass
 *	returns 0 to stop walk, 1 to continue
 */
static int flattenseq_modprewalk (tnode_t **tptr, void *arg)
{
	int i = 1;

	if (*tptr && (*tptr)->tag->ndef->ops && tnode_hascompop ((*tptr)->tag->ndef->ops, "flattenseq")) {
		i = tnode_callcompop ((*tptr)->tag->ndef->ops, "flattenseq", 1, tptr);
	}
	return i;
}
/*}}}*/
/*{{{  static int postscope_modprewalk (tnode_t **tptr, void *arg)*/
/*
 *	called for each node walked during the 'postscope' pass
 *	returns 0 to stop walk, 1 to continue
 */
static int postscope_modprewalk (tnode_t **tptr, void *arg)
{
	int i = 1;

	if (*tptr && (*tptr)->tag->ndef->ops && tnode_hascompop ((*tptr)->tag->ndef->ops, "postscope")) {
		i = tnode_callcompop ((*tptr)->tag->ndef->ops, "postscope", 1, tptr);
	}
	return i;
}
/*}}}*/
/*{{{  static int fetrans1_modprewalk (tnode_t **tptr, void *arg)*/
/*
 *	called for each node walked during the 'fetrans1' pass
 *	returns 0 to stop walk, 1 to continue
 */
static int fetrans1_modprewalk (tnode_t **tptr, void *arg)
{
	guppy_fetrans1_t *fe1 = (guppy_fetrans1_t *)arg;
	int i = 1;

	if (*tptr && (*tptr)->tag->ndef->ops && tnode_hascompop ((*tptr)->tag->ndef->ops, "fetrans1")) {
#if 0
fhandle_printf (FHAN_STDERR, "fetrans1_modprewalk(): call on node [%s:%s]\n", (*tptr)->tag->ndef->name, (*tptr)->tag->name);
#endif
		i = tnode_callcompop ((*tptr)->tag->ndef->ops, "fetrans1", 2, tptr, fe1);
	}
	return i;
}
/*}}}*/
/*{{{  static int fetrans15_modprewalk (tnode_t **tptr, void *arg)*/
/*
 *	called for each node walked during the 'fetrans15' pass
 *	returns 0 to stop walk, 1 to continue
 */
static int fetrans15_modprewalk (tnode_t **tptr, void *arg)
{
	guppy_fetrans15_t *fe15 = (guppy_fetrans15_t *)arg;
	int i = 1;

	if (*tptr && (*tptr)->tag->ndef->ops && tnode_hascompop ((*tptr)->tag->ndef->ops, "fetrans15")) {
#if 0
fhandle_printf (FHAN_STDERR, "fetrans15_modprewalk(): call on node [%s:%s]\n", (*tptr)->tag->ndef->name, (*tptr)->tag->name);
#endif
		i = tnode_callcompop ((*tptr)->tag->ndef->ops, "fetrans15", 2, tptr, fe15);
	}
	return i;
}
/*}}}*/
/*{{{  static int fetrans2_modprewalk (tnode_t **tptr, void *arg)*/
/*
 *	called for each node walked during the 'fetrans2' pass
 *	returns 0 to stop walk, 1 to continue
 */
static int fetrans2_modprewalk (tnode_t **tptr, void *arg)
{
	guppy_fetrans2_t *fe2 = (guppy_fetrans2_t *)arg;
	int i = 1;

	if (*tptr && (*tptr)->tag->ndef->ops && tnode_hascompop ((*tptr)->tag->ndef->ops, "fetrans2")) {
		i = tnode_callcompop ((*tptr)->tag->ndef->ops, "fetrans2", 2, tptr, fe2);
	}
	return i;
}
/*}}}*/
/*{{{  static int fetrans3_modprewalk (tnode_t **tptr, void *arg)*/
/*
 *	called for each node walked during the 'fetrans3' pass
 *	returns 0 to stop walk, 1 to continue
 */
static int fetrans3_modprewalk (tnode_t **tptr, void *arg)
{
	guppy_fetrans3_t *fe3 = (guppy_fetrans3_t *)arg;
	int i = 1;

	if (*tptr && (*tptr)->tag->ndef->ops && tnode_hascompop ((*tptr)->tag->ndef->ops, "fetrans3")) {
		i = tnode_callcompop ((*tptr)->tag->ndef->ops, "fetrans3", 2, tptr, fe3);
	}
	return i;
}
/*}}}*/

/*{{{  void guppy_isetindent (fhandle_t *stream, int indent)*/
/*
 *	set-indent for debugging output
 */
void guppy_isetindent (fhandle_t *stream, int indent)
{
	int i;

	for (i=0; i<indent; i++) {
		fhandle_printf (stream, "    ");
	}
	return;
}
/*}}}*/
/*{{{  langdef_t *guppy_getlangdef (void)*/
/*
 *	returns the language definition for Guppy, or NULL if none
 */
langdef_t *guppy_getlangdef (void)
{
	if (!guppy_priv) {
		return NULL;
	}
	return guppy_priv->langdefs;
}
/*}}}*/
/*{{{  static char **guppy_getlanglibs (target_t *target, int src)*/
/*
 *	returns the language libraries for Guppy, or NULL if none
 *	returned array is freshly allocated, with individual allocations therein.
 */
static char **guppy_getlanglibs (target_t *target, int src)
{
	int nlibs = 1;
	char **tmp;
	
	if (cccsp_get_subtarget () == CCCSP_SUBTARGET_EV3) {
		/* make sure we include "cccsp/guppy_ev3_lib.c" */
		nlibs++;
	}

	tmp = smalloc ((nlibs + 1) * sizeof (char *));

	nlibs = 0;
	if (src) {
		tmp[nlibs++] = string_dup ("cccsp/guppy_cccsp_lib.c");
	} else {
		tmp[nlibs++] = string_dup ("cccsp/guppy_cccsp_lib.o");
	}

	if (cccsp_get_subtarget () == CCCSP_SUBTARGET_EV3) {
		if (src) {
			tmp[nlibs++] = string_dup ("cccsp/guppy_ev3_lib.c");
		} else {
			tmp[nlibs++] = string_dup ("cccsp/guppy_ev3_lib.o");
		}
	}

	tmp[nlibs++] = NULL;

	return tmp;
}
/*}}}*/

/*{{{  int guppy_declify_listtodecllist (tnode_t **listptr, guppy_declify_t *gdl)*/
/*
 *	used during declify to turn lists of declarations and instructions into nested DECLBLOCK nodes
 *	returns 0 on success, non-zero on failure
 */
int guppy_declify_listtodecllist (tnode_t **listptr, guppy_declify_t *gdl)
{
	tnode_t *list = *listptr;
	int nitems = 0;
	tnode_t **items = parser_getlistitems (list, &nitems);
	int i, j;
	tnode_t **nextptr = listptr;

	for (i=0; (i<nitems) && (items[i]->tag == gup.tag_VARDECL); i++);
	for (j=i; (j<nitems) && (items[j]->tag != gup.tag_VARDECL); j++);

	/* note: i is index of first non-decl item, j index of next decl-item or EOL */

#if 0
fprintf (stderr, "guppy_declify_listtodecllist(): i=%d, j=%d, nitems=%d\n", i, j, nitems);
#endif
	if (i > 0) {
		/* at least some declarations -- will trash the whole original list */
		tnode_t *decllist = parser_newlistnode (OrgOf (list));
		tnode_t *instlist = parser_newlistnode (OrgOf (list));
		tnode_t *vdblock = tnode_createfrom (gup.tag_DECLBLOCK, list, decllist, instlist);
		int k;

		for (k=0; k<i; k++) {
			/* move this one into declaration list */
			parser_addtolist (decllist, items[k]);
		}
		for (; k<nitems; k++) {
			/* move this one into process list below */
			parser_addtolist (instlist, items[k]);
		}

		parser_trashlist (list);
		*listptr = vdblock;
		nextptr = tnode_nthsubaddr (vdblock, 1);
	} else if ((j > 0) && (j < (nitems - 1))) {
		/* item at index j is a declaration, keep original list and fiddle at that point */
		tnode_t *decllist = parser_newlistnode (OrgOf (list));
		tnode_t *instlist = parser_newlistnode (OrgOf (list));
		tnode_t *vdblock = tnode_createfrom (gup.tag_DECLBLOCK, list, decllist, instlist);

		while ((j < nitems) && (items[j]->tag == gup.tag_VARDECL)) {
			tnode_t *itm = parser_delfromlist (list, j);

			parser_addtolist (decllist, itm);
			nitems--;
		}
		while (j < nitems) {
			/* and if there are any left, put in instlist */
			tnode_t *itm = parser_delfromlist (list, j);

			parser_addtolist (instlist, itm);
			nitems--;
		}

		/* put decl-block in at end */
		parser_addtolist (list, vdblock);
		nextptr = tnode_nthsubaddr (vdblock, 1);
	}

	if (nextptr) {
		/* recurse down into remains of list and do there */
		guppy_declify_subtree (nextptr, gdl);
	}

	return 0;
}
/*}}}*/
/*{{{  int guppy_declify_listtodecllist_single (tnode_t **listptr, guppy_declify_t *gdl)*/
/*
 *	used during declify to turn lists of declarations and instructions into flat DECLBLOCK nodes (e.g. in a PAR)
 *	returns 0 on success, non-zero on failure
 */
int guppy_declify_listtodecllist_single (tnode_t **listptr, guppy_declify_t *gdl)
{
	tnode_t *list = *listptr;
	int nitems = 0;
	tnode_t **items = parser_getlistitems (list, &nitems);
	int i, j;
	tnode_t *newlist = parser_newlistnode (OrgOf (list));

#if 0
fprintf (stderr, "guppy_declify_listtodecllist_single(): nitems=%d\n", nitems);
#endif
	/* call declify on the subnodes as we go through */
	for (i=0; i<nitems; i=j+1) {
		for (j=i; (j<nitems) && (items[j]->tag == gup.tag_VARDECL); j++);

		/* note: i is index of first decl item; j is index of following process (if any) */
		if (j == i) {
			/* singleton, pop it on the target list alone */
			parser_addtolist (newlist, items[i]);
			items[i] = NULL;
		} else {
			/* at least one declaration, make into declblock */
			tnode_t *decllist = parser_newlistnode (OrgOf (list));
			tnode_t *instlist = parser_newlistnode (OrgOf (list));
			tnode_t *vdblock = tnode_createfrom (gup.tag_DECLBLOCK, list, decllist, instlist);

			for (; i<j; i++) {
				parser_addtolist (decllist, items[i]);
				items[i] = NULL;
			}
			parser_addtolist (instlist, items[j]);
			parser_addtolist (newlist, vdblock);
			items[j] = NULL;
		}
	}
	tnode_free (list);
	*listptr = newlist;

	guppy_declify_subtree (listptr, gdl);

	return 0;
}
/*}}}*/
/*{{{  int guppy_autoseq_listtoseqlist (tnode_t **listptr, guppy_autoseq_t *gas)*/
/*
 *	used during auto-sequencing to turn lists of instructions into 'seq' nodes
 *	returns 0 on success, non-zero on failure
 */
int guppy_autoseq_listtoseqlist (tnode_t **listptr, guppy_autoseq_t *gas)
{
	tnode_t *lst = *listptr;
	int nitems = 0;
	tnode_t **items = parser_getlistitems (lst, &nitems);

	if (nitems > 0) {
		*listptr = tnode_createfrom (gup.tag_SEQ, lst, NULL, lst);
	}

	return 0;
}
/*}}}*/

/*{{{  int guppy_declify_subtree (tnode_t **tptr, guppy_declify_t *gdl)*/
/*
 *	does declify on a parse-tree (unscoped)
 *	returns 0 on success, non-zero on failure
 */
int guppy_declify_subtree (tnode_t **tptr, guppy_declify_t *gdl)
{
	if (!tptr) {
		nocc_serious ("guppy_declify_subtree(): NULL tree-pointer");
		return 1;
	} else if (!gdl) {
		nocc_serious ("guppy_declify_subtree(): NULL declify structure");
		return 1;
	} else if (!*tptr) {
		return 0;
	} else {
		tnode_modprewalktree (tptr, declify_modprewalk, gdl);
	}

	return gdl->errcount;
}
/*}}}*/
/*{{{  int guppy_autoseq_subtree (tnode_t **tptr, guppy_autoseq_t *gas)*/
/*
 *	does auto-sequencing on a parse-tree (unscoped)
 *	returns 0 on success, non-zero on failure
 */
int guppy_autoseq_subtree (tnode_t **tptr, guppy_autoseq_t *gas)
{
	if (!tptr) {
		nocc_serious ("guppy_autoseq_subtree(): NULL tree-pointer");
		return 1;
	} else if (!gas) {
		nocc_serious ("guppy_autoseq_subtree(): NULL autoseq structure");
		return 1;
	} else if (!*tptr) {
		return 0;
	} else {
		tnode_modprewalktree (tptr, autoseq_modprewalk, gas);
	}

	return gas->errcount;
}
/*}}}*/
/*{{{  int guppy_flattenseq_subtree (tnode_t **tptr)*/
/*
 *	does sequence-node flattening on a subtree (unscoped)
 *	returns 0 on success, non-zero on failure
 */
int guppy_flattenseq_subtree (tnode_t **tptr)
{
	if (!tptr) {
		nocc_serious ("guppy_flattenseq_subtree(): NULL tree-pointer");
		return 1;
	} else if (!*tptr) {
		return 0;
	} else {
		tnode_modprewalktree (tptr, flattenseq_modprewalk, NULL);
	}

	return 0;
}
/*}}}*/
/*{{{  int guppy_postscope_subtree (tnode_t **tptr)*/
/*
 *	does post-scope processing on a subtree.
 *	returns 0 on success, non-zero on failure.
 */
int guppy_postscope_subtree (tnode_t **tptr)
{
	if (!tptr) {
		nocc_serious ("guppy_postscope_subtree(): NULL tree-pointer");
		return 1;
	} else if (!*tptr) {
		return 0;
	} else {
		tnode_modprewalktree (tptr, postscope_modprewalk, NULL);
	}

	return 0;
}
/*}}}*/
/*{{{  int guppy_fetrans1_subtree (tnode_t **tptr, guppy_fetrans1_t *fe1)*/
/*
 *	does fetrans1 processing on a subtree.
 *	returns 0 on success, non-zero on failure
 */
int guppy_fetrans1_subtree (tnode_t **tptr, guppy_fetrans1_t *fe1)
{
	if (!tptr) {
		nocc_serious ("guppy_fetrans1_subtree(): NULL tree-pointer");
		fe1->error++;
		return 1;
	} else if (!*tptr) {
		return 0;
	} else {
#if 0
fhandle_printf (FHAN_STDERR, "guppy_fetrans1_subtree(): on [%s:%s]\n", (*tptr)->tag->ndef->name, (*tptr)->tag->name);
#endif
		tnode_modprewalktree (tptr, fetrans1_modprewalk, (void *)fe1);
	}

	return 0;
}
/*}}}*/
/*{{{  int guppy_fetrans1_subtree_newtemps (tnode_t **tptr, guppy_fetrans1_t *fe1)*/
/*
 *	does fetrans1 processing on a subtree, but sets up new declarations to appear at the pointer.
 *	returns 0 on success, non-zero on failure
 */
int guppy_fetrans1_subtree_newtemps (tnode_t **tptr, guppy_fetrans1_t *fe1)
{
	if (!tptr) {
		nocc_serious ("guppy_fetrans1_subtree(): NULL tree-pointer");
		fe1->error++;
		return 1;
	} else if (!*tptr) {
		return 0;
	} else {
		tnode_t *saved_decllist = fe1->decllist;
		tnode_t **saved_inspoint = fe1->inspoint;

		fe1->decllist = NULL;
		fe1->inspoint = tptr;

#if 0
fhandle_printf (FHAN_STDERR, "guppy_fetrans1_subtree(): on [%s:%s]\n", (*tptr)->tag->ndef->name, (*tptr)->tag->name);
#endif
		tnode_modprewalktree (tptr, fetrans1_modprewalk, (void *)fe1);

		fe1->decllist = saved_decllist;
		fe1->inspoint = saved_inspoint;
	}

	return 0;
}
/*}}}*/
/*{{{  int guppy_fetrans15_subtree (tnode_t **tptr, guppy_fetrans15_t *fe15)*/
/*
 *	does fetrans15 processing on a subtree.
 *	returns 0 on success, non-zero on failure
 */
int guppy_fetrans15_subtree (tnode_t **tptr, guppy_fetrans15_t *fe15)
{
	if (!tptr) {
		nocc_serious ("guppy_fetrans15_subtree(): NULL tree-pointer");
		fe15->error++;
		return 1;
	} else if (!*tptr) {
		return 0;
	} else {
#if 0
fhandle_printf (FHAN_STDERR, "guppy_fetrans1_subtree(): on [%s:%s]\n", (*tptr)->tag->ndef->name, (*tptr)->tag->name);
#endif
		tnode_modprewalktree (tptr, fetrans15_modprewalk, (void *)fe15);
	}

	return 0;
}
/*}}}*/
/*{{{  int guppy_fetrans2_subtree (tnode_t **tptr, guppy_fetrans2_t *fe2)*/
/*
 *	does fetrans2 processing on a subtree.
 *	returns 0 on success, non-zero on failure
 */
int guppy_fetrans2_subtree (tnode_t **tptr, guppy_fetrans2_t *fe2)
{
	if (!tptr) {
		nocc_serious ("guppy_fetrans2_subtree(): NULL tree-pointer");
		fe2->error++;
		return 1;
	} else if (!*tptr) {
		return 0;
	} else {
		tnode_modprewalktree (tptr, fetrans2_modprewalk, (void *)fe2);
	}

	return 0;
}
/*}}}*/
/*{{{  int guppy_fetrans3_subtree (tnode_t **tptr, guppy_fetrans3_t *fe3)*/
/*
 *	does fetrans3 processing on a subtree.
 *	returns 0 on success, non-zero on failure
 */
int guppy_fetrans3_subtree (tnode_t **tptr, guppy_fetrans3_t *fe3)
{
	if (!tptr) {
		nocc_serious ("guppy_fetrans3_subtree(): NULL tree-pointer");
		fe3->error++;
		return 1;
	} else if (!*tptr) {
		return 0;
	} else {
		tnode_modprewalktree (tptr, fetrans3_modprewalk, (void *)fe3);
	}

	return 0;
}
/*}}}*/

/*{{{  tnode_t *guppy_fetrans1_maketemp (ntdef_t *tag, tnode_t *org, tnode_t *type, tnode_t *init, guppy_fetrans1_t *fe1)*/
/*
 *	special helper for fetrans1: creating local temporaries
 *	returns name-node (tagged with 'tag').
 */
tnode_t *guppy_fetrans1_maketemp (ntdef_t *tag, tnode_t *org, tnode_t *type, tnode_t *init, guppy_fetrans1_t *fe1)
{
	char *xname = guppy_maketempname (org);
	tnode_t *ndecl, *nname;
	name_t *dname;

#if 0
fhandle_printf (FHAN_STDERR, "guppy_fetrans1_maketemp(): here!\n");
#endif
	if (!fe1->decllist) {
		tnode_t *dblk, *dilist;

		if (!fe1->inspoint) {
			nocc_internal ("guppy_fetrans1_maketemp(): no insert-point!");
		}
		dilist = parser_newlistnode (SLOCI);
		dblk = tnode_createfrom (gup.tag_DECLBLOCK, org, dilist, *fe1->inspoint);

#if 0
fhandle_printf (FHAN_STDERR, "before insert, *fe1->inspoint = [%s:%s]\n", (*fe1->inspoint)->tag->name, (*fe1->inspoint)->tag->ndef->name);
#endif
		*fe1->inspoint = dblk;
		fe1->inspoint = tnode_nthsubaddr (dblk, 1);		/* so it's still us */
		fe1->decllist = dilist;
	}

	dname = name_addname (xname, NULL, type, NULL);
	nname = tnode_createfrom (tag, org, dname);
	SetNameNode (dname, nname);
	ndecl = tnode_createfrom (gup.tag_VARDECL, org, nname, type, init);
	SetNameDecl (dname, ndecl);

	/* add to declaration list */
	parser_addtolist (fe1->decllist, ndecl);

	return nname;
}
/*}}}*/
/*{{{  char *guppy_maketempname (tnode_t *org)*/
/*
 *	make temporary (variable or function) name
 */
char *guppy_maketempname (tnode_t *org)
{
	char *str;

	str = (char *)smalloc (32);
	sprintf (str, "tmp_%d_%8.8x", tempnamecounter, (unsigned int)org);
	tempnamecounter++;

	return str;
}
/*}}}*/
/*{{{  int guppy_chantype_setinout (tnode_t *chantype, int marked_in, int marked_out)*/
/*
 *	sets in/out markers on a channel-type, done via langops
 *	returns 0 on success, non-zero on error
 */
int guppy_chantype_setinout (tnode_t *chantype, int marked_in, int marked_out)
{
	if (!chantype) {
		return -1;
	}
	if (chantype->tag->ndef->lops && tnode_haslangop (chantype->tag->ndef->lops, "chantype_setinout")) {
		return tnode_calllangop (chantype->tag->ndef->lops, "chantype_setinout", 3, chantype, marked_in, marked_out);
	}
	return -1;
}
/*}}}*/
/*{{{  int guppy_chantype_getinout (tnode_t *chantype, int *marked_in, int *marked_out)*/
/*
 *	gets in/out markers on a channel-type, done via langops
 *	returns 0 on success, non-zero on error
 */
int guppy_chantype_getinout (tnode_t *chantype, int *marked_in, int *marked_out)
{
	if (!chantype) {
		return -1;
	}
	if (chantype->tag->ndef->lops && tnode_haslangop (chantype->tag->ndef->lops, "chantype_getinout")) {
		return tnode_calllangop (chantype->tag->ndef->lops, "chantype_getinout", 3, chantype, marked_in, marked_out);
	}
	return -1;
}
/*}}}*/
/*{{{  static copycontrol_e guppy_copyctl_aliasnames (tnode_t *node)*/
/*
 *	used by function below to decide whether or not to alias a node during copy
 *	(will alias namenodes)
 */
static copycontrol_e guppy_copyctl_aliasnames (tnode_t *node)
{
	if (!node) {
		nocc_internal ("guppy_copyctl_aliasnames(): called with NULL node!");
		return COPY_ALIAS;
	}
	if (node->tag->ndef == gup.node_NAMENODE) {
		return COPY_ALIAS;
	}
	return (COPY_SUBS | COPY_HOOKS | COPY_CHOOKS);
}
/*}}}*/
/*{{{  tnode_t *guppy_copytree (tnode_t *tree)*/
/*
 *	copies a parse tree fragment, preserving (aliasing) guppy:namenode nodes
 *	returns new tree on success, NULL on failure (or copy of a null tree)
 */
tnode_t *guppy_copytree (tnode_t *tree)
{
	tnode_t *copy = tnode_copyoraliastree (tree, guppy_copyctl_aliasnames);

	return copy;
}
/*}}}*/

/*{{{  static tnode_t *guppy_make_tlp (tnode_t *userfcn)*/
/*
 *	generates the top-level process tree-structure, just prior to be-passes
 *	returns function definition on success, NULL on failure
 */
static tnode_t *guppy_make_tlp (tnode_t *userfcn)
{
	int has_screen = -1, has_error = -1, has_keyboard = -1;
	tnode_t *ch_screen = NULL, *ch_error = NULL, *ch_keyboard = NULL;
	tnode_t *params = tnode_nthsubof (userfcn, 1);
	tnode_t *tlpdef = NULL;
	tnode_t *tlparams = NULL, *tlpnamenode = NULL;
	name_t *tlpname;
	tnode_t *tlpbody, *tlpbodydlist, *tlpbodypar, *tlpbodypitems, *tlpbodyseq, *tlpbodysitems;
	tnode_t *sd_call;
	guppy_fcndefhook_t *fdh, *userfdh;
	tnode_t *ucallparams;

	userfdh = (guppy_fcndefhook_t *)tnode_nthhookof (userfcn, 0);
	if (!userfdh->pfcndef) {
		nocc_error ("guppy_make_tlp(): user process has no PFCNDEF equivalent..");
		return NULL;
	}

	ucallparams = parser_newlistnode (SLOCI);
	if (parser_islistnode (params)) {
		tnode_t **pitems;
		int i, npitems;

		pitems = parser_getlistitems (params, &npitems);
		for (i=0; i<npitems; i++) {
			tnode_t *fename;

			if (pitems[i]->tag != gup.tag_FPARAM) {
				nocc_error ("guppy_make_tlp(): parameter is not FPARAM, got [%s]", pitems[i]->tag->name);
				return NULL;
			}

#if 0
fprintf (stderr, "guppy_make_tlp(): looking at parameter %d, FPARAM of:\n", i);
tnode_dumptree (pitems[i], 1, FHAN_STDERR);
#endif
			parser_addtolist (ucallparams, NULL);
			fename = tnode_nthsubof (pitems[i], 0);
			/*{{{  figure out arrangement of top-level channels*/
			switch (langops_guesstlp (fename)) {
			default:
				nocc_error ("guppy_make_tlp(): could not guess top-level parameter usage (%d)", i);
				return NULL;
			case 1:
				if (has_keyboard >= 0) {
					nocc_error ("guppy_make_tlp(): confused, two keyboard channels? (%d)", i);
					return NULL;
				}
				has_keyboard = i;
				break;
			case 2:
				if (has_screen >= 0) {
					if (has_error >= 0) {
						nocc_error ("guppy_make_tlp(): confused, two screen channels? (%d)", i);
						return NULL;
					}
					has_error = i;
				} else {
					has_screen = i;
				}
				break;
			case 3:
				if (has_error >= 0) {
					nocc_error ("guppy_make_tlp(): confused, two error channels? (%d)", i);
					return NULL;
				}
				has_error = i;
				break;
			}

			/*}}}*/
		}
	} else {
		nocc_error ("guppy_make_tlp(): no top-level parameters, at all");
		return NULL;
	}
#if 0
fhandle_printf (FHAN_STDERR, "guppy_make_tlp(): here, has_keyboard=%d, has_screen=%d, has_error=%d\n", has_keyboard, has_screen, has_error);
#endif

	/*{{{  create empty-parameter list and name*/
	tlparams = parser_newlistnode (SLOCI);
	tlpname = name_addname ("guppy_main", NULL, tlparams, NULL);
	tlpnamenode = tnode_create (gup.tag_NPFCNDEF, SLOCI, tlpname);
	SetNameNode (tlpname, tlpnamenode);

	/*}}}*/
	/*{{{  create skeleton process declaration*/
	tlpbodydlist = parser_newlistnode (SLOCI);
	tlpbodypitems = parser_newlistnode (SLOCI);
	tlpbodypar = tnode_create (gup.tag_PAR, SLOCI, NULL, tlpbodypitems);
	tlpbodysitems = parser_newlistnode (SLOCI);
	tlpbodyseq = tnode_create (gup.tag_SEQ, SLOCI, NULL, tlpbodysitems);
	parser_addtolist (tlpbodysitems, tlpbodypar);
	parser_addtolist (tlpbodysitems, tnode_create (gup.tag_SHUTDOWN, SLOCI));
	tlpbody = tnode_create (gup.tag_DECLBLOCK, SLOCI, tlpbodydlist, tlpbodyseq);
	fdh = guppy_newfcndefhook ();
	fdh->lexlevel = 0;
	fdh->ispublic = 1;
	fdh->istoplevel = 1;
	fdh->ispar = 0;
	fdh->pfcndef = NULL;
	tlpdef = tnode_create (gup.tag_PFCNDEF, SLOCI, tlpnamenode, tlparams, tlpbody, NULL, fdh);
	SetNameDecl (tlpname, tlpdef);

	/*}}}*/
	/*{{{  create channel declarations*/
	{
		int cids[3] = {has_keyboard, has_screen, has_error};
		char *cnamestrs[3] = {"kyb_chan", "scr_chan", "err_chan"};
		tnode_t **ctargets[3] = {&ch_keyboard, &ch_screen, &ch_error};
		int j;

		for (j=0; j<3; j++) {
			if (cids[j] >= 0) {
				tnode_t *uname = tnode_nthsubof (parser_getfromlist (params, cids[j]), 0);
				tnode_t *utype = typecheck_gettype (uname, NULL);
				tnode_t *prot, *ctype, *cnamenode;
				name_t *cname;
				tnode_t *cdecl, **pptr;

				if (!utype) {
					nocc_error ("non-determined top-level parameter type? (%d)", cids[j]);
					return NULL;
				} else if (utype->tag != gup.tag_CHAN) {
					nocc_error ("top-level parameter type is not a channel [%s]", utype->tag->name);
					return NULL;
				}

				/* create channel name */
				prot = tnode_nthsubof (utype, 0);
				ctype = guppy_newchantype (gup.tag_CHAN, utype, prot);
				cname = name_addname (cnamestrs[j], NULL, ctype, NULL);
				cnamenode = tnode_create (gup.tag_NDECL, SLOCI, cname);
				SetNameNode (cname, cnamenode);
				*(ctargets[j]) = cnamenode;					/* save channel name */
				pptr = parser_getfromlistptr (ucallparams, cids[j]);
				*pptr = cnamenode;

				/* create channel declaration */
				cdecl = tnode_create (gup.tag_VARDECL, SLOCI, cnamenode, ctype, NULL);
				parser_addtolist (tlpbodydlist, cdecl);
			}
		}
	}
	/*}}}*/
	/*{{{  create instance of the user-process*/
	{
		tnode_t *inst;

		inst = tnode_create (gup.tag_PPINSTANCE, SLOCI, tnode_nthsubof (userfdh->pfcndef, 0), NULL, ucallparams);
		parser_addtolist (tlpbodypitems, inst);
	}
	/*}}}*/
	/*{{{  create instances of interface processes*/
	{
		int cids[3] = {has_keyboard, has_screen, has_error};
		char *cprocstrs[3] = {"guppy_keyboard_process", "guppy_screen_process", "guppy_error_process"};
		tnode_t *cchans[3] = {ch_keyboard, ch_screen, ch_error};
		int j;

		for (j=0; j<3; j++) {
			if (cids[j] >= 0) {
				name_t *fcname = name_lookup (cprocstrs[j]);
				tnode_t *ndecl, *nname;
				guppy_fcndefhook_t *fdh;
				tnode_t *inst, *iparms;

				if (!fcname) {
					nocc_error ("guppy_make_tlp(): failed to find a process called \"%s\"", cprocstrs[j]);
					return NULL;
				}
				ndecl = NameDeclOf (fcname);
				nname = NameNodeOf (fcname);

				if (ndecl->tag != gup.tag_FCNDEF) {
					nocc_error ("guppy_make_tlp(): looked up \"%s\" but not a function [%s:%s]",
							cprocstrs[j], ndecl->tag->ndef->name, ndecl->tag->name);
					return NULL;
				}

				fdh = (guppy_fcndefhook_t *)tnode_nthhookof (ndecl, 0);
				if (!fdh) {
					nocc_error ("guppy_make_tlp(): function \"%s\" has no function-hook", cprocstrs[j]);
					return NULL;
				} else if (!fdh->pfcndef) {
					nocc_error ("guppy_make_tlp(): function \"%s\" has no pfcn attached", cprocstrs[j]);
					return NULL;
				}

				/* switch to the PFCNDEF */
				ndecl = fdh->pfcndef;
				nname = tnode_nthsubof (ndecl, 0);

				iparms = parser_newlistnode (SLOCI);
				parser_addtolist (iparms, cchans[j]);
				inst = tnode_create (gup.tag_PPINSTANCE, SLOCI, nname, NULL, iparms);
				parser_addtolist (tlpbodypitems, inst);

#if 0
fhandle_printf (FHAN_STDERR, "guppy_make_tlp(): looked up \"%s\", found:\n", cprocstrs[j]);
if (fcname) {
	tnode_dumptree (nname, 1, FHAN_STDERR);
}
#endif
			}
		}
	}
	/*}}}*/

#if 0
fhandle_printf (FHAN_STDERR, "created a top-level process:\n");
tnode_dumptree (tlpdef, 1, FHAN_STDERR);
#endif
	return tlpdef;
}
/*}}}*/
/*{{{  static int guppy_insert_tlp_shutdown (tnode_t *fcndef)*/
/*
 *	adjusts a top-level function definition (always FCNDEF) to include a shutdown call as the last thing before returning
 *	returns 0 on success, non-zero on failure
 */
static int guppy_insert_tlp_shutdown (tnode_t *fcndef)
{
	tnode_t *body, *slist;
	
	if (fcndef->tag != gup.tag_FCNDEF) {
		nocc_error ("guppy_insert_tlp_shutdown(): top-level function not FCNDEF, got [%s:%s]", fcndef->tag->ndef->name, fcndef->tag->name);
		return -1;
	}
	body = tnode_nthsubof (fcndef, 2);
	if (body->tag == gup.tag_SEQ) {
		slist = tnode_nthsubof (body, 1);
	} else {
		/* insert SEQ into process */
		tnode_t *newseq;

		slist = parser_newlistnode (SLOCI);
		newseq = tnode_create (gup.tag_SEQ, SLOCI, NULL, slist);
		parser_addtolist (slist, body);
		tnode_setnthsub (fcndef, 2, newseq);
		body = newseq;
	}
	parser_addtolist (slist, tnode_create (gup.tag_SHUTDOWN, SLOCI));

	return 0;
}
/*}}}*/
/*{{{  static int declify_cpass (tnode_t **treeptr)*/
/*
 *	called to do the compiler-pass for making declaration blocks
 *	returns 0 on success, non-sero on failure
 */
static int declify_cpass (tnode_t **treeptr)
{
	guppy_declify_t *gdl = (guppy_declify_t *)smalloc (sizeof (guppy_declify_t));
	int err = 0;

	gdl->errcount = 0;
	guppy_declify_subtree (treeptr, gdl);
	err = gdl->errcount;
	sfree (gdl);

	return err;
}
/*}}}*/
/*{{{  static int autoseq_cpass (tnode_t **treeptr)*/
/*
 *	called to do the compiler-pass for auto-sequencing code, applies to particular nodes only
 *	returns 0 on success, non-zero on failure
 */
static int autoseq_cpass (tnode_t **treeptr)
{
	guppy_autoseq_t *gas = (guppy_autoseq_t *)smalloc (sizeof (guppy_autoseq_t));
	int err = 0;

	gas->errcount = 0;
	guppy_autoseq_subtree (treeptr, gas);
	err = gas->errcount;
	sfree (gas);

	return err;
}
/*}}}*/
/*{{{  static int flattenseq_cpass (tnode_t **treeptr)*/
/*
 *	called to do the compiler-pass for flattening sequence nodes
 *	returns 0 on success, non-zero on failure
 */
static int flattenseq_cpass (tnode_t **treeptr)
{
	guppy_flattenseq_subtree (treeptr);
	return 0;
}
/*}}}*/
/*{{{  static int postscope_cpass (tnode_t **treeptr)*/
/*
 *	called to do a post-scope pass over the tree -- unpicks initialising declarations and similar
 *	returns 0 on success, non-zero on failure
 */
static int postscope_cpass (tnode_t **treeptr)
{
	/* if main module, figure out which one is last and make public */
	guppy_postscope_subtree (treeptr);

	if (!compopts.notmainmodule) {
		tnode_t **items;
		int i, nitems;

		if (!parser_islistnode (*treeptr)) {
			nocc_serious ("postscope_cpass(): top-level tree is not a list, got [%s:%s]", (*treeptr)->tag->ndef->name, (*treeptr)->tag->name);
			return -1;
		}

		items = parser_getlistitems (*treeptr, &nitems);
		for (i=nitems-1; i>=0; i--) {
			if (items[i]->tag == gup.tag_FCNDEF) {
				guppy_fcndefhook_t *fdh = (guppy_fcndefhook_t *)tnode_nthhookof (items[i], 0);

				if (!fdh) {
					nocc_serious ("postscope_cpass(): last function definition has no fcndefhook");
					return -1;
				}
				fdh->istoplevel = 1;
				break;
			}
		}
	}
	return 0;
}
/*}}}*/
/*{{{  static int fetrans1_cpass (tnode_t **treeptr)*/
/*
 *	called to do the fetrans1 compiler-pass
 *	returns 0 on success, non-zero on failure
 */
static int fetrans1_cpass (tnode_t **treeptr)
{
	guppy_fetrans1_t *fe1 = guppy_newfetrans1 ();
	int err;

	guppy_fetrans1_subtree (treeptr, fe1);

	err = fe1->error;
	guppy_freefetrans1 (fe1);

	return err;
}
/*}}}*/
/*{{{  static int fetrans15_cpass (tnode_t **treeptr)*/
/*
 *	called to do the fetrans1.5 compiler-pass
 *	returns 0 on success, non-zero on failure
 */
static int fetrans15_cpass (tnode_t **treeptr)
{
	guppy_fetrans15_t *fe15 = guppy_newfetrans15 ();
	int err;

	guppy_fetrans15_subtree (treeptr, fe15);

	err = fe15->error;
	guppy_freefetrans15 (fe15);

	return err;
}
/*}}}*/
/*{{{  static int fetrans2_cpass (tnode_t **treeptr)*/
/*
 *	called to do the fetrans2 compiler-pass
 *	returns 0 on success, non-zero on failure
 */
static int fetrans2_cpass (tnode_t **treeptr)
{
	guppy_fetrans2_t *fe2 = guppy_newfetrans2 ();
	int err;

	/* might have some library stuff at the top */
	while (*treeptr && ((*treeptr)->tag->ndef->tn_flags & TNF_TRANSPARENT)) {
		treeptr = tnode_nthsubaddr (*treeptr, 0);
	}

	guppy_fetrans2_subtree (treeptr, fe2);

	err = fe2->error;
	guppy_freefetrans2 (fe2);

	if (!err) {
		/* before we finish here, insert MAPINIT for language-specific initialisation in namemap */
		tnode_t *minode = tnode_create (gup.tag_MAPINIT, NULL);

		if (!parser_islistnode (*treeptr)) {
			nocc_internal ("fetrans2_cpass(): top-level tree not a list.. [%s:%s]", (*treeptr)->tag->ndef->name, (*treeptr)->tag->name);
			return -1;
		}
		parser_addtolist_front (*treeptr, minode);
	}

	return err;
}
/*}}}*/
/*{{{  static int fetrans3_cpass (tnode_t **treeptr)*/
/*
 *	called to do the fetrans3 compiler-pass
 *	returns 0 on success, non-zero on failure
 */
static int fetrans3_cpass (tnode_t **treeptr)
{
	guppy_fetrans3_t *fe3 = guppy_newfetrans3 ();
	int err;

	guppy_fetrans3_subtree (treeptr, fe3);

	err = fe3->error;
	guppy_freefetrans3 (fe3);

	return err;
}
/*}}}*/
/*{{{  static int fetrans4_cpass (tnode_t **treeptr)*/
/*
 *	called to do the fetrans4 compiler-pass: this constructs the top-level process if building an executable
 *	returns 0 on success, non-zero on failure
 */
static int fetrans4_cpass (tnode_t **treeptr)
{
	int nitems, i;
	tnode_t **items;
	tnode_t *tlfcn, *tlpdef;

	if (compopts.notmainmodule) {
		/* nothing to do in this particular case */
		return 0;
	}

	/* might have some library stuff at the top */
	while (*treeptr && ((*treeptr)->tag->ndef->tn_flags & TNF_TRANSPARENT)) {
		treeptr = tnode_nthsubaddr (*treeptr, 0);
	}

	if (!parser_islistnode (*treeptr)) {
		nocc_internal ("fetrans4_cpass(): top-level tree not a list.. [%s:%s]", (*treeptr)->tag->ndef->name, (*treeptr)->tag->name);
		return -1;
	}

	items = parser_getlistitems (*treeptr, &nitems);
	tlfcn = NULL;
	for (i=nitems-1; i>=0; i--) {
		if (items[i]->tag == gup.tag_FCNDEF) {
			name_t *fname = tnode_nthnameof (tnode_nthsubof (items[i], 0), 0);

			if (!tlfcn) {
				/* going backwards, so this if not set */
				tlfcn = items[i];
			} else if (!strcmp (NameNameOf (fname), "main")) {
				/* if there's one called 'main', use it regardless */
				tlfcn = items[i];
			}
		}
	}
	if (!tlfcn) {
		nocc_error ("fetrans4_cpass(): failed to find top-level process, giving up..");
		return -1;
	}

	if (parser_islistnode (tnode_nthsubof (tlfcn, 1)) && !parser_countlist (tnode_nthsubof (tlfcn, 1))) {
		/* empty parameter list at top-level, so just use this -- do, however, insert shutdown call as last thing*/
		if (guppy_insert_tlp_shutdown (tlfcn)) {
			nocc_error ("fetrans4_cpass(): failed to add shutdown at end of top-level process, giving up..");
			return -1;
		}
	} else {
		tlpdef = guppy_make_tlp (tlfcn);
		if (!tlpdef) {
			nocc_error ("fetrans4_cpass(): failed to create new top-level process, giving up..");
			return -1;
		}
		parser_addtolist (*treeptr, tlpdef);
	}

	return 0;
}
/*}}}*/

/*{{{  static tnode_t *guppy_includefile (char *fname, lexfile_t *curlf)*/
/*
 *	includes a file
 *	returns a tree or NULL
 */
static tnode_t *guppy_includefile (char *fname, lexfile_t *curlf)
{
	tnode_t *tree;
	lexfile_t *lf;

	lf = lexer_open (fname);
	if (!lf) {
		parser_error (SLOCN (curlf), "failed to open @include'd file %s", fname);
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
		parser_error (SLOCN (curlf), "failed to parse @include'd file %s", fname);
		lexer_close (lf);
		return NULL;
	}
	lexer_close (lf);

	return tree;
}
/*}}}*/
/*{{{  static tnode_t *guppy_parsemoduledef (lexfile_t *lf)*/
/*
 *	parses a module (library) definition block.
 *	returns a new library libnode, or NULL on failure
 */
static tnode_t *guppy_parsemoduledef (lexfile_t *lf)
{
	token_t *tok;
	tnode_t *tree = NULL;
	char *modname = NULL;

	/* when we enter here, just parsed the @module bit */
	tok = lexer_nexttoken (lf);
	if (tok->type == STRING) {
		modname = string_ndup (tok->u.str.ptr, tok->u.str.len);

		lexer_freetoken (tok);
		tok = lexer_nexttoken (lf);
	} else if ((tok->type == COMMENT) || (tok->type == NEWLINE)) {
		/* assume same name as source, if available */
	} else {
		parser_error (SLOCN (lf), "expected module name or nothing, but found '%s' instead", lexer_stokenstr (tok));
		lexer_freetoken (tok);
		return NULL;
	}

	while ((tok->type == COMMENT) || (tok->type == NEWLINE)) {
		lexer_freetoken (tok);
		tok = lexer_nexttoken (lf);
	}

	tree = library_newlibnode (lf, modname);

	/* either got all we're getting, or expect intented list of stuff */
	if (tok->type == INDENT) {
		lexer_freetoken (tok);
		tok = lexer_nexttoken (lf);

		while (tok->type != OUTDENT) {
			/*{{{  check contents of @module block*/
			if ((tok->type == COMMENT) || (tok->type == NEWLINE)) {
				/* consume quietly */
			} else if (lexer_tokmatch (gup.tok_ATSIGN, tok)) {
				lexer_freetoken (tok);
				tok = lexer_nexttoken (lf);

				if (lexer_tokmatchlitstr (tok, "version")) {
					/*{{{  version: expect string*/
					char *vstr;

					lexer_freetoken (tok);
					tok = lexer_nexttoken (lf);

					if (tok->type != STRING) {
						parser_error (SLOCN (lf), "expected string, but found '%s' instead", lexer_stokenstr (tok));
						goto skip_to_outdent;
					}

					vstr = string_ndup (tok->u.str.ptr, tok->u.str.len);

					library_setversion (tree, vstr);

#if 0
fprintf (stderr, "guppy_parsemoduledef(): FIXME: handle version! [%s]\n", vstr);
#endif
					sfree (vstr);

					/*}}}*/
				} else if (lexer_tokmatchlitstr (tok, "api")) {
					/*{{{  api: expect integer*/
					int vapi;

					lexer_freetoken (tok);
					tok = lexer_nexttoken (lf);

					if (tok->type != INTEGER) {
						parser_error (SLOCN (lf), "expected number (integer), but found '%s' instead", lexer_stokenstr (tok));
						goto skip_to_outdent;
					}

					vapi = tok->u.ival;

					library_setapi (tree, vapi);
#if 0
fprintf (stderr, "guppy_parsemoduledef(): FIXME: handle API [%d]\n", vapi);
#endif
					/*}}}*/
				} else if (lexer_tokmatchlitstr (tok, "nativelib")) {
					/*{{{  nativelib: expect string*/
					char *lstr;

					lexer_freetoken (tok);
					tok = lexer_nexttoken (lf);

					if (tok->type != STRING) {
						parser_error (SLOCN (lf), "expected string, but found '%s' instead", lexer_stokenstr (tok));
						goto skip_to_outdent;
					}

					lstr = string_ndup (tok->u.str.ptr, tok->u.str.len);

					library_setnativelib (tree, lstr);

					sfree (lstr);
					/*}}}*/
				} else if (lexer_tokmatchlitstr (tok, "uses")) {
					/*{{{  uses: expect string*/
					char *ustr;

					lexer_freetoken (tok);
					tok = lexer_nexttoken (lf);

					if (tok->type != STRING) {
						parser_error (SLOCN (lf), "expected string, but found '%s' instead", lexer_stokenstr (tok));
						goto skip_to_outdent;
					}

					ustr = string_ndup (tok->u.str.ptr, tok->u.str.len);

					library_adduses (tree, ustr);

					sfree (ustr);
					/*}}}*/
				} else if (lexer_tokmatchlitstr (tok, "includes")) {
					/*{{{  includes: expect string*/
					char *istr;

					lexer_freetoken (tok);
					tok = lexer_nexttoken (lf);

					if (tok->type != STRING) {
						parser_error (SLOCN (lf), "expected string, but found '%s' instead", lexer_stokenstr (tok));
						goto skip_to_outdent;
					}

					istr = string_ndup (tok->u.str.ptr, tok->u.str.len);

					library_adduses (tree, istr);

					sfree (istr);
					/*}}}*/
				} else {
					parser_error (SLOCN (lf), "expected version|api|nativelib|uses|includes, found '%s'", lexer_stokenstr (tok));
					goto skip_to_outdent;
				}
			} else {
				parser_error (SLOCN (lf), "expected module options, but found '%s' instead", lexer_stokenstr (tok));
				goto skip_to_outdent;
			}
			/* when we get out of the above, 'tok' is the last relevant token in some correct parsing (comment or newline) */

			lexer_freetoken (tok);
			tok = lexer_nexttoken (lf);
			/*}}}*/
		}

		if (tok->type == OUTDENT) {
			/* eat it up */
			lexer_freetoken (tok);
			tok = NULL;
		} else {
			/* with what we have left, push back into the lexer */
			lexer_pushback (lf, tok);
		}
	} else {
		lexer_pushback (lf, tok);
	}

	return tree;

skip_to_outdent:
	if (tree) {
		/* destroy it */
		tnode_free (tree);
		tree = NULL;
	}
	while ((tok->type != OUTDENT) && (tok->type != END)) {
		lexer_freetoken (tok);
		tok = lexer_nexttoken (lf);
	}
	lexer_freetoken (tok);
	return NULL;
}
/*}}}*/
/*{{{  static tnode_t *guppy_parsemodusedef (lexfile_t *lf)*/
/*
 *	parses a module (library) usage block.
 *	returns a library-use node on success, NULL on failure
 */
static tnode_t *guppy_parsemodusedef (lexfile_t *lf)
{
	token_t *tok;
	tnode_t *tree = NULL;
	char *modname = NULL;
	char *asname = NULL;

	/* when we enter here, just parsed the @use bit */
	tok = lexer_nexttoken (lf);
	if (tok->type != STRING) {
		parser_error (SLOCN (lf), "expected string, found '%s'", lexer_stokenstr (tok));
		lexer_freetoken (tok);
		return NULL;
	}

	modname = string_ndup (tok->u.str.ptr, tok->u.str.len);
	lexer_freetoken (tok);
	tok = lexer_nexttoken (lf);

	/* either 'as "name"' or comment/newline */
	if (lexer_tokmatchlitstr (tok, "as")) {
		lexer_freetoken (tok);
		tok = lexer_nexttoken (lf);

		if (tok->type != STRING) {
			parser_error (SLOCN (lf), "expected string, found '%s'", lexer_stokenstr (tok));
			lexer_freetoken (tok);
			return NULL;
		}

		asname = string_ndup (tok->u.str.ptr, tok->u.str.len);
		lexer_freetoken (tok);
		tok = lexer_nexttoken (lf);
	}

	if ((tok->type != COMMENT) && (tok->type != NEWLINE)) {
		parser_error (SLOCN (lf), "expected end of line, found '%s'", lexer_stokenstr (tok));
		lexer_freetoken (tok);
		return NULL;
	}
	while ((tok->type == COMMENT) || (tok->type == NEWLINE)) {
		/* gobble up */
		lexer_freetoken (tok);
		tok = lexer_nexttoken (lf);
	}

	/* might have some indented conditionals */
	if (tok->type == INDENT) {
		lexer_freetoken (tok);
		tok = lexer_nexttoken (lf);

		/* FIXME: needs implementing! */
		while ((tok->type != OUTDENT) && (tok->type != END)) {
			lexer_freetoken (tok);
			tok = lexer_nexttoken (lf);
		}
		if (tok->type != OUTDENT) {
			parser_error (SLOCN (lf), "expected outdent, found '%s'", lexer_stokenstr (tok));
		} else {
			lexer_freetoken (tok);
			tok = lexer_nexttoken (lf);
		}
	}

	/* whatever it was, push back */
	lexer_pushback (lf, tok);

	tree = library_newusenode (lf, modname);
	if (asname) {
		library_setusenamespace (tree, asname);
		sfree (asname);
	}

	sfree (modname);

	return tree;
}
/*}}}*/
/*{{{  static int guppy_parser_init (lexfile_t *lf)*/
/*
 *	initialises the Guppy parser
 *	returns 0 on success, non-zero on error
 */
static int guppy_parser_init (lexfile_t *lf)
{
	if (!guppy_priv) {
		keyword_t *kw;
		int stopat;

		guppy_priv = guppy_newguppyparse ();

		if (compopts.verbose) {
			nocc_message ("initialising guppy parser..");
		}

		memset ((void *)&gup, 0, sizeof (gup));

		guppy_priv->langdefs = langdef_readdefs ("guppy.ldef");
		if (!guppy_priv->langdefs) {
			nocc_error ("guppy_parser_init(): failed to load language definitions!");
			return 1;
		}

		/*{{{  register some particular tokens (for later comparison)*/
		gup.tok_ATSIGN = lexer_newtoken (SYMBOL, "@");
		gup.tok_STRING = lexer_newtoken (STRING, NULL);
		gup.tok_PUBLIC = lexer_newtoken (KEYWORD, "public");

		/*}}}*/
		/*{{{  register some general reduction functions*/
		fcnlib_addfcn ("guppy_nametoken_to_hook", (void *)guppy_nametoken_to_hook, 1, 1);
		fcnlib_addfcn ("guppy_reduce_checkfixio", (void *)guppy_reduce_checkfixio, 0, 3);

		/*}}}*/
		/*{{{  add compiler passes that will be used to pick apart declaration scope and do auto-seq, plus extra fetrans passes*/
		stopat = nocc_laststopat() + 1;
		opts_add ("stop-declify", '\0', guppy_opthandler_stopat, (void *)stopat, "1stop after declify pass");
		if (nocc_addcompilerpass ("declify", INTERNAL_ORIGIN, "pre-scope", 0, (int (*)(void *))declify_cpass, CPASS_TREEPTR, stopat, NULL)) {
			nocc_serious ("guppy_parser_init(): failed to add \"declify\" compiler pass");
			return 1;
		}

		stopat = nocc_laststopat() + 1;
		opts_add ("stop-auto-sequence", '\0', guppy_opthandler_stopat, (void *)stopat, "1stop after auto-sequence pass");
		if (nocc_addcompilerpass ("auto-sequence", INTERNAL_ORIGIN, "declify", 0, (int (*)(void *))autoseq_cpass, CPASS_TREEPTR, stopat, NULL)) {
			nocc_serious ("guppy_parser_init(): failed to add \"auto-sequence\" compiler pass");
			return 1;
		}

		stopat = nocc_laststopat() + 1;
		opts_add ("stop-flattenseq", '\0', guppy_opthandler_stopat, (void *)stopat, "1stop after flatten-sequence pass");
		if (nocc_addcompilerpass ("flattenseq", INTERNAL_ORIGIN, "auto-sequence", 0, (int (*)(void *))flattenseq_cpass, CPASS_TREEPTR, stopat, NULL)) {
			nocc_serious ("guppy_parser_init(): failed to add \"flattenseq\" compiler pass");
			return 1;
		}

		stopat = nocc_laststopat() + 1;
		opts_add ("stop-postscope", '\0', guppy_opthandler_stopat, (void *)stopat, "1stop after post-scope pass");
		if (nocc_addcompilerpass ("postscope", INTERNAL_ORIGIN, "scope", 0, (int (*)(void *))postscope_cpass, CPASS_TREEPTR, stopat, NULL)) {
			nocc_serious ("guppy_parser_init(): failed to add \"postscope\" compiler pass");
			return 1;
		}

		stopat = nocc_laststopat() + 1;
		opts_add ("stop-fetrans1", '\0', guppy_opthandler_stopat, (void *)stopat, "1stop after fetrans1 pass");
		if (nocc_addcompilerpass ("fetrans1", INTERNAL_ORIGIN, "fetrans", 0, (int (*)(void *))fetrans1_cpass, CPASS_TREEPTR, stopat, NULL)) {
			nocc_serious ("guppy_parser_init(): failed to add \"fetrans1\" compiler pass");
			return 1;
		}

		stopat = nocc_laststopat() + 1;
		opts_add ("stop-fetrans15", '\0', guppy_opthandler_stopat, (void *)stopat, "1stop after fetrans15 pass");
		if (nocc_addcompilerpass ("fetrans15", INTERNAL_ORIGIN, "fetrans1", 0, (int (*)(void *))fetrans15_cpass, CPASS_TREEPTR, stopat, NULL)) {
			nocc_serious ("guppy_parser_init(): failed to add \"fetrans15\" compiler pass");
			return 1;
		}

		stopat = nocc_laststopat() + 1;
		opts_add ("stop-fetrans2", '\0', guppy_opthandler_stopat, (void *)stopat, "1stop after fetrans2 pass");
		if (nocc_addcompilerpass ("fetrans2", INTERNAL_ORIGIN, "fetrans15", 0, (int (*)(void *))fetrans2_cpass, CPASS_TREEPTR, stopat, NULL)) {
			nocc_serious ("guppy_parser_init(): failed to add \"fetrans2\" compiler pass");
			return 1;
		}

		stopat = nocc_laststopat() + 1;
		opts_add ("stop-fetrans3", '\0', guppy_opthandler_stopat, (void *)stopat, "1stop after fetrans3 pass");
		if (nocc_addcompilerpass ("fetrans3", INTERNAL_ORIGIN, "fetrans2", 0, (int (*)(void *))fetrans3_cpass, CPASS_TREEPTR, stopat, NULL)) {
			nocc_serious ("guppy_parser_init(): failed to add \"fetrans3\" compiler pass");
			return 1;
		}

		stopat = nocc_laststopat() + 1;
		opts_add ("stop-fetrans4", '\0', guppy_opthandler_stopat, (void *)stopat, "1stop after fetrans4 pass");
		if (nocc_addcompilerpass ("fetrans4", INTERNAL_ORIGIN, "fetrans3", 0, (int (*)(void *))fetrans4_cpass, CPASS_TREEPTR, stopat, NULL)) {
			nocc_serious ("guppy_parser_init(): failed to add \"fetrans4\" compiler pass");
			return 1;
		}

		/*}}}*/
		/*{{{  create new compiler operations for additional passes*/
		if (tnode_newcompop ("declify", COPS_INVALID, 2, INTERNAL_ORIGIN) < 0) {
			nocc_serious ("guppy_parser_init(): failed to add \"declify\" compiler operation");
			return 1;
		}
		if (tnode_newcompop ("autoseq", COPS_INVALID, 2, INTERNAL_ORIGIN) < 0) {
			nocc_serious ("guppy_parser_init(): failed to add \"autoseq\" compiler operation");
			return 1;
		}
		if (tnode_newcompop ("flattenseq", COPS_INVALID, 1, INTERNAL_ORIGIN) < 0) {
			nocc_serious ("guppy_parser_init(): failed to add \"flattenseq\" compiler operation");
			return 1;
		}
		if (tnode_newcompop ("postscope", COPS_INVALID, 1, INTERNAL_ORIGIN) < 0) {
			nocc_serious ("guppy_parser_init(): failed to add \"postscope\" compiler operation");
			return 1;
		}
		if (tnode_newcompop ("fetrans1", COPS_INVALID, 2, INTERNAL_ORIGIN) < 0) {
			nocc_serious ("guppy_parser_init(): failed to add \"fetrans1\" compiler operation");
			return 1;
		}
		if (tnode_newcompop ("fetrans15", COPS_INVALID, 2, INTERNAL_ORIGIN) < 0) {
			nocc_serious ("guppy_parser_init(): failed to add \"fetrans15\" compiler operation");
			return 1;
		}
		if (tnode_newcompop ("fetrans2", COPS_INVALID, 2, INTERNAL_ORIGIN) < 0) {
			nocc_serious ("guppy_parser_init(): failed to add \"fetrans2\" compiler operation");
			return 1;
		}
		if (tnode_newcompop ("fetrans3", COPS_INVALID, 2, INTERNAL_ORIGIN) < 0) {
			nocc_serious ("guppy_parser_init(): failed to add \"fetrans3\" compiler operation");
			return 1;
		}

		/*}}}*/
		/*{{{  create new language operations (needed for Guppy)*/
		tnode_newlangop ("chantype_setinout", LOPS_INVALID, 3, origin_langparser (&guppy_parser));
		tnode_newlangop ("chantype_getinout", LOPS_INVALID, 3, origin_langparser (&guppy_parser));

		/*}}}*/
		/*{{{  initialise!*/
		if (feunit_do_init_tokens (0, guppy_priv->langdefs, origin_langparser (&guppy_parser))) {
			nocc_error ("guppy_parser_init(): failed to initialise tokens");
			return 1;
		}
		if (feunit_do_init_nodes (feunit_set, 1, guppy_priv->langdefs, origin_langparser (&guppy_parser))) {
			nocc_error ("guppy_parser_init(): failed to initialise nodes");
			return 1;
		}
		if (feunit_do_reg_reducers (feunit_set, 0, guppy_priv->langdefs)) {
			nocc_error ("guppy_parser_init(): failed to register reducers");
			return 1;
		}
		if (feunit_do_init_dfatrans (feunit_set, 1, guppy_priv->langdefs, &guppy_parser, 1)) {
			nocc_error ("guppy_parser_init(): failed to initialise DFAs");
			return 1;
		}
		if (feunit_do_post_setup (feunit_set, 1, guppy_priv->langdefs)) {
			nocc_error ("guppy_parser_init(): failed to post-setup");
			return 1;
		}
		if (langdef_treecheck_setup (guppy_priv->langdefs)) {
			nocc_serious ("guppy_parser_init(): failed to initialise tree-checking!");
		}

		guppy_priv->inode = dfa_lookupbyname ("guppy:decl");
		if (!guppy_priv->inode) {
			nocc_error ("guppy_parser_init(): could not find guppy:decl!");
			return 1;
		}

		if (guppy_udo_init ()) {
			nocc_error ("guppy_parser_init(): failed to initialise user-defined operators!");
			return 1;
		}

		if (compopts.dumpdfas) {
			dfa_dumpdfas (FHAN_STDERR);
		}
		if (compopts.dumpgrules) {
			parser_dumpgrules (FHAN_STDERR);
		}

		/*}}}*/
		/* last, re-init multiway syncs with default end-of-par option */
		mwsync_setresignafterpar (0);

		parser_gettesttags (&testtruetag, &testfalsetag);
	}
	return 0;
}
/*}}}*/
/*{{{  static void guppy_parser_shutdown (lexfile_t *lf)*/
/*
 *	shuts-down the Guppy parser
 */
static void guppy_parser_shutdown (lexfile_t *lf)
{
	guppy_udo_shutdown ();
	return;
}
/*}}}*/


/*{{{  static tnode_t *guppy_parse_preproc (lexfile_t *lf)*/
/*
 *	parses a pre-processor directive of some form (starting '@').
 *	returns tree on success, NULL on error
 */
static tnode_t *guppy_parse_preproc (lexfile_t *lf)
{
	token_t *tok;
	tnode_t *tree = NULL;

	tok = lexer_nexttoken (lf);
	if (!lexer_tokmatch (gup.tok_ATSIGN, tok)) {
		parser_error (SLOCN (lf), "expected to find '@', but found '%s' instead", lexer_stokenstr (tok));
		return NULL;
	}
	lexer_freetoken (tok);
	tok = lexer_nexttoken (lf);
	if (lexer_tokmatchlitstr (tok, "include")) {
		/*{{{  included file*/
		char *ifile;

		lexer_freetoken (tok);
		tok = lexer_nexttoken (lf);

		if (tok->type != STRING) {
			parser_error (SLOCN (lf), "expected string, but found '%s'", lexer_stokenstr (tok));
			goto skip_to_eol;
		}

		ifile = string_ndup (tok->u.str.ptr, tok->u.str.len);
		lexer_freetoken (tok);

		tree = guppy_includefile (ifile, lf);

		/*}}}*/
	} else if (lexer_tokmatchlitstr (tok, "comment")) {
		/*{{{  pre-processor comment*/
		char *str;

		lexer_freetoken (tok);
		tok = lexer_nexttoken (lf);

		if (tok->type != STRING) {
			parser_error (SLOCN (lf), "expected string, but found '%s'", lexer_stokenstr (tok));
			goto skip_to_eol;
		}

		str = string_ndup (tok->u.str.ptr, tok->u.str.len);
		lexer_freetoken (tok);

		tree = tnode_create (gup.tag_PPCOMMENT, SLOCN (lf), guppy_makestringlit (guppy_newprimtype (gup.tag_STRING, NULL, 0), NULL, str));
		sfree (str);

#if 0
fprintf (stderr, "guppy_parser_preproc(): here!  created comment node:\n");
tnode_dumptree (tree, 1, FHAN_STDERR);
#endif

		/*}}}*/
	} else if (lexer_tokmatchlitstr (tok, "external")) {
		/*{{{  some sort of external declaration*/
		char *langstr, *descstr;
		lexfile_t *blf;

		lexer_freetoken (tok);
		tok = lexer_nexttoken (lf);

		if (tok->type != STRING) {
			parser_error (SLOCN (lf), "expected language string, but found '%s'", lexer_stokenstr (tok));
			goto skip_to_eol;
		}

		langstr = string_ndup (tok->u.str.ptr, tok->u.str.len);
		lexer_freetoken (tok);

		tok = lexer_nexttoken (lf);

		if (tok->type != STRING) {
			parser_error (SLOCN (lf), "expected descriptor string, but found '%s'", lexer_stokenstr (tok));
			goto skip_to_eol;
		}

		descstr = string_ndup (tok->u.str.ptr, tok->u.str.len);
		lexer_freetoken (tok);

		/* attempt to open the buffer as a new lex-file and parse it */
		blf = lexer_openbuf (NULL, langstr, descstr);
		if (!blf) {
			parser_error (SLOCN (lf), "failed to open descriptor string as buffer");
			goto skip_to_eol;
		}

		tree = parser_descparse (blf);
		if (!tree) {
			parser_error (SLOCN (lf), "failed to parse external declaration");
			lexer_close (blf);
			goto skip_to_eol;
		}

		lexer_close (blf);

		/*}}}*/
	} else if (lexer_tokmatchlitstr (tok, "module")) {
		/*{{{  module definition (building into a library)*/
		lexer_freetoken (tok);

		tree = guppy_parsemoduledef (lf);
		if (!tree) {
			goto skip_to_eol;		/* abandon! */
		}

		/*}}}*/
	} else if (lexer_tokmatchlitstr (tok, "use")) {
		/*{{{  import module*/
		lexer_freetoken (tok);

		tree = guppy_parsemodusedef (lf);
		if (!tree) {
			goto skip_to_eol;		/* abandon! */
		}

		/*}}}*/
	} else {
		parser_error (SLOCN (lf), "unknown pre-processor directive '%s'", lexer_stokenstr (tok));
		return NULL;
	}

	/* if we get here, means we're done with whatever, expect comment and/or newline */
	goto expect_end;

skip_to_eol:
	/* consume everything until we find a newline */
	tok = lexer_nexttoken (lf);
	while ((tok->type != NEWLINE) && (tok->type != NOTOKEN) && (tok->type != END)) {
		lexer_freetoken (tok);
		tok = lexer_nexttoken (lf);
	}
	if (tok->type == NEWLINE) {
		lexer_pushback (lf, tok);
	} else {
		lexer_freetoken (tok);
	}

expect_end:
	/* skip comments to newline */
	tok = lexer_nexttoken (lf);
	while (tok->type == COMMENT) {
		lexer_freetoken (tok);
		tok = lexer_nexttoken (lf);
	}

#if 0
fprintf (stderr, "guppy_parse_preproc(): here!  token is:\n");
lexer_dumptoken (FHAN_STDERR, tok);
#endif
	if ((tok->type == NOTOKEN) || (tok->type == END)) {
		/* EOF */
		lexer_freetoken (tok);
	} else {
		lexer_pushback (lf, tok);
	}

	return tree;
}
/*}}}*/
/*{{{  static int guppy_skiptoeol (lexfile_t *lf, int skipindent)*/
/*
 *	skips the lexer to the end of a line;  if 'skipindent' is non-zero, will ignore anything indented
 *	to the end of the line.
 *	returns 0 on success (skipped ok), non-zero otherwise
 */
static int guppy_skiptoeol (lexfile_t *lf, int skipindent)
{
	int icount = 0;
	token_t *tok;

	for (;;) {
		tok = lexer_nexttoken (lf);
		if (tok->type == END) {
			/* unexpected */
			lexer_pushback (lf, tok);
			return -1;
		} else if (skipindent && (tok->type == INDENT)) {
			icount++;
		} else if (skipindent && (tok->type == OUTDENT)) {
			icount--;
		} else if ((tok->type == NEWLINE) && (icount <= 0)) {
			/* that's enough */
			lexer_pushback (lf, tok);
			break;
		}
		lexer_freetoken (tok);
	}
	return 0;
}
/*}}}*/
/*{{{  static int guppy_skiptooutdent (lexfile_t *lf)*/
/*
 *	skips the lexer to the next logical outdent (doesn't consume).
 *	returns 0 on success (skipped ok), non-zero otherwise
 */
static int guppy_skiptooutdent (lexfile_t *lf)
{
	int icount = 0;
	token_t *tok;

	for (;;) {
		tok = lexer_nexttoken (lf);
		if (tok->type == END) {
			/* unexpected */
			lexer_pushback (lf, tok);
			return -1;
		}
		if (tok->type == INDENT) {
			icount++;
		} else if (tok->type == OUTDENT) {
			if (!icount) {
				/* stop here */
				lexer_pushback (lf, tok);
				return 0;
			}
			icount--;
		}
		lexer_freetoken (tok);
	}
	/* if we get here, ... */
	return 1;
}
/*}}}*/


/*{{{  static tnode_t *guppy_parser_parseproc (lexfile_t *lf)*/
/*
 *	parses a single process (in its entirety)
 */
static tnode_t *guppy_parser_parseproc (lexfile_t *lf)
{
	tnode_t *tree = NULL;
	token_t *tok = NULL;
	int emrk = parser_markerror (lf);
	int tnflags;

	tree = dfa_walk ("guppy:procstart", 0, lf);
	if (!tree) {
		/* failed to parse something */
		if (parser_checkerror (lf, emrk)) {
			guppy_skiptoeol (lf, 1);
		}
		return NULL;
	}

	tnflags = tnode_tnflagsof (tree);
	if (tnflags & TNF_LONGPROC) {
		/*{{{  long process (e.g. 'seq', 'par', 'alt', etc.)*/
		int ntflags = tnode_ntflagsof (tree);

		if (ntflags & NTF_INDENTED_PROC_LIST) {
			/*{{{  long process, parse list of indented processes into subnode 1*/
			tnode_t *body = guppy_indented_declorproc_list (lf);

			tnode_setnthsub (tree, 1, body);
			/*}}}*/
		} else if (ntflags & NTF_INDENTED_DGUARD_LIST) {
			/*{{{  long process, parse list of indented guards (maybe with leading declarations) into subnode 1*/
			tnode_t *body = guppy_indented_dguard_list (lf);

			tnode_setnthsub (tree, 1, body);
			/*}}}*/
		} else if (ntflags & NTF_INDENTED_EXPR_LIST) {
			/*{{{  long process, parse list of indented expressions and processes into subnode 1*/
			tnode_t *body = guppy_indented_exprproc_list (lf);

			tnode_setnthsub (tree, 1, body);
			/*}}}*/
		} else {
			tnode_warning (tree, "guppy_parser_parseproc(): unhandled LONGPROC [%s]", tree->tag->name);
		}
		/*}}}*/
	} else if (tnflags & TNF_LONGACTION) {
		/*{{{  long action (e.g. case-input)*/
		int ntflags = tnode_ntflagsof (tree);

		if (ntflags & NTF_INDENTED_TCASE_LIST) {
			/*{{{  parse list of indented type-cases and processes into subnode 1*/
			tnode_t *body = guppy_indented_tcase_list (lf);

			tnode_setnthsub (tree, 1, body);
			/*}}}*/
		} else {
			tnode_warning (tree, "guppy_parser_parseproc(): unhandled LONGACTION [%s]", tree->tag->name);
		}
		/*}}}*/
	}

	return tree;
}
/*}}}*/
/*{{{  static tnode_t *guppy_declorproc (lexfile_t *lf)*/
/*
 *	parses a single declaration or process
 */
static tnode_t *guppy_declorproc (lexfile_t *lf)
{
	tnode_t *tree = NULL;
	token_t *tok = NULL;

	if (compopts.verbose > 1) {
		nocc_message ("guppy_declorproc(): parsing declaration or process by test at %s:%d", lf->fnptr, lf->lineno);
	}

	/*{{{  skip newlines and comments*/
	tok = lexer_nexttoken (lf);
	while (tok && ((tok->type == NEWLINE) || (tok->type == COMMENT))) {
		lexer_freetoken (tok);
		tok = lexer_nexttoken (lf);
	}
	if (tok) {
		lexer_pushback (lf, tok);
		tok = NULL;
	}

	/*}}}*/

	/* test for a declaration first of all */
	tree = dfa_walk ("guppy:testfordecl", 0, lf);
	if (!tree) {
		parser_error (SLOCN (lf), "expected to find declaration or process, but didn\'t");
	} else if (tree->tag == testtruetag) {
		/* definitely a declaration */
		tnode_free (tree);
		tree = guppy_parser_parsedef (lf);
	} else if (tree->tag == testfalsetag) {
		/* definitely a process */
		tnode_free (tree);
		tree = guppy_parser_parseproc (lf);
	} else {
#if 0
fhandle_printf (FHAN_STDERR, "guppy_declorproc(): about to fail, but testtruetag == %p\n", testtruetag);
#endif
		nocc_serious ("guppy_declorproc(): guppy_testfordecl DFA returned:");
		tnode_dumptree (tree, 1, FHAN_STDERR);
		tnode_free (tree);
		tree = NULL;
	}

	if (compopts.verbose > 1) {
		nocc_message ("guppy_declorproc(): done parsing declaration or process, got (%s:%s)", tree ? tree->tag->name : "(nil)",
				tree ? tree->tag->ndef->name : "(nil)");
	}

	return tree;
}
/*}}}*/
/*{{{  static tnode_t *guppy_decllistandguard (lexfile_t *lf)*/
/*
 *	parses any number of declarations then a guard;  if declarations are present, builds an encapsulating DECLBLOCK.
 */
static tnode_t *guppy_decllistandguard (lexfile_t *lf)
{
	tnode_t *tree = NULL;
	token_t *tok = NULL;
	tnode_t *decls = NULL;
	tnode_t **gprocptr = NULL;

	if (compopts.verbose > 1) {
		nocc_message ("guppy_decllistandguard(): parsing declarations(s) and guard at %s:%d", lf->fnptr, lf->lineno);
	}

	for (;;) {
		/*{{{  skip newlines and comments*/
		tok = lexer_nexttoken (lf);
		while (tok && ((tok->type == NEWLINE) || (tok->type == COMMENT))) {
			lexer_freetoken (tok);
			tok = lexer_nexttoken (lf);
		}
		if (tok) {
			lexer_pushback (lf, tok);
			tok = NULL;
		}

		/*}}}*/

		tree = dfa_walk ("guppy:testfordecl", 0, lf);
		if (!tree) {
			parser_error (SLOCN (lf), "expected to find declaration and guard, but didn\'t");
		} else if (tree->tag == testtruetag) {
			/* definitely a declaration, deal with it */
			tnode_t *tmp;

			tnode_free (tree);
			tmp = guppy_parser_parsedef (lf);
			if (tmp) {
				if (!decls) {
					decls = parser_newlistnode (OrgOf (tmp));
				}
				parser_addtolist (decls, tmp);
			}
		} else if (tree->tag == testfalsetag) {
			/* not a declaration -- should be a guard */
			tnode_free (tree);
			tree = NULL;
			break;				/* for() */
		} else {
			nocc_serious ("guppy_decllistandguard(): guppy_testfordecl DFA returned:");
			tnode_dumptree (tree, 1, FHAN_STDERR);
			tnode_free (tree);
			return NULL;
		}
	}

	/* see if we have an expression to start with */
	tree = dfa_walk ("guppy:testforexpr", 0, lf);
	if (!tree) {
		parser_error (SLOCN (lf), "expected to find expression or not, but didn\'t");
	} else if (tree->tag == testtruetag) {
		/* starts with an expression, could be a pre-condition if followed by '&' */
		tnode_t *lexpr;

		tnode_free (tree);
		tree = NULL;

		lexpr = dfa_walk ("guppy:expr", 0, lf);
		if (!lexpr) {
			parser_error (SLOCN (lf), "thought it started with an expression, but it didn\'t");
			/* bail */
			return NULL;
		}

		tok = lexer_nexttoken (lf);
		if (!tok) {
			nocc_internal ("guppy_decllistandguard(): pop!");
			return NULL;
		}

		if (lexer_tokmatchlitstr (tok, "&")) {
			/* yes -- pre-conditioned guard */
			tree = dfa_walk ("guppy:guard", 0, lf);
			if (tree) {
				tnode_setnthsub (tree, 0, lexpr);
			}
			lexer_freetoken (tok);
		} else if (lexer_tokmatchlitstr (tok, "?")) {
			lexer_pushback (lf, tok);
			tree = dfa_walk ("guppy:restofinput", 0, lf);
			if (tree) {
				tnode_setnthsub (tree, 0, lexpr);
			}
		} else if (lexpr->tag == gup.tag_MARKEDIN) {
			/* the '?' got consumed in the expression */
#if 0
fhandle_printf (FHAN_STDERR, "guppy_decllistandguard(): looking at guard [MARKEDIN], next tok is '%s', lexpr is:\n", lexer_stokenstr (tok));
tnode_dumptree (lexpr, 1, FHAN_STDERR);
#endif
			lexer_pushback (lf, tok);
			tree = dfa_walk ("guppy:restofguard2", 0, lf);
			if (tree) {
				if (tree->tag == gup.tag_GUARD) {
					tnode_t *inode = tnode_nthsubof (tree, 1);

					tnode_setnthsub (inode, 0, tnode_nthsubof (lexpr, 0));
				} else {
					parser_error (SLOCN (lf), "expected guard, found [%s:%s]", tree->tag->ndef->name, tree->tag->name);
				}
			}
		} else {
			/* dunno.. */
			parser_error (SLOCN (lf), "got here but failed, token was '%s'", lexer_stokenstr (tok));
			return NULL;
		}
	} else if (tree->tag == testfalsetag) {
		/* not an expression, should be a simple guard */
		tnode_free (tree);

		tree = dfa_walk ("guppy:guard", 0, lf);
	} else {
		nocc_serious ("guppy_decllistandguard(): guppy_testforexpr DFA returned:");
		tnode_dumptree (tree, 1, FHAN_STDERR);
		tnode_free (tree);
		return NULL;
	}

	if (!tree) {
		parser_error (SLOCN (lf), "expected to find guard, but didn\'t");
	} else {
		tnode_t *gproc;

		/* parse guarded process */
		gproc = guppy_indented_declorproc_list (lf);
		tnode_setnthsub (tree, 2, gproc);

		if (decls) {
			tnode_t *tmp = tnode_create (gup.tag_DECLBLOCK, SLOCI, decls, tree);

			tree = tmp;
		}
	}

	if (compopts.verbose > 1) {
		nocc_message ("guppy_decllistandguard(): done parsing declaration or process, got (%s:%s)", tree ? tree->tag->name : "(nil)",
				tree ? tree->tag->ndef->name : "(nil)");
	}


	return tree;
}
/*}}}*/
/*{{{  static tnode_t *guppy_indented_declorproc_list (lexfile_t *lf)*/
/*
 *	parses a list of indented processes or definitions.
 */
static tnode_t *guppy_indented_declorproc_list (lexfile_t *lf)
{
	tnode_t *tree = NULL;
	token_t *tok;
	int zflag = 0;

	if (compopts.verbose > 1) {
		nocc_message ("guppy_indented_declorproc_list(): %s:%d: parsing indented declaration or process list", lf->fnptr, lf->lineno);
	}

	tree = parser_newlistnode (SLOCN (lf));

	tok = lexer_nexttoken (lf);
	/*{{{  skip newlines and comments*/
	for (; tok && ((tok->type == NEWLINE) || (tok->type == COMMENT)); tok = lexer_nexttoken (lf)) {
		lexer_freetoken (tok);
	}

	/*}}}*/
	/*{{{  expect indent*/
	if (tok->type != INDENT) {
		parser_error (SLOCN (lf), "expected indent, found:");
		lexer_dumptoken (FHAN_STDERR, tok);
		lexer_pushback (lf, tok);
		tnode_free (tree);
		return NULL;
	}

	/*}}}*/
	lexer_freetoken (tok);

	/* okay, parse declarations and processes */
	for (;;) {
		/*{{{  parse*/
		tnode_t *thisone;

		/* check token for end of indentation */
		tok = lexer_nexttoken (lf);
		/*{{{  skip newlines*/
		for (; tok && ((tok->type == NEWLINE) || (tok->type == COMMENT)); tok = lexer_nexttoken (lf)) {
			lexer_freetoken (tok);
		}

		/*}}}*/
		if (tok->type == OUTDENT) {
			/* got outdent, end-of-list! */
			lexer_freetoken (tok);
			break;		/* for() */
		} else {
			lexer_pushback (lf, tok);
		}

		thisone = guppy_declorproc (lf);

		if (thisone) {
			parser_addtolist (tree, thisone);
			zflag = 0;
		} else if (!zflag) {
			/* failed to parse -- actually, continue blindly.. */
			zflag = 1;
		} else {
			/* not getting anywhere, give up -- search for the next outdent */
			guppy_skiptooutdent (lf);
			tok = lexer_nexttoken (lf);			/* either outdent or end */
			break;
		}
		/*}}}*/
	}

	/* next token is optionally 'end' */
	tok = lexer_nexttoken (lf);
	if ((tok->type == KEYWORD) && lexer_tokmatchlitstr (tok, "end")) {
		lexer_freetoken (tok);

		/* gobble up comments */
		tok = lexer_nexttoken (lf);
		while (tok->type == COMMENT) {
			lexer_freetoken (tok);
			tok = lexer_nexttoken (lf);
		}
	}
	lexer_pushback (lf, tok);

	if (compopts.verbose > 1) {
		nocc_message ("guppy_indented_declorproc_list(): %s:%d: done parsing indented process list (tree at 0x%8.8x)",
				lf->fnptr, lf->lineno, (unsigned int)tree);
	}

#if 0
fprintf (stderr, "guppy_indented_declorproc_list(): returning:\n");
tnode_dumptree (tree, 1, stderr);
#endif
	return tree;
}
/*}}}*/
/*{{{  static tnode_t *guppy_indented_dguard_list (lexfile_t *lf)*/
/*
 *	parses a list of indented (optional declarations) and guards.
 */
static tnode_t *guppy_indented_dguard_list (lexfile_t *lf)
{
	tnode_t *tree = NULL;
	token_t *tok;
	int zflag = 0;

	if (compopts.verbose > 1) {
		nocc_message ("guppy_indented_dguard_list(): %s:%d: parsing indented guard list", lf->fnptr, lf->lineno);
	}

	tok = lexer_nexttoken (lf);
	/*{{{  skip newlines and comments*/
	for (; tok && ((tok->type == NEWLINE) || (tok->type == COMMENT)); tok = lexer_nexttoken (lf)) {
		lexer_freetoken (tok);
	}

	/*}}}*/
	/*{{{  expect indent*/
	if (tok->type != INDENT) {
		parser_error (SLOCN (lf), "expected indent, found:");
		lexer_dumptoken (FHAN_STDERR, tok);
		lexer_pushback (lf, tok);
		tnode_free (tree);
		return NULL;
	}

	/*}}}*/
	lexer_freetoken (tok);

	/* okay, parse declarations and guards */
	tree = parser_newlistnode (SLOCN (lf));
	for (;;) {
		tnode_t *thisone;

		/* check token for end of indentation */
		tok = lexer_nexttoken (lf);
		/*{{{  skip newlines*/
		for (; tok && ((tok->type == NEWLINE) || (tok->type == COMMENT)); tok = lexer_nexttoken (lf)) {
			lexer_freetoken (tok);
		}

		/*}}}*/
		if (tok->type == OUTDENT) {
			/* got outdent, end-of-list! */
			lexer_freetoken (tok);
			break;		/* for() */
		} else {
			lexer_pushback (lf, tok);
		}

		thisone = guppy_decllistandguard (lf);

		if (thisone) {
			parser_addtolist (tree, thisone);
			zflag = 0;
		} else if (!zflag) {
			/* failed to parse -- actually, continue blindly.. */
			zflag = 1;
		} else {
			/* not getting anywhere, give up -- search for the next outdent */
			guppy_skiptooutdent (lf);
			tok = lexer_nexttoken (lf);			/* either outdent or end */
			break;
		}
	}

	if (compopts.verbose > 1) {
		nocc_message ("guppy_indented_dguard_list(): %s:%d: done parsing indented guard list (tree at 0x%8.8x)",
				lf->fnptr, lf->lineno, (unsigned int)tree);
	}
	return tree;
}
/*}}}*/
/*{{{  static tnode_t *guppy_indented_name_list (lexfile_t *lf)*/
/*
 *	parses an indented list of names (and possible assignment-looking things)
 */
static tnode_t *guppy_indented_name_list (lexfile_t *lf)
{
	tnode_t *tree = NULL;
	token_t *tok;

	if (compopts.debugparser) {
		nocc_message ("guppy_indented_name_list(): %s:%d: parsing indented name list", lf->fnptr, lf->lineno);
	}

	tree = parser_newlistnode (SLOCN (lf));

	tok = lexer_nexttoken (lf);
	/*{{{  skip newlines and comments*/
	for (; tok && ((tok->type == NEWLINE) || (tok->type == COMMENT)); tok = lexer_nexttoken (lf)) {
		lexer_freetoken (tok);
	}

	/*}}}*/
	/*{{{  expect indent*/
	if (tok->type != INDENT) {
		parser_error (SLOCN (lf), "expected indent, found:");
		lexer_dumptoken (FHAN_STDERR, tok);
		lexer_pushback (lf, tok);
		tnode_free (tree);
		return NULL;
	}

	/*}}}*/
	lexer_freetoken (tok);

	/* okay, parse names (and possible assignments) */
	for (;;) {
		/*{{{  parse*/
		tnode_t *thisone;

		/* check token for end of indentation */
		tok = lexer_nexttoken (lf);
		/*{{{  skip newlines and comments*/
		for (; tok && ((tok->type == NEWLINE) || (tok->type == COMMENT)); tok = lexer_nexttoken (lf)) {
			lexer_freetoken (tok);
		}

		/*}}}*/
		if (tok->type == OUTDENT) {
			/* got outdent, end-of-list! */
			lexer_freetoken (tok);

			/* _might_ have 'end' as a keyword next */
			tok = lexer_nexttoken (lf);
			if (lexer_tokmatchlitstr (tok, "end")) {
				/* gobble */
				lexer_freetoken (tok);

				tok = lexer_nexttoken (lf);
				/*{{{  skip newlines and comments*/
				for (; tok && ((tok->type == NEWLINE) || (tok->type == COMMENT)); tok = lexer_nexttoken (lf)) {
					lexer_freetoken (tok);
				}

				/*}}}*/
			}
			/* push back what we just saw next regardless */
			lexer_pushback (lf, tok);

			break;		/* for() */
		} else {
			lexer_pushback (lf, tok);
		}

		thisone = dfa_walk ("guppy:nameandassign", 0, lf);

		if (thisone) {
			parser_addtolist (tree, thisone);
		} else {
			/* failed to parse */
			break;		/* for() */
		}
		/*}}}*/
	}

	return tree;
}
/*}}}*/
/*{{{  static tnode_t *guppy_indented_tcase_list (lexfile_t *lf)*/
/*
 *	parses an indented list of type-cases and indented processes
 */
static tnode_t *guppy_indented_tcase_list (lexfile_t *lf)
{
	tnode_t *tree = NULL;
	token_t *tok;

	if (compopts.debugparser) {
		nocc_message ("guppy_indented_tcase_list(): %s:%d: parsing indented tcase list", lf->fnptr, lf->lineno);
	}

	tree = parser_newlistnode (SLOCN (lf));

	tok = lexer_nexttoken (lf);
	/*{{{  skip newlines and comments*/
	for (; tok && ((tok->type == NEWLINE) || (tok->type == COMMENT)); tok = lexer_nexttoken (lf)) {
		lexer_freetoken (tok);
	}

	/*}}}*/
	/*{{{  expect indent*/
	if (tok->type != INDENT) {
		parser_error (SLOCN (lf), "expected indent, found:");
		lexer_dumptoken (FHAN_STDERR, tok);
		lexer_pushback (lf, tok);
		tnode_free (tree);
		return NULL;
	}

	/*}}}*/
	lexer_freetoken (tok);

	for (;;) {
		tnode_t *dblk, *thisone, *thisproc;
		srclocn_t *slocn = SLOCN (lf);

		/* check token for end of indentation */
		tok = lexer_nexttoken (lf);
		/*{{{  skip newlines and comments*/
		for (; tok && ((tok->type == NEWLINE) || (tok->type == COMMENT)); tok = lexer_nexttoken (lf)) {
			lexer_freetoken (tok);
		}

		/*}}}*/
		if (tok->type == OUTDENT) {
			/* got outdent, end-of-list! */
			lexer_freetoken (tok);
			break;		/* for() */
		} else {
			lexer_pushback (lf, tok);
		}

		/* first up, expect a single-line declaration */
		thisone = dfa_walk ("guppy:otherdecl", 0, lf);
		if (!thisone) {
			/* failed to parse, give up */
			break;		/* for() */
		}

		/* else, now expect indented decl-and-procs */
		thisproc = guppy_indented_declorproc_list (lf);
		if (!thisproc) {
			/* failed to parse, give up */
			break;		/* for() */
		}

		dblk = tnode_create (gup.tag_DECLBLOCK, slocn, thisone, thisproc);
		parser_addtolist (tree, dblk);
	}

	return tree;
}
/*}}}*/
/*{{{  static tnode_t *guppy_indented_exprproc_list (lexfile_t *lf)*/
/*
 *	parses an indented list of expressions (maybe including keyword "else") and indented processes.
 *	used for if/case
 */
static tnode_t *guppy_indented_exprproc_list (lexfile_t *lf)
{
	tnode_t *tree = NULL;
	token_t *tok;

	if (compopts.debugparser) {
		nocc_message ("guppy_indented_exprproc_list(): %s:%d: parsing indented expression list", lf->fnptr, lf->lineno);
	}

	tree = parser_newlistnode (SLOCN (lf));

	tok = lexer_nexttoken (lf);
	/*{{{  skip newlines and comments*/
	for (; tok && ((tok->type == NEWLINE) || (tok->type == COMMENT)); tok = lexer_nexttoken (lf)) {
		lexer_freetoken (tok);
	}

	/*}}}*/
	/*{{{  expect indent*/
	if (tok->type != INDENT) {
		parser_error (SLOCN (lf), "expected indent, found:");
		lexer_dumptoken (FHAN_STDERR, tok);
		lexer_pushback (lf, tok);
		tnode_free (tree);
		return NULL;
	}

	/*}}}*/
	lexer_freetoken (tok);

	for (;;) {
		tnode_t *thisone, *thisproc, *condnode;
		srclocn_t *slocn = SLOCN (lf);

		/* check token for end of indentation */
		tok = lexer_nexttoken (lf);
		/*{{{  skip newlines and comments*/
		for (; tok && ((tok->type == NEWLINE) || (tok->type == COMMENT)); tok = lexer_nexttoken (lf)) {
			lexer_freetoken (tok);
		}

		/*}}}*/
		if (tok->type == OUTDENT) {
			/* got outdent, end-of-list! */
			lexer_freetoken (tok);
			break;		/* for() */
		} else {
			lexer_pushback (lf, tok);
		}

		/* expect some sort of conditional expression */
		thisone = dfa_walk ("guppy:expr", 0, lf);
		if (!thisone) {
			/* failed to parse, give up */
			break;		/* for() */
		}

		/* else, now expect indented decl-and-procs */
		thisproc = guppy_indented_declorproc_list (lf);
		if (!thisproc) {
			/* failed to parse, give up */
			break;		/* for() */
		}

		condnode = tnode_create (gup.tag_COND, slocn, thisone, thisproc);
		parser_addtolist (tree, condnode);
	}

	return tree;
}
/*}}}*/
/*{{{  static tnode_t *guppy_parser_parsedef (lexfile_t *lf)*/
/*
 *	parses a single Guppy definition (e.g. process, type, ...)
 *	returns tree on success, NULL on failure
 */
static tnode_t *guppy_parser_parsedef (lexfile_t *lf)
{
	token_t *tok;
	tnode_t *tree = NULL;
	tnode_t **target = &tree;
	tnode_t *thisone;
	int tnflags, ntflags;
	int publictag = 0;

	if (compopts.debugparser) {
		nocc_message ("guppy_parser_parsedef(): starting parse for single definition..");
	}

	/* eat up comments and newlines */
	tok = lexer_nexttoken (lf);
	while ((tok->type == NEWLINE) || (tok->type == COMMENT)) {
		lexer_freetoken (tok);
		tok = lexer_nexttoken (lf);
	}
	if ((tok->type == END) || (tok->type == NOTOKEN)) {
		/* done */
		lexer_freetoken (tok);
		goto skipout;
	}

	if (lexer_tokmatch (gup.tok_PUBLIC, tok)) {
		lexer_freetoken (tok);
		publictag = 1;
	} else {
		lexer_pushback (lf, tok);
	}

	/* get the definition */
	tok = lexer_nexttoken (lf);
	if (lexer_tokmatch (gup.tok_ATSIGN, tok)) {
		lexer_pushback (lf, tok);

		/* try and deal with it as a pre-processor thing */
		*target = guppy_parse_preproc (lf);
		goto skipout;
	} else {
		lexer_pushback (lf, tok);
	}
#if 0
fprintf (stderr, "guppy_parser_parsedef(): about to walk for guppy:decl\n");
#endif
	thisone = dfa_walk ("guppy:decl", 0, lf);
#if 0
fprintf (stderr, "guppy_parser_parsedef(): done walking for guppy:decl, got 0x%8.8x, (%s,%s)\n", (unsigned int)thisone,
	thisone ? thisone->tag->name : "(nil)", thisone ? thisone->tag->ndef->name : "(nil)");
#endif
	if (!thisone) {
		*target = NULL;
		goto skipout;
	}
	*target = thisone;
	if (*target) {
		tnflags = tnode_tnflagsof (*target);
		ntflags = tnode_ntflagsof (*target);
	} else {
		tnflags = 0;
		ntflags = 0;
	}

	/* check ntflags for specific structures (e.g. intented process/decl list) */
	if (ntflags & NTF_INDENTED_PROC_LIST) {
		/* parse list of indented processes into subnode 2 */
		thisone = guppy_indented_declorproc_list (lf);
		tnode_setnthsub (*target, 2, thisone);
	} else if (ntflags & NTF_INDENTED_NAME_LIST) {
		/* parse list of indented names into subnode 1 */
		thisone = guppy_indented_name_list (lf);
		tnode_setnthsub (*target, 1, thisone);
	} else if (ntflags & NTF_INDENTED_TCASE_LIST) {
		/* parse list of indented type-cases and processes into subnode 1 */
		thisone = guppy_indented_tcase_list (lf);
		tnode_setnthsub (*target, 1, thisone);
	} else if (ntflags & NTF_INDENTED_DECL_LIST) {
		/* parse list of declarations into subnode 1 */
		thisone = guppy_indented_declorproc_list (lf);
		tnode_setnthsub (*target, 1, thisone);
	}

	if (publictag) {
		if (tree && (tree->tag == gup.tag_FCNDEF)) {
			library_markpublic (tree);
		}
	} else if (lf->toplevel && lf->sepcomp && tree && (tree->tag == gup.tag_FCNDEF)) {
		library_markpublic (tree);
	}

skipout:

	if (compopts.debugparser) {
		nocc_message ("guppy_parser_parsedef(): done parsing single definition (%p).", tree);
	}

	return tree;
}
/*}}}*/
/*{{{  static int guppy_parser_parsedeflist (lexfile_t *lf, tnode_t **target)*/
/*
 *	parses a list of definitions, all indented at the same level
 *	returns 0 on success, non-zero on failure
 */
static int guppy_parser_parsedeflist (lexfile_t *lf, tnode_t **target)
{
	if (compopts.debugparser) {
		nocc_message ("guppy_parser_parsedeflist(): starting parse of process list from [%s]", lf->fnptr);
	}

	for (;;) {
		tnode_t *thisone;
		int gotall = 0;
		int breakfor = 0;
		token_t *tok;

		tok = lexer_nexttoken (lf);
		while ((tok->type == NEWLINE) || (tok->type == COMMENT)) {
			lexer_freetoken (tok);
			tok = lexer_nexttoken (lf);
		}
		if ((tok->type == END) || (tok->type == NOTOKEN)) {
			/* done */
			lexer_freetoken (tok);
			break;				/* for() */
		}
		lexer_pushback (lf, tok);

		thisone = guppy_parser_parsedef (lf);
#if 0
fprintf (stderr, "guppy_parser_parsedeflist(): sausages");
#endif
		if (!thisone) {
			/* nothing left probably */
			break;				/* for() */
		}
		if (!*target) {
			/* make it a list */
			*target = parser_newlistnode (SLOCN (lf));
		} else if (!parser_islistnode (*target)) {
			nocc_internal ("guppy_parser_parsedeflist(): target is not a list! (%s,%s)", (*target)->tag->name, (*target)->tag->ndef->name);
			return -1;
		}
		parser_addtolist (*target, thisone);
	}
	return 0;
}
/*}}}*/
/*{{{  static tnode_t *guppy_parser_parse (lexfile_t *lf)*/
/*
 *	called to parse a file.
 *	returns a tree on success, NULL on failure
 */
static tnode_t *guppy_parser_parse (lexfile_t *lf)
{
	token_t *tok;
	tnode_t *tree = NULL;
	int i;

	if (compopts.verbose) {
		nocc_message ("guppy_parser_parse(): starting parse..");
	}

	i = guppy_parser_parsedeflist (lf, &tree);

	if (compopts.debugparser) {
		nocc_message ("leftover tokens:");
	}

	tok = lexer_nexttoken (lf);
	while (tok) {
		if (compopts.verbose > 1) {
			lexer_dumptoken (FHAN_STDERR, tok);
		}
		if ((tok->type == END) || (tok->type == NOTOKEN)) {
			lexer_freetoken (tok);
			break;			/* while() */
		}
		if ((tok->type != NEWLINE) && (tok->type != COMMENT)) {
			lf->errcount++;
		}

		lexer_freetoken (tok);
		tok = lexer_nexttoken (lf);
	}

	/* if building for separate compilation and top-level, drop in library node */
	if (lf->toplevel && lf->sepcomp && !lf->islibrary) {
		tnode_t *libnode = library_newlibnode (lf, NULL);		/* use default name */

		tnode_setnthsub (libnode, 0, tree);
		tree = libnode;
	}

	return tree;
}
/*}}}*/
/*{{{  static tnode_t *guppy_parser_descparse (lexfile_t *lf)*/
/*
 *	called to parse a descriptor line(s)
 *	for library (LIBDECL) things, returns as a list, for EXTDECL just returns that,
 *	returns NULL on failure
 */
static tnode_t *guppy_parser_descparse (lexfile_t *lf)
{
	token_t *tok;
	tnode_t *tl, *tree = NULL;

	if (compopts.verbose) {
		nocc_message ("guppy_parser_descparse(): parsing descriptor(s)...");
	}

	tree = dfa_walk ("guppy:descriptor", 0, lf);
#if 0
fhandle_printf (FHAN_STDERR, "guppy_parser_descparse(): got descriptor:\n");
tnode_dumptree (tree, 1, FHAN_STDERR);
#endif
	if (tree && (tree->tag == gup.tag_EXTDECL)) {
		/* just this one */
		return tree;
	}
	
	/* else turn into a list and try and parse anything else */
	tl = parser_newlistnode (SLOCI);
	parser_addtolist (tl, tree);

	tok = lexer_nexttoken (lf);
	while (tok && (tok->type != END)) {
		/* eat up any newlines */
		while (tok->type == NEWLINE) {
			lexer_freetoken (tok);
			tok = lexer_nexttoken (lf);
		}
		if (tok->type != END) {
			lexer_pushback (lf, tok);
			tree = dfa_walk ("guppy:descriptor", 0, lf);

			if (tree) {
				parser_addtolist (tl, tree);
			}
			tok = lexer_nexttoken (lf);
		}
	}

	return tl;
}
/*}}}*/


/*{{{  static int guppy_parser_prescope (tnode_t **tptr, prescope_t *ps)*/
/*
 *	called to pre-scope the parse tree (or a chunk of it)
 *	returns 0 on success, non-zero on failure
 */
static int guppy_parser_prescope (tnode_t **tptr, prescope_t *ps)
{
	/* run a quick pass over the tree to pull up any module definitions */
	int foundmodule = 0;

	tnode_modprewalktree (tptr, pullmodule_modprewalk, &foundmodule);

	if (!ps->hook) {
		guppy_prescope_t *gps = (guppy_prescope_t *)smalloc (sizeof (guppy_prescope_t));

		gps->last_type = NULL;
		gps->procdepth = 0;
		ps->hook = (void *)gps;
		tnode_modprewalktree (tptr, prescope_modprewalktree, (void *)ps);

		ps->hook = NULL;
		if (gps->last_type) {
			tnode_free (gps->last_type);
			gps->last_type = NULL;
		}
		sfree (gps);
	} else {
		tnode_modprewalktree (tptr, prescope_modprewalktree, (void *)ps);
	}
	return ps->err;
}
/*}}}*/
/*{{{  static int guppy_parser_scope (tnode_t **tptr, scope_t *ss)*/
/*
 *	called to scope declaractions in the parse tree
 *	returns 0 on success, non-zero on failure
 */
static int guppy_parser_scope (tnode_t **tptr, scope_t *ss)
{
	if (!ss->langpriv) {
		guppy_scope_t *gss = (guppy_scope_t *)smalloc (sizeof (guppy_scope_t));

		dynarray_init (gss->crosses);
		dynarray_init (gss->cross_lexlevels);
		gss->resolve_nametype_first = NULL;
		ss->langpriv = (void *)gss;

		if (compopts.tracescope) {
			nocc_message ("SCOPE: guppy_parser_scope: about to mod pre/post tree @%p type [%s:%s]",
					*tptr, (*tptr)->tag->ndef->name, (*tptr)->tag->name);
		}
		tnode_modprepostwalktree (tptr, scope_modprewalktree, scope_modpostwalktree, (void *)ss);

		dynarray_trash (gss->cross_lexlevels);
		dynarray_trash (gss->crosses);
		sfree (gss);
		ss->langpriv = NULL;
	} else {
		tnode_modprepostwalktree (tptr, scope_modprewalktree, scope_modpostwalktree, (void *)ss);
	}

	return ss->err;
}
/*}}}*/
/*{{{  static int guppy_parser_typecheck (tnode_t *tptr, typecheck_t *tc)*/
/*
 *	called to type-check a tree
 *	returns 0 on success, non-zero on failure
 */
static int guppy_parser_typecheck (tnode_t *tptr, typecheck_t *tc)
{
	if (!tc->hook) {
		guppy_typecheck_t *gtc = (guppy_typecheck_t *)smalloc (sizeof (guppy_typecheck_t));

		gtc->encfcn = NULL;
		tc->hook = (void *)gtc;

		tnode_prewalktree (tptr, typecheck_prewalktree, (void *)tc);

		sfree (gtc);
		tc->hook = NULL;
	} else {
		tnode_prewalktree (tptr, typecheck_prewalktree, (void *)tc);
	}
	return tc->err;
}
/*}}}*/
/*{{{  static int guppy_parser_typeresolve (tnode_t **tptr, typecheck_t *tc)*/
/*
 *	called to type-resolve a tree
 *	returns 0 on success, non-zero on failure
 */
static int guppy_parser_typeresolve (tnode_t **tptr, typecheck_t *tc)
{
	tnode_modprewalktree (tptr, typeresolve_modprewalktree, (void *)tc);
	return tc->err;
}
/*}}}*/
/*{{{  static int guppy_parser_fetrans (tnode_t **tptr, fetrans_t *fe)*/
/*
 *	does front-end transform for guppy.
 *	returns 0 on success, non-zero on failure
 */
static int guppy_parser_fetrans (tnode_t **tptr, fetrans_t *fe)
{
	int i, r = 0;
	guppy_fetrans_t *gfe;
	tnode_t **litems;

	/* might have some library stuff at the top */
	while (*tptr && ((*tptr)->tag->ndef->tn_flags & TNF_TRANSPARENT)) {
		tptr = tnode_nthsubaddr (*tptr, 0);
	}

	if (!parser_islistnode (*tptr)) {
		nocc_internal ("guppy_parser_fetrans(): expected list at top-level, got [%s:%s]", (*tptr)->tag->ndef->name, (*tptr)->tag->name);
		return -1;
	}

	gfe = (guppy_fetrans_t *)smalloc (sizeof (guppy_fetrans_t));
	gfe->preinslist = NULL;
	gfe->postinslist = NULL;
	fe->langpriv = (void *)gfe;

	for (i=0; i<parser_countlist (*tptr); i++) {
		tnode_t **iptr = parser_getfromlistptr (*tptr, i);
		int nr;

#if 0
fhandle_printf (FHAN_STDERR, "guppy_parser_fetrans(): about to do subtree %d/%d *iptr=0x%8.8x [%s]..\n", i, parser_countlist (*tptr),
		(unsigned int)(*iptr), (*iptr)->tag->name);
// tnode_dumptree (*iptr, 1, FHAN_STDERR);
#endif
		nr = fetrans_subtree (iptr, fe);
#if 0
fhandle_printf (FHAN_STDERR, "guppy_parser_fetrans(): done %d/%d ..  *iptr=0x%8.8x\n", i, parser_countlist (*tptr), (unsigned int)(*iptr));
#endif
		if (nr) {
			r++;
		}

		/* if there's anything to add, will be in gfe's lists */
		if (gfe->preinslist) {
			int j, nitems;
			tnode_t **items;

			items = parser_getlistitems (gfe->preinslist, &nitems);
			for (j=0; j<nitems; j++) {
				if (items[j]) {
					parser_insertinlist (*tptr, items[j], i);
					i++;
				}
			}
			parser_trashlist (gfe->preinslist);
			gfe->preinslist = NULL;
		}
		if (gfe->postinslist) {
			int j, nitems, jc;
			tnode_t **items;

			items = parser_getlistitems (gfe->postinslist, &nitems);
			for (j=0,jc=1; j<nitems; j++) {
				if (items[j]) {
					parser_insertinlist (*tptr, items[j], i+jc);
					jc++;
				}
			}
			parser_trashlist (gfe->postinslist);
			gfe->postinslist = NULL;
		}
	}

	fe->langpriv = NULL;
	sfree (gfe);

	return r;
}
/*}}}*/

