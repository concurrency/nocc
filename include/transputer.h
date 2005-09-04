/*
 *	transputer.h -- transputer instructions (extended)
 *	Copyright (C) 2005 Fred Barnes <frmb@kent.ac.uk>
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

#ifndef __TRANSPUTER_H
#define __TRANSPUTER_H

typedef enum ENUM_transinstr {
	I_INVALID = 0,
	I_LDC = 1,
	I_LDL = 2,
	I_OUT = 10,
	I_IN = 11,
	I_MOVE = 12,
	I_STARTP = 13,
	I_ENDP = 14
} transinstr_e;

typedef enum ENUM_instrlevel {
	INS_INVALID = 0,
	INS_PRIMARY = 1,
	INS_SECONDARY = 2,
	INS_OTHER = 3
} instrlevel_e;

typedef struct TAG_transinstr {
	char *name;
	instrlevel_e level;
	transinstr_e ins;
	void *origin;
} transinstr_t;


extern void transinstr_init (void);
extern transinstr_t *transinstr_lookup (const char *str, const int len);
extern int transinstr_add (const char *str, instrlevel_e level, void *origin);


#endif	/* !__TRANSPUTER_H */

