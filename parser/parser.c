/*
 *	parser.c -- nocc parser framework
 *	Copyright (C) 2004 Fred Barnes <frmb@kent.ac.uk>
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
#include "occampi.h"
#include "names.h"


/*{{{  private stuff*/
typedef struct TAG_reducer {
	char *name;
	void (*reduce)(dfastate_t *, parsepriv_t *, void *);
	void *rarg;
} reducer_t;

STATICSTRINGHASH (reducer_t *, reducers, 6);
STATICDYNARRAY (reducer_t *, areducers);

static ntdef_t *tag_LIST;

typedef struct TAG_ngrule {
	char *name;
	void *grule;
} ngrule_t;

STATICSTRINGHASH (ngrule_t *, ngrules, 6);

/*}}}*/



/*{{{  void parser_init (void)*/
/*
 *	initialises the parser
 */
void parser_init (void)
{
	stringhash_init (reducers);
	dynarray_init (areducers);

	tag_LIST = tnode_lookupnodetag ("list");

	parser_register_reduce ("Rinlist", parser_inlistreduce, NULL);

	stringhash_init (ngrules);

	parser_register_grule ("parser:nullreduce", parser_decode_grule ("N+R-"));
	parser_register_grule ("parser:listresult", parser_decode_grule ("R+N-"));
	return;
}
/*}}}*/
/*{{{  void parser_shutdown (void)*/
/*
 *	shuts-down the parser
 */
void parser_shutdown (void)
{
	/* FIXME: should free up reducers, really .. */
	return;
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
		*max = *max + 8;
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
		*max = *max + 8;
	}
	for (i=*cur + 1; i>1; i--) {
		array[i+1] = array[i];
	}
	array[2] = item;
	*cur = *cur + 1;

	return (array + 2);
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
			/*{{{  COMBINE*/
		case ICDE_COMBINE:
			{
				int ccnt = (int)arg[++ipos];
				ntdef_t *tag = (ntdef_t *)arg[++ipos];
				tnode_t *rnode;

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

					org_file = onode->org_file;
					org_line = onode->org_line;
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

					org_file = otok->origin;
					org_line = otok->lineno;
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
			/*{{{  V -- reverse/swap two items at top of stack*/
		case 'V':
			if (!lsdepth) {
				nocc_warning ("parser_decode_grule(): strange swap nothing..");
			}
			ilen++;
			break;
			/*}}}*/
			/*{{{  T -- token-stack operation*/
		case 'T':
			xrule++;
			if (*xrule != '+') {
				goto report_error_out;
			} else {
				ilen++;
				lsdepth++;
			}
			break;
			/*}}}*/
			/*{{{  X -- user-supplied operations*/
		case 'X':
			if (xrule[1] == '*') {
				xrule++;
				userparams[uplen++] = (void *)va_arg (ap, void (*)(void **));
			} else {
				userparams[uplen++] = (void *)va_arg (ap, void *(*)(void *));
			}
			ilen += 2;
			break;
			/*}}}*/
			/*{{{  Y -- user-supplied whole-stack operation*/
		case 'Y':
			userparams[uplen++] = (void *)va_arg (ap, void (*)(void **, int));
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
			/*{{{  C -- condense into new token*/
		case 'C':
			xrule++;
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
			userparams[uplen++] = (void *)va_arg (ap, ntdef_t *);
			ilen += 3;
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
			/*{{{  V -- reverse/swap two items at top of stack*/
		case 'V':
			icode[i] = ICDE_REV;
			break;
			/*}}}*/
			/*{{{  T -- token-stack operation*/
		case 'T':
			xrule++;
			/* must be T+, already checked */
			icode[i] = ICDE_TSPOP;
			break;
			/*}}}*/
			/*{{{  X -- user-supplied operations*/
		case 'X':
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
			/*{{{  C -- condense into new token*/
		case 'C':
			xrule++;
			icode[i++] = ICDE_COMBINE;
			icode[i++] = (int)(*xrule - '0');
			icode[i] = (unsigned int)(userparams[uplen++]);
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
		}
	}
	icode[i] = ICDE_END;

	return (void *)icode;
report_error_out:
	nocc_error ("parser_decode_grule(): error at char %d in \"%s\"", (int)(xrule - rule), rule);
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

