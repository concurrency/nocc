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
#include <stdarg.h>
#include <ctype.h>

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

typedef enum ENUM_str_style {
	SSTYLE_NONE = 0,
	SSTYLE_XML = 1,
	SSTYLE_NSNAME = 2,
	SSTYLE_ATTR = 3,
	SSTYLE_STR = 4,
	SSTYLE_PTR = 5,
	SSTYLE_COMMENT = 6
} str_style_e;

/*}}}*/


/*{{{  static int str_xml_hi (char *str, str_style_e style)*/
/*
 *	dumps out an ANSI formatting wotsit for syntax highlighting of XML output
 *	returns number of bytes written to the string
 */
static int str_xml_hi (char *str, str_style_e style)
{
	switch (style) {
	case SSTYLE_NONE:	/* style off */
		return sprintf (str, "%c[0m", 27);
	case SSTYLE_XML:	/* for the '<abc' and '/>' bits, or </abc> -- CYAN */
		return sprintf (str, "%c[36m", 27);
	case SSTYLE_NSNAME:	/* for a namespace name "ns" in '<ns:abc', etc. -- MAGENTA */
		return sprintf (str, "%c[35m", 27);
	case SSTYLE_ATTR:	/* start of attribute name -- GREEN */
		return sprintf (str, "%c[32m", 27);
	case SSTYLE_STR:	/* string or quotes for -- RED */
		return sprintf (str, "%c[31m", 27);
	case SSTYLE_PTR:	/* pointer value -- YELLOW */
		return sprintf (str, "%c[33m", 27);
	case SSTYLE_COMMENT:	/* comment -- BLUE */
		return sprintf (str, "%c[34m", 27);
	}
	return 0;
}
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
	fhs->accessfcn = NULL;
	fhs->mkdirfcn = NULL;
	fhs->statfcn = NULL;
	fhs->mapfcn = NULL;
	fhs->unmapfcn = NULL;
	fhs->printffcn = NULL;
	fhs->writefcn = NULL;
	fhs->readfcn = NULL;
	fhs->getsfcn = NULL;
	fhs->flushfcn = NULL;
	fhs->isattyfcn = NULL;

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

	ext = stringhash_lookup (schemes, scheme->prefix);
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
	fhscheme_t *ext;

	ext = stringhash_lookup (schemes, scheme->prefix);
	if (!ext) {
		nocc_serious ("fhandle_unregisterscheme(): for [%s], not registered!", scheme->prefix);
		return -1;
	}
	if (ext != scheme) {
		nocc_serious ("fhandle_unregisterscheme(): for [%s], registered as something else.", scheme->prefix);
		return -1;
	}

	/* remove */
	stringhash_remove (schemes, ext, ext->prefix);
	dynarray_rmitem (aschemes, ext);

	if (compopts.verbose) {
		nocc_message ("unregistering file-handler for [%s] (%s)", ext->prefix, ext->sname);
	}

	return 0;
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
/*{{{  static fhscheme_t *fhandle_lookupscheme (const char *path, int *poffs)*/
/*
 *	returns a scheme associated with a particular path, or the default scheme
 */
static fhscheme_t *fhandle_lookupscheme (const char *path, int *poffs)
{
	char *ch;
	fhscheme_t *scheme;

	for (ch=(char *)path; (*ch != '\0') && (*ch != ':'); ch++);
	if (*ch == '\0') {
		/* none specified, try default */
		scheme = stringhash_lookup (schemes, "file://");
		if (poffs) {
			*poffs = 0;
		}
	} else if ((ch[1] == '/') && (ch[2] == '/')) {
		char *pfx = string_ndup (path, (int)(ch - path) + 3);

		scheme = stringhash_lookup (schemes, pfx);
		sfree (pfx);
		if (poffs) {
			*poffs = (int)(ch - path) + 3;
		}
	} else {
		/* was probably part of the filename, odd, but try anyway */
		scheme = stringhash_lookup (schemes, "file://");
		if (poffs) {
			*poffs = 0;
		}
	}

	return scheme;
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
	char *mcopy = string_dup (mode);
	char *ch = mcopy;
	int mlen = strlen (mcopy);

	if ((*ch == 't') || (*ch == 'b')) {
		/* not relevant on unix */
		ch++;
		mlen--;
	} else if ((ch[mlen - 1] == 't') || (ch[mlen - 1] == 'b')) {
		/* ditto */
		ch[mlen - 1] = '\0';
		mlen--;
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
		fhandle_seterr (NULL, -EINVAL);
		sfree (mcopy);
		return NULL;
	}
	sfree (mcopy);

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

	scheme = fhandle_lookupscheme (path, &poffs);
	if (!scheme) {
		nocc_serious ("fhandle_open(): failed to find a scheme to handle \"%s\"", path);
		fhandle_seterr (NULL, -ENOSYS);
		return NULL;
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
/*{{{  int fhandle_access (const char *path, const int amode)*/
/*
 *	tests for accessibility of a particular file (F_OK, R_OK, W_OK, X_OK).
 *	returns 0 on success, non-zero on failure.
 */
int fhandle_access (const char *path, const int amode)
{
	fhscheme_t *scheme;
	int err, poffs;

	scheme = fhandle_lookupscheme (path, &poffs);
	if (!scheme) {
		nocc_serious ("fhandle_access(): failed to find a scheme to handle \"%s\"", path);
		fhandle_seterr (NULL, -ENOSYS);
		return -ENOSYS;
	}

	if (!scheme->accessfcn) {
		nocc_serious ("fhandle_access(): scheme [%s] does not support access() for \"%s\"", scheme->sname, path);
		err = -ENOSYS;
	} else {
		err = scheme->accessfcn (path + poffs, amode);
	}
	fhandle_seterr (NULL, err);

	return err;
}
/*}}}*/
/*{{{  int fhandle_mkdir (const char *path, const int perm)*/
/*
 *	creates a directory
 *	returns 0 on success, non-zero on failure
 */
int fhandle_mkdir (const char *path, const int perm)
{
	fhscheme_t *scheme;
	int err, poffs;

	scheme = fhandle_lookupscheme (path, &poffs);
	if (!scheme) {
		nocc_serious ("fhandle_mkdir(): failed to find a scheme to handle \"%s\"", path);
		fhandle_seterr (NULL, -ENOSYS);
		return -ENOSYS;
	}

	if (!scheme->mkdirfcn) {
		nocc_serious ("fhandle_mkdir(): scheme [%s] does not support mkdir() for \"%s\"", scheme->sname, path);
		err = -ENOSYS;
	} else {
		err = scheme->mkdirfcn (path + poffs, perm);
	}
	fhandle_seterr (NULL, err);

	return err;
}
/*}}}*/
/*{{{  int fhandle_stat (const char *path, struct stat *st_buf)*/
/*
 *	stat()s a file or directory
 *	returns 0 on success, non-zero on error
 */
int fhandle_stat (const char *path, struct stat *st_buf)
{
	fhscheme_t *scheme;
	int err, poffs;

	scheme = fhandle_lookupscheme (path, &poffs);
	if (!scheme) {
		nocc_serious ("fhandle_stat(): failed to find a scheme to handle \"%s\"", path);
		fhandle_seterr (NULL, -ENOSYS);
		return -ENOSYS;
	}

	if (!scheme->statfcn) {
		nocc_serious ("fhandle_stat(): scheme [%s] does not support stat() for \"%s\"", scheme->sname, path);
		err = -ENOSYS;
	} else {
		err = scheme->statfcn (path + poffs, st_buf);
	}
	fhandle_seterr (NULL, err);

	return err;
}
/*}}}*/
/*{{{  int fhandle_cnewer (const char *path1, const char *path2)*/
/*
 *	determines whether 'path1' is "newer" than 'path2'.
 *	returns 1 if true, or 0 if not, -1 on error
 */
int fhandle_cnewer (const char *path1, const char *path2)
{
	struct stat st1_buf, st2_buf;

	if (fhandle_stat (path1, &st1_buf)) {
		return -1;
	} else if (fhandle_stat (path2, &st2_buf)) {
		return -1;
	}
	if (st1_buf.st_ctime > st2_buf.st_ctime) {
		return 1;
	}
	return 0;
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
	int err;

	if (fh) {
		err = fh->err;
	} else {
		err = last_error_code;
	}

	if (err < 0) {
		/* invert */
		err = -err;
	}
	return err;
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
/*{{{  int fhandle_printf (fhandle_t *fh, const char *fmt, ...)*/
/*
 *	does printf style formatting writing to a file.
 *	returns number of bytes written, -1 on error.
 */
int fhandle_printf (fhandle_t *fh, const char *fmt, ...)
{
	int count = 0;
	va_list ap;

	va_start (ap, fmt);
	count = fhandle_vprintf (fh, fmt, ap);
	va_end (ap);

	return count;
}
/*}}}*/
/*{{{  int fhandle_vprintf (fhandle_t *fh, const char *fmt, va_list ap)*/
/*
 *	does printf style formatting writing to a file.
 *	returns number of bytes written, -1 on error.
 */
int fhandle_vprintf (fhandle_t *fh, const char *fmt, va_list ap)
{
	int count = 0;

	if (!fh) {
		fhandle_seterr (fh, -EINVAL);
		return -1;
	} else if (!fh->scheme) {
		fhandle_seterr (fh, -ENOSYS);
		return -1;
	}

	count = fh->scheme->printffcn (fh, fmt, ap);

	if (count < 0) {
		fhandle_seterr (fh, count);
		count = -1;
	} else {
		fhandle_seterr (fh, 0);
	}
	return count;
}
/*}}}*/
/*{{{  int fhandle_write (fhandle_t *fh, unsigned char *buffer, int size)*/
/*
 *	writes data to a file.
 *	returns number of bytes written on success, <= 0 on error.
 */
int fhandle_write (fhandle_t *fh, unsigned char *buffer, int size)
{
	if (!fh) {
		return fhandle_seterr (fh, -EINVAL);
	} else if (!fh->scheme) {
		return fhandle_seterr (fh, -ENOSYS);
	}

	return fh->scheme->writefcn (fh, buffer, size);
}
/*}}}*/
/*{{{  int fhandle_read (fhandle_t *fh, unsigned char *bufaddr, int max)*/
/*
 *	reads data to a file.
 *	returns number of bytes written on success, <= 0 on error.
 */
int fhandle_read (fhandle_t *fh, unsigned char *bufaddr, int max)
{
	if (!fh) {
		return fhandle_seterr (fh, -EINVAL);
	} else if (!fh->scheme) {
		return fhandle_seterr (fh, -ENOSYS);
	}

	return fh->scheme->readfcn (fh, bufaddr, max);
}
/*}}}*/
/*{{{  int fhandle_gets (fhandle_t *fh, char *bufaddr, int max)*/
/*
 *	reads a line of input from the specified handle to 'bufaddr', at most 'max'-1 chars.
 *	returns characters read on success, 0 on EOF, <0 on error.
 */
int fhandle_gets (fhandle_t *fh, char *bufaddr, int max)
{
	if (!fh) {
		return fhandle_seterr (fh, -EINVAL);
	} else if (!fh->scheme) {
		return fhandle_seterr (fh, -ENOSYS);
	}

	return fh->scheme->getsfcn (fh, bufaddr, max);
}
/*}}}*/
/*{{{  int fhandle_flush (fhandle_t *fh)*/
/*
 *	flushes a particular file.
 *	returns 0 on success, non-zero on failure.
 */
int fhandle_flush (fhandle_t *fh)
{
	if (!fh) {
		return fhandle_seterr (fh, -EINVAL);
	} else if (!fh->scheme) {
		return fhandle_seterr (fh, -ENOSYS);
	}

	return fh->scheme->flushfcn (fh);
}
/*}}}*/
/*{{{  int fhandle_isatty (fhandle_t *fh)*/
/*
 *	returns non-zero if the file handle is a TTY
 */
int fhandle_isatty (fhandle_t *fh)
{
	if (!fh) {
		return fhandle_seterr (fh, -EINVAL);
	} else if (!fh->scheme) {
		return fhandle_seterr (fh, -ENOSYS);
	}

	return fh->scheme->isattyfcn (fh);
}
/*}}}*/
/*{{{  int fhandle_ppxml (fhandle_t *fh, const char *fmt, ...)*/
/*
 *	does pretty-printed XML style formatting if writing to a TTY (or enforced)
 *	returns number of bytes written, -1 on error.
 */
int fhandle_ppxml (fhandle_t *fh, const char *fmt, ...)
{
	int count = 0;
	va_list ap;

	va_start (ap, fmt);
	count = fhandle_vppxml (fh, fmt, ap);
	va_end (ap);

	return count;
}
/*}}}*/
/*{{{  */
/*
 *	does pretty-printed XML style formatting if writing to a TTY (or envorced)
 *	returns number of bytes written, -1 on error.
 */
int fhandle_vppxml (fhandle_t *fh, const char *fmt, va_list ap)
{
	char *tstr, *xstr, *ch, *dh;
	int count = 0;
	int tsize = 256;			/* reasonable length to start with */
	int acnt, xlen;
	va_list ap2;

	if (!fh) {
		fhandle_seterr (fh, -EINVAL);
		return -1;
	} else if (!fh->scheme) {
		fhandle_seterr (fh, -ENOSYS);
		return -1;
	}

	if (!compopts.prettyprint || !fhandle_isatty (fh)) {
		/* regular printf please! */
		return fhandle_vprintf (fh, fmt, ap);
	}

	/*{{{  format string and args into 'tstr' (reallocate for bigger if needed) => tstr, tsize, count*/
	va_copy (ap2, ap);			/* save incase we need to revisit! */

	tstr = (char *)smalloc (tsize);
	count = vsnprintf (tstr, tsize, fmt, ap);

	if (count < 0) {
		/* wrecked */
		sfree (tstr);
		return -1;
	} else if (count >= tsize) {
		/* need more */
		sfree (tstr);

		tsize = count + 1;
		tstr = (char *)smalloc (tsize);

		tstr = (char *)smalloc (tsize);
		count = vsnprintf (tstr, tsize, fmt, ap2);

		if (count < 0) {
			/* wrecked 2nd time */
			sfree (tstr);
			return -1;
		}
	}

	/*}}}*/
	/* whatever is in 'tstr' should look like XML! */
	/*{{{  quickly walk over the string, count how many spaces and '=' we have (outside strings) => acnt */
	for (ch=tstr, acnt=0; *ch != '\0'; ch++) {
		switch (*ch) {
		case '<':
		case '>':
			acnt+=2;
			break;
		case ':':
		case '=':
			acnt+=4;
			break;
		case '\"':
			for (ch++; (*ch != '\"') && (*ch != '\0'); ch++) {
				if (*ch == '\\') {
					/* skip next (escaped) character in string */
					if (ch[1] != '\0') {
						ch++;
					}
				}
			}
			if (*ch == '\"') {
				ch++;
			}
			acnt+=4;
			break;
		}
	}
	/*}}}*/
	/*{{{  allocate new string with enough space, and populate it => xstr */
	acnt++;
	xstr = (char *)smalloc (tsize + (acnt * 16));		/* overly generous perhaps! */

	for (ch=tstr, dh=xstr; *ch != '\0';) {
		switch (*ch) {
		case '<':
			{
				int hasns = 0;
				char *eh = ch + 1;

				dh += str_xml_hi (dh, SSTYLE_XML);
				for (eh=ch+1; (*eh != '\0') && (*eh != ':') && (*eh != ' ') && (*eh != '\t') && (*eh != '>'); eh++);
				if (*eh == ':') {
					hasns = 1;
				}

				*dh = *ch;		/* left chevron */
				dh++, ch++;
				if (hasns) {
					dh += str_xml_hi (dh, SSTYLE_NSNAME);
					for (; (*ch != ':'); ch++) {
						*dh = *ch;
						dh++;
					}
					*dh = *ch;	/* colon */
					dh++, ch++;
					dh += str_xml_hi (dh, SSTYLE_XML);
				}
				/* what's left of the name please! */
				for (; (*ch != '\0') && (*ch != ' ') && (*ch != '\t') && (*ch != '>'); ch++) {
					*dh = *ch;
					dh++;
				}
				if (*ch == '>') {
					/* we'll take this in the same style too, then */
					*dh = *ch;
					ch++, dh++;
				}
			}
			break;
		case '/':
		case '>':
			/* must be closing, or end-of, attribute */
			dh += str_xml_hi (dh, SSTYLE_XML);
			for (; (*ch != '\0') && (*ch != ' ') && (*ch != '\t') && (*ch != '>'); ch++) {
				*dh = *ch;
				dh++;
			}
			if (*ch == '>') {
				/* we'll take this in the same style too, then */
				*dh = *ch;
				ch++, dh++;
			}
			break;
		default:
			if (isalpha (*ch) || (*ch == '_')) {
				/* assume start of attribute */
				char *eh;

#if 0
fprintf (stderr, "assume start of attribute: [%s]\n", ch);
#endif
				for (eh=ch+1; (*eh != '\0') && (*eh != ':') && (*eh != '=') && (*eh != ' ') && (*eh != '\t'); eh++);
				if (*eh == ':') {
					/* got namespace.. */
					dh += str_xml_hi (dh, SSTYLE_NSNAME);
					for (; ch < eh; ch++) {
						*dh = *ch;
						dh++;
					}
					*dh = *ch;	/* colon */
					ch++, dh++;
				}
				dh += str_xml_hi (dh, SSTYLE_ATTR);
				/* what's left of the name please! */
				for (; (*ch != '\0') && (*ch != ' ') && (*ch != '\t') && (*ch != '='); ch++) {
					*dh = *ch;
					dh++;
				}
				if (*ch == '=') {
					dh += str_xml_hi (dh, SSTYLE_NONE);
					*dh = *ch;	/* equals */
					ch++, dh++;
				}

				/* now some value! (should be quoted string in XML..) */
				if (*ch == '\"') {
					int isptr = 1;

#if 0
fprintf (stderr, "arp, looking for pointer in string starting: [%s]\n", ch);
#endif
					if ((ch[1] != '0') || (ch[2] != 'x')) {
						isptr = 0;
					} else {
						for (eh = ch+3; (*eh != '\"') && (*eh != '\0'); eh++) {
							/* expecting hexadecimal */
							if (!isxdigit (*eh)) {
								isptr = 0;
								break;
							}
						}
					}
					if (isptr) {
						/* emit string in different colour */
						dh += str_xml_hi (dh, SSTYLE_PTR);
					} else {
						dh += str_xml_hi (dh, SSTYLE_STR);
					}
					*dh = *ch;	/* opening quote */
					ch++, dh++;
					for (; (*ch != '\0') && (*ch != '\"'); ch++) {
						*dh = *ch;
						dh++;
						if (*ch == '\\') {
							/* escaped character, copy next first */
							if (ch[1] != '\0') {
								ch++;
								*dh = *ch;
								dh++;
							}
						}
					}
					if (*ch == '\"') {
						/* expected closing quote */
						*dh = *ch;
						ch++, dh++;
					}
					dh += str_xml_hi (dh, SSTYLE_NONE);
				}
			} else {
				/* something else.. */
				*dh = *ch;
				ch++, dh++;
			}
			break;
		}
	}
	dh += str_xml_hi (dh, SSTYLE_NONE);

	xlen = (int)(dh - xstr);

	/*}}}*/

	/*{{{  lose tstr*/

	sfree (tstr);
	/*}}}*/

	/* and, finally, print it! */
	fhandle_printf (fh, "%s", xstr);
	sfree (xstr);

	return xlen;
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



