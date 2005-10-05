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
#include "version.h"
#include "symbols.h"

/* things that are considered symbols lie in ASCII ranges:
 *	0x21 (!) -> 0x2F (/)
 *	0x3A (:) -> 0x3F (?)
 *	0x5B ([) -> 0x60 (`)
 */

#define SYMBASE 0x20		/* still need null */
#define SYMSIZE 0x60

static symbol_t symbols[] = {
	{"!", 1, NULL},
	{"?", 1, NULL},
	{"::", 2, NULL},
	{":", 1, NULL},
	{";", 1, NULL},
	{"+", 1, NULL},
	{"*", 1, NULL},
	{"-", 1, NULL},
	{"/", 1, NULL},
	{"\\", 1, NULL},
	{"[", 1, NULL},
	{"]", 1, NULL},
	{"(", 1, NULL},
	{")", 1, NULL},
	{">", 1, NULL},
	{"<", 1, NULL},
	{":=", 2, NULL},
	{"!=", 2, NULL},
	{"=", 1, NULL},
	{"<=", 2, NULL},
	{"=>", 2, NULL},
	{"<>", 2, NULL},
	{"!=", 2, NULL},
	{"><", 2, NULL},
	{"^", 1, NULL},
	{"??", 2, NULL},
	{"[>", 2, NULL},
	{"<]", 2, NULL},
	{"==", 2, NULL},
	{">=", 2, NULL},
	{"=<", 2, NULL},
	{",", 1, NULL},
	{"_", 1, NULL},
	{"~", 1, NULL},
	{"#", 1, NULL},
	{"->", 2, NULL},
	{"/\\", 2, NULL},
	{"\\/", 2, NULL},
	{"<<", 2, NULL},
	{">>", 2, NULL},
	{"<<<", 3, NULL},
	{">>>", 3, NULL},
	{NULL, 0, NULL}
};

static symbol_t ***sym_lookup;


/*{{{  void symbols_init (void)*/
/*
 *	initialises the symbols table
 */
void symbols_init (void)
{
	int i;

	sym_lookup = (symbol_t ***)smalloc (SYMSIZE * sizeof (symbol_t **));
	for (i=0; i<SYMSIZE; i++) {
		sym_lookup[i] = NULL;
	}

	/* initialise from built-in symbols -- always one or two characters long */
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
	return;
}
/*}}}*/
/*{{{  symbol_t *symbols_lookup (const char *str, const int len)*/
/*
 *	looks up a symbol
 */
symbol_t *symbols_lookup (const char *str, const int len)
{
	int fch = (int)(str[0]);
	int sch = (int)(str[1]);

#if 0
fprintf (stderr, "symbols_lookup(): for [%s], len = %d\n", str, len);
#endif
	if ((fch < SYMBASE) || ((fch - SYMBASE) >= SYMSIZE) || !sym_lookup[fch - SYMBASE] || ((len == 1) && !(sym_lookup[fch - SYMBASE][0]))) {
		return NULL;
	} else if (len == 1) {
		return sym_lookup[fch - SYMBASE][0];
	} else if ((fch >= SYMBASE) && ((sch - SYMBASE) < SYMSIZE)) {
		return sym_lookup[fch - SYMBASE][sch - SYMBASE];
	}
	return NULL;
}
/*}}}*/
/*{{{  symbol_t *symbols_match (const char *str, const char *limit)*/
/*
 *	matches a symbol (limit says where we should definitely stop looking)
 */
symbol_t *symbols_match (const char *str, const char *limit)
{
	int fch = (int)(str[0]);
	int sch;

	if ((fch < SYMBASE) || ((fch - SYMBASE) >= SYMSIZE) || !sym_lookup[fch - SYMBASE]) {
		/* no such symbol (row) */
		return NULL;
	} else if ((str + 1) == limit) {
		/* match at end */
		return sym_lookup[fch - SYMBASE][0];
	} else {
		sch = (int)(str[1]);

		if ((sch < SYMBASE) || ((sch - SYMBASE) >= SYMSIZE)) {
			/* match first char only */
			return sym_lookup[fch - SYMBASE][0];
		} else if (sym_lookup[fch - SYMBASE][sch - SYMBASE]) {
			/* matched two */
			return sym_lookup[fch - SYMBASE][sch - SYMBASE];
		} else {
			/* only match one */
			return sym_lookup[fch - SYMBASE][0];
		}
	}
}
/*}}}*/


