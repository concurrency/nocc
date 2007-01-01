/*
 *	feunit.c -- front-end unit helper routines
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


/*{{{  includes*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/types.h>
#include <errno.h>

#include "nocc.h"
#include "support.h"
#include "version.h"
#include "symbols.h"
#include "keywords.h"
#include "lexer.h"
#include "tnode.h"
#include "parser.h"
#include "fcnlib.h"
#include "feunit.h"
#include "extn.h"
#include "dfa.h"
#include "langdef.h"
#include "parsepriv.h"
#include "lexpriv.h"
#include "names.h"
#include "target.h"


/*}}}*/


/*{{{  int feunit_init (void)*/
/*
 *	initialises feunit helpers
 *	returns 0 on success, non-zero on failure
 */
int feunit_init (void)
{
	return 0;
}
/*}}}*/
/*{{{  int feunit_shutdown (void)*/
/*
 *	shuts-down feunit helpers
 *	returns 0 on success, non-zero on failure
 */
int feunit_shutdown (void)
{
	return 0;
}
/*}}}*/



/*{{{  int feunit_do_init_tokens (int earlyfail, langdef_t *ldef, void *origin)*/
/*
 *	extracts and sets up tokens for a particular language definition (always top-level currently)
 *	returns 0 on success, non-zero on failure
 */
int feunit_do_init_tokens (int earlyfail, langdef_t *ldef, void *origin)
{
	int rval = 0;

	if (ldef && ldef->ident && langdef_hassection (ldef, ldef->ident)) {
		langdefsec_t *lsec = langdef_findsection (ldef, ldef->ident);

		if (!lsec) {
			nocc_error ("feunit_do_reg_reducers(): no \"%s\" section in language definition!", ldef->ident);
			return -1;
		} else {
			if (langdef_init_tokens (lsec, origin)) {
				/* failed */
				rval = -1;
			}
		}
	}
	return rval;
}
/*}}}*/
/*{{{  int feunit_do_init_nodes (feunit_t **felist, int earlyfail)*/
/*
 *	calls init_nodes on a set of feunits
 *	returns 0 on success, non-zero on failure
 */
int feunit_do_init_nodes (feunit_t **felist, int earlyfail)
{
	int i, rval = 0;

	for (i=0; felist[i]; i++) {
		if (felist[i]->init_nodes && felist[i]->init_nodes ()) {
			/* failed */
			rval = -1;
			if (earlyfail) {
				break;
			}
		}
	}
	return rval;
}
/*}}}*/
/*{{{  int feunit_do_reg_reducers (feunit_t **felist, int earlyfail, langdef_t *ldef)*/
/*
 *	calls reg_reducers on a set of feunits.  sets up reducers in feunit's language definitions if present.
 *	returns 0 on success, non-zero on failure
 */
int feunit_do_reg_reducers (feunit_t **felist, int earlyfail, langdef_t *ldef)
{
	int i, rval = 0;


	if (ldef && ldef->ident && langdef_hassection (ldef, ldef->ident)) {
		/* pull in general reductions for the language */
		langdefsec_t *lsec = langdef_findsection (ldef, ldef->ident);

		if (!lsec) {
			nocc_error ("feunit_do_reg_reducers(): no \"%s\" section in language definition!", ldef->ident);
			return -1;
		} else {
			if (langdef_reg_reducers (lsec)) {
				rval = -1;
				if (earlyfail) {
					return -1;
				}
			}
		}
	}
	for (i=0; felist[i]; i++) {
		if (felist[i]->reg_reducers && felist[i]->reg_reducers ()) {
			rval = -1;
			if (earlyfail) {
				break;
			}
		}

		if (felist[i]->ident && ldef && langdef_hassection (ldef, felist[i]->ident)) {
			/* load reductions from language definition */
			langdefsec_t *lsec = langdef_findsection (ldef, felist[i]->ident);

			if (!lsec) {
				nocc_error ("feunit_do_reg_reducers(): no \"%s\" section in %s language definition!", felist[i]->ident, ldef->ident);
				return -1;
			}
			if (langdef_reg_reducers (lsec)) {
				rval = -1;
				if (earlyfail) {
					break;
				}
			}
		}
	}
	return rval;
}
/*}}}*/


