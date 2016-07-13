/*
 *	oil.h -- oil language interface for nocc
 *	Copyright (C) 2010-2016 Fred Barnes <frmb@kent.ac.uk>
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

#ifndef __OIL_H
#define __OIL_H

struct TAG_langlexer;
struct TAG_langparser;
struct TAG_langdef;

extern struct TAG_langlexer oil_lexer;
extern struct TAG_langparser oil_parser;

/* node-type and node-tag flag values */
#define TNF_LONGACTION			0x0010		/* long action (e.g. case input, list of things into subnode 1) */

#define NTF_BOOLOP			0x0010		/* boolean operator flag */
#define NTF_SYNCTYPE			0x0020		/* synchronisation type */
#define NTF_INDENTED_PROC_LIST		0x0040		/* for TNF_LONGPROCs, parse a list of indented processes into subnode 1 */
#define NTF_INDENTED_PROC		0x0080		/* for TNF_LONGPROCs, parse an indented process into subnode 1 */
							/* for TNF_LONGDECLs, parse an indented process into subnode 2 */
#define NTF_INDENTED_NAME_LIST		0x0100		/* for TNF_LONGDECLs, parse an indented list of names into subnode 1 */
#define NTF_INDENTED_TCASE_LIST		0x0200		/* for TNF_LONGACTIONSs, parse an indented list of type-case declarations into subnode 1 */
#define NTF_INDENTED_DECL_LIST		0x0400		/* for TNF_LONGDECLs, parse an indented list of declarations into subnode 1 */
#define NTF_INDENTED_DGUARD_LIST	0x0800		/* for TNF_LONGPROCs, parse a list of indented declarations and guards into subnode 1 */
#define NTF_INDENTED_EXPR_LIST		0x1000		/* for TNF_LONGPROCs, parse a list of indented expressions and processes into subnode 1 */

/* implementation-specific language-tag bits */
#define LANGTAG_STYPE			0x00010000	/* sized type (e.g. int8) */


struct TAG_tndef;
struct TAG_ntdef;
struct TAG_token;
struct TAG_chook;
struct TAG_fhandle;
struct TAG_fetrans;
struct TAG_betrans;

typedef struct {
	struct TAG_tndef *node_NAMENODE;
	struct TAG_tndef *node_NAMETYPENODE;
	struct TAG_tndef *node_NAMEPROTOCOLNODE;
	struct TAG_tndef *node_LEAFNODE;
	struct TAG_tndef *node_TYPENODE;

	struct TAG_ntdef *tag_PPCOMMENT;
	struct TAG_ntdef *tag_MAPINIT;

	struct TAG_ntdef *tag_BOOL;
	struct TAG_ntdef *tag_BYTE;
	struct TAG_ntdef *tag_INT;			/* caters for all integer types (sizes and signedness) */
	struct TAG_ntdef *tag_REAL;			/* caters for all real types (sizes) */
	struct TAG_ntdef *tag_CHAR;
	struct TAG_ntdef *tag_STRING;

	struct TAG_ntdef *tag_CHAN;
	struct TAG_ntdef *tag_ANY;
	struct TAG_ntdef *tag_NAME;

	struct TAG_ntdef *tag_ARRAY;
	struct TAG_ntdef *tag_FCNTYPE;
	struct TAG_ntdef *tag_VALTYPE;

	struct TAG_ntdef *tag_LITINT;
	struct TAG_ntdef *tag_LITREAL;
	struct TAG_ntdef *tag_LITCHAR;
	struct TAG_ntdef *tag_LITSTRING;
	struct TAG_ntdef *tag_LITBOOL;

	struct TAG_ntdef *tag_CONSTSTRINGINIT;

	struct TAG_ntdef *tag_FCNDEF;
	struct TAG_ntdef *tag_PFCNDEF;
	struct TAG_ntdef *tag_VARDECL;
	struct TAG_ntdef *tag_FIELDDECL;
	struct TAG_ntdef *tag_FPARAM;
	struct TAG_ntdef *tag_DECLBLOCK;
	struct TAG_ntdef *tag_ENUMDEF;
	struct TAG_ntdef *tag_TYPEDEF;
	struct TAG_ntdef *tag_FPARAMINIT;
	struct TAG_ntdef *tag_VARINIT;
	struct TAG_ntdef *tag_VARFREE;
	struct TAG_ntdef *tag_STRINIT;
	struct TAG_ntdef *tag_STRFREE;
	struct TAG_ntdef *tag_CHANINIT;
	struct TAG_ntdef *tag_ARRAYINIT;
	struct TAG_ntdef *tag_ARRAYFREE;

	struct TAG_ntdef *tag_EXTDECL;
	struct TAG_ntdef *tag_LIBDECL;

	struct TAG_ntdef *tag_SEQ;
	struct TAG_ntdef *tag_PAR;

	struct TAG_ntdef *tag_ALT;
	struct TAG_ntdef *tag_PRIALT;
	struct TAG_ntdef *tag_PRIALTSKIP;

	struct TAG_ntdef *tag_REPLSEQ;
	struct TAG_ntdef *tag_REPLPAR;
	struct TAG_ntdef *tag_REPLALT;

	struct TAG_ntdef *tag_GUARD;

	struct TAG_ntdef *tag_FVNODE;

	struct TAG_ntdef *tag_INSTANCE;
	struct TAG_ntdef *tag_APICALL;
	struct TAG_ntdef *tag_APICALLR;
	struct TAG_ntdef *tag_PPINSTANCE;

	struct TAG_ntdef *tag_NDECL;
	struct TAG_ntdef *tag_NVALDECL;
	struct TAG_ntdef *tag_NABBR;
	struct TAG_ntdef *tag_NVALABBR;
	struct TAG_ntdef *tag_NRESABBR;
	struct TAG_ntdef *tag_NPARAM;
	struct TAG_ntdef *tag_NVALPARAM;
	struct TAG_ntdef *tag_NRESPARAM;
	struct TAG_ntdef *tag_NINITPARAM;
	struct TAG_ntdef *tag_NREPL;
	struct TAG_ntdef *tag_NENUM;
	struct TAG_ntdef *tag_NENUMVAL;

	struct TAG_ntdef *tag_NTYPEDECL;
	struct TAG_ntdef *tag_NFIELD;
	struct TAG_ntdef *tag_NFCNDEF;			/* regular function/procedure */
	struct TAG_ntdef *tag_NPFCNDEF;			/* process-abstracted function/procedure */

	struct TAG_ntdef *tag_SKIP;
	struct TAG_ntdef *tag_STOP;
	struct TAG_ntdef *tag_SHUTDOWN;			/* for the top-level process */

	struct TAG_ntdef *tag_IF;
	struct TAG_ntdef *tag_SHORTIF;
	struct TAG_ntdef *tag_WHILE;
	struct TAG_ntdef *tag_CASE;
	struct TAG_ntdef *tag_RETURN;

	struct TAG_ntdef *tag_OPTION;
	struct TAG_ntdef *tag_COND;
	struct TAG_ntdef *tag_ELSE;

	struct TAG_ntdef *tag_ASSIGN;
	struct TAG_ntdef *tag_SASSIGN;
	struct TAG_ntdef *tag_IS;

	struct TAG_ntdef *tag_INPUT;
	struct TAG_ntdef *tag_OUTPUT;
	struct TAG_ntdef *tag_CASEINPUT;

	struct TAG_ntdef *tag_ADD;
	struct TAG_ntdef *tag_SUB;
	struct TAG_ntdef *tag_MUL;
	struct TAG_ntdef *tag_DIV;
	struct TAG_ntdef *tag_REM;
	struct TAG_ntdef *tag_ASHR;
	struct TAG_ntdef *tag_SHR;
	struct TAG_ntdef *tag_SHL;
	struct TAG_ntdef *tag_BITXOR;
	struct TAG_ntdef *tag_BITAND;
	struct TAG_ntdef *tag_BITOR;

	struct TAG_ntdef *tag_PLUS;
	struct TAG_ntdef *tag_MINUS;
	struct TAG_ntdef *tag_TIMES;

	struct TAG_ntdef *tag_XOR;
	struct TAG_ntdef *tag_AND;
	struct TAG_ntdef *tag_OR;

	struct TAG_ntdef *tag_CONDITIONAL;

	struct TAG_ntdef *tag_ADDIN;
	struct TAG_ntdef *tag_SUBIN;
	struct TAG_ntdef *tag_MULIN;
	struct TAG_ntdef *tag_DIVIN;
	struct TAG_ntdef *tag_REMIN;
	struct TAG_ntdef *tag_ASHRIN;
	struct TAG_ntdef *tag_SHRIN;
	struct TAG_ntdef *tag_SHLIN;
	struct TAG_ntdef *tag_BITXORIN;
	struct TAG_ntdef *tag_BITANDIN;
	struct TAG_ntdef *tag_BITORIN;

	struct TAG_ntdef *tag_PLUSIN;
	struct TAG_ntdef *tag_MINUSIN;
	struct TAG_ntdef *tag_TIMESIN;

	struct TAG_ntdef *tag_XORIN;
	struct TAG_ntdef *tag_ANDIN;
	struct TAG_ntdef *tag_ORIN;

	struct TAG_ntdef *tag_LT;
	struct TAG_ntdef *tag_LE;
	struct TAG_ntdef *tag_GT;
	struct TAG_ntdef *tag_GE;
	struct TAG_ntdef *tag_EQ;
	struct TAG_ntdef *tag_NE;

	struct TAG_ntdef *tag_NOT;
	struct TAG_ntdef *tag_BITNOT;
	struct TAG_ntdef *tag_NEG;

	struct TAG_ntdef *tag_MARKEDIN;
	struct TAG_ntdef *tag_MARKEDOUT;

	struct TAG_ntdef *tag_SIZE;
	struct TAG_ntdef *tag_BYTESIN;

	struct TAG_ntdef *tag_ARRAYSUB;
	struct TAG_ntdef *tag_RECORDSUB;

	struct TAG_ntdef *tag_STRASSIGN;
	struct TAG_ntdef *tag_STRCONCAT;

	struct TAG_ntdef *tag_TIMER;
	struct TAG_ntdef *tag_TIMERREAD;
	struct TAG_ntdef *tag_TIMERWAIT;
	struct TAG_ntdef *tag_AFTER;

	struct TAG_token *tok_ATSIGN;
	struct TAG_token *tok_STRING;
	struct TAG_token *tok_PUBLIC;
	struct TAG_token *tok_SEQUENCE;
	struct TAG_token *tok_INTERLEAVE;
} oil_pset_t;

extern oil_pset_t oil;

typedef struct {
	struct TAG_tnode *last_type;			/* used when handling things like "int a, b, c" */
	int procdepth;					/* procedure/function nesting depth for public/non-public names */
} oil_prescope_t;

typedef struct {
	DYNARRAY (struct TAG_tnode *, crosses);		/* where these things can be remembered (collects names) */
	DYNARRAY (int64_t, cross_lexlevels);		/* lexical levels for each of the above */
	struct TAG_ntdef *resolve_nametype_first;	/* when resolving names, look for these [nodetype] first */
} oil_scope_t;

typedef struct {
	int errcount;					/* number of errors accumulated */
} oil_declify_t;

typedef struct {
	int errcount;					/* number of errors accumulated */
} oil_autoseq_t;

typedef struct {
	struct TAG_tnode *encfcn;			/* enclosing function (checking returns) */
	struct TAG_tnode *encfcnrtype;			/* enclosing function return type */
} oil_typecheck_t;

typedef struct {
	int procdepth;					/* procedure/function nesting depth */
	struct TAG_tnode **insertpoint;			/* where nested procedures get unwound to */
} oil_betrans_t;

typedef struct {
	void *data;
	int bytes;
	int littype;					/* INTEGER, REAL, STRING */
} oil_litdata_t;

typedef struct {
	struct TAG_tnode *preinslist;			/* things to insert before the current node */
	struct TAG_tnode *postinslist;			/* things to insert after the current node */
} oil_fetrans_t;

typedef struct {
	DYNARRAY (struct TAG_tnode *, rnames);		/* names of result parameters in function */
	struct TAG_tnode **inspoint;			/* insert-point for new code (typically current statement) */
	struct TAG_tnode *decllist;			/* where new declarations can go, NULL if need fresh */
	int error;					/* error count */
} oil_fetrans1_t;

typedef struct {
	int expt_proc;					/* expecting a process (vs. expression) */
	int error;					/* error count */
} oil_fetrans15_t;

typedef struct {
	int error;					/* error count */
} oil_fetrans2_t;

typedef struct {
	int error;					/* error count */
} oil_fetrans3_t;

typedef struct TAG_oil_fcndefhook {
	int lexlevel;					/* 0 = outermost */
	int ispublic;					/* explicitly marked as public? */
	int istoplevel;					/* last in top-level compilation unit? */
	int ispar;					/* explicitly need par-stub? */
	struct TAG_tnode *pfcndef;			/* if there is a PFCNDEF for this FCNDEF */
} oil_fcndefhook_t;

typedef struct {
	struct TAG_tnode *decllist;			/* where new declarations can go during mapping, used inside PAR mapping */
} oil_map_t;

extern void oil_isetindent (struct TAG_fhandle *stream, int indent);
extern struct TAG_langdef *oil_getlangdef (void);

extern int oil_autoseq_listtoseqlist (struct TAG_tnode **, oil_autoseq_t *);
extern int oil_declify_listtodecllist (struct TAG_tnode **, oil_declify_t *);
extern int oil_declify_listtodecllist_single (struct TAG_tnode **, oil_declify_t *);

extern oil_fetrans1_t *oil_newfetrans1 (void);
extern void oil_freefetrans1 (oil_fetrans1_t *);
extern oil_fetrans15_t *oil_newfetrans15 (void);
extern void oil_freefetrans15 (oil_fetrans15_t *);
extern oil_fetrans2_t *oil_newfetrans2 (void);
extern void oil_freefetrans2 (oil_fetrans2_t *);
extern oil_fetrans3_t *oil_newfetrans3 (void);
extern void oil_freefetrans3 (oil_fetrans3_t *);

extern int oil_autoseq_subtree (struct TAG_tnode **, oil_autoseq_t *);
extern int oil_declify_subtree (struct TAG_tnode **, oil_declify_t *);
extern int oil_flattenseq_subtree (struct TAG_tnode **);
extern int oil_postscope_subtree (struct TAG_tnode **);
extern int oil_fetrans1_subtree (struct TAG_tnode **, oil_fetrans1_t *);
extern int oil_fetrans1_subtree_newtemps (struct TAG_tnode **, oil_fetrans1_t *);
extern int oil_fetrans15_subtree (struct TAG_tnode **, oil_fetrans15_t *);
extern int oil_fetrans2_subtree (struct TAG_tnode **, oil_fetrans2_t *);
extern int oil_fetrans3_subtree (struct TAG_tnode **, oil_fetrans3_t *);

extern struct TAG_tnode *oil_fetrans1_maketemp (struct TAG_ntdef *tag, struct TAG_tnode *org, struct TAG_tnode *type, struct TAG_tnode *init, oil_fetrans1_t *fe1);
extern struct TAG_tnode *oil_newprimtype (struct TAG_ntdef *tag, struct TAG_tnode *org, const int size);
extern struct TAG_tnode *oil_newchantype (struct TAG_ntdef *tag, struct TAG_tnode *org, struct TAG_tnode *protocol);

extern struct TAG_tnode *oil_makeintlit (struct TAG_tnode *type, struct TAG_tnode *org, const int value);
extern struct TAG_tnode *oil_makereallit (struct TAG_tnode *type, struct TAG_tnode *org, const double value);
extern struct TAG_tnode *oil_makestringlit (struct TAG_tnode *type, struct TAG_tnode *org, const char *value);

extern char *oil_maketempname (struct TAG_tnode *org);
extern struct TAG_tnode *oil_copytree (struct TAG_tnode *tree);

extern oil_fcndefhook_t *oil_newfcndefhook (void);
extern void oil_freefcndefhook (oil_fcndefhook_t *fdh);

extern int oil_chantype_setinout (struct TAG_tnode *chantype, int marked_in, int marked_out);
extern int oil_chantype_getinout (struct TAG_tnode *chantype, int *marked_in, int *marked_out);

/* front-end units */
extern struct TAG_feunit oil_assign_feunit;		/* oil_assign.c */
extern struct TAG_feunit oil_cflow_feunit;		/* oil_cflow.c */
extern struct TAG_feunit oil_cnode_feunit;		/* oil_cnode.c */
extern struct TAG_feunit oil_decls_feunit;		/* oil_decls.c */
extern struct TAG_feunit oil_fcndef_feunit;		/* oil_fcndef.c */
extern struct TAG_feunit oil_io_feunit;			/* oil_io.c */
extern struct TAG_feunit oil_lit_feunit;		/* oil_lit.c */
extern struct TAG_feunit oil_misc_feunit;		/* oil_misc.c */
extern struct TAG_feunit oil_oper_feunit;		/* oil_oper.c */
extern struct TAG_feunit oil_primproc_feunit;		/* oil_primproc.c */
extern struct TAG_feunit oil_types_feunit;		/* oil_types.c */
extern struct TAG_feunit oil_instance_feunit;		/* oil_instance.c */
extern struct TAG_feunit oil_timer_feunit;		/* oil_timer.c */

/* these are for language units to use in reductions */
extern void *oil_nametoken_to_hook (void *ntok);
extern void *oil_stringtoken_to_namehook (void *ntok);

/* option handlers inside front-end */
struct TAG_cmd_option;
extern int oil_lexer_opthandler_flag (struct TAG_cmd_option *opt, char ***argwalk, int *argleft);

/* oil_udo.c */

extern int oil_udo_init (void);
extern int oil_udo_shutdown (void);

extern char *oil_udo_maketempfcnname (struct TAG_tnode *node);
extern char *oil_udo_newfunction (const char *str, struct TAG_tnode *res, struct TAG_tnode *parm);


#endif	/* !__OIL_H */
