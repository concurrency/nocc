/*
 *	langops.h -- interface to langage-level operations
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

#ifndef __LANGOPS_H
#define __LANGOPS_H

struct TAG_tnode;

extern int langops_init (void);
extern int langops_shutdown (void);

extern void langops_getdescriptor (struct TAG_tnode *node, char **str);
extern void langops_getname (struct TAG_tnode *node, char **str);
extern int langops_isconst (struct TAG_tnode *node);
extern int langops_constvalof (struct TAG_tnode *node, void *ptr);
extern int langops_valbyref (struct TAG_tnode *node);
extern int langops_isvar (struct TAG_tnode *node);
extern int langops_iscomplex (struct TAG_tnode *node, int deep);
extern struct TAG_tnode *langops_retypeconst (struct TAG_tnode *node, struct TAG_tnode *type);
extern struct TAG_tnode *langops_dimtreeof (struct TAG_tnode *node);
extern struct TAG_tnode *langops_hiddenparamsof (struct TAG_tnode *node);
extern int langops_hiddenslotsof (struct TAG_tnode *node);
extern int langops_typehash (struct TAG_tnode *node, const int hsize, void *ptr);
extern int langops_typehash_blend (const int dsize, void *dptr, const int ssize, void *sptr);


#endif	/* !__LANGOPS_H */

