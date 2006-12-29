/*
 *	prescope.h -- interface to the NOCC pre-scoper
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

#ifndef __PRESCOPE_H
#define __PRESCOPE_H

struct TAG_tnode;
struct TAG_langparser;

typedef struct TAG_prescope {
	int err;		/* error count */
	int warn;		/* warning count */
	void *hook;		/* hook for language-specific use */
	struct TAG_langparser *lang;
} prescope_t;


extern int prescope_init (void);
extern int prescope_shutdown (void);


extern int prescope_subtree (struct TAG_tnode **t, prescope_t *ps);
extern int prescope_tree (struct TAG_tnode **t, struct TAG_langparser *lang);
extern int prescope_modprewalktree (struct TAG_tnode **node, void *arg);

extern void prescope_warning (struct TAG_tnode *node, prescope_t *ps, const char *fmt, ...);
extern void prescope_error (struct TAG_tnode *node, prescope_t *ps, const char *fmt, ...);


#endif	/* !__PRESCOPE_H */

