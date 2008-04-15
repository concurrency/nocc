/*
 *	valueset.h -- interface to value->stuff mapping routines
 *	Copyright (C) 2008 Fred Barnes <frmb@kent.ac.uk>
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

#ifndef __VALUESET_H
#define __VALUESET_H

typedef enum ENUM_setstrategy {
	STRAT_NONE,
	STRAT_CHAIN,			/* simple IF type test-chain */
	STRAT_TABLE,			/* jump table */
	STRAT_HASH			/* hashed jump table */
} setstrategy_e;

typedef struct TAG_valueset {
	DYNARRAY (int, values);
	DYNARRAY (tnode_t *, links);
	int v_min, v_max;
	int v_base, v_limit;
	setstrategy_e strat;
} valueset_t;


extern int valueset_init (void);
extern int valueset_shutdown (void);

extern valueset_t *valueset_create (void);
extern void valueset_free (valueset_t *vset);
extern void valueset_dumptree (valueset_t *vset, int indent, FILE *stream);

extern int valueset_insert (valueset_t *vset, int val, tnode_t *link);
extern int valueset_decide (valueset_t *vset);



#endif	/* !__VALUESET_H */

