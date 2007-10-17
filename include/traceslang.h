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


struct TAG_tndef;
struct TAG_ntdef;
struct TAG_tnode;

typedef struct {

} traceslang_pset_t;

extern traceslang_pset_t traceslang;

extern void traceslang_isetindent (FILE *stream, int indent);
extern struct TAG_langdef *traceslang_getlangdef (void);

extern struct TAG_feunit traceslang_expr_feunit;	/* traceslang_expr.c */


#endif	/* !__TRACESLANG_H */

