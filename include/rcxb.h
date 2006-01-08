/*
 *	rcxb.h -- BASIC-style language for the LEGO Mindstorms (tm) RCX
 *	Copyright (C) 2006 Fred Barnes <frmb@kent.ac.uk>
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

#ifndef __RCXB_H
#define __RCXB_H

struct TAG_langlexer;
struct TAG_langparser;

extern struct TAG_langlexer rcxb_lexer;
extern struct TAG_langparser rcxb_parser;

struct TAG_tndef;
struct TAG_ntdef;
struct TAG_token;


typedef struct {
	struct TAG_tndef *node_ACTIONNODE;
	struct TAG_tndef *node_LEAFNODE;

	struct TAG_ntdef *tag_NAME;

	struct TAG_ntdef *tag_MOTORA;
	struct TAG_ntdef *tag_MOTORB;
	struct TAG_ntdef *tag_MOTORC;
	struct TAG_ntdef *tag_SENSOR1;
	struct TAG_ntdef *tag_SENSOR2;
	struct TAG_ntdef *tag_SENSOR3;

	struct TAG_ntdef *tag_LITSTR;
	struct TAG_ntdef *tag_LITINT;

	struct TAG_ntdef *tag_SETMOTOR;
	struct TAG_ntdef *tag_SETSENSOR;
} rcxb_pset_t;

extern rcxb_pset_t rcxb;


extern void rcxb_isetindent (FILE *stream, int indent);

extern struct TAG_feunit rcxb_program_feunit;           /* rcxb_program.c */


#endif	/* !__RCXB_H */

