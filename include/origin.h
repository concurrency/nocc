/*
 *	origin.h -- compiler origins
 *	Copyright (C) 2007 Fred Barnes <frmb@kent.ac.uk>
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

#ifndef	__ORIGIN_H
#define	__ORIGIN_H

struct TAG_langparser;
struct TAG_langlexer;

typedef enum ENUM_origin {
	ORG_INVALID = 0,
	ORG_INTERNAL = 1,
	ORG_EXTN = 2,
	ORG_LANGPARSER = 3,
	ORG_LANGLEXER = 4
} origin_e;

typedef struct TAG_origin {
	origin_e type;
	union {
		struct {
			char *file;
			int line;
		} internal;
		struct {
			struct TAG_langparser *lp;
		} langparser;
		struct {
			struct TAG_langlexer *ll;
		} langlexer;
	} u;
} origin_t;

extern int origin_init (void);
extern int origin_shutdown (void);

extern origin_t *origin_internal (const char *filename, const int lineno);
extern origin_t *origin_langparser (struct TAG_langparser *lp);
extern origin_t *origin_langlexer (struct TAG_langlexer *ll);


#define INTERNAL_ORIGIN origin_internal(__FILE__,__LINE__)


#endif	/* !__ORIGIN_H */

