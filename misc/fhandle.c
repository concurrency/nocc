/*
 *	fhandle.c -- file-handling abstraction for NOCC
 *	Copyright (C) 2013 Fred Barnes <frmb@kent.ac.uk>
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
#include <fcntl.h>

#include "nocc.h"
#include "support.h"
#include "version.h"
#include "lexer.h"
#include "lexpriv.h"
#include "parser.h"
#include "parsepriv.h"
#include "fhandle.h"
#include "fhandlepriv.h"


/*}}}*/
/*{{{  private types/data*/

STATICSTRINGHASH (fhscheme_t *, schemes, 3);
STATICDYNARRAY (fhscheme_t *, aschemes);


/*}}}*/


/*{{{  static fhscheme_t *fhandle_newfhscheme (void)*/
/*
 *	creates a new fhscheme_t structure
 */
static fhscheme_t *fhandle_newfhscheme (void)
{
	fhscheme_t *fhs = (fhscheme_t *)smalloc (sizeof (fhscheme_t));

	fhs->sname = NULL;
	fhs->sdesc = NULL;
	fhs->prefix = NULL;

	fhs->spriv = NULL;
	fhs->usecount = 0;

	return fhs;
}
/*}}}*/
/*{{{  static void fhandle_freefhscheme (fhscheme_t *fhs)*/
/*
 *	frees a fhscheme_t structure
 */
static void fhandle_freefhscheme (fhscheme_t *fhs)
{
	if (!fhs) {
		nocc_serious ("fhandle_freefhscheme(): NULL pointer");
		return;
	}
	if (fhs->sname) {
		sfree (fhs->sname);
	}
	if (fhs->sdesc) {
		sfree (fhs->sdesc);
	}
	if (fhs->prefix) {
		sfree (fhs->prefix);
	}

	sfree (fhs);
	return;
}
/*}}}*/


/*{{{  int fhandle_registerscheme (fhscheme_t *scheme)*/
/*
 *	called to register a file-handling scheme
 *	returns 0 on success, non-zero on failure
 */
int fhandle_registerscheme (fhscheme_t *scheme)
{
	return -1;
}
/*}}}*/
/*{{{  int fhandle_unregisterscheme (fhscheme_t *scheme)*/
/*
 *	called to unregister a file-handling scheme
 *	returns 0 on success, non-zero on failure
 */
int fhandle_unregisterscheme (fhscheme_t *scheme)
{
	nocc_serious ("fhandle_unregisterscheme(): unimplemented!");
	return -1;
}
/*}}}*/


/*{{{  int fhandle_init (void)*/
/*
 *	called to initialise file-handling parts of the compiler (done early)
 *	returns 0 on success, non-zero on failure
 */
int fhandle_init (void)
{
	stringhash_sinit (schemes);
	dynarray_init (aschemes);

	return 0;
}
/*}}}*/
/*{{{  int fhandle_shutdown (void)*/
/*
 *	called to shut-down file-handling parts of the compiler
 *	returns 0 on success, non-zero on failure
 */
int fhandle_shutdown (void)
{
	int i;
	
	for (i=0; i<DA_CUR (aschemes); i++) {
		fhscheme_t *fhs = DA_NTHITEM (aschemes, i);

		if (fhs) {
			fhandle_freefhscheme (fhs);
		}
	}
	dynarray_trash (aschemes);
	stringhash_trash (schemes);

	return 0;
}
/*}}}*/



