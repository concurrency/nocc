/*
 *	postcheck.h -- interface to post-checker
 *	Copyright (C) 2006 Fred Barnes <frmb@kent.ac.uk>
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

#ifndef __POSTCHECK_H
#define __POSTCHECK_H

struct TAG_langparser;

typedef struct TAG_postcheck {
	struct TAG_langparser *lang;
	void *langpriv;
} postcheck_t;

struct TAG_tnode;

extern int postcheck_init (void);
extern int postcheck_shutdown (void);

extern int postcheck_subtree (struct TAG_tnode **treep, postcheck_t *pc);
extern int postcheck_tree (struct TAG_tnode **treep, struct TAG_langparser *lang);


#endif	/* !__POSTCHECK_H */


