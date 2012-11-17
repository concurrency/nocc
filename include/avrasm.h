/*
 *	avrasm.h -- AVR assembler
 *	Copyright (C) 2012 Fred Barnes <frmb@kent.ac.uk>
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

#ifndef __AVRASM_H
#define __AVRASM_H

struct TAG_langlexer;
struct TAG_langparser;

extern struct TAG_langlexer avrasm_lexer;
extern struct TAG_langparser avrasm_parser;

struct TAG_tnode;
struct TAG_tndef;
struct TAG_ntdef;
struct TAG_token;
struct TAG_langdef;

typedef struct {
	struct TAG_tndef *node_INSNODE;
	struct TAG_tndef *node_LABELNODE;

	struct TAG_token *tok_STRING;
	struct TAG_token *tok_DOT;

	struct TAG_ntdef *tag_NAME;

	struct TAG_ntdef *tag_LITSTR;
	struct TAG_ntdef *tag_LITINT;
	struct TAG_ntdef *tag_LITREG;
	struct TAG_ntdef *tag_LITINS;

	struct TAG_ntdef *tag_SEGMENTMARK;

	struct TAG_ntdef *tag_MACRODEF;

	struct TAG_ntdef *tag_CONST;
	struct TAG_ntdef *tag_CONST16;

	struct TAG_ntdef *tag_TEXTSEG;
	struct TAG_ntdef *tag_DATASEG;
	struct TAG_ntdef *tag_EEPROMSEG;

	struct TAG_ntdef *tag_ORG;
	struct TAG_ntdef *tag_EQU;
	struct TAG_ntdef *tag_DEF;

	struct TAG_ntdef *tag_UMINUS;
	struct TAG_ntdef *tag_ADD;
	struct TAG_ntdef *tag_SUB;
	struct TAG_ntdef *tag_MUL;
	struct TAG_ntdef *tag_DIV;
	struct TAG_ntdef *tag_REM;
	struct TAG_ntdef *tag_BITAND;
	struct TAG_ntdef *tag_BITOR;
	struct TAG_ntdef *tag_BITXOR;
	struct TAG_ntdef *tag_BITNOT;

	struct TAG_ntdef *tag_GLABELDEF;
	struct TAG_ntdef *tag_LLABELDEF;

	struct TAG_ntdef *tag_GLABEL;
	struct TAG_ntdef *tag_LLABEL;
	struct TAG_ntdef *tag_EQUNAME;
	struct TAG_ntdef *tag_MACRONAME;

	struct TAG_ntdef *tag_INSTR;
} avrasm_pset_t;

extern avrasm_pset_t avrasm;

extern void avrasm_isetindent (FILE *stream, int indent);	/* avrasm_parser.c */
extern struct TAG_langdef *avrasm_getlangdef (void);

extern int avrasm_subequ_subtree (struct TAG_tnode **tptr);

extern struct TAG_feunit avrasm_program_feunit;			/* avrasm_program.c */


#endif	/* !__AVRASM_H */

