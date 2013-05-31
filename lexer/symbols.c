/*
 *	symbols.c -- symbol handling
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
#include <sys/types.h>
#include <unistd.h>

#include "nocc.h"
#include "support.h"
#include "origin.h"
#include "version.h"
#include "lexer.h"
#include "lexpriv.h"
#include "symbols.h"

/* things that are considered symbols lie in ASCII ranges:
 *	0x21 (!) -> 0x2F (/)
 *	0x3A (:) -> 0x3F (?)
 *	0x5B ([) -> 0x60 (`)
 */

#define SYMBASE 0x20		/* still need null */
#define SYMSIZE 0x60

static symbol_t symbols[] = {
	{"!", 1, LANGTAG_OCCAMPI | LANGTAG_EAC | LANGTAG_GUPPY, NULL},
	{"?", 1, LANGTAG_OCCAMPI | LANGTAG_EAC | LANGTAG_GUPPY, NULL},
	{"::", 2, LANGTAG_OCCAMPI, NULL},
	{":", 1, LANGTAG_OCCAMPI | LANGTAG_GUPPY, NULL},
	{";", 1, LANGTAG_OCCAMPI | LANGTAG_GUPPY, NULL},
	{"+", 1, LANGTAG_OCCAMPI | LANGTAG_GUPPY, NULL},
	{"*", 1, LANGTAG_OCCAMPI | LANGTAG_GUPPY, NULL},
	{"-", 1, LANGTAG_OCCAMPI | LANGTAG_GUPPY, NULL},
	{"/", 1, LANGTAG_OCCAMPI | LANGTAG_EAC | LANGTAG_GUPPY, NULL},
	{"\\", 1, LANGTAG_OCCAMPI | LANGTAG_EAC | LANGTAG_GUPPY, NULL},
	{"[", 1, LANGTAG_OCCAMPI | LANGTAG_EAC | LANGTAG_GUPPY, NULL},
	{"]", 1, LANGTAG_OCCAMPI | LANGTAG_EAC | LANGTAG_GUPPY, NULL},
	{"(", 1, LANGTAG_OCCAMPI | LANGTAG_EAC | LANGTAG_GUPPY, NULL},
	{")", 1, LANGTAG_OCCAMPI | LANGTAG_EAC | LANGTAG_GUPPY, NULL},
	{">", 1, LANGTAG_OCCAMPI | LANGTAG_EAC | LANGTAG_GUPPY, NULL},
	{"<", 1, LANGTAG_OCCAMPI | LANGTAG_EAC | LANGTAG_GUPPY, NULL},
	{":=", 2, LANGTAG_OCCAMPI, NULL},
	{"!=", 2, LANGTAG_OCCAMPI | LANGTAG_GUPPY, NULL},
	{"=", 1, LANGTAG_OCCAMPI | LANGTAG_EAC | LANGTAG_GUPPY, NULL},
	{"<=", 2, LANGTAG_OCCAMPI | LANGTAG_GUPPY, NULL},
	{"=>", 2, LANGTAG_OCCAMPI | LANGTAG_GUPPY, NULL},
	{"<>", 2, LANGTAG_OCCAMPI, NULL},
	{"><", 2, LANGTAG_OCCAMPI | LANGTAG_GUPPY, NULL},
	{"^", 1, LANGTAG_OCCAMPI | LANGTAG_EAC | LANGTAG_GUPPY, NULL},
	{"??", 2, LANGTAG_OCCAMPI, NULL},
	{"[>", 2, LANGTAG_OCCAMPI, NULL},
	{"<]", 2, LANGTAG_OCCAMPI, NULL},
	{"==", 2, LANGTAG_OCCAMPI | LANGTAG_GUPPY, NULL},
	{">=", 2, LANGTAG_OCCAMPI, NULL},
	{"=<", 2, LANGTAG_OCCAMPI, NULL},
	{",", 1, LANGTAG_OCCAMPI | LANGTAG_EAC | LANGTAG_GUPPY, NULL},
	{"_", 1, LANGTAG_OCCAMPI, NULL},
	{".", 1, LANGTAG_GUPPY, NULL},
	{"~", 1, LANGTAG_OCCAMPI | LANGTAG_EAC | LANGTAG_GUPPY, NULL},
	{"#", 1, LANGTAG_OCCAMPI | LANGTAG_GUPPY, NULL},
	{"@", 1, LANGTAG_OCCAMPI | LANGTAG_GUPPY, NULL},
	{"->", 2, LANGTAG_OCCAMPI | LANGTAG_EAC | LANGTAG_GUPPY, NULL},
	{"<-", 2, LANGTAG_EAC, NULL},
	{"/\\", 2, LANGTAG_OCCAMPI, NULL},
	{"\\/", 2, LANGTAG_OCCAMPI, NULL},
	{"<<", 2, LANGTAG_OCCAMPI | LANGTAG_GUPPY, NULL},
	{">>", 2, LANGTAG_OCCAMPI | LANGTAG_GUPPY, NULL},
	{"&", 1, LANGTAG_OCCAMPI | LANGTAG_GUPPY, NULL},
	{"|", 1, LANGTAG_OCCAMPI | LANGTAG_GUPPY, NULL},
	{"||", 2, LANGTAG_OCCAMPI | LANGTAG_EAC | LANGTAG_GUPPY, NULL},
	{"&&", 2, LANGTAG_GUPPY, NULL},
	{"{", 1, LANGTAG_OCCAMPI | LANGTAG_EAC | LANGTAG_GUPPY, NULL},
	{"}", 1, LANGTAG_OCCAMPI | LANGTAG_EAC | LANGTAG_GUPPY, NULL},
	{"[]", 2, LANGTAG_OCCAMPI | LANGTAG_GUPPY, NULL},
	{NULL, 0, LANGTAG_OCCAMPI, NULL}
};

static symbol_t ***sym_lookup = NULL;
static symbol_t ***sym_extras = NULL;


/*{{{  void symbols_init (void)*/
/*
 *	initialises the symbols table
 *	returns 0 on success, non-zero on failure
 */
int symbols_init (void)
{
	int i;

	sym_lookup = (symbol_t ***)smalloc (SYMSIZE * sizeof (symbol_t **));
	for (i=0; i<SYMSIZE; i++) {
		sym_lookup[i] = NULL;
	}

	/* initialise from built-in symbols -- always (and only) one or two characters long */
	for (i=0; symbols[i].match; i++) {
		int fch = (int)(symbols[i].match[0] - SYMBASE);
		int sch = (int)(symbols[i].match[1]);

		if (sch != 0) {
			sch -= SYMBASE;
		}
		if (!sym_lookup[fch]) {
			int j;

			sym_lookup[fch] = (symbol_t **)smalloc (SYMSIZE * sizeof (symbol_t *));
			for (j=0; j<SYMSIZE; j++) {
				sym_lookup[fch][j] = NULL;
			}
		}
		sym_lookup[fch][sch] = &(symbols[i]);
	}

	/* setup extras array -- holds symbols added at run-time */
	sym_extras = (symbol_t ***)smalloc (SYMSIZE * sizeof (symbol_t **));
	for (i=0; i<SYMSIZE; i++) {
		sym_extras[i] = NULL;
	}

	return 0;
}
/*}}}*/
/*{{{  int symbols_shutdown (void)*/
/*
 *	shuts down symbols table
 *	returns 0 on success, non-zero on failure
 */
int symbols_shutdown (void)
{
	return 0;
}
/*}}}*/
/*{{{  symbol_t *symbols_lookup (const char *str, const int len, const unsigned int langtag)*/
/*
 *	looks up a symbol
 */
symbol_t *symbols_lookup (const char *str, const int len, const unsigned int langtag)
{
	int fch = (int)(str[0]);
	int sch = (int)(str[1]);

#if 0
fprintf (stderr, "symbols_lookup(): for [%s], len = %d\n", str, len);
#endif
	if ((fch < SYMBASE) || ((fch - SYMBASE) >= SYMSIZE) || (!sym_lookup[fch - SYMBASE] && !sym_extras[fch - SYMBASE]) || ((len == 1) && !(sym_lookup[fch - SYMBASE][0]))) {
		return NULL;
	} else if (sym_extras[fch - SYMBASE]) {
		int i;

		/* look through extras */
		for (i=0; sym_extras[fch - SYMBASE][i]; i++) {
			if ((sym_extras[fch - SYMBASE][i]->mlen == len) && !strncmp (sym_extras[fch - SYMBASE][i]->match, str, len)) {
				/* this one */
				symbol_t *sym = sym_extras[fch - SYMBASE][i];

				if (!langtag || ((sym->langtag & LANGTAG_LANGMASK & langtag) == langtag)) {
					return sym;
				}
			}
		}
	}

	/* look at predefined symbols */
	if (len == 1) {
		symbol_t *sym = sym_lookup[fch - SYMBASE][0];

		if (sym && langtag && ((sym->langtag & LANGTAG_LANGMASK & langtag) != langtag)) {
			return NULL;
		}
		return sym;
	} else if ((len == 2) && (fch >= SYMBASE) && ((sch - SYMBASE) < SYMSIZE)) {
		symbol_t *sym = sym_lookup[fch - SYMBASE][sch - SYMBASE];

		if (sym && langtag && ((sym->langtag & LANGTAG_LANGMASK & langtag) != langtag)) {
			return NULL;
		}
		return sym;
	}
	return NULL;
}
/*}}}*/
/*{{{  symbol_t *symbols_match (const char *str, const char *limit, const unsigned int langtag)*/
/*
 *	matches a symbol (limit says where we should definitely stop looking)
 */
symbol_t *symbols_match (const char *str, const char *limit, const unsigned int langtag)
{
	int fch = (int)(str[0]);
	int sch;

	if ((fch < SYMBASE) || ((fch - SYMBASE) >= SYMSIZE) || (!sym_lookup[fch - SYMBASE] && !sym_extras[fch - SYMBASE])) {
		/* no such symbol (row) */
		return NULL;
	} else if (sym_extras[fch - SYMBASE]) {
		int i;

		/* look through extras */
		for (i=0; sym_extras[fch - SYMBASE][i]; i++) {
			if (((str + sym_extras[fch - SYMBASE][i]->mlen) <= limit) && !strncmp (sym_extras[fch - SYMBASE][i]->match, str, sym_extras[fch - SYMBASE][i]->mlen)) {
				/* this one */
				symbol_t *sym = sym_extras[fch - SYMBASE][i];

				if (!langtag || ((sym->langtag & LANGTAG_LANGMASK & langtag) == langtag)) {
					return sym;
				}
			}
		}
	}

	/* look at predefined symbols */
	if ((str + 1) == limit) {
		/* match at end */
		symbol_t *sym = sym_lookup[fch - SYMBASE][0];

		return sym;
	} else {
		symbol_t *sym = NULL;

		sch = (int)(str[1]);

		if ((sch < SYMBASE) || ((sch - SYMBASE) >= SYMSIZE)) {
			/* match first char only */
			sym = sym_lookup[fch - SYMBASE][0];
		} else if (sym_lookup[fch - SYMBASE][sch - SYMBASE]) {
			/* matched two */
			sym = sym_lookup[fch - SYMBASE][sch - SYMBASE];
		} else {
			/* only match one */
			sym = sym_lookup[fch - SYMBASE][0];
		}
		if (sym && langtag && ((sym->langtag & LANGTAG_LANGMASK & langtag) != langtag)) {
			return NULL;
		}
		return sym;
	}
	return NULL;
}
/*}}}*/


/*{{{  symbol_t *symbols_add (const char *str, const int len, const unsigned int langtag, origin_t *origin)*/
/*
 *	adds a new symbol to the compiler -- placed in sym_extras array
 *	returns new symbol on success, NULL on failure
 */
symbol_t *symbols_add (const char *str, const int len, const unsigned int langtag, origin_t *origin)
{
	symbol_t *sym = symbols_lookup (str, len, 0);
	int fch;

	if (sym) {
		/* symbol in any language */
		if ((sym->langtag & LANGTAG_LANGMASK) == (langtag & LANGTAG_LANGMASK)) {
			nocc_warning ("symbols_add(): already got symbol [%s]", sym->match);
			return sym;
		}
		if ((sym->langtag & LANGTAG_IMASK) & (langtag & LANGTAG_IMASK)) {
			nocc_serious ("clobbering implementation-specific bits in symbol [%s]", sym->match);
		}

		/* merge in this one */
		sym->langtag |= langtag;
		return sym;
	}
	sym = (symbol_t *)smalloc (sizeof (symbol_t));
	sym->match = string_ndup (str, len);
	sym->mlen = len;
	sym->langtag = langtag;
	sym->origin = origin;

	fch = (int)(str[0]);
	if ((fch < SYMBASE) || ((fch - SYMBASE) >= SYMSIZE)) {
		nocc_error ("symbols_add(): symbol [%s] start does not fit in table!", sym->match);
		sfree (sym->match);
		sfree (sym);
		return NULL;
	}
	fch -= SYMBASE;
	if (len >= 3) {
		/* must go in extras */
		if (sym_extras[fch]) {
			int i;
			symbol_t **newextras;

			/* count entries and extend */
			for (i=0; sym_extras[fch][i]; i++);
			newextras = (symbol_t **)smalloc ((i + 2) * sizeof (symbol_t *));
			for (i=0; sym_extras[fch][i]; newextras[i] = sym_extras[fch][i], i++);
			newextras[i++] = sym;
			newextras[i] = NULL;

			/* free old, park new */
			sfree (sym_extras[fch]);
			sym_extras[fch] = newextras;
		} else {
			/* creating fresh */
			sym_extras[fch] = (symbol_t **)smalloc (2 * sizeof (symbol_t *));
			sym_extras[fch][0] = sym;
			sym_extras[fch][1] = NULL;
		}
	} else {
		/* must go in fixed symbol table */
		int sch = (int)(sym->match[1]);

		if (sch != 0) {
			sch -= SYMBASE;
		}
		if (!sym_lookup[fch]) {
			int j;

			sym_lookup[fch] = (symbol_t **)smalloc (SYMSIZE * sizeof (symbol_t *));
			for (j=0; j<SYMSIZE; j++) {
				sym_lookup[fch][j] = NULL;
			}
		}
		sym_lookup[fch][sch] = sym;
	}

	return sym;
}
/*}}}*/


