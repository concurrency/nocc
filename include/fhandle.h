/*
 *	fhandle.h -- file I/O abstraction
 *	Copyright (C) 2013-2015 Fred Barnes <frmb@kent.ac.uk>
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

#ifndef __FHANDLE_H
#define __FHANDLE_H

struct TAG_fhscheme;
struct stat;

typedef struct TAG_fhandle {
	struct TAG_fhscheme *scheme;	/* particular scheme (implementation) */
	void *ipriv;			/* private per-file for implementation */
	char *path;			/* actual path (whole thing) */
	char *spath;			/* scheme path (points into above), without leading file:// etc. */
	int err;			/* last error associated with open-file */
} fhandle_t;

extern fhandle_t *fhandle_fopen (const char *path, const char *mode);
extern fhandle_t *fhandle_open (const char *path, const int mode, const int perm);
extern int fhandle_access (const char *path, const int amode);
extern int fhandle_mkdir (const char *path, const int perm);
extern int fhandle_stat (const char *path, struct stat *st_buf);
extern int fhandle_cnewer (const char *path1, const char *path2);
extern int fhandle_close (fhandle_t *fh);
extern int fhandle_lasterr (fhandle_t *fh);
extern unsigned char *fhandle_mapfile (fhandle_t *fh, size_t offset, size_t length);
extern int fhandle_unmapfile (fhandle_t *fh, unsigned char *ptr, size_t offset, size_t length);
extern int fhandle_printf (fhandle_t *fh, const char *fmt, ...) __attribute__ ((format (printf, 2, 3)));
extern int fhandle_vprintf (fhandle_t *fh, const char *fmt, va_list ap);
extern int fhandle_write (fhandle_t *fh, unsigned char *buffer, int size);
extern int fhandle_read (fhandle_t *fh, unsigned char *bufaddr, int max);
extern int fhandle_gets (fhandle_t *fh, char *bufaddr, int max);
extern int fhandle_flush (fhandle_t *fh);
extern int fhandle_isatty (fhandle_t *fh);
extern int fhandle_ppxml (fhandle_t *fh, const char *fmt, ...) __attribute__ ((format (printf, 2, 3)));
extern int fhandle_vppxml (fhandle_t *fh, const char *fmt, va_list ap);


extern int fhandle_init (void);
extern int fhandle_shutdown (void);


/* assorted handler initialisation/shutdown routines */
extern int file_unix_init (void);
extern int file_unix_shutdown (void);

extern int file_url_init (void);
extern int file_url_shutdown (void);

/* specially handled thing */
extern fhandle_t *file_unix_getstderr (void);
#define FHAN_STDERR (file_unix_getstderr())
extern fhandle_t *file_unix_getstdout (void);
#define FHAN_STDOUT (file_unix_getstdout())

#endif	/* !__FHANDLE_H */

