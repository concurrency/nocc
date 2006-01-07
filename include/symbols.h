/*
 *	symbols.h -- lexer symbol interface
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

#ifndef __SYMBOLS_H
#define __SYMBOLS_H

typedef struct TAG_symbol {
	char *match;
	int mlen;
	void *origin;
} symbol_t;

extern int symbols_init (void);
extern int symbols_shutdown (void);

extern symbol_t *symbols_lookup (const char *str, const int len);
extern symbol_t *symbols_match (const char *str, const char *limit);

extern symbol_t *symbols_add (const char *str, const int len, void *origin);

#endif	/* !__SYMBOLS_H */

