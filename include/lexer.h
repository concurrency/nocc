/*
 *	lexer.h -- interface to the lexer
 *	Copyright (C) 2004 Fred Barnes <frmb@kent.ac.uk>
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

#ifndef __LEXER_H
#define __LEXER_H

typedef enum {
	NOTOKEN = 0,
	KEYWORD = 1,
	INTEGER = 2,
	REAL = 3,
	STRING = 4,
	NAME = 5,
	SYMBOL = 6,
	COMMENT = 7,
	NEWLINE = 8,
	INDENT = 9,
	OUTDENT = 10,
	INAME = 11,
	END = 12
} tokentype_t;

struct TAG_keyword;
struct TAG_symbol;
struct TAG_langlexer;
struct TAG_langparser;


typedef struct TAG_lexfile {
	char *filename;
	char *fnptr;		/* pointer into the above */
	void *priv;		/* private data for lexer */
	void *ppriv;		/* private data for parser */
	int lineno;
	struct TAG_langlexer *lexer;
	struct TAG_langparser *parser;
	int errcount;
	int warncount;
	DYNARRAY (struct TAG_token *, tokbuffer);
} lexfile_t;


extern void lexer_init (void);
extern lexfile_t *lexer_open (char *filename);
extern void lexer_close (lexfile_t *lf);
extern void lexer_warning (lexfile_t *lf, char *fmt, ...);
extern void lexer_error (lexfile_t *lf, char *fmt, ...);

typedef struct TAG_token {
	tokentype_t type;
	lexfile_t *origin;
	int lineno;
	union {
		struct TAG_keyword *kw;
		int ival;
		double dval;
		struct {
			char *ptr;
			int len;
		} str;
		char *name;
		struct TAG_symbol *sym;
	} u;
} token_t;

extern token_t *lexer_nexttoken (lexfile_t *lf);
extern void lexer_pushback (lexfile_t *lf, token_t *tok);

extern token_t *lexer_newtoken (tokentype_t type, ...);
extern void lexer_dumptoken (FILE *stream, token_t *tok);
extern void lexer_dumptoken_short (FILE *stream, token_t *tok);
extern void lexer_freetoken (token_t *tok);

extern int lexer_tokmatch (token_t *formal, token_t *actual);


#endif	/* !__LEXER_H */

