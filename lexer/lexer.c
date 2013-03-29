/*
 *	lexer.c -- nocc lexer
 *	Copyright (C) 2004-2007 Fred Barnes <frmb@kent.ac.uk>
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
#include "origin.h"
#include "symbols.h"
#include "keywords.h"
#include "lexer.h"
#include "lexpriv.h"
#include "parser.h"
#include "fhandle.h"

/*}}}*/


/*{{{  private data*/

STATICDYNARRAY (langlexer_t *, langlexers);

STATICDYNARRAY (lexfile_t *, lexfiles);
STATICDYNARRAY (lexfile_t *, openlexfiles);

/*}}}*/


/*{{{  int lexer_init (void)*/
/*
 *	initialises the lexer
 *	returns 0 on success, non-zero on failure
 */
int lexer_init (void)
{
	dynarray_init (langlexers);
	dynarray_init (lexfiles);
	dynarray_init (openlexfiles);
	return 0;
}
/*}}}*/
/*{{{  int lexer_shutdown (void)*/
/*
 *	shuts-down the lexer
 *	returns 0 on success, non-zero on failure
 */
int lexer_shutdown (void)
{
	int i;

	dynarray_trash (langlexers);
	for (i=DA_CUR (openlexfiles) - 1; i >= 0; i--) {
		lexfile_t *olf = DA_NTHITEM (openlexfiles, i);

		lexer_close (olf);
	}
	dynarray_trash (openlexfiles);
	dynarray_trash (lexfiles);
	return 0;
}
/*}}}*/


/*{{{  int lexer_relpathto (const char *filename, char *target, int tsize)*/
/*
 *	determines the current relative path to a particular file-name, base on something that we can read
 *	puts the resulting path in 'target'
 *	returns 0 on success, non-zero on failure
 */
int lexer_relpathto (const char *filename, char *target, int tsize)
{
	/* check for current directory first */
	if (!access (filename, R_OK)) {
		/* this one will do */
		if (strlen (filename) >= tsize) {
			/* too big! */
			return -1;
		}
		strcpy (target, filename);
		return 0;
	}

	if (!DA_CUR (openlexfiles)) {
		/* nothing previous to search, assume this one */
		if (strlen (filename) >= tsize) {
			/* too big! */
			return -1;
		}
		strcpy (target, filename);
	} else {
		char fnbuf[FILENAME_MAX];
		int fnlen = 0;
		lexfile_t *lastopen = DA_NTHITEM (openlexfiles, DA_CUR (openlexfiles) - 1);

#if 0
fprintf (stderr, "lexer_relpathto(): lastopen->filename=[%s] @0x%8.8x, lastopen->fnptr=[%s] @0x%8.8x\n", lastopen->filename, (unsigned int)lastopen->filename, lastopen->fnptr, (unsigned int)lastopen->fnptr);
#endif
		/* see if there's a path in it, if so, copy it */
		if ((lastopen->fnptr > lastopen->filename) && (*filename != '/')) {
			int plen = (int)(lastopen->fnptr - lastopen->filename);

			if (plen > (FILENAME_MAX - 3)) {
				nocc_error ("path too long..?!");
				return -1;
			}
			strncpy (fnbuf, lastopen->filename, plen);
			fnlen = plen;
			fnbuf[fnlen] = '\0';
		}

		fnlen += snprintf (fnbuf + fnlen, FILENAME_MAX-(fnlen + 1), "%s", filename);

		if (!access (fnbuf, R_OK)) {
			/* can read this, use it */
			if (fnlen >= tsize) {
				/* too big! */
				return -1;
			}
			strcpy (target, fnbuf);
		} else {
			/* default */
			if (strlen (filename) >= tsize) {
				/* too big! */
				return -1;
			}
			strcpy (target, filename);
		}
	}
	return 0;
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
	char fnbuf[FILENAME_MAX];
	int fnlen = 0;
	int langlexidx = -1;

#if 0
fprintf (stderr, "lexer_open(): filename=[%s], DA_CUR(openlexfiles)=%d\n", filename, DA_CUR (openlexfiles));
#endif
	/* if a path isn't specified, do a search based on extension (relative current directory first) */
	fextn = NULL;

	if (lexer_relpathto (filename, fnbuf, FILENAME_MAX)) {
		/* failed somewhere, default */
	} else {
		fnlen = strlen (fnbuf);
	}

	if (access (fnbuf, R_OK)) {
		/*{{{  search through include and library directories*/
		for (i=0; i<DA_CUR (compopts.ipath); i++) {
			char *ipath = DA_NTHITEM (compopts.ipath, i);

			fnlen = snprintf (fnbuf, FILENAME_MAX - 1, "%s/", ipath);
			fnlen += snprintf (fnbuf + fnlen, FILENAME_MAX - (fnlen + 1), "%s", filename);

			if (!access (fnbuf, R_OK)) {
				break;		/* for() */
			}
		}
		if (i < DA_CUR (compopts.ipath)) {
			/* found in the includes */
		} else {
			/* search through library directories */
			for (i=0; i<DA_CUR (compopts.lpath); i++) {
				char *lpath = DA_NTHITEM (compopts.lpath, i);

				fnlen = snprintf (fnbuf, FILENAME_MAX - 1, "%s/", lpath);
				fnlen += snprintf (fnbuf + fnlen, FILENAME_MAX - (fnlen + 1), "%s", filename);

				if (!access (fnbuf, R_OK)) {
					break;		/* for() */
				}
			}
			if (i == DA_CUR (compopts.lpath)) {
				nocc_error ("unable to access %s for reading: %s", fnbuf, strerror (errno));
				return NULL;
			}
		}
		/*}}}*/
	}

	/*{{{  extract extension from filename and set "langlexidx"*/
	for (fextn = (char *)fnbuf + (fnlen - 1); (fextn > (char *)fnbuf) && (*fextn != '.'); fextn--);
	/* first, check to see if any of the already-open lexers support this one */
	for (i=0; i<DA_CUR (openlexfiles); i++) {
		int j;
		langlexer_t *ollex = (DA_NTHITEM (openlexfiles, i))->lexer;

		for (j=0; ollex->fileexts[j]; j++) {
			if (!strcmp (ollex->fileexts[j], fextn)) {
				/* this one */
				int k;

#if 0
fprintf (stderr, "lexer_open(): openlexfile[%d] has supported extension \"%s\"\n", i, fextn);
#endif
				for (k=0; k<DA_CUR (langlexers); k++) {
					if (DA_NTHITEM (langlexers, k) == ollex) {
						langlexidx = k;
						break;		/* for(k) */
					}
				}
				break;		/* for(j) */
			}
		}
		if (langlexidx >= 0) {
			break;		/* for(i) */
		}
	}
	for (i=0; (langlexidx < 0) && (i<DA_CUR (langlexers)); i++) {
		int j;
		langlexer_t *llex = DA_NTHITEM (langlexers, i);

		for (j=0; llex->fileexts[j]; j++) {
			if (!strcmp (llex->fileexts[j], fextn)) {
				/* this one will do! */
				langlexidx = i;
				break;		/* for() */
			}
		}
		if (langlexidx >= 0) {
			break;		/* for() */
		}
	}
	if (langlexidx < 0) {
		nocc_error ("failed to find lexer for %s", fnbuf);
		return NULL;
	}

	/*}}}*/
	/*{{{  already know about this one ?*/
	for (i=0; i<DA_CUR(lexfiles); i++) {
		lf = DA_NTHITEM (lexfiles, i);
		if (!strcmp (lf->filename, fnbuf)) {
			break;
		}
	}
	if (i == DA_CUR (lexfiles)) {
		lf = (lexfile_t *)smalloc (sizeof (lexfile_t));

		lf->filename = string_dup (fnbuf);
		for (lf->fnptr = lf->filename + (strlen (lf->filename) - 1); (lf->fnptr > lf->filename) && ((lf->fnptr)[-1] != '/'); (lf->fnptr)--);

		lf->priv = NULL;
		lf->lineno = 0;
		dynarray_add (lexfiles, lf);

		lf->toplevel = 0;
		lf->islibrary = 0;
		lf->sepcomp = 0;
	}
	if (lf->priv) {
		nocc_error ("%s is already open!", lf->filename);
		return NULL;
	}
	if (stat (lf->filename, &stbuf)) {
		nocc_error ("failed to stat %s: %s", lf->filename, strerror (errno));
		return NULL;
	}

	/*}}}*/
	/*{{{  open it*/
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
	lf->lexer = DA_NTHITEM (langlexers, langlexidx);		/* &occampi_lexer; */
	lf->parser = lf->lexer->parser;					/* &occampi_parser; */
	lf->errcount = 0;
	lf->warncount = 0;
	lf->ppriv = NULL;
	dynarray_init (lf->tokbuffer);
	if (lf->lexer->openfile) {
		lf->lexer->openfile (lf, lp);
	}
	/*}}}*/
	/* oki, ready for lexing..! */
	dynarray_add (openlexfiles, lf);

	return lf;
}
/*}}}*/
/*{{{  lexfile_t *lexer_openbuf (char *fname, char *langname, char *buf)*/
/*
 *	"opens" a buffer for lexing -- a copy of the buffer is made
 *	returns lexer-handle on success, NULL on failure
 */
lexfile_t *lexer_openbuf (char *fname, char *langname, char *buf)
{
	lexfile_t *lf = NULL;
	lexpriv_t *lp;
	int langlexidx = -1;

	if (fname && !langname) {
		/*{{{  try and extract an extension to work out which language we should use*/
		char *fextn;
		int i;

		for (fextn = fname + (strlen (fname) - 1); (fextn > fname) && (*fextn != '.'); fextn--);
		for (i=0; i<DA_CUR (langlexers); i++) {
			int j;
			langlexer_t *llex = DA_NTHITEM (langlexers, i);

			for (j=0; llex->fileexts[j]; j++) {
				if (!strcmp (llex->fileexts[j], fextn)) {
					langlexidx = i;
					break;		/* for() */
				}
			}
			if (langlexidx >= 0) {
				break;		/* for() */
			}
		}
		if (langlexidx < 0) {
			nocc_error ("failed to find lexer for buffer from %s", fname);
			return NULL;
		}
		/*}}}*/
	} else if (langname) {
		/*{{{  find a lexer from language name*/
		int i;

		for (i=0; i<DA_CUR (langlexers); i++) {
			langlexer_t *llex = DA_NTHITEM (langlexers, i);

			if (!strcmp (langname, llex->langname)) {
				langlexidx = i;
				break;		/* for() */
			}
		}
		if (langlexidx < 0) {
			nocc_error ("no lexer for language %s", langname);
			return NULL;
		}
		/*}}}*/
	} else {
		/*{{{  erm, fail!*/
		nocc_internal ("lexer_openbuf(): no fname or langname");
		return NULL;
		/*}}}*/
	}

	/*{{{  create the lexfile_t for this buffer*/
	lf = (lexfile_t *)smalloc (sizeof (lexfile_t));
	lf->filename = fname ? string_dup (fname) : string_dup ("(unknown buffer)");

	for (lf->fnptr = lf->filename + (strlen (lf->filename) - 1); (lf->fnptr > lf->filename) && ((lf->fnptr)[-1] != '/'); (lf->fnptr)--);

	lf->priv = NULL;
	lf->lineno = 0;

	dynarray_add (lexfiles, lf);

	/*}}}*/
	/*{{{  do a pseudo-open on it*/
	lp = (lexpriv_t *)smalloc (sizeof (lexpriv_t));
	lf->priv = (void *)lp;
	lp->fd = -1;
	lp->size = strlen (buf);
	lp->buffer = string_dup (buf);
	lp->offset = 0;

	lf->lineno = 1;
	lf->lexer = DA_NTHITEM (langlexers, langlexidx);
	lf->parser = lf->lexer->parser;
	lf->errcount = 0;
	lf->warncount = 0;
	lf->ppriv = NULL;
	lf->toplevel = 0;
	lf->islibrary = 0;
	lf->sepcomp = 0;

	dynarray_init (lf->tokbuffer);
	if (lf->lexer->openfile) {
		lf->lexer->openfile (lf, lp);
	}
	/*}}}*/
	/* ready for lexing :) */
	dynarray_add (openlexfiles, lf);

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
	lexfile_t *lastopen;

	if (!lp) {
		nocc_internal ("lexer_close() on %s is not open", lf->filename);
		return;
	}

	/* make sure it was the last thing opened! */
	if (!DA_CUR (openlexfiles)) {
		nocc_internal ("lexer_close(): no open files!");
		return;
	}
	lastopen = DA_NTHITEM (openlexfiles, DA_CUR (openlexfiles) - 1);
	if (lastopen != lf) {
		nocc_internal ("lexer_close(): closing [%s], but [%s] was last opened", lf->filename, lastopen->filename);
		return;
	}
	DA_SETNTHITEM (openlexfiles, DA_CUR (openlexfiles) - 1, NULL);
	dynarray_delitem (openlexfiles, DA_CUR (openlexfiles) - 1);

	
	if (lp->size && lp->buffer && (lp->fd >= 0)) {
		if (lf->lexer && lf->lexer->closefile) {
			lf->lexer->closefile (lf, lp);
		}
		munmap ((void *)(lp->buffer), lp->size);
		close (lp->fd);
	} else if (lp->size && lp->buffer) {
		/* must have been a buffer, free it */
		sfree (lp->buffer);
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
/*{{{  int lexer_getcodeline (lexfile_t *lf, char **rbuf)*/
/*
 *	gets the current input line in the lexer (returns fresh string in '*rbuf')
 *	returns 0 on success, non-zero on failure
 */
int lexer_getcodeline (lexfile_t *lf, char **rbuf)
{
	lexpriv_t *lp = (lexpriv_t *)(lf->priv);
	char *lrptr = NULL;
	int i;

	if (!lf || !lp || !lf->lexer) {
		*rbuf = string_dup ("");
		return 0;
	}
	if (!lf->lexer->getcodeline) {
		*rbuf = string_dup ("");
		return 0;
	}
	i = lf->lexer->getcodeline (lf, lp, &lrptr);
	if (!lrptr || i) {
		*rbuf = string_dup ("");
		if (lrptr) {
			sfree (lrptr);
		}
		return 0;
	}

	*rbuf = lrptr;
	return 0;
}
/*}}}*/
/*{{{  char *lexer_filenameof (lexfile_t *lf)*/
/*
 *	returns the filename associated with a lexer
 */
char *lexer_filenameof (lexfile_t *lf)
{
	if (!lf) {
		return "(none)";
	}
	return lf->fnptr;
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
				tok->u.kw = keywords_lookup (name, strlen (name), 0);
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
	case INAME:
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
	case LSPECIAL:
		{
			void *ptr = va_arg (ap, void *);

			tok->u.lspec = ptr;
		}
		break;
	case SYMBOL:
		{
			char *sym = va_arg (ap, char *);

			if (sym) {
				tok->u.sym = symbols_lookup (sym, strlen (sym), 0);
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
	if (tok->iptr) {
		fprintf (stream, "iptr=\"0x%8.8x\" ", (unsigned int)tok->iptr);
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
	case INAME:
		fprintf (stream, "iname\" value=\"%s\" />\n", tok->u.str.ptr);
		break;
	case LSPECIAL:
		fprintf (stream, "lspecial\" value=\"%p\" />\n", tok->u.lspec);
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
	if (!tok) {
		fprintf (stream, "<** NULL TOKEN **>");
		return;
	}

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
	case INAME:
		fprintf (stream, "iname\">");
		break;
	case LSPECIAL:
		fprintf (stream, "lspecial\">");
		break;
	case NAME:
		fprintf (stream, "name\">");
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
/*{{{  char *lexer_stokenstr (token_t *tok)*/
/*
 *	puts a token string in a local static buffer (error reporting)
 */
char *lexer_stokenstr (token_t *tok)
{
	static char *strbuf = NULL;
	static int strbuflen = 0;

	if (!strbuf) {
		strbuf = (char *)smalloc (128);
		strbuflen = 128;
	}

	switch (tok->type) {
	case NOTOKEN:
		strcpy (strbuf, "*UNKNOWN*");
		break;
	case KEYWORD:
		if (strlen (tok->u.kw->name) >= strbuflen) {
			strbuflen = strlen (tok->u.kw->name) + 128;
			if (strbuf) {
				sfree (strbuf);
			}
			strbuf = (char *)smalloc (strbuflen);
		}
		strcpy (strbuf, tok->u.kw->name);
		break;
	case INTEGER:
		strcpy (strbuf, "integer constant");
		break;
	case REAL:
		strcpy (strbuf, "real constant");
		break;
	case STRING:
		strcpy (strbuf, "string constant");
		break;
	case INAME:
	case NAME:
		strcpy (strbuf, "name");
		break;
	case LSPECIAL:
		strcpy (strbuf, "language special");
		break;
	case SYMBOL:
		if (tok->u.sym->mlen >= (strbuflen - 2)) {
			strbuflen = tok->u.sym->mlen + 128;
			if (strbuf) {
				sfree (strbuf);
			}
			strbuf = (char *)smalloc (strbuflen);
		}
		strbuf[0] = '\'';
		strncpy (strbuf + 1, tok->u.sym->match, tok->u.sym->mlen);
		strbuf[tok->u.sym->mlen + 1] = '\'';
		strbuf[tok->u.sym->mlen + 2] = '\0';
		break;
	case COMMENT:
		strcpy (strbuf, "comment");
		break;
	case NEWLINE:
		strcpy (strbuf, "newline");
		break;
	case INDENT:
		strcpy (strbuf, "indent");
		break;
	case OUTDENT:
		strcpy (strbuf, "outdent");
		break;
	case END:
		strcpy (strbuf, "end of file");
		break;
	}

	return strbuf;
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
	case INAME:
		if (tok->u.str.ptr) {
			sfree (tok->u.str.ptr);
		}
		break;
	case LSPECIAL:
		if (tok->origin && tok->origin->lexer && tok->origin->lexer->freelspecial) {
			tok->origin->lexer->freelspecial (tok->origin, tok->u.lspec);
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
	case LSPECIAL:
		/* all these are instant matches */
		return 1;
	case INAME:
		/* both sides must have a name set, and must match */
		if (!formal->u.str.ptr || !actual->u.str.ptr) {
			nocc_internal ("lexer_tokmatch(): INAME without string set");
			return 0;
		}
		if ((formal->u.str.len == actual->u.str.len) && !strcmp (formal->u.str.ptr, actual->u.str.ptr)) {
			return 1;
		}
		return 0;
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
/*{{{  int lexer_tokmatchlitstr (token_t *actual, const char *str)*/
/*
 *	returns non-zero if "actual" is some string/name/symbol and matches "str"
 */
int lexer_tokmatchlitstr (token_t *actual, const char *str)
{
	if (!actual) {
		nocc_warning ("lexer_tokmatchlitstr(): null token");
		return 0;
	}
	switch (actual->type) {
	case NOTOKEN:
	case INTEGER:
	case REAL:
	case COMMENT:
	case NEWLINE:
	case INDENT:
	case OUTDENT:
	case END:
	case LSPECIAL:
		return 0;
	case SYMBOL:
		if (actual->u.sym->mlen != strlen (str)) {
			return 0;
		}
		return !memcmp (str, actual->u.sym->match, actual->u.sym->mlen);
	case KEYWORD:
		return !strcmp (str, actual->u.kw->name);
	case NAME:
		return !strcmp (str, actual->u.name);
	case INAME:
	case STRING:
		if (strlen (str) == actual->u.str.len) {
			return !strncmp (str, actual->u.str.ptr, actual->u.str.len);
		}
		return 0;
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
	lf->warncount++;

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
	lf->errcount++;

	nocc_outerrmsg (warnbuf);
	return;
}
/*}}}*/


/*{{{  int lexer_registerlang (langlexer_t *ll)*/
/*
 *	registers a language for lexing
 *	returns 0 on success, non-zero on failure
 */
int lexer_registerlang (langlexer_t *ll)
{
	int i;

	for (i=0; i<DA_CUR (langlexers); i++) {
		langlexer_t *llex = DA_NTHITEM (langlexers, i);

		if (ll == llex) {
			nocc_warning ("lexer_registerlang(): lexer for [%s] already registered", ll->langname);
			return -1;
		}
	}
	dynarray_add (langlexers, ll);

	return 0;
}
/*}}}*/
/*{{{  int lexer_unregisterlang (langlexer_t *ll)*/
/*
 *	unregisters a language for lexing
 *	returns 0 on success, non-zero on failure
 */
int lexer_unregisterlang (langlexer_t *ll)
{
	int i;

	for (i=0; i<DA_CUR (langlexers); i++) {
		langlexer_t *llex = DA_NTHITEM (langlexers, i);

		if (ll == llex) {
			dynarray_delitem (langlexers, i);
			return 0;
		}
	}
	nocc_warning ("lexer_unregisterlang(): lexer for [%s] is not registered", ll->langname);
	return -1;
}
/*}}}*/


/*{{{  void lexer_dumplexers (FILE *stream)*/
/*
 *	dumps registered lexers
 */
void lexer_dumplexers (FILE *stream)
{
	int i;

	for (i=0; i<DA_CUR (langlexers); i++) {
		langlexer_t *llex = DA_NTHITEM (langlexers, i);
		int j;

		fprintf (stream, "name = \"%s\", bits = 0x%8.8x, fexts = [", llex->langname, llex->langtag);
		for (j=0; llex->fileexts[j]; j++) {
			fprintf (stream, "%s%s", j ? "," : "", llex->fileexts[j]);
		}
		fprintf (stream, "]\n");
	}
	return;
}
/*}}}*/
/*{{{  langlexer_t **lexer_getlanguages (int *nlangs)*/
/*
 *	gets the array of registered language lexers
 */
langlexer_t **lexer_getlanguages (int *nlangs)
{
	*nlangs = DA_CUR (langlexers);
	return DA_PTR (langlexers);
}
/*}}}*/


