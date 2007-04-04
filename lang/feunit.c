/*
 *	feunit.c -- front-end unit helper routines
 *	Copyright (C) 2006-2007 Fred Barnes <frmb@kent.ac.uk>
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
#include "origin.h"
#include "version.h"
#include "symbols.h"
#include "keywords.h"
#include "lexer.h"
#include "tnode.h"
#include "treecheck.h"
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



/*{{{  int feunit_do_init_tokens (int earlyfail, langdef_t *ldef, origin_t *origin)*/
/*
 *	extracts and sets up tokens for a particular language definition (always top-level currently)
 *	returns 0 on success, non-zero on failure
 */
int feunit_do_init_tokens (int earlyfail, langdef_t *ldef, origin_t *origin)
{
	int rval = 0;

	if (ldef && ldef->ident && langdef_hassection (ldef, ldef->ident)) {
		langdefsec_t *lsec = langdef_findsection (ldef, ldef->ident);

		if (!lsec) {
			nocc_error ("feunit_do_reg_reducers(): no \"%s\" section in language definition!", ldef->ident);
			return -1;
		} else {
			if (langdef_init_tokens (lsec, origin_to_langtag (origin), origin)) {
				/* failed */
				rval = -1;
			}
		}
	}
	return rval;
}
/*}}}*/
/*{{{  int feunit_do_init_nodes (feunit_t **felist, int earlyfail, langdef_t *ldef, origin_t *origin)*/
/*
 *	calls init_nodes on a set of feunits.  also sets up any nodes defined in the relevant language-definition section(s) if present.
 *	returns 0 on success, non-zero on failure
 */
int feunit_do_init_nodes (feunit_t **felist, int earlyfail, langdef_t *ldef, origin_t *origin)
{
	int i, rval = 0;

	/*{{{  if we have a matching language section, init nodes from language definition*/
	if (ldef && ldef->ident && langdef_hassection (ldef, ldef->ident)) {
		langdefsec_t *lsec = langdef_findsection (ldef, ldef->ident);

		if (!lsec) {
			nocc_error ("feunit_do_init_nodes(): no \"%s\" section in language definition!", ldef->ident);
			return -1;
		} else {
			if (langdef_init_nodes (lsec, origin)) {
				/* failed */
				rval = -1;
				if (earlyfail) {
					return -1;
				}
			}
		}
	}

	/*}}}*/
	/*{{{  do init_nodes on front-end units*/
	for (i=0; felist[i]; i++) {
		if (felist[i]->init_nodes && felist[i]->init_nodes ()) {
			/* failed */
			rval = -1;
			if (earlyfail) {
				break;
			}
		}

		/* see if we have any in the language definition */
		if (ldef && felist[i]->ident && langdef_hassection (ldef, felist[i]->ident)) {
			/* init nodes in language definition section */
			langdefsec_t *lsec = langdef_findsection (ldef, felist[i]->ident);

			if (!lsec) {
				nocc_error ("feunit_do_init_nodes(): no \"%s\" section in %s language definition!", felist[i]->ident, ldef->ident);
				return -1;
			}
			if (langdef_init_nodes (lsec, origin)) {
				/* failed */
				rval = -1;
				if (earlyfail) {
					return -1;
				}
			}
		}
	}

	/*}}}*/
	/*{{{  if the language definition has a '<lang>-postprod' section, load nodes from this*/
	if (ldef && ldef->ident) {
		char *endsident = (char *)smalloc (strlen (ldef->ident) + 12);
		
		sprintf (endsident, "%s-postprod", ldef->ident);

		if (langdef_hassection (ldef, endsident)) {
			/* do top-level post-setup */
			langdefsec_t *lsec = langdef_findsection (ldef, endsident);

			if (!lsec) {
				nocc_error ("feunit_do_post_setup(): no \"%s\" section in language definition!", ldef->ident);
				return -1;
			} else {
				if (langdef_init_nodes (lsec, origin)) {
					rval = -1;
					if (earlyfail) {
						return -1;
					}
				}
			}
		}
	}

	/*}}}*/

	return rval;
}
/*}}}*/
/*{{{  int feunit_do_reg_reducers (feunit_t **felist, int earlyfail, langdef_t *ldef)*/
/*
 *	calls reg_reducers on a set of feunits.  sets up reducers in language definition section if present.
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
/*{{{  int feunit_do_init_dfatrans (feunit_t **felist, int earlyfail, langdef_t *ldef, langparser_t *lang, int doextn)*/
/*
 *	calls init_dfatrans on a set of feunits.  sets up DFA rules in feunit's langugage definitions if present.
 *	also uses language name to read pre and post grammars ("lang" and "lang-postprod").
 *	also resolves, etc. the DFAs
 *	returns 0 on success, non-zero on failure
 */
int feunit_do_init_dfatrans (feunit_t **felist, int earlyfail, langdef_t *ldef, langparser_t *lang, int doextn)
{
	int i, x = 0;
	DYNARRAY (dfattbl_t *, transtbls);
	int rval = 0;

	/*{{{  initialise*/
	dfa_clear_deferred ();
	dynarray_init (transtbls);

	/*}}}*/
	/*{{{  pre-production DFAs (from <lang> section)*/
	if (ldef && langdef_hassection (ldef, ldef->ident)) {
		langdefsec_t *lsec = langdef_findsection (ldef, ldef->ident);

		if (!lsec) {
			nocc_error ("feunit_do_init_dfatrans(): no \"%s\" section in language definition!", ldef->ident);
			rval = -1;
			if (earlyfail) {
				goto local_out;
			}
		} else {
			dfattbl_t **t_table;
			int t_size = 0;

			t_table = langdef_init_dfatrans (lsec, &t_size);
			if (t_size > 0) {
				/* add them */
				int j;

				for (j=0; j<t_size; j++) {
					dynarray_add (transtbls, t_table[j]);
				}
			}
			if (t_table) {
				sfree (t_table);
			}
		}
	}

	/*}}}*/
	/*{{{  per front-end unit DFAs*/
	for (i=0; felist[i]; i++) {
		if (felist[i]->init_dfatrans) {
			dfattbl_t **t_table;
			int t_size = 0;

			/* can't fail in any meaningful way, but heyho */
			t_table = felist[i]->init_dfatrans (&t_size);
			if (t_size > 0) {
				/* add them */
				int j;

				for (j=0; j<t_size; j++) {
					dynarray_add (transtbls, t_table[j]);
				}
			}
			if (t_table) {
				sfree (t_table);
			}
		}

		if (ldef && felist[i]->ident && langdef_hassection (ldef, felist[i]->ident)) {
			/* load DFA grammars from language definition */
			langdefsec_t *lsec = langdef_findsection (ldef , felist[i]->ident);

			if (!lsec) {
				nocc_error ("feunit_do_init_dfatrans(): no \"%s\" section in %s language definition!", felist[i]->ident, ldef->ident);
				rval = -1;
				if (earlyfail) {
					goto local_out;
				}
			} else {
				dfattbl_t **t_table;
				int t_size = 0;

				t_table = langdef_init_dfatrans (lsec, &t_size);
				if (t_size > 0) {
					/* add them */
					int j;

					for (j=0; j<t_size; j++) {
						dynarray_add (transtbls, t_table[j]);
					}
				}
				if (t_table) {
					sfree (t_table);
				}
			}
		}
	}
	/*}}}*/
	/*{{{  post-production DFAs (from <lang-postprod> section)*/
	if (ldef) {
		char *endsident = (char *)smalloc (strlen (ldef->ident) + 12);
		
		sprintf (endsident, "%s-postprod", ldef->ident);

		if (langdef_hassection (ldef, endsident)) {
			langdefsec_t *lsec = langdef_findsection (ldef, endsident);

			if (!lsec) {
				nocc_error ("feunit_do_init_dfatrans(): no \"%s\" section in %s language definition!", endsident, ldef->ident);
				rval = -1;
				if (earlyfail) {
					goto local_out;
				}
			} else {
				dfattbl_t **t_table;
				int t_size = 0;

				t_table = langdef_init_dfatrans (lsec, &t_size);
				if (t_size > 0) {
					/* add them */
					int j;

					for (j=0; j<t_size; j++) {
						dynarray_add (transtbls, t_table[j]);
					}
				}
				if (t_table) {
					sfree (t_table);
				}
			}
		}
		sfree (endsident);
	}

	/*}}}*/

	/*{{{  load grammar items for extensions -- if requested*/
	if (doextn && lang) {
		if (extn_preloadgrammar (lang, &DA_PTR(transtbls), &DA_CUR(transtbls), &DA_MAX(transtbls))) {
			rval = -1;
			if (earlyfail) {
				goto local_out;
			}
		}
	}
	/*}}}*/
	/*{{{  do DFA transition-table merges*/
	dfa_mergetables (DA_PTR (transtbls), DA_CUR (transtbls));

	/*}}}*/

	/*{{{  debug dump of grammars if requested*/
	if (compopts.dumpgrammar) {
		for (i=0; i<DA_CUR (transtbls); i++) {
			dfattbl_t *ttbl = DA_NTHITEM (transtbls, i);

			if (ttbl) {
				dfa_dumpttbl (stderr, ttbl);
			}
		}
	}

	/*}}}*/

	/*{{{  debugging dump for visualisation here :)*/
	if (compopts.savenameddfa[0] && compopts.savenameddfa[1]) {
		FILE *ostream = fopen (compopts.savenameddfa[1], "w");

		if (!ostream) {
			nocc_error ("failed to open %s for writing: %s", compopts.savenameddfa[1], strerror (errno));
			/* ignore this generally */
		} else {
			for (i=0; i<DA_CUR (transtbls); i++) {
				dfattbl_t *ttbl = DA_NTHITEM (transtbls, i);

				if (!ttbl->op && ttbl->name && !strcmp (compopts.savenameddfa[0], ttbl->name)) {
					dfa_dumpttbl_gra (ostream, ttbl);
				}
			}
			fclose (ostream);
		}
	}

	/*}}}*/
	/*{{{  convert into DFA nodes proper*/

	x = 0;
	for (i=0; i<DA_CUR (transtbls); i++) {
		dfattbl_t *ttbl = DA_NTHITEM (transtbls, i);

		/* only convert non-addition nodes */
		if (ttbl && !ttbl->op) {
			x += !dfa_tbltodfa (ttbl);
		}
	}

	if (compopts.dumpgrammar) {
		dfa_dumpdeferred (stderr);
	}

	if (dfa_match_deferred ()) {
		/* failed */
		rval = -1;
		if (earlyfail) {
			goto local_out;
		}
	}

	/*}}}*/
	/*{{{  load DFA items for extensions -- if requested*/
	if (doextn && lang) {
		if (extn_postloadgrammar (lang)) {
			rval = -1;
			if (earlyfail) {
				goto local_out;
			}
		}
	}

	/*}}}*/

local_out:
	/*{{{  free up tables*/
	for (i=0; i<DA_CUR (transtbls); i++) {
		dfattbl_t *ttbl = DA_NTHITEM (transtbls, i);

		if (ttbl) {
			dfa_freettbl (ttbl);
		}
	}
	dynarray_trash (transtbls);

	/*}}}*/

	if (x && !rval) {
		rval = 1;
	}
	
	return rval;
}
/*}}}*/
/*{{{  int feunit_do_post_setup (feunit_t **felist, int earlyfail, langdef_t *ldef)*/
/*
 *	calls post_setup on a set of feunits.  sets up any post-setup in language definition section if present.
 *	returns 0 on success, non-zero on failure
 */
int feunit_do_post_setup (feunit_t **felist, int earlyfail, langdef_t *ldef)
{
	int i, rval = 0;

	for (i=0; felist[i]; i++) {
		if (felist[i]->post_setup && felist[i]->post_setup ()) {
			rval = -1;
			if (earlyfail) {
				break;
			}
		}
		if (felist[i]->ident && ldef && langdef_hassection (ldef, felist[i]->ident)) {
			/* load post-setup information from language definition */
			langdefsec_t *lsec = langdef_findsection (ldef, felist[i]->ident);

			if (!lsec) {
				nocc_error ("feunit_do_post_setup(): no \"%s\" section in %s language definition!", felist[i]->ident, ldef->ident);
				return -1;
			}
			if (langdef_post_setup (lsec)) {
				rval = -1;
				if (earlyfail) {
					break;
				}
			}
		}
	}
	if (ldef && ldef->ident) {
		char *endsident = (char *)smalloc (strlen (ldef->ident) + 12);
		
		sprintf (endsident, "%s-postprod", ldef->ident);

		if (langdef_hassection (ldef, endsident)) {
			/* do top-level post-setup */
			langdefsec_t *lsec = langdef_findsection (ldef, endsident);

			if (!lsec) {
				nocc_error ("feunit_do_post_setup(): no \"%s\" section in language definition!", ldef->ident);
				return -1;
			} else {
				if (langdef_post_setup (lsec)) {
					rval = -1;
					if (earlyfail) {
						return -1;
					}
				}
			}
		}
	}
	return rval;
}
/*}}}*/


