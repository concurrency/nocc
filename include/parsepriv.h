/*
 *	parsepriv.h -- private parser interface
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

#ifndef __PARSEPRIV_H
#define __PARSEPRIV_H


struct TAG_lexfile;
struct TAG_tnode;
struct TAG_token;
struct TAG_dfastate;
struct TAG_prescope;
struct TAG_scope;
struct TAG_typecheck;
struct TAG_fetrans;
struct TAG_langlexer;

typedef struct TAG_langparser {
	char *langname;
	int (*init)(struct TAG_lexfile *);
	void (*shutdown)(struct TAG_lexfile *);

	struct TAG_tnode *(*parse)(struct TAG_lexfile *);
	struct TAG_tnode *(*descparse)(struct TAG_lexfile *);
	int (*scope)(struct TAG_tnode **, struct TAG_scope *);
	int (*prescope)(struct TAG_tnode **, struct TAG_prescope *);
	int (*typecheck)(struct TAG_tnode *, struct TAG_typecheck *);
	int (*fetrans)(struct TAG_tnode **, struct TAG_fetrans *);

	struct TAG_tnode *(*maketemp)(struct TAG_tnode ***, struct TAG_tnode *);						/* insert-point-ptr, type */
	struct TAG_tnode *(*makeseqassign)(struct TAG_tnode ***, struct TAG_tnode *, struct TAG_tnode *, struct TAG_tnode *);	/* insert-point-ptr, lhs, rhs, type */

	void *tagstruct_hook;			/* where language can attach its tndef/ntdef/token structure */
	struct TAG_langlexer *lexer;		/* lexer for this language */
} langparser_t;

/* this is private to the parser */
typedef struct TAG_parsepriv {
	void *lhook;					/* for language-specific state */
	struct TAG_lexfile *lf;				/* lexfile */

	DYNARRAY (struct TAG_token *, tokstack);	/* token-stack in parser */
} parsepriv_t;

extern int parser_register_reduce (const char *name, void (*reduce)(struct TAG_dfastate *, parsepriv_t *, void *), void *rarg);
extern void (*parser_lookup_reduce (const char *name))(struct TAG_dfastate *, parsepriv_t *, void *);
extern void *parser_lookup_rarg (const char *name);

extern void parser_generic_reduce (struct TAG_dfastate *dfast, parsepriv_t *pp, void *rarg);
extern void *parser_decode_grule (const char *rule, ...);
extern void parser_free_grule (void *rarg);
extern void parser_dumpgrules (FILE *stream);

extern int parser_register_grule (const char *name, void *grule);
extern void *parser_lookup_grule (const char *name);
extern char *parser_nameof_reducer (void *reducefcn, void *reducearg);

extern void parser_inlistreduce (struct TAG_dfastate *dfast, parsepriv_t *pp, void *rarg);


#endif	/* !__PARSEPRIV_H */

