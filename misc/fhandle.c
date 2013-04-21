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
#include <errno.h>

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

STATICSTRINGHASH (fhscheme_t *, schemes, 3);		/* hashed on prefix */
STATICDYNARRAY (fhscheme_t *, aschemes);

static int last_error_code;

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

	fhs->openfcn = NULL;
	fhs->closefcn = NULL;

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
/*{{{  static fhandle_t *fhandle_newfhandle (void)*/
/*
 *	creates a new fhandle_t structure (blank)
 */
static fhandle_t *fhandle_newfhandle (void)
{
	fhandle_t *fhan = (fhandle_t *)smalloc (sizeof (fhandle_t));

	fhan->scheme = NULL;
	fhan->ipriv = NULL;
	fhan->path = NULL;
	fhan->spath = NULL;
	fhan->err = 0;

	return fhan;
}
/*}}}*/
/*{{{  static void fhandle_freefhandle (fhandle_t *fhan)*/
/*
 *	frees a fhandle_t structure (including path if non-null)
 */
static void fhandle_freefhandle (fhandle_t *fhan)
{
	if (!fhan) {
		nocc_serious ("fhandle_freefhandle(): NULL pointer!");
		return;
	}
	if (fhan->path) {
		sfree (fhan->path);
		fhan->path = NULL;
	}
	fhan->spath = NULL;
	sfree (fhan);
	return;
}
/*}}}*/


/*{{{  fhscheme_t *fhandle_newscheme (void)*/
/*
 *	creates a blank fhscheme_t and returns it
 */
fhscheme_t *fhandle_newscheme (void)
{
	return fhandle_newfhscheme ();
}
/*}}}*/
/*{{{  void fhandle_freescheme (fhscheme_t *sptr)*/
/*
 *	trashes a fhscheme_t structure
 */
void fhandle_freescheme (fhscheme_t *sptr)
{
	fhandle_freefhscheme (sptr);
}
/*}}}*/


/*{{{  int fhandle_registerscheme (fhscheme_t *scheme)*/
/*
 *	called to register a file-handling scheme
 *	returns 0 on success, non-zero on failure
 */
int fhandle_registerscheme (fhscheme_t *scheme)
{
	fhscheme_t *ext;

	ext = stringhash_lookup (schemes, scheme->sname);
	if (ext) {
		nocc_serious ("fhandle_registerscheme(): for [%s], already registered!", scheme->prefix);
		return -1;
	}

	stringhash_insert (schemes, scheme, scheme->prefix);
	dynarray_add (aschemes, scheme);

	if (compopts.verbose) {
		nocc_message ("registering file-handler for [%s] (%s)", scheme->prefix, scheme->sname);
	}

	return 0;
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


/*{{{  static int fhandle_seterr (fhandle_t *fh, int err)*/
/*
 *	locally sets the last error in global and handle
 *	returns same parameter.
 */
static int fhandle_seterr (fhandle_t *fh, int err)
{
	last_error_code = err;
	if (fh) {
		fh->err = err;
	}
	return err;
}
/*}}}*/


/*{{{  fhandle_t *fhandle_fopen (const char *path, const char *mode)*/
/*
 *	opens a file.  'mode' should be fopen(3) style, "r", "w+", etc. with explicit text/binary suffix first if present
 *	returns a new file-handle or NULL
 */
fhandle_t *fhandle_fopen (const char *path, const char *mode)
{
	int mmode = 0;
	int mperm = 0;
	char *ch = (char *)mode;

	if ((*ch == 't') || (*ch == 'b')) {
		/* not relevant on unix */
		ch++;
	}

	if (!strcmp (ch, "r")) {
		mmode = O_RDONLY;
	} else if (!strcmp (ch, "r+")) {
		mmode = O_RDWR;
	} else if (!strcmp (ch, "w")) {
		mmode = O_WRONLY | O_CREAT | O_TRUNC;
		mperm = 0644;
	} else if (!strcmp (ch, "w+")) {
		mmode = O_RDWR | O_CREAT | O_TRUNC;
		mperm = 0644;
	} else if (!strcmp (ch, "a")) {
		mmode = O_WRONLY | O_APPEND | O_CREAT;
		mperm = 0644;
	} else if (!strcmp (ch, "a+")) {
		mmode = O_RDWR | O_APPEND | O_CREAT;
		mperm = 0644;
	} else {
		nocc_serious ("fhandle_fopen(): unknown mode string \"%s\" for \"%s\"", mode, path);
		return NULL;
	}

	return fhandle_open (path, mmode, mperm);
}
/*}}}*/
/*{{{  fhandle_t *fhandle_open (const char *path, const int mode, const int perm)*/
/*
 *	opens a file.  'mode' should be open(2) style constants (O_...); 'perm' only meaningful if O_CREAT included.
 *	returns a new file-handle of NULL
 */
fhandle_t *fhandle_open (const char *path, const int mode, const int perm)
{
	fhandle_t *fhan;
	char *ch;
	fhscheme_t *scheme;
	int poffs, err;

	for (ch=(char *)path; (*ch != '\0') && (*ch != ':'); ch++);
	if (*ch == '\0') {
		/* none specified, try default */
		scheme = stringhash_lookup (schemes, "file://");
		if (!scheme) {
			nocc_serious ("fhandle_open(): no \"file://\" scheme!  cannot open \"%s\"", path);
			fhandle_seterr (NULL, -ENOSYS);
			return NULL;
		}
		poffs = 0;
	} else if ((ch[1] == '/') && (ch[2] == '/')) {
		char *pfx = string_ndup (path, (int)(ch - path) + 3);

		scheme = stringhash_lookup (schemes, pfx);
		sfree (pfx);
		if (!scheme) {
			nocc_serious ("fhandle_open(): no scheme registered for \"%s\"", path);
			fhandle_seterr (NULL, -ENOSYS);
			return NULL;
		}
		poffs = (int)(ch - path) + 3;
	} else {
		/* was probably part of the filename, odd, but try anyway */
		scheme = stringhash_lookup (schemes, "file://");
		if (!scheme) {
			nocc_serious ("fhandle_open(): no \"file://\" scheme!  cannot open \"%s\"", path);
			fhandle_seterr (NULL, -ENOSYS);
			return NULL;
		}
		poffs = 0;
	}

	fhan = fhandle_newfhandle ();
	fhan->scheme = scheme;
	fhan->path = string_dup (path);
	fhan->spath = fhan->path + poffs;

	err = scheme->openfcn (fhan, mode, perm);
	fhandle_seterr (fhan, err);
	if (err) {
		/* failed */
		fhandle_freefhandle (fhan);
		return NULL;
	}
	return fhan;
}
/*}}}*/
/*{{{  int fhandle_close (fhandle_t *fh)*/
/*
 *	closes a file-handle.
 *	returns 0 on success, non-zero on failure.
 */
int fhandle_close (fhandle_t *fh)
{
	int err;

	if (!fh) {
		return fhandle_seterr (fh, -EINVAL);
	} else if (!fh->scheme) {
		return fhandle_seterr (fh, -ENOSYS);
	}
	err = fh->scheme->closefcn (fh);
	fhandle_seterr (fh, err);

	if (!err) {
		/* trash handle */
		fhandle_freefhandle (fh);
	}
	return err;
}
/*}}}*/
/*{{{  int fhandle_lasterr (fhandle_t *fh)*/
/*
 *	gets the most recent error (or ESUCCESS)
 */
int fhandle_lasterr (fhandle_t *fh)
{
	if (fh) {
		return fh->err;
	}
	return last_error_code;
}
/*}}}*/
/*{{{  unsigned char *fhandle_mapfile (fhandle_t *fh, size_t offset, size_t length)*/
/*
 *	memory-maps a file.  offset and length ought to be sensible according to mmap(2).
 *	returns mapped pointer on success, NULL on failure.
 */
unsigned char *fhandle_mapfile (fhandle_t *fh, size_t offset, size_t length)
{
	unsigned char *ptr;
	int err;

	if (!fh) {
		fhandle_seterr (fh, -EINVAL);
		return NULL;
	} else if (!fh->scheme) {
		fhandle_seterr (fh, -ENOSYS);
		return NULL;
	}

	err = fh->scheme->mapfcn (fh, &ptr, offset, length);
	if (err) {
		fhandle_seterr (fh, err);
		return NULL;
	}

	return ptr;
}
/*}}}*/
/*{{{  int fhandle_unmapfile (fhandle_t *fh, unsigned char *ptr, size_t offset, size_t length)*/
/*
 *	un-memory-maps a file.  offset and length should be the same as used with fhandle_mapfile(), and 'ptr' the correct pointer.
 *	returns 0 on success, non-zero on error.
 */
int fhandle_unmapfile (fhandle_t *fh, unsigned char *ptr, size_t offset, size_t length)
{
	int err;

	if (!fh) {
		return fhandle_seterr (fh, -EINVAL);
	} else if (!fh->scheme) {
		return fhandle_seterr (fh, -ENOSYS);
	}

	err = fh->scheme->unmapfcn (fh, ptr, offset, length);
	fhandle_seterr (fh, err);

	return err;
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

	last_error_code = 0;

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



