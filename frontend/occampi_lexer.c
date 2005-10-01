/*
 *	occampi_lexer.c -- lexer for occam-pi
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
#include "occampi.h"


/*{{{  forward decls.*/
static int occampi_openfile (lexfile_t *lf, lexpriv_t *lp);
static int occampi_closefile (lexfile_t *lf, lexpriv_t *lp);
static token_t *occampi_nexttoken (lexfile_t *lf, lexpriv_t *lp);


/*}}}*/
/*{{{  public lexer struct*/
langlexer_t occampi_lexer = {
	langname: "occam-pi",
	fileexts: {".occ", ".inc", NULL},
	openfile: occampi_openfile,
	closefile: occampi_closefile,
	nexttoken: occampi_nexttoken,
	parser: NULL
};


/*}}}*/
/*{{{  private lexer struct*/
typedef struct TAG_occampi_lex {
	int curindent;			/* current indent */
	int scanto_indent;		/* target indent when it changes */
	int newlineflag;
} occampi_lex_t;


/*}}}*/


/*{{{  static int check_hex (char *ptr, int len)*/
/*
 *	checks that a hex-value is valid (as in digits)
 *	return 0 if ok, non-zero otherwise
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
 *	decodes a hex value.  returns it
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


/*{{{  static int occampi_openfile (lexfile_t *lf, lexpriv_t *lp)*/
/*
 *	called once an occam-pi source file has been opened
 */
static int occampi_openfile (lexfile_t *lf, lexpriv_t *lp)
{
	occampi_lex_t *lop;

	/* FIXME: check that this is a valid occam-pi source file somehow ? */
	lop = (occampi_lex_t *)smalloc (sizeof (occampi_lex_t));
	lop->curindent = 0;
	lop->scanto_indent = 0;
	lop->newlineflag = 1;		/* effectively get a newline straight-away */
	
	lp->langpriv = (void *)lop;
	lf->lineno = 1;
	return 0;
}
/*}}}*/
/*{{{  static int occampi_closefile (lexfile_t *lf, lexpriv_t *lp)*/
/*
 *	called before an occam-pi source file is closed
 */
static int occampi_closefile (lexfile_t *lf, lexpriv_t *lp)
{
	occampi_lex_t *lop = (occampi_lex_t *)(lp->langpriv);

	if (!lop) {
		nocc_internal ("occampi_closefile() not open! (%s)", lf->filename);
		return -1;
	}
	sfree (lop);
	lp->langpriv = NULL;

	return 0;
}
/*}}}*/
/*{{{  static token_t *occampi_nexttoken (lexfile_t *lf, lexpriv_t *lp)*/
/*
 *	called to retrieve the next token.
 *	Yes, I know goto is horrible, but it avoids some ugliness here..
 */
static token_t *occampi_nexttoken (lexfile_t *lf, lexpriv_t *lp)
{
	occampi_lex_t *lop = (occampi_lex_t *)(lp->langpriv);
	token_t *tok = NULL;
	char *ch, *chlim;
	char *dh;

	if (!lf || !lp) {
		return NULL;
	}

	tok = (token_t *)smalloc (sizeof (token_t));
	tok->type = NOTOKEN;
	tok->origin = (void *)lf;
	tok->lineno = lf->lineno;
	if (lp->offset == lp->size) {
		/* reached EOF */
		tok->type = END;
		return tok;
	}

	/* make some sort of guess on what we're dealing with */
tokenloop:
	ch = lp->buffer + lp->offset;
	chlim = lp->buffer + lp->size;

	if (lop->newlineflag) {
		/*{{{  had a new line recently, check for indent/outdent*/
		int thisindent = 0;

		/* measure indentation */
		for (dh=ch; (dh < chlim) && ((*dh == ' ') || (*dh == '\t')); dh++) {
			if (*dh == ' ') {
				thisindent++;
			} else {
				/* tabs are 8 spaces.. :) -- and align to the next tab-stop */
				thisindent = ((thisindent >> 3) + 1) << 3;
			}
		}
		lp->offset += (int)(dh - ch);
		lop->newlineflag = 0;
		ch = dh;
		lop->scanto_indent = thisindent;
		/* then scoop this up next */
		/*}}}*/
	}

	/* might still be scanning to a specific indent point */
	if (lop->scanto_indent < lop->curindent) {
		lop->curindent -= 2;
		if (lop->scanto_indent > lop->curindent) {
			/* fix up for odd spacing */
			lop->curindent = lop->scanto_indent;
		}
		tok->type = OUTDENT;
		return tok;
	} else if (lop->scanto_indent > lop->curindent) {
		lop->curindent += 2;
		if (lop->scanto_indent < lop->curindent) {
			/* fix up for odd spacing */
			lop->curindent = lop->scanto_indent;
		}
		tok->type = INDENT;
		return tok;
	}

	if (((*ch >= 'a') && (*ch <= 'z')) || ((*ch >= 'A') && (*ch <= 'Z'))) {
		/*{{{  probably a keyword or name*/
		keyword_t *kw;
		char *tmpstr;
		
		/* scan something that matches a word */
		for (dh=ch+1; (dh < chlim) && (((*dh >= 'a') && (*dh <= 'z')) ||
				((*dh >= 'A') && (*dh <= 'Z')) ||
				(*dh == '.') ||
				((*dh >= '0') && (*dh <= '9'))); dh++);
		
		tmpstr = string_ndup (ch, (int)(dh - ch));
		kw = keywords_lookup (tmpstr, (int)(dh - ch));
		sfree (tmpstr);

		if (!kw) {
			/* assume name */
			tok->type = NAME;
			tok->u.name = string_ndup (ch, (int)(dh - ch));
		} else {
			/* keyword found */
			tok->type = KEYWORD;
			tok->u.kw = kw;
		}
		lp->offset += (int)(dh - ch);
		/*}}}*/
	} else if ((*ch >= '0') && (*ch <= '9')) {
		/*{{{  number of sorts*/
		char *dh;
		char *npbuf = NULL;

		tok->type = INTEGER;
		for (dh=ch+1; (dh < chlim) && (((*dh >= '0') && (*dh <= '9')) || (*dh == '.')); dh++) {
			if (*dh == '.') {
				if (tok->type == REAL) {
					lexer_error (lf, "malformed real constant");
					goto out_error1;
				}
				tok->type = REAL;
			}
		}
		lp->offset += (int)(dh - ch);
		/* parse it */
		npbuf = (char *)smalloc ((int)(dh - ch) + 1);
		memcpy (npbuf, ch, (int)(dh - ch));
		npbuf[(int)(dh - ch)] = '\0';
		if ((tok->type == REAL) && (sscanf (npbuf, "%lf", &tok->u.dval) != 1)) {
			lexer_error (lf, "malformed floating-point constant: %s", npbuf);
			sfree (npbuf);
			goto out_error1;
		} else if ((tok->type == INTEGER) && (sscanf (npbuf, "%d", &tok->u.ival) != 1)) {
			lexer_error (lf, "malformed integer constant: %s", npbuf);
			sfree (npbuf);
			goto out_error1;
		} else {
			sfree (npbuf);
		}
		/*}}}*/
	} else switch (*ch) {
		/*{{{  \r, \n (newline) */
	case '\r':
		lp->offset++;
		goto tokenloop;
	case '\n':
		lf->lineno++;
		lp->offset++;
		tok->type = NEWLINE;
		/* and skip multiple newlines */
		for (dh = ++ch; (dh < chlim) && ((*dh == '\n') || (*dh == '\r')); dh++) {
			if (*dh == '\n') {
				lf->lineno++;
			}
		}
		lp->offset += (int)(dh - ch);
		lop->newlineflag = 1;
		/* return here, don't walk over whitespace */
		return tok;
		break;
		/*}}}*/
		/*{{{  -  (minus or comment)*/
	case '-':
		if ((ch + 1) == chlim) {
			/* strange place to have this */
			goto default_label;
		}
		if (*(ch + 1) == '-') {
			/* start of comment */
			tok->type = COMMENT;
			/* scan to end-of-line */
			for (dh=ch+2; (dh < chlim) && (*dh != '\n') && (*dh != '\r'); dh++);
			lp->offset += (int)(dh - ch);
			/* definitely a comment */
		} else {
			goto default_label;
		}
		break;
		/*}}}*/
		/*{{{  space, tab*/
	case ' ':
	case '\t':
		/* shouldn't see this */
		lexer_warning (lf, "unexpected whitespace");
		lp->offset ++;
		goto tokenloop;
		break;
		/*}}}*/
		/*{{{  ' (character)*/
	case '\'':
		/* return this as an integer */
		{
			char *dh = ch;

			tok->type = INTEGER;
			dh++;
			if (*dh == '*') {
				/* escape character */
				dh++;
				switch (*dh) {
				case 'n':
					tok->u.ival = (int)'\n';
					dh++;
					break;
				case 'c':
					tok->u.ival = (int)'\r';
					dh++;
					break;
				case 't':
					tok->u.ival = (int)'\t';
					dh++;
					break;
				case '*':
				case '\'':
				case '\"':
					tok->u.ival = (int)(*dh);
					dh++;
					break;
				case '#':
					if (check_hex (dh + 1, 2)) {
						lp->offset += 6;
						lexer_error (lf, "malformed hexidecimal escape in character constant");
						goto out_error1;
					}
					tok->u.ival = decode_hex (dh + 1, 2);
					dh += 3;
					break;
				default:
					lexer_error (lf, "unknown escape character \'%c\'", *dh);
					dh += 4;
					goto out_error1;
				}
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

			/*{{{  scan string*/
			for (dh = ch + 1; (dh < chlim) && (*dh != '\"'); dh++, slen++) {
				switch (*dh) {
				case '*':
					/* escape char */
					dh++;
					if (dh == chlim) {
						lexer_error (lf, "unexpected end of file");
						goto out_error1;
					}
					switch (*dh) {
					case ' ':
					case '\t':
					case '\r':
					case '\n':
						/* string-continuation */
						for (; (dh < chlim) && ((*dh == ' ') || (*dh == '\t')); dh++);
						if (dh == chlim) {
							lexer_error (lf, "unexpected end of file");
							goto out_error1;
						}
						if ((*dh == '-') && ((dh + 1) < chlim) && (*(dh+1) == '-')) {
							/* comment to end-of-line */
							for (dh++; (dh < chlim) && (*dh != '\n'); dh++);
							if (dh == chlim) {
								lexer_error (lf, "unexpected end of file");
								goto out_error1;
							}
						}
						if (*dh == '\r') {
							dh++;
							if (dh == chlim) {
								lexer_error (lf, "unexpected end of file");
								goto out_error1;
							}
						}
						if (*dh != '\n') {
							lexer_error (lf, "expected end-of-line, found \'%c\'", *dh);
							goto out_error1;
						}
						lf->lineno++;
						/* set at start of next line now (don't advance dh, for() does that) */
						/* but..  do need to scan and find the next '*' */
						for (dh++; (*dh == ' ') || (*dh == '\t'); dh++);
						if (*dh != '*') {
							lexer_error (lf, "bad string continuation, unexpected \'%c\'", *dh);
						}
						break;
					case 'n':
					case 'c':
					case 't':
					case '\'':
					case '\"':
					case '*':
						break;
					case '#':
						/* eat up 2 hex chars */
						if ((dh + 2) >= chlim) {
							lexer_error (lf, "bad hex constant");
							goto out_error1;
						}
						if (check_hex (dh + 1, 2)) {
							lexer_error (lf, "bad hex value");
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
				lexer_error (lf, "unexpected end of file");
				goto out_error1;
			}
			tok->u.str.ptr = (char *)smalloc (slen + 1);
			tok->u.str.len = 0;		/* fixup in a bit */
			xch = tok->u.str.ptr;
			slen = 0;
			/*{{{  now actually process it*/
			for (dh = (ch + 1); (dh < chlim) && (*dh != '\"'); dh++) {
				switch (*dh) {
				case '*':
					/* escape char */
					dh++;
					switch (*dh) {
					case ' ':
					case '\t':
					case '\n':
					case '\r':
						/* string-continuation */
						for (; (dh < chlim) && ((*dh == ' ') || (*dh == '\t')); dh++);
						if (*dh == '#') {
							/* comment to end-of-line */
							for (dh++; (dh < chlim) && (*dh != '\r') && (*dh != '\n'); dh++);
						}
						if (*dh == '\r') {
							dh++;
						}
						for (dh++; (*dh == ' ') || (*dh == '\t'); dh++);
						break;
					case 'n':
						*(xch++) = '\n';
						slen++;
						break;
					case 'c':
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
					case '*':
						*(xch++) = '*';
						slen++;
						break;
					case '#':
						*(xch++) = (char)decode_hex (dh + 1, 2);
						slen++;
						dh += 2;
						break;
					}
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
		/*{{{  default (symbol)*/
	default:
		/* try and match as a symbol */
default_label:
		{
			symbol_t *sym = symbols_match (ch, chlim);

			if (sym) {
				/* found something */
				tok->type = SYMBOL;
				tok->u.sym = sym;
				lp->offset += sym->mlen;
			} else {
				/* unknown.. */
				char *tmpstr = string_ndup (ch, ((chlim - ch) < 2) ? 1 : 2);

				lexer_error (lf, "tokeniser error [%s]", tmpstr);
				sfree (tmpstr);
				goto out_error1;
			}
		}
		break;
		/*}}}*/
	}

	/* skip any remaining whitespace */
	ch = lp->buffer + lp->offset;
	for (dh = ch; (dh < chlim) && ((*dh == ' ') || (*dh == '\t')); dh++);
	lp->offset += (int)(dh - ch);

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



