/*
 *	library.h -- library/separate-compilation interface
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

#ifndef __LIBRARY_H
#define __LIBRARY_H

struct TAG_tnode;
struct TAG_lexfile;

extern struct TAG_tnode *library_newlibnode (struct TAG_lexfile *lf, char *libname);
extern int library_addincludes (struct TAG_tnode *libnode, char *iname);
extern int library_adduses (struct TAG_tnode *libnode, char *lname);
extern struct TAG_tnode *library_newlibpublictag (struct TAG_lexfile *lf, char *name);
extern int library_markpublic (struct TAG_tnode *node);


extern int library_init (void);
extern int library_shutdown (void);

#endif	/* !__LIBRARY_H */

