/*
 *	constprop.h -- interface to the constant propagator for NOCC
 *	Copyright (C) 2005-2016 Fred Barnes <frmb@kent.ac.uk>
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

#ifndef __CONSTPROP_H
#define __CONSTPROP_H

struct TAG_tnode;


typedef enum ENUM_consttype {
	CONST_INVALID = 0,
	CONST_BOOL = 1,
	CONST_BYTE = 2,
	CONST_INT = 3,			/* NOTE: INT is now 64-bit */
	CONST_DOUBLE = 4,
	CONST_ULL = 5
} consttype_e;

extern int constprop_init (void);
extern int constprop_shutdown (void);

extern struct TAG_tnode *constprop_newconst (consttype_e ctype, struct TAG_tnode *orig, struct TAG_tnode *type, ...);
extern int constprop_isconst (struct TAG_tnode *node);
extern consttype_e constprop_consttype (struct TAG_tnode *tptr);
extern int constprop_sametype (struct TAG_tnode *tptr1, struct TAG_tnode *tptr2);
extern int64_t constprop_intvalof (struct TAG_tnode *tptr);
extern int constprop_tree (struct TAG_tnode **tptr);
extern int constprop_checkintrange (struct TAG_tnode *node, const int issigned, const int bits);

extern void constprop_error (struct TAG_tnode *tptr, const char *fmt, ...) __attribute__ ((format (printf, 2, 3)));
extern void constprop_warning (struct TAG_tnode *tptr, const char *fmt, ...) __attribute__ ((format (printf, 2, 3)));


#endif	/* !__CONSTPROP_H */

