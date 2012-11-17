/*
 *	avrinstr.h -- AVR instructions (for front-end and back-end)
 *	Copyright (C) 2012 Fred Barnes <frmb@kent.ac.uk>
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

#ifndef __AVRINSTR_H
#define __AVRINSTR_H

typedef enum ENUM_avrinstr {
	INS_INVALID = 0,
	/* arithmetic and logic */
	INS_ADD = 1,
	INS_ADC = 2,
	INS_ADIW = 3,
	INS_SUB = 4,
	INS_SUBI = 5,
	INS_SBC = 6,
	INS_SBCI = 7,
	INS_SBIW = 8,
	INS_AND = 9,
	INS_ANDI = 10,
	INS_OR = 11,
	INS_ORI = 12,
	INS_EOR = 13,
	INS_COM = 14,
	INS_NEG = 15,
	INS_SBR = 16,
	INS_CBR = 17,
	INS_INC = 18,
	INS_DEC = 19,
	INS_TST = 20,
	INS_CLR = 21,
	INS_SER = 22,
	INS_MUL = 23,
	INS_MULS = 24,
	INS_MULSU = 25,
	INS_FMUL = 26,
	INS_FMULS = 27,
	INS_FMULSU = 28,
	/* flow control */
	INS_RJMP = 29,
	INS_IJMP = 30,
	INS_EIJMP = 31,
	INS_JMP = 32,
	INS_RCALL = 33,
	INS_ICALL = 34,
	INS_EICALL = 35,
	INS_CALL = 36,
	INS_RET = 37,
	INS_RETI = 38,
	INS_CPSE = 39,
	INS_CP = 40,
	INS_CPC = 41,
	INS_CPI = 42,
	INS_SBRC = 43,
	INS_SBRS = 44,
	INS_SBIC = 45,
	INS_SBIS = 46,
	INS_BRBS = 47,
	INS_BRBC = 48,
	INS_BREQ = 49,
	INS_BRNE = 50,
	INS_BRCS = 51,
	INS_BRCC = 52,
	INS_BRSH = 53,
	INS_BRLO = 54,
	INS_BRMI = 55,
	INS_BRPL = 56,
	INS_BRGE = 57,
	INS_BRLT = 58,
	INS_BRHS = 59,
	INS_BRHC = 60,
	INS_BRTS = 61,
	INS_BRTC = 62,
	INS_BRVS = 63,
	INS_BRVC = 64,
	INS_BRIE = 65,
	INS_BRID = 66,
	/* data transfer */
	INS_MOV = 67,
	INS_MOVW = 68,
	INS_LDI = 69,
	INS_LDS = 70,
	INS_LD = 71,
	INS_LDD = 72,
	INS_STS = 73,
	INS_ST = 74,
	INS_STD = 75,
	INS_LPM = 76,
	INS_ELPM = 77,
	INS_SPM = 78,
	INS_IN = 79,
	INS_OUT = 80,
	INS_PUSH = 81,
	INS_POP = 82,
	/* bit and bit-test */
	INS_LSL = 83,
	INS_LSR = 84,
	INS_ROL = 85,
	INS_ROR = 86,
	INS_ASR = 87,
	INS_SWAP = 88,
	INS_BSET = 89,
	INS_BCLR = 90,
	INS_SBI = 91,
	INS_CBI = 92,
	INS_BST = 93,
	INS_BLD = 94,
	INS_SEC = 95,
	INS_CLC = 96,
	INS_SEN = 97,
	INS_CLN = 98,
	INS_SEZ = 99,
	INS_CLZ = 100,
	INS_SEI = 101,
	INS_CLI = 102,
	INS_SES = 103,
	INS_CLS = 104,
	INS_SEV = 105,
	INS_CLV = 106,
	INS_SET = 107,
	INS_CLT = 108,
	INS_SEH = 109,
	INS_CLH = 110,
	/* MCU control */
	INS_BREAK = 111,
	INS_NOP = 112,
	INS_SLEEP = 113,
	INS_WDR = 114,
} avrinstr_e;

typedef enum ENUM_avrinstr_mode {
	IMODE_NONE = 0x0000,
	IMODE_REG = 0x0001,		/* general register */
	IMODE_CONST8 = 0x0002,		/* 8-bit constant */
	IMODE_CONST3 = 0x0004,		/* 3-bit constant */
	IMODE_CONSTCODE = 0x0008,	/* constant address in code */
	IMODE_CONSTMEM = 0x0010,	/* constant address in memory */
	IMODE_CONSTIO = 0x0020,		/* constant address in I/O space */
	IMODE_INCDEC = 0x0040,		/* supports pre-decrement / post-increment */
	IMODE_XYZ = 0x0080,		/* X,Y,Z registers only */
} avrinstr_mode_e;

typedef struct TAG_avrinstr_tbl {
	avrinstr_e ins;
	const char *str;
	avrinstr_mode_e arg0;
	avrinstr_mode_e arg1;
} avrinstr_tbl_t;

typedef enum ENUM_avrmcu {
	AVR_INVALID = 0,
	AVR_AT90S1200 = 1,
	AVR_AT90S2313 = 2,
	AVR_AT90S8515 = 3,
	AVR_ATMEGA328 = 4,
	AVR_ATMEGA1280 = 5
} avrmcu_e;

typedef struct TAG_avrtarget {
	avrmcu_e mcu;			/* MCU identifier (AVR_...) */
	const char *name;		/* meaningful name */
	int intr_count;			/* number of interrupts (expected at start of code) */
	int intr_size;			/* interrupt instruction size (in bytes) [2 or 4] */
	int code_size;			/* size of code [flash] (in bytes) */
	int ram_start;			/* start address of actual SRAM in data-space */
	int ram_size;			/* size of SRAM (in bytes) */
	int io_size;			/* I/O space size (in addresses) */
	int eeprom_size;		/* EEPROM size (in bytes) */
} avrtarget_t;


#endif	/* !__AVRINSTR_H */

