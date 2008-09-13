/*
 *	codegen.h -- interface for NOCC code-generation
 *	Copyright (C) 2005-2007 Fred Barnes <frmb@kent.ac.uk>
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

#ifndef __CODEGEN_H
#define	__CODEGEN_H

struct TAG_target;
struct TAG_tnode;
struct TAG_name;
struct TAG_lexfile;
struct TAG_coderops;
struct TAG_chook;
struct TAG_crypto;

typedef enum ENUM_codegen_parammode {
	PARAM_INVALID = 0,
	PARAM_REF = 1,
	PARAM_VAL = 2
} codegen_parammode_e;

struct TAG_codegen;

typedef struct TAG_codegen_pcall {
	void (*fcn)(struct TAG_codegen *, void *);
	void *arg;
} codegen_pcall_t;


typedef struct TAG_codegen {
	struct TAG_target *target;		/* target */
	int error;				/* error-count */
	char *fname;				/* file-name we're generating into */
	int fd;					/* descriptor we're generating into */
	struct TAG_coderops *cops;		/* specific code-generation routines */
	int labcount;				/* ever-increasing label counter */
	struct TAG_tnode **cinsertpoint;	/* coder insert-point (for constants, etc.) */
	DYNARRAY (struct TAG_tnode *, be_blks);	/* enclosing back-end blocks, stack of */
	DYNARRAY (void *, tcgstates);		/* target code-generation states, stack of */
	struct TAG_chook *pc_chook;		/* pre-code code-generation hook */
	struct TAG_crypto *digest;		/* where we store the code-gen digest (optional) */
	DYNARRAY (codegen_pcall_t *, pcalls);	/* post-codegen calls */
} codegen_t;

typedef struct TAG_codegeninithook {
	struct TAG_codegeninithook *next;
	void (*init)(struct TAG_tnode *, codegen_t *, void *);
	void *arg;
} codegeninithook_t;

typedef struct TAG_codegenfinalhook {
	struct TAG_codegenfinalhook *next;
	void (*final)(struct TAG_tnode *, codegen_t *, void *);
	void *arg;
} codegenfinalhook_t;

typedef struct TAG_coderops {
	void (*loadpointer)(codegen_t *, struct TAG_tnode *, int);
	void (*loadnthpointer)(codegen_t *, struct TAG_tnode *, int, int);
	void (*loadatpointer)(codegen_t *, struct TAG_tnode *, int);
	void (*loadname)(codegen_t *, struct TAG_tnode *, int);
	void (*loadparam)(codegen_t *, struct TAG_tnode *, codegen_parammode_e);
	void (*loadlocalpointer)(codegen_t *, int);
	void (*loadlexlevel)(codegen_t *, int);
	void (*loadvsp)(codegen_t *, int);
	void (*loadmsp)(codegen_t *, int);
	void (*loadlocal)(codegen_t *, int);
	void (*loadnonlocal)(codegen_t *, int);
	void (*storenonlocal)(codegen_t *, int);
	void (*storepointer)(codegen_t *, struct TAG_tnode *, int);
	void (*storenthpointer)(codegen_t *, struct TAG_tnode *, int, int);
	void (*storeatpointer)(codegen_t *, struct TAG_tnode *, int);
	void (*storename)(codegen_t *, struct TAG_tnode *, int);
	void (*storelocal)(codegen_t *, int);
	void (*loadconst)(codegen_t *, int);
	void (*addconst)(codegen_t *, int);
	void (*wsadjust)(codegen_t *, int);
	void (*comment)(codegen_t *, const char *fmt, ...) __attribute__ ((format (printf, 2, 3)));
	void (*setwssize)(codegen_t *, int, int);
	void (*setvssize)(codegen_t *, int);
	void (*setmssize)(codegen_t *, int);
	void (*setnamedlabel)(codegen_t *, const char *);
	void (*setnamelabel)(codegen_t *, struct TAG_name *);
	void (*setlabel)(codegen_t *, int);
	void (*callnamedlabel)(codegen_t *, const char *, int);
	void (*callnamelabel)(codegen_t *, struct TAG_name *, int);
	void (*calllabel)(codegen_t *, int, int);
	void (*procentry)(codegen_t *, const char *);
	void (*procnameentry)(codegen_t *, struct TAG_name *);
	void (*procreturn)(codegen_t *, int);
	void (*funcreturn)(codegen_t *, int);
	void (*funcresults)(codegen_t *, int);
	void (*tsecondary)(codegen_t *, int);
	void (*loadlabaddr)(codegen_t *, int);
	void (*constlabaddr)(codegen_t *, int);
	void (*constlabdiff)(codegen_t *, int, int);
	void (*branch)(codegen_t *, int, int);
	void (*debugline)(codegen_t *, struct TAG_tnode *);
	void (*trashistack)(codegen_t *);
	void (*tcoff)(codegen_t *, int, const char *, const int);
} coderops_t;


extern int precode_addtoprecodevars (struct TAG_tnode *tptr, struct TAG_tnode *pcvar);
extern int precode_pullupprecodevars (struct TAG_tnode *dest_tptr, struct TAG_tnode *src_tptr);

extern int codegen_generate_code (struct TAG_tnode **tptr, struct TAG_lexfile *lf, struct TAG_target *target);
extern int codegen_subcodegen (struct TAG_tnode *tree, codegen_t *cgen);
extern int codegen_subprecode (struct TAG_tnode **tptr, codegen_t *cgen);

extern struct TAG_chook *codegen_getcodegeninithook (void);
extern struct TAG_chook *codegen_getcodegenfinalhook (void);

extern void codegen_setinithook (struct TAG_tnode *node, void (*init)(struct TAG_tnode *, codegen_t *, void *), void *arg);
extern void codegen_setfinalhook (struct TAG_tnode *node, void (*final)(struct TAG_tnode *, codegen_t *, void *), void *arg);
extern void codegen_setpostcall (codegen_t *cgen, void (*func)(codegen_t *, void *), void *arg);
extern void codegen_clearpostcall (codegen_t *cgen, void (*func)(codegen_t *, void *), void *arg);

extern void codegen_warning (codegen_t *cgen, const char *fmt, ...) __attribute__ ((format (printf, 2, 3)));
extern void codegen_error (codegen_t *cgen, const char *fmt, ...) __attribute__ ((format (printf, 2, 3)));
extern void codegen_fatal (codegen_t *cgen, const char *fmt, ...) __attribute__ ((format (printf, 2, 3)));

extern int codegen_write_bytes (codegen_t *cgen, const char *ptr, int bytes);
extern int codegen_write_string (codegen_t *cgen, const char *str);
extern int codegen_write_fmt (codegen_t *cgen, const char *fmt, ...) __attribute__ ((format (printf, 2, 3)));

extern int codegen_new_label (codegen_t *cgen);

extern void codegen_nocoder (codegen_t *cgen, const char *op);

extern int codegen_check_beblock (struct TAG_tnode *node, codegen_t *cgen, int err);
extern int codegen_check_beblockref (struct TAG_tnode *node, codegen_t *cgen, int err);
extern int codegen_check_bename (struct TAG_tnode *node, codegen_t *cgen, int err);
extern int codegen_check_benameref (struct TAG_tnode *node, codegen_t *cgen, int err);
extern int codegen_check_beconst (struct TAG_tnode *node, codegen_t *cgen, int err);
extern int codegen_check_beindexed (struct TAG_tnode *node, codegen_t *cgen, int err);

extern void codegen_precode_seenproc (codegen_t *cgen, struct TAG_name *name, struct TAG_tnode *node);

extern int codegen_init (void);
extern int codegen_shutdown (void);


#define codegen_callops(CG,OP,ARGS...)	((!(CG)->cops || !(CG)->cops->OP) ? codegen_nocoder ((CG), #OP) : (CG)->cops->OP (CG, ## ARGS))

#endif	/* !__CODEGEN_H */

