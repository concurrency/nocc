/*
 *	file_unix.c -- Unix file handling for NOCC
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
#include <unistd.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>

#include "nocc.h"
#include "support.h"
#include "version.h"
#include "fhandle.h"
#include "fhandlepriv.h"

/*}}}*/
/*{{{  local types/vars*/

typedef struct TAG_unixfhandle {
	int fd;
} unixfhandle_t;

typedef struct TAG_unixfhscheme {
	DYNARRAY (fhandle_t *, ofiles);
} unixfhscheme_t;

static fhscheme_t *unix_fhscheme;

/*}}}*/


/*{{{  static unixfhandle_t *unix_newfhandle (void)*/
/*
 *	creates a new unixfhandle_t structure
 */
static unixfhandle_t *unix_newfhandle (void)
{
	unixfhandle_t *ufhan = (unixfhandle_t *)smalloc (sizeof (unixfhandle_t));

	ufhan->fd = -1;
	return ufhan;
}
/*}}}*/
/*{{{  static void unix_freefhandle (unixfhandle_t *ufhan)*/
/*
 *	frees a unixfhandle_t structure
 */
static void unix_freefhandle (unixfhandle_t *ufhan)
{
	if (!ufhan) {
		nocc_serious ("unix_freefhandle(): NULL pointer!");
		return;
	}
	sfree (ufhan);
	return;
}
/*}}}*/
/*{{{  static unixfhscheme_t *unix_newfhscheme (void)*/
/*
 *	creates a new unixfhscheme_t structure
 */
static unixfhscheme_t *unix_newfhscheme (void)
{
	unixfhscheme_t *uscheme = (unixfhscheme_t *)smalloc (sizeof (unixfhscheme_t));

	dynarray_init (uscheme->ofiles);

	return uscheme;
}
/*}}}*/
/*{{{  static void unix_freefhscheme (unixfhscheme_t *uscheme)*/
/*
 *	frees a unixfhscheme_t structure
 */
static void unix_freefhscheme (unixfhscheme_t *uscheme)
{
	if (!uscheme) {
		nocc_serious ("unix_freefhscheme(): NULL pointer!");
		return;
	}
	dynarray_trash (uscheme->ofiles);
	sfree (uscheme);
	return;
}
/*}}}*/


/*{{{  static int unix_openfcn (fhandle_t *fhan, const int mode, const int perm)*/
/*
 *	called to open a file
 *	returns 0 on success, non-zero on error
 */
static int unix_openfcn (fhandle_t *fhan, const int mode, const int perm)
{
	unixfhandle_t *ufhan = unix_newfhandle ();

	if (perm && (mode & O_CREAT)) {
		ufhan->fd = open (fhan->spath, mode, perm);
		if (ufhan->fd < 0) {
			unix_freefhandle (ufhan);
			return errno;
		}
	} else {
		ufhan->fd = open (fhan->spath, mode);
		if (ufhan->fd < 0) {
			unix_freefhandle (ufhan);
			return errno;
		}
	}
	fhan->ipriv = (void *)ufhan;
	return 0;
}
/*}}}*/
/*{{{  static int unix_closefcn (fhandle_t *fhan)*/
/*
 *	called to close a file
 *	returns 0 on success, non-zero on error
 */
static int unix_closefcn (fhandle_t *fhan)
{
	unixfhandle_t *ufhan = (unixfhandle_t *)fhan->ipriv;

	if (!ufhan) {
		nocc_serious ("unix_closefcn(): missing state! [%s]", fhan->path);
		return -1;
	}
	if (ufhan->fd >= 0) {
		close (ufhan->fd);
		ufhan->fd = -1;
	}
	unix_freefhandle (ufhan);
	fhan->ipriv = NULL;

	return 0;
}
/*}}}*/
/*{{{  static int unix_mapfcn (fhandle_t *fhan, unsigned char **pptr, size_t offset, size_t length)*/
/*
 *	called to memory-map a file
 *	returns 0 on success, non-zero on error
 */
static int unix_mapfcn (fhandle_t *fhan, unsigned char **pptr, size_t offset, size_t length)
{
	unixfhandle_t *ufhan = (unixfhandle_t *)fhan->ipriv;
	unsigned char *ptr;

	if (!ufhan) {
		nocc_serious ("unix_mapfcn(): missing state! [%s]", fhan->path);
		return -1;
	} else if (ufhan->fd < 0) {
		nocc_serious ("unix_mapfcn(): file not actually open [%s]", fhan->path);
		return -1;
	}

	/* FIXME: cater for write-mappings (not currently used) */
	ptr = (unsigned char *)mmap ((void *)0, length, PROT_READ, MAP_SHARED, ufhan->fd, offset);
	if (ptr == ((unsigned char *)-1)) {
		/* failed */
		return errno;
	}
	*pptr = ptr;
	return 0;
}
/*}}}*/
/*{{{  static int unix_unmapfcn (fhandle_t *fhan, unsigned char *ptr, size_t offset, size_t length)*/
/*
 *	un-memory-maps a file
 *	returns 0 on success, non-zero on error
 */
static int unix_unmapfcn (fhandle_t *fhan, unsigned char *ptr, size_t offset, size_t length)
{
	unixfhandle_t *ufhan = (unixfhandle_t *)fhan->ipriv;
	int err;

	if (!ufhan) {
		nocc_serious ("unix_unmapfcn(): missing state! [%s]", fhan->path);
		return -1;
	} else if (ufhan->fd < 0) {
		nocc_serious ("unix_unmapfcn(): file not actually open [%s]", fhan->path);
		return -1;
	}

	err = munmap (ptr, length);
	return err;
}
/*}}}*/


/*{{{  int file_unix_init (void)*/
/*
 *	called to initialise and register the Unix file-handling scheme
 *	returns 0 on success, non-zero on failure
 */
int file_unix_init (void)
{
	unixfhscheme_t *uscheme = unix_newfhscheme ();

	unix_fhscheme = fhandle_newscheme ();

	unix_fhscheme->sname = string_dup ("host");
	unix_fhscheme->sdesc = string_dup ("unix host file-system");
	unix_fhscheme->prefix = string_dup ("file://");

	unix_fhscheme->spriv = (void *)uscheme;
	unix_fhscheme->usecount = 0;

	unix_fhscheme->openfcn = unix_openfcn;
	unix_fhscheme->closefcn = unix_closefcn;
	unix_fhscheme->mapfcn = unix_mapfcn;
	unix_fhscheme->unmapfcn = unix_unmapfcn;

	if (fhandle_registerscheme (unix_fhscheme)) {
		nocc_serious ("file_unix_init(): failed to register scheme!");

		unix_fhscheme->spriv = NULL;
		unix_freefhscheme (uscheme);
		fhandle_freescheme (unix_fhscheme);
		unix_fhscheme = NULL;
		return -1;
	}

	return 0;
}
/*}}}*/
/*{{{  int file_unix_shutdown (void)*/
/*
 *	called to shut-down the Unix file-handling scheme
 *	returns 0 on success, non-zero on failure
 */
int file_unix_shutdown (void)
{
	unixfhscheme_t *uscheme;

	if (!unix_fhscheme) {
		nocc_serious ("file_unix_shutdown(): not initialised!");
		return -1;
	}

	if (fhandle_unregisterscheme (unix_fhscheme)) {
		nocc_serious ("file_unix_shutdown(): cannot unregister, in use?");
		return -1;
	}

	uscheme = (unixfhscheme_t *)unix_fhscheme->spriv;
	if (uscheme) {
		unix_freefhscheme (uscheme);
		unix_fhscheme->spriv = NULL;
	}

	fhandle_freescheme (unix_fhscheme);
	unix_fhscheme = NULL;

	return 0;
}
/*}}}*/



