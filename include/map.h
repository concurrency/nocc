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

typedef struct TAG_mapstate {
	char *id;
	void *ptr;
} mapstate_t;

#define MAP_MAPSTATE_BITSIZE (3)

typedef struct TAG_map {
	int lexlevel;
	struct TAG_target *target;
	int err;
	int warn;
	void *hook;			/* for language-specific things */

	struct TAG_chook *mapchook;
	struct TAG_chook *allocevhook;
	struct TAG_chook *precodehook;

	DYNARRAY (struct TAG_tnode *, thisblock);		/* indexed by lex-level */
	DYNARRAY (struct TAG_tnode **, thisprocparams);		/* indexed by lex-level */
	struct TAG_tnode *thisberesult;

	int inparamlist;		/* non-zero if we're mapping parameters */
	STRINGHASH (mapstate_t *, mstate, MAP_MAPSTATE_BITSIZE);
} map_t;


extern int map_mapnames (struct TAG_tnode **tptr, struct TAG_target *target);
extern int map_subpremap (struct TAG_tnode **tptr, map_t *mdata);
extern int map_submapnames (struct TAG_tnode **tptr, map_t *mdata);
extern int map_subbemap (struct TAG_tnode **tptr, map_t *mdata);
extern int map_addtoresult (struct TAG_tnode **nodep, map_t *mdata);

extern int map_pushlexlevel (map_t *mdata, struct TAG_tnode *thisblock, struct TAG_tnode **thisprocparams);
extern int map_poplexlevel (map_t *mdata);
extern struct TAG_tnode *map_thisblock_cll (map_t *mdata);
extern struct TAG_tnode **map_thisprocparams_cll (map_t *mdata);
extern struct TAG_tnode *map_thisblock_ll (map_t *mdata, int lexlevel);
extern struct TAG_tnode **map_thisprocparams_ll (map_t *mdata, int lexlevel);

extern int map_hasstate (map_t *mdata, const char *id);
extern void *map_getstate (map_t *mdata, const char *id);
extern int map_setstate (map_t *mdata, const char *id, void *data);
extern void map_clearstate (map_t *mdata, const char *id);

extern int map_init (void);
extern int map_shutdown (void);


#endif	/* !__MAP_H */

