/*
 *	lexpriv.h -- private definitions for lexer components
 *	Copyright (C) 2004-2016 Fred Barnes <frmb@kent.ac.uk>
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

#ifndef __LEXPRIV_H
#define __LEXPRIV_H

struct TAG_langparser;
struct TAG_origin;

struct TAG_fhandle;

/* this holds the data being lexed */
typedef struct TAG_lexpriv {
	struct TAG_fhandle *fhan;
	int offset;
	int size;
	char *buffer;
	void *langpriv;
} lexpriv_t;

#define LANGTAG_OCCAMPI		0x00000001
#define LANGTAG_HOPP		0x00000002
#define LANGTAG_MCSP		0x00000004
#define LANGTAG_RCXB		0x00000008
#define LANGTAG_TRLANG		0x00000010
#define LANGTAG_TRACESLANG	0x00000020
#define LANGTAG_GUPPY		0x00000040
#define LANGTAG_EAC		0x00000080
#define LANGTAG_AVRASM		0x00000100
#define LANGTAG_OIL		0x00000200

#define LANGTAG_LANGMASK	0x0000ffff
#define LANGTAG_IMASK		0xffff0000


/* this defines support for a language */
typedef struct TAG_langlexer {
	char *langname;			/* "occam-pi" */
	unsigned int langtag;		/* bitfield */

	int (*openfile)(lexfile_t *, lexpriv_t *);
	int (*closefile)(lexfile_t *, lexpriv_t *);
	token_t *(*nexttoken)(lexfile_t *, lexpriv_t *);
	int (*getcodeline)(lexfile_t *, lexpriv_t *, char **);		/* returned buffer */

	/* special token handling */
	void (*freelspecial)(lexfile_t *, void *);

	struct TAG_langparser *parser;		/* associated parser */
	char *fileexts[];	/* {".occ", ".inc", NULL} */
} langlexer_t;


/* language registration */
extern int lexer_registerlang (langlexer_t *ll);
extern int lexer_unregisterlang (langlexer_t *ll);

extern langlexer_t **lexer_getlanguages (int *nlangs);


#endif	/* !__LEXPRIV_H */

