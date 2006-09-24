/*
 *	mwsync.h -- multi-way synchronisation definitions (new style for ETC)
 *	Copyright (C) 2006 Fred Barnes <frmb@kent.ac.uk>
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

#ifndef __MWSYNC_H
#define __MWSYNC_H


struct TAG_feunit;
struct TAG_cmd_option;
struct TAG_tnode;
struct TAG_name;
struct TAG_ntdef;
struct TAG_tndef;
struct TAG_langparser;
struct TAG_map;


typedef struct TAG_mwsyncpstk {
	DYNARRAY (struct TAG_tnode *, parblks);		/* PAR nodes themselves */
	DYNARRAY (struct TAG_tnode **, paripoints);	/* barrier name insert point for par (parbarrier) */
	DYNARRAY (struct TAG_tnode *, parbarriers);	/* associated PAR barrier variables */
	DYNARRAY (struct TAG_tnode *, bnames);		/* barrier name variables */
	DYNARRAY (struct TAG_tnode **, bipoints);	/* barrier name insert point (procbarrier) */
} mwsyncpstk_t;

typedef struct TAG_mwsynctrans {
	DYNARRAY (struct TAG_tnode *, varptr);		/* barrier var-decl */
	DYNARRAY (struct TAG_tnode *, bnames);		/* barrier name variables (in declarations) */
	DYNARRAY (mwsyncpstk_t *, pstack);		/* PAR stack */
	int inparams;					/* set if we're currently walking over formal params (or other PROCBARRIER references) */
	int error;
} mwsynctrans_t;


/* this one gets attached to a PARBARRIER node */
typedef struct TAG_mwsyncpbinfo {
	int ecount;					/* enroll count */
	struct TAG_tnode *ecount_expr;			/* enroll count (if expression) */
	int sadjust;					/* sync adjust */
	struct TAG_tnode *sadjust_expr;			/* sync adjust (if expression) */

	struct TAG_tnode *parent;			/* parent PARBARRIER */
	int exprisproctype;				/* set if the 'expression' of a PAR barrier is a PROCBARRIER reference (rather than something of type BARRIERTYPE) */
	int nprocbarriers;				/* incremented as PROCBARRIERs attached to this PARBARRIER are created */
} mwsyncpbinfo_t;

/* this one gets attached to an ALT or equivalent node */
typedef struct TAG_mwsyncaltinfo {
	int bcount;					/* non-zero if we have barrier syncs here */
	int nbcount;					/* non-zero if we have non-barrier syncs here */
} mwsyncaltinfo_t;


/* node types for multiway syncs */
typedef struct TAG_mwsi {
	struct TAG_tndef *node_LEAFTYPE;

	struct TAG_ntdef *tag_PARBARRIER;
	struct TAG_ntdef *tag_PROCBARRIER;

	struct TAG_ntdef *tag_BARRIERTYPE;
	struct TAG_ntdef *tag_BARRIERREFTYPE;
	struct TAG_ntdef *tag_PARBARRIERTYPE;
	struct TAG_ntdef *tag_PROCBARRIERTYPE;
} mwsi_t;

extern mwsi_t mwsi;

extern int mwsync_transsubtree (struct TAG_tnode **tptr, mwsynctrans_t *mwi);

extern mwsyncaltinfo_t *mwsync_newmwsyncaltinfo (void);
extern void mwsync_freemwsyncaltinfo (mwsyncaltinfo_t *altinf);
extern void mwsync_setaltinfo (struct TAG_tnode *node, mwsyncaltinfo_t *altinf);
extern mwsyncaltinfo_t *mwsync_getaltinfo (struct TAG_tnode *node);

extern int mwsync_mwsynctrans_makebarriertype (struct TAG_tnode **typep, mwsynctrans_t *mwi);
extern int mwsync_mwsynctrans_pushvar (struct TAG_tnode *varptr, struct TAG_tnode *bnames, struct TAG_tnode ***bodypp, struct TAG_ntdef *decltag, mwsynctrans_t *mwi);
extern int mwsync_mwsynctrans_pushparam (struct TAG_tnode *paramptr, struct TAG_tnode *pname, mwsynctrans_t *mwi);
extern int mwsync_mwsynctrans_popvar (struct TAG_tnode *varptr, mwsynctrans_t *mwi);
extern int mwsync_mwsynctrans_pushvarmark (mwsynctrans_t *mwi);
extern int mwsync_mwsynctrans_popvarto (const int level, mwsynctrans_t *mwi);
extern int mwsync_mwsynctrans_nameref (struct TAG_tnode **tptr, struct TAG_name *name, struct TAG_ntdef *decltag, mwsynctrans_t *mwi);
extern int mwsync_mwsynctrans_parallel (struct TAG_tnode *parnode, struct TAG_tnode **ipoint, struct TAG_tnode **bodies, int nbodies, mwsynctrans_t *mwi);

extern mwsyncpbinfo_t *mwsync_findpbinfointree (struct TAG_tnode *tptr, struct TAG_tnode *name);
extern struct TAG_name *mwsync_basenameof (struct TAG_name *name, struct TAG_map *mdata);

extern int mwsync_mwsynctrans_startnamerefs (mwsynctrans_t *mwi);
extern int mwsync_mwsynctrans_endnamerefs (mwsynctrans_t *mwi);

extern int mwsync_init (struct TAG_langparser *langptr);
extern int mwsync_setresignafterpar (int resign_after_par);
extern int mwsync_settransafterfetrans (int trans_after_fetrans);
extern int mwsync_shutdown (void);

extern struct TAG_feunit mwsync_feunit;


#endif	/* !__MWSYNC_H */

