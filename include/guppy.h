/*
 *	guppy.h -- Guppy language interface for nocc
 *	Copyright (C) 2010-2013 Fred Barnes <frmb@kent.ac.uk>
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

#ifndef __GUPPY_H
#define __GUPPY_H

struct TAG_langlexer;
struct TAG_langparser;
struct TAG_langdef;

extern struct TAG_langlexer guppy_lexer;
extern struct TAG_langparser guppy_parser;

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

	struct TAG_ntdef *tag_EXTDECL;

	struct TAG_ntdef *tag_SEQ;
	struct TAG_ntdef *tag_PAR;

	struct TAG_ntdef *tag_REPLSEQ;
	struct TAG_ntdef *tag_REPLPAR;

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

	struct TAG_ntdef *tag_IF;
	struct TAG_ntdef *tag_WHILE;
	struct TAG_ntdef *tag_RETURN;

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
	struct TAG_ntdef *tag_BITXOR;
	struct TAG_ntdef *tag_BITAND;
	struct TAG_ntdef *tag_BITOR;

	struct TAG_ntdef *tag_XOR;
	struct TAG_ntdef *tag_AND;
	struct TAG_ntdef *tag_OR;

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

	struct TAG_token *tok_ATSIGN;
	struct TAG_token *tok_STRING;
	struct TAG_token *tok_PUBLIC;

} guppy_pset_t;

extern guppy_pset_t gup;

typedef struct {
	struct TAG_tnode *last_type;			/* used when handling things like "int a, b, c" */
	int procdepth;					/* procedure/function nesting depth for public/non-public names */
} guppy_prescope_t;

typedef struct {
	DYNARRAY (struct TAG_tnode *, crosses);		/* where these things can be remembered (collects names) */
	struct TAG_ntdef *resolve_nametype_first;	/* when resolving names, look for these [nodetype] first */
} guppy_scope_t;

typedef struct {
	int errcount;					/* number of errors accumulated */
} guppy_declify_t;

typedef struct {
	int errcount;					/* number of errors accumulated */
} guppy_autoseq_t;

typedef struct {
	struct TAG_tnode *encfcn;			/* enclosing function (checking returns) */
	struct TAG_tnode *encfcnrtype;			/* enclosing function return type */
} guppy_typecheck_t;

typedef struct {
	int procdepth;					/* procedure/function nesting depth */
	struct TAG_tnode **insertpoint;			/* where nested procedures get unwound to */
} guppy_betrans_t;

typedef struct {
	void *data;
	int bytes;
	int littype;					/* INTEGER, REAL, STRING */
} guppy_litdata_t;

typedef struct {
	struct TAG_tnode *inslist;			/* list where inserted definitions can go */
	int insidx;					/* whereabouts in the list */
	int changed;					/* set if changes are made */
} guppy_fetrans_t;

typedef struct {
	DYNARRAY (struct TAG_tnode *, rnames);		/* names of result parameters in function */
	struct TAG_tnode **inspoint;			/* insert-point for new code (typically current statement) */
	struct TAG_tnode *decllist;			/* where new declarations can go, NULL if need fresh */
	int error;					/* error count */
} guppy_fetrans1_t;

typedef struct {
	int error;					/* error count */
} guppy_fetrans2_t;

typedef struct {
	int error;					/* error count */
} guppy_fetrans3_t;

typedef struct TAG_guppy_fcndefhook {
	int lexlevel;					/* 0 = outermost */
	int ispublic;					/* explicitly marked as public? */
	int istoplevel;					/* last in top-level compilation unit? */
	int ispar;					/* explicitly need par-stub? */
	struct TAG_tnode *pfcndef;			/* if there is a PFCNDEF for this FCNDEF */
} guppy_fcndefhook_t;

typedef struct {
	struct TAG_tnode *decllist;			/* where new declarations can go during mapping, used inside PAR mapping */
} guppy_map_t;

extern void guppy_isetindent (struct TAG_fhandle *stream, int indent);
extern struct TAG_langdef *guppy_getlangdef (void);

extern int guppy_autoseq_listtoseqlist (struct TAG_tnode **, guppy_autoseq_t *);
extern int guppy_declify_listtodecllist (struct TAG_tnode **, guppy_declify_t *);
extern int guppy_declify_listtodecllist_single (struct TAG_tnode **, guppy_declify_t *);

extern guppy_fetrans1_t *guppy_newfetrans1 (void);
extern void guppy_freefetrans1 (guppy_fetrans1_t *);
extern guppy_fetrans2_t *guppy_newfetrans2 (void);
extern void guppy_freefetrans2 (guppy_fetrans2_t *);
extern guppy_fetrans3_t *guppy_newfetrans3 (void);
extern void guppy_freefetrans3 (guppy_fetrans3_t *);

extern int guppy_autoseq_subtree (struct TAG_tnode **, guppy_autoseq_t *);
extern int guppy_declify_subtree (struct TAG_tnode **, guppy_declify_t *);
extern int guppy_flattenseq_subtree (struct TAG_tnode **);
extern int guppy_postscope_subtree (struct TAG_tnode **);
extern int guppy_fetrans1_subtree (struct TAG_tnode **, guppy_fetrans1_t *);
extern int guppy_fetrans2_subtree (struct TAG_tnode **, guppy_fetrans2_t *);
extern int guppy_fetrans3_subtree (struct TAG_tnode **, guppy_fetrans3_t *);

extern struct TAG_tnode *guppy_fetrans1_maketemp (struct TAG_ntdef *tag, struct TAG_tnode *org, struct TAG_tnode *type, struct TAG_tnode *init, guppy_fetrans1_t *fe1);
extern struct TAG_tnode *guppy_newprimtype (struct TAG_ntdef *tag, struct TAG_tnode *org, const int size);

extern struct TAG_tnode *guppy_makeintlit (struct TAG_tnode *type, struct TAG_tnode *org, const int value);
extern struct TAG_tnode *guppy_makereallit (struct TAG_tnode *type, struct TAG_tnode *org, const double value);
extern struct TAG_tnode *guppy_makestringlit (struct TAG_tnode *type, struct TAG_tnode *org, const char *value);

extern char *guppy_maketempname (struct TAG_tnode *org);

extern guppy_fcndefhook_t *guppy_newfcndefhook (void);
extern void guppy_freefcndefhook (guppy_fcndefhook_t *fdh);

extern int guppy_chantype_setinout (struct TAG_tnode *chantype, int marked_in, int marked_out);

/* front-end units */
extern struct TAG_feunit guppy_assign_feunit;		/* guppy_assign.c */
extern struct TAG_feunit guppy_cflow_feunit;		/* guppy_cflow.c */
extern struct TAG_feunit guppy_cnode_feunit;		/* guppy_cnode.c */
extern struct TAG_feunit guppy_decls_feunit;		/* guppy_decls.c */
extern struct TAG_feunit guppy_fcndef_feunit;		/* guppy_fcndef.c */
extern struct TAG_feunit guppy_io_feunit;		/* guppy_io.c */
extern struct TAG_feunit guppy_lit_feunit;		/* guppy_lit.c */
extern struct TAG_feunit guppy_misc_feunit;		/* guppy_misc.c */
extern struct TAG_feunit guppy_oper_feunit;		/* guppy_oper.c */
extern struct TAG_feunit guppy_primproc_feunit;		/* guppy_primproc.c */
extern struct TAG_feunit guppy_types_feunit;		/* guppy_types.c */
extern struct TAG_feunit guppy_instance_feunit;		/* guppy_instance.c */

/* these are for language units to use in reductions */
extern void *guppy_nametoken_to_hook (void *ntok);
extern void *guppy_stringtoken_to_namehook (void *ntok);

/* option handlers inside front-end */
struct TAG_cmd_option;
extern int guppy_lexer_opthandler_flag (struct TAG_cmd_option *opt, char ***argwalk, int *argleft);


#endif	/* !__GUPPY_H */
