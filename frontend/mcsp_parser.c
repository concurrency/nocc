/*
 *	mcsp_parser.c -- MCSP parser for nocc
 *	Copyright (C) 2006-2007 Fred Barnes <frmb@kent.ac.uk>
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
#include <errno.h>

#include "nocc.h"
#include "support.h"
#include "version.h"
#include "symbols.h"
#include "keywords.h"
#include "lexer.h"
#include "lexpriv.h"
#include "tnode.h"
#include "parser.h"
#include "langdef.h"
#include "dfa.h"
#include "parsepriv.h"
#include "mcsp.h"
#include "library.h"
#include "feunit.h"
#include "names.h"
#include "scope.h"
#include "prescope.h"
#include "typecheck.h"
#include "fetrans.h"
#include "extn.h"
#include "mwsync.h"

/*}}}*/
/*{{{  forward decls*/
static int mcsp_parser_init (lexfile_t *lf);
static void mcsp_parser_shutdown (lexfile_t *lf);
static tnode_t *mcsp_parser_parse (lexfile_t *lf);
static tnode_t *mcsp_parser_descparse (lexfile_t *lf);
static int mcsp_parser_prescope (tnode_t **tptr, prescope_t *ps);
static int mcsp_parser_scope (tnode_t **tptr, scope_t *ss);
static int mcsp_parser_typecheck (tnode_t *tptr, typecheck_t *tc);
static int mcsp_parser_fetrans (tnode_t **tptr, fetrans_t *fe);


/*}}}*/
/*{{{  global vars*/

mcsp_pset_t mcsp;

langparser_t mcsp_parser = {
	langname:	"mcsp",
	init:		mcsp_parser_init,
	shutdown:	mcsp_parser_shutdown,
	parse:		mcsp_parser_parse,
	descparse:	mcsp_parser_descparse,
	prescope:	mcsp_parser_prescope,
	scope:		mcsp_parser_scope,
	typecheck:	mcsp_parser_typecheck,
	postcheck:	NULL,
	fetrans:	mcsp_parser_fetrans,
	getlangdef:	mcsp_getlangdef,
	maketemp:	NULL,
	makeseqassign:	NULL,
	tagstruct_hook:	(void *)&mcsp,
	lexer:		NULL
};


/*}}}*/
/*{{{  private types/vars*/
typedef struct {
	dfanode_t *inode;
	langdef_t *langdefs;
} mcsp_parse_t;

static mcsp_parse_t *mcsp_priv = NULL;

static feunit_t *feunit_set[] = {
	&mwsync_feunit,
	&mcsp_decl_feunit,
	&mcsp_process_feunit,
	&mcsp_oper_feunit,
	&mcsp_snode_feunit,
	&mcsp_instance_feunit,
	&mcsp_cnode_feunit,
	NULL
};

/*}}}*/


/*{{{  static mcsp_parse_t *mcsp_newmcspparse (void)*/
/*
 *	creates a new mcsp_parse_t structure
 */
static mcsp_parse_t *mcsp_newmcspparse (void)
{
	mcsp_parse_t *mpse = (mcsp_parse_t *)smalloc (sizeof (mcsp_parse_t));

	mpse->inode = NULL;
	mpse->langdefs = NULL;

	return mpse;
}
/*}}}*/
/*{{{  static void mcsp_freemcspparse (mcsp_parse_t *mpse)*/
/*
 *	frees an mcsp_parse_t structure
 */
static void mcsp_freemcspparse (mcsp_parse_t *mpse)
{
	if (!mpse) {
		nocc_warning ("mcsp_freemcspparse(): NULL pointer!");
		return;
	}
	if (mpse->langdefs) {
		langdef_freelangdef (mpse->langdefs);
		mpse->langdefs = NULL;
	}
	/* leave inode alone */
	mpse->inode = NULL;
	sfree (mpse);
	return;
}
/*}}}*/

/*{{{  void mcsp_isetindent (FILE *stream, int indent)*/
/*
 *	set-indent for debugging output
 */
void mcsp_isetindent (FILE *stream, int indent)
{
	int i;

	for (i=0; i<indent; i++) {
		fprintf (stream, "    ");
	}
	return;
}
/*}}}*/
/*{{{  langdef_t *mcsp_getlangdef (void)*/
/*
 *	returns the MCSP language definitions
 */
langdef_t *mcsp_getlangdef (void)
{
	if (!mcsp_priv) {
		return NULL;
	}
	return mcsp_priv->langdefs;
}
/*}}}*/


/*{{{  mcsp_alpha_t *mcsp_newalpha (void)*/
/*
 *	creates an empty mcsp_alpha_t structure
 */
mcsp_alpha_t *mcsp_newalpha (void)
{
	mcsp_alpha_t *alpha = (mcsp_alpha_t *)smalloc (sizeof (mcsp_alpha_t));

	alpha->elist = parser_newlistnode (NULL);
	return alpha;
}
/*}}}*/
/*{{{  void mcsp_addtoalpha (mcsp_alpha_t *alpha, tnode_t *event)*/
/*
 *	adds an event to an mcsp_alpha_t hook
 */
void mcsp_addtoalpha (mcsp_alpha_t *alpha, tnode_t *event)
{
	tnode_t **items;
	int nitems, i;

	if (!alpha) {
		nocc_internal ("mcsp_addtoalpha(): NULL alphabet!");
		return;
	}
	if (!event) {
		return;
	}

	items = parser_getlistitems (alpha->elist, &nitems);
	for (i=0; i<nitems; i++) {
		if (items[i] == event) {
			/* already here */
			return;
		}
	}
	parser_addtolist (alpha->elist, event);
	return;
}
/*}}}*/
/*{{{  void mcsp_mergealpha (mcsp_alpha_t *alpha, mcsp_alpha_t *others)*/
/*
 *	merges one alphabet into another
 */
void mcsp_mergealpha (mcsp_alpha_t *alpha, mcsp_alpha_t *others)
{
	tnode_t **items;
	int nitems, i;

	if (!alpha) {
		nocc_internal ("mcsp_mergealpha(): NULL alphabet!");
		return;
	}
	if (!others) {
		return;
	}

	items = parser_getlistitems (others->elist, &nitems);
	for (i=0; i<nitems; i++) {
		mcsp_addtoalpha (alpha, items[i]);
	}
	return;
}
/*}}}*/
/*{{{  void mcsp_mergeifalpha (mcsp_alpha_t *alpha, tnode_t *node)*/
/*
 *	merges alphabet from a node into another alphabet
 *	(checks to see that the given node has one first!)
 */
void mcsp_mergeifalpha (mcsp_alpha_t *alpha, tnode_t *node)
{
	if ((node->tag->ndef == mcsp.node_DOPNODE) || (node->tag->ndef == mcsp.node_SNODE) || (node->tag->ndef == mcsp.node_CNODE)) {
		mcsp_alpha_t *other = (mcsp_alpha_t *)tnode_nthhookof (node, 0);

		if (other) {
			mcsp_mergealpha (alpha, other);
		}
	}
	return;
}
/*}}}*/
/*{{{  void mcsp_freealpha (mcsp_alpha_t *alpha)*/
/*
 *	frees an mcsp_alpha_t
 */
void mcsp_freealpha (mcsp_alpha_t *alpha)
{
	if (alpha) {
		/* the list is of references */
		tnode_t **events;
		int nevents, i;

		events = parser_getlistitems (alpha->elist, &nevents);
		for (i=0; i<nevents; i++) {
			events[i] = NULL;
		}
		tnode_free (alpha->elist);
		alpha->elist = NULL;

		sfree (alpha);
	} else {
		nocc_warning ("mcsp_freealpha(): tried to free NULL!");
	}
	return;
}
/*}}}*/
/*{{{  int mcsp_cmpalphaentry (tnode_t *t1, tnode_t *t2)*/
/*
 *	compares two alphabet entries when sorting
 */
int mcsp_cmpalphaentry (tnode_t *t1, tnode_t *t2)
{
	if ((t1->tag != mcsp.tag_EVENT) || (t2->tag != mcsp.tag_EVENT)) {
		nocc_warning ("mcsp_cmpalphaentry(): non-event in alphabet");
	}

	if ((t1->tag != mcsp.tag_EVENT) && (t2->tag != mcsp.tag_EVENT)) {
		return 0;
	} else if (t1->tag != mcsp.tag_EVENT) {
		return -1;
	} else if (t2->tag != mcsp.tag_EVENT) {
		return 1;
	}
	if (t1 == t2) {
		return 0;
	}
	return strcmp (NameNameOf (tnode_nthnameof (t1, 0)), NameNameOf (tnode_nthnameof (t2, 0)));
}
/*}}}*/
/*{{{  void mcsp_sortandmergealpha (mcsp_alpha_t *a1, mcsp_alpha_t *a2, mcsp_alpha_t *isect, mcsp_alpha_t *diff)*/
/*
 *	sorts two alphabets and merges them.  "isect" gets ("a1" intersect "a2"),
 *	"diff" gets (("a1" union "a2") - ("a1" intersect "a2"))
 */
void mcsp_sortandmergealpha (mcsp_alpha_t *a1, mcsp_alpha_t *a2, mcsp_alpha_t *isect, mcsp_alpha_t *diff)
{
	tnode_t **a1items, **a2items;
	int a1n, a2n;
	int a1i, a2i;

#if 0
fprintf (stderr, "mcsp_sortandmergealpha():\na1 = \n");
mcsp.node_CNODE->hook_dumptree (NULL, (void *)a1, 1, stderr);
fprintf (stderr, "mcsp_sortandmergealpha():\na2 = \n");
mcsp.node_CNODE->hook_dumptree (NULL, (void *)a2, 1, stderr);
#endif
	parser_sortlist (a1->elist, mcsp_cmpalphaentry);
	parser_sortlist (a2->elist, mcsp_cmpalphaentry);
#if 0
fprintf (stderr, "mcsp_sortandmergealpha(): here!\n");
#endif

	a1items = parser_getlistitems (a1->elist, &a1n);
	a2items = parser_getlistitems (a2->elist, &a2n);

	for (a1i = a2i = 0; (a1i < a1n) && (a2i < a2n); ) {
		tnode_t *a1item = a1items[a1i];
		tnode_t *a2item = a2items[a2i];
		int r;

		r = mcsp_cmpalphaentry (a1item, a2item);
		if (r == 0) {
			/* same, add to intersections */
			if (isect) {
				mcsp_addtoalpha (isect, a1item);
			}
			a1i++;
			a2i++;
		} else if (r < 0) {
			/* in a1, not in a2, add to differences */
			if (diff) {
				mcsp_addtoalpha (diff, a1item);
			}
			a1i++;
		} else {
			/* in a2, not in a1, add to differences */
			if (diff) {
				mcsp_addtoalpha (diff, a2item);
			}
			a2i++;
		}
	}
	return;
}
/*}}}*/


/*{{{  void mcsp_alpha_hook_free (void *hook)*/
/*
 *	frees an alpha hook (mcsp_alpha_t)
 */
void mcsp_alpha_hook_free (void *hook)
{
	mcsp_alpha_t *alpha = (mcsp_alpha_t *)hook;

	if (alpha) {
		mcsp_freealpha (alpha);
	}
	return;
}
/*}}}*/
/*{{{  void *mcsp_alpha_hook_copy (void *hook)*/
/*
 *	copies an alpha hook (mcsp_alpha_t)
 */
void *mcsp_alpha_hook_copy (void *hook)
{
	mcsp_alpha_t *alpha = (mcsp_alpha_t *)hook;
	mcsp_alpha_t *nalpha;
	tnode_t **items;
	int nitems, i;

	if (!alpha) {
		return NULL;
	}
	nalpha = mcsp_newalpha ();

	items = parser_getlistitems (alpha->elist, &nitems);
	for (i=0; i<nitems; i++) {
		if (items[i]) {
			parser_addtolist (nalpha->elist, items[i]);
		}
	}
	
	return nalpha;
}
/*}}}*/
/*{{{  void mcsp_alpha_hook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	dumps an alpha hook (debugging)
 */
void mcsp_alpha_hook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	mcsp_alpha_t *alpha = (mcsp_alpha_t *)hook;
	
	if (alpha) {
		mcsp_isetindent (stream, indent);
		fprintf (stream, "<mcsp:alphahook addr=\"0x%8.8x\">\n", (unsigned int)alpha);
		tnode_dumptree (alpha->elist, indent + 1, stream);
		mcsp_isetindent (stream, indent);
		fprintf (stream, "</mcsp:alphahook>\n");
	}
	return;
}
/*}}}*/


/*{{{  static int mcsp_parser_init (lexfile_t *lf)*/
/*
 *	initialises the MCSP parser
 *	returns 0 on success, non-zero on failure
 */
static int mcsp_parser_init (lexfile_t *lf)
{
	if (compopts.verbose) {
		nocc_message ("initialising MCSP parser..");
	}
	if (!mcsp_priv) {
		mcsp_priv = mcsp_newmcspparse ();

		memset ((void *)&mcsp, 0, sizeof (mcsp));

		/* tell multiway syncs that synctrans should happen _after_ fetrans */
		mwsync_settransafterfetrans (1);

		mcsp_priv->langdefs = langdef_readdefs ("mcsp.ldef");
		if (!mcsp_priv->langdefs) {
			nocc_error ("mcsp_parser_init(): failed to load language definitions!");
			return 1;
		}

		/* initialise! */
		if (feunit_do_init_tokens (0, mcsp_priv->langdefs, (void *)&mcsp_parser)) {
			nocc_error ("mcsp_parser_init(): failed to initialise tokens");
			return 1;
		}
		if (feunit_do_init_nodes (feunit_set, 1, mcsp_priv->langdefs, (void *)&mcsp_parser)) {
			nocc_error ("mcsp_parser_init(): failed to initialise nodes");
			return 1;
		}
		if (feunit_do_reg_reducers (feunit_set, 0, mcsp_priv->langdefs)) {
			nocc_error ("mcsp_parser_init(): failed to register reducers");
			return 1;
		}
		if (feunit_do_init_dfatrans (feunit_set, 1, mcsp_priv->langdefs, &mcsp_parser, 1)) {
			nocc_error ("mcsp_parser_init(): failed to initialise DFAs");
			return 1;
		}
		if (feunit_do_post_setup (feunit_set, 1, mcsp_priv->langdefs)) {
			nocc_error ("mcsp_parser_init(): failed to post-setup");
			return 1;
		}
		if (langdef_treecheck_setup (mcsp_priv->langdefs)) {
			nocc_serious ("mcsp_parser_init(): failed to initialise tree-checking!");
		}

		mcsp_priv->inode = dfa_lookupbyname ("mcsp:procdecl");
		if (!mcsp_priv->inode) {
			nocc_error ("mcsp_parser_init(): could not find mcsp:procdecl");
			return 1;
		}
		if (compopts.dumpdfas) {
			dfa_dumpdfas (stderr);
		}
		if (compopts.dumpgrules) {
			parser_dumpgrules (stderr);
		}

		/* last, re-init multiway syncs with default end-of-par option */
		mwsync_setresignafterpar (1);
	}
	return 0;
}
/*}}}*/
/*{{{  static void mcsp_parser_shutdown (lexfile_t *lf)*/
/*
 *	shuts-down the MCSP parser
 */
static void mcsp_parser_shutdown (lexfile_t *lf)
{
	if (mcsp_priv) {
		mcsp_freemcspparse (mcsp_priv);
		mcsp_priv = NULL;
	}
	return;
}
/*}}}*/


/*{{{  static tnode_t *mcsp_parser_parse (lexfile_t *lf)*/
/*
 *	called to parse a file (or chunk of MCSP)
 *	returns a tree on success, NULL on failure
 */
static tnode_t *mcsp_parser_parse (lexfile_t *lf)
{
	token_t *tok;
	tnode_t *tree = NULL;
	tnode_t **target = &tree;

	if (compopts.verbose) {
		nocc_message ("mcsp_parser_parse(): starting parse..");
	}

	for (;;) {
		tnode_t *thisone;
		int tnflags;
		int breakfor = 0;

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
		lexer_pushback (lf, tok);

		thisone = dfa_walk ("mcsp:procdecl", lf);
		if (!thisone) {
			*target = NULL;
			break;		/* for() */
		}
		*target = thisone;
		while (*target) {
			/* sink through nodes */
			tnflags = tnode_tnflagsof (*target);
			if (tnflags & TNF_TRANSPARENT) {
				target = tnode_nthsubaddr (*target, 0);
			} else if (tnflags & TNF_SHORTDECL) {
				target = tnode_nthsubaddr (*target, 2);
			} else if (tnflags & TNF_LONGDECL) {
				target = tnode_nthsubaddr (*target, 3);
			} else {
				/* assume done */
				breakfor = 1;
				break;		/* while() */
			}
		}
		if (breakfor) {
			break;		/* for() */
		}
	}

	if (compopts.verbose) {
		nocc_message ("leftover tokens:");
	}

	tok = lexer_nexttoken (lf);
	while (tok) {
		if (compopts.verbose) {
			lexer_dumptoken (stderr, tok);
		}
		if ((tok->type == END) || (tok->type == NOTOKEN)) {
			lexer_freetoken (tok);
			break;
		}
		if ((tok->type != NEWLINE) && (tok->type != COMMENT)) {
			lf->errcount++;				/* errors.. */
		}

		lexer_freetoken (tok);
		tok = lexer_nexttoken (lf);
	}

	return tree;
}
/*}}}*/
/*{{{  static tnode_t *mcsp_parser_descparse (lexfile_t *lf)*/
/*
 *	parses an MCSP descriptor -- actually some specification/process
 *	returns tree on success, NULL on failure
 */
static tnode_t *mcsp_parser_descparse (lexfile_t *lf)
{
	token_t *tok;
	tnode_t *tree = NULL;
	tnode_t **target = &tree;

	if (compopts.verbose) {
		nocc_message ("mcsp_parser_descparse(): parsing descriptor (specification)..");
	}

	for (;;) {
		tnode_t *thisone;
		int breakfor = 0;
		int tnflags;

		tok = lexer_nexttoken (lf);
		while (tok->type == NEWLINE) {
			lexer_freetoken (tok);
			tok = lexer_nexttoken (lf);
		}
		if ((tok->type == END) || (tok->type == NOTOKEN)) {
			/* done */
			lexer_freetoken (tok);
			break;		/* for() */
		}
		lexer_pushback (lf, tok);

		/* walk as a descriptor-line */
		thisone = dfa_walk ("mcsp:process", lf);
		if (!thisone) {
			*target = NULL;
			break;		/* for() */
		}
#if 0
fprintf (stderr, "mcsp_parser_descparse(): thisone->tag->name = [%s], thisone->tag->ndef->name = [%s]\n", thisone->tag->name, thisone->tag->ndef->name);
#endif
		*target = thisone;
		while (*target) {
			/* sink through things */
			tnflags = tnode_tnflagsof (*target);
			if (tnflags & TNF_TRANSPARENT) {
				target = tnode_nthsubaddr (*target, 0);
			} else if (tnflags & TNF_SHORTDECL) {
				target = tnode_nthsubaddr (*target, 2);
			} else if (tnflags & TNF_LONGDECL) {
				target = tnode_nthsubaddr (*target, 3);
			} else {
				/* assume we're done! */
				breakfor = 1;
				break;		/* while() */
			}
		}
		if (breakfor) {
			break;		/* for() */
		}

		/* next token should be newline or end */
		tok = lexer_nexttoken (lf);
		if ((tok->type != NEWLINE) && (tok->type != END)) {
			parser_error (lf, "in descriptor, expected newline or end, found [%s]", lexer_stokenstr (tok));
			if (tree) {
				tnode_free (tree);
			}
			lexer_freetoken (tok);
			tree = NULL;
			break;		/* for() */
		}
		lexer_pushback (lf, tok);
		/* and go round */
	}

#if 0
fprintf (stderr, "mcsp_parser_descparse(): got tree:\n");
tnode_dumptree (tree, 1, stderr);
#endif

	return tree;
}
/*}}}*/
/*{{{  static int mcsp_parser_prescope (tnode_t **tptr, prescope_t *ps)*/
/*
 *	called to pre-scope the parse tree (whole MCSP only!)
 *	returns 0 on success, non-zero on failure
 */
static int mcsp_parser_prescope (tnode_t **tptr, prescope_t *ps)
{
	tnode_modprewalktree (tptr, prescope_modprewalktree, (void *)ps);

	return ps->err;
}
/*}}}*/
/*{{{  static int mcsp_parser_scope (tnode_t **tptr, scope_t *ss)*/
/*
 *	called to scope declarations in the parse tree
 *	returns 0 on success, non-zero on failure
 */
static int mcsp_parser_scope (tnode_t **tptr, scope_t *ss)
{
	mcsp_scope_t *mss = (mcsp_scope_t *)smalloc (sizeof (mcsp_scope_t));

	mss->uvinsertlist = NULL;
	mss->uvscopemark = NULL;
	mss->inamescope = 0;

	ss->langpriv = (void *)mss;

	tnode_modprepostwalktree (tptr, scope_modprewalktree, scope_modpostwalktree, (void *)ss);

	sfree (mss);
	return ss->err;
}
/*}}}*/
/*{{{  static int mcsp_parser_typecheck (tnode_t *tptr, typecheck_t *tc)*/
/*
 *	called to type-check a tree
 *	returns 0 on success, non-zero on failure
 */
static int mcsp_parser_typecheck (tnode_t *tptr, typecheck_t *tc)
{
	tnode_prewalktree (tptr, typecheck_prewalktree, (void *)tc);
	return tc->err;
}
/*}}}*/
/*{{{  static int mcsp_parser_fetrans (tnode_t **tptr, fetrans_t *fe)*/
/*
 *	called to do front-end transforms on a tree
 *	returns 0 on success, non-zero on failure
 */
static int mcsp_parser_fetrans (tnode_t **tptr, fetrans_t *fe)
{
	mcsp_fetrans_t *mfe = (mcsp_fetrans_t *)smalloc (sizeof (mcsp_fetrans_t));
	int err;

	fe->langpriv = (void *)mfe;
	mfe->errcount = 0;
	mfe->curalpha = NULL;
	mfe->uvinsertlist = NULL;

	for (mfe->parse=0; mfe->parse < 2; mfe->parse++) {
		fetrans_subtree (tptr, fe);
	}

	err = mfe->errcount;

	if (!err && !compopts.notmainmodule) {
		/*{{{  need to do a little work for interfacing with the world*/
		tnode_t **xptr, *lastlongdecl;

		for (xptr = tptr, lastlongdecl = NULL; *xptr;) {
			int tnflags = tnode_tnflagsof (*xptr);

			if (tnflags & TNF_TRANSPARENT) {
				xptr = tnode_nthsubaddr (*xptr, 0);
			} else if (tnflags & TNF_SHORTDECL) {
				xptr = tnode_nthsubaddr (*xptr, 2);
			} else if (tnflags & TNF_LONGDECL) {
				lastlongdecl = *xptr;
				xptr = tnode_nthsubaddr (*xptr, 3);
			} else {
				/* dunno! */
				nocc_warning ("mcsp_parser_fetrans(): stopping innermost walk at [%s]", (*xptr)->tag->name);
				break;
			}
		}
		
		if (!lastlongdecl) {
			nocc_warning ("mcsp_parser_fetrans(): did not find a long declaration..");
			err = 1;
		} else {
			/*{{{  do the hard work*/
			tnode_t *xdeclnode, *xparams, *xvardecls, *xiseq, *xiargs, *xscreenname;
			tnode_t *realparams = tnode_nthsubof (lastlongdecl, 1);
			tnode_t *realname = tnode_nthsubof (lastlongdecl, 0);
			tnode_t **rplist;
			int nrpitems, i;

			rplist = parser_getlistitems (realparams, &nrpitems);
#if 0
fprintf (stderr, "mcsp_parser_fetrans(): wanting to build environment, %d realparams =\n", nrpitems);
tnode_dumptree (realparams, 1, stderr);
#endif

			/*{{{  build VARDECLs for parameters*/
			xiseq = tnode_create (mcsp.tag_SEQCODE, NULL, NULL, parser_newlistnode (NULL), NULL);
			xvardecls = xiseq;
			xiargs = parser_newlistnode (NULL);
			for (i=0; i<nrpitems; i++) {
				tnode_t *namenode = tnode_nthsubof (rplist[i], 0);
				name_t *pname = tnode_nthnameof (namenode, 0);
				tnode_t *xnamenode;
				name_t *xname;

				xname = name_addname (NameNameOf (pname), NULL, NameTypeOf (pname), NULL);
				xnamenode = tnode_create (mcsp.tag_EVENT, NULL, xname);
				SetNameNode (xname, xnamenode);
				xvardecls = tnode_create (mcsp.tag_VARDECL, NULL, xnamenode, tnode_create (mcsp.tag_EVENTTYPE, NULL), NULL, xvardecls);
				SetNameType (xname, tnode_nthsubof (xvardecls, 1));
				SetNameDecl (xname, xvardecls);

				/* also add namenodes to a list we'll use for params later */
				parser_addtolist (xiargs, xnamenode);
			}

			/*}}}*/
#if 0
fprintf (stderr, "mcsp_parser_fetrans(): built xvardecls =\n");
tnode_dumptree (xvardecls, 1, stderr);
#endif
			/*{{{  build the top-level parameter list*/
			{
				tnode_t *xnamenode, *fparam;
				name_t *xname;

				xname = name_addname ("screenchannel", NULL, NULL, NULL);
				xnamenode = tnode_create (mcsp.tag_CHAN, NULL, xname);
				xscreenname = xnamenode;
				SetNameNode (xname, xnamenode);
				fparam = tnode_create (mcsp.tag_FPARAM, NULL, xnamenode, NULL);
				SetNameDecl (xname, fparam);

				xparams = parser_newlistnode (NULL);
				parser_addtolist (xparams, fparam);
			}
			/*}}}*/
			/*{{{  build the top-level XPROCDECL*/
			{
				name_t *pname = tnode_nthnameof (realname, 0);
				name_t *xname;
				tnode_t *xnamenode;
				char *nbuf;
				
				nbuf = (char *)smalloc (strlen (NameNameOf (pname)) + 8);
				sprintf (nbuf, "MCSP$%s", NameNameOf (pname));
				xname = name_addname (nbuf, NULL, xparams, NULL);
				sfree (nbuf);
				xnamenode = tnode_create (mcsp.tag_PROCDEF, NULL, xname);
				SetNameNode (xname, xnamenode);

				xdeclnode = tnode_create (mcsp.tag_XPROCDECL, NULL, xnamenode, xparams, xvardecls, NULL);
				SetNameDecl (xname, xdeclnode);
			}
			/*}}}*/
#if 0
fprintf (stderr, "mcsp_parser_fetrans(): built xdeclnode =\n");
tnode_dumptree (xdeclnode, 1, stderr);
#endif
			/*{{{  fudge in a descriptor line*/
			{
				name_t *pname = tnode_nthnameof (realname, 0);
				char *dbuf = (char *)smalloc (strlen (NameNameOf (pname)) + 128);
				chook_t *deschook = tnode_lookupchookbyname ("fetrans:descriptor");

				sprintf (dbuf, "PROC MCSP$%s (CHAN! BYTE screen)", NameNameOf (pname));
				tnode_setchook (xdeclnode, deschook, dbuf);
			}
			/*}}}*/
			/*{{{  park it..*/
			*xptr = xdeclnode;

			/*}}}*/
			/*{{{  create main process body and add*/
			{
				tnode_t *xinstance;		/* of "realname" */
				tnode_t *xalt;
				tnode_t *xparnode, *xparlist;

				xinstance = tnode_create (mcsp.tag_INSTANCE, NULL, realname, xiargs);
				/*{{{  build "xalt"*/
				{
					tnode_t *guardlist = parser_newlistnode (NULL);
					tnode_t **xitems;
					int nxitems, i;

					xalt = tnode_create (mcsp.tag_ALT, NULL, guardlist, NULL);
					xitems = parser_getlistitems (xiargs, &nxitems);

					for (i=0; i<nxitems; i++) {
						tnode_t *gproc = tnode_create (mcsp.tag_CHANWRITE, NULL, xscreenname, xitems[i], NULL);
						tnode_t *guard;

						guard = tnode_create (mcsp.tag_GUARD, NULL, xitems[i], gproc);

						parser_addtolist (guardlist, guard);
					}

					xalt = tnode_create (mcsp.tag_ILOOP, NULL, xalt, NULL);
				}
				/*}}}*/

				xparlist = parser_buildlistnode (NULL, xinstance, xalt, NULL);
				xparnode = tnode_create (mcsp.tag_PARCODE, NULL, NULL, xparlist, NULL);
				/* xparnode = tnode_create (mcsp.tag_PRIDROP, NULL, xparnode, NULL); */


				parser_addtolist (tnode_nthsubof (xiseq, 1), xparnode);
			}
			/*}}}*/
			/*}}}*/
		}
		/*}}}*/
	}

	/* do a 3rd pass to get PARCODE events */
	mfe->parse = 2;
	mfe->curalpha = NULL;
	fetrans_subtree (tptr, fe);

	sfree (mfe);

	return err;
}
/*}}}*/


