/*
 *	feunit.h -- language front-end initialiser interface
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

#ifndef __FEUNIT_H
#define __FEUNIT_H

struct TAG_dfattbl;
struct TAG_langdef;

typedef struct TAG_feunit {
	int (*init_nodes)(void);				/* setup node types/tags */
	int (*reg_reducers)(void);				/* register named reduction functions */
	struct TAG_dfattbl **(*init_dfatrans)(int *);		/* setup DFA transition tables */
	int (*post_setup)(void);				/* incase anything else is needed */
	const char *ident;					/* ident of this unit (for loading language definitions) */
} feunit_t;


extern int feunit_init (void);
extern int feunit_shutdown (void);

extern int feunit_do_init_tokens (int earlyfail, struct TAG_langdef *ldef, void *origin);
extern int feunit_do_init_nodes (feunit_t **felist, int earlyfail);
extern int feunit_do_reg_reducers (feunit_t **felist, int earlyfail, struct TAG_langdef *ldef);


#endif	/* !__FEUNIT_H */

