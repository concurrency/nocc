/*
 *	dfa.h -- DFA interface/definitions
 *	Copyright (C) 2005-2007 Fred Barnes <frmb@kent.ac.uk>
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

#ifndef __DFA_H
#define __DFA_H

struct TAG_token;
struct TAG_dfastate;
struct TAG_tnode;
struct TAG_lexfile;
struct TAG_dfaerrorhandler;

#define DFAFLAG_NONE 0x0000
#define DFAFLAG_NOCONSUME 0x0001	/* don't consume the token */
#define DFAFLAG_KEEP 0x0004		/* keep hold of the token on the token-stack */
#define DFAFLAG_PUSHSTACK 0x0008	/* push the DFA state stack when making this transition */
#define DFAFLAG_DEFERRED 0x0010		/* special for the parser when putting the DFA together */
#define DFAFLAG_DEFTARGET 0x0020	/* special for the parser when putting the DFA together */


/*
 *	DFA nodes
 */
typedef struct TAG_dfanode {
	DYNARRAY (struct TAG_token *, match);
	DYNARRAY (struct TAG_dfanode *, target);
	DYNARRAY (struct TAG_dfanode *, pushto);
	DYNARRAY (int, flags);

	void (*reduce)(struct TAG_dfastate *, struct TAG_parsepriv *, void *);
	void *rarg;
	void *dfainfo;			/* linked to a nameddfa_t node */
	int incoming;			/* number of incoming transitions */
} dfanode_t;


/*
 *	state stack when walking a DFA
 */
typedef struct TAG_dfastate {
	struct TAG_dfastate *prev;
	dfanode_t *cur;
	struct TAG_tnode *local;
	struct TAG_tnode **ptr;
	DYNARRAY (struct TAG_tnode *, nodestack);
} dfastate_t;


/*
 *	DFA transition tables
 */
typedef struct TAG_dfattblent {
	int s_state;
	int e_state;
	char *e_named;		/* if the ending state is complex */
	void *namedptr;		/* if given as a pointer */
	char *match;
	void (*reduce)(struct TAG_dfastate *, struct TAG_parsepriv *, void *);
	void *rarg;
	char *rname;		/* pending unresolved reductions */
} dfattblent_t;

typedef struct TAG_dfattbl {
	char *name;
	int op;			/* 0 = new, 1 = add */
	int nstates;
	DYNARRAY (dfattblent_t *, entries);
} dfattbl_t;


extern int dfa_init (void);
extern int dfa_shutdown (void);

extern void dfa_dumpdfa (FILE *stream, dfanode_t *dfa);
extern void dfa_dumpnameddfa (FILE *stream, char *dfaname);
extern void dfa_dumpdfas (FILE *stream);

extern dfanode_t *dfa_newnode (void);
extern dfanode_t *dfa_newnode_init (void (*reduce)(dfastate_t *, struct TAG_parsepriv *, void *), void *rarg);
extern void dfa_addmatch (dfanode_t *dfa, struct TAG_token *tok, dfanode_t *target, int flags);
extern void dfa_addpush (dfanode_t *dfa, struct TAG_token *tok, dfanode_t *pushto, dfanode_t *target, int flags);
extern void dfa_matchpush (dfanode_t *dfa, char *pushto, dfanode_t *target, int deferring);
extern void dfa_defaultto (dfanode_t *dfa, char *target);
extern void dfa_defaultpush (dfanode_t *dfa, char *pushto, dfanode_t *target);
extern void dfa_defaultreturn (dfanode_t *dfa);
extern int dfa_setname (dfanode_t *dfa, char *name);
extern void dfa_seterrorhandler (char *name, struct TAG_dfaerrorhandler *ehan);
extern struct TAG_dfaerrorhandler *dfa_geterrorhandler (char *name);
extern dfanode_t *dfa_lookupbyname (char *name);
extern int dfa_findmatch (dfanode_t *dfa, struct TAG_token *tok, dfanode_t **r_pushto, dfanode_t **r_target, int *r_flags);

extern dfanode_t *dfa_decoderule (const char *rule, ...);
extern dfanode_t *dfa_decodetrans (const char *rule, ...);

extern void dfa_freettbl (dfattbl_t *ttbl);
extern void dfa_dumpttbl (FILE *stream, dfattbl_t *ttbl);
extern void dfa_dumpttbl_gra (FILE *stream, dfattbl_t *ttbl);
extern dfattbl_t *dfa_transtotbl (const char *rule, ...);
extern dfattbl_t *dfa_bnftotbl (const char *rule, ...);
extern dfanode_t *dfa_tbltodfa (dfattbl_t *ttbl);

extern int dfa_mergetables (dfattbl_t **tables, int ntables);
extern int dfa_clear_deferred (void);
extern int dfa_match_deferred (void);
extern void dfa_dumpdeferred (FILE *stream);


extern int dfa_advance (dfastate_t **dfast, struct TAG_parsepriv *pp, struct TAG_token *tok);
extern void dfa_pushnode (dfastate_t *dfast, struct TAG_tnode *node);
extern struct TAG_tnode *dfa_popnode (dfastate_t *dfast);

extern struct TAG_tnode *dfa_walk (char *rname, struct TAG_lexfile *lf);

extern dfastate_t *dfa_newstate (dfastate_t *prev);
extern dfastate_t *dfa_newstate_init (dfastate_t *prev, char *iname);
extern void dfa_freestate (dfastate_t *dfast);


#endif	/* !__DFA_H */

