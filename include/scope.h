/*
 *	scope.h -- interface to the nocc scoper
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

#ifndef __SCOPE_H
#define __SCOPE_H

struct TAG_langparser;
struct TAG_namespace;
struct TAG_tnode;

typedef struct TAG_scope {
	int err;		/* error count */
	int warn;		/* warning count */
	int scoped;		/* count of scoped names */
	struct TAG_langparser *lang;
	void *langpriv;		/* private structure for the langauage during scope */
	DYNARRAY (struct TAG_namespace *, defns);	/* defining namespaces */
	DYNARRAY (struct TAG_namespace *, usens);	/* in-use namespaces */
} scope_t;

extern int scope_init (void);
extern int scope_shutdown (void);


extern int scope_pushdefns (scope_t *ss, struct TAG_namespace *ns);
extern int scope_popdefns (scope_t *ss, struct TAG_namespace *ns);
extern int scope_pushusens (scope_t *ss, struct TAG_namespace *ns);
extern int scope_popusens (scope_t *ss, struct TAG_namespace *ns);

extern int scope_tree (struct TAG_tnode *t, struct TAG_langparser *lang);
extern int scope_subtree (struct TAG_tnode **tptr, scope_t *sarg);
extern int scope_modprewalktree (struct TAG_tnode **node, void *arg);
extern int scope_modpostwalktree (struct TAG_tnode **node, void *arg);

extern void scope_warning (struct TAG_tnode *node, scope_t *ss, const char *fmt, ...) __attribute__ ((format (printf, 3, 4)));
extern void scope_error (struct TAG_tnode *node, scope_t *ss, const char *fmt, ...) __attribute__ ((format (printf, 3, 4)));


#endif	/* !__SCOPE_H */

