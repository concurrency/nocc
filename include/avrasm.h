/*
 *	avrasm.h -- AVR assembler
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

#ifndef __AVRASM_H
#define __AVRASM_H

struct TAG_langlexer;
struct TAG_langparser;

extern struct TAG_langlexer avrasm_lexer;
extern struct TAG_langparser avrasm_parser;

struct TAG_tnode;
struct TAG_tndef;
struct TAG_ntdef;
struct TAG_token;
struct TAG_langdef;

typedef struct {
	char *str;
} avrasm_lspecial_t;


typedef struct {
	struct TAG_tndef *node_INSNODE;
	struct TAG_tndef *node_LABELNODE;
	struct TAG_tndef *node_TERNODE;
	struct TAG_tndef *node_DOPNODE;
	struct TAG_tndef *node_MOPNODE;

	struct TAG_token *tok_STRING;
	struct TAG_token *tok_DOT;
	struct TAG_token *tok_PLUS;
	struct TAG_token *tok_MINUS;
	struct TAG_token *tok_REGX;
	struct TAG_token *tok_REGY;
	struct TAG_token *tok_REGZ;

	struct TAG_ntdef *tag_NAME;

	struct TAG_ntdef *tag_LITSTR;
	struct TAG_ntdef *tag_LITINT;
	struct TAG_ntdef *tag_LITREG;
	struct TAG_ntdef *tag_LITINS;

	struct TAG_ntdef *tag_XYZREG;

	struct TAG_ntdef *tag_SEGMENTMARK;
	struct TAG_ntdef *tag_TARGETMARK;
	struct TAG_ntdef *tag_MCUMARK;

	struct TAG_ntdef *tag_MACRODEF;

	struct TAG_ntdef *tag_CONST;
	struct TAG_ntdef *tag_CONST16;

	struct TAG_ntdef *tag_SPACE;
	struct TAG_ntdef *tag_SPACE16;

	struct TAG_ntdef *tag_TEXTSEG;
	struct TAG_ntdef *tag_DATASEG;
	struct TAG_ntdef *tag_EEPROMSEG;

	struct TAG_ntdef *tag_ORG;
	struct TAG_ntdef *tag_EQU;
	struct TAG_ntdef *tag_DEF;

	struct TAG_ntdef *tag_ADD;
	struct TAG_ntdef *tag_SUB;
	struct TAG_ntdef *tag_MUL;
	struct TAG_ntdef *tag_DIV;
	struct TAG_ntdef *tag_REM;
	struct TAG_ntdef *tag_BITAND;
	struct TAG_ntdef *tag_BITOR;
	struct TAG_ntdef *tag_BITXOR;
	struct TAG_ntdef *tag_SHL;
	struct TAG_ntdef *tag_SHR;

	struct TAG_ntdef *tag_UMINUS;
	struct TAG_ntdef *tag_BITNOT;
	struct TAG_ntdef *tag_HI;
	struct TAG_ntdef *tag_LO;

	struct TAG_ntdef *tag_COND;

	struct TAG_ntdef *tag_GLABELDEF;
	struct TAG_ntdef *tag_LLABELDEF;

	struct TAG_ntdef *tag_USLAB;

	struct TAG_ntdef *tag_GLABEL;
	struct TAG_ntdef *tag_LLABEL;
	struct TAG_ntdef *tag_EQUNAME;
	struct TAG_ntdef *tag_MACRONAME;
	struct TAG_ntdef *tag_PARAMNAME;

	/* these are for macros */
	struct TAG_ntdef *tag_FPARAM;
	struct TAG_ntdef *tag_INSTANCE;

	struct TAG_ntdef *tag_INSTR;

	/* higher-level stuff */
	struct TAG_ntdef *tag_REGPAIR;

	struct TAG_ntdef *tag_FCNDEF;
	struct TAG_ntdef *tag_LETDEF;

	struct TAG_ntdef *tag_FCNPARAM;

	struct TAG_ntdef *tag_FCNNAME;
	struct TAG_ntdef *tag_FCNPARAMNAME;
	struct TAG_ntdef *tag_LETNAME;

	struct TAG_ntdef *tag_DSTORE;
	struct TAG_ntdef *tag_DLOAD;

	struct TAG_ntdef *tag_INT8;
	struct TAG_ntdef *tag_UINT8;
	struct TAG_ntdef *tag_INT16;
	struct TAG_ntdef *tag_UINT16;
	struct TAG_ntdef *tag_SIGNED;
	struct TAG_ntdef *tag_UNSIGNED;
} avrasm_pset_t;

extern avrasm_pset_t avrasm;

typedef struct TAG_subequ {
	int errcount;
} subequ_t;

typedef struct TAG_submacro {
	int errcount;
} submacro_t;

typedef struct TAG_hlltypecheck {
	int errcount;
} hlltypecheck_t;

typedef struct TAG_hllsimplify {
	int errcount;
	struct TAG_tnode *list_cxt;
	int list_itm;
} hllsimplify_t;

/* used to tag labels (compiler hook) */
typedef struct {
	struct TAG_tnode *zone;	/* one of the segment leaves */
	int addr;		/* actual address (byte offset) */
} label_chook_t;

extern void avrasm_isetindent (FILE *stream, int indent);	/* avrasm_parser.c */
extern struct TAG_langdef *avrasm_getlangdef (void);

extern int avrasm_langop_inseg (struct TAG_tnode *node);

extern int avrasm_subequ_subtree (struct TAG_tnode **tptr, struct TAG_subequ *se);
extern int avrasm_submacro_subtree (struct TAG_tnode **tptr, struct TAG_submacro *sm);
extern int avrasm_hlltypecheck_subtree (struct TAG_tnode **tptr, struct TAG_hlltypecheck *hltc);

extern label_chook_t *avrasm_newlabelchook (void);
extern void avrasm_freelabelchook (label_chook_t *lch);

extern struct TAG_tnode *avrasm_llscope_fixref (struct TAG_tnode **tptr, int labid, int labdir, void *llsptr);

/* in avrasm_program.c, where specific structures reside */
extern int avrasm_getlitintval (struct TAG_tnode *node);
extern int avrasm_getxyzreginfo (struct TAG_tnode *node, int *reg, int *prepost, int *offs);

extern struct TAG_feunit avrasm_program_feunit;			/* avrasm_program.c */
extern struct TAG_feunit avrasm_hll_feunit;			/* avrasm_hll.c */


#endif	/* !__AVRASM_H */

