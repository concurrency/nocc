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

/* note: these don't have to be in a particular order */

typedef enum ENUM_transinstr {
	I_INVALID = 0,
	I_LDC = 1,
	I_LDL = 2,
	I_OUT = 10,
	I_IN = 11,
	I_MOVE = 12,
	I_STARTP = 13,
	I_ENDP = 14,
	I_RESCHEDULE = 15,
	I_ADD = 16,
	I_SUB = 17,
	I_MUL = 18,
	I_DIV = 19,
	I_REM = 20,
	I_SUM = 21,
	I_DIFF = 22,
	I_PROD = 23,
	I_NEG = 24,
	I_NOT = 25,
	I_J = 26,
	I_CJ = 27,
	I_EQ = 28,
	I_LT = 29,
	I_GT = 30,
	I_BOOLINVERT = 31,
	I_STOPP = 32,
	I_RUNP = 33,
	I_SETERR = 34,
	I_LD = 35,
	I_ST = 36,
	I_MALLOC = 37,
	I_MRELEASE = 38,
	I_TRAP = 39,
	I_SB = 40,
	I_LB = 41,
	I_SW = 42,
	I_LW = 43,
	I_ADC = 44,
	I_NULL = 45,
	I_SETPRI = 46,
	I_GETPRI = 47,
	I_POP = 48,
	I_ALT = 49,
	I_ALTWT = 50,
	I_TALTWT = 51,
	I_ALTEND = 52,
	I_ENBC = 53,
	I_ENBS = 54,
	I_ENBT = 55,
	I_DISC = 56,
	I_DISS = 57,
	I_DIST = 58,
	I_FBARINIT = 59,
	I_FBARSYNC = 60,
	I_FBARRESIGN = 61,
	I_FBARENROLL = 62,
	I_MTNEW = 63,
	I_MTFREE = 64,
	I_MTCLONE = 65,
	I_MWENB = 66,
	I_MWDIS = 67,
	I_MWALTWT = 68,
	I_MWTALTWT = 69,
	I_MWALT = 70,
	I_MWALTEND = 71,
	I_MWS_BINIT = 72,
	I_MWS_PBRILNK = 73,
	I_MWS_PBRULNK = 74,
	I_MWS_PPILNK = 75,
	I_MWS_PBENROLL = 76,
	I_MWS_PBRESIGN = 77,
	I_MWS_PBADJSYNC = 78,
	I_MWS_SYNC = 79
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

