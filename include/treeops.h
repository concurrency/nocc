/*
 *	treeops.h -- interface to tree operations
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

#ifndef __TREEOPS_H
#define __TREEOPS_H

struct TAG_tnode;
struct TAG_ntdef;

extern struct TAG_tnode *treeops_findintree (struct TAG_tnode *tree, struct TAG_ntdef *tag);
extern struct TAG_tnode **treeops_findintreeptr (struct TAG_tnode **tree, struct TAG_ntdef *tag);
extern struct TAG_tnode *treeops_findtwointree (struct TAG_tnode *tree, struct TAG_ntdef *tag1, struct TAG_ntdef *tag2);
extern struct TAG_tnode *treeops_findprocess (struct TAG_tnode *tree);

extern struct TAG_tnode *treeops_transform (struct TAG_tnode *tree, ...);

extern int treeops_init (void);
extern int treeops_shutdown (void);


#endif	/* !__TREEOPS_H */

