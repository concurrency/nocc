/*
 *	mcsp.h -- machine-readable CSP language interface for nocc
 *	Copyright (C) 2005-2006 Fred Barnes <frmb@kent.ac.uk>
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

#ifndef __MCSP_H
#define __MCSP_H

struct TAG_langlexer;
struct TAG_langparser;

extern struct TAG_langlexer mcsp_lexer;
extern struct TAG_langparser mcsp_parser;


struct TAG_tndef;
struct TAG_ntdef;
struct TAG_token;


typedef struct {
	/* saved node-types */
	struct TAG_tndef *node_NAMENODE;
	struct TAG_tndef *node_DOPNODE;			/* hooks mcsp_alpha_t */
	struct TAG_tndef *node_SNODE;			/* hooks mcsp_alpha_t */
	struct TAG_tndef *node_CNODE;			/* hooks mcsp_alpha_t */
	struct TAG_tndef *node_SCOPENODE;
	struct TAG_tndef *node_LEAFPROC;
	struct TAG_tndef *node_CONSTNODE;
	struct TAG_tndef *node_SPACENODE;
	struct TAG_tndef *node_REPLNODE;

	/* front-end tags */
	struct TAG_ntdef *tag_NAME;
	struct TAG_ntdef *tag_EVENT;
	struct TAG_ntdef *tag_PROCDEF;
	struct TAG_ntdef *tag_CHAN;
	struct TAG_ntdef *tag_VAR;

	struct TAG_ntdef *tag_SUBEVENT;
	struct TAG_ntdef *tag_THEN;
	struct TAG_ntdef *tag_PAR;
	struct TAG_ntdef *tag_ALPHAPAR;
	struct TAG_ntdef *tag_ILEAVE;
	struct TAG_ntdef *tag_SEQ;
	struct TAG_ntdef *tag_ICHOICE;
	struct TAG_ntdef *tag_ECHOICE;

	struct TAG_ntdef *tag_SKIP;
	struct TAG_ntdef *tag_STOP;
	struct TAG_ntdef *tag_DIV;
	struct TAG_ntdef *tag_CHAOS;

	struct TAG_ntdef *tag_HIDE;
	struct TAG_ntdef *tag_FIXPOINT;

	struct TAG_ntdef *tag_PROCDECL;
	struct TAG_ntdef *tag_XPROCDECL;

	struct TAG_ntdef *tag_INSTANCE;

	struct TAG_ntdef *tag_STRING;

	struct TAG_ntdef *tag_REPLSEQ;
	struct TAG_ntdef *tag_REPLPAR;
	struct TAG_ntdef *tag_REPLILEAVE;

	/* heading into the back-end */
	struct TAG_ntdef *tag_FPARAM;
	struct TAG_ntdef *tag_UPARAM;
	struct TAG_ntdef *tag_VARDECL;
	struct TAG_ntdef *tag_HIDDENPARAM;
	struct TAG_ntdef *tag_RETURNADDRESS;
	struct TAG_ntdef *tag_SYNC;
	struct TAG_ntdef *tag_SEQCODE;
	struct TAG_ntdef *tag_PARCODE;
	struct TAG_ntdef *tag_ALT;
	struct TAG_ntdef *tag_GUARD;
	struct TAG_ntdef *tag_CHANWRITE;
	struct TAG_ntdef *tag_PARSPACE;
	struct TAG_ntdef *tag_ILOOP;
	struct TAG_ntdef *tag_PRIDROP;

} mcsp_pset_t;

extern mcsp_pset_t mcsp;


struct TAG_tnode;
struct TAG_prescope;
struct TAG_scope;
struct TAG_feunit;


extern void mcsp_isetindent (FILE *stream, int indent);

extern struct TAG_feunit mcsp_process_feunit;		/* mcsp_process.c */

/* option handlers inside MCSP front-end */
struct TAG_cmd_option;
extern int mcsp_lexer_opthandler_flag (struct TAG_cmd_option *opt, char ***argwalk, int *argleft);


/*{{{  mcsp_lex_t structure (private lang-specific lexer state, here for mcsp_process to inspect)*/
typedef struct TAG_mcsp_lex {
	int unboundvars;		/* permit unbound variables ? */
} mcsp_lex_t;


/*}}}*/
/*{{{  mcsp_scope_t structure (private lang-specific scope state)*/
typedef struct TAG_mcsp_scope {
	struct TAG_tnode *uvinsertlist;		/* insert-list for unbound variables */
	void *uvscopemark;			/* where the scoper was */
	unsigned int inamescope : 1;		/* non-zero if scoping instance name (lhs) */
} mcsp_scope_t;


/*}}}*/
/*{{{  mcsp_alpha_t structure (hooked onto various nodes to indicate alphabets involved)*/
typedef struct TAG_mcsp_alpha {
	struct TAG_tnode *elist;
} mcsp_alpha_t;

/*}}}*/
/*{{{  mcsp_fetrans_t structure (private lang-specific front-end transform state)*/
typedef struct TAG_mcsp_fetrans {
	struct TAG_tnode *uvinsertlist;		/* insert-list for unbound variables */
	int errcount;				/* errors */
	int parse;				/* counter round front-end transforms */
	mcsp_alpha_t *curalpha;			/* current alphabet -- used in the last fetrans pass */
} mcsp_fetrans_t;


/*}}}*/


#endif	/* !__MCSP_H */

