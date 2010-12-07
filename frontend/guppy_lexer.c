/*
 *	guppy_lexer.c -- lexer for Guppy
 *	Copyright (C) 2010 Fred Barnes <frmb@kent.ac.uk>
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

/*{{{  includes*/
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "nocc.h"
#include "support.h"
#include "version.h"
#include "symbols.h"
#include "keywords.h"
#include "lexer.h"
#include "lexpriv.h"
#include "guppy.h"
#include "opts.h"

/*}}}*/

/*{{{  forward decls.*/

static int guppy_openfile (lexfile_t *lf, lexpriv_t *lp);
static int guppy_closefile (lexfile_t *lf, lexpriv_t *lp);
static token_t *guppy_nexttoken (lexfile_t *lf, lexpriv_t *lp);
static int guppy_getcodeline (lexfile_t *lf, lexpriv_t *lp, char **rbuf);


/*}}}*/
/*{{{  public lexer struct*/
langlexer_t guppy_lexer = {
	langname: "guppy",
	langtag: LANGTAG_GUPPY,
	fileexts: {".gpp", "gpi", NULL},
	openfile: guppy_openfile,
	closefile: guppy_closefile,
	nexttoken: guppy_nexttoken,
	getcodeline: guppy_getcodeline,
	parser: NULL
};

/*}}}*/
/*{{{  private lexer struct*/
typedef struct TAG_guppy_lex {
	DYNARRAY (int, indent_offsets);		/* history of where indentations occur */
	int newlineflag;
	int oldnewline;

} guppy_lex_t;

/*}}}*/
/*{{{  private types & data*/


/*}}}*/


/*{{{  static int check_hex (char *ptr, int len)*/
/*
 *	checks that a hex-value is valid (as in digits)
 *	returns 0 if ok, non-zero otherwise
 */
static int check_hex (char *ptr, int len)
{
	for (; len; len--, ptr++) {
		if (((*ptr >= 'a') && (*ptr <= 'f')) ||
				((*ptr >= 'A') && (*ptr <= 'F')) ||
				((*ptr >= '0') && (*ptr <= '9'))) {
			/* skip */
		} else {
			return 1;
		}
	}
	return 0;
}
/*}}}*/
/*{{{  static int decode_hex (char *ptr, int len)*/
/*
 *	decodes a hex value.  returns it.
 */
static int decode_hex (char *ptr, int len)
{
	int val = 0;

	for (; len; len--, ptr++) {
		val <<= 4;
		if ((*ptr >= 'a') && (*ptr <= 'f')) {
			val |= ((*ptr - 'a') + 10);
		} else if ((*ptr >= 'A') && (*ptr <= 'F')) {
			val |= ((*ptr - 'A') + 10);
		} else {
			val |= (*ptr - '0');
		}
	}
	return val;
}
/*}}}*/
/*{{{  static int guppy_escape_char (lexfile_t *lf, guppy_lex_t *lop, char **ptr)*/
/*
 *	extracts an escape sequence from a string
 *	returns the character it represents, -255 on error
 */
static int guppy_escape_char (lexfile_t *lf, guppy_lex_t *lop, char **ptr)
{
	int echr = 0;

	if (**ptr != '\\') {
		lexer_error (lf, "guppy_escape_char(): called incorrectly");
		(*ptr)++;
		goto out_error1;
	} else {
		(*ptr)++;
		switch (**ptr) {
		case 'n':
			echr = (int)'\n';
			(*ptr)++;
			break;
		case 'r':
			echr = (int)'\r';
			(*ptr)++;
			break;
		case '\\':
			echr = (int)'\\';
			(*ptr)++;
			break;
		case '\'':
			echr = (int)'\'';
			(*ptr)++;
			break;
		case '\"':
			echr = (int)'\"';
			(*ptr)++;
			break;
		case 'x':
			if (check_hex (*ptr + 1, 2)) {
				lexer_error (lf, "malformed hexadecimal escape in character constant");
				goto out_error1;
			}
			echr = decode_hex (*ptr + 1, 2);
			(*ptr) += 3;
			break;
		default:
			lexer_error (lf, "unknown escape character \'%c\'", **ptr);
			(*ptr)++;
			goto out_error1;
		}
	}
	return echr;
out_error1:
	return -255;
}
/*}}}*/


/*{{{  static int guppy_openfile (lexfile_t *lf, lexpriv_t *lp)*/
/*
 *	called once a guppy source file has been opened
 */
static int guppy_openfile (lexfile_t *lf, lexpriv_t *lp)
{
	guppy_lex_t *lop;

	lop = (guppy_lex_t *)smalloc (sizeof (guppy_lex_t));
	dynarray_init (lop->indent_offsets);
	lop->newlineflag = 1;
	lop->oldnewline = 0;

	lp->langpriv = (void *)lop;
	lf->lineno = 1;
	return 0;
}
/*}}}*/
/*{{{  static int guppy_closefile (lexfile_t *lf, lexpriv_t *lp)*/
/*
 *	called before a guppy source file is closed
 */
static int guppy_closefile (lexfile_t *lf, lexpriv_t *lp)
{
	guppy_lex_t *lop = (guppy_lex_t *)(lp->langpriv);

	if (!lop) {
		nocc_internal ("guppy_closefile() not open! (%s)", lf->filename);
		return -1;
	}
	sfree (lop);
	lp->langpriv = NULL;

	return 0;
}
/*}}}*/
/*{{{  */
/*
 *	called to retrieve the next token.
 */
static token_t *guppy_nexttoken (lexfile_t *lf, lexpriv_t *lp)
{
	guppy_lex_t *lop = (guppy_lex_t *)(lp->langpriv);
	token_t *tok = NULL;
	char *ch, *chlim;
	char *dh;

	if (!lf || !lp || !lop) {
		return NULL;
	}

	tok = lexer_newtoken (NOTOKEN);
	tok->origin = (void *)lf;
	tok->lineno = lf->lineno;

	/* FIXME: missing stuff */
	return tok;
}
/*}}}*/
/*{{{  static int guppy_getcodeline (lexfile_t *lf, lexpriv_t *lp, char **rbuf)*/
/*
 *	gets the current line of code from the input file and puts it in a new buffer (pointer returned in '*rbuf')
 *	returns 0 on success, non-zero on failure
 */
static int guppy_getcodeline (lexfile_t *lf, lexpriv_t *lp, char **rbuf)
{
	guppy_lex_t *lop;
	char *chstart, *chend;

	if (!lp || !lp->langpriv) {
		return -1;
	}
	if (!lp->buffer) {
		return -1;
	}

	lop = (guppy_lex_t *)(lp->langpriv);
	if (lp->offset == lp->size) {
		chend = lp->buffer + lp->offset;
		for (chstart = chend; (chstart > lp->buffer) && (chstart[-1] != '\n') && (chstart[-1] != '\r'); chstart--);
	} else {
		for (chend = lp->buffer + lp->offset; (chend < (lp->buffer + lp->size)) && (*chend != '\n') && (*chend != '\r'); chend++);
		for (chstart = lp->buffer + lp->offset; (chstart > lp->buffer) && (chstart[-1] != '\n') && (chstart[-1] != '\r'); chstart--);
	}
	*rbuf = string_ndup (chstart, (int)(chend - chstart));

	return 0;
}
/*}}}*/



