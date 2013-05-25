/*
 *	cccsp.h -- top-level interface to the CCSP C backend.
 *	Copyright (C) 2008-2013 Fred Barnes <frmb@kent.ac.uk>
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

typedef enum ENUM_cccsp_apicall {
	NOAPI = 0,
	CHAN_IN = 1,
	CHAN_OUT = 2,
	STOP_PROC = 3,
	PROC_PAR = 4,
	LIGHT_PROC_INIT = 5,
	PROC_PARAM = 6,
	GET_PROC_PARAM = 7
} cccsp_apicall_e;

typedef struct TAG_cccsp_apicall {
	cccsp_apicall_e call;
	char *name;
	int needwptr;
} cccsp_apicall_t;

typedef struct TAG_cccsp_mapdata {
	int target_indir;				/* language-specific use (Guppy) */
	struct TAG_tnode *process_id;			/* current process identifier node (wptr) */
	void *langhook;					/* language-specific hook (Guppy) */
} cccsp_mapdata_t;

typedef struct TAG_cccsp_preallocate {
	struct TAG_target *target;
	int lexlevel;

	int collect;
} cccsp_preallocate_t;

extern int cccsp_set_initialiser (struct TAG_tnode *bename, struct TAG_tnode *init);
extern struct TAG_tnode *cccsp_create_apicallname (cccsp_apicall_e);
extern char *cccsp_make_entryname (const char *name, const int procabs);

extern struct TAG_tnode *cccsp_create_addrof (struct TAG_tnode *arg, struct TAG_target *target);
extern int cccsp_set_indir (struct TAG_tnode *benode, int indir, struct TAG_target *target);
extern int cccsp_get_indir (struct TAG_tnode *benode, struct TAG_target *target);

extern int cccsp_set_toplevelname (struct TAG_name *tlname, struct TAG_target *target);
extern struct TAG_tnode *cccsp_create_wptr (struct TAG_srclocn *org, struct TAG_target *target);
extern struct TAG_tnode *cccsp_create_workspace (struct TAG_srclocn *org, struct TAG_target *target);
extern int cccsp_set_workspace_nparams (struct TAG_tnode *wsnode, int nparams);
extern int cccsp_set_workspace_nwords (struct TAG_tnode *wsnode, int nwords);
extern struct TAG_tnode *cccsp_create_workspace_nwordsof (struct TAG_tnode *wsnode, struct TAG_target *target);

#endif	/* !__CCCSP_H */
