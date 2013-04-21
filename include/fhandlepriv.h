/*
 *	fhandlepriv.h -- private stuff for file I/O abstractions
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

#ifndef __FHANDLEPRIV_H
#define __FHANDLEPRIV_H

struct TAG_fhandle;

typedef struct TAG_fhscheme {
	char *sname;			/* scheme name ("host") */
	char *sdesc;			/* meaningful description ("host file-system") */
	char *prefix;			/* path prefix ("file://") */

	void *spriv;			/* implementation-specific hook */
	int usecount;			/* usage-count (of 'open' files) */

	int (*openfcn)(struct TAG_fhandle *, const int, const int);
	int (*closefcn)(struct TAG_fhandle *);
	int (*mapfcn)(struct TAG_fhandle *, unsigned char **, size_t, size_t);
	int (*unmapfcn)(struct TAG_fhandle *, unsigned char *, size_t, size_t);
} fhscheme_t;


extern fhscheme_t *fhandle_newscheme (void);
extern void fhandle_freescheme (fhscheme_t *sptr);

extern int fhandle_registerscheme (fhscheme_t *scheme);
extern int fhandle_unregisterscheme (fhscheme_t *scheme);


#endif	/* !__FHANDLEPRIV_H */

