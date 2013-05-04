/*
 *	file_url.c -- URL style file handling for NOCC
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
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>

#include "nocc.h"
#include "support.h"
#include "version.h"
#include "fhandle.h"
#include "fhandlepriv.h"
#include "crypto.h"
#include "opts.h"

/*}}}*/
/*{{{  local types/vars*/

typedef struct TAG_urlfhandle {
	fhandle_t *realfile;			/* handle on real (unix) file, in cache-dir */
} urlfhandle_t;

typedef struct TAG_urlfhscheme {
	DYNARRAY (fhandle_t *, ofiles);
	char *cachepath;
} urlfhscheme_t;

static fhscheme_t *url_fhscheme = NULL;


/*}}}*/


/*{{{  static int url_clear_localdir (const char *path)*/
/*
 *	clears out a local directory (the cache)
 *	returns 0 on success, non-zero on failure
 */
static int url_clear_localdir (const char *path)
{
	DIR *dfd;
	int err = 0;
	struct dirent *dent;

	dfd = opendir (path);
	if (!dfd) {
		return -errno;
	}
	if (compopts.verbose) {
		nocc_message ("URL: clearing cache as directed");
	}

	while ((dent = readdir (dfd)) != NULL) {
		/* filename should look something like "xxxxxxxx-..." */
		int c, plen = strlen (dent->d_name);

		if (plen < 9) {
			continue;			/* too short, ignore */
		}
		for (c=0; c<8; c++) {
			if (((dent->d_name[c] >= 'a') && (dent->d_name[c] <= 'f')) || ((dent->d_name[c] >= '0') && (dent->d_name[c] <= '9')) ||
					((dent->d_name[c] >= 'A') && (dent->d_name[c] <= 'F'))) {
				/* valid hex digit, carry on */
			} else {
				break;		/* not hex, ignore */
			}
		}
		if (c == 8) {
			if (dent->d_name[c] == '-') {
				/* probable */
				char *npath = string_fmt ("%s/%s", path, dent->d_name);

				if (unlink (npath)) {
					nocc_error ("URL: failed to remove cached file [%s]: %s", npath, strerror (errno));
					err++;
				}
				sfree (npath);
			}
		}
	}

	closedir (dfd);

	return err;
}
/*}}}*/
/*{{{  static int url_opthandler (cmd_option_t *opt, char ***argwalk, int *argleft)*/
/*
 *	option handler for URL file scheme
 *	returns 0 on success, non-zero on failure
 */
static int url_opthandler (cmd_option_t *opt, char ***argwalk, int *argleft)
{
	int optv = (int)opt->arg;

	switch (optv) {
		/*{{{  --clear-cache*/
	case 0:
		/* NOTE: this uses the host file-system directly, means the cache *cannot* be anywhere else! */
		if (!access (compopts.cachedir, F_OK)) {
			/* exists at least, try and empty it */
			int err = url_clear_localdir (compopts.cachedir);

			if (err) {
				nocc_warning ("failed to empty URL cache directory [%s]", compopts.cachedir);
			}
		}
		break;
		/*}}}*/
	default:
		nocc_error ("url_opthandler(): unknown option [%s]", **argwalk);
		return -1;
	}

	return 0;
}
/*}}}*/

/*{{{  static urlfhandle_t *url_newfhandle (void)*/
/*
 *	creates a new urlfhandle_t structure
 */
static urlfhandle_t *url_newfhandle (void)
{
	urlfhandle_t *ufhan = (urlfhandle_t *)smalloc (sizeof (urlfhandle_t));

	ufhan->realfile = NULL;
	return ufhan;
}
/*}}}*/
/*{{{  static void url_freefhandle (urlfhandle_t *ufhan)*/
/*
 *	frees a urlfhandle_t structure
 */
static void url_freefhandle (urlfhandle_t *ufhan)
{
	if (!ufhan) {
		nocc_serious ("url_freefhandle(): NULL pointer!");
		return;
	}
	sfree (ufhan);
	return;
}
/*}}}*/
/*{{{  static urlfhscheme_t *url_newfhscheme (void)*/
/*
 *	creates a new urlfhscheme_t structure
 */
static urlfhscheme_t *url_newfhscheme (void)
{
	urlfhscheme_t *uscheme = (urlfhscheme_t *)smalloc (sizeof (urlfhscheme_t));

	dynarray_init (uscheme->ofiles);
	uscheme->cachepath = NULL;

	return uscheme;
}
/*}}}*/
/*{{{  static void url_freefhscheme (urlfhscheme_t *uscheme)*/
/*
 *	frees a urlfhscheme_t structure
 */
static void url_freefhscheme (urlfhscheme_t *uscheme)
{
	if (!uscheme) {
		nocc_serious ("url_freefhscheme(): NULL pointer!");
		return;
	}
	dynarray_trash (uscheme->ofiles);
	sfree (uscheme);
	return;
}
/*}}}*/

/*{{{  static char *url_gethashedfilename (const char *path)*/
/*
 *	generates the crypto-path (local file-name) for a specific http://.../ path
 */
static char *url_gethashedfilename (const char *path)
{
	crypto_t *cry;
	char *hashcode;
	char *lclpath;
	char *tch;

	/* create a unique hash for the URL (use crypto functions) */
	cry = crypto_newdigest ();
	if (!cry) {
		unsigned int hval = sh_stringhash (path, strlen (path));

		hashcode = string_fmt ("%8.8x", hval);
	} else {
		crypto_writedigest (cry, (unsigned char *)path, strlen (path));
		hashcode = crypto_readdigest (cry, NULL);
		crypto_freedigest (cry);
	}

	/* find trailing name */
	for (tch = (char *)path + strlen (path); (tch > path) && (tch[-1] != '/'); tch--);
	
	if (strlen (hashcode) > 8) {
		/* only going to take the first 8 chars (32 bits) */
		hashcode[8] = '\0';
	}
	lclpath = string_fmt ("%s/%s-%s", compopts.cachedir, hashcode, tch);
#if 0
fhandle_printf (FHAN_STDERR, "url_gethashedfilename(): path to cached file is [%s] -> [%s]\n", path, lclpath);
#endif

	return lclpath;
}
/*}}}*/
/*{{{  static int url_getfile (fhandle_t *fhan)*/
/*
 *	called to "get" a file, needed by 'open' and 'access'.  Will set hashcode, etc.
 *	returns 0 on success, non-zero on error
 */
static int url_getfile (const char *rpath, const char *lpath)
{
	char *tmpopts = string_fmt ("wget %s", compopts.wget_opts);
	char **abits = split_string (tmpopts, 1);
	int i;
	pid_t fres;
	int err = 0;

	for (i=0; abits[i]; i++) {
		if (!strcmp (abits[i], "%u")) {
			/* replace with real path (URL) */
			sfree (abits[i]);
			abits[i] = string_dup (rpath);
		} else if (!strcmp (abits[i], "%l")) {
			/* replace with local path (file) */
			sfree (abits[i]);
			abits[i] = string_dup (lpath);
		}
	}

	fres = fork ();
	if (fres == -1) {
		nocc_serious ("url_getfile(): failed to fork(): %s", strerror (errno));
		err = -EIO;
		goto clean_out;
	}
	if (fres == 0) {
		/* we are the child process */
		execv (compopts.wget_p, abits);
		nocc_internal ("failed to run %s: %s", compopts.wget_p, strerror (errno));
		_exit (1);
	} else {
		/* we are the parent, wait for the child */
		pid_t wres;
		int status = 0;

		wres = waitpid (fres, &status, 0);
		if (wres < 0) {
			nocc_serious ("url_getfile(): wait() failed with: %s", strerror (errno));
			err = -EIO;
			goto clean_out;
		} else if (!WIFEXITED (status)) {
			nocc_serious ("url_getfile(): child process waited, but didn't exit normally?, status = 0x%8.8x", (unsigned int)status);
			err = -EIO;
			goto clean_out;
		}
		if (WEXITSTATUS (status)) {
			/* exited with error, e.g. 404 */
			if (compopts.verbose) {
				nocc_message ("URL: failed to download [%s]", rpath);
			}
			err = -ENOENT;
			goto clean_out;
		}
		/* else we got it! */
	}

clean_out:
	if (err) {
		/* remove anything that might have been created */
		unlink (lpath);
	}

	for (i=0; abits[i]; i++) {
		sfree (abits[i]);
		abits[i] = NULL;
	}
	sfree (abits);
	sfree (tmpopts);

	return err;
}
/*}}}*/


/*{{{  static int url_openfcn (fhandle_t *fhan, const int mode, const int perm)*/
/*
 *	called to open a file
 *	returns 0 on success, non-zero on error
 */
static int url_openfcn (fhandle_t *fhan, const int mode, const int perm)
{
	urlfhandle_t *ufhan = url_newfhandle ();
	char *lpath = url_gethashedfilename (fhan->spath);
	int err = 0;

	if (compopts.cache_pref && !fhandle_access (lpath, R_OK)) {
		/* got local copy and preferred */
	} else {
		err = url_getfile (fhan->path, lpath);
		if (err) {
			url_freefhandle (ufhan);
			sfree (lpath);
			return err;
		}
	}

	ufhan->realfile = fhandle_open (lpath, mode, perm);
	if (!ufhan->realfile) {
		err = fhandle_lasterr (NULL);
		url_freefhandle (ufhan);
		sfree (lpath);
		return err;
	}

	/* if we get here, opened succesfully */
	fhan->ipriv = (void *)ufhan;

	return 0;
}
/*}}}*/
/*{{{  static int url_accessfcn (const char *path, int amode)*/
/*
 *	called to test accessibility of a file
 *	returns 0 on success, non-zero on error
 */
static int url_accessfcn (const char *path, int amode)
{
	char *rpath = string_fmt ("http://%s", path);		/* grotty.. */
	char *lpath = url_gethashedfilename (path);
	int err;

#if 0
fhandle_printf (FHAN_STDERR, "url_accessfcn(): rpath = [%s], lpath = [%s], amode=%x\n", rpath, lpath, amode);
#endif
	if (amode & W_OK) {
		sfree (lpath);
		sfree (rpath);
		return -EIO;			/* cannot write */
	}
	if (amode & X_OK) {
		sfree (lpath);
		sfree (rpath);
		return -EPERM;			/* cannot execute anything we might have downloaded! */
	}
	err = fhandle_access (lpath, amode);
	if (!err) {
		/* means it must exist and/or is readable */
		sfree (lpath);
		sfree (rpath);
		return 0;
	}
#if 0
fhandle_printf (FHAN_STDERR, "url_accessfcn(): error is: %s\n", strerror (fhandle_lasterr (NULL)));
#endif
	/* doesn't exist, try and download it */
	err = url_getfile (rpath, lpath);

	return err;
}
/*}}}*/
/*{{{  static int url_mkdirfcn (const char *path, const int perm)*/
/*
 *	called to create a directory (no-op)
 *	returns 0 on success, non-zero on failure
 */
static int url_mkdirfcn (const char *path, const int perm)
{
	return -ENOSYS;
}
/*}}}*/
/*{{{  static int url_statfcn (const char *path, struct stat *st_buf)*/
/*
 *	called to stat a URL -- in reality, downloads and stats the local copy
 *	returns 0 on success, non-zero on error
 */
static int url_statfcn (const char *path, struct stat *st_buf)
{
	int err = 0;
	char *lpath = url_gethashedfilename (path);

	err = fhandle_access (lpath, R_OK);
	if (err) {
		char *rpath = string_fmt ("http://%s", path);

		err = url_getfile (rpath, lpath);
		sfree (rpath);
	}
	if (err) {
		sfree (lpath);
		return err;
	}
	err = fhandle_stat (lpath, st_buf);
	sfree (lpath);

	return err;
}
/*}}}*/
/*{{{  static int url_closefcn (fhandle_t *fhan)*/
/*
 *	called to close a file
 *	returns 0 on success, non-zero on error
 */
static int url_closefcn (fhandle_t *fhan)
{
	urlfhandle_t *ufhan = (urlfhandle_t *)fhan->ipriv;

	if (!ufhan) {
		nocc_serious ("url_closefcn(): missing state! [%s]", fhan->path);
		return -1;
	}
	if (ufhan->realfile) {
		fhandle_close (ufhan->realfile);
		ufhan->realfile = NULL;
	}
	url_freefhandle (ufhan);
	fhan->ipriv = NULL;

	return 0;
}
/*}}}*/
/*{{{  static int url_mapfcn (fhandle_t *fhan, unsigned char **pptr, size_t offset, size_t length)*/
/*
 *	called to memory-map a URL (done on local copy)
 *	returns 0 on success, non-zero on error
 */
static int url_mapfcn (fhandle_t *fhan, unsigned char **pptr, size_t offset, size_t length)
{
	urlfhandle_t *ufhan = (urlfhandle_t *)fhan->ipriv;
	unsigned char *mapaddr;

	if (!ufhan) {
		nocc_serious ("url_mapfcn(): missing state! [%s]", fhan->path);
		return -1;
	} else if (!ufhan->realfile) {
		nocc_serious ("url_mapfcn(): local file not actually open [%s]", fhan->path);
		return -1;
	}

	mapaddr = fhandle_mapfile (ufhan->realfile, offset, length);
	if (!mapaddr) {
		return -1;
	}

	*pptr = mapaddr;
	return 0;
}
/*}}}*/
/*{{{  static int url_unmapfcn (fhandle_t *fhan, unsigned char *ptr, size_t offset, size_t length)*/
/*
 *	called to un-memory-map a URL (done on local copy)
 *	returns 0 on success, non-zero on error
 */
static int url_unmapfcn (fhandle_t *fhan, unsigned char *ptr, size_t offset, size_t length)
{
	urlfhandle_t *ufhan = (urlfhandle_t *)fhan->ipriv;
	int err;

	if (!ufhan) {
		nocc_serious ("url_unmapfcn(): missing state! [%s]", fhan->path);
		return -1;
	} else if (!ufhan->realfile) {
		nocc_serious ("url_unmapfcn(): local file not actually open [%s]", fhan->path);
		return -1;
	}
	err = fhandle_unmapfile (ufhan->realfile, ptr, offset, length);
	return err;
}
/*}}}*/
/*{{{  static int url_printffcn (fhandle_t *fhan, const char *fmt, va_list ap)*/
/*
 *	does printf style formatting into a URL (done on local file).
 *	returns bytes written on success, < 0 on error
 */
static int url_printffcn (fhandle_t *fhan, const char *fmt, va_list ap)
{
	urlfhandle_t *ufhan = (urlfhandle_t *)fhan->ipriv;
	int res;

	if (!ufhan) {
		nocc_serious ("url_printffcn(): missing state! [%s]", fhan->path);
		return -1;
	} else if (!ufhan->realfile) {
		nocc_serious ("url_printffcn(): local file not actually open [%s]", fhan->path);
		return -1;
	}
	res = fhandle_vprintf (ufhan->realfile, fmt, ap);

	return res;
}
/*}}}*/
/*{{{  static int url_writefcn (fhandle_t *fhan, unsigned char *buffer, int size)*/
/*
 *	writes data to a URL (done on local file).
 *	returns number of bytes written on success, <= 0 on error.
 */
static int url_writefcn (fhandle_t *fhan, unsigned char *buffer, int size)
{
	urlfhandle_t *ufhan = (urlfhandle_t *)fhan->ipriv;
	int res;

	if (!ufhan) {
		nocc_serious ("url_writefcn(): missing state! [%s]", fhan->path);
		return -1;
	} else if (!ufhan->realfile) {
		nocc_serious ("url_writefcn(): local file not actually open [%s]", fhan->path);
		return -1;
	}
	res = fhandle_write (ufhan->realfile, buffer, size);

	return res;
}
/*}}}*/
/*{{{  static int url_readfcn (fhandle_t *fhan, unsigned char *bufaddr, int max)*/
/*
 *	reads data from a URL (done on local file).
 *	returns number of bytes read on success, < 0 on error, 0 on EOF.
 */
static int url_readfcn (fhandle_t *fhan, unsigned char *bufaddr, int max)
{
	urlfhandle_t *ufhan = (urlfhandle_t *)fhan->ipriv;
	int res;

	if (!ufhan) {
		nocc_serious ("url_readfcn(): missing state! [%s]", fhan->path);
		return -1;
	} else if (!ufhan->realfile) {
		nocc_serious ("url_readfcn(): local file not actually open [%s]", fhan->path);
		return -1;
	}
	res = fhandle_read (ufhan->realfile, bufaddr, max);

	return res;
}
/*}}}*/
/*{{{  static int url_getsfcn (fhandle_t *fhan, char *bufaddr, int max)*/
/*
 *	reads a single line of text from a URL (done on local file).
 *	returns number of bytes read on success, < 0 on error, 0 on EOF.
 */
static int url_getsfcn (fhandle_t *fhan, char *bufaddr, int max)
{
	urlfhandle_t *ufhan = (urlfhandle_t *)fhan->ipriv;
	int res;

	if (!ufhan) {
		nocc_serious ("url_getsfcn(): missing state! [%s]", fhan->path);
		return -1;
	} else if (!ufhan->realfile) {
		nocc_serious ("url_getsfcn(): local file not actually open [%s]", fhan->path);
		return -1;
	}
	res = fhandle_gets (ufhan->realfile, bufaddr, max);

	return res;
}
/*}}}*/
/*{{{  static int url_flushfcn (fhandle_t *fhan)*/
/*
 *	flushes an underlying stream (done on local file).
 *	returns 0 on success, non-zero on failure
 */
static int url_flushfcn (fhandle_t *fhan)
{
	urlfhandle_t *ufhan = (urlfhandle_t *)fhan->ipriv;
	int res;

	if (!ufhan) {
		nocc_serious ("url_flushfcn(): missing state! [%s]", fhan->path);
		return -1;
	} else if (!ufhan->realfile) {
		nocc_serious ("url_flushfcn(): local file not actually open [%s]", fhan->path);
		return -1;
	}
	res = fhandle_flush (ufhan->realfile);

	return res;
}
/*}}}*/

/*{{{  int file_url_init (void)*/
/*
 *	called to initialise and register the URL file-handling scheme
 *	returns 0 on success, non-zero on failure
 */
int file_url_init (void)
{
	urlfhscheme_t *uscheme;

	if (!compopts.cachedir) {
		nocc_serious ("file_url_init(): cannot initialise because no cachedir set!");
		return -1;
	}
	if (!compopts.wget_p || !compopts.wget_opts) {
		nocc_serious ("file_url_init(): cannot initialise because wget or options are not set!");
		return -1;
	}
	if (fhandle_access (compopts.cachedir, F_OK)) {
		/* probably doesn't exist, try and create it */
		if (fhandle_mkdir (compopts.cachedir, 0700)) {
			nocc_error ("URL: failed to create cache-directory [%s]: %s", compopts.cachedir, strerror (fhandle_lasterr (NULL)));
			return -1;
		}

		if (compopts.verbose) {
			nocc_message ("URL: created cache directory [%s]", compopts.cachedir);
		}
	}
	
	uscheme = url_newfhscheme ();
	url_fhscheme = fhandle_newscheme ();

	url_fhscheme->sname = string_dup ("url");
	url_fhscheme->sdesc = string_dup ("copy-on-write URL downloads");
	url_fhscheme->prefix = string_dup ("http://");

	url_fhscheme->spriv = (void *)uscheme;
	url_fhscheme->usecount = 0;

	if (compopts.cachedir) {
		uscheme->cachepath = string_dup (compopts.cachedir);
	}

	url_fhscheme->openfcn = url_openfcn;
	url_fhscheme->accessfcn = url_accessfcn;
	url_fhscheme->mkdirfcn = url_mkdirfcn;
	url_fhscheme->statfcn = url_statfcn;
	url_fhscheme->closefcn = url_closefcn;
	url_fhscheme->mapfcn = url_mapfcn;
	url_fhscheme->unmapfcn = url_unmapfcn;
	url_fhscheme->printffcn = url_printffcn;
	url_fhscheme->writefcn = url_writefcn;
	url_fhscheme->readfcn = url_readfcn;
	url_fhscheme->getsfcn = url_getsfcn;
	url_fhscheme->flushfcn = url_flushfcn;

	if (fhandle_registerscheme (url_fhscheme)) {
		nocc_serious ("file_url_init(): failed to register scheme!");

		url_fhscheme->spriv = NULL;
		url_freefhscheme (uscheme);
		fhandle_freescheme (url_fhscheme);
		url_fhscheme = NULL;
		return -1;
	}

	/* all good so far, register command-line option for clearing the cache */
	opts_add ("clear-cache", '\0', url_opthandler, (void *)0, "1clear URL download cache directory");

	return 0;
}
/*}}}*/
/*{{{  int file_url_shutdown (void)*/
/*
 *	called to shut-down the URL file-handling scheme
 *	returns 0 on success, non-zero on failure
 */
int file_url_shutdown (void)
{
	urlfhscheme_t *uscheme;

	if (!url_fhscheme) {
		nocc_serious ("file_url_shutdown(): not initialised!");
		return -1;
	}

	if (fhandle_unregisterscheme (url_fhscheme)) {
		nocc_serious ("file_url_shutdown(): cannot unregister, in use?");
		return -1;
	}

	uscheme = (urlfhscheme_t *)url_fhscheme->spriv;
	if (uscheme) {
		url_freefhscheme (uscheme);
		url_fhscheme->spriv = NULL;
	}

	fhandle_freescheme (url_fhscheme);
	url_fhscheme = NULL;

	return 0;
}
/*}}}*/



