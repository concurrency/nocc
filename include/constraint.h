/*
 *	constraint.h -- constraints handling for NOCC
 *	Copyright (C) 2007 Fred Barnes <frmb@kent.ac.uk>
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

#ifndef __CONSTRAINT_H
#define __CONSTRAINT_H

struct TAG_tnode;
struct TAG_tndef;
struct TAG_ntdef;


typedef struct TAG_cnstrlang {
	struct TAG_ntdef *tag_CVAR;	
} cnstrlang_t;


extern int constraint_init (void);
extern int constraint_shutdown (void);

#endif	/* !__CONSTRAINT_H */

