/*
 *	precheck.h -- interface to pre-checker
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

#ifndef __PRECHECK_H
#define __PRECHECK_H

struct TAG_tnode;

extern int precheck_init (void);
extern int precheck_shutdown (void);

extern int precheck_subtree (struct TAG_tnode *tree);
extern int precheck_tree (struct TAG_tnode *tree);


#endif	/* !__PRECHECK_H */


