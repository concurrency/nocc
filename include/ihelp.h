/*
 *	ihelp.h -- interactive help stuff for NOCC
 *	Copyright (C) 2011 Fred Barnes <frmb@kent.ac.uk>
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

#ifndef __IHELP_H
#define	__IHELP_H

typedef struct TAG_ihelpentry {
	char *id;			/* used as the key for the stringhash of these */
	char *text;
} ihelpentry_t;

#define IHELPSET_BITSIZE	(4)

typedef struct TAG_ihelpset {
	char *lang;			/* language ("en") */
	char *tag;			/* tag for extras */
	STRINGHASH (ihelpentry_t *, entries, IHELPSET_BITSIZE);
} ihelpset_t;

extern char *ihelp_getentry (const char *lang, const char *tag, const char *entry);

extern int ihelp_init (void);
extern int ihelp_shutdown (void);

#endif	/* !__IHELP_H */

