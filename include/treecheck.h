/*
 *	treecheck.h -- tree-node checking interface for NOCC
 *	Copyright (C) 2007 Fred Barnes <frmb@kent.ac.uk>
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

#ifndef __TREECHECK_H
#define __TREECHECK_H

struct TAG_tnode;
struct TAG_tndef;

typedef struct TAG_treecheckdef {
	DYNARRAY (char *, descs);		/* human-readable descriptions for subnodes, names and hooks */
	char *invbefore;			/* pass before which this node-type is invalid */
	char *invafter;				/* pass after which this node-type is invalid */
	int cvalid;				/* whether the node type is currently valid */

	struct TAG_tndef *tndef;		/* node-type definition */
} treecheckdef_t;


extern int treecheck_init (void);
extern int treecheck_finalise (void);
extern int treecheck_shutdown (void);


extern treecheckdef_t *treecheck_createcheck (char *nodename, int nsub, int nname, int nhook, char **descs, char *invbefore, char *invafter);
extern int treecheck_destroycheck (treecheckdef_t *tcdef);

extern int treecheck_prepass (struct TAG_tnode *tree, const char *pname, const int penabled);
extern int treecheck_postpass (struct TAG_tnode *tree, const char *pname, const int penabled);


#endif	/* !__TREECHECK_H */

