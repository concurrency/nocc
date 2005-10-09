/*
 *	occampi.h -- occam-pi language interface for nocc
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

#ifndef __OCCAMPI_H
#define __OCCAMPI_H

struct TAG_langlexer;
struct TAG_langparser;

extern struct TAG_langlexer occampi_lexer;
extern struct TAG_langparser occampi_parser;

/* node-type and node-tag flag values */
#define NTF_BOOLOP	0x0001		/* boolean operator flag */

struct TAG_tndef;
struct TAG_ntdef;
struct TAG_token;
struct TAG_chook;


typedef struct {
	struct TAG_tndef *node_NAMENODE;
	struct TAG_tndef *node_LEAFNODE;
	struct TAG_tndef *node_TYPENODE;

	struct TAG_ntdef *tag_BOOL;
	struct TAG_ntdef *tag_BYTE;
	struct TAG_ntdef *tag_INT;
	struct TAG_ntdef *tag_INT16;
	struct TAG_ntdef *tag_INT32;
	struct TAG_ntdef *tag_INT64;
	struct TAG_ntdef *tag_REAL32;
	struct TAG_ntdef *tag_REAL64;
	struct TAG_ntdef *tag_CHAR;
	struct TAG_ntdef *tag_CHAN;
	struct TAG_ntdef *tag_NAME;
	struct TAG_ntdef *tag_ASINPUT;
	struct TAG_ntdef *tag_ASOUTPUT;
	struct TAG_ntdef *tag_SUBSCRIPT;
	struct TAG_ntdef *tag_RECORDSUB;
	struct TAG_ntdef *tag_ARRAYSUB;
	struct TAG_ntdef *tag_ARRAY;
	struct TAG_ntdef *tag_MOBILE;
	struct TAG_ntdef *tag_FUNCTIONTYPE;

	struct TAG_ntdef *tag_VARDECL;
	struct TAG_ntdef *tag_ABBREV;
	struct TAG_ntdef *tag_VALABBREV;
	struct TAG_ntdef *tag_PROCDECL;
	struct TAG_ntdef *tag_SHORTFUNCDECL;
	struct TAG_ntdef *tag_FUNCDECL;
	struct TAG_ntdef *tag_TYPEDECL;
	struct TAG_ntdef *tag_FIELDDECL;

	struct TAG_ntdef *tag_FPARAM;
	struct TAG_ntdef *tag_VALFPARAM;

	struct TAG_ntdef *tag_SKIP;
	struct TAG_ntdef *tag_STOP;
	struct TAG_ntdef *tag_SEQ;
	struct TAG_ntdef *tag_PAR;
	struct TAG_ntdef *tag_ASSIGN;
	struct TAG_ntdef *tag_INPUT;
	struct TAG_ntdef *tag_OUTPUT;
	struct TAG_ntdef *tag_HIDDENPARAM;
	struct TAG_ntdef *tag_RETURNADDRESS;
	struct TAG_ntdef *tag_PARSPACE;
	struct TAG_ntdef *tag_IF;
	struct TAG_ntdef *tag_SHORTIF;
	struct TAG_ntdef *tag_ALT;
	struct TAG_ntdef *tag_CASE;
	struct TAG_ntdef *tag_CONDITIONAL;
	struct TAG_ntdef *tag_VALOF;

	struct TAG_ntdef *tag_LITBYTE;
	struct TAG_ntdef *tag_LITINT;
	struct TAG_ntdef *tag_LITREAL;
	struct TAG_ntdef *tag_LITARRAY;

	struct TAG_ntdef *tag_MUL;
	struct TAG_ntdef *tag_DIV;
	struct TAG_ntdef *tag_REM;
	struct TAG_ntdef *tag_ADD;
	struct TAG_ntdef *tag_SUB;
	struct TAG_ntdef *tag_PLUS;
	struct TAG_ntdef *tag_MINUS;
	struct TAG_ntdef *tag_TIMES;
	struct TAG_ntdef *tag_UMINUS;
	struct TAG_ntdef *tag_BITNOT;
	struct TAG_ntdef *tag_RELEQ;
	struct TAG_ntdef *tag_RELNEQ;
	struct TAG_ntdef *tag_RELLT;
	struct TAG_ntdef *tag_RELLEQ;
	struct TAG_ntdef *tag_RELGT;
	struct TAG_ntdef *tag_RELGEQ;

	struct TAG_ntdef *tag_NDECL;
	struct TAG_ntdef *tag_NPARAM;
	struct TAG_ntdef *tag_NVALPARAM;
	struct TAG_ntdef *tag_NPROCDEF;
	struct TAG_ntdef *tag_NFUNCDEF;
	struct TAG_ntdef *tag_NTYPEDECL;
	struct TAG_ntdef *tag_NFIELD;
	struct TAG_ntdef *tag_NABBR;
	struct TAG_ntdef *tag_NVALABBR;

	struct TAG_ntdef *tag_PINSTANCE;
	struct TAG_ntdef *tag_FINSTANCE;
	struct TAG_ntdef *tag_BUILTINPROC;


	struct TAG_token *tok_COLON;
	struct TAG_token *tok_INPUT;
	struct TAG_token *tok_OUTPUT;
	struct TAG_token *tok_HASH;
	struct TAG_token *tok_STRING;
	struct TAG_token *tok_PUBLIC;

	struct TAG_chook *chook_typeattr;
} occampi_pset_t;

extern occampi_pset_t opi;

typedef enum ENUM_occampi_typeattr {
	TYPEATTR_NONE = 0x00,
	TYPEATTR_MARKED_IN = 0x01,
	TYPEATTR_MARKED_OUT = 0x02
} occampi_typeattr_t;

struct TAG_tnode;
struct TAG_prescope;
struct TAG_scope;
struct TAG_feunit;


typedef struct {
	struct TAG_tnode *last_type;
} occampi_prescope_t;

typedef struct {
	void *data;
	int bytes;
} occampi_litdata_t;

extern void occampi_isetindent (FILE *stream, int indent);


extern struct TAG_feunit occampi_primproc_feunit;	/* occampi_primproc.c */
extern struct TAG_feunit occampi_cnode_feunit;		/* occampi_cnode.c */
extern struct TAG_feunit occampi_snode_feunit;		/* occampi_snode.c */
extern struct TAG_feunit occampi_decl_feunit;		/* occampi_decl.c */
extern struct TAG_feunit occampi_action_feunit;		/* occampi_action.c */
extern struct TAG_feunit occampi_lit_feunit;		/* occampi_lit.c */
extern struct TAG_feunit occampi_type_feunit;		/* occampi_type.c */
extern struct TAG_feunit occampi_instance_feunit;	/* occampi_instance.c */
extern struct TAG_feunit occampi_dtype_feunit;		/* occampi_dtype.c */
extern struct TAG_feunit occampi_oper_feunit;		/* occampi_oper.c */
extern struct TAG_feunit occampi_function_feunit;	/* occampi_function.c */
extern struct TAG_feunit occampi_mobiles_feunit;	/* occampi_mobiles.c */
extern struct TAG_feunit occampi_initial_feunit;	/* occampi_initial.c */

/* these are for language units to use in reductions */
extern void *occampi_nametoken_to_hook (void *ntok);
extern void *occampi_integertoken_to_hook (void *itok);
extern void *occampi_realtoken_to_hook (void *itok);

/* option handlers inside occam-pi front-end */
struct TAG_cmd_option;
extern int occampi_lexer_opthandler_flag (struct TAG_cmd_option *opt, char ***argwalk, int *argleft);


#endif	/* !__OCCAMPI_H */

