/*
 *	keywords.h -- keyword processing
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

#ifndef __KEYWORDS_H
#define __KEYWORDS_H

typedef struct TAG_keyword {
	char *name;
	int tagval;
	void *origin;
} keyword_t;

extern void keywords_init (void);
extern keyword_t *keywords_lookup (const char *str, const int len);
extern keyword_t *keywords_add (const char *str, const int tagval, void *origin);


#endif	/* !__KEYWORDS_H */

