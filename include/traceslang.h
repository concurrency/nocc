/*
 *	traceslang.h -- traces language for NOCC
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

#ifndef __TRACESLANG_H
#define __TRACESLANG_H

struct TAG_feunit;
struct TAG_langdef;
struct TAG_langlexer;
struct TAG_langparser;

extern struct TAG_langlexer traceslang_lexer;
extern struct TAG_langparser traceslang_parser;

extern int traceslang_init (void);
extern int traceslang_shutdown (void);

extern int traceslang_initialise (void);

#define NTF_TRACESLANGSTRUCTURAL 0x0010
#define NTF_TRACESLANGCOPYALIAS 0x0020

struct TAG_tndef;
struct TAG_ntdef;
struct TAG_tnode;

typedef struct {
	struct TAG_ntdef *tag_NAME;
	struct TAG_ntdef *tag_LITINT;
	struct TAG_ntdef *tag_LITSTR;
	struct TAG_ntdef *tag_SEQ;
	struct TAG_ntdef *tag_PAR;
	struct TAG_ntdef *tag_DET;
	struct TAG_ntdef *tag_NDET;
	struct TAG_ntdef *tag_INPUT;
	struct TAG_ntdef *tag_OUTPUT;
	struct TAG_ntdef *tag_NPARAM;
	struct TAG_ntdef *tag_EVENT;
	struct TAG_ntdef *tag_SKIP;
	struct TAG_ntdef *tag_STOP;
	struct TAG_ntdef *tag_CHAOS;
	struct TAG_ntdef *tag_DIV;
	struct TAG_ntdef *tag_INSTANCE;
} traceslang_pset_t;

extern traceslang_pset_t traceslang;


typedef struct TAG_traceslang_eset {
	DYNARRAY (struct TAG_tnode *, events);
} traceslang_eset_t;

extern void traceslang_isetindent (FILE *stream, int indent);
extern struct TAG_langdef *traceslang_getlangdef (void);

/* traceslang_expr.c */
extern struct TAG_feunit traceslang_expr_feunit;

/* traceslang_fe.c */
extern struct TAG_tnode *traceslang_newevent (struct TAG_tnode *locn);
extern struct TAG_tnode *traceslang_newnparam (struct TAG_tnode *locn);

extern int traceslang_isequal (struct TAG_tnode *n1, struct TAG_tnode *n2);

extern struct TAG_tnode *traceslang_structurecopy (struct TAG_tnode *expr);
extern int traceslang_registertracetype (struct TAG_ntdef *tag);
extern int traceslang_unregistertracetype (struct TAG_ntdef *tag);
extern int traceslang_isregisteredtracetype (struct TAG_ntdef *tag);

extern traceslang_eset_t *traceslang_newset (void);
extern void traceslang_freeset (traceslang_eset_t *eset);
extern void traceslang_dumpset (traceslang_eset_t *eset, int indent, FILE *stream);

extern traceslang_eset_t *traceslang_firstevents (struct TAG_tnode *expr);
extern traceslang_eset_t *traceslang_lastevents (struct TAG_tnode *expr);



#endif	/* !__TRACESLANG_H */

