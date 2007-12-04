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
struct TAG_chook;

typedef enum ENUM_tchknodetype {
	TCN_INVALID = 0,
	TCN_SEQ = 1,
	TCN_PAR = 2,
	TCN_FIXPOINT = 3,
	TCN_ATOM = 4,
	TCN_INPUT = 5,
	TCN_OUTPUT = 6,
	TCN_DET = 7,
	TCN_NDET = 8,
	TCN_ATOMREF = 9,
	TCN_NODEREF = 10,
	TCN_SKIP = 11,
	TCN_STOP = 12,
	TCN_DIV = 13,
	TCN_CHAOS = 14
} tchknodetype_e;

typedef struct TAG_tchknode {
	tchknodetype_e type;
	struct TAG_tnode *orgnode;			/* so we know where it came from */
	int mark;
	union {
		struct {
			DYNARRAY (struct TAG_tchknode *, items);
		} tcnlist;				/* for SEQ, PAR, DET and NDET */
		struct {
			struct TAG_tchknode *id;	/* fixpoint process identifier */
			struct TAG_tchknode *proc;	/* RHS process */
		} tcnfix;
		struct {
			char *id;
		} tcnatom;
		struct {
			struct TAG_tchknode *varptr;
		} tcnio;				/* for INPUT and OUTPUT */
		struct {
			struct TAG_tchknode *aref;
		} tcnaref;				/* ATOMREF */
		struct {
			struct TAG_tnode *nref;		/* tnode-level pointer */
		} tcnnref;
	} u;
} tchknode_t;

typedef struct TAG_tchk_bucket {
	struct TAG_tchk_bucket *prevbucket;		/* previous bucket */
	DYNARRAY (tchknode_t *, items);			/* partial collections */
} tchk_bucket_t;

typedef struct TAG_tchk_traces {
	DYNARRAY (tchknode_t *, items);			/* set of traces */
} tchk_traces_t;

typedef struct TAG_tchk_state {
	struct TAG_tchk_state *prevstate;		/* previous state */
	int inparams;					/* non-zero if we're examining a parameter list */

	DYNARRAY (tchknode_t *, ivars);			/* interesting/interface variables */
	tchk_traces_t *traces;				/* collected traces */
	tchk_bucket_t *bucket;				/* partial collections */

	int err;
	int warn;
} tchk_state_t;

typedef struct TAG_tchk_tracewalk {
	tchknode_t *thisnode;
	DYNARRAY (tchknode_t *, stack);
	DYNARRAY (int, data);
	int depth;
	int end;
} tchk_tracewalk_t;

typedef struct TAG_tchk_check {
	tchk_state_t *state;
	tchk_traces_t *traces;
	struct TAG_tnode *spec;

	tchk_tracewalk_t *thiswalk;
	tchknode_t *thistrace;
	struct TAG_tnode *thisspec;

	int err;
	int warn;
} tchk_check_t;

extern int tracescheck_init (void);
extern int tracescheck_shutdown (void);

extern int tracescheck_subtree (struct TAG_tnode *tree, tchk_state_t *tcstate);
extern int tracescheck_tree (struct TAG_tnode *tree, struct TAG_langparser *lang);

extern void tracescheck_dumpbucket (tchk_bucket_t *tcb, int indent, FILE *stream);
extern void tracescheck_dumptraces (tchk_traces_t *tct, int indent, FILE *stream);
extern void tracescheck_dumpstate (tchk_state_t *tcstate, int indent, FILE *stream);
extern void tracescheck_dumpnode (tchknode_t *tcn, int indent, FILE *stream);

extern int tracescheck_formattraces (tchknode_t *tcn, char **sptr);

extern tchk_state_t *tracescheck_pushstate (tchk_state_t *tcstate);
extern tchk_state_t *tracescheck_popstate (tchk_state_t *tcstate);

extern tchk_bucket_t *tracescheck_newbucket (void);
extern int tracescheck_pushbucket (tchk_state_t *tcstate);
extern int tracescheck_popbucket (tchk_state_t *tcstate);
extern tchk_bucket_t *tracescheck_pullbucket (tchk_state_t *tcstate);
extern int tracescheck_freebucket (tchk_bucket_t *tcb);

extern int tracescheck_modprewalk (tchknode_t **tcnptr, int (*func)(tchknode_t **, void *), void *arg);
extern int tracescheck_prewalk (tchknode_t *tcn, int (*func)(tchknode_t *, void *), void *arg);

extern tchknode_t *tracescheck_dupref (tchknode_t *tcn);
extern tchknode_t *tracescheck_copynode (tchknode_t *tcn);
extern tchknode_t *tracescheck_createatom (void);
extern tchknode_t *tracescheck_createnode (tchknodetype_e type, struct TAG_tnode *orgnode, ...);
extern int tracescheck_simplifynode (tchknode_t **tcnptr);
extern tchknode_t *tracescheck_firstevent (tchknode_t *tcn);

extern int tracescheck_addtolistnode (tchknode_t *tcn, tchknode_t *item);
extern int tracescheck_buckettotraces (tchk_state_t *tcstate);
extern tchk_traces_t *tracescheck_newtraces (void);
extern tchk_traces_t *tracescheck_pulltraces (tchk_state_t *tcstate);
extern int tracescheck_freetraces (tchk_traces_t *tct);
extern int tracescheck_simplifytraces (tchk_traces_t *tct);
extern tchk_traces_t *tracescheck_copytraces (tchk_traces_t *tr);
extern int tracescheck_prunetraces (tchk_traces_t *tct, struct TAG_tnode *vlist);

extern int tracescheck_addivar (tchk_state_t *tcstate, tchknode_t *tcn);
extern int tracescheck_addtobucket (tchk_state_t *tcstate, tchknode_t *tcn);
extern int tracescheck_cleanrefchooks (tchk_state_t *tcstate, struct TAG_tnode *tptr);

extern tchk_tracewalk_t *tracescheck_startwalk (tchknode_t *start);
extern tchknode_t *tracescheck_stepwalk (tchk_tracewalk_t *ttw);
extern int tracescheck_endwalk (tchk_tracewalk_t *ttw);
extern void tracescheck_testwalk (tchknode_t *tcn);

extern tchknode_t *tracescheck_totrace (struct TAG_tnode *tptr);

extern struct TAG_chook *tracescheck_getnoderefchook (void);
extern struct TAG_chook *tracescheck_gettracesrefchook (void);
extern struct TAG_chook *tracescheck_gettraceschook (void);
extern struct TAG_chook *tracescheck_getimplchook (void);
extern struct TAG_chook *tracescheck_getbvarschook (void);

extern int tracescheck_docheckspec (struct TAG_tnode *spec, tchk_traces_t *traces, tchk_state_t *tcstate, struct TAG_tnode *locn);
extern int tracescheck_dosubcheckspec (struct TAG_tnode *spec, tchknode_t *trace, tchk_check_t *tcc);

extern void tracescheck_warning (struct TAG_tnode *node, tchk_state_t *tcstate, const char *fmt, ...) __attribute__ ((format (printf, 3, 4)));
extern void tracescheck_error (struct TAG_tnode *node, tchk_state_t *tcstate, const char *fmt, ...) __attribute__ ((format (printf, 3, 4)));

extern void tracescheck_checkwarning (struct TAG_tnode *node, tchk_check_t *tcc, const char *fmt, ...) __attribute__ ((format (printf, 3, 4)));
extern void tracescheck_checkerror (struct TAG_tnode *node, tchk_check_t *tcc, const char *fmt, ...) __attribute__ ((format (printf, 3, 4)));


#endif	/* !__TRACESCHECK_H */

