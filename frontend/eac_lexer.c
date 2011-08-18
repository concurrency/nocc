/*
 *	eac_lexer.c -- EAC lexer for NOCC
 *	Copyright (C) 2011 Fred Barnes <frmb@kent.ac.uk>
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
#include "eac.h"
#include "opts.h"


/*}}}*/
/*{{{  forward decls.*/

static int eac_openfile (lexfile_t *lf, lexpriv_t *lp);
static int eac_closefile (lexfile_t *lf, lexpriv_t *lp);
static token_t *eac_nexttoken (lexfile_t *lf, lexpriv_t *lp);
static int eac_getcodeline (lexfile_t *lf, lexpriv_t *lp, char **rbuf);


/*}}}*/
/*{{{  public lexer struct (eac_lexer)*/
langlexer_t eac_lexer = {
	.langname = "eac",
	.langtag = LANGTAG_EAC,
	.fileexts = {".eac", NULL},
	.openfile = eac_openfile,
	.closefile = eac_closefile,
	.nexttoken = eac_nexttoken,
	.getcodeline = eac_getcodeline,
	.parser = NULL
};

/*}}}*/
/*{{{  private lexer struct (eac_lex_t)*/
typedef struct TAG_eac_lex {
	
} eac_lex_t;

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
/*{{{  static int eac_escape_char (lexfile_t *lf, eac_lex_t *lop, char **ptr)*/
/*
 *	extracts an escape sequence from a string
 *	returns the character it represents, -255 on error
 */
static int eac_escape_char (lexfile_t *lf, eac_lex_t *lop, char **ptr)
{
	int echr = 0;

	if (**ptr != '\\') {
		lexer_error (lf, "eac_escape_char(): called incorrectly");
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


/*{{{  static int eac_openfile (lexfile_t *lf, lexpriv_t *lp)*/
/*
 *	called once an EAC source file has been opened
 *	return 0 on success, non-zero on failure
 */
static int eac_openfile (lexfile_t *lf, lexpriv_t *lp)
{
	eac_lex_t *lop;

	lop = (eac_lex_t *)smalloc (sizeof (eac_lex_t));
	
	lp->langpriv = (void *)lop;
	lf->lineno = 1;

	return 0;
}
/*}}}*/
/*{{{  static int eac_closefile (lexfile_t *lf, lexpriv_t *lp)*/
/*
 *	called before an EAC source file is closed
 *	returns 0 on success, non-zero on failure
 */
static int eac_closefile (lexfile_t *lf, lexpriv_t *lp)
{
	eac_lex_t *lop = (eac_lex_t *)(lp->langpriv);

	if (!lop) {
		nocc_internal ("eac_closefile() not open! (%s)", lf->filename);
		return -1;
	}
	sfree (lop);
	lp->langpriv = NULL;

	return 0;
}
/*}}}*/
/*{{{  static token_t *eac_nexttoken (lexfile_t *lf, lexpriv_t *lp)*/
/*
 *	called to retrieve the next token.
 *	return token or NULL on error.
 */
static token_t *eac_nexttoken (lexfile_t *lf, lexpriv_t *lp)
{
	eac_lex_t *lop = (eac_lex_t *)(lp->langpriv);
	token_t *tok = NULL;
	char *ch, *chlim;
	char *dh;

	if (!lf || !lp || !lop) {
		return NULL;
	}

	tok = lexer_newtoken (NOTOKEN);
	tok->origin = (void *)lf;
	tok->lineno = lf->lineno;

tokenloop:
	if (lp->offset == lp->size) {
		/* reached EOF */
		tok->type = END;
		goto out_tok;
	}

	/* make a guess as to what's next */
	ch = lp->buffer + lp->offset;
	chlim = lp->buffer + lp->size;

	/* decode next token */
	if (((*ch >= 'a') && (*ch <= 'z')) || ((*ch >= 'A') && (*ch <= 'Z'))) {
		/*{{{  probably a name/keyword*/
		keyword_t *kw;
		char *tmpstr;

		/* scan for something that matches a word */
		for (dh=ch+1; (dh < chlim) && (((*dh >= 'a') && (*dh <= 'z')) ||
				((*dh >= 'A') && (*dh <= 'Z')) ||
				(*dh == '.') ||
				((*dh >= '0') && (*dh <= '9'))); dh++);

		tmpstr = string_ndup (ch, (int)(dh - ch));
		kw = keywords_lookup (tmpstr, (int)(dh - ch), LANGTAG_EAC);
		sfree (tmpstr);

		if (!kw) {
			/* assume name */
			tok->type = NAME;
			tok->u.name = string_ndup (ch, (int)(dh - ch));
		} else {
			/* is keyword */
			tok->type = KEYWORD;
			tok->u.kw = kw;
		}
		lp->offset += (int)(dh - ch);
		/*}}}*/
	} else if ((*ch >= '0') && (*ch <= '9')) {
		/*{{{  number of sorts*/
		char *npbuf = NULL;

		tok->type = INTEGER;
		for (dh=ch+1; (dh < chlim) && ((*dh >= '0') && (*dh <= '9')); dh++);
		lp->offset += (int)(dh - ch);

		/* parse it */
		npbuf = string_ndup (ch, (int)(dh - ch));
		if (sscanf (npbuf, "%d", &tok->u.ival) != 1) {
			lexer_error (lf, "malformed integer constant: %s", npbuf);
			sfree (npbuf);
			goto out_error1;
		} else {
			sfree (npbuf);
		}
		/*}}}*/
	} else switch (*ch) {
		/*{{{  \r, \n (newline)*/
	case '\r':
		lp->offset++;
		goto tokenloop;
	case '\n':
		lf->lineno++;
		lp->offset++;
		tok->type = NEWLINE;

		/* and skip multiple blank lines */
		for (dh = ++ch; (dh < chlim) && ((*dh == '\n') || (*dh == '\r')); dh++) {
			if (*dh == '\n') {
				lf->lineno++;
			}
		}

		/* and because whitespace isn't interesting, eat that up too */
		for (; (dh < chlim) && ((*dh == ' ') || (*dh == '\t')); dh++);

		lp->offset += (int)(dh - ch);
		goto out_tok;
		/*}}}*/
		/*{{{  space, tab (unexpected)*/
	case ' ':
	case '\t':
		/* shouldn't see this */
		lexer_warning (lf, "unexpected whitespace");
		lp->offset++;
		goto tokenloop;
		/*}}}*/
		/*{{{  ' (character)*/
	case '\'':
		/* return as integer */
		{
			char *dh = ch;
			int eschar;

			tok->type = INTEGER;
			dh++;
			if (*dh == '\\') {
				/* escape character */
				eschar = eac_escape_char (lf, lop, &dh);
				if (eschar == -255) {
					goto out_error1;
				}
				tok->u.ival = eschar;
			} else {
				/* regular character */
				tok->u.ival = (int)(*dh);
				dh++;
			}
			/* expect closing quote */
			if (*dh != '\'') {
				lexer_error (lf, "malformed character constant");
				goto out_error1;
			}
			dh++;

			lp->offset += (int)(dh - ch);
		}
		break;
		/*}}}*/
		/*{{{  " (string)*/
	case '\"':
		tok->type = STRING;

		/* scan string */
		{
			int slen = 0;
			char *xch;

			/*{{{  scan string to end*/
			for (dh = ch + 1; (dh < chlim) && (*dh != '\"'); dh++, slen++) {
				switch (*dh) {
				case '\\':
					/* escape char */
					dh++;
					if (dh == chlim) {
						lexer_error (lf, "unexpected end of file in string");
						goto out_error1;
					}
					switch (*dh) {
					case ' ':
					case '\t':
					case '\r':
					case '\n':
						/* string continuation (onto next line) */
						/* FIXME! */
						break;
					case 'n':
					case 'r':
					case 't':
					case '\'':
					case '\"':
						break;
					case 'x':
						/* eat up 2 hex chars */
						if ((dh + 2) >= chlim) {
							lexer_error (lf, "bad hexadecimal constant");
							goto out_error1;
						}
						if (check_hex (dh + 1, 2)) {
							lexer_error (lf, "bad hexadecimal value");
							goto out_error1;
						}
						dh += 2;
						break;
					default:
						lexer_error (lf, "unhandled escape: \\%c", *dh);
						goto out_error1;
					}
					break;
				}
			}
			/*}}}*/
			if (dh == chlim) {
				lexer_error (lf, "unexpected end of file in string");
				goto out_error1;
			}

			tok->u.str.ptr = (char *)smalloc (slen + 1);
			tok->u.str.len = 0;
			xch = tok->u.str.ptr;
			slen = 0;

			/*{{{  now actually process*/
			for (dh = (ch + 1); (dh < chlim) && (*dh != '\"'); dh++) {
				switch (*dh) {
				case '\\':
					/*{{{  escape char*/
					dh++;
					switch (*dh) {
					case ' ':
					case '\t':
					case '\n':
					case '\r':
						/* string continuation (onto next line) */
						/* FIXME! */
						break;
					case 'n':
						*(xch++) = '\n';
						slen++;
						break;
					case 'r':
						*(xch++) = '\r';
						slen++;
						break;
					case '\'':
						*(xch++) = '\'';
						slen++;
						break;
					case '\"':
						*(xch++) = '\"';
						slen++;
						break;
					case 't':
						*(xch++) = '\t';
						slen++;
						break;
					case '\\':
						*(xch++) = '\\';
						slen++;
						break;
					case 'x':
						*(xch++) = (char)decode_hex (dh + 1, 2);
						slen++;
						dh += 2;
						break;
					}
					/*}}}*/
					break;
				default:
					*(xch++) = *dh;
					slen++;
					break;
				}
			}
			/*}}}*/
			
			dh++;
			*xch = '\0';
			tok->u.str.len = slen;
			lp->offset += (int)(dh - ch);
		}
		break;
		/*}}}*/
		/*{{{  # (comment)*/
	case '#':
		tok->type = COMMENT;
		/* scane to EOL */
		for (dh = ch+1; (dh < chlim) && (*dh != '\n') && (*dh != '\r'); dh++);
		lp->offset += (int)(dh - ch);
		break;
		/*}}}*/
		/*{{{  default (symbol)*/
	default:
		/* try and match symbol */
default_label:
		{
			symbol_t *sym = symbols_match (ch, chlim, LANGTAG_EAC);

#if 0
nocc_message ("in eac_nexttoken/default: ch = \'%c\', sym = %p", *ch, sym);
#endif
			if (sym) {
				/* found something */
				tok->type = SYMBOL;
				tok->u.sym = sym;
				lp->offset += sym->mlen;
			} else {
				/* unknown.. */
				char *tmpstr = string_ndup (ch, ((int)(chlim - ch) < 2) ? 1 : 2);

				lexer_error (lf, "tokeniser error at [%s]", tmpstr);
				sfree (tmpstr);
				goto out_error1;
			}
		}
		break;
		/*}}}*/
	}

	/* eat up any remaining whitespace */
	ch = lp->buffer + lp->offset;
	for (dh = ch; (ch < chlim) && ((*dh == ' ') || (*dh == '\t')); dh++);
	lp->offset += (int)(dh - ch);
out_tok:
	return tok;

out_error1:
	/* skip to end of line or file */
	tok->type = NOTOKEN;
	ch = lp->buffer + lp->offset;
	for (dh = ch; (dh < chlim) && (*dh != '\n'); dh++);
	lp->offset += (int)(dh - ch);

	return tok;
}
/*}}}*/
/*{{{  static int eac_getcodeline (lexfile_t *lf, lexpriv_t *lp, char **rbuf)*/
/*
 *	gets the current line of code from the input file and puts it in a new buffer (pointer returned in '*rbuf')
 *	returns 0 on success, non-zero on failure
 */
static int eac_getcodeline (lexfile_t *lf, lexpriv_t *lp, char **rbuf)
{
	eac_lex_t *lop;
	char *chstart, *chend;

	if (!lp || !lp->langpriv) {
		return -1;
	}
	if (!lp->buffer) {
		return -1;
	}

	lop = (eac_lex_t *)(lp->langpriv);
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


