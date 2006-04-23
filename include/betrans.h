/*
 *	betrans.h -- interface to back-end tree transform
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

#ifndef __BETRANS_H
#define __BETRANS_H

struct TAG_tnode;
struct TAG_target;
struct TAG_chook;
struct TAG_ntdef;

typedef struct TAG_betrans {
	struct TAG_target *target;
	struct TAG_tnode **insertpoint;
	struct TAG_chook *betranstaghook;
	struct TAG_chook *betransnodehook;
	void *priv;			/* for language attachment */
} betrans_t;


typedef struct TAG_betranstag {		/* used with the betrans:tag compiler-hook */
	struct TAG_ntdef *flag;			/* node-tag used to flag certain things */
	int val;
} betranstag_t;


extern int betrans_init (void);
extern int betrans_shutdown (void);


extern betranstag_t *betrans_newtag (struct TAG_ntdef *tag, int val);
extern void betrans_tagnode (struct TAG_tnode *t, struct TAG_ntdef *tag, int val, betrans_t *be);
extern struct TAG_ntdef *betrans_gettag (struct TAG_tnode *t, int *valp, betrans_t *be);

extern int betrans_subtree (struct TAG_tnode **tptr, betrans_t *bt);
extern int betrans_tree (struct TAG_tnode **tptr, struct TAG_target *target);


#endif	/* !__BETRANS_H */

