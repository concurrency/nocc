/*
 *	rcxb_lexer.c -- lexer for BASIC-style language
 *	Copyright (C) 2006 Fred Barnes <frmb@kent.ac.uk>
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
#include "rcxb.h"
#include "opts.h"


/*}}}*/
/*{{{  forward decls*/
static int rcxb_openfile (lexfile_t *lf, lexpriv_t *lp);
static int rcxb_closefile (lexfile_t *lf, lexpriv_t *lp);
static token_t *rcxb_nexttoken (lexfile_t *lf, lexpriv_t *lp);
static int rcxb_getcodeline (lexfile_t *lf, lexpriv_t *lp, char **rbuf);


/*}}}*/
/*{{{  public lexer struct*/
langlexer_t rcxb_lexer = {
	.langname = "rcxbasic",
	.langtag = LANGTAG_RCXB,
	.fileexts = {".bas", ".ncb", NULL},
	.openfile = rcxb_openfile,
	.closefile = rcxb_closefile,
	.nexttoken = rcxb_nexttoken,
	.getcodeline = rcxb_getcodeline,
	.freelspecial = NULL,
	.parser = NULL
};

/*}}}*/
/*{{{  private types*/
typedef struct TAG_rcxb_lex {
	int is_setup;
	keyword_t *kw_rem;
} rcxb_lex_t;

/*}}}*/


/*{{{  static int rcxb_openfile (lexfile_t *lf, lexpriv_t *lp)*/
/*
 *	called once an RCX-BASIC source file has been opened
 */
static int rcxb_openfile (lexfile_t *lf, lexpriv_t *lp)
{
	rcxb_lex_t *lrp;

	lrp = (rcxb_lex_t *)smalloc (sizeof (rcxb_lex_t));
	lrp->is_setup = 0;
	lrp->kw_rem = NULL;
	
	lp->langpriv = (void *)lrp;
	lf->lineno = 1;
	return 0;
}
/*}}}*/
/*{{{  static int rcxb_closefile (lexfile_t *lf, lexpriv_t *lp)*/
/*
 *	called before an MCSP source file is closed
 */
static int rcxb_closefile (lexfile_t *lf, lexpriv_t *lp)
{
	rcxb_lex_t *lrp = (rcxb_lex_t *)(lp->langpriv);

	if (!lrp) {
		nocc_internal ("rcxb_closefile() not open! (%s)", lf->filename);
		return -1;
	}
	sfree (lrp);
	lp->langpriv = NULL;

	return 0;
}
/*}}}*/
/*{{{  static token_t *rcxb_nexttoken (lexfile_t *lf, lexpriv_t *lp)*/
/*
 *	returns the next token
 */
static token_t *rcxb_nexttoken (lexfile_t *lf, lexpriv_t *lp)
{
	rcxb_lex_t *lrp = (rcxb_lex_t *)lp->langpriv;
	token_t *tok = NULL;
	char *ch, *chlim, *dh;

	if (!lf || !lp) {
		return NULL;
	}
	if (!lrp->is_setup) {
		lrp->kw_rem = keywords_lookup ("rem", 3, LANGTAG_RCXB);
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

tokenloop:
	ch = lp->buffer + lp->offset;
	chlim = lp->buffer + lp->size;

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
		kw = keywords_lookup (tmpstr, (int)(dh - ch), LANGTAG_RCXB);
		sfree (tmpstr);

		if (!kw) {
			/* assume name */
			tok->type = NAME;
			tok->u.name = string_ndup (ch, (int)(dh - ch));
		} else if (kw == lrp->kw_rem) {
			/* rem ... -- comment to end-of-line */
			for (dh=ch+1; (dh < chlim) && (*dh != '\n') && (*dh != '\r'); dh++);
			tok->type = COMMENT;
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

		/* return here, don't walk over whitespace */
		return tok;
		break;
		/*}}}*/
		/*{{{  # (comment)*/
	case '#':
		tok->type = COMMENT;
		/* scan to end-of-line */
		for (dh=ch+1; (dh < chlim) && (*dh != '\n') && (*dh != '\r'); dh++);
		lp->offset += (int)(dh - ch);

		break;
		/*}}}*/
		/*{{{  space, tab*/
	case ' ':
	case '\t':
		/* skip over leading whitespace */
		lp->offset++;
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
						lexer_error (lf, "unexpected end of file");
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
		}
		break;
		/*}}}*/
		/*{{{  default (symbol)*/
	default:
		/* try and match as a symbol */
default_label:
		{
			symbol_t *sym = symbols_match (ch, chlim, LANGTAG_RCXB);

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
/*{{{  static int rcxb_getcodeline (lexfile_t *lf, lexpriv_t *lp, char **rbuf)*/
/*
 *	get the current code line from the input buffer (returns a fresh string in '*rbuf')
 *	returns 0 on success, non-zero on failure
 */
static int rcxb_getcodeline (lexfile_t *lf, lexpriv_t *lp, char **rbuf)
{
	*rbuf = NULL;
	return -1;
}
/*}}}*/

