/*
 *	eac.h -- Escape analysis code interface for NOCC
 *	Copyright (C) 2011-2013 Fred Barnes <frmb@kent.ac.uk>
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

#ifndef __EAC_H
#define __EAC_H

#define EAC_INTERACTIVE (1)
#define EAC_DEF	(2)

struct TAG_langlexer;
struct TAG_langparser;
struct TAG_langdef;

extern struct TAG_langlexer eac_lexer;
extern struct TAG_langparser eac_parser;


struct TAG_tndef;
struct TAG_ntdef;
struct TAG_token;
struct TAG_chook;
struct TAG_fhandle;

typedef struct {
	struct TAG_tndef *node_NAMENODE;
	struct TAG_tndef *node_LEAFNODE;

	struct TAG_ntdef *tag_NAME;

	struct TAG_ntdef *tag_DECL;
	struct TAG_ntdef *tag_VARDECL;

	struct TAG_ntdef *tag_ESET;
	struct TAG_ntdef *tag_ESEQ;

	struct TAG_ntdef *tag_SVREND;
	struct TAG_ntdef *tag_CLIEND;

	struct TAG_ntdef *tag_INPUT;
	struct TAG_ntdef *tag_OUTPUT;

	struct TAG_ntdef *tag_VARCOMP;

	struct TAG_ntdef *tag_PAR;
	struct TAG_ntdef *tag_HIDE;
	struct TAG_ntdef *tag_INSTANCE;
	struct TAG_ntdef *tag_SUBST;

	struct TAG_ntdef *tag_NPROCDEF;
	struct TAG_ntdef *tag_NCHANVAR;
	struct TAG_ntdef *tag_NSVRCHANVAR;
	struct TAG_ntdef *tag_NCLICHANVAR;
	struct TAG_ntdef *tag_NVAR;

	struct TAG_ntdef *tag_PROC;
	struct TAG_ntdef *tag_CHANVAR;
	struct TAG_ntdef *tag_VAR;

	struct TAG_ntdef *tag_FVPEXPR;

	struct TAG_token *tok_ATSIGN;
} eac_pset_t;

extern eac_pset_t eac;

struct TAG_compcxt;

/* handler for interactive mode */
extern int eac_callback_line (char *line, struct TAG_compcxt *ccx);
extern void eac_mode_in (struct TAG_compcxt *ccx);
extern void eac_mode_out (struct TAG_compcxt *ccx);

extern void eac_isetindent (struct TAG_fhandle *stream, int indent);
extern struct TAG_langdef *eac_getlangdef (void);

/* front-end units */
extern struct TAG_feunit eac_code_feunit;		/* eac_code.c */

/* for language units to use in reductions */
extern void *eac_nametoken_to_hook (void *ntok);
extern void *eac_stringtoken_to_namehook (void *ntok);

/* option handlers inside front-end */
struct TAG_cmd_option;
extern int eac_lexer_opthandler_flag (struct TAG_cmd_option *opt, char ***argwalk, int *argleft);


#endif	/* !__EAC_H */

