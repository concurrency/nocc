/*
 *	extn.h -- dynamic extension interface
 *	Copyright (C) 2004 Fred Barnes <frmb@kent.ac.uk>
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

#ifndef __EXTN_H
#define __EXTN_H

struct TAG_langparser;
struct TAG_dfattbl;

typedef struct TAG_extn {
	char *name;
	char *desc;
	char *filename;

	int (*init)(struct TAG_extn *);
	int (*preloadgrammar)(struct TAG_extn *, struct TAG_langparser *, struct TAG_dfattbl ***, int *, int *);
	int (*postloadgrammar)(struct TAG_extn *, struct TAG_langparser *);

	void *hook;
} extn_t;

extern void extn_init (void);
extern int extn_loadextn (const char *fname);

extern int extn_register (extn_t *extn);

extern int extn_preloadgrammar (struct TAG_langparser *lang, struct TAG_dfattbl ***ttblptr, int *ttblcur, int *ttblmax);
extern int extn_postloadgrammar (struct TAG_langparser *lang);


#endif	/* !__EXTN_H */

