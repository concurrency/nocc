/*
 *	tracescheck.h -- interface to traces checker
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

#ifndef __TRACESCHECK_H
#define __TRACESCHECK_H

struct TAG_tnode;
struct TAG_langparser;

typedef struct TAG_tchk_state {
	int err;
	int warn;
} tchk_state_t;


extern int tracescheck_init (void);
extern int tracescheck_shutdown (void);

extern int tracescheck_subtree (struct TAG_tnode *tree, tchk_state_t *tcstate);
extern int tracescheck_tree (struct TAG_tnode *tree, struct TAG_langparser *lang);


#endif	/* !__TRACESCHECK_H */

