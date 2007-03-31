/*
 *	langdeflookup.h -- lookups for language definition keywords
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

#ifndef __LANGDEFLOOKUP_H
#define __LANGDEFLOOKUP_H


typedef enum ENUM_langdeflookup {
	LDL_INVALID = 0,
	LDL_IDENT = 1,
	LDL_DESC = 2,
	LDL_MAINTAINER = 3,
	LDL_SECTION = 4,
	LDL_GRULE = 5,
	LDL_RFUNC = 6,
	LDL_BNF = 7,
	LDL_TABLE = 8,
	LDL_SYMBOL = 9,
	LDL_KEYWORD = 10,
	LDL_DFAERR = 11,
	LDL_TNODE = 12
} langdeflookup_e;

typedef struct TAG_langdeflookup {
	char *name;
	langdeflookup_e ldl;
	void *origin;
} langdeflookup_t;


extern int langdeflookup_init (void);
extern int langdeflookup_shutdown (void);

extern langdeflookup_t *langdeflookup_lookup (const char *str, const int len);
extern int langdeflookup_add (const char *str, void *origin);

#endif	/* !__LANGDEFLOOKUP_H */


