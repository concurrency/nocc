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
static tnode_t *occampi_indented_process_list (lexfile_t *lf);


/*}}}*/
/*{{{  global vars*/

occampi_pset_t opi;		/* attach tags, etc. here */

langparser_t occampi_parser = {
	langname:	"occampi",
	init:		occampi_parser_init,
	shutdown:	occampi_parser_shutdown,
	parse:		occampi_parser_parse,
	prescope:	occampi_parser_prescope,
	scope:		occampi_parser_scope,
	typecheck:	occampi_parser_typecheck,
	tagstruct_hook:	(void *)&opi
};


/*}}}*/
/*{{{  private types/vars*/
typedef struct {
	dfanode_t *inode;
} occampi_parse_t;


static occampi_parse_t *occampi_priv = NULL;

/*}}}*/


/*{{{  occampi reductions*/
/*{{{  static void *occampi_nametoken_to_hook (void *ntok)*/
/*
 *	turns a name token into a hooknode for a tag_NAME
 */
static void *occampi_nametoken_to_hook (void *ntok)
{
	token_t *tok = (token_t *)ntok;
	char *rawname;

	rawname = tok->u.name;
	tok->u.name = NULL;

	lexer_freetoken (tok);

	return (void *)rawname;
}
/*}}}*/
/*{{{  static void *occampi_integertoken_to_hook (void *itok)*/
/*
 *	turns an integer token into a hooknode for tag_LITINT
 */
static void *occampi_integertoken_to_hook (void *itok)
{
	token_t *tok = (token_t *)itok;
	occampi_litdata_t *ldata = (occampi_litdata_t *)smalloc (sizeof (occampi_litdata_t));

	ldata->bytes = sizeof (tok->u.ival);
	ldata->data = mem_ndup (&(tok->u.ival), ldata->bytes);

	lexer_freetoken (tok);

	return (void *)ldata;
}
/*}}}*/
/*{{{  static void *occampi_realtoken_to_hook (void *itok)*/
/*
 *	turns an real token into a hooknode for tag_LITREAL
 */
static void *occampi_realtoken_to_hook (void *itok)
{
	token_t *tok = (token_t *)itok;
	occampi_litdata_t *ldata = (occampi_litdata_t *)smalloc (sizeof (occampi_litdata_t));

	ldata->bytes = sizeof (tok->u.dval);
	ldata->data = mem_ndup (&(tok->u.dval), ldata->bytes);

	lexer_freetoken (tok);

	return (void *)ldata;
}
/*}}}*/
/*{{{  static void occampi_reduce_primtype (dfastate_t *dfast, parsepriv_t *pp, void *rarg)*/
/*
 *	reduces a primitive type
 */
static void occampi_reduce_primtype (dfastate_t *dfast, parsepriv_t *pp, void *rarg)
{
	token_t *tok;
	ntdef_t *tag;

	tok = parser_gettok (pp);
	tag = tnode_lookupnodetag (tok->u.kw->name);
	*(dfast->ptr) = tnode_create (tag, tok->origin);
	lexer_freetoken (tok);

	return;
}
/*}}}*/
/*{{{  static void occampi_reduce_primproc (dfastate_t *dfast, parsepriv_t *pp, void *rarg)*/
/*
 *	reduces a primitive process
 */
static void occampi_reduce_primproc (dfastate_t *dfast, parsepriv_t *pp, void *rarg)
{
	token_t *tok;
	ntdef_t *tag;

	tok = parser_gettok (pp);
	tag = tnode_lookupnodetag (tok->u.kw->name);
	*(dfast->ptr) = tnode_create (tag, tok->origin);
	lexer_freetoken (tok);

	return;
}
/*}}}*/
/*{{{  static void occampi_reduce_cnode (dfastate_t *dfast, parsepriv_t *pp, void *rarg)*/
/*
 *	reduces a constructor node (e.g. SEQ, PAR, FORKING, CLAIM, ..)
 */
static void occampi_reduce_cnode (dfastate_t *dfast, parsepriv_t *pp, void *rarg)
{
	token_t *tok;
	ntdef_t *tag;

	tok = parser_gettok (pp);
	tag = tnode_lookupnodetag (tok->u.kw->name);
	*(dfast->ptr) = tnode_create (tag, tok->origin, NULL, NULL);
	lexer_freetoken (tok);

	return;
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
	parser_register_reduce ("Roccampi:primtype", occampi_reduce_primtype, NULL);
	parser_register_reduce ("Roccampi:primproc", occampi_reduce_primproc, NULL);
	parser_register_reduce ("Roccampi:cnode", occampi_reduce_cnode, NULL);
	parser_register_reduce ("Roccampi:inlist", occampi_inlistreduce, NULL);

	return 0;
}
/*}}}*/
/*}}}*/


/*{{{  tree-node hook functions*/

/* all got moved out, space for future population ;) */

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


	/*{{{  setup generic reductions*/
	parser_register_grule ("opi:namereduce", parser_decode_grule ("T+St0XC1R-", occampi_nametoken_to_hook, opi.tag_NAME));
	parser_register_grule ("opi:integerreduce", parser_decode_grule ("T+St0X0VC2R-", occampi_integertoken_to_hook, opi.tag_LITINT));
	parser_register_grule ("opi:realreduce", parser_decode_grule ("0T+St0XC2R-", occampi_realtoken_to_hook, opi.tag_LITREAL));
	parser_register_grule ("opi:namepush", parser_decode_grule ("T+St0XC1N-", occampi_nametoken_to_hook, opi.tag_NAME));
	parser_register_grule ("opi:nullreduce", parser_decode_grule ("N+R-"));
	parser_register_grule ("opi:nullpush", parser_decode_grule ("0N-"));
	parser_register_grule ("opi:nullset", parser_decode_grule ("0R-"));
	parser_register_grule ("opi:chanpush", parser_decode_grule ("N+Sn0C1N-", opi.tag_CHAN));
	parser_register_grule ("opi:fparam2nsreduce", parser_decode_grule ("N+Sn0N+C2R-", opi.tag_FPARAM));
	parser_register_grule ("opi:fparam2tsreduce", parser_decode_grule ("T+St0XC1T+XC1C2R-", occampi_nametoken_to_hook, opi.tag_NAME, occampi_nametoken_to_hook, opi.tag_NAME, opi.tag_FPARAM));
	parser_register_grule ("opi:fparam1tsreduce", parser_decode_grule ("T+St0XC10C2R-", occampi_nametoken_to_hook, opi.tag_NAME, opi.tag_FPARAM));
	parser_register_grule ("opi:declreduce", parser_decode_grule ("SN1N+N+0C3R-", opi.tag_VARDECL));
	parser_register_grule ("opi:assignreduce", parser_decode_grule ("SN1N+N+V0C3R-", opi.tag_ASSIGN));
	parser_register_grule ("opi:outputreduce", parser_decode_grule ("SN1N+N+V0C3R-", opi.tag_OUTPUT));
	parser_register_grule ("opi:procdeclreduce", parser_decode_grule ("SN1N+N+V00C4R-", opi.tag_PROCDECL));
	parser_register_grule ("opi:pinstancereduce", parser_decode_grule ("SN1N+N+V0C3R-", opi.tag_PINSTANCE));
	/*}}}*/


	/*{{{  create DFAs*/
	dynarray_init (transtbls);
	dynarray_add (transtbls, dfa_bnftotbl ("occampi:name ::= +Name {<opi:namereduce>}"));
	dynarray_add (transtbls, dfa_bnftotbl ("occampi:namelist ::= { occampi:name @@, 1 }"));
	dynarray_add (transtbls, dfa_bnftotbl ("occampi:primtype ::= ( +@INT | +@BYTE ) {Roccampi:primtype}"));
	dynarray_add (transtbls, dfa_bnftotbl ("occampi:protocol ::= ( occampi:primtype | +Name {<opi:namepush>} ) {<opi:nullreduce>}"));
	dynarray_add (transtbls, dfa_bnftotbl ("occampi:primproc ::= ( +@SKIP | +@STOP ) {Roccampi:primproc}"));
	dynarray_add (transtbls, dfa_bnftotbl ("occampi:vardecl ::= ( occampi:primtype | @CHAN occampi:protocol {<opi:chanpush>} ) occampi:namelist @@: {<opi:declreduce>}"));
	dynarray_add (transtbls, dfa_bnftotbl ("occampi:fparam ::= ( occampi:primtype occampi:name {<opi:fparam2nsreduce>} | " \
				"+Name ( +Name {<opi:fparam2tsreduce>} | -* {<opi:fparam1tsreduce>} ) | " \
				"@CHAN occampi:protocol {<opi:chanpush>} occampi:name {<opi:fparam2nsreduce>} )"));
	dynarray_add (transtbls, dfa_bnftotbl ("occampi:fparamlist ::= ( -@@) {<opi:nullset>} | { occampi:fparam @@, 1 } )"));
	dynarray_add (transtbls, dfa_transtotbl ("occampi:exprnamestart ::= [ 0 +Name 1 ] [ 1 @@( 2 ] [ 2 {<opi:namepush>} ] [ 2 @@) 3 ] [ 3 {<opi:nullreduce>} -* ] " \
				"[ 1 -* 4 ] [ 4 {<opi:namereduce>} -* ]"));
	dynarray_add (transtbls, dfa_bnftotbl ("occampi:expr ::= ( -Name occampi:exprnamestart {<opi:nullreduce>} | +Integer {<opi:integerreduce>} | +Real {<opi:realreduce>} )"));
	dynarray_add (transtbls, dfa_bnftotbl ("occampi:exprsemilist ::= { occampi:expr @@; 1 }"));
	dynarray_add (transtbls, dfa_bnftotbl ("occampi:exprcommalist ::= { occampi:expr @@, 1 }"));
	dynarray_add (transtbls, dfa_bnftotbl ("occampi:aparamlist ::= ( -@@) {<opi:nullset>} | { occampi:expr @@, 1 } )"));

	dynarray_add (transtbls, dfa_transtotbl ("occampi:namestart ::= [ 0 +Name 1 ] [ 1 @@:= 2 ] [ 2 {<opi:namepush>} ] [ 2 occampi:expr 3 ] [ 3 {<opi:assignreduce>} -* ] " \
				"[ 1 @@! 4 ] [ 4 {<opi:namepush>} ] [ 4 occampi:exprsemilist 5 ] [ 5 @@: 6 ] [ 6 {<opi:declreduce>} -* ] [ 5 -* 7 ] [ 7 {<opi:outputreduce>} -* ] " \
				"[ 1 @@( 8 ] [ 8 {<opi:namepush>} ] [ 8 occampi:aparamlist 9 ] [ 9 @@) 10 ] [ 10 {<opi:pinstancereduce>} -* ]"));
	dynarray_add (transtbls, dfa_transtotbl ("occampi:procdecl ::= [ 0 @PROC 1 ] [ 1 occampi:name 2 ] [ 2 @@( 3 ] [ 3 occampi:fparamlist 4 ] [ 4 @@) 5 ] [ 5 {<opi:procdeclreduce>} -* ]"));

	dynarray_add (transtbls, dfa_bnftotbl ("occampi:cproc ::= ( +@SEQ | +@PAR ) -Newline {Roccampi:cnode}"));
	dynarray_add (transtbls, dfa_bnftotbl ("occampi:declorprocstart ::= ( occampi:vardecl | occampi:procdecl | occampi:primproc | occampi:cproc | occampi:namestart ) {<opi:nullreduce>}"));

	dynarray_add (transtbls, dfa_bnftotbl ("occampi:cproc +:= +@IF -Newline {Roccampi:cnode}"));

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
	/*{{{  convert into DFA nodes proper*/
	x = 0;
	for (i=0; i<DA_CUR (transtbls); i++) {
		dfattbl_t *ttbl = DA_NTHITEM (transtbls, i);

		/* only convert non-addition nodes */
		if (ttbl && !ttbl->op) {
			x += !dfa_tbltodfa (ttbl);
		}
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
	tndef_t *tnd;
	/* ntdef_t *ntd; */
	/* compops_t *cops; */
	int i;

	/*{{{  occampi:leafnode -- INT, BYTE, SKIP, STOP*/
	i = -1;
	tnd = opi.node_LEAFNODE = tnode_newnodetype ("occampi:leafnode", &i, 0, 0, 0, TNF_NONE);
	i = -1;
	opi.tag_SKIP = tnode_newnodetag ("SKIP", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_STOP = tnode_newnodetag ("STOP", &i, tnd, NTF_NONE);
	/*}}}*/
	/*{{{  occampi:cnode -- SEQ, PAR*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:cnode", &i, 2, 0, 0, TNF_LONGPROC);
	i = -1;
	opi.tag_SEQ = tnode_newnodetag ("SEQ", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_PAR = tnode_newnodetag ("PAR", &i, tnd, NTF_NONE);
	/*}}}*/

	occampi_decl_nodes_init ();
	occampi_type_nodes_init ();
	occampi_action_nodes_init ();
	occampi_lit_nodes_init ();
	occampi_instance_nodes_init ();

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


/*{{{  static tnode_t *occampi_declorprocstart (lexfile_t *lf)*/
/*
 *	parses a declaration/process for single-liner's, or start of a declaration/process
 *	for multi-liners
 */
static tnode_t *occampi_declorprocstart (lexfile_t *lf)
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
		return NULL;
	}

	lexer_pushback (lf, tok);

	tree = dfa_walk ("occampi:declorprocstart", lf);

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
/*{{{  static tnode_t *occampi_declorproc (lexfile_t *lf)*/
/*
 *	this parses a whole declaration or process, then returns it
 */
static tnode_t *occampi_declorproc (lexfile_t *lf)
{
	tnode_t *tree = NULL;
	int tnflags;

	if (compopts.verbose) {
		nocc_message ("occampi_declorproc(): parsing declaration or process start");
	}

	tree = occampi_declorprocstart (lf);
	if (!tree) {
		return NULL;
	}
#if 0
fprintf (stderr, "occampi_declorproc(): got this tree from declorprocstart:\n");
tnode_dumptree (tree, 1, stderr);
#endif

	/* decide how to deal with it */
	tnflags = tnode_tnflagsof (tree);
	if (tnflags & TNF_LONGDECL) {
		/*{{{  long declaration (e.g. PROC, CHAN TYPE, etc.)*/
		if (tree->tag == opi.tag_PROCDECL) {
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
		if ((tree->tag == opi.tag_SEQ) || (tree->tag == opi.tag_PAR)) {
			/* parse a list of processes into subnode 1 */
			tnode_t *body;

			body = occampi_indented_process_list (lf);
			tnode_setnthsub (tree, 1, body);
		}
		/*}}}*/
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

		thisone = occampi_declorproc (lf);
		if (!thisone) {
			*target = NULL;
			break;		/* for() */
		}
		*target = thisone;
		tnflags = tnode_tnflagsof (*target);
		if (tnflags & TNF_LONGDECL) {
			target = tnode_nthsubaddr (*target, 3);
		} else if (tnflags & TNF_SHORTDECL) {
			target = tnode_nthsubaddr (*target, 2);
		} else {
			/* assume we're done! */
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
/*{{{  static tnode_t *occampi_indented_process_list (lexfile_t *lf)*/
/*
 *	parses a list of indented processes
 */
static tnode_t *occampi_indented_process_list (lexfile_t *lf)
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

		thisone = occampi_declorproc (lf);
		if (!thisone) {
			*target = NULL;
			break;		/* for() */
		}
		*target = thisone;
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
			break;		/* for() */
		} else {
			lexer_pushback (lf, tok);
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

		thisone = occampi_declorproc (lf);
		if (!thisone) {
			*target = NULL;
			break;		/* for() */
		}
		*target = thisone;
		tnflags = tnode_tnflagsof (*target);
		if (tnflags & TNF_LONGDECL) {
			target = tnode_nthsubaddr (*target, 3);
		} else if (tnflags & TNF_SHORTDECL) {
			target = tnode_nthsubaddr (*target, 2);
		} else {
			/* assume we're done! */
			break;		/* for() */
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


