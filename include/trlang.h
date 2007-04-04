/*
 *	trlang.h -- tree-rewriting language for NOCC
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

#ifndef __TRLANG_H
#define __TRLANG_H

struct TAG_langlexer;
struct TAG_langparser;

extern struct TAG_langlexer trlang_lexer;
extern struct TAG_langparser trlang_parser;


extern int trlang_init (void);
extern int trlang_shutdown (void);

extern int trlang_initialise (void);


struct TAG_tndef;
struct TAG_ntdef;
struct TAG_token;

typedef struct {
	struct TAG_ntdef *tag_NAME;
	struct TAG_ntdef *tag_LITINT;
	struct TAG_ntdef *tag_LITSTR;
	struct TAG_ntdef *tag_FUNCTIONDEF;
	struct TAG_ntdef *tag_IF;
	struct TAG_ntdef *tag_ELSE;
} trlang_pset_t;

extern trlang_pset_t trlang;

extern void trlang_isetindent (FILE *stream, int indent);
extern struct TAG_langdef *trlang_getlangdef (void);

extern struct TAG_feunit trlang_expr_feunit;		/* trlang_expr.c */


#endif	/* !__TRLANG_H */

