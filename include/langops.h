/*
 *	langops.h -- interface to langage-level operations
 *	Copyright (C) 2005-2016  Fred Barnes, University of Kent <frmb@kent.ac.uk>
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
struct TAG_name;
struct TAG_tchk_traces;

extern int langops_init (void);
extern int langops_shutdown (void);

extern void langops_getdescriptor (struct TAG_tnode *node, char **str);
extern void langops_getname (struct TAG_tnode *node, char **str);
extern int langops_isconst (struct TAG_tnode *node);
extern int64_t langops_constvalof (struct TAG_tnode *node, void *ptr);
extern int langops_constsizeof (struct TAG_tnode *node);
extern int langops_valbyref (struct TAG_tnode *node);
extern int langops_isvar (struct TAG_tnode *node);
extern int langops_iscomplex (struct TAG_tnode *node, int deep);
extern int langops_isaddressable (struct TAG_tnode *node);
extern int langops_isdefpointer (struct TAG_tnode *node);
extern struct TAG_tnode *langops_retypeconst (struct TAG_tnode *node, struct TAG_tnode *type);
extern struct TAG_tnode *langops_dimtreeof (struct TAG_tnode *node);
extern struct TAG_tnode *langops_dimtreeof_node (struct TAG_tnode *node, struct TAG_tnode *varnode);
extern struct TAG_tnode *langops_hiddenparamsof (struct TAG_tnode *node);
extern int langops_hiddenslotsof (struct TAG_tnode *node);
extern int langops_typehash (struct TAG_tnode *node, const int hsize, void *ptr);
extern int langops_typehash_blend (const int dsize, void *dptr, const int ssize, void *sptr);
extern struct TAG_tnode *langops_getbasename (struct TAG_tnode *node);
extern struct TAG_tnode *langops_getfieldname (struct TAG_tnode *node);
extern struct TAG_tnode *langops_getfieldnamelist (struct TAG_tnode *node);
extern int langops_iscommunicable (struct TAG_tnode *node);
extern struct TAG_tnode *langops_gettags (struct TAG_tnode *node);
extern struct TAG_name *langops_nameof (struct TAG_tnode *node);
extern struct TAG_tnode *langops_tracespecof (struct TAG_tnode *node);
extern void langops_getctypeof (struct TAG_tnode *node, char **str);
extern int langops_guesstlp (struct TAG_tnode *node);
extern struct TAG_tnode *langops_initcall (struct TAG_tnode *type, struct TAG_tnode *name);
extern struct TAG_tnode *langops_freecall (struct TAG_tnode *type, struct TAG_tnode *name);

#endif	/* !__LANGOPS_H */

