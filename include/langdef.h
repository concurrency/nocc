/*
 *	langdef.h -- language definitions for NOCC
 *	Copyright (C) 2006 Fred Barnes <frmb@kent.ac.uk>
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

#ifndef __LANGDEF_H
#define __LANGDEF_H


struct TAG_tnode;
struct TAG_lexfile;


typedef enum ENUM_langdefent {
	REDUCTION = 1,
	DFATRANS = 2,
	DFABNF = 3
} langdefent_e;

typedef struct TAG_langdefent {
	langdefent_e type;
	union {
		struct {
			char *desc;		/* reduction specification (GRL) */
		} redex;
		char *dfarule;			/* for DFATRANS and DFABNF */
	} u;
} langdefent_t;

typedef struct TAG_langdef {
	char *ident;
	DYNARRAY (langdefent_t *, ents);
} langdef_t;


extern int langdef_init (void);
extern int langdef_shutdown (void);


#endif	/* !__LANGDEF_H */

