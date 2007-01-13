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
#include "occampi.h"
#include "opts.h"
/*}}}*/

/*{{{  forward decls.*/
static int occampi_openfile (lexfile_t *lf, lexpriv_t *lp);
static int occampi_closefile (lexfile_t *lf, lexpriv_t *lp);
static token_t *occampi_nexttoken (lexfile_t *lf, lexpriv_t *lp);
static int occampi_getcodeline (lexfile_t *lf, lexpriv_t *lp, char **rbuf);


/*}}}*/
/*{{{  public lexer struct*/
langlexer_t occampi_lexer = {
	langname: "occam-pi",
	fileexts: {".occ", ".inc", NULL},
	openfile: occampi_openfile,
	closefile: occampi_closefile,
	nexttoken: occampi_nexttoken,
	getcodeline: occampi_getcodeline,
	parser: NULL
};


/*}}}*/
/*{{{  private lexer struct*/
typedef struct TAG_occampi_lex {
	int curindent;			/* current indent */
	int scanto_indent;		/* target indent when it changes */
	int newlineflag;
	int oldnewline;
	int cescapes;			/* whether we're using C-style escape characters */
	int coperators;			/* whether we're using C-style operators */
} occampi_lex_t;


/*}}}*/
/*{{{  private types + data*/
typedef struct TAG_copmap {
	char *str_lookup;
	tokentype_t ttype_lookup;
	token_t *tok_lookup;
	char *str_replace;
	tokentype_t ttype_replace;
	token_t *tok_replace;
} copmap_t;

static copmap_t copmap[] = {
	{"=", SYMBOL, NULL, ":=", SYMBOL, NULL},
	{"==", SYMBOL, NULL, "=", SYMBOL, NULL},
	{"!=", SYMBOL, NULL, "<>", SYMBOL, NULL},
	{NULL, NOTOKEN, NULL, NULL, NOTOKEN, NULL}
};
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
/*{{{  static int occampi_escape_char (lexfile_t *lf, occampi_lex_t *lop, char **ptr)*/
/*
 *	extracts an escape sequence from a string
 *	returns the character it represents, -255 on error
 */
static int occampi_escape_char (lexfile_t *lf, occampi_lex_t *lop, char **ptr)
{
	int echr = 0;

	if (!lop->cescapes && (**ptr != '*')) {
		lexer_error (lf, "occampi_escape_char(): called incorrectly");
		(*ptr)++;
		goto out_error1;
	} else if (!lop->cescapes) {
		(*ptr)++;
		switch (**ptr) {
		case 'n':
			echr = (int)'\n';
			(*ptr)++;
			break;
		case 'c':
			echr = (int)'\r';
			(*ptr)++;
			break;
		case 't':
			echr = (int)'\t';
			(*ptr)++;
			break;
		case '*':
		case '\'':
		case '\"':
			echr = (int)(**ptr);
			(*ptr)++;
			break;
		case '#':
			if (check_hex (*ptr + 1, 2)) {
				lexer_error (lf, "malformed hexidecimal escape in character constant");
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


/*{{{  static void copmap_init (void)*/
/*
 *	initialises the C operator map
 */
static void copmap_init (void)
{
	if (!copmap[0].tok_lookup) {
		int i;

		for (i=0; copmap[i].str_lookup; i++) {
			copmap[i].tok_lookup = lexer_newtoken (copmap[i].ttype_lookup, copmap[i].str_lookup);
			copmap[i].tok_replace = lexer_newtoken (copmap[i].ttype_replace, copmap[i].str_replace);
		}
	}
	return;
}
/*}}}*/
/*{{{  static int copmap_ctooccampi (token_t *tok)*/
/*
 *	does replacements for C-style operators
 *	returns 0 if nothing changed, non-zero otherwise
 */
static int copmap_ctooccampi (token_t *tok)
{
	int i;

	for (i=0; copmap[i].tok_lookup; i++) {
		if (lexer_tokmatch (copmap[i].tok_lookup, tok)) {
			/* change token symbol */
			tok->u.sym = copmap[i].tok_replace->u.sym;
			return 1;
		}
	}
	return 0;
}
/*}}}*/


/*{{{  int occampi_lexer_opthandler_flag (cmd_option_t *opt, char ***argwalk, int *argleft)*/
/*
 *	option-handler for occam-pi language options
 *	returns 0 on success, non-zero on failure
 */
int occampi_lexer_opthandler_flag (cmd_option_t *opt, char ***argwalk, int *argleft)
{
	int optv = (int)opt->arg;
	lexfile_t *ptrarg;

	switch (optv) {
	case 1:
		/* set C-style operators */
		if (*argleft < 2) {
			nocc_error ("occampi_lexer_opthandler_flag(): missing pointer argument!");
			return -1;
		}
		ptrarg = (lexfile_t *)((*argwalk)[1]);

		nocc_message ("using C style operators for %s", ptrarg ? ptrarg->fnptr : "(unknown)");

		/*{{{  actually set inside lexer private data*/
		{
			lexpriv_t *lp = (lexpriv_t *)ptrarg->priv;
			occampi_lex_t *lop = (occampi_lex_t *)lp->langpriv;

			copmap_init ();
			lop->coperators = 1;
		}
		/*}}}*/
		break;
	default:
		return -1;
	}

	return 0;
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
	lop->oldnewline = 0;
	lop->cescapes = 0;
	lop->coperators = 0;
	
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

	tok = lexer_newtoken (NOTOKEN);
	tok->origin = (void *)lf;
	tok->lineno = lf->lineno;

tokenloop:
	if (lp->offset == lp->size) {
		/* reached EOF */
		tok->type = END;
		return tok;
	}

	/* make some sort of guess on what we're dealing with */
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
		lop->oldnewline = 1;
		ch = dh;
		lop->scanto_indent = thisindent;
		/* then scoop this up next */
		/*}}}*/
	} else {
		lop->oldnewline = 0;
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
			int eschar;

			tok->type = INTEGER;
			dh++;
			if (*dh == '*') {
				/* escape character */
				eschar = occampi_escape_char (lf, lop, &dh);
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
		/*{{{  # (symbol or hex-constant)*/
	case '#':
		if (lop->oldnewline) {
			/* recently had a newline, probably a symbol (pre-processor) */
			goto default_label;
		} else {
			/*{{{  hexidecimal number*/
			char *dh;
			char *npbuf = NULL;

			tok->type = INTEGER;
			for (dh=ch+1; (dh < chlim) && (((*dh >= '0') && (*dh <= '9')) || ((*dh >= 'A') && (*dh <= 'F'))); dh++);
			lp->offset += (int)(dh - ch);
			/* parse it */
			npbuf = (char *)smalloc ((int)(dh - ch));
			memcpy (npbuf, ch+1, (int)(dh - ch) - 1);
			npbuf[(int)(dh - ch) - 1] = '\0';
			if (sscanf (npbuf, "%x", &tok->u.ival) != 1) {
				lexer_error (lf, "malformed hexidecimal constant: %s", npbuf);
				sfree (npbuf);
				goto out_error1;
			} else {
				sfree (npbuf);
			}
			/*}}}*/
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
				
				if (lop->coperators) {
					copmap_ctooccampi (tok);
				}
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
/*{{{  static int occampi_getcodeline (lexfile_t *lf, lexpriv_t *lp, char **rbuf)*/
/*
 *	gets the current line of code from the input file and puts it in a new buffer (pointer returned in '*rbuf')
 *	returns 0 on success, non-zero on failure
 */
static int occampi_getcodeline (lexfile_t *lf, lexpriv_t *lp, char **rbuf)
{
	occampi_lex_t *lop;
	char *chstart, *chend;

	if (!lp || !lp->langpriv) {
		return -1;
	}
	if (!lp->buffer) {
		return -1;
	}

	lop = (occampi_lex_t *)(lp->langpriv);
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


