/*
 *	avrasm_program.c -- handling for AVR assembler programs
 *	Copyright (C) 2012-2016 Fred Barnes <frmb@kent.ac.uk>
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

/*{{{  includes*/
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/types.h>

#include "nocc.h"
#include "support.h"
#include "version.h"
#include "fhandle.h"
#include "symbols.h"
#include "keywords.h"
#include "lexer.h"
#include "lexpriv.h"
#include "tnode.h"
#include "parser.h"
#include "fcnlib.h"
#include "dfa.h"
#include "parsepriv.h"
#include "avrasm.h"
#include "feunit.h"
#include "names.h"
#include "scope.h"
#include "prescope.h"
#include "typecheck.h"
#include "constprop.h"
#include "usagecheck.h"
#include "map.h"
#include "codegen.h"
#include "target.h"
#include "avrinstr.h"

/*}}}*/
/*{{{  private types/data*/
typedef struct TAG_avrasm_lithook {
	char *data;
	int len;
} avrasm_lithook_t;

typedef struct TAG_avrasm_xyzhook {
	int reg;			/* 0=X, 1=Y, 2=Z */
	int prepost;			/* 0=none, -1 = predec, 1 = postinc */
	int offs;			/* constant offset (where supported) */
} avrasm_xyzhook_t;

typedef struct TAG_avrasm_uslabhook {
	int id;				/* label identifier */
	int dir;			/* direction: <0 = back, 0 = unset, >0 = forward */
} avrasm_uslabhook_t;

static avrinstr_tbl_t avrasm_itable[] = {
	{INS_ADD, "add", IMODE_REG, IMODE_REG},
	{INS_ADC, "adc", IMODE_REG, IMODE_REG},
	{INS_ADIW, "adiw", IMODE_REG | IMODE_XYZ, IMODE_CONST8},
	{INS_SUB, "sub", IMODE_REG, IMODE_REG},
	{INS_SUBI, "subi", IMODE_REG, IMODE_CONST8},
	{INS_SBC, "sbc", IMODE_REG, IMODE_REG},
	{INS_SBCI, "sbci", IMODE_REG, IMODE_CONST8},
	{INS_SBIW, "sbiw", IMODE_REG | IMODE_XYZ, IMODE_CONST8},
	{INS_AND, "and", IMODE_REG, IMODE_REG},
	{INS_ANDI, "andi", IMODE_REG, IMODE_CONST8},
	{INS_OR, "or", IMODE_REG, IMODE_REG},
	{INS_ORI, "ori", IMODE_REG, IMODE_CONST8},
	{INS_EOR, "eor", IMODE_REG, IMODE_REG},
	{INS_COM, "com", IMODE_REG, IMODE_NONE},
	{INS_NEG, "neg", IMODE_REG, IMODE_NONE},
	{INS_SBR, "sbr", IMODE_REG, IMODE_CONST8},
	{INS_CBR, "cbr", IMODE_REG, IMODE_CONST8},
	{INS_INC, "inc", IMODE_REG, IMODE_NONE},
	{INS_DEC, "dec", IMODE_REG, IMODE_NONE},
	{INS_TST, "tst", IMODE_REG, IMODE_NONE},
	{INS_CLR, "clr", IMODE_REG, IMODE_NONE},
	{INS_SER, "ser", IMODE_REG, IMODE_NONE},
	{INS_MUL, "mul", IMODE_REG, IMODE_REG},
	{INS_MULS, "muls", IMODE_REG, IMODE_REG},
	{INS_MULSU, "mulsu", IMODE_REG, IMODE_REG},
	{INS_FMUL, "fmul", IMODE_REG, IMODE_REG},
	{INS_FMULS, "fmuls", IMODE_REG, IMODE_REG},
	{INS_FMULSU, "fmulsu", IMODE_REG, IMODE_REG},

	{INS_RJMP, "rjmp", IMODE_CONSTCODE, IMODE_NONE},
	{INS_IJMP, "ijmp", IMODE_NONE, IMODE_NONE},
	{INS_EIJMP, "eijmp", IMODE_NONE, IMODE_NONE},
	{INS_JMP, "jmp", IMODE_CONSTCODE, IMODE_NONE},
	{INS_RCALL, "rcall", IMODE_CONSTCODE, IMODE_NONE},
	{INS_ICALL, "icall", IMODE_NONE, IMODE_NONE},
	{INS_EICALL, "eicall", IMODE_NONE, IMODE_NONE},
	{INS_CALL, "call", IMODE_CONSTCODE, IMODE_NONE},
	{INS_RET, "ret", IMODE_NONE, IMODE_NONE},
	{INS_RETI, "reti", IMODE_NONE, IMODE_NONE},
	{INS_CPSE, "cpse", IMODE_REG, IMODE_REG},
	{INS_CP, "cp", IMODE_REG, IMODE_REG},
	{INS_CPC, "cpc", IMODE_REG, IMODE_REG},
	{INS_CPI, "cpi", IMODE_REG, IMODE_CONST8},
	{INS_SBRC, "sbrc", IMODE_REG, IMODE_CONST3},
	{INS_SBRS, "sbrs", IMODE_REG, IMODE_CONST3},
	{INS_SBIC, "sbic", IMODE_CONSTIO, IMODE_CONST3},
	{INS_SBIS, "sbis", IMODE_CONSTIO, IMODE_CONST3},
	{INS_BRBS, "brbs", IMODE_CONST3, IMODE_CONSTCODE},
	{INS_BRBC, "brbc", IMODE_CONST3, IMODE_CONSTCODE},
	{INS_BREQ, "breq", IMODE_CONSTCODE, IMODE_NONE},
	{INS_BRNE, "brne", IMODE_CONSTCODE, IMODE_NONE},
	{INS_BRCS, "brcs", IMODE_CONSTCODE, IMODE_NONE},
	{INS_BRCC, "brcc", IMODE_CONSTCODE, IMODE_NONE},
	{INS_BRSH, "brsh", IMODE_CONSTCODE, IMODE_NONE},
	{INS_BRLO, "brlo", IMODE_CONSTCODE, IMODE_NONE},
	{INS_BRMI, "brmi", IMODE_CONSTCODE, IMODE_NONE},
	{INS_BRPL, "brpl", IMODE_CONSTCODE, IMODE_NONE},
	{INS_BRGE, "brge", IMODE_CONSTCODE, IMODE_NONE},
	{INS_BRLT, "brlt", IMODE_CONSTCODE, IMODE_NONE},
	{INS_BRHS, "brhs", IMODE_CONSTCODE, IMODE_NONE},
	{INS_BRHC, "brhc", IMODE_CONSTCODE, IMODE_NONE},
	{INS_BRTS, "brts", IMODE_CONSTCODE, IMODE_NONE},
	{INS_BRTC, "brtc", IMODE_CONSTCODE, IMODE_NONE},
	{INS_BRVS, "brvs", IMODE_CONSTCODE, IMODE_NONE},
	{INS_BRVC, "brvc", IMODE_CONSTCODE, IMODE_NONE},
	{INS_BRIE, "brie", IMODE_CONSTCODE, IMODE_NONE},
	{INS_BRID, "brid", IMODE_CONSTCODE, IMODE_NONE},

	{INS_MOV, "mov", IMODE_REG, IMODE_REG},
	{INS_MOVW, "movw", IMODE_REG, IMODE_REG},
	{INS_LDI, "ldi", IMODE_REG, IMODE_CONST8},
	{INS_LDS, "lds", IMODE_REG, IMODE_CONSTMEM},
	{INS_LD, "ld", IMODE_REG, IMODE_XYZ | IMODE_INCDEC},
	{INS_LDD, "ldd", IMODE_REG, IMODE_XYZ},
	{INS_STS, "sts", IMODE_CONSTMEM, IMODE_REG},
	{INS_ST, "st", IMODE_XYZ | IMODE_INCDEC, IMODE_REG},
	{INS_STD, "std", IMODE_XYZ, IMODE_REG},
	{INS_LPM, "lpm", IMODE_REG, IMODE_XYZ | IMODE_INCDEC},
	{INS_ELPM, "elpm", IMODE_REG, IMODE_XYZ | IMODE_INCDEC},
	{INS_SPM, "spm", IMODE_NONE, IMODE_NONE},
	{INS_IN, "in", IMODE_REG, IMODE_CONSTIO},
	{INS_OUT, "out", IMODE_CONSTIO, IMODE_REG},
	{INS_PUSH, "push", IMODE_REG, IMODE_NONE},
	{INS_POP, "pop", IMODE_REG, IMODE_NONE},

	{INS_LSL, "lsl", IMODE_REG, IMODE_NONE},
	{INS_LSR, "lsr", IMODE_REG, IMODE_NONE},
	{INS_ROL, "rol", IMODE_REG, IMODE_NONE},
	{INS_ROR, "ror", IMODE_REG, IMODE_NONE},
	{INS_ASR, "asr", IMODE_REG, IMODE_NONE},
	{INS_SWAP, "swap", IMODE_REG, IMODE_NONE},
	{INS_BSET, "bset", IMODE_CONST3, IMODE_NONE},
	{INS_BCLR, "bclr", IMODE_CONST3, IMODE_NONE},
	{INS_SBI, "sbi", IMODE_CONSTIO, IMODE_CONST3},
	{INS_CBI, "cbi", IMODE_CONSTIO, IMODE_CONST3},
	{INS_BST, "bst", IMODE_REG, IMODE_CONST3},
	{INS_BLD, "bld", IMODE_REG, IMODE_CONST3},
	{INS_SEC, "sec", IMODE_NONE, IMODE_NONE},
	{INS_CLC, "clc", IMODE_NONE, IMODE_NONE},
	{INS_SEN, "sen", IMODE_NONE, IMODE_NONE},
	{INS_CLN, "cln", IMODE_NONE, IMODE_NONE},
	{INS_SEZ, "sez", IMODE_NONE, IMODE_NONE},
	{INS_CLZ, "clz", IMODE_NONE, IMODE_NONE},
	{INS_SEI, "sei", IMODE_NONE, IMODE_NONE},
	{INS_CLI, "cli", IMODE_NONE, IMODE_NONE},
	{INS_SES, "ses", IMODE_NONE, IMODE_NONE},
	{INS_CLS, "cls", IMODE_NONE, IMODE_NONE},
	{INS_SEV, "sev", IMODE_NONE, IMODE_NONE},
	{INS_CLV, "clv", IMODE_NONE, IMODE_NONE},
	{INS_SET, "set", IMODE_NONE, IMODE_NONE},
	{INS_CLT, "clt", IMODE_NONE, IMODE_NONE},
	{INS_SEH, "seh", IMODE_NONE, IMODE_NONE},
	{INS_CLH, "clh", IMODE_NONE, IMODE_NONE},

	{INS_BREAK, "break", IMODE_NONE, IMODE_NONE},
	{INS_NOP, "nop", IMODE_NONE, IMODE_NONE},
	{INS_SLEEP, "sleep", IMODE_NONE, IMODE_NONE},
	{INS_WDR, "wdr", IMODE_NONE, IMODE_NONE},

	{INS_INVALID, NULL, IMODE_NONE, IMODE_NONE}
};

STATICSTRINGHASH(avrinstr_tbl_t *, avrasm_nitable, 5);

typedef struct TAG_submacrosub {
	name_t *findname;
	tnode_t *aparam;
} submacrosub_t;


static avrtarget_t avrasm_ttable[] = {
	/* MCU-ID,	Name,		i-cnt,	i-sze,	flash,	sram-s,	sram-l,	io-l,	eep-s,	bswap	*/
	{AVR_AT90S1200,	"AT90S1200",	4,	2,	1024,	0,	0,	0x40,	64,	0},
	{AVR_ATMEGA328, "ATMEGA328",	26,	2,	32768,	0x100,	2048,	0x100,	1024,	1},
	{AVR_ATMEGA1280, "ATMEGA1280",	57,	4,	131072,	0x200,	8192,	0x200,	4096,	1},
	{AVR_ATMEGA2560, "ATMEGA2560",	57,	4,	262144,	0x200,	8192,	0x200,	4096,	1},
	{AVR_INVALID, NULL, 0, 0, 0, 0, 0, 0, 0, 0}
};


static chook_t *label_chook = NULL;
static int templabel_counter = 0;


/*}}}*/


/*{{{  avrtarget_t *avrasm_findtargetbyname (const char *name)*/
/*
 *	finds a specific MCU target by name.
 *	returns the avrtarget_t structure on success, NULL if not found
 */
avrtarget_t *avrasm_findtargetbyname (const char *name)
{
	int i;

	for (i=0; avrasm_ttable[i].name; i++) {
		if (!strcasecmp (name, avrasm_ttable[i].name)) {
			return &(avrasm_ttable[i]);
		}
	}
	return NULL;
}
/*}}}*/
/*{{{  avrtarget_t *avrasm_findtargetbymark (tnode_t *mark)*/
/*
 *	finds a specific MCU target by some string constant.
 *	returns the avrtarget_t structure on success, NULL if not found
 */
avrtarget_t *avrasm_findtargetbymark (tnode_t *mark)
{
	avrasm_lithook_t *lit;

	if (mark->tag != avrasm.tag_LITSTR) {
		return NULL;
	}
	lit = (avrasm_lithook_t *)tnode_nthhookof (mark, 0);

	return avrasm_findtargetbyname (lit->data);
}
/*}}}*/

/*{{{  int avrasm_getxyzreginfo (tnode_t *node, int *reg, int *prepost, int *offs)*/
/*
 *	extracts XYZ register details from an XYZREG node
 *	returns 0 on success, non-zero on error
 */
int avrasm_getxyzreginfo (tnode_t *node, int *reg, int *prepost, int *offs)
{
	avrasm_xyzhook_t *xyzh;

	if (node->tag != avrasm.tag_XYZREG) {
		nocc_serious ("avrasm_getxyzreginfo(): node not XYZREG, got [%s]", node->tag->name);
		return 1;
	}
	xyzh = (avrasm_xyzhook_t *)tnode_nthhookof (node, 0);

	if (reg) {
		*reg = xyzh->reg;
	}
	if (prepost) {
		*prepost = xyzh->prepost;
	}
	if (offs) {
		*offs = xyzh->offs;
	}
	return 0;
}
/*}}}*/
/*{{{  int avrasm_getlitintval (tnode_t *node)*/
/*
 *	extracts value from a literal integer
 *	abandons compilation if not literal integer
 */
int avrasm_getlitintval (tnode_t *node)
{
	avrasm_lithook_t *lh;

	if (constprop_isconst (node)) {
		return constprop_intvalof (node);
	}
	if (node->tag != avrasm.tag_LITINT) {
		nocc_internal ("avrasm_getlitintval(): not LITINT, got [%s]", node->tag->name);
		return 0;
	}
	lh = (avrasm_lithook_t *)tnode_nthhookof (node, 0);
	return *(int *)(lh->data);
}
/*}}}*/
/*{{{  int avrasm_getlitinsval (tnode_t *node)*/
/*
 *	extracts value from a literal instruction (INS_.. constant)
 *	abandons compilation if not literal instruction
 */
int avrasm_getlitinsval (tnode_t *node)
{
	avrasm_lithook_t *lh;

	if (constprop_isconst (node)) {
		return constprop_intvalof (node);
	}
	if (node->tag != avrasm.tag_LITINS) {
		nocc_internal ("avrasm_getlitinsval(): not LITINS, got [%s]", node->tag->name);
		return 0;
	}
	lh = (avrasm_lithook_t *)tnode_nthhookof (node, 0);
	return *(int *)(lh->data);
}
/*}}}*/
/*{{{  int avrasm_getlitregval (tnode_t *node)*/
/*
 *	extracts value from a literal register
 *	abandons compilation if not literal register
 */
int avrasm_getlitregval (tnode_t *node)
{
	avrasm_lithook_t *lh;

	if (constprop_isconst (node)) {
		return constprop_intvalof (node);
	}
	if (node->tag != avrasm.tag_LITREG) {
		nocc_internal ("avrasm_getlitintval(): not LITREG, got [%s]", node->tag->name);
		return 0;
	}
	lh = (avrasm_lithook_t *)tnode_nthhookof (node, 0);
	return *(int *)(lh->data);
}
/*}}}*/

/*{{{  static avrasm_lithook_t *new_avrasmlithook (void)*/
/*
 *	creates a new avrasm_lithook_t structure
 */
static avrasm_lithook_t *new_avrasmlithook (void)
{
	avrasm_lithook_t *litdata = (avrasm_lithook_t *)smalloc (sizeof (avrasm_lithook_t));

	litdata->data = NULL;
	litdata->len = 0;

	return litdata;
}
/*}}}*/
/*{{{  static void free_avrasmlithook (avrasm_lithook_t *lh)*/
/*
 *	frees an avrasm_lithook_t structure
 */
static void free_avrasmlithook (avrasm_lithook_t *lh)
{
	if (!lh) {
		nocc_warning ("free_avrasmlithook(): NULL pointer!");
		return;
	}
	if (lh->data && lh->len) {
		sfree (lh->data);
		lh->data = NULL;
		lh->len = 0;
	}
	sfree (lh);
	return;
}
/*}}}*/
/*{{{  static avrasm_xyzhook_t *new_avrasmxyzhook (void)*/
/*
 *	creates a new avrasm_xyzhook_t structure
 */
static avrasm_xyzhook_t *new_avrasmxyzhook (void)
{
	avrasm_xyzhook_t *xyzdata = (avrasm_xyzhook_t *)smalloc (sizeof (avrasm_xyzhook_t));

	xyzdata->reg = 0;
	xyzdata->prepost = 0;
	xyzdata->offs = 0;

	return xyzdata;
}
/*}}}*/
/*{{{  static void free_avrasmxyzhook (avrasm_xyzhook_t *xyzh)*/
/*
 *	frees an avrasm_xyzhook_t structure
 */
static void free_avrasmxyzhook (avrasm_xyzhook_t *xyzh)
{
	if (!xyzh) {
		nocc_warning ("free_avrasmxyzhook(): NULL pointer!");
		return;
	}
	sfree (xyzh);
	return;
}
/*}}}*/
/*{{{  static avrasm_uslabhook_t *new_avrasmuslabhook (void)*/
/*
 *	creates a new avrasm_uslabhook_t structure
 */
static avrasm_uslabhook_t *new_avrasmuslabhook (void)
{
	avrasm_uslabhook_t *usdata = (avrasm_uslabhook_t *)smalloc (sizeof (avrasm_uslabhook_t));

	usdata->id = 0;
	usdata->dir = 0;

	return usdata;
}
/*}}}*/
/*{{{  static void free_avrasmublabhook (avrasm_uslabhook_t *ush)*/
/*
 *	frees an avrasm_uslabhook_t structure
 */
static void free_avrasmuslabhook (avrasm_uslabhook_t *ush)
{
	if (!ush) {
		nocc_warning ("free_avrasmuslabhook(): NULL pointer!");
		return;
	}
	sfree (ush);
	return;
}
/*}}}*/

/*{{{  static void *avrasm_nametoken_to_hook (void *ntok)*/
/*
 *	turns a name token into a hooknode for a tag_NAME
 */
static void *avrasm_nametoken_to_hook (void *ntok)
{
	token_t *tok = (token_t *)ntok;
	char *rawname;

	rawname = tok->u.name;
	tok->u.name = NULL;

	lexer_freetoken (tok);

	return (void *)rawname;
}
/*}}}*/
/*{{{  static void *avrasm_regtoken_to_node (void *ntok)*/
/*
 *	turns a token representing a register into a literal (LITREG)
 */
static void *avrasm_regtoken_to_node (void *ntok)
{
	token_t *tok = (token_t *)ntok;
	int r = -1;
	int i;
	tnode_t *node = NULL;
	char xbuf[16];

	for (i=0; i<32; i++) {
		sprintf (xbuf, "r%d", i);
		if (lexer_tokmatchlitstr (tok, xbuf)) {
			r = i;
			break;		/* for() */
		}
	}
	if (r < 0) {
		lexer_error (tok->origin, "expected register identifier, found [%s]", lexer_stokenstr (tok));
	}
	if (r >= 0) {
		avrasm_lithook_t *lh = new_avrasmlithook ();

		lh->len = sizeof (r);
		lh->data = mem_ndup (&r, lh->len);

		node = tnode_create (avrasm.tag_LITREG, SLOCN (tok->origin), lh);
	}

	lexer_freetoken (tok);

	return (void *)node;
}
/*}}}*/
/*{{{  static void *avrasm_instoken_to_node (void *ntok)*/
/*
 *	turns a token representing an instruction into an instruction literal (LITINS)
 */
static void *avrasm_instoken_to_node (void *ntok)
{
	token_t *tok = (token_t *)ntok;
	tnode_t *node = NULL;
	avrinstr_tbl_t *tent;

	if (tok->type != KEYWORD) {
		lexer_error (tok->origin, "instruction \"%s\" is not a keyword token", lexer_stokenstr (tok));
		lexer_freetoken (tok);
		return NULL;
	}
	tent = stringhash_lookup (avrasm_nitable, tok->u.kw->name);

	if (!tent) {
		lexer_error (tok->origin, "unhandled instruction \"%s\"", lexer_stokenstr (tok));
	} else {
		avrasm_lithook_t *lh = new_avrasmlithook ();
		int inum = tent->ins;

		lh->len = sizeof (inum);
		lh->data = mem_ndup (&inum, lh->len);

		node = tnode_create (avrasm.tag_LITINS, SLOCN (tok->origin), lh);
	}

	lexer_freetoken (tok);

	return (void *)node;
}
/*}}}*/
/*{{{  static void *avrasm_stringtoken_to_node (void *ntok)*/
/*
 *	turns a string token in to a LITSTR node
 */
static void *avrasm_stringtoken_to_node (void *ntok)
{
	token_t *tok = (token_t *)ntok;
	tnode_t *node = NULL;
	avrasm_lithook_t *litdata = new_avrasmlithook ();

	if (tok->type != STRING) {
		lexer_error (tok->origin, "expected string, found [%s]", lexer_stokenstr (tok));
		sfree (litdata);
		lexer_freetoken (tok);
		return NULL;
	} 
	litdata->data = string_ndup (tok->u.str.ptr, tok->u.str.len);
	litdata->len = tok->u.str.len;

	node = tnode_create (avrasm.tag_LITSTR, SLOCN (tok->origin), (void *)litdata);
	lexer_freetoken (tok);

	return (void *)node;
}
/*}}}*/
/*{{{  static void *avrasm_integertoken_to_node (void *ntok)*/
/*
 *	turns an integer token into a LITINT node
 */
static void *avrasm_integertoken_to_node (void *ntok)
{
	token_t *tok = (token_t *)ntok;
	tnode_t *node = NULL;
	avrasm_lithook_t *litdata = new_avrasmlithook ();

	if (tok->type != INTEGER) {
		lexer_error (tok->origin, "expected integer, found [%s]", lexer_stokenstr (tok));
		sfree (litdata);
		lexer_freetoken (tok);
		return NULL;
	} 
	litdata->len = sizeof (int);
	litdata->data = mem_ndup (&(tok->u.ival), litdata->len);

	node = tnode_create (avrasm.tag_LITINT, SLOCN (tok->origin), (void *)litdata);
	lexer_freetoken (tok);

	return (void *)node;
}
/*}}}*/
/*{{{  static void avrasm_xyzreduce (dfastate_t *dfast, parsepriv_t *pp, void *rarg)*/
/*
 *	scoops up tokens on the local stack for XYZ register things
 */
static void avrasm_xyzreduce (dfastate_t *dfast, parsepriv_t *pp, void *rarg)
{
	token_t *tok = parser_gettok (pp);
	avrasm_xyzhook_t *xyzh = new_avrasmxyzhook ();
	tnode_t *node;
	int gotoffs = 0;

	if (tok->type == INTEGER) {
		/* is an offset */
		xyzh->offs = tok->u.ival;
		lexer_freetoken (tok);
		tok = parser_gettok (pp);
		gotoffs = 1;
	} else {
		xyzh->offs = 0;
	}

	if (lexer_tokmatch (avrasm.tok_PLUS, tok)) {
		/* post-increment */
		if (!gotoffs) {
			xyzh->prepost = 1;
		} /* else it's of the form "Y+n", etc. */
		lexer_freetoken (tok);
		tok = parser_gettok (pp);
	} else {
		xyzh->prepost = 0;
	}

	if (lexer_tokmatch (avrasm.tok_REGX, tok)) {
		xyzh->reg = 0;
	} else if (lexer_tokmatch (avrasm.tok_REGY, tok)) {
		xyzh->reg = 1;
	} else if (lexer_tokmatch (avrasm.tok_REGZ, tok)) {
		xyzh->reg = 2;
	} else {
		parser_error (SLOCL (pp->lf, tok->lineno), "invalid token in avrasm_xyzreduce(): got [%s]", lexer_stokenstr (tok));
	}

#if 0
fprintf (stderr, "avrasm_xyzreduce(): tok =\n");
lexer_dumptoken (stderr, tok);
#endif
	lexer_freetoken (tok);
	tok = parser_peekemptytok (pp);
	if (tok && lexer_tokmatch (avrasm.tok_MINUS, tok)) {
		tok = parser_gettok (pp);
		/* pre-decrement */
		xyzh->prepost = -1;
		lexer_freetoken (tok);
	}

	node = tnode_create (avrasm.tag_XYZREG, SLOCN (pp->lf), xyzh);
	*(dfast->ptr) = node;

	return;
}
/*}}}*/
/*{{{  static void avrasm_uslabdefreduce (dfastate_t *dfast, parsepriv_t *pp, void *rarg)*/
/*
 *	scoops up a token used to define a "local label".
 *	done specially because the lexer sees "L0" as a single name;  we just want the integer bit here.
 */
static void avrasm_uslabdefreduce (dfastate_t *dfast, parsepriv_t *pp, void *rarg)
{
	token_t *tok = parser_gettok (pp);
	int id;
	tnode_t *node = NULL;
	avrasm_lithook_t *litdata;
	char *ch;
	int onlybin = 1;
	
	if (tok->type != NAME) {
		parser_error (SLOCL (pp->lf, tok->lineno), "invalid token in avrasm_uslabdefreduce(): got [%s]", lexer_stokenstr (tok));
		lexer_freetoken (tok);
		return;
	}
	if (strlen (tok->u.name) < 2) {
		goto out_badname;
	}
	if ((tok->u.name[0] != 'L') && (tok->u.name[0] != 'l')) {
		goto out_badname;
	}
	if (sscanf (tok->u.name + 1, "%d", &id) != 1) {
		goto out_badname;
	}
	if (id < 0) {
		goto out_badname;
	}

	/* sanity check: if this could be parsed as a binary number (followed by 'b') warn about it */
	for (ch = tok->u.name + 1; *ch != '\0'; ch++) {
		if ((*ch != '0') && (*ch != '1')) {
			onlybin = 0;
			break;
		}
	}
	if (onlybin && (strlen (tok->u.name) > 3)) {
		parser_warning (SLOCL (pp->lf, tok->lineno), "backward references to this may be interpreted as binary digits");
	}

	litdata = new_avrasmlithook ();
	litdata->len = sizeof (int);
	litdata->data = mem_ndup (&id, litdata->len);

	node = tnode_create (avrasm.tag_LITINT, SLOCN (tok->origin), (void *)litdata);
	node = tnode_create (avrasm.tag_LLABELDEF, SLOCN (tok->origin), node);
	*(dfast->ptr) = node;
	lexer_freetoken (tok);
	return;
out_badname:
	parser_error (SLOCL (pp->lf, tok->lineno), "invalid local label name [%s]", tok->u.name);
	lexer_freetoken (tok);
	return;
}
/*}}}*/
/*{{{  static void avrasm_uslabreduce (dfastate_t *dfast, parsepriv_t *pp, void *rarg)*/
/*
 *	scoops up one or more tokens referencing a "local label".
 */
static void avrasm_uslabreduce (dfastate_t *dfast, parsepriv_t *pp, void *rarg)
{
	token_t *tok = parser_gettok (pp);
	tnode_t *node = NULL;
	avrasm_uslabhook_t *ush = NULL;
	avrasm_lspecial_t *lspec = NULL;
	int slen;

	if (tok->type != LSPECIAL) {
		parser_error (SLOCL (pp->lf, tok->lineno), "invalid token in avrasm_uslabreduce(): got [%s]", lexer_stokenstr (tok));
		lexer_freetoken (tok);
		return;
	}

	/* expecting a string like "0f" or "2b" to reference a local label */
	lspec = (avrasm_lspecial_t *)tok->u.lspec;
	ush = new_avrasmuslabhook ();
	slen = strlen (lspec->str);
	if (slen < 2) {
		goto out_badname;
	}
	if (lspec->str[slen - 1] == 'f') {
		ush->dir = 1;
	} else if (lspec->str[slen - 1] == 'b') {
		ush->dir = -1;
	} else {
		goto out_badname;
	}
	lspec->str[slen - 1] = '\0';
	if (sscanf (lspec->str, "%d", &ush->id) != 1) {
		/* patch it back together.. */
		lspec->str[slen - 1] = (ush->dir > 0) ? 'f' : 'b';
		goto out_badname;
	}

	node = tnode_create (avrasm.tag_USLAB, SLOCN (tok->origin), (void *)ush);
	*(dfast->ptr) = node;

	lexer_freetoken (tok);
	return;
out_badname:
	parser_error (SLOCL (pp->lf, tok->lineno), "invalid local label reference [%s]", lspec->str);
	lexer_freetoken (tok);
	return;
}
/*}}}*/

/*{{{  tnode_t *avrasm_newlitins (tnode_t *orgnode, int ins)*/
/*
 *	creates a new literal instruction node
 */
tnode_t *avrasm_newlitins (tnode_t *orgnode, int ins)
{
	tnode_t *node = NULL;
	avrasm_lithook_t *lh = new_avrasmlithook ();

	lh->len = sizeof (ins);
	lh->data = mem_ndup (&ins, lh->len);

	node = tnode_createfrom (avrasm.tag_LITINS, orgnode, lh);

	return node;
}
/*}}}*/
/*{{{  tnode_t *avrasm_newlitint (tnode_t *orgnode, int val)*/
/*
 *	creates a new literal instruction node
 */
tnode_t *avrasm_newlitint (tnode_t *orgnode, int val)
{
	tnode_t *node = NULL;
	avrasm_lithook_t *lh = new_avrasmlithook ();

	lh->len = sizeof (val);
	lh->data = mem_ndup (&val, lh->len);

	node = tnode_createfrom (avrasm.tag_LITINT, orgnode, lh);

	return node;
}
/*}}}*/
/*{{{  tnode_t *avrasm_newlitreg (tnode_t *orgnode, int reg)*/
/*
 *	creates a new literal register node
 */
tnode_t *avrasm_newlitreg (tnode_t *orgnode, int reg)
{
	tnode_t *node = NULL;
	avrasm_lithook_t *lh = new_avrasmlithook ();

	lh->len = sizeof (reg);
	lh->data = mem_ndup (&reg, lh->len);

	node = tnode_createfrom (avrasm.tag_LITREG, orgnode, lh);

	return node;
}
/*}}}*/
/*{{{  tnode_t *avrasm_newxyzreginfo (tnode_t *orgnode, int reg, int prepost, int offs)*/
/*
 *	creates a new XYZ register node
 */
tnode_t *avrasm_newxyzreginfo (tnode_t *orgnode, int reg, int prepost, int offs)
{
	avrasm_xyzhook_t *xyzh = new_avrasmxyzhook ();
	tnode_t *node;

	xyzh->reg = reg;
	xyzh->prepost = prepost;
	xyzh->offs = offs;

	node = tnode_createfrom (avrasm.tag_XYZREG, orgnode, xyzh);
	return node;
}
/*}}}*/

/*{{{  name_t *avrasm_newtemplabel (tnode_t *orgnode, tnode_t **labdecl, tnode_t **labname)*/
/*
 *	creates a new temporary label (GLABEL) and returns it; creates declaration node regardless.
 */
name_t *avrasm_newtemplabel (tnode_t *orgnode, tnode_t **labdecl, tnode_t **labname)
{
	char *tmpname = string_fmt ("TL%d", templabel_counter);
	name_t *lname;
	tnode_t *name = tnode_createfrom (avrasm.tag_GLABEL, orgnode, NULL);
	tnode_t *decl = tnode_createfrom (avrasm.tag_GLABELDEF, orgnode, name);

	templabel_counter++;
	lname = name_addname (tmpname, decl, NULL, name);
	tnode_setnthname (name, 0, lname);

	if (labdecl) {
		*labdecl = decl;
	}
	if (labname) {
		*labname = name;
	}

	return lname;
}
/*}}}*/

/*{{{  static void avrasm_rawnamenode_hook_free (void *hook)*/
/*
 *	frees a rawnamenode hook (name-bytes)
 */
static void avrasm_rawnamenode_hook_free (void *hook)
{
	if (hook) {
		sfree (hook);
	}
	return;
}
/*}}}*/
/*{{{  static void *avrasm_rawnamenode_hook_copy (void *hook)*/
/*
 *	copies a rawnamenode hook (name-bytes)
 */
static void *avrasm_rawnamenode_hook_copy (void *hook)
{
	char *rawname = (char *)hook;

	if (rawname) {
		return string_dup (rawname);
	}
	return NULL;
}
/*}}}*/
/*{{{  static void avrasm_rawnamenode_hook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)*/
/*
 *	dump-tree for rawnamenode hook (name-bytes)
 */
static void avrasm_rawnamenode_hook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)
{
	avrasm_isetindent (stream, indent);
	fhandle_printf (stream, "<avrasmrawnamenode value=\"%s\" />\n", hook ? (char *)hook : "(null)");
	return;
}
/*}}}*/

/*{{{  static void avrasm_litnode_hook_free (void *hook)*/
/*
 *	frees a litnode hook
 */
static void avrasm_litnode_hook_free (void *hook)
{
	avrasm_lithook_t *ld = (avrasm_lithook_t *)hook;

	if (ld) {
		free_avrasmlithook (ld);
	}

	return;
}
/*}}}*/
/*{{{  static void *avrasm_litnode_hook_copy (void *hook)*/
/*
 *	copies a litnode hook (name-bytes)
 */
static void *avrasm_litnode_hook_copy (void *hook)
{
	avrasm_lithook_t *lit = (avrasm_lithook_t *)hook;

	if (lit) {
		avrasm_lithook_t *newlit = new_avrasmlithook ();

		newlit->data = lit->data ? mem_ndup (lit->data, lit->len) : NULL;
		newlit->len = lit->len;

		return (void *)newlit;
	}
	return NULL;
}
/*}}}*/
/*{{{  static void avrasm_litnode_hook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)*/
/*
 *	dump-tree for litnode hook (name-bytes)
 */
static void avrasm_litnode_hook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)
{
	avrasm_lithook_t *lit = (avrasm_lithook_t *)hook;

	avrasm_isetindent (stream, indent);
	if (node->tag == avrasm.tag_LITINS) {
		char *sdata = mkhexbuf ((unsigned char *)lit->data, lit->len);
		int iidx;
		int ins = *(int *)lit->data;

		for (iidx=0; avrasm_itable[iidx].str; iidx++) {
			if (avrasm_itable[iidx].ins == ins) {
				break;			/* for() */
			}
		}
		if (avrasm_itable[iidx].str) {
			fhandle_printf (stream, "<avrasmlitnode instr=\"%s\" />\n", avrasm_itable[iidx].str);
		} else {
			fhandle_printf (stream, "<avrasmlitnode size=\"%d\" value=\"%s\" />\n", lit ? lit->len : 0, sdata);
		}

		sfree (sdata);
	} else if (node->tag == avrasm.tag_LITSTR) {
		fhandle_printf (stream, "<avrasmlitnode size=\"%d\" value=\"%s\" />\n", lit ? lit->len : 0, (lit && lit->data) ? lit->data : "(null)");
	} else if (node->tag == avrasm.tag_LITREG) {
		fhandle_printf (stream, "<avrasmlitnode size=\"%d\" value=\"r%d\" />\n", lit ? lit->len : 0, (lit && lit->data) ? *(int *)(lit->data) : -1);
	} else {
		char *sdata = mkhexbuf ((unsigned char *)lit->data, lit->len);

		fhandle_printf (stream, "<avrasmlitnode size=\"%d\" value=\"%s\" />\n", lit ? lit->len : 0, sdata);
		sfree (sdata);
	}

	return;
}
/*}}}*/

/*{{{  static void avrasm_xyznode_hook_free (void *hook)*/
/*
 *	frees an xyznode hook
 */
static void avrasm_xyznode_hook_free (void *hook)
{
	avrasm_xyzhook_t *hh = (avrasm_xyzhook_t *)hook;

	if (hh) {
		free_avrasmxyzhook (hh);
	}

	return;
}
/*}}}*/
/*{{{  static void *avrasm_xyznode_hook_copy (void *hook)*/
/*
 *	copies an xyznode hook
 */
static void *avrasm_xyznode_hook_copy (void *hook)
{
	avrasm_xyzhook_t *xyzh = (avrasm_xyzhook_t *)hook;

	if (xyzh) {
		avrasm_xyzhook_t *newxyzh = new_avrasmxyzhook ();

		newxyzh->reg = xyzh->reg;
		newxyzh->prepost = xyzh->prepost;
		newxyzh->offs = xyzh->offs;

		return (void *)newxyzh;
	}
	return NULL;
}
/*}}}*/
/*{{{  static void avrasm_xyznode_hook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)*/
/*
 *	dump-tree for xyznode hook (specials)
 */
static void avrasm_xyznode_hook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)
{
	avrasm_xyzhook_t *xyzh = (avrasm_xyzhook_t *)hook;

	avrasm_isetindent (stream, indent);
	fhandle_printf (stream, "<avrasmxyznode reg=\"%d\" prepost=\"%d\" offs=\"%d\" />\n", xyzh->reg, xyzh->prepost, xyzh->offs);

	return;
}
/*}}}*/

/*{{{  static void avrasm_uslabnode_hook_free (void *hook)*/
/*
 *	frees an uslabnode hook
 */
static void avrasm_uslabnode_hook_free (void *hook)
{
	avrasm_uslabhook_t *ush = (avrasm_uslabhook_t *)hook;

	if (ush) {
		free_avrasmuslabhook (ush);
	}

	return;
}
/*}}}*/
/*{{{  static void *avrasm_uslabnode_hook_copy (void *hook)*/
/*
 *	copies an uslabnode hook
 */
static void *avrasm_uslabnode_hook_copy (void *hook)
{
	avrasm_uslabhook_t *ush = (avrasm_uslabhook_t *)hook;

	if (ush) {
		avrasm_uslabhook_t *newush = new_avrasmuslabhook ();

		newush->id = ush->id;
		newush->dir = ush->dir;

		return (void *)newush;
	}
	return NULL;
}
/*}}}*/
/*{{{  static void avrasm_uslabnode_hook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)*/
/*
 *	dump-tree for uslabnode hook (specials)
 */
static void avrasm_uslabnode_hook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)
{
	avrasm_uslabhook_t *ush = (avrasm_uslabhook_t *)hook;

	avrasm_isetindent (stream, indent);
	fhandle_printf (stream, "<avrasmuslabnode id=\"%d\" dir=\"%d\" />\n", ush->id, ush->dir);

	return;
}
/*}}}*/

/*{{{  static int avrasm_prescope_macrodef (compops_t *cops, tnode_t **node, prescope_t *ps)*/
/*
 *	pre-scopes a macro definition
 *	returns 0 to stop walk, 1 to continue
 */
static int avrasm_prescope_macrodef (compops_t *cops, tnode_t **node, prescope_t *ps)
{
	tnode_t **paramptr = tnode_nthsubaddr (*node, 1);

	if (!*paramptr) {
		/* no parameters, but create empty list for it */
		*paramptr = parser_newlistnode ((*node)->org);
	} else {
		tnode_t **plist;
		int nitems, i;

		plist = parser_getlistitems (*paramptr, &nitems);
		for (i=0; i<nitems; i++) {
			tnode_t *tmpparam = tnode_createfrom (avrasm.tag_FPARAM, plist[i], plist[i]);

			plist[i] = tmpparam;
		}
	}
	return 1;
}
/*}}}*/
/*{{{  static int avrasm_scopein_macrodef (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	scopes in a macro definition
 *	returns 0 to stop walk, 1 to continue
 */
static int avrasm_scopein_macrodef (compops_t *cops, tnode_t **node, scope_t *ss)
{
	tnode_t **namep = tnode_nthsubaddr (*node, 0);

	if ((*namep)->tag != avrasm.tag_NAME) {
		scope_error (*node, ss, "macro name is not a name");
	} else {
		char *rawname = (char *)tnode_nthhookof (*namep, 0);
		name_t *mname;
		tnode_t *namenode;
		void *nsmark = name_markscope ();

		/* scope parameters and body */
		tnode_modprepostwalktree (tnode_nthsubaddr (*node, 1), scope_modprewalktree, scope_modpostwalktree, (void *)ss);
		tnode_modprepostwalktree (tnode_nthsubaddr (*node, 2), scope_modprewalktree, scope_modpostwalktree, (void *)ss);

		name_markdescope (nsmark);

		mname = name_addscopenamess (rawname, *node, NULL, NULL, ss);
		namenode = tnode_createfrom (avrasm.tag_MACRONAME, *node, mname);
		SetNameNode (mname, namenode);

		tnode_free (*namep);
		*namep = namenode;

		ss->scoped++;
	}
	return 0;
}
/*}}}*/
/*{{{  static int avrasm_typecheck_macrodef (compops_t *cops, tnode_t *node, typecheck_t *tc)*/
/*
 *	does type-checking for a macro definition -- no-op
 *	returns 0 to stop walk, 1 to continue
 */
static int avrasm_typecheck_macrodef (compops_t *cops, tnode_t *node, typecheck_t *tc)
{
	return 0;
}
/*}}}*/

/*{{{  static int avrasm_constprop_litnode (compops_t *cops, tnode_t **tptr)*/
/*
 *	does constant propagation on literal nodes (integers only)
 *	returns 0 to stop walk, 1 to continue (meaningless in post-walk)
 */
static int avrasm_constprop_litnode (compops_t *cops, tnode_t **tptr)
{
#if 0
fprintf (stderr, "avrasm_constprop_litnode(): here!\n");
#endif
	if ((*tptr)->tag == avrasm.tag_LITINT) {
		avrasm_lithook_t *lit = (avrasm_lithook_t *)tnode_nthhookof (*tptr, 0);
		int val = *(int *)(lit->data);

		*tptr = constprop_newconst (CONST_INT, *tptr, NULL, val);
	} else if ((*tptr)->tag == avrasm.tag_LITREG) {
		avrasm_lithook_t *lit = (avrasm_lithook_t *)tnode_nthhookof (*tptr, 0);
		int val = *(int *)(lit->data);

		*tptr = constprop_newconst (CONST_INT, *tptr, NULL, val);
	}	
	return 1;
}
/*}}}*/
/*{{{  static int64_t avrasm_isconst_litnode (langops_t *lops, tnode_t *node)*/
/*
 *	returns non-zero if a constant (should always be true)
 */
static int64_t avrasm_isconst_litnode (langops_t *lops, tnode_t *node)
{
	avrasm_lithook_t *lit = (avrasm_lithook_t *)tnode_nthhookof (node, 0);

	return (int64_t)lit->len;
}
/*}}}*/
/*{{{  static int64_t avrasm_constvalof_litnode (langops_t *lops, tnode_t *node, void *ptr)*/
/*
 *	extracts the value of a constant (for all numeric constant types)
 */
static int64_t avrasm_constvalof_litnode (langops_t *lops, tnode_t *node, void *ptr)
{
	avrasm_lithook_t *lit = (avrasm_lithook_t *)tnode_nthhookof (node, 0);

	if ((node->tag == avrasm.tag_LITINT) || (node->tag == avrasm.tag_LITREG)) {
		return (int64_t)(*(int *)(lit->data));
	}
	return 0;
}
/*}}}*/

/*{{{  static int avrasm_scopein_fparamnode (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	scopes in a formal parameter (macros)
 *	returns 0 to stop walk, 1 to continue
 */
static int avrasm_scopein_fparamnode (compops_t *cops, tnode_t **node, scope_t *ss)
{
	tnode_t **namep = tnode_nthsubaddr (*node, 0);

	if ((*namep)->tag != avrasm.tag_NAME) {
		scope_error (*node, ss, "parameter name is not a name");
	} else {
		char *rawname = (char *)tnode_nthhookof (*namep, 0);
		name_t *fpname;
		tnode_t *namenode;

		fpname = name_addscopenamess (rawname, *node, NULL, NULL, ss);
		namenode = tnode_createfrom (avrasm.tag_PARAMNAME, *node, fpname);
		SetNameNode (fpname, namenode);

		tnode_free (*namep);
		*namep = namenode;

		ss->scoped++;
	}
	return 1;
}
/*}}}*/

/*{{{  static int avrasm_prescope_instancenode (compops_t *cops, tnode_t **node, prescope_t *ps)*/
/*
 *	pre-scopes an instance node
 *	returns 0 to stop walk, 1 to continue
 */
static int avrasm_prescope_instancenode (compops_t *cops, tnode_t **node, prescope_t *ps)
{
	tnode_t **apptr = tnode_nthsubaddr (*node, 1);

	if (!*apptr) {
		*apptr = parser_newlistnode ((*node)->org);
	}
	return 1;
}
/*}}}*/
/*{{{  static int avrasm_submacro_instancenode_subbody (tnode_t **node, submacrosub_t *sms)*/
/*
 *	does substitutions inside a tree for macro parameters
 *	returns 0 to stop walk, 1 to continue
 */
static int avrasm_submacro_instancenode_subbody (tnode_t **node, submacrosub_t *sms)
{
	if ((*node)->tag == avrasm.tag_PARAMNAME) {
		name_t *thisname = tnode_nthnameof (*node, 0);

		if (thisname == sms->findname) {
			// tnode_free (*node);
			*node = tnode_copytree (sms->aparam);
		}
	}
	return 1;
}
/*}}}*/
/*{{{  static int avrasm_submacro_instancenode (compops_t *cops, tnode_t **node, submacro_t *sm)*/
/*
 *	does macro substitution on an instance node
 *	returns 0 to stop walk, 1 to continue
 */
static int avrasm_submacro_instancenode (compops_t *cops, tnode_t **node, submacro_t *sm)
{
	tnode_t *macnnode = tnode_nthsubof (*node, 0);
	name_t *macname;
	tnode_t *macdecl, *macbody;
	tnode_t *fparamlist, *aparamlist;
	int nfparams, naparams;
	tnode_t **fparams, **aparams;
	tnode_t *bodycopy;

	if (macnnode->tag != avrasm.tag_MACRONAME) {
		tnode_error (*node, "instance of macro is not a macro name [%s]", macnnode->tag->name);
		sm->errcount++;
		return 0;
	}

	macname = tnode_nthnameof (macnnode, 0);
	macdecl = NameDeclOf (macname);
	macbody = tnode_nthsubof (macdecl, 2);
	fparamlist = tnode_nthsubof (macdecl, 1);
	aparamlist = tnode_nthsubof (*node, 1);

	if (!parser_islistnode (fparamlist)) {
		tnode_error (*node, "macro [%s] has no formal parameter list..", NameNameOf (macname));
		sm->errcount++;
		return 0;
	} else if (!parser_islistnode (aparamlist)) {
		tnode_error (*node, "instance of macro [%s] has no actual parameters..", NameNameOf (macname));
		sm->errcount++;
		return 0;
	}

	fparams = parser_getlistitems (fparamlist, &nfparams);
	aparams = parser_getlistitems (aparamlist, &naparams);

	if (naparams > nfparams) {
		tnode_error (*node, "too many parameters in instance of macro [%s]", NameNameOf (macname));
		sm->errcount++;
		return 0;
	} else if (naparams < nfparams) {
		tnode_error (*node, "too few parameters in instance of macro [%s]", NameNameOf (macname));
		sm->errcount++;
		return 0;
	}

	bodycopy = tnode_copytree (macbody);
	/* ASSERT: bodybody is a list of things */
	if (!parser_islistnode (bodycopy)) {
		nocc_serious ("copied macro body, but not a list..");
		return 0;
	} else {
		tnode_t **bitems;
		int nbitems, j;

		bitems = parser_getlistitems (bodycopy, &nbitems);

		for (j=0; j<nbitems; j++) {
			/* move origin to instance */
			bitems[j]->org = (*node)->org;
		}
	}
#if 0
fprintf (stderr, "avrasm_submacro_instancenode(): original body=\n");
tnode_dumptree (macbody, 1, stderr);
fprintf (stderr, "copy=\n");
tnode_dumptree (bodycopy, 1, stderr);
fprintf (stderr, "fparams=\n");
tnode_dumptree (fparamlist, 1, stderr);
fprintf (stderr, "aparams=\n");
tnode_dumptree (aparamlist, 1, stderr);
#endif

	if (nfparams > 0) {
		submacrosub_t *sms = NULL;
		int i;

		sms = (submacrosub_t *)smalloc (sizeof (submacrosub_t));
		for (i=0; i<nfparams; i++) {
			sms->findname = tnode_nthnameof (tnode_nthsubof (fparams[i], 0), 0);
			sms->aparam = aparams[i];
			tnode_modprewalktree (&bodycopy, (int (*)(tnode_t **, void *))avrasm_submacro_instancenode_subbody, sms);
		}
		sfree (sms);
	}

	// tnode_free (*node);
	*node = bodycopy;

	return 1;
}
/*}}}*/

/*{{{  static int avrasm_scopein_rawname (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	scopes in a free-floating name
 *	returns 0 to stop walk, 1 to continue
 */
static int avrasm_scopein_rawname (compops_t *cops, tnode_t **node, scope_t *ss)
{
	tnode_t *name = *node;
	char *rawname;
	name_t *sname = NULL;

	if (name->tag != avrasm.tag_NAME) {
		scope_error (name, ss, "name not raw-name!");
		return 0;
	}
	rawname = tnode_nthhookof (name, 0);

	sname = name_lookupss (rawname, ss);
	if (sname) {
		/* resolved */
		*node = NameNodeOf (sname);
		tnode_free (name);
	} else {
		scope_error (name, ss, "unresolved name \"%s\"", rawname);
	}
	return 1;
}
/*}}}*/
/*{{{  static int avrasm_scopein_equnode (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	scopes in an EQU or DEF definition
 *	returns 0 to stop walk, 1 to continue
 */
static int avrasm_scopein_equnode (compops_t *cops, tnode_t **node, scope_t *ss)
{
	tnode_t **namep = tnode_nthsubaddr (*node, 0);
	
	if ((*namep)->tag != avrasm.tag_NAME) {
		scope_error (*node, ss, "equ/def name is not a name");
	} else {
		char *rawname = (char *)tnode_nthhookof (*namep, 0);
		name_t *ename;
		tnode_t *namenode;

		ename = name_addscopenamess (rawname, *node, NULL, NULL, ss);
		namenode = tnode_createfrom (avrasm.tag_EQUNAME, *node, ename);
		SetNameNode (ename, namenode);

		tnode_free (*namep);
		*namep = namenode;

		ss->scoped++;
	}
	return 1;
}
/*}}}*/
/*{{{  static int avrasm_subequ_equnode (compops_t *cops, tnode_t **tptr, subequ_t *se)*/
/*
 *	does EQU and DEF substitutions on a EQU/DEF definition (RHS adjust)
 *	returns 0 to stop walk, 1 to continue
 */
static int avrasm_subequ_equnode (compops_t *cops, tnode_t **tptr, subequ_t *se)
{
	tnode_t **exprp = tnode_nthsubaddr (*tptr, 1);

#if 0
fprintf (stderr, "avrasm_subequ_equnode(): here, node is:\n");
tnode_dumptree (*tptr, 1, stderr);
#endif
	avrasm_subequ_subtree (exprp, se);
	return 0;
}
/*}}}*/

/*{{{  static int avrasm_llscope_uslabnode (compops_t *cops, tnode_ **tptr, void *lls)*/
/*
 *	does local-label scoping on a USLAB node (unscoped label)
 *	returns 0 to stop walk, 1 to continue
 */
static int avrasm_llscope_uslabnode (compops_t *cops, tnode_t **tptr, void *lls)
{
	avrasm_uslabhook_t *uslh = (avrasm_uslabhook_t *)tnode_nthhookof (*tptr, 0);

#if 0
fprintf (stderr, "avrasm_llscope_uslabnode(): here!, id=%d, dir=%d\n", uslh->id, uslh->dir);
#endif
	avrasm_llscope_fixref (tptr, uslh->id, uslh->dir, lls);
	return 1;
}
/*}}}*/

/*{{{  static int avrasm_subequ_namenode (compops_t *cops, tnode_t **tptr, subequ_t *se)*/
/*
 *	does EQU and DEF substitutions on a namenode (EQU)
 *	returns 0 to stop walk, 1 to continue
 */
static int avrasm_subequ_namenode (compops_t *cops, tnode_t **tptr, subequ_t *se)
{
	if ((*tptr)->tag == avrasm.tag_EQUNAME) {
		name_t *name = tnode_nthnameof (*tptr, 0);
		tnode_t *decl = NameDeclOf (name);

		if (!decl) {
			nocc_serious ("avrasm_subequ_namenode(): no declaration for EQUNAME!");
			se->errcount++;
		} else if ((decl->tag == avrasm.tag_EQU) || (decl->tag == avrasm.tag_DEF)) {
			tnode_t *rhs = tnode_nthsubof (decl, 1);

#if 0
fprintf (stderr, "avrasm_subequ_namenode(): here, rhs of decl is:\n");
tnode_dumptree (rhs, 1, stderr);
#endif
			// tnode_free (*tptr);
			*tptr = tnode_copytree (rhs);
		} else {
			nocc_serious ("avrasm_subequ_namenode(): bad declaration for EQUNAME!");
			se->errcount++;
		}
	}
	return 1;
}
/*}}}*/
/*{{{  static int64_t avrasm_getname_namenode (langops_t *lops, tnode_t *node, char **str)*/
/*
 *	does get-name for an AVRASM name
 *	returns 0 to stop walk, 1 to continue
 */
static int64_t avrasm_getname_namenode (langops_t *lops, tnode_t *node, char **str)
{
	char *tmp;
	name_t *name = tnode_nthnameof (node, 0);

	if (NameNameOf (name)) {
		tmp = string_dup (NameNameOf (name));
	} else {
		tmp = string_fmt ("(anonymous)");
	}

	if (*str) {
		sfree (*str);
	}
	*str = tmp;
	return 0;
}
/*}}}*/

/*{{{  static int avrasm_prescope_targetnode (compops_t *cops, tnode_t **tptr, prescope_t *ps)*/
/*
 *	does pre-scope for a target node (.target or .mcu)
 *	returns 0 to stop walk, 1 to continue
 */
static int avrasm_prescope_targetnode (compops_t *cops, tnode_t **tptr, prescope_t *ps)
{
	if ((*tptr)->tag == avrasm.tag_TARGETMARK) {
		tnode_t *expr = tnode_nthsubof (*tptr, 0);
		avrasm_lithook_t *lh;
		char *rawstr;
		char *tcpu = NULL, *tvendor = NULL, *tos = NULL;
		char *ch;

		if (expr->tag != avrasm.tag_LITSTR) {
			prescope_error (*tptr, ps, ".target expression is not a string");
			return 0;
		}
		lh = (avrasm_lithook_t *)tnode_nthhookof (expr, 0);
		rawstr = string_ndup (lh->data, lh->len);

		for (ch=rawstr; (*ch != '\0') && (*ch != '-'); ch++);
		tcpu = string_ndup (rawstr, (int)(ch - rawstr));
		if (*ch == '-') {
			char *dh;

			for (dh = ++ch; (*ch != '\0') && (*ch != '-'); ch++);
			tvendor = string_ndup (dh, (int)(ch - dh));
			if (*ch == '-') {
				ch++;
				tos = string_dup (ch);
			}
		}
		sfree (rawstr);

		/* attempt to set default compiler target */
		nocc_setdefaulttarget (tcpu, tvendor, tos);

		sfree (tcpu);
		if (tvendor) {
			sfree (tvendor);
		}
		if (tos) {
			sfree (tos);
		}
	} else if ((*tptr)->tag == avrasm.tag_MCUMARK) {
		tnode_t *expr = tnode_nthsubof (*tptr, 0);

		if (expr->tag != avrasm.tag_LITSTR) {
			prescope_error (*tptr, ps, ".mcu expression is not a string");
			return 0;
		}

		/* this gets left in the tree, dealt with later on */
	}

	return 1;
}
/*}}}*/

/*{{{  static int avrasm_typecheck_xyznode (compops_t *cops, tnode_t *node, typecheck_t *tc)*/
/*
 *	does type-check for an XYZ node, makes sure offset is in range (if used)
 *	returns 0 to stop walk, 1 to continue
 */
static int avrasm_typecheck_xyznode (compops_t *cops, tnode_t *node, typecheck_t *tc)
{
	avrasm_xyzhook_t *xyzh = (avrasm_xyzhook_t *)tnode_nthhookof (node, 0);

	if (node->tag == avrasm.tag_XYZREG) {
		if ((xyzh->offs < 0) || (xyzh->offs > 63)) {
			typecheck_error (node, tc, "constant offset %d out of range [0 - 63]", xyzh->offs);
			return 0;
		}
	}
	return 1;
}
/*}}}*/

/*{{{  int avrasm_inseg_true (langops_t *lops, tnode_t *node)*/
/*
 *	returns true for nodes that should be inside a segment (e.g. instruction, label, .org, etc.)
 */
int avrasm_inseg_true (langops_t *lops, tnode_t *node)
{
	return 1;
}
/*}}}*/

/*{{{  static int avrasm_check_insarg (avrinstr_tbl_t *ins, int argnum, avrinstr_mode_e mode, tnode_t *node, tnode_t *arg, typecheck_t *tc, int trpass)*/
/*
 *	checks that an instruction's arguments match the usage context.
 *	returns 0 on success, non-zero on failure (also reports error via typechecker)
 */
static int avrasm_check_insarg (avrinstr_tbl_t *ins, int argnum, avrinstr_mode_e mode, tnode_t *node, tnode_t *arg, typecheck_t *tc, int trpass)
{
	int good;

	if ((mode == IMODE_NONE) && arg) {
		typecheck_error (node, tc, "unexpected argument %d for \"%s\" instruction", argnum, ins->str);
		return 1;
	} else if (mode == IMODE_NONE) {
		return 0;
	}
	if (!arg) {
		typecheck_error (node, tc, "missing argument %d for \"%s\" instruction", argnum, ins->str);
		return 1;
	}

	/* assume bad to start with */
	good = 0;

	if (mode & IMODE_REG) {
		/*{{{  expecting register*/
		if (arg->tag == avrasm.tag_LITREG) {
			good = 1;
		} else if (trpass && constprop_isconst (arg)) {
			/* allow constant after constprop pass (before typeresolve) */
			good = 1;
		}
		/*}}}*/
	}
	if (mode & IMODE_XYZ) {
		/*{{{  expecting X, Y or Z register*/
		if (arg->tag == avrasm.tag_XYZREG) {
			good = 1;
		}
		/*}}}*/
	}
	if (mode & IMODE_CONST8) {
		/*{{{  expecting 8-bit constant*/
		if (arg->tag == avrasm.tag_LITINT) {
			good = 1;
		} else if (constprop_isconst (arg)) {
			good = 1;
		} else if ((arg->tag == avrasm.tag_GLABEL) || (arg->tag == avrasm.tag_LLABEL)) {
			/* allow labels, will hurl error later if too wide */
			good = 1;
		} else if (arg->tag->ndef == avrasm.node_DOPNODE) {
			/* check LHS & RHS */
			if (!avrasm_check_insarg (ins, argnum, mode, node, tnode_nthsubof (arg, 0), tc, trpass) &&
					!avrasm_check_insarg (ins, argnum, mode, node, tnode_nthsubof (arg, 1), tc, trpass)) {
				good = 1;
			}
		} else if (arg->tag->ndef == avrasm.node_MOPNODE) {
			/* check OP */
			if (!avrasm_check_insarg (ins, argnum, mode, node, tnode_nthsubof (arg, 0), tc, trpass)) {
				good = 1;
			}
		}
		/*}}}*/
	}
	if (mode & IMODE_CONST3) {
		/*{{{  expecting 3-bit constant*/
		if (arg->tag == avrasm.tag_LITINT) {
			good = 1;
		} else if (constprop_isconst (arg)) {
			good = 1;
		} else if (arg->tag->ndef == avrasm.node_DOPNODE) {
			/* check LHS & RHS */
			if (!avrasm_check_insarg (ins, argnum, mode, node, tnode_nthsubof (node, 0), tc, trpass) &&
					!avrasm_check_insarg (ins, argnum, mode, node, tnode_nthsubof (node, 1), tc, trpass)) {
				good = 1;
			}
		} else if (arg->tag->ndef == avrasm.node_MOPNODE) {
			/* check OP */
			if (!avrasm_check_insarg (ins, argnum, mode, node, tnode_nthsubof (arg, 0), tc, trpass)) {
				good = 1;
			}
		}
		/*}}}*/
	}
	if (mode & IMODE_CONSTCODE) {
		/*{{{  expecting label address, or relative position*/
		if ((arg->tag == avrasm.tag_GLABEL) || (arg->tag == avrasm.tag_LLABEL)) {
			if (!trpass) {
				/* first-step typecheck, allow any label */
				good = 1;
			} else {
				name_t *lname = tnode_nthnameof (arg, 0);
				tnode_t *ndecl = NameDeclOf (lname);

				if (ndecl && label_chook && tnode_haschook (ndecl, label_chook)) {
					label_chook_t *lch = (label_chook_t *)tnode_getchook (ndecl, label_chook);

					if (lch->zone->tag == avrasm.tag_TEXTSEG) {
						good = 1;
					} else {
						typecheck_warning (node, tc, "label \"%s\" is not in the code/text segment", NameNameOf (lname));
					}
				}
			}
		} else if (arg->tag == avrasm.tag_LITINT) {
			good = 1;
		} else if (constprop_isconst (arg)) {
			good = 1;
		} else if (arg->tag->ndef == avrasm.node_DOPNODE) {
			/* check LHS & RHS */
			if (!avrasm_check_insarg (ins, argnum, mode, node, tnode_nthsubof (node, 0), tc, trpass) &&
					!avrasm_check_insarg (ins, argnum, mode, node, tnode_nthsubof (node, 1), tc, trpass)) {
				good = 1;
			}
		} else if (arg->tag->ndef == avrasm.node_MOPNODE) {
			/* check OP */
			if (!avrasm_check_insarg (ins, argnum, mode, node, tnode_nthsubof (arg, 0), tc, trpass)) {
				good = 1;
			}
		}
		/*}}}*/
	}
	if (mode & IMODE_CONSTMEM) {
#if 0
fprintf (stderr, "avrasm_check_insarg(IMODE_CONSTMEM): of [%p]:\n", arg);
tnode_dumptree (arg, 1, FHAN_STDERR);
#endif
		/*{{{  expecting constant address, label address or relative position*/
		if ((arg->tag == avrasm.tag_GLABEL) || (arg->tag == avrasm.tag_LLABEL)) {
			if (!trpass) {
				/* first-step typecheck, allow any label */
				good = 1;
			} else {
				name_t *lname = tnode_nthnameof (arg, 0);
				tnode_t *ndecl = NameDeclOf (lname);

				if (ndecl && label_chook && tnode_haschook (ndecl, label_chook)) {
					label_chook_t *lch = (label_chook_t *)tnode_getchook (ndecl, label_chook);

					if (lch->zone->tag == avrasm.tag_DATASEG) {
						good = 1;
					} else {
						typecheck_warning (node, tc, "label \"%s\" is not in the data segment", NameNameOf (lname));
					}
				}
			}
		} else if (arg->tag == avrasm.tag_LITINT) {
			good = 1;
		} else if (constprop_isconst (arg)) {
			good = 1;
		} else if (arg->tag->ndef == avrasm.node_DOPNODE) {
			/* check LHS & RHS */
			if (!avrasm_check_insarg (ins, argnum, mode, node, tnode_nthsubof (arg, 0), tc, trpass) &&
					!avrasm_check_insarg (ins, argnum, mode, node, tnode_nthsubof (arg, 1), tc, trpass)) {
				good = 1;
			}
		} else if (arg->tag->ndef == avrasm.node_MOPNODE) {
			/* check OP */
			if (!avrasm_check_insarg (ins, argnum, mode, node, tnode_nthsubof (arg, 0), tc, trpass)) {
				good = 1;
			}
		}
		/*}}}*/
	}
	if (mode & IMODE_CONSTIO) {
		/*{{{  expecting I/O address constant*/
		if (arg->tag == avrasm.tag_LITINT) {
			good = 1;
		} else if (constprop_isconst (arg)) {
			good = 1;
		}
		/*}}}*/
	}

	if (!good) {
		typecheck_error (node, tc, "incompatible argument %d for \"%s\" instruction", argnum, ins->str);
		return 1;
	}

	return 0;
}
/*}}}*/
/*{{{  static int avrasm_typecheck_insnode (compops_t *cops, tnode_t *node, typecheck_t *tc)*/
/*
 *	typecheck for instruction nodes
 *	returns 0 to stop walk, 1 to continue
 */
static int avrasm_typecheck_insnode (compops_t *cops, tnode_t *node, typecheck_t *tc)
{
	tnode_t *ins_n = tnode_nthsubof (node, 0);
	avrasm_lithook_t *lit;
	int iidx, ins;

	if (ins_n->tag != avrasm.tag_LITINS) {
		nocc_serious ("avrasm_typecheck_insnode(): not an instruction constant [%s]", ins_n->tag->name);
		return 0;
	}
	lit = (avrasm_lithook_t *)tnode_nthhookof (ins_n, 0);
	ins = *(int *)lit->data;

	for (iidx=0; avrasm_itable[iidx].str; iidx++) {
		if (avrasm_itable[iidx].ins == ins) {
			break;			/* for() */
		}
	}
	if (!avrasm_itable[iidx].str) {
		nocc_serious ("avrasm_typecheck_insnode(): impossible instruction %d", ins);
		return 0;
	}

	avrasm_check_insarg (&(avrasm_itable[iidx]), 1, avrasm_itable[iidx].arg0, node, tnode_nthsubof (node, 1), tc, 0);
	avrasm_check_insarg (&(avrasm_itable[iidx]), 2, avrasm_itable[iidx].arg1, node, tnode_nthsubof (node, 2), tc, 0);

	tnode_setnthhook (node, 0, (void *)&(avrasm_itable[iidx]));
	return 1;
}
/*}}}*/
/*{{{  static int avrasm_typeresolve_insnode (compops_t *cops, tnode_t **nodep, typecheck_t *tc)*/
/*
 *	type-resolve for instruction nodes -- does slightly more detailed checking than 'typecheck' alone (and able to change tree)
 *	returns 0 to stop walk, 1 to continue
 */
static int avrasm_typeresolve_insnode (compops_t *cops, tnode_t **nodep, typecheck_t *tc)
{
	tnode_t *node = *nodep;
	tnode_t *ins_n = tnode_nthsubof (node, 0);
	avrasm_lithook_t *lit;
	int iidx, ins;

	if (ins_n->tag != avrasm.tag_LITINS) {
		nocc_serious ("avrasm_typeresolve_insnode(): not an instruction constant [%s]", ins_n->tag->name);
		return 0;
	}
	lit = (avrasm_lithook_t *)tnode_nthhookof (ins_n, 0);
	ins = *(int *)lit->data;

	for (iidx = 0; avrasm_itable[iidx].str; iidx++) {
		if (avrasm_itable[iidx].ins == ins) {
			break;			/* for() */
		}
	}
	if (!avrasm_itable[iidx].str) {
		nocc_serious ("avrasm_typeresolve_insnode(): impossible instruction %d", ins);
		return 0;
	}

	avrasm_check_insarg (&(avrasm_itable[iidx]), 1, avrasm_itable[iidx].arg0, node, tnode_nthsubof (node, 1), tc, 1);
	avrasm_check_insarg (&(avrasm_itable[iidx]), 2, avrasm_itable[iidx].arg1, node, tnode_nthsubof (node, 2), tc, 1);
	return 1;
}
/*}}}*/

/*{{{  static void avrasm_insnode_hook_free (void *hook)*/
/*
 *	frees an instruction-node hook (instruction details)
 */
static void avrasm_insnode_hook_free (void *hook)
{
	avrinstr_tbl_t *inst = (avrinstr_tbl_t *)hook;

	/* nothing! */
	return;
}
/*}}}*/
/*{{{  static void *avrasm_insnode_hook_copy (void *hook)*/
/*
 *	copies an instruction-node hook (instruction details)
 */
static void *avrasm_insnode_hook_copy (void *hook)
{
	avrinstr_tbl_t *inst = (avrinstr_tbl_t *)hook;

	return (void *)hook;
}
/*}}}*/
/*{{{  static void avrasm_insnode_hook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)*/
/*
 *	dump-tree for an instruction-node hook
 */
static void avrasm_insnode_hook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)
{
	avrinstr_tbl_t *inst = (avrinstr_tbl_t *)hook;

	if (inst) {
		avrasm_isetindent (stream, indent);
		fhandle_printf (stream, "<avrinstr id=\"%d\" str=\"%s\" addr=\"%p\" />\n", (int)inst->ins, inst->str, inst);
	}
	return;
}
/*}}}*/

/*{{{  static int avrasm_checkconstdata (tnode_t *tptr, tnode_t *cinstr, typecheck_t *tc);*/
/*
 *	checks a constant (.const/.const16) value for const-ness
 *	returns 0 if okay, non-zero on failure (emits errors)
 */
static int avrasm_checkconstdata (tnode_t *tptr, tnode_t *cinstr, typecheck_t *tc)
{
	if (tptr->tag == avrasm.tag_LITINT) {
		return 0;
	} else if (tptr->tag == avrasm.tag_LITSTR) {
		return 0;
	} else if (tptr->tag == avrasm.tag_GLABEL) {
		return 0;
	} else if (tptr->tag == avrasm.tag_LLABEL) {
		return 0;
	} else if (tptr->tag->ndef == avrasm.node_DOPNODE) {
		/* check LHS & RHS */
		return (avrasm_checkconstdata (tnode_nthsubof (tptr, 0), cinstr, tc) ||
				avrasm_checkconstdata (tnode_nthsubof (tptr, 1), cinstr, tc));
	} else if (tptr->tag->ndef == avrasm.node_MOPNODE) {
		/* check op */
		return avrasm_checkconstdata (tnode_nthsubof (tptr, 0), cinstr, tc);
	}
	typecheck_error (cinstr, tc, "expected constant value, found [%s]", tptr->tag->name);
	return 1;
}
/*}}}*/
/*{{{  static int avrasm_typecheck_constnode (compops_t *cops, tnode_t *node, typecheck_t *tc)*/
/*
 *	type-check for constant-node, checks for constant values (possibly label addresses)
 *	returns 0 to stop walk, 1 to continue
 */
static int avrasm_typecheck_constnode (compops_t *cops, tnode_t *node, typecheck_t *tc)
{
	tnode_t **argp = tnode_nthsubaddr (node, 0);
	tnode_t **items;
	int i, nitems;

	parser_ensurelist (argp, node);

	items = parser_getlistitems (*argp, &nitems);
	for (i=0; i<nitems; i++) {
		avrasm_checkconstdata (items[i], node, tc);
	}

	return 1;
}
/*}}}*/
/*{{{  static int avrasm_typeresolve_constnode (compops_t *cops, tnode_t **nodep, typecheck_t *tc)*/
/*
 *	type-resolve for constant-node, unpacks strings
 *	returns 0 to stop walk, 1 to continue
 */
static int avrasm_typeresolve_constnode (compops_t *cops, tnode_t **nodep, typecheck_t *tc)
{
	int i;
	tnode_t *cdata = tnode_nthsubof (*nodep, 0);

	for (i=0; i<parser_countlist (cdata); i++) {
		tnode_t *item = parser_getfromlist (cdata, i);

		if (item->tag == avrasm.tag_LITSTR) {
			int j;
			avrasm_lithook_t *lit = tnode_nthhookof (item, 0);

			/* empty out string */
			for (j=0; j<lit->len; j++) {
				tnode_t *tmp = constprop_newconst (CONST_INT, tnode_copytree (item), NULL, (int)(lit->data[j]));

				parser_insertinlist (cdata, tmp, i+j);
			}
			i += j;

			/* remove ourselves */
			parser_delfromlist (cdata, i);
			i--;
		}
	}
	return 1;
}
/*}}}*/

/*{{{  static int avrasm_typeresolve_spacenode (compops_t *cops, tnode_t **nodep, typecheck_t *tc)*/
/*
 *	type-resolve for a space-node, checks constant spacing
 *	returns 0 to stop walk, 1 to continue
 */
static int avrasm_typeresolve_spacenode (compops_t *cops, tnode_t **nodep, typecheck_t *tc)
{
	tnode_t *node = *nodep;
	tnode_t *arg = tnode_nthsubof (node, 0);

	if (!constprop_isconst (arg)) {
		typecheck_error (node, tc, "reserved space quantity not a constant (got [%s])", arg->tag->name);
		return 0;
	}
	return 1;
}
/*}}}*/

/*{{{  static int avrasm_constprop_dopnode (compops_t *cops, tnode_t **tptr)*/
/*
 *	does constant propagation for dop-node
 */
static int avrasm_constprop_dopnode (compops_t *cops, tnode_t **tptr)
{
	tnode_t *lhs = tnode_nthsubof (*tptr, 0);
	tnode_t *rhs = tnode_nthsubof (*tptr, 1);

	if (constprop_isconst (lhs) && constprop_isconst (rhs)) {
		/* can probably reduce this! */
		int lhval = constprop_intvalof (lhs);
		int rhval = constprop_intvalof (rhs);

		if ((*tptr)->tag == avrasm.tag_ADD) {
			*tptr = constprop_newconst (CONST_INT, *tptr, NULL, lhval + rhval);
		} else if ((*tptr)->tag == avrasm.tag_SUB) {
			*tptr = constprop_newconst (CONST_INT, *tptr, NULL, lhval - rhval);
		} else if ((*tptr)->tag == avrasm.tag_MUL) {
			*tptr = constprop_newconst (CONST_INT, *tptr, NULL, lhval * rhval);
		} else if ((*tptr)->tag == avrasm.tag_DIV) {
			*tptr = constprop_newconst (CONST_INT, *tptr, NULL, lhval / rhval);
		} else if ((*tptr)->tag == avrasm.tag_REM) {
			*tptr = constprop_newconst (CONST_INT, *tptr, NULL, lhval % rhval);
		} else if ((*tptr)->tag == avrasm.tag_BITAND) {
			*tptr = constprop_newconst (CONST_INT, *tptr, NULL, lhval & rhval);
		} else if ((*tptr)->tag == avrasm.tag_BITOR) {
			*tptr = constprop_newconst (CONST_INT, *tptr, NULL, lhval | rhval);
		} else if ((*tptr)->tag == avrasm.tag_BITXOR) {
			*tptr = constprop_newconst (CONST_INT, *tptr, NULL, lhval ^ rhval);
		} else if ((*tptr)->tag == avrasm.tag_SHL) {
			*tptr = constprop_newconst (CONST_INT, *tptr, NULL, lhval << rhval);
		} else if ((*tptr)->tag == avrasm.tag_SHR) {
			*tptr = constprop_newconst (CONST_INT, *tptr, NULL, lhval >> rhval);
		}
	}
	return 1;
}
/*}}}*/
/*{{{  static int avrasm_constprop_mopnode (compops_t *cops, tnode_t **tptr)*/
/*
 *	does constant propagation for mop-node
 */
static int avrasm_constprop_mopnode (compops_t *cops, tnode_t **tptr)
{
	tnode_t *op = tnode_nthsubof (*tptr, 0);

	if (constprop_isconst (op)) {
		int val = constprop_intvalof (op);

		if ((*tptr)->tag == avrasm.tag_UMINUS) {
			*tptr = constprop_newconst (CONST_INT, *tptr, NULL, -val);
		} else if ((*tptr)->tag == avrasm.tag_BITNOT) {
			*tptr = constprop_newconst (CONST_INT, *tptr, NULL, ~val);
		} else if ((*tptr)->tag == avrasm.tag_HI) {
			*tptr = constprop_newconst (CONST_INT, *tptr, NULL, (val >> 8) & 0xff);
		} else if ((*tptr)->tag == avrasm.tag_LO) {
			*tptr = constprop_newconst (CONST_INT, *tptr, NULL, val & 0xff);
		}
	}
	return 1;
}
/*}}}*/

/*{{{  static int avrasm_scopein_ternode (compops_t *cops, tnode_t **nodep, scope_t *ss)*/
/*
 *	does scope-in for a ternary node -- slightly special handling for undefined labels
 *	returns 0 to stop walk, 1 to continue
 */
static int avrasm_scopein_ternode (compops_t *cops, tnode_t **nodep, scope_t *ss)
{
	tnode_t *cond = tnode_nthsubof (*nodep, 0);

#if 0
fprintf (stderr, "avrasm_scopein_ternode(): here! cond = [%s]\n", cond->tag->name);
#endif
	if (cond->tag == avrasm.tag_NAME) {
		char *rawname;
		name_t *sname = NULL;

		rawname = tnode_nthhookof (cond, 0);

		sname = name_lookupss (rawname, ss);
		if (!sname) {
			/* don't have this, replace with 2nd thing */
			*nodep = tnode_nthsubof (*nodep, 2);
		} else {
			/* do have this, replace with 1st thing */
			*nodep = tnode_nthsubof (*nodep, 1);
			if (!*nodep) {
				/* wasn't set, e.g. "foo ?: bar", becomes condition */
				*nodep = cond;
			}
		}
		tnode_modprepostwalktree (nodep, scope_modprewalktree, scope_modpostwalktree, (void *)ss);
		return 0;
	}
	return 1;
}
/*}}}*/
/*{{{  static int avrasm_constprop_ternode (compops_t *cops, tnode_t **tptr)*/
/*
 *	does constant propagation for a ternary node
 *	returns 0 to stop walk, 1 to continue (meaningless in post-walk)
 */
static int avrasm_constprop_ternode (compops_t *cops, tnode_t **tptr)
{
	tnode_t *op = tnode_nthsubof (*tptr, 0);

	if (constprop_isconst (op)) {
		int val = constprop_intvalof (op);

		/* only need the first one really */
		if (val) {
			*tptr = tnode_nthsubof (*tptr, 1);
		} else {
			*tptr = tnode_nthsubof (*tptr, 2);
		}
	}
	return 1;
}
/*}}}*/

/*{{{  static int avrasm_program_init_nodes (void)*/
/*
 *	initialises nodes for AVR assembler
 *	returns 0 on success, non-zero on failure
 */
static int avrasm_program_init_nodes (void)
{
	tndef_t *tnd;
	int i;
	compops_t *cops;
	langops_t *lops;

	/*{{{  register reduction functions*/
	fcnlib_addfcn ("avrasm_nametoken_to_hook", (void *)avrasm_nametoken_to_hook, 1, 1);
	fcnlib_addfcn ("avrasm_stringtoken_to_node", (void *)avrasm_stringtoken_to_node, 1, 1);
	fcnlib_addfcn ("avrasm_integertoken_to_node", (void *)avrasm_integertoken_to_node, 1, 1);
	fcnlib_addfcn ("avrasm_regtoken_to_node", (void *)avrasm_regtoken_to_node, 1, 1);
	fcnlib_addfcn ("avrasm_instoken_to_node", (void *)avrasm_instoken_to_node, 1, 1);

	fcnlib_addfcn ("avrasm_xyzreduce", (void *)avrasm_xyzreduce, 0, 3);
	fcnlib_addfcn ("avrasm_uslabreduce", (void *)avrasm_uslabreduce, 0, 3);
	fcnlib_addfcn ("avrasm_uslabdefreduce", (void *)avrasm_uslabdefreduce, 0, 3);

	/*}}}*/
	/*{{{  avrasm:rawnamenode -- NAME*/
	i = -1;
	tnd = tnode_newnodetype ("avrasm:rawnamenode", &i, 0, 0, 1, TNF_NONE);			/* hooks: 0 = raw-name */
	tnd->hook_free = avrasm_rawnamenode_hook_free;
	tnd->hook_copy = avrasm_rawnamenode_hook_copy;
	tnd->hook_dumptree = avrasm_rawnamenode_hook_dumptree;
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (avrasm_scopein_rawname));
	tnd->ops = cops;

	i = -1;
	avrasm.tag_NAME = tnode_newnodetag ("AVRASMNAME", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  avrasm:litnode -- LITSTR, LITINT, LITREG, LITINS*/
	i = -1;
	tnd = tnode_newnodetype ("avrasm:litnode", &i, 0, 0, 1, TNF_NONE);			/* hooks: 0 = avrasm_lithook_t */
	tnd->hook_free = avrasm_litnode_hook_free;
	tnd->hook_copy = avrasm_litnode_hook_copy;
	tnd->hook_dumptree = avrasm_litnode_hook_dumptree;
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "constprop", 1, COMPOPTYPE (avrasm_constprop_litnode));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "isconst", 1, LANGOPTYPE (avrasm_isconst_litnode));
	tnode_setlangop (lops, "constvalof", 2, LANGOPTYPE (avrasm_constvalof_litnode));
	tnd->lops = lops;

	i = -1;
	avrasm.tag_LITSTR = tnode_newnodetag ("AVRASMLITSTR", &i, tnd, NTF_NONE);
	i = -1;
	avrasm.tag_LITINT = tnode_newnodetag ("AVRASMLITINT", &i, tnd, NTF_NONE);
	i = -1;
	avrasm.tag_LITREG = tnode_newnodetag ("AVRASMLITREG", &i, tnd, NTF_NONE);
	i = -1;
	avrasm.tag_LITINS = tnode_newnodetag ("AVRASMLITINS", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  avrasm:xyznode -- XYZREG*/
	i = -1;
	tnd = tnode_newnodetype ("avrasm:xyznode", &i, 0, 0, 1, TNF_NONE);			/* hooks: 0 = avrasm_xyzhook_t */
	tnd->hook_free = avrasm_xyznode_hook_free;
	tnd->hook_copy = avrasm_xyznode_hook_copy;
	tnd->hook_dumptree = avrasm_xyznode_hook_dumptree;
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (avrasm_typecheck_xyznode));
	tnd->ops = cops;

	i = -1;
	avrasm.tag_XYZREG = tnode_newnodetag ("AVRASMXYZREG", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  avrasm:leafnode -- TEXTSEG, DATASEG, EEPROMSEG*/
	i = -1;
	tnd = tnode_newnodetype ("avrasm:leafnode", &i, 0, 0, 0, TNF_NONE);
	cops = tnode_newcompops ();
	tnd->ops = cops;

	i = -1;
	avrasm.tag_TEXTSEG = tnode_newnodetag ("AVRASMTEXTSEG", &i, tnd, NTF_NONE);
	i = -1;
	avrasm.tag_DATASEG = tnode_newnodetag ("AVRASMDATASEG", &i, tnd, NTF_NONE);
	i = -1;
	avrasm.tag_EEPROMSEG = tnode_newnodetag ("AVRASMEEPROMSEG", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  avrasm:orgnode -- ORG*/
	i = -1;
	tnd = tnode_newnodetype ("avrasm:dirnode", &i, 1, 0, 0, TNF_NONE);			/* subnodes: 0 = origin-expression */
	cops = tnode_newcompops ();
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "avrasm_inseg", 1, LANGOPTYPE (avrasm_inseg_true));
	tnd->lops = lops;

	i = -1;
	avrasm.tag_ORG = tnode_newnodetag ("AVRASMORG", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  avrasm:targetnode -- TARGETMARK, MCUMARK*/
	i = -1;
	tnd = tnode_newnodetype ("avrasm:targetnode", &i, 1, 0, 0, TNF_NONE);			/* subnodes: 0 = target-string */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "prescope", 2, COMPOPTYPE (avrasm_prescope_targetnode));
	tnd->ops = cops;

	i = -1;
	avrasm.tag_TARGETMARK = tnode_newnodetag ("AVRASMTARGETMARK", &i, tnd, NTF_NONE);
	i = -1;
	avrasm.tag_MCUMARK = tnode_newnodetag ("AVRASMMCUMARK", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  avrasm:segmentnode -- SEGMENTMARK*/
	i = -1;
	tnd = tnode_newnodetype ("avrasm:segmentnode", &i, 2, 0, 0, TNF_NONE);			/* subnodes: 0 = segment-const, 1 = content-list */
	cops = tnode_newcompops ();
	tnd->ops = cops;

	i = -1;
	avrasm.tag_SEGMENTMARK = tnode_newnodetag ("AVRASMSEGMENTMARK", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  avrasm:constnode -- CONST, CONST16*/
	i = -1;
	tnd = tnode_newnodetype ("avrasm:constnode", &i, 1, 0, 0, TNF_NONE);			/* subnodes: 0 = data */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (avrasm_typecheck_constnode));
	tnode_setcompop (cops, "typeresolve", 2, COMPOPTYPE (avrasm_typeresolve_constnode));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "avrasm_inseg", 1, LANGOPTYPE (avrasm_inseg_true));
	tnd->lops = lops;

	i = -1;
	avrasm.tag_CONST = tnode_newnodetag ("AVRASMCONST", &i, tnd, NTF_NONE);
	i = -1;
	avrasm.tag_CONST16 = tnode_newnodetag ("AVRASMCONST16", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  avrasm:spacenode -- SPACE, SPACE16*/
	i = -1;
	tnd = tnode_newnodetype ("avrasm:spacenode", &i, 1, 0, 0, TNF_NONE);			/* subnodes: 1 = byte-size-expr */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "typeresolve", 2, COMPOPTYPE (avrasm_typeresolve_spacenode));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "avrasm_inseg", 1, LANGOPTYPE (avrasm_inseg_true));
	tnd->lops = lops;

	i = -1;
	avrasm.tag_SPACE = tnode_newnodetag ("AVRASMSPACE", &i, tnd, NTF_NONE);
	i = -1;
	avrasm.tag_SPACE16 = tnode_newnodetag ("AVRASMSPACE16", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  avrasm:macrodef -- MACRODEF*/
	i = -1;
	tnd = tnode_newnodetype ("avrasm:macrodef", &i, 3, 0, 0, TNF_NONE);			/* subnodes: 0 = name, 1 = params, 2 = body */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "prescope", 2, COMPOPTYPE (avrasm_prescope_macrodef));
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (avrasm_scopein_macrodef));
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (avrasm_typecheck_macrodef));
	tnd->ops = cops;

	i = -1;
	avrasm.tag_MACRODEF = tnode_newnodetag ("AVRASMMACRODEF", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  avrasm:equnode -- EQU, DEF*/
	i = -1;
	tnd = tnode_newnodetype ("avrasm:equnode", &i, 2, 0, 0, TNF_NONE);			/* subnodes: 0 = name, 1 = expr */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (avrasm_scopein_equnode));
	tnode_setcompop (cops, "subequ", 2, COMPOPTYPE (avrasm_subequ_equnode));
	tnd->ops = cops;

	i = -1;
	avrasm.tag_EQU = tnode_newnodetag ("AVRASMEQU", &i, tnd, NTF_NONE);
	i = -1;
	avrasm.tag_DEF = tnode_newnodetag ("AVRASMDEF", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  avrasm:fparamnode -- FPARAM*/
	i = -1;
	tnd = tnode_newnodetype ("avrasm:fparamnode", &i, 1, 0, 0, TNF_NONE);			/* subnodes: 0 = parameter-name */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (avrasm_scopein_fparamnode));
	tnd->ops = cops;

	i = -1;
	avrasm.tag_FPARAM = tnode_newnodetag ("AVRASMFPARAM", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  avrasm:instancenode -- INSTANCE*/
	i = -1;
	tnd = tnode_newnodetype ("avrasm:instancenode", &i, 2, 0, 0, TNF_NONE);			/* subnodes: 0 = name, 1 = params */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "prescope", 2, COMPOPTYPE (avrasm_prescope_instancenode));
	tnode_setcompop (cops, "submacro", 2, COMPOPTYPE (avrasm_submacro_instancenode));
	tnd->ops = cops;

	i = -1;
	avrasm.tag_INSTANCE = tnode_newnodetag ("AVRASMINSTANCE", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  avrasm:labdefnode -- GLABELDEF, LLABELDEF*/
	i = -1;
	tnd = tnode_newnodetype ("avrasm:labdefnode", &i, 1, 0, 0, TNF_NONE);			/* subnodes: 0 = label-name */
	cops = tnode_newcompops ();
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "avrasm_inseg", 1, LANGOPTYPE (avrasm_inseg_true));
	tnd->lops = lops;

	i = -1;
	avrasm.tag_GLABELDEF = tnode_newnodetag ("AVRASMGLABELDEF", &i, tnd, NTF_NONE);
	i = -1;
	avrasm.tag_LLABELDEF = tnode_newnodetag ("AVRASMLLABELDEF", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  avrasm:uslabnode -- USLAB*/
	i = -1;
	tnd = tnode_newnodetype ("avrasm:uslabnode", &i, 0, 0, 1, TNF_NONE);			/* hooks: 0 = avrasm_uslabhook_t */
	tnd->hook_free = avrasm_uslabnode_hook_free;
	tnd->hook_copy = avrasm_uslabnode_hook_copy;
	tnd->hook_dumptree = avrasm_uslabnode_hook_dumptree;
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "llscope", 2, COMPOPTYPE (avrasm_llscope_uslabnode));
	tnd->ops = cops;

	i = -1;
	avrasm.tag_USLAB = tnode_newnodetag ("AVRASMUSLAB", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  avrasm:namenode -- GLABEL, LLABEL, EQUNAME, MACRONAME, PARAMNAME*/
	i = -1;
	tnd = tnode_newnodetype ("avrasm:namenode", &i, 0, 1, 0, TNF_NONE);			/* namenodes: 0 = name */
	avrasm.node_NAMENODE = tnd;
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "subequ", 2, COMPOPTYPE (avrasm_subequ_namenode));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "getname", 2, LANGOPTYPE (avrasm_getname_namenode));
	tnd->lops = lops;

	i = -1;
	avrasm.tag_GLABEL = tnode_newnodetag ("AVRASMGLABEL", &i, tnd, NTF_NONE);
	i = -1;
	avrasm.tag_LLABEL = tnode_newnodetag ("AVRASMLLABEL", &i, tnd, NTF_NONE);
	i = -1;
	avrasm.tag_EQUNAME = tnode_newnodetag ("AVRASMEQUNAME", &i, tnd, NTF_NONE);
	i = -1;
	avrasm.tag_MACRONAME = tnode_newnodetag ("AVRASMMACRONAME", &i, tnd, NTF_NONE);
	i = -1;
	avrasm.tag_PARAMNAME = tnode_newnodetag ("AVRASMPARAMNAME", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  avrasm:insnode -- INSTR*/
	i = -1;
	tnd = tnode_newnodetype ("avrasm:insnode", &i, 3, 0, 1, TNF_NONE);			/* subnodes: 0 = const-instr, 1 = arg0, 2 = arg1; hooks: 0 = avrinstr_tbl_t* */
	tnd->hook_free = avrasm_insnode_hook_free;
	tnd->hook_copy = avrasm_insnode_hook_copy;
	tnd->hook_dumptree = avrasm_insnode_hook_dumptree;
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (avrasm_typecheck_insnode));
	tnode_setcompop (cops, "typeresolve", 2, COMPOPTYPE (avrasm_typeresolve_insnode));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "avrasm_inseg", 1, LANGOPTYPE (avrasm_inseg_true));
	tnd->lops = lops;

	i = -1;
	avrasm.tag_INSTR = tnode_newnodetag ("AVRASMINSTR", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  avrasm:mopnode -- UMINUS, BITNOT, HI, LO*/
	i = -1;
	tnd = tnode_newnodetype ("avrasm:mopnode", &i, 1, 0, 0, TNF_NONE);			/* subnodes: 0 = operand */
	avrasm.node_MOPNODE = tnd;
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "constprop", 1, COMPOPTYPE (avrasm_constprop_mopnode));
	tnd->ops = cops;

	i = -1;
	avrasm.tag_UMINUS = tnode_newnodetag ("AVRASMUMINUS", &i, tnd, NTF_NONE);
	i = -1;
	avrasm.tag_BITNOT = tnode_newnodetag ("AVRASMBITNOT", &i, tnd, NTF_NONE);
	i = -1;
	avrasm.tag_HI = tnode_newnodetag ("AVRASMHI", &i, tnd, NTF_NONE);
	i = -1;
	avrasm.tag_LO = tnode_newnodetag ("AVRASMLO", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  avrasm:dopnode -- ADD, SUB, MUL, DIV, REM, BITADD, BITOR, BITXOR, SHL, SHR*/
	i = -1;
	tnd = tnode_newnodetype ("avrasm:dopnode", &i, 2, 0, 0, TNF_NONE);			/* subnodes: 0 = left, 1 = right */
	avrasm.node_DOPNODE = tnd;
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "constprop", 1, COMPOPTYPE (avrasm_constprop_dopnode));
	tnd->ops = cops;

	i = -1;
	avrasm.tag_ADD = tnode_newnodetag ("AVRASMADD", &i, tnd, NTF_NONE);
	i = -1;
	avrasm.tag_SUB = tnode_newnodetag ("AVRASMSUB", &i, tnd, NTF_NONE);
	i = -1;
	avrasm.tag_MUL = tnode_newnodetag ("AVRASMMUL", &i, tnd, NTF_NONE);
	i = -1;
	avrasm.tag_DIV = tnode_newnodetag ("AVRASMDIV", &i, tnd, NTF_NONE);
	i = -1;
	avrasm.tag_REM = tnode_newnodetag ("AVRASMREM", &i, tnd, NTF_NONE);
	i = -1;
	avrasm.tag_BITAND = tnode_newnodetag ("AVRASMBITAND", &i, tnd, NTF_NONE);
	i = -1;
	avrasm.tag_BITOR = tnode_newnodetag ("AVRASMBITOR", &i, tnd, NTF_NONE);
	i = -1;
	avrasm.tag_BITXOR = tnode_newnodetag ("AVRASMBITXOR", &i, tnd, NTF_NONE);
	i = -1;
	avrasm.tag_SHL = tnode_newnodetag ("AVRASMSHL", &i, tnd, NTF_NONE);
	i = -1;
	avrasm.tag_SHR = tnode_newnodetag ("AVRASMSHR", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  avrasm:ternode -- COND*/
	i = -1;
	tnd = tnode_newnodetype ("avrasm:ternode", &i, 3, 0, 0, TNF_NONE);			/* subnodes: 0..2 = operands */
	avrasm.node_TERNODE = tnd;
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "constprop", 1, COMPOPTYPE (avrasm_constprop_ternode));
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (avrasm_scopein_ternode));
	tnd->ops = cops;

	i = -1;
	avrasm.tag_COND = tnode_newnodetag ("AVRASMCOND", &i, tnd, NTF_NONE);

	/*}}}*/

	return 0;
}
/*}}}*/
/*{{{  static int avrasm_program_post_setup (void)*/
/*
 *	does any final setup for the AVR assembler
 *	returns 0 on success, non-zero on failure
 */
static int avrasm_program_post_setup (void)
{
	int i;

	stringhash_sinit (avrasm_nitable);
	for (i=0; avrasm_itable[i].str; i++) {
		stringhash_insert (avrasm_nitable, &(avrasm_itable[i]), avrasm_itable[i].str);
	}

	label_chook = tnode_lookupornewchook ("avrasm:labelinfo");

	return 0;
}
/*}}}*/

/*{{{  avrasm_program_feunit (feunit_t)*/
feunit_t avrasm_program_feunit = {
	.init_nodes = avrasm_program_init_nodes,
	.reg_reducers = NULL,
	.init_dfatrans = NULL,
	.post_setup = avrasm_program_post_setup,
	.ident = "avrasm-program"
};

/*}}}*/

