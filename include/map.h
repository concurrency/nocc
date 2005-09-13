/*
 *	map.h -- interface to the nocc mapper
 *	Copyright (C) 2005 Fred Barnes <frmb@kent.ac.uk>
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

#ifndef __MAP_H
#define __MAP_H

struct TAG_tnode;
struct TAG_target;
struct TAG_chook;

typedef struct TAG_map {
	int lexlevel;
	struct TAG_target *target;
	int err;
	int warn;
	struct TAG_chook *mapchook;
	struct TAG_tnode *thisblock;
	struct TAG_tnode **thisprocparams;
} map_t;


extern int map_mapnames (struct TAG_tnode **tptr, struct TAG_target *target);
extern int map_subpremap (struct TAG_tnode **tptr, map_t *mdata);
extern int map_submapnames (struct TAG_tnode **tptr, map_t *mdata);

extern int map_init (void);
extern int map_shutdown (void);


#endif	/* !__MAP_H */

