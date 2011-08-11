/*
 *	eac.h -- Escape analysis code interface for NOCC
 *	Copyright (C) 2011 Fred Barnes <frmb@kent.ac.uk>
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

struct TAG_langlexer;
struct TAG_langparser;
struct TAG_langdef;

extern struct TAG_langlexer eac_lexer;
extern struct TAG_langparser eac_parser;


struct TAG_tndef;
struct TAG_ntdef;
struct TAG_token;
struct TAG_chook;

typedef struct {
	struct TAG_tndef *node_NAMENODE;
	struct TAG_tndef *node_LEAFNODE;

	struct TAG_ntdef *tag_NAME;

	struct TAG_ntdef *tag_EACDEF;
	struct TAG_ntdef *tag_FPARAM;

	struct TAG_ntdef *tag_ESET;
	struct TAG_ntdef *tag_ESEQ;

	struct TAG_token *tok_ATSIGN;
} eac_pset_t;

extern eac_pset_t eac;


extern void eac_isetindent (FILE *stream, int indent);
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
