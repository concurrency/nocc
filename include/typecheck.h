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
	int err;			/* error count */
	int warn;			/* warning count */
	void *hook;			/* hook for language-specific use */
	struct TAG_langparser *lang;	/* language */

	/* when doing type-actuals on parameters */
	tnode_t *this_ftype;		/* formal-parameter type */
	tnode_t *this_aparam;		/* actual-parameter */

	/* when doing cases for a variant protocol input */
	tnode_t *this_protocol;
} typecheck_t;

/* these are used to categorise types */
typedef enum ENUM_typecat {
	TYPE_NOTTYPE = 0,
	TYPE_SIGNED = 0x0001,
	TYPE_WIDTHSET = 0x0002,
	TYPE_DYNAMIC = 0x0004,
	TYPE_USERDEFINED = 0x0008,
	TYPE_INTEGER = 0x0010,
	TYPE_REAL = 0x0020,
	TYPE_DATA = 0x0040,
	TYPE_COMM = 0x0080,

	TYPE_WIDTHMASK = 0xffff0000
} typecat_e;


extern void typecheck_init (void);
extern void typecheck_shutdown (void);


extern int typecheck_subtree (struct TAG_tnode *t, typecheck_t *tc);
extern int typecheck_tree (struct TAG_tnode *t, struct TAG_langparser *lang);
extern int typecheck_prewalktree (struct TAG_tnode *node, void *arg);
extern struct TAG_tnode *typecheck_gettype (struct TAG_tnode *node, struct TAG_tnode *default_type);
extern struct TAG_tnode *typecheck_getsubtype (struct TAG_tnode *node, struct TAG_tnode *default_type);
extern struct TAG_tnode *typecheck_typeactual (struct TAG_tnode *formaltype, struct TAG_tnode *actualtype, struct TAG_tnode *node, typecheck_t *tc);
extern struct TAG_tnode *typecheck_fixedtypeactual (struct TAG_tnode *formaltype, struct TAG_tnode *actualtype, struct TAG_tnode *node, typecheck_t *tc, const int deep);
extern struct TAG_tnode *typecheck_typereduce (struct TAG_tnode *type);
extern int typecheck_cantypecast (struct TAG_tnode *node, struct TAG_tnode *srctype);
extern int typecheck_istype (struct TAG_tnode *node);
extern typecat_e typecheck_typetype (struct TAG_tnode *node);

extern int typeresolve_subtree (struct TAG_tnode **tptr, typecheck_t *tc);
extern int typeresolve_tree (struct TAG_tnode **tptr, struct TAG_langparser *lang);
extern int typeresolve_modprewalktree (struct TAG_tnode **tptr, void *arg);


extern void typecheck_warning (struct TAG_tnode *node, typecheck_t *tc, const char *fmt, ...) __attribute__ ((format (printf, 3, 4)));
extern void typecheck_error (struct TAG_tnode *node, typecheck_t *tc, const char *fmt, ...) __attribute__ ((format (printf, 3, 4)));

extern int typecheck_haserror (typecheck_t *tc);


#endif	/* !__TYPECHECK_H */

