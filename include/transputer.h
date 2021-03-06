/*
 *	transputer.h -- transputer instructions (extended)
 *	Copyright (C) 2005-2016 Fred Barnes <frmb@kent.ac.uk>
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

struct TAG_origin;

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
	I_MTALLOC = 63,
	I_MTRELEASE = 64,
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
	I_MWS_SYNC = 79,
	I_MWS_ALTLOCK = 80,
	I_MWS_ALTUNLOCK = 81,
	I_MWS_ALT = 82,
	I_MWS_ALTEND = 83,
	I_MWS_ENB = 84,
	I_MWS_DIS = 85,
	I_MWS_ALTPOSTLOCK = 86,
	I_MWS_PPBASEOF = 87,
	I_MWS_PPPAROF = 88,
	I_REV = 89,
	I_IOR = 90,
	I_IOW = 91,
	I_IOR8 = 92,
	I_IOW8 = 93,
	I_IOR16 = 94,
	I_IOW16 = 95,
	I_IOR32 = 96,
	I_IOW32 = 97,
	I_SHL = 98,
	I_SHR = 99,
	I_OR = 100,
	I_AND = 101,
	I_XOR = 102,
	I_CSUB0 = 103,
	I_LDTIMER = 104,
	I_TIN = 105,
	I_ENBC2 = 106,
	I_ENBT2 = 107,
	I_ENBS2 = 108,
	I_JCSUB0 = 109,
	I_JTABLE = 110,
	I_GETAFF = 111,
	I_SETAFF = 112,
	I_GETPAS = 113,
	I_FPSTTEST = 114,
	I_FPLDNLDBI = 115,
	I_FPCHKERR = 116,
	I_FPSTNLDB = 117,
	I_FPLDTEST = 118,
	I_FPLDNLSNI = 119,
	I_FPADD = 120,
	I_FPSTNLSN = 121,
	I_FPSUB = 122,
	I_FPLDNLDB = 123,
	I_FPMUL = 124,
	I_FPDIV = 125,
	I_FPRANGE = 126,
	I_FPLDNLSN = 127,
	I_FPNAN = 128,
	I_FPORDERED = 129,
	I_FPNOTFINITE = 130,
	I_FPGT = 131,
	I_FPEQ = 132,
	I_FPI32TOR32 = 133,
	I_FPGE = 134,
	I_FPI32TOR64 = 134,
	I_FPB32TOR64 = 135,
	I_FPLG = 136,
	I_FPTESTERR = 137,
	I_FPRTOI32 = 138,
	I_FPSTNLI32 = 139,
	I_FPLDZEROSN = 140,
	I_FPLDZERODB = 141,
	I_FPINT = 142,
	I_FPDUP = 143,
	I_FPREV = 144,
	I_FPLDNLADDDB = 145,
	I_FPENTRY3 = 146,
	I_FPLDNLMULDB = 147,
	I_FPENTRY2 = 148,
	I_FPLDNLADDSN = 149,
	I_FPENTRY = 150,
	I_FPLDNLMULSN = 151,
	I_FPREM = 152,
	I_FPRN = 153,
	I_FPDIVBY2 = 154,
	I_FPMULBY2 = 155,
	I_FPSQRT = 156,
	I_FPRP = 157,
	I_FPRM = 158,
	I_FPRZ = 159,
	I_FPR32TOR64 = 160,
	I_FPR64TOR32 = 161,
	I_FPEXPDEC32 = 162,
	I_FPEXPINC32 = 163,
	I_FPABS = 164,
	I_FPADDDBSN = 165,
	I_FPCHKI32 = 166,
	I_FPCHKI64 = 167,
	I_FPPOP = 168,
	I_FPSTALL = 169,
	I_FPLDALL = 170,
	I_SMALLER = 171,
	I_GREATER = 172,
	I_UADD = 173,
	I_USUB = 174,
	I_UMUL = 175,
	I_UDIV = 176,
	I_UREM = 177,
	I_UPROD = 178,
	I_OUTBYTE = 179,
	I_OUTWORD = 180,
	/* for the non-existant T10000 */
	I_SHUTDOWN = 181
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
	struct TAG_origin *origin;
} transinstr_t;


extern void transinstr_init (void);
extern transinstr_t *transinstr_lookup (const char *str, const int len);
extern int transinstr_add (const char *str, instrlevel_e level, struct TAG_origin *origin);


#endif	/* !__TRANSPUTER_H */

