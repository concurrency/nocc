/*
 *	parser.h -- parser interface
 *	Copyright (C) 2004-2016 Fred Barnes, University of Kent <frmb@kent.ac.uk>
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

struct TAG_srclocn;
struct TAG_lexfile;
struct TAG_tnode;
struct TAG_token;
struct TAG_parsepriv;
struct TAG_ntdef;

extern int parser_init (void);
extern int parser_shutdown (void);

extern struct TAG_parsepriv *parser_newparsepriv (void);
extern void parser_freeparsepriv (struct TAG_parsepriv *pp);
extern struct TAG_token *parser_peektok (struct TAG_parsepriv *pp);
extern struct TAG_token *parser_peekemptytok (struct TAG_parsepriv *pp);
extern struct TAG_token *parser_gettok (struct TAG_parsepriv *pp);
extern void parser_pushtok (struct TAG_parsepriv *pp, struct TAG_token *tok);

extern struct TAG_tnode *parser_parse (struct TAG_lexfile *lf);
extern int parser_initandfcn (struct TAG_lexfile *lf, void (*fcn)(struct TAG_lexfile *, void *), void *arg);
extern struct TAG_tnode *parser_descparse (struct TAG_lexfile *lf);
extern char *parser_langname (struct TAG_lexfile *lf);

extern int parser_gettesttags (struct TAG_ntdef **truep, struct TAG_ntdef **falsep);

extern struct TAG_tnode *parser_newlistnode (struct TAG_srclocn *src);
extern struct TAG_tnode *parser_buildlistnode (struct TAG_srclocn *src, ...);
extern struct TAG_tnode *parser_makelistnode (struct TAG_tnode *node);
extern struct TAG_tnode **parser_addtolist (struct TAG_tnode *list, struct TAG_tnode *item);
extern struct TAG_tnode **parser_addtolist_front (struct TAG_tnode *list, struct TAG_tnode *item);
extern struct TAG_tnode *parser_delfromlist (struct TAG_tnode *list, int idx);
extern struct TAG_tnode *parser_getfromlist (struct TAG_tnode *list, int idx);
extern struct TAG_tnode **parser_getfromlistptr (struct TAG_tnode *list, int idx);
extern void parser_insertinlist (struct TAG_tnode *list, struct TAG_tnode *item, int idx);
extern void parser_mergeinlist (struct TAG_tnode *list, struct TAG_tnode *sublist, int idx);
extern struct TAG_tnode *parser_rmfromlist (struct TAG_tnode *list, struct TAG_tnode *item);
extern int parser_islistnode (struct TAG_tnode *node);
extern struct TAG_tnode **parser_getlistitems (struct TAG_tnode *list, int *nitems);
extern int parser_cleanuplist (struct TAG_tnode *list);
extern void parser_inlistfixup (void **tos);
extern void parser_sortlist (struct TAG_tnode *list, int (*cmpfcn)(struct TAG_tnode *, struct TAG_tnode *));
extern void parser_trashlist (struct TAG_tnode *list);
extern int parser_countlist (struct TAG_tnode *list);
extern struct TAG_tnode *parser_ensurelist (struct TAG_tnode **nodeptr, struct TAG_tnode *orgref);
extern int parser_collapselist (struct TAG_tnode *list);

extern void parser_warning (struct TAG_srclocn *src, const char *fmt, ...) __attribute__ ((format (printf, 2, 3)));
extern void parser_error (struct TAG_srclocn *src, const char *fmt, ...) __attribute__ ((format (printf, 2, 3)));

extern int parser_markerror (struct TAG_lexfile *lf);
extern int parser_checkerror (struct TAG_lexfile *lf, const int mark);


#endif	/*! __PARSER_H */

