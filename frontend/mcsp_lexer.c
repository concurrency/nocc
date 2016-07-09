/*
 *	mcsp_lexer.c -- lexer for MCSP
 *	Copyright (C) 2006-2016 Fred Barnes <frmb@kent.ac.uk>
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
#include "mcsp.h"
#include "opts.h"


/*}}}*/
/*{{{  forward decls*/
static int mcsp_openfile (lexfile_t *lf, lexpriv_t *lp);
static int mcsp_closefile (lexfile_t *lf, lexpriv_t *lp);
static token_t *mcsp_nexttoken (lexfile_t *lf, lexpriv_t *lp);
static int mcsp_getcodeline (lexfile_t *lf, lexpriv_t *lp, char **rbuf);


/*}}}*/
/*{{{  public lexer struct*/
langlexer_t mcsp_lexer = {
	.langname = "mcsp",
	.langtag = LANGTAG_MCSP,
	.fileexts = {".mcsp", ".csp", NULL},
	.openfile = mcsp_openfile,
	.closefile = mcsp_closefile,
	.nexttoken = mcsp_nexttoken,
	.getcodeline = mcsp_getcodeline,
	.freelspecial = NULL,
	.parser = NULL
};

/*}}}*/
/*{{{  private data*/
static int default_unboundvars = 0;


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
/*{{{  static int mcsp_escape_char (lexfile_t *lf, mcsp_lex_t *lop, char **ptr)*/
/*
 *	extracts an escape sequence from a string
 *	returns the character it represents, -255 on error
 */
static int mcsp_escape_char (lexfile_t *lf, mcsp_lex_t *lmp, char **ptr)
{
	int echr = 0;

	if (**ptr != '\\') {
		lexer_error (lf, "mcsp_escape_char(): called incorrectly");
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
		case 't':
			echr = (int)'\t';
			(*ptr)++;
			break;
		case '\\':
		case '\'':
		case '\"':
			echr = (int)(**ptr);
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


/*{{{  int mcsp_lexer_opthandler_flag (cmd_option_t *opt, char ***argwalk, int *argleft)*/
/*
 *	option handler for MCSP language options
 *	returns 0 on success, non-zero on failure
 */
int mcsp_lexer_opthandler_flag (cmd_option_t *opt, char ***argwalk, int *argleft)
{
	int optv = (int)opt->arg;
/*	lexfile_t *ptrarg; */

	switch (optv) {
	case 1:
		/* allow unbound vars */
		/* FIXME: this responds to option processing in code */
#if 0
		if (*argleft < 2) {
			nocc_error ("mcsp_lexer_opthandler_flag(): missing pointer argument!");
			return -1;
		}
		ptrarg = (lexfile_t *)((*argwalk)[1]);

		nocc_message ("allowing unbound events in %s", ptrarg ? ptrarg->fnptr : "(unknown)");
		/*{{{  actually set inside lexer private data*/
		{
			lexpriv_t *lp = (lexpriv_t *)ptrarg->priv;
			mcsp_lex_t *lmp = (mcsp_lex_t *)lp->langpriv;

			lmp->unboundvars = 1;
		}
		/*}}}*/
#else
		default_unboundvars = 1;
#endif
		break;
	default:
		return -1;
	}

	return 0;
}
/*}}}*/


/*{{{  static int mcsp_openfile (lexfile_t *lf, lexpriv_t *lp)*/
/*
 *	called once an MCSP source file has been opened
 */
static int mcsp_openfile (lexfile_t *lf, lexpriv_t *lp)
{
	mcsp_lex_t *lmp;

	lmp = (mcsp_lex_t *)smalloc (sizeof (mcsp_lex_t));
	lmp->unboundvars = default_unboundvars;
	
	lp->langpriv = (void *)lmp;
	lf->lineno = 1;
	return 0;
}
/*}}}*/
/*{{{  static int mcsp_closefile (lexfile_t *lf, lexpriv_t *lp)*/
/*
 *	called before an MCSP source file is closed
 */
static int mcsp_closefile (lexfile_t *lf, lexpriv_t *lp)
{
	mcsp_lex_t *lmp = (mcsp_lex_t *)(lp->langpriv);

	if (!lmp) {
		nocc_internal ("mcsp_closefile() not open! (%s)", lf->filename);
		return -1;
	}
	sfree (lmp);
	lp->langpriv = NULL;

	return 0;
}
/*}}}*/
/*{{{  static token_t *mcsp_nexttoken (lexfile_t *lf, lexpriv_t *lp)*/
/*
 *	returns the next token
 */
static token_t *mcsp_nexttoken (lexfile_t *lf, lexpriv_t *lp)
{
	mcsp_lex_t *lmp = (mcsp_lex_t *)lp->langpriv;
	token_t *tok = NULL;
	char *ch, *chlim, *dh;

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
		kw = keywords_lookup (tmpstr, (int)(dh - ch), LANGTAG_MCSP);
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
		/* shouldn't see this */
		lexer_warning (lf, "unexpected whitespace");
		lp->offset++;
		goto tokenloop;
		break;
		/*}}}*/
		/*{{{  ' (character)*/
	case '\'':
		/* returned as an integer */
		{
			char *dh = ch;
			int eschar;

			tok->type = INTEGER;
			dh++;
			if (*dh == '\\') {
				/* escape character */
				eschar = mcsp_escape_char (lf, lmp, &dh);
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
					case 'x':
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
					case 'x':
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
		{
			symbol_t *sym = symbols_match (ch, chlim, LANGTAG_MCSP);

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
/*{{{  static int mcsp_getcodeline (lexfile_t *lf, lexpriv_t *lp, char **rbuf)*/
/*
 *	gets the next line of code from the input buffer (returns fresh string in '*rbuf')
 *	returns 0 on success, non-zero on failure
 */
static int mcsp_getcodeline (lexfile_t *lf, lexpriv_t *lp, char **rbuf)
{
	*rbuf = NULL;
	return -1;
}
/*}}}*/

