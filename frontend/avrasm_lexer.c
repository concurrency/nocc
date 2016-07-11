/*
 *	avrasm_lexer.c -- lexer for AVR assembler sources
 *	Copyright (C) 2012-2016 Fred Barnes <frmb@kent.ac.uk>
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
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>

#include "nocc.h"
#include "support.h"
#include "version.h"
#include "symbols.h"
#include "keywords.h"
#include "lexer.h"
#include "lexpriv.h"
#include "avrasm.h"
#include "opts.h"

/*}}}*/
/*{{{  forward decls*/
static int avrasm_openfile (lexfile_t *lf, lexpriv_t *lp);
static int avrasm_closefile (lexfile_t *lf, lexpriv_t *lp);
static token_t *avrasm_nexttoken (lexfile_t *lf, lexpriv_t *lp);
static int avrasm_getcodeline (lexfile_t *lf, lexpriv_t *lp, char **rbuf);
static void avrasm_freelspecial (lexfile_t *lf, void *lspec);


/*}}}*/
/*{{{  public lexer struct*/
langlexer_t avrasm_lexer = {
	.langname = "avrasm",
	.langtag = LANGTAG_AVRASM,
	.fileexts = {".asm", ".inc", NULL},
	.openfile = avrasm_openfile,
	.closefile = avrasm_closefile,
	.nexttoken = avrasm_nexttoken,
	.getcodeline = avrasm_getcodeline,
	.freelspecial = avrasm_freelspecial,
	.parser = NULL
};

/*}}}*/
/*{{{  private types*/
typedef struct TAG_avrasm_lex {
	int dummy;
} avrasm_lex_t;

/*}}}*/


/*{{{  static avrasm_lspecial_t *avrasm_newavrasmlspecial (void)*/
/*
 *	creates a new avrasm_lspecial_t structure
 */
static avrasm_lspecial_t *avrasm_newavrasmlspecial (void)
{
	avrasm_lspecial_t *als = (avrasm_lspecial_t *)smalloc (sizeof (avrasm_lspecial_t));

	als->str = NULL;
	return als;
}
/*}}}*/
/*{{{  static void avrasm_freeavrasmlspecial (avrasm_lspecial_t *als)*/
/*
 *	frees an avrasm_lspecial_t structure
 */
static void avrasm_freeavrasmlspecial (avrasm_lspecial_t *als)
{
	if (!als) {
		nocc_serious ("avrasm_freelspecial(): NULL pointer!");
		return;
	}
	if (als->str) {
		sfree (als->str);
		als->str = NULL;
	}
	sfree (als);
	return;
}
/*}}}*/


/*{{{  static int avrasm_openfile (lexfile_t *lf, lexpriv_t *lp)*/
/*
 *	called once an AVR assembler source file has been opened
 */
static int avrasm_openfile (lexfile_t *lf, lexpriv_t *lp)
{
	avrasm_lex_t *lrp;

	lrp = (avrasm_lex_t *)smalloc (sizeof (avrasm_lex_t));
	lrp->dummy = 0;
	
	lp->langpriv = (void *)lrp;
	lf->lineno = 1;
	lf->colno = 1;
	return 0;
}
/*}}}*/
/*{{{  static int avrasm_closefile (lexfile_t *lf, lexpriv_t *lp)*/
/*
 *	called before an AVR assembler source file is closed
 */
static int avrasm_closefile (lexfile_t *lf, lexpriv_t *lp)
{
	avrasm_lex_t *lrp = (avrasm_lex_t *)(lp->langpriv);

	if (!lrp) {
		nocc_internal ("avrasm_closefile() not open! (%s)", lf->filename);
		return -1;
	}
	sfree (lrp);
	lp->langpriv = NULL;

	return 0;
}
/*}}}*/
/*{{{  static token_t *avrasm_nexttoken (lexfile_t *lf, lexpriv_t *lp)*/
/*
 *	returns the next token
 */
static token_t *avrasm_nexttoken (lexfile_t *lf, lexpriv_t *lp)
{
	avrasm_lex_t *lrp = (avrasm_lex_t *)lp->langpriv;
	token_t *tok = NULL;
	char *ch, *chlim, *dh;

	if (!lf || !lp) {
		return NULL;
	}

	tok = (token_t *)smalloc (sizeof (token_t));
	tok->type = NOTOKEN;
	tok->origin = (void *)lf;
	tok->lineno = lf->lineno;
	tok->colno = lf->colno;
	tok->tokwidth = 0;
	if (lp->offset == lp->size) {
		/* reached EOF */
		tok->type = END;
		return tok;
	}

tokenloop:
	ch = lp->buffer + lp->offset;
	chlim = lp->buffer + lp->size;

	/* special case -- if we've hit the end, must be nothing (literally) here */
	if (ch >= chlim) {
		tok->type = END;
		return tok;
	}

	/* guess what we're dealing with -- not so interested in indentation here :) */
	if (((*ch >= 'a') && (*ch <= 'z')) || ((*ch >= 'A') && (*ch <= 'Z'))) {
		/*{{{  probably a keyword or name*/
		keyword_t *kw;
		char *tmpstr;
		
		/* scan something that matches a word */
		for (dh=ch+1; (dh < chlim) && (((*dh >= 'a') && (*dh <= 'z')) ||
				((*dh >= 'A') && (*dh <= 'Z')) ||
				(*dh == '_') ||
				((*dh >= '0') && (*dh <= '9'))); dh++);
		
		tmpstr = string_ndup (ch, (int)(dh - ch));
		kw = keywords_lookup (tmpstr, (int)(dh - ch), LANGTAG_AVRASM);
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
		tok->tokwidth = (int)(dh - ch);
		lf->colno += (int)(dh - ch);
		/*}}}*/
	} else if ((*ch >= '0') && (*ch <= '9')) {
		/*{{{  number of sorts*/
		char *dh;
		char *npbuf = NULL;
		int ishex = 0;
		int isbin = ((*ch == '0') || (*ch == '1'));

		tok->type = INTEGER;
		dh = ch + 1;
		if ((dh < chlim) && (*dh == 'x')) {
			/* probably a hexadecimal constant */
			dh++;
			ishex = 1;
			isbin = 0;
			for (; (dh < chlim) && (((*dh >= '0') && (*dh <= '9')) || ((*dh >= 'a') && (*dh <= 'f')) || (*dh == '.')); dh++) {
				if (*dh == '.') {
					lexer_error (lf, "malformed hexadecimal constant");
					goto out_error1;
				}
			}
		} else {
			for (; (dh < chlim) && (((*dh >= '0') && (*dh <= '9')) || (*dh == '.')); dh++) {
				if (*dh == '.') {
					if (tok->type == REAL) {
						lexer_error (lf, "malformed real constant");
						goto out_error1;
					}
					tok->type = REAL;
					isbin = 0;
				} else if (isbin && (*dh != '0') && (*dh != '1')) {
					isbin = 0;
				}
			}
		}
		/* check to see if it's a forward/backward label reference */
		if ((tok->type == INTEGER) && ((*dh == 'f') || (*dh == 'b'))) {
			if ((*dh == 'b') && isbin && ((int)(dh - ch) > 2)) {
				/* assume binary if digits fit and more than 2 digits worth */
				int ival = 0;
				char *vh;

				for (vh=ch; vh<dh; vh++) {
					ival <<= 1;
					if (*vh == '1') {
						ival |= 1;
					}
				}
				tok->u.ival = ival;
				dh++;
				lp->offset += (int)(dh - ch);
				tok->tokwidth = (int)(dh - ch);
				lf->colno += (int)(dh - ch);
			} else {
				/* assume it is a label reference */
				avrasm_lspecial_t *als = avrasm_newavrasmlspecial ();

				dh++;
				tok->type = LSPECIAL;
				lp->offset += (int)(dh - ch);
				tok->tokwidth = (int)(dh - ch);
				lf->colno += (int)(dh - ch);

				tok->u.lspec = (void *)als;
				als->str = (char *)smalloc ((int)(dh - ch) + 1);
				memcpy (als->str, ch, (int)(dh - ch));
				als->str[(int)(dh - ch)] = '\0';
			}
		} else {
			lp->offset += (int)(dh - ch);
			tok->tokwidth = (int)(dh - ch);
			lf->colno += (int)(dh - ch);

			/* parse it */
			npbuf = (char *)smalloc ((int)(dh - ch) + 1);
			memcpy (npbuf, ch, (int)(dh - ch));
			npbuf[(int)(dh - ch)] = '\0';
			if ((tok->type == INTEGER) && ishex && (sscanf (npbuf, "%x", &tok->u.ival) != 1)) {
				lexer_error (lf, "malformed hexadecimal constant: %s", npbuf);
				sfree (npbuf);
				goto out_error1;
			} else if ((tok->type == REAL) && (sscanf (npbuf, "%lf", &tok->u.dval) != 1)) {
				lexer_error (lf, "malformed floating-point constant: %s", npbuf);
				sfree (npbuf);
				goto out_error1;
			} else if ((tok->type == INTEGER) && !ishex && (sscanf (npbuf, "%d", &tok->u.ival) != 1)) {
				lexer_error (lf, "malformed integer constant: %s", npbuf);
				sfree (npbuf);
				goto out_error1;
			} else {
				sfree (npbuf);
			}
		}
		/*}}}*/
	} else switch (*ch) {
		/*{{{  \r, \n (newline) */
	case '\r':
		lp->offset++;
		goto tokenloop;
	case '\n':
		lf->lineno++;
		lf->colno = 1;
		lp->offset++;
		tok->type = NEWLINE;
		/* and skip multiple newlines */
		for (dh = ++ch; (dh < chlim) && ((*dh == '\n') || (*dh == '\r')); dh++) {
			if (*dh == '\n') {
				lf->lineno++;
			}
		}
		lp->offset += (int)(dh - ch);

		/* return here, don't walk over whitespace */
		return tok;
		break;
		/*}}}*/
		/*{{{  ; (comment)*/
	case ';':
		tok->type = COMMENT;
		/* scan to end-of-line */
		for (dh=ch+1; (dh < chlim) && (*dh != '\n') && (*dh != '\r'); dh++);
		lp->offset += (int)(dh - ch);
		lf->colno += (int)(dh - ch);

		break;
		/*}}}*/
		/*{{{  space, tab*/
	case ' ':
	case '\t':
		/* skip over leading whitespace */
		lp->offset++;
		lf->colno++;
		goto tokenloop;
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
				case '\\':
					/* escape char */
					dh++;
					if (dh == chlim) {
						if (!compopts.unexpected) {
							lexer_error (lf, "unexpected end of file");
						}
						goto out_error1;
					}
					switch (*dh) {
					case 'n':
					case 'r':
					case 't':
					case '\'':
					case '\"':
					case '\\':
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
				if (!compopts.unexpected) {
					lexer_error (lf, "unexpected end of file");
				}
				goto out_error1;
			}
			tok->u.str.ptr = (char *)smalloc (slen + 1);
			tok->u.str.len = 0;		/* fixup in a bit */
			xch = tok->u.str.ptr;
			slen = 0;
			/*{{{  now actually process it*/
			for (dh = (ch + 1); (dh < chlim) && (*dh != '\"'); dh++) {
				switch (*dh) {
				case '\\':
					/* escape char */
					dh++;
					switch (*dh) {
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
			tok->tokwidth = (int)(dh - ch);
			lf->colno += (int)(dh - ch);
		}
		break;
		/*}}}*/
		/*{{{  ' (character)*/
	case '\'':
		{
			int wid = 0;

			tok->type = INTEGER;
			ch++;
			lp->offset++;
			wid++;
			if ((ch + 1) >= chlim) {
				if (!compopts.unexpected) {
					lexer_error (lf, "unexpected end of file");
				}
				goto out_error1;
			}
			if (*ch == '\\') {
				/*{{{  escape character*/
				ch++;
				lp->offset++;
				wid++;
				switch (*ch) {
				case 'n':
					tok->u.ival = (int)'\n';
					break;
				case 'r':
					tok->u.ival = (int)'\r';
					break;
				case '\'':
					tok->u.ival = (int)'\'';
					break;
				case '\"':
					tok->u.ival = (int)'\"';
					break;
				case 't':
					tok->u.ival = (int)'\t';
					break;
				case '\\':
					tok->u.ival = (int)'\\';
					break;
				default:
					lexer_error (lf, "unknown escape character \'\\%c\'", *ch);
					break;
				}
				ch++;
				lp->offset++;
				wid++;
				/*}}}*/
			} else {
				/* regular character */
				tok->u.ival = (int)(*ch);
				ch++;
				lp->offset++;
				wid++;
			}
			/* expect closing quote */
			if (*ch != '\'') {
				lexer_error (lf, "malformed character constant (expected closing quote, got \'%c\')", *ch);
				goto out_error1;
			}
			lp->offset++;
			wid++;

			tok->tokwidth = wid;
			lf->colno += wid;
		}
		break;
		/*}}}*/
		/*{{{  default (symbol)*/
	default:
		/* try and match as a symbol */
		{
			symbol_t *sym = symbols_match (ch, chlim, LANGTAG_AVRASM);

			if (sym) {
				/* found something */
				tok->type = SYMBOL;
				tok->u.sym = sym;
				lp->offset += sym->mlen;
				tok->tokwidth = sym->mlen;
				lf->colno += sym->mlen;
				
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
	lf->colno += (int)(dh - ch);

	return tok;
out_error1:
	/* skip to end of line or file */
	tok->type = NOTOKEN;
	ch = lp->buffer + lp->offset;
	for (dh = ch; (dh < chlim) && (*dh != '\n'); dh++);
	lp->offset += (int)(dh - ch);
	lf->colno += (int)(dh - ch);

	return tok;
}
/*}}}*/
/*{{{  static int avrasm_getcodeline (lexfile_t *lf, lexpriv_t *lp, char **rbuf)*/
/*
 *	get the current code line from the input buffer (returns a fresh string in '*rbuf')
 *	returns 0 on success, non-zero on failure
 */
static int avrasm_getcodeline (lexfile_t *lf, lexpriv_t *lp, char **rbuf)
{
	*rbuf = NULL;
	return -1;
}
/*}}}*/
/*{{{  static void avrasm_freelspecial (lexfile_t *lf, void *lspec)*/
/*
 *	frees an avrasm_lspecial_t structure
 */
static void avrasm_freelspecial (lexfile_t *lf, void *lspec)
{
	avrasm_lspecial_t *als = (avrasm_lspecial_t *)lspec;

	if (!als) {
		return;
	}
	avrasm_freeavrasmlspecial (als);
	return;
}
/*}}}*/

