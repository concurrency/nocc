/*
 *	names.h -- nocc name handling
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

#ifndef __NAMES_H
#define __NAMES_H

struct TAG_tnode;
struct TAG_namelist;

typedef struct TAG_name {
	int refc;
	struct TAG_tnode *decl;
	struct TAG_tnode *type;
	struct TAG_tnode *namenode;
	struct TAG_namelist *me;
} name_t;

typedef struct TAG_namelist {
	char *name;
	DYNARRAY (name_t *, scopes);
	int curscope;
} namelist_t;


extern void name_init (void);
extern void name_shutdown (void);
extern name_t *name_lookup (char *str);
extern name_t *name_addscopename (char *str, struct TAG_tnode *decl, struct TAG_tnode *type, struct TAG_tnode *namenode);
extern void name_scopename (name_t *name);
extern void name_descopename (name_t *name);
extern void name_delname (name_t *name);
extern name_t *name_addtempname (struct TAG_tnode *decl, struct TAG_tnode *type, struct TAG_ntdef *nametag, struct TAG_tnode **namenode);

extern void *name_markscope (void);
extern void name_markdescope (void *mark);

extern void name_dumpname (name_t *name, int indent, FILE *stream);
extern void name_dumpnames (FILE *stream);


#define NameDeclOf(N)		(N)->decl
#define NameTypeOf(N)		(N)->type
#define NameNodeOf(N)		(N)->namenode
#define NameNameOf(N)		(N)->me->name

#define NameDeclAddr(N)		(&((N)->decl))
#define NameTypeAddr(N)		(&((N)->type))
#define NameNodeAddr(N)		(&((N)->namenode))

#define SetNameDecl(N,T)	(N)->decl = (T)
#define SetNameType(N,T)	(N)->type = (T)
#define SetNameNode(N,T)	(N)->namenode = (T)

#endif	/* !__NAMES_H */

