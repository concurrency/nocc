/*
 *	hopp_lexer.c -- lexer for haskell occam-pi parser output
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
#include "occampi.h"
#include "opts.h"


/*}}}*/


/*{{{  forward decls*/

static int hopp_openfile (lexfile_t *lf, lexpriv_t *lp);
static int hopp_closefile (lexfile_t *lf, lexpriv_t *lp);
static token_t *hopp_nexttoken (lexfile_t *lf, lexpriv_t *lp);

/*}}}*/
/*{{{  public lexer struct*/

langlexer_t hopp_lexer = {
	langname: "occam-pi",
	fileexts: {".hopp", NULL},
	openfile: hopp_openfile,
	closefile: hopp_closefile,
	nexttoken: hopp_nexttoken,
	parser: NULL
};


/*}}}*/


/*{{{  static int hopp_openfile (lexfile_t *lf, lexpriv_t *lp)*/
/*
 *	called once a haskell parsed occam-pi file has been opened
 *	returns 0 on success, non-zero on failure
 */
static int hopp_openfile (lexfile_t *lf, lexpriv_t *lp)
{
	lp->langpriv = NULL;
	lf->lineno = 1;

	return 0;
}
/*}}}*/
/*{{{  static int hopp_closefile (lexfile_t *lf, lexpriv_t *lp)*/
/*
 *	called before a haskell parsed occam-pi file is closed
 *	returns 0 on success, non-zero on failure
 */
static int hopp_closefile (lexfile_t *lf, lexpriv_t *lp)
{
	return 0;
}
/*}}}*/
/*{{{  static token_t *hopp_nexttoken (lexfile_t *lf, lexpriv_t *lp)*/
/*
 *	called to retrieve the next token
 */
static token_t *hopp_nexttoken (lexfile_t *lf, lexpriv_t *lp)
{
	token_t *tok;

	if (!lf || !lp) {
		return NULL;
	}

	tok = lexer_newtoken (NOTOKEN);
	tok->origin = (void *)lf;
	tok->lineno = lf->lineno;
	if (lp->offset == lp->size) {
		/* hit EOF */
		tok->type = END;
		return tok;
	}

	return tok;
}
/*}}}*/


