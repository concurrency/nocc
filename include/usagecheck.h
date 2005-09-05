/*
 *	usagecheck.h -- interface to parallel usage checker
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

#ifndef __USAGECHECK_H
#define __USAGECHECK_H

struct TAG_tnode;
struct TAG_langparser;
struct TAG_chook;


typedef enum ENUM_uchk_mode {
	USAGE_NONE = 0x00,
	USAGE_READ = 0x01,
	USAGE_WRITE = 0x02,
	USAGE_INPUT = 0x04,
	USAGE_XINPUT = 0x08,
	USAGE_OUTPUT = 0x10
} uchk_mode_t;

typedef struct TAG_uchk_state {
	DYNARRAY (void *, ucstack);
	DYNARRAY (void *, setptrs);
	int ucptr;
} uchk_state_t;


extern int usagecheck_init (void);
extern int usagecheck_shutdown (void);

extern int usagecheck_addname (struct TAG_tnode *node, uchk_state_t *ucstate, uchk_mode_t mode);

extern int usagecheck_begin_branches (struct TAG_tnode *node, uchk_state_t *ucstate);
extern int usagecheck_end_branches (struct TAG_tnode *node, uchk_state_t *ucstate);
extern int usagecheck_branch (struct TAG_tnode *node, uchk_state_t *ucstate);
extern void usagecheck_newbranch (uchk_state_t *ucstate);
extern void usagecheck_endbranch (uchk_state_t *ucstate);

extern int usagecheck_subtree (struct TAG_tnode *node, uchk_state_t *ucstate);
extern int usagecheck_tree (struct TAG_tnode *tree, struct TAG_langparser *lang);

extern int usagecheck_marknode (struct TAG_tnode *node, uchk_mode_t mode, int do_nested);


#endif	/* !__USAGECHECK_H */

