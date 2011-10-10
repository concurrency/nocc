/*
 *	eac_code.c -- EAC for NOCC
 *	Copyright (C) 2011 Fred Barnes <frmb@kent.ac.uk>
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

#include "nocc.h"
#include "support.h"
#include "version.h"
#include "fcnlib.h"
#include "symbols.h"
#include "keywords.h"
#include "lexer.h"
#include "lexpriv.h"
#include "tnode.h"
#include "parser.h"
#include "dfa.h"
#include "parsepriv.h"
#include "eac.h"
#include "feunit.h"
#include "names.h"
#include "scope.h"
#include "prescope.h"
#include "constprop.h"
#include "typecheck.h"
#include "usagecheck.h"
#include "postcheck.h"
#include "fetrans.h"
#include "mwsync.h"
#include "betrans.h"
#include "map.h"
#include "codegen.h"
#include "target.h"
#include "transputer.h"
#include "eacpriv.h"


/*}}}*/
/*{{{  private types*/

typedef struct TAG_eac_treesearch {
	tnode_t *find;				/* thing we're looking for */
	int isinput;				/* LHS of an input */
	int isoutput;				/* LHS of an output */
	int found;				/* non-zero if found anywhere */
} eac_treesearch_t;

/*}}}*/
/*{{{  private data*/

static int eac_ignore_unresolved = 0;
static int eac_interactive_mode = 0;		/* affects what the parser and other compiler passes might do */

static name_t *eac_nameinstancesintree_search = NULL;
static tnode_t *eac_tlfvpexpr = NULL;

/*}}}*/


/*{{{  static void eac_rawnamenode_hook_free (void *hook)*/
/*
 *	frees a rawnamenode hook (name-bytes)
 */
static void eac_rawnamenode_hook_free (void *hook)
{
	if (hook) {
		sfree (hook);
	}
	return;
}
/*}}}*/
/*{{{  static void *eac_rawnamenode_hook_copy (void *hook)*/
/*
 *	copies a rawnamenode hook (name-bytes)
 */
static void *eac_rawnamenode_hook_copy (void *hook)
{
	char *rawname = (char *)hook;

	if (rawname) {
		return string_dup (rawname);
	}
	return NULL;
}
/*}}}*/
/*{{{  static void eac_rawnamenode_hook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	dump-tree for rawnamenode hook (name-bytes)
 */
static void eac_rawnamenode_hook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	eac_isetindent (stream, indent);
	fprintf (stream, "<eacrawnamenode value=\"%s\" />\n", hook ? (char *)hook : "(null)");
	return;
}
/*}}}*/


/*{{{  static eac_treesearch_t *eac_newtreesearch (void)*/
/*
 *	creates a new eac_treesearch_t structure
 */
static eac_treesearch_t *eac_newtreesearch (void)
{
	eac_treesearch_t *ts = (eac_treesearch_t *)smalloc (sizeof (eac_treesearch_t));

	ts->find = NULL;
	ts->isinput = 0;
	ts->isoutput = 0;
	ts->found = 0;

	return ts;
}
/*}}}*/
/*{{{  static void eac_freetreesearch (eac_treesearch_t *ts)*/
/*
 *	frees an eac_treesearch_t structure
 */
static void eac_freetreesearch (eac_treesearch_t *ts)
{
	if (!ts) {
		nocc_internal ("eac_freetreesearch(): NULL pointer!");
		return;
	}
	sfree (ts);
	return;
}
/*}}}*/


/*{{{  int eac_isinteractive (void)*/
/*
 *	returns non-zero if parsing/etc. in the context of interactive mode
 */
int eac_isinteractive (void)
{
	return eac_interactive_mode;
}
/*}}}*/
/*{{{  static eac_subst_t *eac_newsubst (void)*/
/*
 *	creates a new eac_subst_t structure
 */
static eac_subst_t *eac_newsubst (void)
{
	eac_subst_t *subst = (eac_subst_t *)smalloc (sizeof (eac_subst_t));

	subst->count = 0;
	subst->newtree = NULL;
	subst->oldname = NULL;

	return subst;
}
/*}}}*/
/*{{{  static void eac_freesubst (eac_subst_t *subst)*/
/*
 *	frees an eac_subst_t structure
 */
static void eac_freesubst (eac_subst_t *subst)
{
	if (!subst) {
		nocc_internal ("eac_freesubst(): NULL pointer!");
		return;
	}

	subst->newtree = NULL;
	subst->oldname = NULL;
	sfree (subst);
	return;
}
/*}}}*/


/*{{{  static int eac_format_instr (char *str, int *sleft, const char *fmt, ...)*/
/*
 *	formats into a string.
 *	returns number of bytes written.
 */
static int eac_format_instr (char *str, int *sleft, const char *fmt, ...)
{
	int w;
	va_list ap;

	va_start (ap, fmt);
	w = vsnprintf (str, *sleft, fmt, ap);
	va_end (ap);

	if (w > 0) {
		*sleft -= w;
		return w;
	}
	return 0;
}
/*}}}*/
/*{{{  static int eac_format_inexpr (char *ptr, int sleft, tnode_t *expr)*/
/*
 *	formats an escape analysis expression into a string.
 *	returns number of bytes added.
 */
static int eac_format_inexpr (char *str, int *sleft, tnode_t *expr)
{
	int this = 0;
	int tleft = *sleft;

	if (!expr) {
		return 0;
	}

	if (parser_islistnode (expr)) {
		tnode_t **items;
		int nitems, i;

		items = parser_getlistitems (expr, &nitems);

		for (i=0; i<nitems; i++) {
			if (i) {
				this += eac_format_instr (str + this, sleft, ", ");
			}
			this += eac_format_inexpr (str + this, sleft, items[i]);
		}
	} else if (expr->tag == eac.tag_DECL) {
		this = eac_format_inexpr (str, sleft, tnode_nthsubof (expr, 0));
		this += eac_format_instr (str + this, sleft, " (");
		this += eac_format_inexpr (str + this, sleft, tnode_nthsubof (expr, 1));
		this += eac_format_instr (str + this, sleft, ") = \n\t");
		this += eac_format_inexpr (str + this, sleft, tnode_nthsubof (expr, 2));
	} else if (expr->tag->ndef == eac.node_NAMENODE) {
		this = eac_format_instr (str, sleft, "%s", NameNameOf (tnode_nthnameof (expr, 0))); 
	} else if (expr->tag == eac.tag_VARDECL) {
		/* free-var or parameter */
		this = eac_format_inexpr (str, sleft, tnode_nthsubof (expr, 0));
	} else if (expr->tag == eac.tag_ESET) {
		this = eac_format_instr (str, sleft, "{");
		this += eac_format_inexpr (str + this, sleft, tnode_nthsubof (expr, 0));
		this += eac_format_instr (str + this, sleft, "}");
	} else if (expr->tag == eac.tag_ESEQ) {
		this = eac_format_instr (str, sleft, "<");
		this += eac_format_inexpr (str + this, sleft, tnode_nthsubof (expr, 0));
		this += eac_format_instr (str + this, sleft, ">");
	} else if (expr->tag == eac.tag_INPUT) {
		this = eac_format_inexpr (str, sleft, tnode_nthsubof (expr, 0));
		this += eac_format_instr (str + this, sleft, "?");
		this += eac_format_inexpr (str + this, sleft, tnode_nthsubof (expr, 1));
	} else if (expr->tag == eac.tag_OUTPUT) {
		this = eac_format_inexpr (str, sleft, tnode_nthsubof (expr, 0));
		this += eac_format_instr (str + this, sleft, "!");
		this += eac_format_inexpr (str + this, sleft, tnode_nthsubof (expr, 1));
	} else if (expr->tag == eac.tag_VARCOMP) {
		this = eac_format_inexpr (str, sleft, tnode_nthsubof (expr, 0));
		this += eac_format_instr (str + this, sleft, "<-");
		if (parser_islistnode (tnode_nthsubof (expr, 1))) {
			this += eac_format_instr (str + this, sleft, "{");
			this += eac_format_inexpr (str + this, sleft, tnode_nthsubof (expr, 1));
			this += eac_format_instr (str + this, sleft, "}");
		} else {
			this += eac_format_inexpr (str + this, sleft, tnode_nthsubof (expr, 1));
		}
	} else if (expr->tag == eac.tag_SVREND) {
		this = eac_format_instr (str, sleft, "~");
		this += eac_format_inexpr (str + this, sleft, tnode_nthsubof (expr, 0));
	} else if (expr->tag == eac.tag_CLIEND) {
		this = eac_format_instr (str, sleft, "^");
		this += eac_format_inexpr (str + this, sleft, tnode_nthsubof (expr, 0));
	} else if (expr->tag == eac.tag_PAR) {
		this = eac_format_inexpr (str, sleft, tnode_nthsubof (expr, 0));
		this += eac_format_instr (str + this, sleft, " || ");
		this += eac_format_inexpr (str + this, sleft, tnode_nthsubof (expr, 1));
	} else if (expr->tag == eac.tag_HIDE) {
		this = eac_format_inexpr (str, sleft, tnode_nthsubof (expr, 0));
		this += eac_format_instr (str + this, sleft, " \\ {");
		this += eac_format_inexpr (str + this, sleft, tnode_nthsubof (expr, 1));
		this += eac_format_instr (str + this, sleft, "}");
	} else if (expr->tag == eac.tag_INSTANCE) {
		this = eac_format_inexpr (str, sleft, tnode_nthsubof (expr, 0));
		this += eac_format_instr (str + this, sleft, " (");
		this += eac_format_inexpr (str + this, sleft, tnode_nthsubof (expr, 1));
		this += eac_format_instr (str + this, sleft, ")");
	} else if (expr->tag == eac.tag_SUBST) {
		this = eac_format_inexpr (str, sleft, tnode_nthsubof (expr, 0));
		this += eac_format_instr (str + this, sleft, " [");
		this += eac_format_inexpr (str + this, sleft, tnode_nthsubof (expr, 1));
		this += eac_format_instr (str + this, sleft, " / ");
		this += eac_format_inexpr (str + this, sleft, tnode_nthsubof (expr, 2));
		this += eac_format_instr (str + this, sleft, "]");
	} else if (expr->tag == eac.tag_FVPEXPR) {
		this = eac_format_inexpr (str, sleft, tnode_nthsubof (expr, 0));
	}

	return this;
}
/*}}}*/
/*{{{  char *eac_format_expr (tnode_t *expr)*/
/*
 *	formats an escape analysis expression for human-readable display.
 */
char *eac_format_expr (tnode_t *expr)
{
	int slen = 1024;
	int sleft = slen - 1;
	char *str = (char *)smalloc (slen * sizeof (char));

	*str = '\0';
	eac_format_inexpr (str, &sleft, expr);

	return str;
}
/*}}}*/


/*{{{  static int eac_varmatch (tnode_t *var, tnode_t *findin)*/
/*
 *	determines if two var references are the same-name
 *	returns truth value
 */
static int eac_varmatch (tnode_t *var, tnode_t *findin)
{
	if (var->tag == eac.tag_SVREND) {
		var = tnode_nthsubof (var, 0);
	} else if (var->tag == eac.tag_CLIEND) {
		var = tnode_nthsubof (var, 0);
	}

	if (findin->tag == eac.tag_SVREND) {
		findin = tnode_nthsubof (findin, 0);
	} else if (findin->tag == eac.tag_CLIEND) {
		findin = tnode_nthsubof (findin, 0);
	}

	if (var->tag->ndef != eac.node_NAMENODE) {
		return 0;
	}
	if (findin->tag->ndef != eac.node_NAMENODE) {
		return 0;
	}

	if (tnode_nthnameof (var, 0) == tnode_nthnameof (findin, 0)) {
		return 1;
	}
	return 0;
}
/*}}}*/
/*{{{  static int eac_findintree (tnode_t *tree, void *arg)*/
/*
 *	called for each node looking for something
 *	returns 0 to stop walk, 1 to continue
 */
static int eac_findintree (tnode_t *tree, void *arg)
{
	eac_treesearch_t *ts = (eac_treesearch_t *)arg;

	if (tree->tag == eac.tag_INPUT) {
		tnode_t *lhs = tnode_nthsubof (tree, 0);

		if (eac_varmatch (ts->find, lhs)) {
			ts->isinput++;
			ts->found++;
		}
	} else if (tree->tag == eac.tag_OUTPUT) {
		tnode_t *lhs = tnode_nthsubof (tree, 0);

		if (eac_varmatch (ts->find, lhs)) {
			ts->isoutput++;
			ts->found++;
		}
	} else {
		if (eac_varmatch (ts->find, tree)) {
			ts->found++;
		}
	}
	return 1;
}
/*}}}*/


/*{{{  static char *eac_unusednamein (const char *basename, tnode_t *namelist)*/
/*
 *	attempts to find an unused name by adding primes to things
 *	returns new name string (allocated)
 */
static char *eac_unusednamein (const char *basename, tnode_t *namelist)
{
	int i, ncount;
	tnode_t **items;
	char *tmpnewname = (char *)smalloc (strlen (basename) + 128);
	int nnlen, maxlen;
	int first = 1;

	/* prime temporary new name */
	strcpy (tmpnewname, basename);
	nnlen = strlen (basename);
	maxlen = nnlen + 127;
	tmpnewname[nnlen] = '\0';

tryagain:
	if (!first) {
		/* add a prime.. */
		tmpnewname[nnlen] = '\'';
		nnlen++;
		tmpnewname[nnlen] = '\0';
		if (nnlen == maxlen) {
			nocc_internal ("eac_unusednamein(): added 128 or so primes, but no fresh name found -- giving up!");
			sfree (tmpnewname);
			return string_dup ("gopher$$$");
		}
	} else {
		first = 0;
	}

	items = parser_getlistitems (namelist, &ncount);
	for (i=0; i<ncount; i++) {
		tnode_t *thisone = items[i];

		if (thisone->tag == eac.tag_VARDECL) {
			thisone = tnode_nthsubof (thisone, 0);
		}
		if (thisone->tag->ndef == eac.node_NAMENODE) {
			name_t *name = tnode_nthnameof (thisone, 0);

			if (!strcmp (NameNameOf (name), tmpnewname)) {
				/* already got one, try again */
				goto tryagain;
			}
		}
	}

	/* not used! */
	return tmpnewname;
}
/*}}}*/
/*{{{  static int eac_substituteintree_walk (tnode_t **tptr, void *arg)*/
/*
 *	substitutes within a tree
 *	returns 0 to stop walk, 1 to continue
 */
static int eac_substituteintree_walk (tnode_t **tptr, void *arg)
{
	eac_subst_t *subst = (eac_subst_t *)arg;
	tnode_t *node = *tptr;

	if (node->tag->ndef == eac.node_NAMENODE) {
		name_t *name = tnode_nthnameof (node, 0);

		if (name == subst->oldname) {
			/* this one */
			subst->count++;
			*tptr = tnode_copytree (subst->newtree);

			tnode_free (node);
		}
	}
	return 1;
}
/*}}}*/
/*{{{  int eac_substituteintree (tnode_t **tptr, eac_subst_t *subst)*/
/*
 *	substitutes some tree for a name in a given parse-tree.  Makes copies
 *	of the source tree to be used (usually just a name reference).
 *
 *	return 0 on success, non-zero on error.
 */
int eac_substituteintree (tnode_t **tptr, eac_subst_t *subst)
{
	tnode_modprewalktree (tptr, eac_substituteintree_walk, (void *)subst);
	return 0;
}
/*}}}*/
/*{{{  static int eac_nameinstancesintree_walk (tnode_t *tree, void *arg)*/
/*
 *	does a tree-walk looking for a particular name; uses 'eac_nameinstancesintree_search'
 *	for the name to find.
 *	returns 0 to stop walk, 1 to continue.
 */
static int eac_nameinstancesintree_walk (tnode_t *tree, void *arg)
{
	int *countp = (int *)arg;

	if (tree->tag->ndef == eac.node_NAMENODE) {
		/* NOTE: this uses actual name, not lexical, comparison */
		if (tnode_nthnameof (tree, 0) == eac_nameinstancesintree_search) {
			*countp = *countp + 1;
		}
	}
	return 1;
}
/*}}}*/
/*{{{  static int eac_nameinstancesintree (tnode_t *tree, name_t *name)*/
/*
 *	counts the number of instances of a particular name in a tree
 */
static int eac_nameinstancesintree (tnode_t *tree, name_t *name)
{
	int count = 0;
	name_t *oldi = eac_nameinstancesintree_search;

	if (!tree) {
		return 0;
	}
	eac_nameinstancesintree_search = name;
	tnode_prewalktree (tree, eac_nameinstancesintree_walk, (void *)&count);
	eac_nameinstancesintree_search = oldi;

	return count;
}
/*}}}*/
/*{{{  static int eac_hideinset (tnode_t **esetp, tnode_t *varref)*/
/*
 *	hides a name in a set of sequences, modifying as needed
 *	returns 0 on success, non-zero on failure
 */
static int eac_hideinset (tnode_t **esetp, tnode_t *varref)
{
	/* FIXME: incomplete! */
#if 1
fprintf (stderr, "want to hide:\n");
tnode_dumptree (varref, 1, stderr);
fprintf (stderr, "in event-set:\n");
tnode_dumptree (*esetp, 1, stderr);
#endif
	if ((*esetp)->tag == eac.tag_ESET) {
		tnode_t **seqs;
		int nseqs, i;

		seqs = parser_getlistitems (tnode_nthsubof (*esetp, 0), &nseqs);

		for (i=0; i<nseqs; i++) {
			eac_treesearch_t *ts = eac_newtreesearch ();

			ts->find = varref;
			tnode_prewalktree (seqs[i], eac_findintree, (void *)ts);
#if 1
fprintf (stderr, "searched in sequence (isinput=%d, isoutput=%d, found=%d):\n", ts->isinput, ts->isoutput, ts->found);
tnode_dumptree (seqs[i], 1, stderr);
#endif
			eac_freetreesearch (ts);
		}
	}
	return 0;
}
/*}}}*/
/*{{{  static int eac_simplifytree_walk (tnode_t **tptr, void *arg)*/
/*
 *	does simplifications on a parse-tree (walk)
 *	returns 0 to stop walk, 1 to continue
 */
static int eac_simplifytree_walk (tnode_t **tptr, void *arg)
{
	tnode_t *node = *tptr;
	int *errp = (int *)arg;

	if (!node) {
		return 0;
	}

	if (node->tag == eac.tag_SUBST) {
		/*{{{  do substitution of some tree for a name*/
		int i, ncount, ecount;
		tnode_t **newlist, **extlist;
		int scount = 0;

		if (!parser_islistnode (tnode_nthsubof (node, 1)) || !parser_islistnode (tnode_nthsubof (node, 2))) {
			tnode_error (node, "substitution items not a list!");
			*errp = *errp + 1;
			return 0;
		}

		newlist = parser_getlistitems (tnode_nthsubof (node, 1), &ncount);
		extlist = parser_getlistitems (tnode_nthsubof (node, 2), &ecount);
		if (ncount != ecount) {
			tnode_error (node, "unbalanced substitution! [%d/%d]", ncount, ecount);
			*errp = *errp + 1;
			return 0;
		}

		for (i=0; i<ncount; i++) {
			eac_subst_t *ss;

			if (extlist[i]->tag->ndef != eac.node_NAMENODE) {
				tnode_error (node, "item %d in substitution list is not a name", i);
				*errp = *errp + 1;
				return 0;
			}

			ss = eac_newsubst ();
			ss->oldname = tnode_nthnameof (extlist[i], 0);
			ss->newtree = newlist[i];

			eac_substituteintree (tnode_nthsubaddr (node, 0), ss);

			scount += ss->count;
			eac_freesubst (ss);
		}

		/* okay, lose the substitution and replace with its modified contents */
		*tptr = tnode_nthsubof (node, 0);
		tnode_setnthsub (node, 0, NULL);		/* just left with subst lists */

		tnode_free (node);

		/* walk the new node, then we're done */
		tnode_modprewalktree (tptr, eac_simplifytree_walk, (void *)errp);

		return 0;
		/*}}}*/
	} else if (node->tag == eac.tag_INSTANCE) {
		/*{{{  substitute contents of instance for its placement*/
		/* FIXME: assume type-checking for parameters has been done! */
		tnode_t *inst = tnode_nthsubof (node, 0);
		name_t *pname;
		tnode_t *pdecl, *pcopy;
		tnode_t **pexplist, **iexplist;
		int npitems, niitems, i;

		if (inst->tag != eac.tag_NPROCDEF) {
			tnode_error (node, "instance not of a procedure!");
			*errp = *errp + 1;
			return 0;
		}

		pname = tnode_nthnameof (inst, 0);
		pdecl = NameDeclOf (pname);

		/*{{{  sanity checks*/
		if (!pdecl) {
			tnode_error (node, "no such procedure");
			*errp = *errp + 1;
			return 0;
		}

		if (pdecl->tag != eac.tag_DECL) {
			tnode_error (node, "declaration not of expected DECL type (%s,%s)",
					pdecl->tag->name, pdecl->tag->ndef->name);
			*errp = *errp + 1;
			return 0;
		}
		/*}}}*/

		pcopy = tnode_copytree (tnode_nthsubof (pdecl, 2));

		/*{{{  this *should* be a FVPEXPR node, original name references preserved */

		if (pcopy->tag != eac.tag_FVPEXPR) {
			tnode_error (node, "copied procedure body is not a free-var expression (%s,%s)",
					pcopy->tag->name, pcopy->tag->ndef->name);
			*errp = *errp + 1;
			tnode_free (pcopy);
			return 0;
		}
		/*}}}*/
		/*{{{  sanity check*/
		if (!parser_islistnode (tnode_nthsubof (pdecl, 1))) {
			tnode_error (node, "procedure parameter list not list");
			*errp = *errp + 1;
			tnode_free (pcopy);
			return 0;
		}
		if (!parser_islistnode (tnode_nthsubof (node, 1))) {
			tnode_error (node, "instance parameter list not list");
			*errp = *errp + 1;
			tnode_free (pcopy);
			return 0;
		}
		/*}}}*/
		/*{{{  first, replace parameters in expression copy*/
		pexplist = parser_getlistitems (tnode_nthsubof (pdecl, 1), &npitems);
		iexplist = parser_getlistitems (tnode_nthsubof (node, 1), &niitems);

		if (npitems != niitems) {
			tnode_error (node, "parameter count mismatch! (actual %d, formal %d)", niitems, npitems);
			*errp = *errp + 1;
			tnode_free (pcopy);
			return 0;
		}

		for (i=0; i<npitems; i++) {
			eac_subst_t *ss = eac_newsubst ();
			tnode_t *pfname = NULL;

			if (pexplist[i]->tag != eac.tag_VARDECL) {
				tnode_error (pexplist[i], "formal parameter %d is not a variable declaration!", i);
				*errp = *errp + 1;
				eac_freesubst (ss);
				tnode_free (pcopy);
				return 0;
			}
			pfname = tnode_nthsubof (pexplist[i], 0);

			if (pfname->tag->ndef != eac.node_NAMENODE) {
				tnode_error (pexplist[i], "formal parameter %d is not a name!", i);
				*errp = *errp + 1;
				eac_freesubst (ss);
				tnode_free (pcopy);
				return 0;
			}
			ss->oldname = tnode_nthnameof (pfname, 0);
			ss->newtree = iexplist[i];

			eac_substituteintree (&pcopy, ss);
			eac_freesubst (ss);
		}

		/*}}}*/
		/*{{{  pull up and rename (where necessary) bound free-vars in copied instance 'pcopy'*/
		if (!eac_tlfvpexpr) {
			tnode_error (node, "replacing instance, but no top-level free-var list set!");
			*errp = *errp + 1;
			tnode_free (pcopy);
			return 0;
		} else {
			tnode_t **lbitems;
			int lbcount;
			tnode_t *tmpnode;

			lbitems = parser_getlistitems (tnode_nthsubof (pcopy, 1), &lbcount);
			for (i=0; i<lbcount; i++) {
				name_t *lbname;
				tnode_t *tlvlist = tnode_nthsubof (eac_tlfvpexpr, 1);
				tnode_t *newnamenode, *newdecl;
				name_t *newname;
				char *newsname;
				eac_subst_t *ss;

				if (lbitems[i]->tag != eac.tag_VARDECL) {
					tnode_error (node, "local free-var item not var-decl! (%s,%s)",
							lbitems[i]->tag->name, lbitems[i]->tag->ndef->name);
					*errp = *errp + 1;
					tnode_free (pcopy);
					return 0;
				}
				lbname = tnode_nthnameof (tnode_nthsubof (lbitems[i], 0), 0);

				/* find an unused name (maybe the same!) */
				newsname = eac_unusednamein (NameNameOf (lbname), tlvlist);
				newnamenode = tnode_createfrom (tnode_nthsubof (lbitems[i], 0)->tag, node, NULL);
				newdecl = tnode_createfrom (lbitems[i]->tag, node, newnamenode);
				newname = name_addname (newsname, newdecl, NULL, newnamenode);
				sfree (newsname);

				ss = eac_newsubst ();
				tnode_setnthname (newnamenode, 0, newname);

				/* add declaration to top-level list */
				parser_addtolist (tlvlist, newdecl);

				/* substitute inside copied body */
				ss->oldname = lbname;
				ss->newtree = newnamenode;

				eac_substituteintree (tnode_nthsubaddr (pcopy, 0), ss);
				eac_freesubst (ss);

			}

			/* free-var list in pcopy now redundant */
			tmpnode = tnode_nthsubof (pcopy, 0);
			tnode_setnthsub (pcopy, 0, NULL);
			tnode_free (pcopy);
			pcopy = tmpnode;
		}
		/*}}}*/

		/* replace instance node with adjusted procedure copy and free */
		*tptr = pcopy;
		tnode_free (node);

		/*}}}*/
	} else if (node->tag == eac.tag_FVPEXPR) {
		/*{{{  free-var list, if top-level, make a note and descend*/
		if (!eac_tlfvpexpr) {
			eac_tlfvpexpr = node;
			tnode_modprewalktree (tnode_nthsubaddr (node, 0), eac_simplifytree_walk, arg);
			eac_tlfvpexpr = NULL;
		}
		/*}}}*/
	} else if (node->tag == eac.tag_PAR) {
		/*{{{  parallel composition -- simple, do subnodes then coalesce escape sets*/
		tnode_t **lhsp = tnode_nthsubaddr (node, 0);
		tnode_t **rhsp = tnode_nthsubaddr (node, 1);

		tnode_modprewalktree (lhsp, eac_simplifytree_walk, arg);
		tnode_modprewalktree (rhsp, eac_simplifytree_walk, arg);

		if (((*lhsp)->tag == eac.tag_ESET) && ((*rhsp)->tag == eac.tag_ESET)) {
			/* join together */
			tnode_t *lhsseq = tnode_nthsubof (*lhsp, 0);
			tnode_t *rhsseq = tnode_nthsubof (*rhsp, 0);

			while (parser_countlist (rhsseq) > 0) {
				tnode_t *item = parser_delfromlist (rhsseq, 0);

				parser_addtolist (lhsseq, item);
			}

			*tptr = *lhsp;
			tnode_setnthsub (node, 0, NULL);
			tnode_free (node);
		}

		/*}}}*/
	} else if (node->tag == eac.tag_HIDE) {
		/*{{{  hiding operator -- does something moderately complex*/
		tnode_t **lhsp = tnode_nthsubaddr (node, 0);

		tnode_modprewalktree (lhsp, eac_simplifytree_walk, arg);

		if ((*lhsp)->tag == eac.tag_ESET) {
			/* can do hiding (hopefully!) */
			tnode_t **hitems;
			int i, hcount;

			hitems = parser_getlistitems (tnode_nthsubof (node, 1), &hcount);
			for (i=0; i<hcount; i++) {
				/* hide this one.. */
				eac_hideinset (lhsp, hitems[i]);
#if 0
fprintf (stderr, "eac_simplifytree_walk(): want to hide:\n");
tnode_dumptree (hitems[i], 4, stderr);
#endif
			}

			/* unhook hiding node and replace with modified LHS */
			*tptr = *lhsp;
			tnode_setnthsub (node, 0, NULL);
			tnode_free (node);
		}

		/*}}}*/
	}

	return 1;
}
/*}}}*/
/*{{{  static int eac_simplifytree (tnode_t **tptr)*/
/*
 *	attempts to do simplifications on a parse-tree:
 *	 - perform substitutions
 *	 - expand named instances
 *	returns 0 on success, non-zero on failure
 */
static int eac_simplifytree (tnode_t **tptr)
{
	int err = 0;

	if (!*tptr) {
		return 0;
	}
	tnode_modprewalktree (tptr, eac_simplifytree_walk, (void *)&err);

	return err;
}
/*}}}*/
/*{{{  static int eac_cleanuptree_walk (tnode_t **tptr, void *arg)*/
/*
 *	does tree clean-up, pre-walk
 *	returns 0 to stop walk, 1 to continue
 */
static int eac_cleanuptree_walk (tnode_t **tptr, void *arg)
{
	tnode_t *node = *tptr;
	int *errp = (int *)arg;

	if (!node) {
		return 0;
	}

	if (node->tag == eac.tag_FVPEXPR) {
		tnode_t *fvplist = tnode_nthsubof (node, 1);
		tnode_t **items;
		int nitems, i;

		if (!parser_islistnode (fvplist)) {
			tnode_error (node, "free-vars not a list!");
			*errp = *errp + 1;
			return 0;
		}

		items = parser_getlistitems (fvplist, &nitems);
		for (i=0; i<nitems; i++) {
			name_t *name;
			tnode_t *ndecl;

			if (items[i]->tag != eac.tag_VARDECL) {
				tnode_error (node, "item %d in freevars list is not a variable declaration (%s,%s)",
						i, items[i]->tag->name, items[i]->tag->ndef->name);
				*errp = *errp + 1;
				return 0;
			}

			ndecl = tnode_nthsubof (items[i], 0);

			if (ndecl->tag->ndef != eac.node_NAMENODE) {
				tnode_error (node, "declared item %d in freevars list is not a name (%s,%s)",
						i, ndecl->tag->name, ndecl->tag->ndef->name);
				*errp = *errp + 1;
				return 0;
			}

			name = tnode_nthnameof (ndecl, 0);

			/* see if it occurs in the tree at all */
			if (!eac_nameinstancesintree (tnode_nthsubof (node, 0), name)) {
				/* nope, it doesn't, remove it from the list */
				tnode_t *item = parser_delfromlist (fvplist, i);

				i--, nitems--;
				tnode_free (item);			/* and free */
			}
		}

		/* leave empty free-var lists just incase we need to fill them up again later on! */
	}

	return 1;
}
/*}}}*/
/*{{{  static int eac_cleanuptree (tnode_t **tptr)*/
/*
 *	attempts to clean-up a tree after simplifications and things,
 *	removes bound variable names that have no occurences
 *	returns 0 on success, non-zero on failure
 */
static int eac_cleanuptree (tnode_t **tptr)
{
	int err = 0;

	if (!*tptr) {
		return 0;
	}
	tnode_modprewalktree (tptr, eac_cleanuptree_walk, (void *)&err);

	return err;
}
/*}}}*/


/*{{{  int eac_evaluate (const char *str)*/
/*
 *	evaluates a string (probably EAC expression or process)
 *	returns 0 on success, non-zero on failure
 */
int eac_evaluate (const char *str, const int interactive_mode)
{
	char *lstr = string_dup (str);
	lexfile_t *lf = lexer_openbuf ("interactive", "eac", lstr);
	int rcde = 0;
	tnode_t *tree = NULL;
	int oldintr = eac_interactive_mode;
	char *resstr = NULL;

	if (!lf) {
		printf ("failed to open lexer for expression\n");
		rcde = 1;
		goto out_cleanup;
	}

	eac_interactive_mode = interactive_mode;
	tree = parser_parse (lf);
	lexer_close (lf);

	if (!tree) {
		printf ("failed to parse expression\n");
		rcde = 2;
		goto out_cleanup;
	}

	/* okay, got a parse tree! */
	rcde = nocc_runfepasses (&lf, &tree, 1, NULL);
	if (rcde) {
		printf ("failed to run front-end on expression\n");
		rcde = 3;
		goto out_cleanup;
	}

	/* got something that went through the front-end okay -- do simplifications & reductions */
	rcde = eac_simplifytree (&tree);
	if (rcde) {
		printf ("failed to simplify expression\n");
		rcde = 4;
		goto out_cleanup;
	}

	rcde = eac_cleanuptree (&tree);
	if (rcde) {
		printf ("failed to clean-up after expression simplifications\n");
		rcde = 5;
		goto out_cleanup;
	}

	/* okay, pretty-print */
	resstr = eac_format_expr (tree);
	printf ("%s\n", resstr);

	if (compopts.verbose) {
		tnode_dumptree (tree, 1, stderr);
	}
	if (interactive_mode == EAC_DEF) {
		/* tree added to names in parser */
	} else {
		tnode_free (tree);
	}

out_cleanup:
	if (resstr) {
		sfree (resstr);
		resstr = NULL;
	}
	sfree (lstr);
	lstr = NULL;

	eac_interactive_mode = oldintr;
	return rcde;
}
/*}}}*/
/*{{{  int eac_parseprintexp (const char *str)*/
/*
 *	parses and prints an expression (doesn't evaluate)
 *	returns 0 on success, non-zero on failure
 */
int eac_parseprintexp (const char *str)
{
	char *lstr = string_dup (str);
	lexfile_t *lf = lexer_openbuf ("interactive", "eac", lstr);
	int rcde = 0;
	tnode_t *tree = NULL;
	int oldintr = eac_interactive_mode;
	char *resstr = NULL;

	if (!lf) {
		printf ("failed to open lexer for expression\n");
		rcde = 1;
		goto out_cleanup;
	}

	eac_interactive_mode = 1;
	tree = parser_parse (lf);
	lexer_close (lf);

	if (!tree) {
		printf ("failed to parse expression\n");
		rcde = 2;
		goto out_cleanup;
	}

	/* okay, got a parse tree! */
	rcde = nocc_runfepasses (&lf, &tree, 1, NULL);
	if (rcde) {
		printf ("failed to run front-end on expression\n");
		rcde = 3;
		goto out_cleanup;
	}

	/* okay, pretty-print */
	resstr = eac_format_expr (tree);
	printf ("%s\n", resstr);

	if (compopts.verbose) {
		tnode_dumptree (tree, 1, stdout);
	}
	tnode_free (tree);

out_cleanup:
	if (resstr) {
		sfree (resstr);
		resstr = NULL;
	}
	sfree (lstr);
	lstr = NULL;

	eac_interactive_mode = oldintr;
	return rcde;
}
/*}}}*/


/*{{{  static int eac_scopein_paramlist (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	scopes in parameters for a procedure definition;
 *	called directly as no specific fparam type exists.
 *	returns 0 on success, non-zero on failure.
 */
static int eac_scopein_paramlist (compops_t *cops, tnode_t **node, scope_t *ss)
{
	tnode_t **items;
	int nitems, i;

	if (!*node) {
		return 0;
	}
	if (!parser_islistnode (*node)) {
		/* not a list, probably singleton */
		*node = parser_buildlistnode (OrgFileOf (*node), *node, NULL);
	}

	items = parser_getlistitems (*node, &nitems);
	for (i=0; i<nitems; i++) {
		char *rawname;
		name_t *varname;
		tnode_t *namenode, *olditem = items[i];
		ntdef_t *typetag = NULL;
		tnode_t *realname = NULL;

		if (items[i]->tag == eac.tag_SVREND) {
			realname = tnode_nthsubof (items[i], 0);
			typetag = eac.tag_NSVRCHANVAR;
		} else if (items[i]->tag == eac.tag_CLIEND) {
			realname = tnode_nthsubof (items[i], 0);
			typetag = eac.tag_NCLICHANVAR;
		} else {
			realname = items[i];
			typetag = eac.tag_NCHANVAR;
		}

		if (realname->tag != eac.tag_NAME) {
			scope_error (items[i], ss, "parameter not a name!");
			return 1;
		}

		rawname = tnode_nthhookof (realname, 0);
		varname = name_addscopenamess (rawname, realname, NULL, NULL, ss);
		namenode = tnode_createfrom (typetag, realname, varname);
		SetNameNode (varname, namenode);
		items[i] = tnode_createfrom (eac.tag_VARDECL, olditem, namenode);

		/* free old param */
		tnode_free (olditem);
		ss->scoped++;
	}

	return 0;
}
/*}}}*/
/*{{{  static int eac_scopein_freevars (compops_t *cops, tnode_t *fvlist, scope_t *ss)*/
/*
 *	scopes in free-variables for a procedure definition -- these are free in escape sets or process compositions.
 *	returns 0 on success, non-zero on failure.
 */
static int eac_scopein_freevars (compops_t *cops, tnode_t *fvlist, scope_t *ss)
{
	tnode_t **items;
	int nitems, i;

	if (!fvlist) {
		return 0;
	}
	if (!parser_islistnode (fvlist)) {
		nocc_internal ("eac_scopein_freevars(): fvlist not list! (%s,%s)", fvlist->tag->name, fvlist->tag->ndef->name);
		return -1;
	}

	items = parser_getlistitems (fvlist, &nitems);
	for (i=0; i<nitems; i++) {
		char *rawname;
		name_t *varname;
		tnode_t *namenode, *olditem = items[i];

		if (items[i]->tag != eac.tag_NAME) {
			scope_error (items[i], ss, "parameter not a name!");
			return 1;
		}

		rawname = tnode_nthhookof (items[i], 0);
		varname = name_addscopenamess (rawname, items[i], NULL, NULL, ss);
		namenode = tnode_createfrom (eac.tag_NVAR, items[i], varname);
		SetNameNode (varname, namenode);
		items[i] = tnode_createfrom (eac.tag_VARDECL, olditem, namenode);

		/* free old name -- copy anyway */
		tnode_free (olditem);
		ss->scoped++;
	}

	return 0;
}
/*}}}*/
/*{{{  static int eac_scope_scanfreevars (tnode_t *node, void *arg)*/
/*
 *	looks for free name references inside processes, adds them to a list.
 *	returns 0 to stop walk, 1 to continue
 */
static int eac_scope_scanfreevars (tnode_t *node, void *arg)
{
	tnode_t *fvlist = (tnode_t *)arg;

#if 0
nocc_message ("eac_scope_scanfreevars(): looking at (%s)", node->tag->name);
#endif
	if (node->tag == eac.tag_NAME) {
		char *rawname = tnode_nthhookof (node, 0);
		tnode_t **xitems;
		int nxitems, i;

		xitems = parser_getlistitems (fvlist, &nxitems);
		for (i=0; i<nxitems; i++) {
			/* assert: fvlist is a list of eac_NAMEs */
			char *thisname = tnode_nthhookof (xitems[i], 0);

			if (!strcmp (thisname, rawname)) {
				/* got this name already */
				break;		/* for() */
			}
		}
		if (i == nxitems) {
			parser_addtolist (fvlist, tnode_copytree (node));
		}
	}
	return 1;
}
/*}}}*/


/*{{{  static int eac_prescope_declnode (compops_t *cops, tnode_t **tptr, prescope_t *ps)*/
/*
 *	pre-scopes a process definition -- makes sure parameter list is a list
 *	returns 0 to stop walk, 1 to continue
 */
static int eac_prescope_declnode (compops_t *cops, tnode_t **tptr, prescope_t *ps)
{
	tnode_t *node = *tptr;

	if (node->tag == eac.tag_DECL) {
		tnode_t **paramsptr = tnode_nthsubaddr (node, 1);

		if (!*paramsptr) {
			/* no parameters, leave empty list */
			*paramsptr = parser_newlistnode (OrgFileOf (node));
		} else if (!parser_islistnode (*paramsptr)) {
			/* singleton */
			*paramsptr = parser_makelistnode (*paramsptr);
		}
	}

	return 1;
}
/*}}}*/
/*{{{  static int eac_scopein_declnode (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	scopes in a process definition
 *	returns 0 to stop walk, 1 to continue
 */
static int eac_scopein_declnode (compops_t *cops, tnode_t **node, scope_t *ss)
{
	tnode_t *name = tnode_nthsubof (*node, 0);
	void *nsmark;
	char *rawname;
	name_t *procname;
	tnode_t *newname;

	if (name->tag != eac.tag_NAME) {
		scope_error (name, ss, "eac_scopein_declnode(): declaration name not name! (%s,%s)", name->tag->name, name->tag->ndef->name);
		return 0;
	}
	rawname = tnode_nthhookof (name, 0);
	procname = name_addscopenamess (rawname, *node, NULL, NULL, ss);
	newname = tnode_createfrom (eac.tag_NPROCDEF, name, procname);
	SetNameNode (procname, newname);
	tnode_setnthsub (*node, 0, newname);

	/* free old name */
	tnode_free (name);
	ss->scoped++;

	nsmark = name_markscope ();
	/* scope parameters */
	eac_scopein_paramlist (cops, tnode_nthsubaddr (*node, 1), ss);

	/* scope body, primarily to pick out parameters */
	scope_subtree (tnode_nthsubaddr (*node, 2), ss);

	/* remove params from visible scope */
	name_markdescope (nsmark);

	return 0;
}
/*}}}*/


/*{{{  static int eac_scope_fixchanvars (tnode_t *node, void *arg)*/
/*
 *	looks for free name references inside processes, adds them to a list.
 *	returns 0 to stop walk, 1 to continue
 */
static int eac_scope_fixchanvars (tnode_t *node, void *arg)
{
	tnode_t		 *lhs, *rhs;
	name_t		 *namenode;
	tnode_t		 *fvlist = (tnode_t *)arg;
	tnode_t		**xitems;
	int		  nxitems, found = 0, i;
	char		 *name, *rhsname;


#if 0
nocc_message ("eac_scope_fixchanvars(): looking at (%s)", node->tag->name);
#endif
	if (node->tag == eac.tag_INPUT || node->tag == eac.tag_OUTPUT) {
		lhs = tnode_nthsubof(node, 0);
		if (lhs->tag == eac.tag_NVAR) {
			lhs->tag = eac.tag_NCHANVAR;
			parser_addtolist (fvlist, tnode_copytree (lhs));

		}

	} else if (node->tag == eac.tag_INSTANCE) {
		rhs = tnode_nthsubof(node, 1);
		xitems = parser_getlistitems(rhs, &nxitems);
		for (i=0; i<nxitems; i++) {
			if (xitems[i]->tag == eac.tag_NVAR) {
				xitems[i]->tag = eac.tag_NCHANVAR;
				parser_addtolist(fvlist, tnode_copytree(xitems[i]));
			}
		}
	}

	else if (node->tag == eac.tag_NVAR) {
		rhsname = NameNameOf(tnode_nthnameof(node, 0));
		xitems = parser_getlistitems (fvlist, &nxitems);
		for (i=0; i<nxitems; i++) {
			name = NameNameOf(tnode_nthnameof(xitems[i], 0));
			if (strcmp(rhsname, name) == 0) {
				node->tag = eac.tag_NCHANVAR;
			}
		}
	}
	return 1;
}
/*}}}*/
/*{{{  static int eac_scopein_fvpenode (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	scopes in a process expression (associates free-vars with processes)
 *	returns 0 to stop walk, 1 to continue
 */
static int eac_scopein_fvpenode (compops_t *cops, tnode_t **node, scope_t *ss)
{
	tnode_t *fvlist, *tfvlist;
	void *nsmark;
	int eac_lastunresolved = eac_ignore_unresolved;

	/* scope expression, ignoring free-vars */
	eac_ignore_unresolved = 1;
	scope_subtree (tnode_nthsubaddr (*node, 0), ss);
	eac_ignore_unresolved = eac_lastunresolved;

	/* scan body looking for leftover free variables */
	fvlist = parser_newlistnode (OrgFileOf (*node));
	tnode_prewalktree (tnode_nthsubof (*node, 0), eac_scope_scanfreevars, fvlist);

	nsmark = name_markscope ();

	/* scope in free variables and attach to tree */
	eac_scopein_freevars (cops, fvlist, ss);
	tnode_setnthsub (*node, 1, fvlist);

	/* scope body again */
	scope_subtree (tnode_nthsubaddr (*node, 0), ss);

	tfvlist =  parser_newlistnode (OrgFileOf (*node));
	tnode_prewalktree (tnode_nthsubof (*node, 0), eac_scope_fixchanvars, tfvlist);
	tnode_prewalktree (tnode_nthsubof (*node, 0), eac_scope_fixchanvars, tfvlist);
	name_markdescope (nsmark);

	return 0;
}
/*}}}*/


/*{{{  static int eac_scopein_rawname (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	scopes in a free-floating name
 */
static int eac_scopein_rawname (compops_t *cops, tnode_t **node, scope_t *ss)
{
	tnode_t *name = *node;
	char *rawname;
	name_t *sname = NULL;

	if (name->tag != eac.tag_NAME) {
		scope_error (name, ss, "name not raw-name!");
		return 0;
	}
	rawname = tnode_nthhookof (name, 0);

#if 0
fprintf (stderr, "eac_scopein_rawname: here! rawname = \"%s\"\n", rawname);
#endif
	sname = name_lookupss (rawname, ss);
	if (sname) {
		/* resolved */
		*node = tnode_copytree (NameNodeOf (sname));

		tnode_free (name);
	} else {
		if (!eac_ignore_unresolved) {
			scope_error (name, ss, "unresolved name \"%s\"", rawname);
		}
		/* else we ignore this fact */
	}

	return 1;
}
/*}}}*/


/*{{{  static int eac_fetrans_declnode (compops_t *cops, tnode_t **tptr, fetrans_t *fe)*/
/*
 *	front-end transformations for procedure declarations
 *	returns 0 to stop walk, non-zero to continue
 */
static int eac_fetrans_declnode (compops_t *cops, tnode_t **tptr, fetrans_t *fe)
{
	tnode_t *name = tnode_nthsubof (*tptr, 0);
	name_t *nname = tnode_nthnameof (name, 0);
	eac_istate_t *istate = eac_getistate ();
	int i;

	for (i=0; i<DA_CUR (istate->procs); i++) {
		if (DA_NTHITEM (istate->procs, i) == nname) {
			/* already got this one */
			return 1;
		}
	}
	dynarray_add (istate->procs, nname);

	return 1;
}
/*}}}*/


/*{{{  static int eac_prescope_psubstnode (compops_t *cops, tnode_t **tptr, prescope_t *ps)*/
/*
 *	pre-scope for a substitution node -- make sure substitution items are lists
 *	returns 0 to stop walk, 1 to continue
 */
static int eac_prescope_psubstnode (compops_t *cops, tnode_t **tptr, prescope_t *ps)
{
	tnode_t *node = *tptr;

	if (node->tag == eac.tag_SUBST) {
		tnode_t **newlp = tnode_nthsubaddr (node, 1);
		tnode_t **extlp = tnode_nthsubaddr (node, 2);

		if (!parser_islistnode (*newlp)) {
			*newlp = parser_makelistnode (*newlp);
		}
		if (!parser_islistnode (*extlp)) {
			*extlp = parser_makelistnode (*extlp);
		}
	}

	return 1;
}
/*}}}*/


/*{{{  static int eac_prescope_instancenode (compops_t *cops, tnode_t **tptr, prescope_t *ps)*/
/*
 *	pre-scope for instance node -- names sure parameter list is a list
 *	return 0 to stop walk, 1 to continue
 */
static int eac_prescope_instancenode (compops_t *cops, tnode_t **tptr, prescope_t *ps)
{
	tnode_t *node = *tptr;

	if (node->tag == eac.tag_INSTANCE) {
		tnode_t **paramsptr = tnode_nthsubaddr (node, 1);

		if (!*paramsptr) {
			/* make empty list */
			*paramsptr = parser_newlistnode (OrgFileOf (node));
		} else if (!parser_islistnode (*paramsptr)) {
			*paramsptr = parser_makelistnode (*paramsptr);
		}
	}

	return 1;
}
/*}}}*/
/*{{{  static int eac_typecheck_instancenode (compops_t *cops, tnode_t *node, typecheck_t *tc)*/
/*
 *	type-checking for instance node
 *	returns 0 to stop walk, 1 to continue
 */
static int eac_typecheck_instancenode (compops_t *cops, tnode_t *node, typecheck_t *tc)
{
	tnode_t *inst = tnode_nthsubof (node, 0);
	name_t *pname, *apname;
	tnode_t *aparams = tnode_nthsubof (node, 1);
	tnode_t *fparams;
	tnode_t **aplist, **fplist;
	int naparams, nfparams, i;

#if 0
fprintf (stderr, "eac_typecheck_instancenode(): instance of:\n");
tnode_dumptree (inst, 1, stderr);
#endif
	/* FIXME: check 'inst' is a process definition, and that parameter counts match */
	/* also check that parameters are variables, not process names */

	if (inst->tag != eac.tag_NPROCDEF) {
		typecheck_error (node, tc, "named instance is not a process definition");
		return 1;
	}

	pname = tnode_nthnameof (inst, 0);
	fparams = tnode_nthsubof (NameDeclOf (pname), 1);

	aplist = parser_getlistitems (aparams, &naparams);
	fplist = parser_getlistitems (fparams, &nfparams);

	if (naparams != nfparams) {
		typecheck_error (node, tc, "number of actual parameters (%d) "
		    "does not match formal (%d) in call of \"%s\"",
		    naparams, nfparams, NameNameOf (pname));
		return 1;
	}

	for (i=0; i<naparams; i++) {
		/* Check actual parameter aplist[i] is sensible,and perhaps,
		 * matches formal. */
#if 0
		fprintf(stderr, "aparam [%d of %d] :", (i+1), naparams);
		tnode_dumptree(aplist[i],1,stderr);
		fprintf(stderr, "fparam [%d of %d] :", (i+1), naparams);
		tnode_dumptree(fplist[i], 1,stderr);
#endif
		/* check AP is somthing we are expecting */
		if (!(aplist[i]->tag == eac.tag_NVAR || aplist[i]->tag == eac.tag_NCHANVAR ||
				aplist[i]->tag == eac.tag_SVREND || aplist[i]->tag == eac.tag_CLIEND)) {
			char *ap_name = (aplist[i]->tag->ndef == eac.node_NAMENODE) ? NameNameOf(tnode_nthnameof(aplist[i], 0)) : "unknown";
			typecheck_error(node, tc, "1. actual param does not match formal params...\n"
					"\tparameter '%s' is not a variable in call of \"%s\"",
					ap_name, NameNameOf(pname));
			return 1;
		}

		if (fplist[i]->tag == eac.tag_VARDECL) {
			tnode_t *inner = tnode_nthsubof(fplist[i], 0);
			if ( (inner->tag == eac.tag_NVAR) && (aplist[i]->tag != eac.tag_NVAR) ) {
				typecheck_error(aplist[i], tc,
						"2. actual param[%d] does not match formal params in call of %s."
						" Found %s expected %s)",
						(i+1), NameNameOf(pname), aplist[i]->tag->name, inner->tag->name);
				return 1;
			} else if (inner->tag == eac.tag_NCHANVAR) {
				if ( !(aplist[i]->tag == eac.tag_NCHANVAR ||
						aplist[i]->tag == eac.tag_CLIEND ||
						aplist[i]->tag == eac.tag_SVREND)) {
					typecheck_error(aplist[i], tc,
							"3. actual param[%d] does not match formal params in call of %s."
							" Found %s expected %s)",
							(i+1), NameNameOf(pname), aplist[i]->tag->name, inner->tag->name);
				return 1;

				}
			}
		} else {
			typecheck_error(fplist[i], tc, "5. formal param[%d] expecting VARDECL found %s",
					(i+1), fplist[i]->tag->name);
			return 1;
		}

	}

	return 1;
}
/*}}}*/


/*{{{  static int eac_prescope_varcompnode (compops_t *cops, tnode_t **tptr, prescope_t *ps)*/
/*
 *	pre-scope for instance node -- names sure parameter list is a list
 *	return 0 to stop walk, 1 to continue
 */
static int eac_prescope_varcompnode (compops_t *cops, tnode_t **tptr, prescope_t *ps)
{
	tnode_t *node = *tptr;

	if (node->tag == eac.tag_VARCOMP) {
		tnode_t **rhs = tnode_nthsubaddr (node, 1);

		if (!*rhs) {
			/* make empty list */
			*rhs = parser_newlistnode (OrgFileOf (node));
		} else if (!parser_islistnode (*rhs)) {
			*rhs = parser_makelistnode (*rhs);
		}
	}

	return 1;
}
/*}}}*/


/*{{{  static int eac_prescope_pcompnode (compops_t *cops, tnode_t **tptr, prescope_t *ps)*/
/*
 *	pre-scope for a process composition node (PAR/HIDE)
 *	returns 0 to stop walk, 1 to continue
 */
static int eac_prescope_pcompnode (compops_t *cops, tnode_t **tptr, prescope_t *ps)
{
	tnode_t *node = *tptr;

	if (node->tag == eac.tag_HIDE) {
		tnode_t **hptr = tnode_nthsubaddr (node, 1);

		if (!*hptr) {
			/* make empty list */
			*hptr = parser_newlistnode (OrgFileOf (node));
		} else if (!parser_islistnode (*hptr)) {
			*hptr = parser_makelistnode (*hptr);
		}
	}
	return 1;
}
/*}}}*/
/*{{{ static int eac_typecheck_pcompnode (compops_t *cops, tnode_t *node, typecheck_t *tc)*/
/*
 *	type-checks a process comprehension node (HIDE/PAR)
 *	returns 0 to stop walk, 1 to continue
 */
static int
eac_typecheck_pcompnode (compops_t *cops, tnode_t *node, typecheck_t *tc)
{
	tnode_t *lhs = NULL;
	tnode_t *rhs = NULL;
	tnode_t **varlist;
	int nvar, i;

	if (node->tag == eac.tag_HIDE) {
#if 0
		fprintf(stderr, "____eac_typecheck_hidenode___\n");
		tnode_dumpstree (node, 1, stderr);
		fprintf(stderr, "________\n");
#endif
		lhs = tnode_nthsubof (node, 0);
		rhs = tnode_nthsubof (node, 1);

		/* TODO: handle LHS if needed */

		/* RHS: should be .... TODO */
		varlist = parser_getlistitems (rhs, &nvar);
		for (i = 0; i < nvar; ++i) {
			/* check each param is a var */
#if 0
			fprintf(stderr, "____eac_typecheck_hidenode: [RHS:%d]___\n",i);
			tnode_dumpstree (varlist[i], 1, stderr);
			fprintf(stderr, "________\n");
#endif
			if (!(varlist[i]->tag == eac.tag_NCHANVAR)) {
				typecheck_error(node, tc, "\"%s\" in hidding list should be a NCHANVAR but found a %s",
				    (varlist[i]->tag->ndef == eac.node_NAMENODE) 
				    ? NameNameOf(tnode_nthnameof (varlist[i], 0)) : "unknown",
				    varlist[i]->tag->name);
				return 1;
			}
		}
	}

	return 1;
}
/*}}}*/


/*{{{ static int eac_typecheck_varcompnode (compops_t *cops, tnode_t *node, typecheck_t *tc)*/
/*
 *	type-checks a variable comprehension node (VARCOMP)
 *	returns 0 to stop walk, 1 to continue
 */
static int
eac_typecheck_varcompnode (compops_t *cops, tnode_t *node, typecheck_t *tc)
{
	tnode_t *lhs = tnode_nthsubof (node, 0);
	tnode_t *rhs = tnode_nthsubof (node, 1);
	tnode_t **varlist;
	int nvar, i;

		if  (!(lhs->tag == eac.tag_NVAR) || lhs->tag == eac.tag_VARCOMP) {
			typecheck_error(node, tc, "\"%s\" on LHS of VARCOMP should be a data type but found a %s",
			    (lhs->tag->ndef == eac.node_NAMENODE) ? NameNameOf(tnode_nthnameof (lhs, 0)) : "unknown",
			    lhs->tag->name);
			return 1;
		}

		varlist = parser_getlistitems (rhs, &nvar);

		for (i = 0 ; i < nvar; ++i) {
			if (!(varlist[i]->tag == eac.tag_NVAR || varlist[i]->tag == eac.tag_VARCOMP)) {
				typecheck_error(node, tc, "\"%s\" on RHS of VARCOMP should be a data type but found a %s",
				    (varlist[i]->tag->ndef == eac.node_NAMENODE) ? NameNameOf(tnode_nthnameof (varlist[i], 0)) : "unknown",
				    varlist[i]->tag->name);
				return 1;

			}
		}

	return 1;

}
/*}}}*/


/*{{{ static int eac_typecheck_chanmark (compops_t *cops, tnode_t *node, typecheck_t *tc)*/
/*
 *	type-checks a channel-mark (SVREND/CLIEND) node
 *	returns 0 to stop walk, 1 to continue
 */
static int
eac_typecheck_chanmark (compops_t *cops, tnode_t *node, typecheck_t *tc)
{
	tnode_t *inner;

#if 0
	fprintf(stderr, "____eac_typecheck_chanmark___\n");
	tnode_dumpstree (node, 1, stderr);
	fprintf(stderr, "________\n");
#endif

	if (node->tag == eac.tag_CLIEND || node->tag == eac.tag_SVREND) {
		inner = tnode_nthsubof(node, 0);
		if (inner->tag == eac.tag_NVAR) {
			//TODO
		} else if (((node->tag == eac.tag_CLIEND) && (inner->tag != eac.tag_NCLICHANVAR)) ||
				((node->tag == eac.tag_SVREND) && (inner->tag != eac.tag_NSVRCHANVAR))) {
			typecheck_error(node, tc, "Found a %s when expecting a %s", inner->tag->name,
					(node->tag == eac.tag_CLIEND ?  eac.tag_NCLICHANVAR->name : eac.tag_NSVRCHANVAR->name ));
		}
	}
	return 1;

}
/*}}}*/


/*{{{ static int eac_typecheck_actionnode (compops_t *cops, tnode_t *node, typecheck_t *tc)*/
/*
 *	type-checks an action-node (INPUT/OUTPUT)
 *	returns 0 to stop walk, 1 to continue
 */
static int
eac_typecheck_actionnode (compops_t *cops, tnode_t *node, typecheck_t *tc)
{
	tnode_t *lhs = tnode_nthsubof (node, 0);
	tnode_t *rhs = tnode_nthsubof (node, 1);
	name_t *lhs_name, *rhs_name;

#if 0
	fprintf (stderr, "eac_typecheck_actionnode(): action of:\n");
	tnode_dumpstree (node, 1, stderr);
	fprintf(stderr, "\n");

	fprintf(stderr, "LHS:");
	tnode_dumpstree (lhs, 1, stderr);
	fprintf(stderr, "RHS:");
	tnode_dumpstree (rhs, 2, stderr);

	fprintf(stderr, "----\n\n");
#endif

	/* Check LHS is a channel */
	if (!(lhs->tag == eac.tag_NCHANVAR || lhs->tag == eac.tag_SVREND || lhs->tag == eac.tag_CLIEND)) {
		if (lhs->tag->ndef != eac.node_NAMENODE) {
			typecheck_error (node, tc, "Item on LHS of %s is not a name.",
					(node->tag == eac.tag_INPUT ? "INPUT" : "OUTPUT"));
#if 0
			tnode_dumptree(lhs, 1, stderr);
			fprintf(stderr, "\n\n");
#endif
			return 1;
		}

		lhs_name = tnode_nthnameof (lhs, 0);
		typecheck_error(node, tc, "\"%s\" on LHS of %s should be a "
		    "channel but found a %s",
		    NameNameOf(lhs_name), (node->tag == eac.tag_INPUT ?
		    "INPUT" : "OUTPUT"), lhs->tag->name);
		return 1;
	}

	/* Check RHS is a VAR or a VARCOMP */
	if (!(rhs->tag == eac.tag_NVAR || rhs->tag == eac.tag_VARCOMP ||
			rhs->tag == eac.tag_CLIEND || rhs->tag == eac.tag_SVREND)) {
		if (rhs->tag->ndef != eac.node_NAMENODE) {
			typecheck_error (node, tc, "Item on RHS of %s is not a name.",
					(node->tag == eac.tag_INPUT ? "INPUT" : "OUTPUT"));
#if 0
			tnode_dumptree(rhs, 1, stderr);
#endif
			return 1;
		}
		rhs_name = tnode_nthnameof (rhs, 0);
		typecheck_error(node, tc, "\"%s\" on RHS of %s should be a "
		    "variable but found a %s",
		    NameNameOf(rhs_name), (node->tag == eac.tag_INPUT ?
		    "INPUT" : "OUTPUT"), rhs->tag->name);
		return 1;
	}

	return 1;

}
/*}}}*/


/*{{{  static int eac_prescope_esetnode (compops_t *cops, tnode_t **tptr, prescope_t *ps)*/
/*
 *	pre-scope for escape set node -- make sure any contents are a list
 *	returns 0 to stop walk, 1 to continue
 */
static int eac_prescope_esetnode (compops_t *cops, tnode_t **tptr, prescope_t *ps)
{
	tnode_t *node = *tptr;

	if (node->tag == eac.tag_ESET) {
		tnode_t **cptr = tnode_nthsubaddr (node, 0);

		if (!*cptr) {
			/* make empty list */
			*cptr = parser_newlistnode (OrgFileOf (node));
		} else if (!parser_islistnode (*cptr)) {
			*cptr = parser_makelistnode (*cptr);
		}
	}

	return 1;
}
/*}}}*/




/*{{{  static int eac_code_init_nodes (void)*/
/*
 *	initialises EAC declaration nodes
 *	returns 0 on success, non-zero on failure
 */
static int eac_code_init_nodes (void)
{
	tndef_t *tnd;
	int i;
	compops_t *cops;
	langops_t *lops;

	/*{{{  register named functions*/
	fcnlib_addfcn ("eac_nametoken_to_hook", (void *)eac_nametoken_to_hook, 1, 1);

	/*}}}*/
	/*{{{  eac:rawnamenode -- EACNAME*/
	i = -1;
	tnd = tnode_newnodetype ("eac:rawnamenode", &i, 0, 0, 1, TNF_NONE);			/* hooks: raw-name */
	tnd->hook_free = eac_rawnamenode_hook_free;
	tnd->hook_copy = eac_rawnamenode_hook_copy;
	tnd->hook_dumptree = eac_rawnamenode_hook_dumptree;
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (eac_scopein_rawname));
	tnd->ops = cops;

	i = -1;
	eac.tag_NAME = tnode_newnodetag ("EACNAME", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  eac:namenode -- EACNPROCDEF, EACNCHANVAR, EACNVAR*/
	i = -1;
	tnd = tnode_newnodetype ("eac:namenode", &i, 0, 1, 0, TNF_NONE);			/* names: name */
	cops = tnode_newcompops ();
	tnd->ops = cops;
	eac.node_NAMENODE = tnd;

	i = -1;
	eac.tag_NPROCDEF = tnode_newnodetag ("EACNPROCDEF", &i, tnd, NTF_NONE);
	i = -1;
	eac.tag_NCHANVAR = tnode_newnodetag ("EACNCHANVAR", &i, tnd, NTF_NONE);
	i = -1;
	eac.tag_NSVRCHANVAR = tnode_newnodetag ("EACNSVRCHANVAR", &i, tnd, NTF_NONE);
	i = -1;
	eac.tag_NCLICHANVAR = tnode_newnodetag ("EACNCLICHANVAR", &i, tnd, NTF_NONE);
	i = -1;
	eac.tag_NVAR = tnode_newnodetag ("EACNVAR", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  eac:actionnode -- EACINPUT, EACOUTPUT*/
	i = -1;
	tnd = tnode_newnodetype ("eac:actionnode", &i, 2, 0, 0, TNF_NONE);			/* subnodes: left, right */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (eac_typecheck_actionnode));
	tnd->ops = cops;

	i = -1;
	eac.tag_INPUT = tnode_newnodetag ("EACINPUT", &i, tnd, NTF_NONE);
	i = -1;
	eac.tag_OUTPUT = tnode_newnodetag ("EACOUTPUT", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  eac:varcomp -- EACVARCOMP*/
	i = -1;
	tnd = tnode_newnodetype ("eac:varcomp", &i, 2, 0, 0, TNF_NONE);				/* subnodes: left, right */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "prescope", 2, COMPOPTYPE (eac_prescope_varcompnode));
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (eac_typecheck_varcompnode));
	tnd->ops = cops;

	i = -1;
	eac.tag_VARCOMP = tnode_newnodetag ("EACVARCOMP", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  eac:declnode -- EACDECL*/
	i = -1;
	tnd = tnode_newnodetype ("eac:declnode", &i, 3, 0, 0, TNF_NONE);			/* subnodes: name, params, body */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "prescope", 2, COMPOPTYPE (eac_prescope_declnode));
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (eac_scopein_declnode));
	tnode_setcompop (cops, "fetrans", 2, COMPOPTYPE (eac_fetrans_declnode));
	tnd->ops = cops;

	i = -1;
	eac.tag_DECL = tnode_newnodetag ("EACDECL", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  eac:fvpenode -- EACFVPEXPR*/
	i = -1;
	tnd = tnode_newnodetype ("eac:fvpenode", &i, 2, 0, 0, TNF_NONE);			/* subnodes: expression, freevars */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (eac_scopein_fvpenode));
	tnd->ops = cops;

	i = -1;
	eac.tag_FVPEXPR = tnode_newnodetag ("EACFVPEXPR", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  eac:eseqnode -- EACESEQ*/
	i = -1;
	tnd = tnode_newnodetype ("eac:eseqnode", &i, 1, 0, 0, TNF_NONE);			/* subnodes: escape-sequence-list */
	cops = tnode_newcompops ();
	tnd->ops = cops;

	i = -1;
	eac.tag_ESEQ = tnode_newnodetag ("EACESEQ", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  eac:esetnode -- EACESET*/
	i = -1;
	tnd = tnode_newnodetype ("eac:esetnode", &i, 2, 0, 0, TNF_NONE);			/* subnodes: escape-sequences, freevars */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "prescope", 2, COMPOPTYPE (eac_prescope_esetnode));
	tnd->ops = cops;

	i = -1;
	eac.tag_ESET = tnode_newnodetag ("EACESET", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  eac:chanmark -- EACSVREND, EACCLIEND*/
	i = -1;
	tnd = tnode_newnodetype ("eac:chanmark", &i, 1, 0, 0, TNF_NONE);			/* subnodes: channel */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (eac_typecheck_chanmark));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	
	/* TODO: add LOPS_ISCOMMUNICABLE : call through to langops_iscommunicable() on subnode */
	tnd->lops = lops;

	i = -1;
	eac.tag_SVREND = tnode_newnodetag ("EACSVREND", &i, tnd, NTF_NONE);
	/* lops = tnode_newlangops();*/

	i = -1;
	eac.tag_CLIEND = tnode_newnodetag ("EACCLIEND", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  eac:pcompnode -- EACPAR, EACHIDE*/
	i = -1;
	tnd = tnode_newnodetype ("eac:pcompnode", &i, 2, 0, 0, TNF_NONE);			/* subnodes: left, right */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "prescope", 2, COMPOPTYPE (eac_prescope_pcompnode));
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (eac_typecheck_pcompnode));
	tnd->ops = cops;

	i = -1;
	eac.tag_PAR = tnode_newnodetag ("EACPAR", &i, tnd, NTF_NONE);
	i = -1;
	eac.tag_HIDE = tnode_newnodetag ("EACHIDE", &i, tnd, NTF_NONE);
	/*}}}*/
	/*{{{  eac:psubstnode -- EACSUBST*/
	i = -1;
	tnd = tnode_newnodetype ("eac:psubstnode", &i, 3, 0, 0, TNF_NONE);			/* subnodes: expression, new-name, in-expr-name */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "prescope", 2, COMPOPTYPE (eac_prescope_psubstnode));
	tnd->ops = cops;

	i = -1;
	eac.tag_SUBST = tnode_newnodetag ("EACSUBST", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  eac:instancenode -- EACINSTANCE*/
	i = -1;
	tnd = tnode_newnodetype ("eac:instancenode", &i, 2, 0, 0, TNF_NONE);			/* subnodes: name, params */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "prescope", 2, COMPOPTYPE (eac_prescope_instancenode));
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (eac_typecheck_instancenode));
	tnd->ops = cops;

	i = -1;
	eac.tag_INSTANCE = tnode_newnodetag ("EACINSTANCE", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  eac:typenode -- EACPROC, EACCHANVAR, EACVAR*/
	i = -1;
	tnd = tnode_newnodetype ("eac:typenode", &i, 0, 0, 0, TNF_NONE);
	cops = tnode_newcompops ();
	tnd->ops = cops;

	i = -1;
	eac.tag_PROC = tnode_newnodetag ("EACPROC", &i, tnd, NTF_NONE);
	i = -1;
	eac.tag_CHANVAR = tnode_newnodetag ("EACCHANVAR", &i, tnd, NTF_NONE);
	i = -1;
	eac.tag_VAR = tnode_newnodetag ("EACVAR", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  eac:vardeclnode -- EACVARDECL*/
	i = -1;
	tnd = tnode_newnodetype ("eac:vardeclnode", &i, 1, 0, 0, TNF_NONE);			/* subnodes: name */
	cops = tnode_newcompops ();
	tnd->ops = cops;

	i = -1;
	eac.tag_VARDECL = tnode_newnodetag ("EACVARDECL", &i, tnd, NTF_NONE);

	/*}}}*/

	return 0;
}
/*}}}*/


/*{{{  eac_code_feunit (feunit_t)*/
feunit_t eac_code_feunit = {
	.init_nodes = eac_code_init_nodes,
	.reg_reducers = NULL,
	.init_dfatrans = NULL,
	.post_setup = NULL,
	.ident = "eac-code"
};
/*}}}*/


