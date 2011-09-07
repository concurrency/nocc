/*
 *	eacpriv.h -- private stuff for escape analysis (mostly interactive things)
 *	Copyright (C) 2011 Fred Barnes <frmb@kent.ac.uk>
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
 *	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#ifndef __EACPRIV_H
#define __EACPRIV_H

struct TAG_name;

typedef struct TAG_eac_istate {
	DYNARRAY (struct TAG_name *, procs);			/* names of defined procedures */
} eac_istate_t;

extern void eac_init_istate (void);
extern void eac_shutdown_istate (void);

extern eac_istate_t *eac_getistate (void);

struct TAG_tnode;

extern char *eac_format_expr (struct TAG_tnode *expr);
extern int eac_evaluate (const char *str);
extern int eac_isinteractive (void);


typedef struct TAG_eac_subst {
	int count;
	struct TAG_tnode *newtree;
	struct TAG_name *oldname;
} eac_subst_t;

extern int eac_substituteintree (struct TAG_tnode **tptr, eac_subst_t *subst);

#endif	/* !__EACPRIV_H */

