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
	.langname = "guppy",
	.langtag = LANGTAG_GUPPY,
	.fileexts = {".gpp", ".gpi", NULL},
	.openfile = guppy_openfile,
	.closefile = guppy_closefile,
	.nexttoken = guppy_nexttoken,
	.getcodeline = guppy_getcodeline,
	.freelspecial = NULL,
	.parser = NULL
};

/*}}}*/
/*{{{  private lexer struct*/
typedef struct TAG_guppy_lex {
	DYNARRAY (int, indent_offsets);		/* history of where indentations occur */
	int curindent;				/* where we are, index into the above */
	int scanto_indent;			/* where we are currently scanning to */

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
	dynarray_add (lop->indent_offsets, 0);		/* first indent (zero'th) is left-margin */
	lop->curindent = 0;
	lop->scanto_indent = 0;
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
/*{{{  static token_t *guppy_nexttoken (lexfile_t *lf, lexpriv_t *lp)*/
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

tokenloop:
	if (lp->offset == lp->size) {
		/* reached EOF, check indentation leftovers */
		if (lop->curindent > 0) {
			lop->curindent--;
			tok->type = OUTDENT;
			dynarray_delitem (lop->indent_offsets, DA_CUR (lop->indent_offsets) - 1);
			goto out_tok;
		}
		tok->type = END;
		goto out_tok;
	}

	/* make some sort of guess on what we're dealing with */
	ch = lp->buffer + lp->offset;
	chlim = lp->buffer + lp->size;

	if (lop->newlineflag) {
		/*{{{  had a newline recently, check for indent/outdent*/
		int thisindent = 0;
		int xind;

		/* measure indentation */
		for (dh=ch; (dh < chlim) && ((*dh == ' ') || (*dh == '\t')); dh++) {
			if (*dh == ' ') {
				thisindent++;
			} else {
				/* tabs are 8 spaces, aligned at next tab-stop */
				thisindent = ((thisindent >> 3) + 1) << 3;
			}
		}

		/* special-case: if we have a comment, skip to next-line */
		if ((dh < chlim) && (*dh == '#')) {
			/* yes, skip to EOL and onto next line */
			for (; (dh < chlim) && (*dh != '\n'); dh++);
			lp->offset += (int)(dh - ch);

			goto tokenloop;
		}

		/* move buffer along */
		lp->offset += (int)(dh - ch);
		ch = dh;
		lop->newlineflag = 0;
		lop->oldnewline = 1;
		if (ch < chlim) {
			if ((*ch == '\n') || (*ch == '\r')) {
				/* empty line or just whitespace */
				goto tokenloop;
			}
		}

		/* find out where we are in relation to indent history */
		for (xind=0; (xind < DA_CUR (lop->indent_offsets)) && (thisindent > DA_NTHITEM (lop->indent_offsets, xind)); xind++);
#if 0
nocc_message ("guppy_nexttoken(): newlineflag, thisindent = %d, xind = %d, DA_CUR (offs) = %d, curindent = %d, scanto = %d", thisindent, xind, DA_CUR (lop->indent_offsets), lop->curindent, lop->scanto_indent);
#endif

		if (xind == DA_CUR (lop->indent_offsets)) {
			/* add this one */
			dynarray_add (lop->indent_offsets, thisindent);
			lop->scanto_indent = xind;
#if 0
nocc_message ("greater: setting scanto = %d, DA_CUR (offs) = %d", lop->scanto_indent, DA_CUR (lop->indent_offsets));
#endif
		} else if (thisindent == DA_NTHITEM (lop->indent_offsets, xind)) {
			/* exactly this one, scan to it */
			lop->scanto_indent = xind;
#if 0
nocc_message ("same-as: setting scanto = %d, DA_CUR (offs) = %d", lop->scanto_indent, DA_CUR (lop->indent_offsets));
#endif
		} else if (thisindent < DA_NTHITEM (lop->indent_offsets, xind)) {
#if 0
nocc_message ("off-width: setting scanto = %d, DA_CUR (offs) = %d", lop->scanto_indent, DA_CUR (lop->indent_offsets));
#endif
			/* not exactly this one, but less than the next, generate outdent and then indent */
			lop->scanto_indent = xind;
			lop->curindent = xind - 1;
			DA_SETNTHITEM (lop->indent_offsets, xind, thisindent);

			tok->type = OUTDENT;
			goto out_tok;
		}

		/*}}}*/
	} else {
		lop->oldnewline = 0;
	}

#if 0
nocc_message ("here: scanto = %d, curindent = %d", lop->scanto_indent, lop->curindent);
#endif

	/* might still be scanning to a specific indent point */
	if (lop->scanto_indent < lop->curindent) {
		if (lop->curindent > 0) {
			/* remove knowledge of this one */
			dynarray_delitem (lop->indent_offsets, lop->curindent);
		}
		lop->curindent--;
		tok->type = OUTDENT;
#if 0
nocc_message ("OUTDENT: reducing curindent to %d, scanto = %d", lop->curindent, lop->scanto_indent);
#endif
		goto out_tok;
	} else if (lop->scanto_indent > lop->curindent) {
		lop->curindent++;
		tok->type = INDENT;
#if 0
nocc_message ("INDENT: advancing curindent to %d, scanto = %d", lop->curindent, lop->scanto_indent);
#endif
		goto out_tok;
	}

	/* next thing for a token! */
	if (((*ch >= 'a') && (*ch <= 'z')) || ((*ch >= 'A') && (*ch <= 'Z'))) {
		/*{{{  probably a keyword or name*/
		keyword_t *kw;
		char *tmpstr;
		char *nstart;

		/* scan something that matches a word */
		for (dh=ch+1; (dh < chlim) && (((*dh >= 'a') && (*dh <= 'z')) ||
				((*dh >= 'A') && (*dh <= 'Z')) ||
				(*dh == '_') ||
				((*dh >= '0') && (*dh <= '9'))); dh++);
		/* see if it ends in a number, e.g. "int8" */
		for (nstart=dh-1; (nstart > ch) && (*nstart >= '0') && (*nstart <= '9'); nstart--);
		nstart++;

		tmpstr = string_ndup (ch, (int)(dh - ch));
		kw = keywords_lookup (tmpstr, (int)(dh - ch), LANGTAG_GUPPY);
		sfree (tmpstr);

		if (!kw) {
			if (nstart < dh) {
				/* check to see if it's a special type (int8, etc.) */
				tmpstr = string_ndup (ch, (int)(nstart - ch));
				kw = keywords_lookup (tmpstr, (int)(nstart - ch), LANGTAG_GUPPY);
				sfree (tmpstr);

#if 0
fprintf (stderr, "guppy-lexer: number-ending keyword 0x%8.8x\n", (unsigned int)kw);
#endif
				if (kw && (kw->langtag & LANGTAG_STYPE)) {
					/* yes, and the end is all number */
					int size;

					tmpstr = string_ndup (nstart, (int)(dh - nstart));
					if (sscanf (tmpstr, "%d", &size) != 1) {
						lexer_error (lf, "invalid number in sized keyword [%s]", kw->name);
						sfree (tmpstr);
						goto out_error1;
					}

					tok->type = KEYWORD;
					tok->u.kw = kw;
					tok->iptr = (void *)size;
				} else {
					/* assume name */
					tok->type = NAME;
					tok->u.name = string_ndup (ch, (int)(dh - ch));
				}
			} else {
				/* assume name */
				tok->type = NAME;
				tok->u.name = string_ndup (ch, (int)(dh - ch));
			}
		} else {
			/* keyword found */
			tok->type = KEYWORD;
			tok->u.kw = kw;
		}
		lp->offset += (int)(dh - ch);
		/*}}}*/
	} else if ((*ch >= '0') && (*ch <= '9')) {
		/*{{{  number of sorts*/
		char *npbuf = NULL;

		tok->type = INTEGER;
		for (dh=ch+1; (dh < chlim) && (((*dh >= '0') && (*dh <= '9')) || (*dh == '.')); dh++) {
			if (*dh == '.') {
				if (tok->type == REAL) {
					lexer_error (lf, "malformed real number");
					goto out_error1;
				}
				tok->type = REAL;
			}
		}
		lp->offset += (int)(dh - ch);

		/* parse it */
		npbuf = string_ndup (ch, (int)(dh - ch));
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
		lp->offset += (int)(dh - ch);
		lop->newlineflag = 1;
		/* return here, don't touch whitespace */
		goto out_tok;
		/*}}}*/
		/*{{{  space, tab (unexpected)*/
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
		/* return this as an integer */
		{
			char *dh = ch;
			int eschar;

			tok->type = INTEGER;
			dh++;
			if (*dh == '\\') {
				/* escape character */
				eschar = guppy_escape_char (lf, lop, &dh);
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
					/* escape character */
					dh++;
					if (dh == chlim) {
						lexer_error (lf, "expected end of file");
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
			tok->u.str.len = 0;			/* fixup in a bit */
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
		/* scan to end-of-line */
		for (dh=ch+1; (dh < chlim) && (*dh != '\n') && (*dh != '\r'); dh++);
		lp->offset += (int)(dh - ch);
		break;
		/*}}}*/
		/*{{{  default (symbol)*/
	default:
		/* try and match a symbol */
default_label:
		{
			symbol_t *sym = symbols_match (ch, chlim, LANGTAG_GUPPY);

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

	/* skip any remaining whitespace */
	ch = lp->buffer + lp->offset;
	for (dh = ch; (dh < chlim) && ((*dh == ' ') || (*dh == '\t')); dh++);
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



