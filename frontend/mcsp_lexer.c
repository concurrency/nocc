/*
 *	mcsp_lexer.c -- lexer for MCSP
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
#include "mcsp.h"
#include "opts.h"


/*}}}*/
/*{{{  forward decls*/
static int mcsp_openfile (lexfile_t *lf, lexpriv_t *lp);
static int mcsp_closefile (lexfile_t *lf, lexpriv_t *lp);
static token_t *mcsp_nexttoken (lexfile_t *lf, lexpriv_t *lp);


/*}}}*/
/*{{{  public lexer struct*/
langlexer_t mcsp_lexer = {
	langname: "mcsp",
	fileexts: {".mcsp", ".csp", NULL},
	openfile: mcsp_openfile,
	closefile: mcsp_closefile,
	nexttoken: mcsp_nexttoken,
	parser: NULL
};

/*}}}*/
/*{{{  private types*/
typedef struct TAG_mcsp_lex {
	int curindent;			/* current indent */
} mcsp_lex_t;


/*}}}*/


/*{{{  static int mcsp_openfile (lexfile_t *lf, lexpriv_t *lp)*/
/*
 *	called once an MCSP source file has been opened
 */
static int mcsp_openfile (lexfile_t *lf, lexpriv_t *lp)
{
	mcsp_lex_t *lmp;

	lmp = (mcsp_lex_t *)smalloc (sizeof (mcsp_lex_t));
	lmp->curindent = 0;
	
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
/*{{{  */
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

		/* return here, don't walk over whitespace */
		return tok;
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


