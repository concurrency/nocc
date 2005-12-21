/*
 *	allocate.h -- memory allocator interface for NOCC
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

#ifndef __ALLOCATE_H
#define __ALLOCATE_H

struct TAG_tnode;
struct TAG_target;
struct TAG_chook;

typedef struct TAG_allocate {
	struct TAG_target *target;
	struct TAG_chook *varmap_chook;
	void *allochook;
	struct TAG_chook *ev_chook;
} allocate_t;

extern int allocate_init (void);
extern int allocate_shutdown (void);

extern int allocate_tree (struct TAG_tnode **tptr, struct TAG_target *target);
extern int preallocate_tree (struct TAG_tnode **tptr, struct TAG_target *target);

extern int allocate_walkvarmap (struct TAG_tnode *t, int memsp, int (*map_func)(void *, int, int, int, int, void *), int (*item_func)(void *, struct TAG_tnode *, int *, int *, void *), void *maparg, void *itemarg);

#endif	/* !__ALLOCATE_H */


