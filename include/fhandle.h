/*
 *	fhandle.h -- file I/O abstraction
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

#ifndef __FHANDLE_H
#define __FHANDLE_H

struct TAG_fhscheme;

typedef struct TAG_fhandle {
	struct TAG_fhscheme *scheme;	/* particular scheme (implementation) */
	void *ipriv;			/* private per-file for implementation */
	char *path;			/* actual path (whole thing) */
	char *spath;			/* scheme path (points into above), without leading file:// etc. */
} fhandle_t;

extern fhandle_t *fhandle_fopen (const char *path, const char *mode);
extern fhandle_t *fhandle_open (const char *path, const int mode, const int perm);
extern int fhandle_close (fhandle_t *fh);


extern int fhandle_init (void);
extern int fhandle_shutdown (void);


/* assorted handler initialisation/shutdown routines */
extern int file_unix_init (void);
extern int file_unix_shutdown (void);


#endif	/* !__FHANDLE_H */

