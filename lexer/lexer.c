/*
 *	lexer.c -- nocc lexer
 *	Copyright (C) 2004 Fred Barnes <frmb@kent.ac.uk>
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
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "nocc.h"
#include "support.h"
#include "version.h"
#include "symbols.h"
#include "keywords.h"
#include "lexer.h"
#include "lexpriv.h"
#include "parser.h"
#include "occampi.h"

/*{{{  private stuff*/
STATICDYNARRAY (lexfile_t *, lexfiles);

/*}}}*/


/*{{{  void lexer_init (void)*/
/*
 *	initialises the lexer
 */
void lexer_init (void)
{
	dynarray_init (lexfiles);
	return;
}
/*}}}*/
/*{{{  lexfile_t *lexer_open (char *filename)*/
/*
 *	opens a file for lexing
 */
lexfile_t *lexer_open (char *filename)
{
	lexfile_t *lf = NULL;
	lexpriv_t *lp;
	int i;
	struct stat stbuf;
	char *fextn;

	/* if a path isn't specified, do a search based on extension (relative current directory first) */
	fextn = NULL;

	if (access (filename, R_OK)) {
		nocc_error ("unable to access %s for reading: %s", filename, strerror (errno));
		return NULL;
	}
	/* already know about this one ? */
	for (i=0; i<DA_CUR(lexfiles); i++) {
		lf = DA_NTHITEM (lexfiles, i);
		if (!strcmp (lf->filename, filename)) {
			break;
		}
	}
	if (i == DA_CUR (lexfiles)) {
		lf = (lexfile_t *)smalloc (sizeof (lexfile_t));
		lf->filename = string_dup (filename);
		for (lf->fnptr = lf->filename + (strlen (lf->filename) - 1); (lf->fnptr > lf->filename) && ((lf->fnptr)[-1] != '/'); (lf->fnptr)--);

		lf->priv = NULL;
		lf->lineno = 0;
		dynarray_add (lexfiles, lf);
	}
	if (lf->priv) {
		nocc_error ("%s is already open!", lf->filename);
		return NULL;
	}
	if (stat (lf->filename, &stbuf)) {
		nocc_error ("failed to stat %s: %s", lf->filename, strerror (errno));
		return NULL;
	}
	/* open it */
	lp = (lexpriv_t *)smalloc (sizeof (lexpriv_t));
	lf->priv = (void *)lp;
	lp->fd = open (lf->filename, O_RDONLY);
	if (lp->fd < 0) {
		nocc_error ("failed to open %s for reading: %s", lf->filename, strerror (errno));
		sfree (lp);
		lf->priv = NULL;
		return NULL;
	}
	lp->size = (int)stbuf.st_size;
	lp->offset = 0;
	lp->buffer = (char *)mmap ((void *)0, lp->size, PROT_READ, MAP_SHARED, lp->fd, 0);
	if (lp->buffer == ((char *)-1)) {
		nocc_error ("failed to map %s for reading: %s", lf->filename, strerror (errno));
		close (lp->fd);
		sfree (lp);
		lf->priv = NULL;
		return NULL;
	}
	lf->lineno = 1;
	lf->lexer = &occampi_lexer;
	lf->parser = &occampi_parser;
	lf->errcount = 0;
	lf->warncount = 0;
	lf->ppriv = NULL;
	dynarray_init (lf->tokbuffer);
	if (lf->lexer->openfile) {
		lf->lexer->openfile (lf, lp);
	}
	/* oki, ready for lexing..! */
	return lf;
}
/*}}}*/
/*{{{  void lexer_close (lexfile_t *lf)*/
/*
 *	closes a lex file
 */
void lexer_close (lexfile_t *lf)
{
	lexpriv_t *lp = (lexpriv_t *)(lf->priv);

	if (!lf) {
		nocc_internal ("lexer_close() on %s is not open", lf->filename);
		return;
	}
	if (lp->size && lp->buffer && (lp->fd >= 0)) {
		if (lf->lexer && lf->lexer->closefile) {
			lf->lexer->closefile (lf, lp);
		}
		munmap ((void *)(lp->buffer), lp->size);
		close (lp->fd);
	}
	sfree (lp);
	lf->priv = NULL;
	/* leave lf alone for things that refer back to it */
	return;
}
/*}}}*/
/*{{{  token_t *lexer_nexttoken (lexfile_t *lf)*/
/*
 *	tokenises the next bit of the input stream
 */
token_t *lexer_nexttoken (lexfile_t *lf)
{
	lexpriv_t *lp = (lexpriv_t *)(lf->priv);
	token_t *tok;

	if (!lf || !lp || !lf->lexer) {
		return NULL;
	}

	if (DA_CUR (lf->tokbuffer)) {
		int idx = DA_CUR (lf->tokbuffer) - 1;
		/* get one from the buffer */
		tok = DA_NTHITEM (lf->tokbuffer, idx);
		dynarray_delitem (lf->tokbuffer, idx);
	} else {
		tok = lf->lexer->nexttoken (lf, lp);
	}
	return tok;
}
/*}}}*/
/*{{{  void lexer_pushback (lexfile_t *lf, token_t *tok)*/
/*
 *	pushes a token back into the lexer
 */
void lexer_pushback (lexfile_t *lf, token_t *tok)
{
	if (!lf) {
		nocc_internal ("lexer_pushback(): null lexfile");
		return;
	}
	dynarray_add (lf->tokbuffer, tok);
	return;
}
/*}}}*/


/*{{{  token_t *lexer_newtoken (tokentype_t type, ...)*/
/*
 *	creates a new token, usually used for matching
 */
token_t *lexer_newtoken (tokentype_t type, ...)
{
	token_t *tok;
	va_list ap;

	tok = (token_t *)smalloc (sizeof (token_t));
	tok->type = type;
	va_start (ap, type);
	switch (type) {
	case NOTOKEN:
		break;
	case KEYWORD:
		{
			char *name = va_arg (ap, char *);

			if (name) {
				tok->u.kw = keywords_lookup (name, strlen (name));
			} else {
				tok->u.kw = NULL;
			}
		}
		break;
	case INTEGER:
		tok->u.ival = va_arg (ap, int);
		break;
	case REAL:
		tok->u.dval = va_arg (ap, double);
		break;
	case STRING:
		{
			char *str = va_arg (ap, char *);

			if (str) {
				tok->u.str.len = strlen (str);
				tok->u.str.ptr = string_dup (str);
			} else {
				tok->u.str.len = 0;
				tok->u.str.ptr = NULL;
			}
		}
		break;
	case NAME:
		{
			char *name = va_arg (ap, char *);

			if (name) {
				tok->u.name = string_dup (name);
			} else {
				tok->u.name = NULL;
			}
		}
		break;
	case SYMBOL:
		{
			char *sym = va_arg (ap, char *);

			if (sym) {
				tok->u.sym = symbols_lookup (sym, strlen (sym));
			} else {
				tok->u.sym = NULL;
			}
		}
		break;
	case COMMENT:
	case NEWLINE:
	case INDENT:
	case OUTDENT:
	case END:
		break;
	}
	va_end (ap);
	return tok;
}
/*}}}*/
/*{{{  void lexer_dumptoken (FILE *stream, token_t *tok)*/
/*
 *	prints out what a token is (for debugging)
 */
void lexer_dumptoken (FILE *stream, token_t *tok)
{
	fprintf (stream, "<token ");

	if (tok->origin) {
		fprintf (stream, "origin=\"%s:%d\" ", tok->origin->fnptr, tok->lineno);
	}

	fprintf (stream, "type=\"");
	switch (tok->type) {
	case NOTOKEN:
		fprintf (stream, "notoken\" />\n");
		break;
	case KEYWORD:
		fprintf (stream, "keyword\" value=\"%s\" />\n", tok->u.kw->name);
		break;
	case INTEGER:
		fprintf (stream, "integer\" value=\"0x%8.8x\" />\n", (unsigned int)tok->u.ival);
		break;
	case REAL:
		fprintf (stream, "real\" value=\"%lf\" />\n", tok->u.dval);
		break;
	case STRING:
		fprintf (stream, "string\">\n");
		fprintf (stream, "    <![CDATA[%s]]>\n", tok->u.str.ptr);
		fprintf (stream, "</token>\n");
		break;
	case NAME:
		fprintf (stream, "name\" value=\"%s\" />\n", tok->u.name);
		break;
	case SYMBOL:
		fprintf (stream, "symbol\" value=\"%s\" />\n", tok->u.sym->match);
		break;
	case COMMENT:
		fprintf (stream, "comment\" />\n");
		break;
	case NEWLINE:
		fprintf (stream, "newline\" />\n");
		break;
	case INDENT:
		fprintf (stream, "indent\" />\n");
		break;
	case OUTDENT:
		fprintf (stream, "outdent\" />\n");
		break;
	case END:
		fprintf (stream, "end\" />\n");
		break;
	}
	return;
}
/*}}}*/
/*{{{  void lexer_dumptoken_short (FILE *stream, token_t *tok)*/
/*
 *	prints out a token in short form (for debugging)
 */
void lexer_dumptoken_short (FILE *stream, token_t *tok)
{
	fprintf (stream, "<token type=\"");
	switch (tok->type) {
	case NOTOKEN:
		fprintf (stream, "notoken\">");
		break;
	case KEYWORD:
		fprintf (stream, "keyword\" value=\"%s\">", tok->u.kw->name);
		break;
	case INTEGER:
		fprintf (stream, "integer\">");
		break;
	case REAL:
		fprintf (stream, "real\"");
		break;
	case STRING:
		fprintf (stream, "string\">");
		break;
	case NAME:
		fprintf (stream, "name\"");
		break;
	case SYMBOL:
		fprintf (stream, "symbol\" value=\"%s\">", tok->u.sym->match);
		break;
	case COMMENT:
		fprintf (stream, "comment\">");
		break;
	case NEWLINE:
		fprintf (stream, "newline\">");
		break;
	case INDENT:
		fprintf (stream, "indent\">");
		break;
	case OUTDENT:
		fprintf (stream, "outdent\">");
		break;
	case END:
		fprintf (stream, "end\">");
		break;
	}
	return;
}
/*}}}*/
/*{{{  void lexer_freetoken (token_t *tok)*/
/*
 *	frees a token
 */
void lexer_freetoken (token_t *tok)
{
	if (!tok) {
		nocc_internal ("lexer_freetoken: NULL token");
		return;
	}
	switch (tok->type) {
	case NAME:
		if (tok->u.name) {
			sfree (tok->u.name);
		}
		break;
	case STRING:
		if (tok->u.str.ptr) {
			sfree (tok->u.str.ptr);
		}
		break;
	default:
		break;
	}
	memset ((void *)tok, 0, sizeof (token_t));
	sfree (tok);
	return;
}
/*}}}*/


/*{{{  int lexer_tokmatch (token_t *formal, token_t *actual)*/
/*
 *	returns non-zero if "actual" is a match for "formal"
 */
int lexer_tokmatch (token_t *formal, token_t *actual)
{
	if (!formal || !actual) {
		nocc_warning ("lexer_tokmatch(): null token");
		return 0;
	}
	if (formal->type == NOTOKEN) {
		/* anything matches this */
		return 1;
	}
	if (formal->type != actual->type) {
		return 0;
	}
	switch (formal->type) {
	case NOTOKEN:
		return 1;	/* any actual is a match for this */
	case KEYWORD:
		return (formal->u.kw == actual->u.kw);
	case INTEGER:
	case REAL:
	case STRING:
	case NAME:
		/* all these are instant matches */
		return 1;
	case SYMBOL:
		return (formal->u.sym == actual->u.sym);
	case COMMENT:
	case NEWLINE:
	case INDENT:
	case OUTDENT:
	case END:
		/* these are instant matches too */
		return 1;
	}
	return 0;
}
/*}}}*/


/*{{{  void lexer_warning (lexfile_t *lf, char *fmt, ...)*/
/*
 *	generates lexer warnings
 */
void lexer_warning (lexfile_t *lf, char *fmt, ...)
{
	va_list ap;
	static char warnbuf[512];
	int n;

	va_start (ap, fmt);
	n = sprintf (warnbuf, "%s:%d (warning) ", lf->fnptr, lf->lineno);
	vsnprintf (warnbuf + n, 512 - n, fmt, ap);
	va_end (ap);

	nocc_outerrmsg (warnbuf);
	return;
}
/*}}}*/
/*{{{  void lexer_error (lexfile_t *lf, char *fmt, ...)*/
/*
 *	generates lexer errors
 */
void lexer_error (lexfile_t *lf, char *fmt, ...)
{
	va_list ap;
	static char warnbuf[512];
	int n;

	va_start (ap, fmt);
	n = sprintf (warnbuf, "%s:%d (error) ", lf->fnptr, lf->lineno);
	vsnprintf (warnbuf + n, 512 - n, fmt, ap);
	va_end (ap);

	nocc_outerrmsg (warnbuf);
	return;
}
/*}}}*/

