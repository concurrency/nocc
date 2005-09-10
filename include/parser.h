/*
 *	parser.h -- parser interface
 *	Copyright (C) 2004-2005 Fred Barnes <frmb@kent.ac.uk>
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

#ifndef __PARSER_H
#define __PARSER_H

struct TAG_lexfile;
struct TAG_tnode;
struct TAG_token;
struct TAG_parsepriv;

extern void parser_init (void);
extern void parser_shutdown (void);

extern struct TAG_parsepriv *parser_newparsepriv (void);
extern void parser_freeparsepriv (struct TAG_parsepriv *pp);
extern struct TAG_token *parser_peektok (struct TAG_parsepriv *pp);
extern struct TAG_token *parser_gettok (struct TAG_parsepriv *pp);
extern void parser_pushtok (struct TAG_parsepriv *pp, struct TAG_token *tok);

extern struct TAG_tnode *parser_parse (struct TAG_lexfile *lf);
extern char *parser_langname (struct TAG_lexfile *lf);

extern struct TAG_tnode *parser_newlistnode (struct TAG_lexfile *lf);
extern struct TAG_tnode **parser_addtolist (struct TAG_tnode *list, struct TAG_tnode *item);
extern struct TAG_tnode **parser_addtolist_front (struct TAG_tnode *list, struct TAG_tnode *item);
extern int parser_islistnode (struct TAG_tnode *node);
extern struct TAG_tnode **parser_getlistitems (struct TAG_tnode *list, int *nitems);

extern void parser_warning (struct TAG_lexfile *lf, const char *fmt, ...);
extern void parser_error (struct TAG_lexfile *lf, const char *fmt, ...);


#endif	/*! __PARSER_H */

