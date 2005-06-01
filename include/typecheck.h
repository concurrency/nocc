/*
 *	typecheck.h -- interface to the nocc type-checker
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

#ifndef __TYPECHECK_H
#define __TYPECHECK_H

struct TAG_tnode;
struct TAG_langparser;

typedef struct TAG_typecheck {
	int err;		/* error count */
	int warn;		/* warning count */
	void *hook;		/* hook for language-specific use */
} typecheck_t;


extern void typecheck_init (void);
extern void typecheck_shutdown (void);


extern int typecheck_tree (struct TAG_tnode *t, struct TAG_langparser *lang);
extern int typecheck_prewalktree (struct TAG_tnode *node, void *arg);
extern struct TAG_tnode *typecheck_gettype (struct TAG_tnode *node, struct TAG_tnode *default_type);
extern struct TAG_tnode *typecheck_typeactual (struct TAG_tnode *formaltype, struct TAG_tnode *actualtype, struct TAG_tnode *node, typecheck_t *tc);


extern void typecheck_warning (struct TAG_tnode *node, typecheck_t *tc, const char *fmt, ...);
extern void typecheck_error (struct TAG_tnode *node, typecheck_t *tc, const char *fmt, ...);


#endif	/* !__TYPECHECK_H */

