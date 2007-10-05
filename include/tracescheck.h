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

typedef enum ENUM_tchknodetype {
	TCN_INVALID = 0,
	TCN_SEQ = 1,
	TCN_PAR = 2,
	TCN_FIXPOINT = 3,
	TCN_ATOM = 4,
	TCN_INPUT = 5,
	TCN_OUTPUT = 6,
	TCN_DET = 7,
	TCN_NDET = 8
} tchknodetype_e;

typedef struct TAG_tchknode {
	tchknodetype_e type;
	union {
		struct {
			DYNARRAY (struct TAG_chknode *, items);
		} tcnlist;				/* for SEQ, PAR, DET and NDET */
		struct {
			struct TAG_chknode *id;		/* fixpoint process identifier */
			struct TAG_chknode *proc;	/* RHS process */
		} tcnfix;
		struct {
			char *id;
		} tcnatom;
		struct {
			struct TAG_tnode *varptr;	/* tnode-level name-node */
		} tcnio;				/* for INPUT and OUTPUT */
	} u;
} tchknode_t;


typedef struct TAG_tchk_state {
	int inparams;

	int err;
	int warn;
} tchk_state_t;


extern int tracescheck_init (void);
extern int tracescheck_shutdown (void);

extern int tracescheck_subtree (struct TAG_tnode *tree, tchk_state_t *tcstate);
extern int tracescheck_tree (struct TAG_tnode *tree, struct TAG_langparser *lang);


#endif	/* !__TRACESCHECK_H */

