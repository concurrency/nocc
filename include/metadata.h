/*
 *	metadata.h -- separated meta-data information for NOCC
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

#ifndef __METADATA_H
#define __METADATA_H

/* meta-data hooks */
typedef struct TAG_metadata {
	char *name;
	char *data;
} metadata_t;

typedef struct TAG_metadatalist {
	DYNARRAY (metadata_t *, items);
} metadatalist_t;


extern int metadata_init (void);
extern int metadata_shutdown (void);

extern int metadata_addreservedname (const char *name);
extern int metadata_isreservedname (const char *name);
extern int metadata_fixreserved (metadata_t *md);

extern metadata_t *metadata_newmetadata (void);
extern metadata_t *metadata_createmetadata (char *name, char *data);
extern metadata_t *metadata_copymetadata (metadata_t *md);
extern void metadata_freemetadata (metadata_t *md);
extern metadatalist_t *metadata_newmetadatalist (void);
extern void metadata_freemetadatalist (metadatalist_t *mdl);




#endif	/* !__METADATA_H */

