/*
 *	cccsp.h -- top-level interface to the CCSP C backend.
 *	Copyright (C) 2008-2015 Fred Barnes <frmb@kent.ac.uk>
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

#ifndef __CCCSP_H
#define __CCCSP_H

extern int cccsp_init (void);
extern int cccsp_shutdown (void);

struct TAG_tnode;
struct TAG_map;
struct TAG_target;
struct TAG_srclocn;
struct TAG_name;
struct TAG_fhandle;
struct TAG_chook;

extern struct TAG_chook *cccsp_sfi_entrychook;
extern struct TAG_chook *cccsp_parinfochook;

struct TAG_cccsp_sfi_entry;
struct TAG_cccsp_parinfo_entry;

typedef enum ENUM_cccsp_subtarget {
	CCCSP_SUBTARGET_DEFAULT = 0,
	CCCSP_SUBTARGET_EV3 = 1
} cccsp_subtarget_e;

typedef enum ENUM_cccsp_apicall {
	NOAPI = 0,
	CHAN_IN = 1,
	CHAN_OUT = 2,
	STOP_PROC = 3,
	PROC_PAR = 4,
	LIGHT_PROC_INIT = 5,
	PROC_PARAM = 6,
	GET_PROC_PARAM = 7,
	MEM_ALLOC = 8,
	MEM_RELEASE = 9,
	MEM_RELEASE_CHK = 10,
	STR_INIT = 11,
	STR_FREE = 12,
	STR_ASSIGN = 13,
	STR_CONCAT = 14,
	STR_CLEAR = 15,
	CHAN_INIT = 16,
	TIMER_READ = 17,
	TIMER_WAIT = 18,
	SHUTDOWN = 19,
	ALT_START = 20,
	ALT_END = 21,
	ALT_ENBC = 22,
	ALT_DISC = 23,
	ALT_WAIT = 24,
	PROC_ALT = 25,
	LIGHT_PROC_FREE = 26,
	ARRAY_INIT = 27,
	ARRAY_INIT_ALLOC = 28,
	ARRAY_FREE = 29
} cccsp_apicall_e;

#define CCCSP_APICALL_LAST ARRAY_FREE

typedef struct TAG_cccsp_apicall {
	cccsp_apicall_e call;
	char *name;
	int stkwords;					/* number of C stack words this requires (excluding parameters) */
} cccsp_apicall_t;

typedef struct TAG_cccsp_mapdata {
	int target_indir;				/* language-specific use (Guppy) */
	struct TAG_tnode *process_id;			/* current process identifier node (wptr) */
	void *langhook;					/* language-specific hook (Guppy) */

	struct TAG_cccsp_parinfo_entry *thisentry;	/* when mapping parallel things, relevant entry */
} cccsp_mapdata_t;

typedef struct TAG_cccsp_preallocate {
	struct TAG_target *target;
	int lexlevel;

	int collect;
} cccsp_preallocate_t;

typedef struct TAG_cccsp_dcg {
	struct TAG_target *target;
	struct TAG_cccsp_sfi_entry *thisfcn;
} cccsp_dcg_t;

typedef struct TAG_cccsp_reallocate {
	struct TAG_target *target;
	int lexlevel;
	int error;

	int maxpar;					/* maximum space used by parallel processes (in words) */
} cccsp_reallocate_t;

typedef struct TAG_cccsp_parinfo_entry {
	struct TAG_tnode *namenode;			/* namenode associated with the parallel process instance */
	struct TAG_tnode *wsspace;			/* workspace reservation for this particular instance */
} cccsp_parinfo_entry_t;

typedef struct TAG_cccsp_parinfo {
	DYNARRAY (cccsp_parinfo_entry_t *, entries);
	int nwords;
} cccsp_parinfo_t;

extern void cccsp_isetindent (struct TAG_fhandle *stream, int indent);

extern int cccsp_set_initialiser (struct TAG_tnode *bename, struct TAG_tnode *init);
extern struct TAG_tnode *cccsp_create_apicallname (cccsp_apicall_e);
extern int cccsp_stkwords_apicallnode (struct TAG_tnode *call);
extern char *cccsp_make_entryname (const char *name, const int procabs);
extern char *cccsp_make_apicallname (struct TAG_tnode *call);

extern struct TAG_tnode *cccsp_create_addrof (struct TAG_tnode *arg, struct TAG_target *target);
extern int cccsp_set_indir (struct TAG_tnode *benode, int indir, struct TAG_target *target);
extern int cccsp_get_indir (struct TAG_tnode *benode, struct TAG_target *target);

extern int cccsp_set_toplevelname (struct TAG_name *tlname, struct TAG_target *target);


extern struct TAG_tnode *cccsp_create_wptr (struct TAG_srclocn *org, struct TAG_target *target);
extern struct TAG_tnode *cccsp_create_workspace (struct TAG_srclocn *org, struct TAG_target *target);
extern int cccsp_set_workspace_nparams (struct TAG_tnode *wsnode, int nparams);
extern int cccsp_set_workspace_nwords (struct TAG_tnode *wsnode, int nwords);
extern struct TAG_tnode *cccsp_create_workspace_nwordsof (struct TAG_tnode *wsnode, struct TAG_target *target);
extern struct TAG_tnode *cccsp_create_utype (struct TAG_srclocn *org, struct TAG_target *target, const char *name, struct TAG_tnode *fields);
extern struct TAG_tnode *cccsp_create_etype (struct TAG_srclocn *org, struct TAG_target *target, const char *name, struct TAG_tnode *fields);
extern struct TAG_tnode *cccsp_create_ename (struct TAG_tnode *fename, struct TAG_map *mdata);
extern struct TAG_tnode *cccsp_create_arraysub (struct TAG_srclocn *org, struct TAG_target *target, struct TAG_tnode *base, struct TAG_tnode *index, int indir, struct TAG_tnode *type);
extern struct TAG_tnode *cccsp_create_recordsub (struct TAG_srclocn *org, struct TAG_target *target, struct TAG_tnode *base, struct TAG_tnode *field, int indir, struct TAG_tnode *type);
extern struct TAG_tnode *cccsp_create_null (struct TAG_srclocn *org, struct TAG_target *target);
extern struct TAG_tnode *cccsp_create_notprocess (struct TAG_srclocn *org, struct TAG_target *target);


extern int cccsp_preallocate_subtree (struct TAG_tnode *tptr, cccsp_preallocate_t *cpa);
extern int cccsp_precode_subtree (struct TAG_tnode **nodep, struct TAG_codegen *cgen);
extern int cccsp_cccspdcg_subtree (struct TAG_tnode *node, cccsp_dcg_t *dcg);
extern int cccsp_cccspdcgfix_subtree (struct TAG_tnode *node);
extern int cccsp_getblockspace (struct TAG_tnode *beblk, int *mysize, int *nestsize);
extern int cccsp_setblockspace (struct TAG_tnode *beblk, int *mysize, int *nestsize);
extern int cccsp_addtofixups (struct TAG_tnode *beblk, struct TAG_tnode *node);
extern int cccsp_reallocate_subtree (struct TAG_tnode *tptr, cccsp_reallocate_t *cra);

extern cccsp_parinfo_entry_t *cccsp_newparinfoentry (void);
extern void cccsp_freeparinfoentry (cccsp_parinfo_entry_t *pent);
extern cccsp_parinfo_t *cccsp_newparinfo (void);
extern void cccsp_freeparinfo (cccsp_parinfo_t *pset);
extern int cccsp_linkparinfo (cccsp_parinfo_t *pset, cccsp_parinfo_entry_t *pent);

extern cccsp_subtarget_e cccsp_get_subtarget (void);

extern struct TAG_cccsp_sfi_entry *cccsp_sfiofname (struct TAG_name *name, int pinst);

/* related to cccsp_sfi.c */

typedef struct TAG_cccsp_sfi_entry {
	char *name;				/* name of this one */
	DYNARRAY (struct TAG_cccsp_sfi_entry *, children);	/* children of this one */
	int framesize;				/* framesize as extracted from gcc */
	int allocsize;				/* allocation size (framesize + max(children)) */

	int parfixup;				/* used when reallocating */
} cccsp_sfi_entry_t;

#define SFIENTRIES_BITSIZE	(5)

typedef struct TAG_cccsp_sfi {
	STRINGHASH (cccsp_sfi_entry_t *, entries, SFIENTRIES_BITSIZE);
} cccsp_sfi_t;

extern int cccsp_sfi_init (void);
extern int cccsp_sfi_shutdown (void);

extern cccsp_sfi_entry_t *cccsp_sfi_lookupornew (char *name);
extern cccsp_sfi_entry_t *cccsp_sfi_copyof (cccsp_sfi_entry_t *ent);
extern void cccsp_sfi_addchild (cccsp_sfi_entry_t *parent, cccsp_sfi_entry_t *child);
extern int cccsp_sfi_loadcalls (const char *fname);
extern int cccsp_sfi_loadusage (const char *fname);
extern int cccsp_sfi_calc_alloc (void);
extern void cccsp_sfi_dumptable (struct TAG_fhandle *stream);


#endif	/* !__CCCSP_H */
