/*
 *	occampi_parser.c -- occam-pi parser for nocc
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
#include <errno.h>

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
#include "typecheck.h"
#include "extn.h"

/*}}}*/

/*{{{  forward decls*/
static int occampi_parser_init (lexfile_t *lf);
static void occampi_parser_shutdown (lexfile_t *lf);
static tnode_t *occampi_parser_parse (lexfile_t *lf);
static int occampi_parser_prescope (tnode_t **tptr, prescope_t *ps);
static int occampi_parser_scope (tnode_t **tptr, scope_t *ss);
static int occampi_parser_typecheck (tnode_t *tptr, typecheck_t *tc);

static tnode_t *occampi_indented_process (lexfile_t *lf);
static tnode_t *occampi_indented_process_list (lexfile_t *lf, char *leaddfa);


/*}}}*/
/*{{{  global vars*/

occampi_pset_t opi;		/* attach tags, etc. here */

langparser_t occampi_parser = {
	langname:	"occam-pi",
	init:		occampi_parser_init,
	shutdown:	occampi_parser_shutdown,
	parse:		occampi_parser_parse,
	prescope:	occampi_parser_prescope,
	scope:		occampi_parser_scope,
	typecheck:	occampi_parser_typecheck,
	tagstruct_hook:	(void *)&opi,
	lexer:		NULL
};


/*}}}*/
/*{{{  private types/vars*/
typedef struct {
	dfanode_t *inode;
} occampi_parse_t;


static occampi_parse_t *occampi_priv = NULL;

static feunit_t *feunit_set[] = {
	&occampi_primproc_feunit,
	&occampi_cnode_feunit,
	&occampi_snode_feunit,
	&occampi_decl_feunit,
	&occampi_dtype_feunit,
	&occampi_action_feunit,
	&occampi_lit_feunit,
	&occampi_type_feunit,
	&occampi_instance_feunit,
	&occampi_oper_feunit,
	&occampi_function_feunit,
	&occampi_mobiles_feunit,
	&occampi_initial_feunit,
	NULL
};

/*}}}*/


/*{{{  occampi reductions*/
/*{{{  void *occampi_nametoken_to_hook (void *ntok)*/
/*
 *	turns a name token into a hooknode for a tag_NAME
 */
void *occampi_nametoken_to_hook (void *ntok)
{
	token_t *tok = (token_t *)ntok;
	char *rawname;

	rawname = tok->u.name;
	tok->u.name = NULL;

	lexer_freetoken (tok);

	return (void *)rawname;
}
/*}}}*/
/*{{{  void *occampi_integertoken_to_hook (void *itok)*/
/*
 *	turns an integer token into a hooknode for tag_LITINT
 */
void *occampi_integertoken_to_hook (void *itok)
{
	token_t *tok = (token_t *)itok;
	occampi_litdata_t *ldata = (occampi_litdata_t *)smalloc (sizeof (occampi_litdata_t));

	ldata->bytes = sizeof (tok->u.ival);
	ldata->data = mem_ndup (&(tok->u.ival), ldata->bytes);

	lexer_freetoken (tok);

	return (void *)ldata;
}
/*}}}*/
/*{{{  void *occampi_realtoken_to_hook (void *itok)*/
/*
 *	turns an real token into a hooknode for tag_LITREAL
 */
void *occampi_realtoken_to_hook (void *itok)
{
	token_t *tok = (token_t *)itok;
	occampi_litdata_t *ldata = (occampi_litdata_t *)smalloc (sizeof (occampi_litdata_t));

	ldata->bytes = sizeof (tok->u.dval);
	ldata->data = mem_ndup (&(tok->u.dval), ldata->bytes);

	lexer_freetoken (tok);

	return (void *)ldata;
}
/*}}}*/
/*{{{  static void occampi_inlistreduce (dfastate_t *dfast, parsepriv_t *pp, void *rarg)*/
/*
 *	this reduces into a list
 */
static void occampi_inlistreduce (dfastate_t *dfast, parsepriv_t *pp, void *rarg)
{
	tnode_t *node;

	if (!parser_islistnode (dfast->local)) {
		/* make into a listnode, set it */
		node = dfast->local;

		dfast->local = parser_newlistnode (pp->lf);
		parser_addtolist (dfast->local, node);
	}
	dfast->ptr = parser_addtolist (dfast->local, NULL);

	return;
}
/*}}}*/
/*{{{  static void occampi_debug_gstack (void **items, int icnt)*/
/*
 *	used for debugging generic reductions
 */
static void occampi_debug_gstack (void **items, int icnt)
{
	int i;

	fprintf (stderr, "occampi_debug_gstack(): icnt=%d, items:  ", icnt);
	for (i=0; i<icnt; i++) {
		fprintf (stderr, "0x%8.8x  ", (unsigned int)(items[i]));
	}
	fprintf (stderr, "\n");
	return;
}
/*}}}*/


/*{{{  static void occampi_register_reducers (void)*/
/*
 *	register reduction functions with the parser
 *	returns 0 on success, non-zero on error
 */
static int occampi_register_reducers (void)
{
	int i;

	parser_register_reduce ("Roccampi:inlist", occampi_inlistreduce, NULL);

	/*{{{  setup generic reductions*/
	parser_register_grule ("opi:nullreduce", parser_decode_grule ("N+R-"));
	parser_register_grule ("opi:nullpush", parser_decode_grule ("0N-"));
	parser_register_grule ("opi:nullset", parser_decode_grule ("0R-"));
	parser_register_grule ("opi:subscriptreduce", parser_decode_grule ("N+N+0C3R-", opi.tag_SUBSCRIPT));
	parser_register_grule ("opi:xsubscriptreduce", parser_decode_grule ("N+N+V0C3R-", opi.tag_SUBSCRIPT));
	/*}}}*/

	for (i=0; feunit_set[i]; i++) {
		feunit_t *thisunit = feunit_set[i];

		if (thisunit->reg_reducers && thisunit->reg_reducers ()) {
			return -1;
		}
	}

	return 0;
}
/*}}}*/
/*}}}*/
/*{{{  error handling*/
/*{{{  static void occampi_namestart_dfaeh_stuck (dfanode_t *dfanode, token_t *tok)*/
/*
 *	called when the parser gets stuck inside an "occampi:namestart"
 */
static void occampi_namestart_dfaeh_stuck (dfanode_t *dfanode, token_t *tok)
{
	char *msg;

	msg = dfa_expectedmatchstr (dfanode, tok, "in declaration or process");
	parser_error (tok->origin, msg);

	return;
}
/*}}}*/
/*{{{  static void occampi_namestartname_dfaeh_stuck (dfanode_t *dfanode, token_t *tok)*/
/*
 *	called when the parser gets stuck inside an "occampi:namestartname"
 */
static void occampi_namestartname_dfaeh_stuck (dfanode_t *dfanode, token_t *tok)
{
	char *msg;

	msg = dfa_expectedmatchstr (dfanode, tok, "in declaration or process");
	parser_error (tok->origin, msg);

	return;
}
/*}}}*/


/*}}}*/


/*{{{  static int occampi_dfas_init (void)*/
/*
 *	initialises the occam-pi DFA structures
 *	returns 0 on success, non-zero on error
 */
static int occampi_dfas_init (void)
{
	int x, i;
	DYNARRAY (dfattbl_t *, transtbls);


	/*{{{  create DFAs*/
	dfa_clear_deferred ();
	dynarray_init (transtbls);

	for (i=0; feunit_set[i]; i++) {
		feunit_t *thisunit = feunit_set[i];

		if (thisunit->init_dfatrans) {
			dfattbl_t **t_table;
			int t_size = 0;

			/* can't fail in any meaningful way, but heyho */
			t_table = thisunit->init_dfatrans (&t_size);
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

	dynarray_add (transtbls, dfa_transtotbl ("occampi:exprnamestart ::= [ 0 +Name 1 ] [ 1 @@( 2 ] [ 1 @@[ 5 ] [ 2 {<opi:namepush>} ] [ 2 @@) 3 ] [ 3 {<opi:nullreduce>} -* ] " \
				"[ 1 -* 4 ] [ 4 {<opi:namereduce>} -* ] [ 5 {<opi:namepush>} ] [ 5 occampi:expr 6 ] [ 6 @@] 7 ] [ 7 {<opi:xsubscriptreduce>} -* ]"));
	dynarray_add (transtbls, dfa_transtotbl ("occampi:expr +:= [ 0 -Name 1 ] [ 0 +Integer 3 ] [ 0 +Real 4 ] [ 0 @@( 7 ] [ 1 occampi:exprnamestart 2 ] [ 2 {<opi:nullreduce>} -* 5 ] " \
				"[ 3 {<opi:integerreduce>} -* 5 ] [ 4 {<opi:realreduce>} -* 5 ] [ 5 -* ] [ 5 %occampi:restofexpr 6 ] [ 6 {<opi:resultpush>} ] [ 6 -* <occampi:restofexpr> ] " \
				"[ 7 occampi:expr 8 ] [ 8 @@) 9 ] [ 9 {<opi:nullreduce>} -* ]"));
	dynarray_add (transtbls, dfa_transtotbl ("occampi:operand +:= [ 0 +Name 1 ] [ 1 {<opi:namereduce>} -* ]"));
	/* dynarray_add (transtbls, dfa_bnftotbl ("occampi:expr ::= ( -Name occampi:exprnamestart {<opi:nullreduce>} | +Integer {<opi:integerreduce>} | +Real {<opi:realreduce>} )")); */
	dynarray_add (transtbls, dfa_bnftotbl ("occampi:exprsemilist ::= { occampi:expr @@; 1 }"));
	dynarray_add (transtbls, dfa_bnftotbl ("occampi:exprcommalist ::= { occampi:expr @@, 1 }"));
	dynarray_add (transtbls, dfa_transtotbl ("occampi:namestart ::= [ 0 +Name 1 ] [ 1 {<opi:namepush>} ] [ 1 -* <occampi:namestartname> ]"));

	dynarray_add (transtbls, dfa_transtotbl ("occampi:bracketstart ::= [ 0 +@@[ 1 ] [ 1 +Integer 2 ] [ 2 +@@] 3 ] [ 3 {<parser:rewindtokens>} -* <occampi:vardecl:bracketstart> ]"));

	dynarray_add (transtbls, dfa_bnftotbl ("occampi:declorprocstart +:= ( occampi:vardecl | occampi:abbrdecl | occampi:procdecl | occampi:typedecl | occampi:primproc | occampi:cproc | occampi:namestart | " \
				"occampi:builtinprocinstance | occampi:bracketstart ) {<opi:nullreduce>}"));

	/*{{{  load grammar items for extensions*/
	if (extn_preloadgrammar (&occampi_parser, &DA_PTR(transtbls), &DA_CUR(transtbls), &DA_MAX(transtbls))) {
		return 1;
	}

	/*}}}*/

	dfa_mergetables (DA_PTR (transtbls), DA_CUR (transtbls));

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
			return 1;
		}
		for (i=0; i<DA_CUR (transtbls); i++) {
			dfattbl_t *ttbl = DA_NTHITEM (transtbls, i);

			if (!ttbl->op && ttbl->name && !strcmp (compopts.savenameddfa[0], ttbl->name)) {
				dfa_dumpttbl_gra (ostream, ttbl);
			}
		}
		fclose (ostream);
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
		/* failed here, get out */
		return 1;
	}

	/*}}}*/
	/*{{{  free up tables*/
	for (i=0; i<DA_CUR (transtbls); i++) {
		dfattbl_t *ttbl = DA_NTHITEM (transtbls, i);

		if (ttbl) {
			dfa_freettbl (ttbl);
		}
	}
	dynarray_trash (transtbls);

	/*}}}*/
	/*{{{  load DFA items for extensions*/
	if (extn_postloadgrammar (&occampi_parser)) {
		return 1;
	}

	/*}}}*/

	if (x) {
		return 1;
	}
	/*}}}*/
	/*{{{  others (tokens)*/
	opi.tok_COLON = lexer_newtoken (SYMBOL, ":");
	opi.tok_HASH = lexer_newtoken (SYMBOL, "#");
	opi.tok_STRING = lexer_newtoken (STRING, NULL);

	/*}}}*/
	/*{{{  COMMENT: very manual construction*/
#if 0
	decl = dfa_newnode ();
	tmp = dfa_newnode ();
	tmp2 = dfa_newnode ();
	dfa_matchpush (decl, "occampi:primtype", tmp);
	dfa_matchpush (tmp, "occampi:name", tmp2);
	tmp = tmp2;
	tmp2 = dfa_newnode_init (occampi_reduce_decl);
	dfa_addmatch (tmp, lexer_newtoken (SYMBOL, ":"), tmp2, 0);
	dfa_defaultreturn (tmp2);			/* makes this a returning state */
	dfa_setname (decl, "occampi:decl");

	dfa_dumpdfas (stderr);
#endif
	/*}}}*/
	return 0;
}
/*}}}*/
/*{{{  static void occampi_nodes_init (void)*/
/*
 *	initialises the occam-pi node-types and node-tags
 *	returns 0 on success, non-zero on error
 */
static int occampi_nodes_init (void)
{
	int i;

	for (i=0; feunit_set[i]; i++) {
		feunit_t *thisunit = feunit_set[i];

		if (thisunit->init_nodes && thisunit->init_nodes ()) {
			return -1;
		}
	}

	return 0;
}
/*}}}*/
/*{{{  static int occampi_post_setup (void)*/
/*
 *	calls any post-setup routines for the parser
 */
static int occampi_post_setup (void)
{
	int i;
	static dfaerrorhandler_t namestart_eh = { occampi_namestart_dfaeh_stuck };
	static dfaerrorhandler_t namestartname_eh = { occampi_namestartname_dfaeh_stuck };

	for (i=0; feunit_set[i]; i++) {
		feunit_t *thisunit = feunit_set[i];

		if (thisunit->post_setup && thisunit->post_setup ()) {
			return -1;
		}
	}

	dfa_seterrorhandler ("occampi:namestart", &namestart_eh);
	dfa_seterrorhandler ("occampi:namestartname", &namestartname_eh);

	return 0;
}
/*}}}*/
/*{{{  void occampi_isetindent (FILE *stream, int indent)*/
/*
 *	set-indent for debugging output
 */
void occampi_isetindent (FILE *stream, int indent)
{
	int i;

	for (i=0; i<indent; i++) {
		fprintf (stream, "    ");
	}
	return;
}
/*}}}*/


/*{{{  static tnode_t *occampi_includefile (char *fname, lexfile_t *curlf)*/
/*
 *	includes a file
 *	returns a tree or NULL
 */
static tnode_t *occampi_includefile (char *fname, lexfile_t *curlf)
{
	tnode_t *tree;
	lexfile_t *lf;

	lf = lexer_open (fname);
	if (!lf) {
		parser_error (curlf, "failed to open #INCLUDE'd file %s", fname);
		return NULL;
	}
	if (compopts.verbose) {
		nocc_message ("sub-parsing ...");
	}
	tree = parser_parse (lf);
	if (!tree) {
		parser_error (curlf, "failed to parse #INCLUDE'd file %s", fname);
		lexer_close (lf);
		return NULL;
	}
	lexer_close (lf);

	return tree;
}
/*}}}*/
/*{{{  static void occampi_parser_init (lexfile_t *lf)*/
/*
 *	initialises the occam-pi parser
 *	returns 0 on success, non-zero on error
 */
static int occampi_parser_init (lexfile_t *lf)
{
	if (compopts.verbose) {
		nocc_message ("initialising occam-pi parser..");
	}
	if (!occampi_priv) {
		occampi_priv = (occampi_parse_t *)smalloc (sizeof (occampi_parse_t));
		occampi_priv->inode = NULL;

		/* wipe out the "opi" structure */
		memset ((void *)&opi, 0, sizeof (opi));

		/* initialise! */
		if (occampi_nodes_init ()) {
			nocc_error ("occampi_parser_init(): failed to initialise nodes");
			return 1;
		}
		if (occampi_register_reducers ()) {
			nocc_error ("occampi_parser_init(): failed to register reducers");
			return 1;
		}
		if (occampi_dfas_init ()) {
			nocc_error ("occampi_parser_init(): failed to initialise DFAs");
			return 1;
		}
		if (occampi_post_setup ()) {
			nocc_error ("occampi_parser_init(): failed to post-setup");
			return 1;
		}

		occampi_priv->inode = dfa_lookupbyname ("occampi:declorprocstart");
		if (!occampi_priv->inode) {
			nocc_error ("occampi_parser_init(): could not find occampi:declorprocstart!");
			return 1;
		}
		if (compopts.dumpdfas) {
			dfa_dumpdfas (stderr);
		}
	}
	return 0;
}
/*}}}*/
/*{{{  static void occampi_parser_shutdown (lexfile_t *lf)*/
/*
 *	shuts-down the occam-pi parser
 */
static void occampi_parser_shutdown (lexfile_t *lf)
{
	return;
}
/*}}}*/


/*{{{  static tnode_t *occampi_declorprocstart (lexfile_t *lf, int *gotall, char *thedfa)*/
/*
 *	parses a declaration/process for single-liner's, or start of a declaration/process
 *	for multi-liners.  Sets "gotall" non-zero if the tree returned is largely whole
 */
static tnode_t *occampi_declorprocstart (lexfile_t *lf, int *gotall, char *thedfa)
{
	token_t *tok;
	tnode_t *tree = NULL;

	/*
	 * for starts of declarations/processes, parsing things like:
	 *     PROC name (paramlist)
	 *     INT FUNCTION name (paramlist)
	 *     DATA TYPE
	 *     CHAN TYPE
	 *     VALOF
	 *     SEQ, PAR, IF, ALT
	 *     WHILE
	 *     CASE
	 *     in ? CASE
	 *     etc.
	 */

	*gotall = 0;
restartpoint:
	/* skip newlines/comments */
	tok = lexer_nexttoken (lf);
	while (tok && ((tok->type == NEWLINE) || (tok->type == COMMENT))) {
		lexer_freetoken (tok);
		tok = lexer_nexttoken (lf);
	}

	if (!tok || (tok->type == END)) {
		if (tok) {
			lexer_freetoken (tok);
		}
		return tree;
	}

	if (lexer_tokmatch (opi.tok_HASH, tok)) {
		/*{{{  probably a pre-processor action of some kind*/
		token_t *nexttok = lexer_nexttoken (lf);

		if (nexttok && lexer_tokmatchlitstr (nexttok, "INCLUDE")) {
			/*{{{  #INCLUDE*/
			lexer_freetoken (tok);
			lexer_freetoken (nexttok);

			nexttok = lexer_nexttoken (lf);
			if (nexttok && lexer_tokmatch (opi.tok_STRING, nexttok)) {
				/*{{{  include another file*/
#if 0
fprintf (stderr, "occampi_declorprocstart(): think i should be including another file about here.. :)  [%s]\n", nexttok->u.str.ptr);
#endif
				tree = occampi_includefile (nexttok->u.str.ptr, lf);
				lexer_freetoken (nexttok);
				*gotall = 1;
				/*}}}*/
			} else {
				parser_error (lf, "expected string found ");
				lexer_dumptoken (stderr, nexttok);
				return tree;
			}
			/*}}}*/
		} else if (nexttok && lexer_tokmatchlitstr (nexttok, "OPTION")) {
			/*{{{  #OPTION*/
			lexer_freetoken (tok);
			lexer_freetoken (nexttok);

			nexttok = lexer_nexttoken (lf);
			if (nexttok && lexer_tokmatch (opi.tok_STRING, nexttok)) {
				/*{{{  option for the compiler*/
				char *scopy = string_ndup (nexttok->u.str.ptr, nexttok->u.str.len);

				lexer_freetoken (nexttok);
				if (nocc_dooption_arg (scopy, (void *)lf) < 0) {
					/* failed while processing option */
					parser_error (lf, "failed while processing #OPTION directive");
					sfree (scopy);
					return tree;
				}

				sfree (scopy);
				/*}}}*/
			} else {
				parser_error (lf, "expected string found ");
				lexer_dumptoken (stderr, nexttok);
				return tree;
			}
			/*}}}*/
		}
		if (!tree) {
			/* didn't get anything here, go round */
			goto restartpoint;
		}
		/*}}}*/
	} else {
		lexer_pushback (lf, tok);
		tree = dfa_walk (thedfa ? thedfa : "occampi:declorprocstart", lf);
	}

	return tree;
}
/*}}}*/
/*{{{  static int occampi_procend (lexfile_t *lf)*/
/*
 *	parses a single colon (marking the end of some declaration)
 *	returns 0 on success, -1 on error
 */
static int occampi_procend (lexfile_t *lf)
{
	token_t *tok;

	tok = lexer_nexttoken (lf);
	while (tok && ((tok->type == NEWLINE) || (tok->type == COMMENT))) {
		lexer_freetoken (tok);
		tok = lexer_nexttoken (lf);
	}
	if (!tok) {
		parser_error (lf, "end!");
		return -1;
	}
	if (!lexer_tokmatch (opi.tok_COLON, tok)) {
		parser_error (lf, "expected : found");
		lexer_dumptoken (stderr, tok);
		lexer_pushback (lf, tok);
		return -1;
	}
	lexer_freetoken (tok);
	return 0;
}
/*}}}*/
/*{{{  static tnode_t *occampi_declorproc (lexfile_t *lf, int *gotall, char *thedfa)*/
/*
 *	this parses a whole declaration or process, then returns it
 */
static tnode_t *occampi_declorproc (lexfile_t *lf, int *gotall, char *thedfa)
{
	tnode_t *tree = NULL;
	int tnflags;

	if (compopts.verbose) {
		nocc_message ("occampi_declorproc(): parsing declaration or process start");
	}

	tree = occampi_declorprocstart (lf, gotall, thedfa);
	if (!tree) {
		return NULL;
	}
#if 0
fprintf (stderr, "occampi_declorproc(): got this tree from declorprocstart:\n");
tnode_dumptree (tree, 1, stderr);
#endif

	/* decide how to deal with it */
	if (!*gotall) {
		tnflags = tnode_tnflagsof (tree);
		if (tnflags & TNF_LONGDECL) {
			/*{{{  long declaration (e.g. PROC, CHAN TYPE, etc.)*/
			if ((tree->tag == opi.tag_PROCDECL) || (tree->tag == opi.tag_FUNCDECL)) {
				/* parse body into subnode 2 */
				tnode_t *body;

				body = occampi_indented_process (lf);
				tnode_setnthsub (tree, 2, body);

				/* check trailing colon */
				if (occampi_procend (lf) < 0) {
					/* failed, but error already reported */
				}
			}
			/*}}}*/
		} else if (tnflags & TNF_LONGPROC) {
			/*{{{  long process (e.g. SEQ, CLAIM, FORKING, etc.)*/
			if ((tree->tag == opi.tag_SEQ) || (tree->tag == opi.tag_PAR) || (tree->tag == opi.tag_SHORTIF)) {
				/* parse a list of processes into subnode 1 */
				tnode_t *body;

				body = occampi_indented_process_list (lf, NULL);
				tnode_setnthsub (tree, 1, body);
			} else if (tree->tag == opi.tag_IF) {
				/* parse a list of indented conditions + processes into subnode1 */
				tnode_t *body;

				body = occampi_indented_process_list (lf, "occampi:ifcond");
				tnode_setnthsub (tree, 1, body);
			} else if (tree->tag == opi.tag_CONDITIONAL) {
				/* parse indented process into subnode 1 */
				tnode_t *body;

				body = occampi_indented_process (lf);
				tnode_setnthsub (tree, 1, body);
			}
			/*}}}*/
		}
	}

	return tree;
}
/*}}}*/
/*{{{  static tnode_t *occampi_indented_process (lexfile_t *lf)*/
/*
 *	parses an indented process
 */
static tnode_t *occampi_indented_process (lexfile_t *lf)
{
	tnode_t *tree = NULL;
	tnode_t **target = &tree;
	token_t *tok;

	tok = lexer_nexttoken (lf);
	/* skip newlines */
	for (; tok && (tok->type == NEWLINE); tok = lexer_nexttoken (lf)) {
		lexer_freetoken (tok);
	}
	/* expect indent */
	if (tok->type != INDENT) {
		parser_error (lf, "expected indent, found:");
		lexer_dumptoken (stderr, tok);
		lexer_pushback (lf, tok);
		return NULL;
	}
	lexer_freetoken (tok);

	/* okay, parse declarations and process */
	for (;;) {
		tnode_t *thisone;
		int tnflags;
		int breakfor = 0;
		int gotall = 0;

		thisone = occampi_declorproc (lf, &gotall, NULL);
		if (!thisone) {
			*target = NULL;
			break;		/* for() */
		}
		*target = thisone;
		while (*target) {
			/* sink through anything included, etc. */
			tnflags = tnode_tnflagsof (*target);
			if (tnflags & TNF_LONGDECL) {
				target = tnode_nthsubaddr (*target, 3);
			} else if (tnflags & TNF_SHORTDECL) {
				target = tnode_nthsubaddr (*target, 2);
			} else {
				/* assume we're done! */
				breakfor = 1;
				break;		/* while() */
			}
		}
		if (breakfor) {
			break;		/* for() */
		}
	}

	tok = lexer_nexttoken (lf);
	/* skip newlines */
	for (; tok && (tok->type == NEWLINE); tok = lexer_nexttoken (lf)) {
		lexer_freetoken (tok);
	}
	/* expect outdent */
	if (tok->type != OUTDENT) {
		parser_error (lf, "expected outdent, found:");
		lexer_dumptoken (stderr, tok);
		lexer_pushback (lf, tok);
		if (tree) {
			tnode_free (tree);
		}
		tree = NULL;
		return NULL;
	}
	lexer_freetoken (tok);

	return tree;
}
/*}}}*/
/*{{{  static tnode_t *occampi_indented_process_list (lexfile_t *lf, char *leaddfa)*/
/*
 *	parses a list of indented processes.  if "leaddfa" is non-null, parses that
 *	indented before the process (that may have leading declarations too)
 */
static tnode_t *occampi_indented_process_list (lexfile_t *lf, char *leaddfa)
{
	tnode_t *tree = NULL;
	tnode_t *stored;
	tnode_t **target = &stored;
	token_t *tok;

	tree = parser_newlistnode (lf);

	tok = lexer_nexttoken (lf);
	/* skip newlines */
	for (; tok && (tok->type == NEWLINE); tok = lexer_nexttoken (lf)) {
		lexer_freetoken (tok);
	}
	/* expect indent */
	if (tok->type != INDENT) {
		parser_error (lf, "expected indent, found:");
		lexer_dumptoken (stderr, tok);
		lexer_pushback (lf, tok);
		tnode_free (tree);
		return NULL;
	}
	lexer_freetoken (tok);

	/* okay, parse declarations and process */
	for (;;) {
		tnode_t *thisone;
		int tnflags;
		int breakfor = 0;
		int gotall = 0;

		thisone = occampi_declorproc (lf, &gotall, leaddfa);
		if (!thisone) {
			*target = NULL;
			break;		/* for() */
		}
		*target = thisone;
		while (*target) {
			/* sink through trees */
			tnflags = tnode_tnflagsof (*target);
			if (tnflags & TNF_LONGDECL) {
				target = tnode_nthsubaddr (*target, 3);
			} else if (tnflags & TNF_SHORTDECL) {
				target = tnode_nthsubaddr (*target, 2);
			} else {
				/* process with some leading declarations probably -- add to the list */
				parser_addtolist (tree, stored);
				stored = NULL;
				target = &stored;
			}

			/* peek at the next token -- if outdent, get out */
			tok = lexer_nexttoken (lf);
			for (; tok && (tok->type == NEWLINE); tok = lexer_nexttoken (lf)) {
				lexer_freetoken (tok);
			}
			if (tok && (tok->type == OUTDENT)) {
				lexer_pushback (lf, tok);
				breakfor = 1;
				break;		/* while() */
			} else {
				lexer_pushback (lf, tok);
			}
		}
		if (breakfor) {
			break;		/* for() */
		}
	}

	tok = lexer_nexttoken (lf);
	/* skip newlines */
	for (; tok && (tok->type == NEWLINE); tok = lexer_nexttoken (lf)) {
		lexer_freetoken (tok);
	}
	/* expect outdent */
	if (tok->type != OUTDENT) {
		parser_error (lf, "expected outdent, found:");
		lexer_dumptoken (stderr, tok);
		lexer_pushback (lf, tok);
		if (tree) {
			tnode_free (tree);
		}
		tree = NULL;
		return NULL;
	}
	lexer_freetoken (tok);

	return tree;
}
/*}}}*/


/*{{{  static tnode_t *occampi_parser_parse (lexfile_t *lf)*/
/*
 *	called to parse a file.
 *	returns a tree on success, NULL on failure
 */
static tnode_t *occampi_parser_parse (lexfile_t *lf)
{
	token_t *tok;
	tnode_t *tree = NULL;
	tnode_t **target = &tree;

	if (compopts.verbose) {
		nocc_message ("occampi_parser_parse(): starting parse..");
	}

	for (;;) {
		tnode_t *thisone;
		int tnflags;
		int gotall = 0;
		int breakfor = 0;

		thisone = occampi_declorproc (lf, &gotall, NULL);
		if (!thisone) {
			*target = NULL;
			break;		/* for() */
		}
		*target = thisone;
		while (*target) {
			/* sink through stuff, although we shouldn't really see this here */
			tnflags = tnode_tnflagsof (*target);
			if (tnflags & TNF_LONGDECL) {
				target = tnode_nthsubaddr (*target, 3);
			} else if (tnflags & TNF_SHORTDECL) {
				target = tnode_nthsubaddr (*target, 2);
			} else {
				/* assume we're done! */
				breakfor = 1;
				break;		/* while() */
			}
		}
		if (breakfor) {
			break;
		}
	}

	if (compopts.verbose) {
		nocc_message ("leftover tokens:");
	}

	tok = lexer_nexttoken (lf);
	while (tok) {
		if (compopts.verbose) {
			lexer_dumptoken (stderr, tok);
		}
		if ((tok->type == END) || (tok->type == NOTOKEN)) {
			lexer_freetoken (tok);
			break;
		}
		if ((tok->type != NEWLINE) && (tok->type != COMMENT)) {
			lf->errcount++;				/* errors.. */
		}

		lexer_freetoken (tok);
		tok = lexer_nexttoken (lf);
	}

	return tree;
}
/*}}}*/
/*{{{  static int occampi_parser_prescope (tnode_t **tptr, prescope_t *ps)*/
/*
 *	called to pre-scope the parse tree
 *	returns 0 on success, non-zero on failure
 */
static int occampi_parser_prescope (tnode_t **tptr, prescope_t *ps)
{
	occampi_prescope_t *ops = (occampi_prescope_t *)smalloc (sizeof (occampi_prescope_t));

	ops->last_type = NULL;
	ps->hook = (void *)ops;
	tnode_modprewalktree (tptr, prescope_modprewalktree, (void *)ps);

	sfree (ops);
	return ps->err;
}
/*}}}*/
/*{{{  static int occampi_parser_scope (tnode_t **tptr)*/
/*
 *	called to scope declarations in the parse tree
 *	returns 0 on success, non-zero on failure
 */
static int occampi_parser_scope (tnode_t **tptr, scope_t *ss)
{
	tnode_modprepostwalktree (tptr, scope_modprewalktree, scope_modpostwalktree, (void *)ss);
	return ss->err;
}
/*}}}*/
/*{{{  static int occampi_parser_typecheck (tnode_t *tptr, typecheck_t *tc)*/
/*
 *	called to type-check a tree
 *	returns 0 on success, non-zero on failure
 */
static int occampi_parser_typecheck (tnode_t *tptr, typecheck_t *tc)
{
	tnode_prewalktree (tptr, typecheck_prewalktree, (void *)tc);
	return tc->err;
}
/*}}}*/


