/*
 *	codegen.h -- interface for NOCC code-generation
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

#ifndef __CODEGEN_H
#define	__CODEGEN_H

struct TAG_target;
struct TAG_tnode;
struct TAG_name;
struct TAG_lexfile;
struct TAG_coderops;
struct TAG_chook;

typedef enum ENUM_codegen_parammode {
	PARAM_INVALID = 0,
	PARAM_REF = 1,
	PARAM_VAL = 2
} codegen_parammode_t;

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
} codegen_t;

typedef struct TAG_coderops {
	void (*loadpointer)(codegen_t *, struct TAG_tnode *, int);
	void (*loadname)(codegen_t *, struct TAG_tnode *, int);
	void (*loadparam)(codegen_t *, struct TAG_tnode *, codegen_parammode_t);
	void (*loadlocalpointer)(codegen_t *, int);
	void (*loadlexlevel)(codegen_t *, int);
	void (*storepointer)(codegen_t *, struct TAG_tnode *, int);
	void (*storename)(codegen_t *, struct TAG_tnode *, int);
	void (*storelocal)(codegen_t *, int);
	void (*loadconst)(codegen_t *, int);
	void (*wsadjust)(codegen_t *, int);
	void (*comment)(codegen_t *, const char *fmt, ...);
	void (*setwssize)(codegen_t *, int, int);
	void (*setvssize)(codegen_t *, int);
	void (*setmssize)(codegen_t *, int);
	void (*setnamedlabel)(codegen_t *, const char *);
	void (*setlabel)(codegen_t *, int);
	void (*callnamedlabel)(codegen_t *, const char *, int);
	void (*calllabel)(codegen_t *, int, int);
	void (*procreturn)(codegen_t *, int);
	void (*tsecondary)(codegen_t *, int);
	void (*loadlabaddr)(codegen_t *, int);
	void (*branch)(codegen_t *, int, int);
} coderops_t;


extern int codegen_generate_code (struct TAG_tnode **tptr, struct TAG_lexfile *lf, struct TAG_target *target);
extern int codegen_subcodegen (struct TAG_tnode *tree, codegen_t *cgen);
extern int codegen_subprecode (struct TAG_tnode **tptr, codegen_t *cgen);

extern void codegen_setinithook (struct TAG_tnode *node, void (*init)(struct TAG_tnode *, codegen_t *, void *), void *arg);

extern void codegen_warning (codegen_t *cgen, const char *fmt, ...);
extern void codegen_error (codegen_t *cgen, const char *fmt, ...);
extern void codegen_fatal (codegen_t *cgen, const char *fmt, ...);

extern int codegen_write_bytes (codegen_t *cgen, const char *ptr, int bytes);
extern int codegen_write_string (codegen_t *cgen, const char *str);
extern int codegen_write_fmt (codegen_t *cgen, const char *fmt, ...);

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

