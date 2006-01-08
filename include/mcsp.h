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
	struct TAG_tndef *node_NAMENODE;
	struct TAG_tndef *node_DOPNODE;
	struct TAG_tndef *node_SCOPENODE;
	struct TAG_tndef *node_LEAFPROC;
	struct TAG_tndef *node_CONSTNODE;

	struct TAG_ntdef *tag_NAME;
	struct TAG_ntdef *tag_EVENT;
	struct TAG_ntdef *tag_PROCDEF;

	struct TAG_ntdef *tag_SUBEVENT;
	struct TAG_ntdef *tag_THEN;
	struct TAG_ntdef *tag_PAR;
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

	struct TAG_ntdef *tag_STRING;
} mcsp_pset_t;

extern mcsp_pset_t mcsp;


struct TAG_tnode;
struct TAG_prescope;
struct TAG_scope;
struct TAG_feunit;


extern void mcsp_isetindent (FILE *stream, int indent);

extern struct TAG_feunit mcsp_process_feunit;		/* mcsp_process.c */


#endif	/* !__MCSP_H */

