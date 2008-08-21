/*
 *	mobilitycheck.h -- mobility checker interface for NOCC
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

#ifndef __MOBILITYCHECK_H
#define __MOBILITYCHECK_H

struct TAG_tnode;
struct TAG_langparser;
struct TAG_chook;

typedef enum ENUM_mchknodetype {
	MCN_INVALID = 0,
	MCN_INPUT = 1,
	MCN_OUTPUT = 2,
	MCN_PARAM = 3,
	MCN_PARAMREF = 4,
	MCN_VAR = 5,
	MCN_VARREF = 6,
	MCN_SEQ = 7
} mchknodetype_e;

typedef struct TAG_mchknode {
	mchknodetype_e type;
	struct TAG_tnode *orgnode;			/* so we know where it came from */
	union {
		struct {
			struct TAG_mchknode *chanptr;
			struct TAG_mchknode *varptr;
		} mcnio;				/* for INPUT and OUTPUT */
		struct {
			char *id;
		} mcnpv;				/* PARAM or VAR */
		struct {
			struct TAG_mchknode *ref;
		} mcnref;				/* PARAMREF or VARREF */
		struct {
			DYNARRAY (struct TAG_mchknode *, items);
		} mcnlist;				/* SEQ */
	} u;
} mchknode_t;

typedef struct TAG_mchk_traces {
	DYNARRAY (mchknode_t *, items);			/* set of mobility traces */
	DYNARRAY (mchknode_t *, params);		/* bound parameters */
	DYNARRAY (mchknode_t *, vars);			/* free variables */
} mchk_traces_t;

typedef struct TAG_mchk_bucket {
	struct TAG_mchk_bucket *prevbucket;		/* previous bucket */
	DYNARRAY (mchknode_t *, items);			/* partial collections */
} mchk_bucket_t;


typedef struct TAG_mchk_state {
	struct TAG_mchk_state *prevstate;		/* previous state */
	int inparams;					/* non-zero if we're examining a parameter list */

	DYNARRAY (mchknode_t *, ichans);		/* interesting channels (on which mobiles move) */
	DYNARRAY (mchknode_t *, ivars);			/* interesting variables (which hold mobiles) */
	mchk_bucket_t *bucket;				/* partial trace collections */

	int err;
	int warn;
} mchk_state_t;


extern int mobilitycheck_init (void);
extern int mobilitycheck_shutdown (void);

extern int mobilitycheck_subtree (struct TAG_tnode *node, mchk_state_t *mcstate);
extern int mobilitycheck_tree (struct TAG_tnode *node, struct TAG_langparser *lang);

extern void mobilitycheck_dumpbucket (mchk_bucket_t *mcb, int indent, FILE *stream);
extern void mobilitycheck_dumptraces (mchk_traces_t *mct, int indent, FILE *stream);
extern void mobilitycheck_dumpstate (mchk_state_t *mcstate, int indent, FILE *stream);
extern void mobilitycheck_dumpnode (mchknode_t *mcn, int indent, FILE *stream);

extern mchknode_t *mobilitycheck_copynode (mchknode_t *mcn);

extern mchk_state_t *mobilitycheck_pushstate (mchk_state_t *mcstate);
extern mchk_state_t *mobilitycheck_popstate (mchk_state_t *mcstate);

extern struct TAG_chook *mobilitycheck_gettraceschook (void);


#endif	/* !__MOBILITYCHECK_H */

