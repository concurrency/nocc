/*
 *	occampi_initial.c -- INITIAL declarations
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

/* INITIAL declarations were added to the original compiler by Jim Moores */

/* note: this is the sort of thing extensions are intended to provide, but in
 * this case it's a bit excessive.  The INITIAL keyword is built-in, but extensions
 * would probably need to create them.  However, it doesn't store anything in
 * the occampi language structure (more self-contained the better :)) -- other
 * bits could still probably lookup to see if they exist, though ..
 */

/* FIXME: also it's a little ugly in here, until abbreviations are supported.. */

/*{{{  includes*/
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/types.h>

#include "nocc.h"
#include "support.h"
#include "version.h"
#include "symbols.h"
#include "keywords.h"
#include "lexer.h"
#include "lexpriv.h"
#include "tnode.h"
#include "parser.h"
#include "dfa.h"
#include "parsepriv.h"
#include "occampi.h"
#include "feunit.h"
#include "names.h"
#include "scope.h"
#include "prescope.h"


/*}}}*/


/*{{{  private data*/
static ntdef_t *tag_INITIAL = NULL;

/*}}}*/


/*{{{  static int occampi_prescope_initial (tnode_t **nodep, prescope_t *ps)*/
/*
 *	pre-scopes an INITIAL declaration -- doesn't do anything yet ... (FIXME)
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_prescope_initial (tnode_t **nodep, prescope_t *ps)
{
	return 1;
}
/*}}}*/
/*{{{  static int occampi_scopein_initial (tnode_t **nodep, scope_t *ss)*/
/*
 *	scopes an INITIAL declaration -- which shouldn't really be here, but for now..
 *	FIXME: in prescope once abbreviations are sorted
 */
static int occampi_scopein_initial (tnode_t **nodep, scope_t *ss)
{
	tnode_t *name = tnode_nthsubof (*nodep, 0);
	tnode_t *type;
	char *rawname;
	name_t *sname = NULL;
	tnode_t *newname, *assign, **bodyp;

	if (name->tag != opi.tag_NAME) {
		scope_error (name, ss, "name not raw-name!");
		return 0;
	}
	rawname = tnode_nthhookof (name, 0);
#if 0
fprintf (stderr, "occampi_scopein_initial: here! rawname = \"%s\"\n", rawname);
#endif

	if (scope_subtree (tnode_nthsubaddr (*nodep, 1), ss)) {
		/* failed to scope type */
		return 0;
	}
	if (scope_subtree (tnode_nthsubaddr (*nodep, 3), ss)) {
		/* failed to scope expression */
		return 0;
	}

	type = tnode_nthsubof (*nodep, 1);

	sname = name_addscopename (rawname, *nodep, type, NULL);
	newname = tnode_createfrom (opi.tag_NDECL, name, sname);
	SetNameNode (sname, newname);
	tnode_setnthsub (*nodep, 0, newname);

	/* now the new name is in scope, build an assignment */
	assign = tnode_createfrom (opi.tag_ASSIGN, *nodep, tnode_copytree (name), tnode_nthsubof (*nodep, 3), NULL);

	/* free the old name */
	tnode_free (name);
	ss->scoped++;

	/* build new body and scope it */
	scope_subtree (&assign, ss);
	bodyp = tnode_nthsubaddr (*nodep, 2);

	*bodyp = tnode_createfrom (opi.tag_SEQ, *nodep, NULL, parser_buildlistnode ((*nodep)->org_file, assign, *bodyp, NULL));

	/* fix this node to be a variable declaration */
	*nodep = tnode_createfrom (opi.tag_VARDECL, *nodep, tnode_nthsubof (*nodep, 0), tnode_nthsubof (*nodep, 1), tnode_nthsubof (*nodep, 2));

#if 0
fprintf (stderr, "occampi_scopein_initial(): *bodyp before scoping it is:\n");
tnode_dumptree (*bodyp, 1, stderr);
#endif
	scope_subtree (tnode_nthsubaddr (*nodep, 2), ss);

	return 0;
}
/*}}}*/
/*{{{  static int occampi_scopeout_initial (tnode_t **nodep, scope_t *ss)*/
/*
 *	scopes out an INITIAL declaration -- dummy..
 *	FIXME: in prescope once abbreviations are sorted
 */
static int occampi_scopeout_initial (tnode_t **nodep, scope_t *ss)
{
	tnode_t *name = tnode_nthsubof (*nodep, 0);
	name_t *sname;

	if (name->tag != opi.tag_NDECL) {
		scope_error (name, ss, "not NDECL!");
		return 0;
	}
	sname = tnode_nthnameof (name, 0);

#if 0
fprintf (stderr, "occampi_scopeout_initial: here! sname->me->name = \"%s\"\n", sname->me->name);
#endif

	name_descopename (sname);

	return 1;
}
/*}}}*/


/*{{{  static int occampi_initial_init_nodes (void)*/
/*
 *	initialises nodes for occam-pi INITIAL declarations
 */
static int occampi_initial_init_nodes (void)
{
	tndef_t *tnd;
	int i;
	compops_t *cops;

	/*{{{  occampi:initialnode -- INITIAL*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:initialnode", &i, 4, 0, 0, TNF_SHORTDECL);
	cops = tnode_newcompops ();
	cops->prescope = occampi_prescope_initial;
	cops->scopein = occampi_scopein_initial;
	cops->scopeout = occampi_scopeout_initial;
	tnd->ops = cops;

	i = -1;
	tag_INITIAL = tnode_newnodetag ("INITIAL", &i, tnd, NTF_NONE);

	/*}}}*/

	return 0;
}
/*}}}*/
/*{{{  static int occampi_initial_reg_reducers (void)*/
/*
 *	registers reducers for INITIAL declarations
 */
static int occampi_initial_reg_reducers (void)
{
	parser_register_grule ("opi:initialreduce", parser_decode_grule ("SN1N+N+N+<0VC4R-", tag_INITIAL));

	return 0;
}
/*}}}*/
/*{{{  static dfattbl_t **occampi_initial_init_dfatrans (int *ntrans)*/
/*
 *	creates and returns DFA transition tables for INITIAL declarations
 */
static dfattbl_t **occampi_initial_init_dfatrans (int *ntrans)
{
	DYNARRAY (dfattbl_t *, transtbl);

	dynarray_init (transtbl);
	dynarray_add (transtbl, dfa_transtotbl ("occampi:declorprocstart +:= [ 0 @INITIAL 1 ] [ 1 occampi:name 2 ] [ 1 occampi:type 2 ] [ 2 occampi:name 3 ] [ 3 @IS 4 ] " \
				"[ 4 occampi:expr 5 ] [ 5 @@: 6 ] [ 6 {<opi:initialreduce>} Newline 7 ] [ 7 -* ]"));

	*ntrans = DA_CUR (transtbl);
	return DA_PTR (transtbl);
}
/*}}}*/



/*{{{  occampi_initial_feunit (feunit_t)*/
feunit_t occampi_initial_feunit = {
	init_nodes: occampi_initial_init_nodes,
	reg_reducers: occampi_initial_reg_reducers,
	init_dfatrans: occampi_initial_init_dfatrans,
	post_setup: NULL
};
/*}}}*/

